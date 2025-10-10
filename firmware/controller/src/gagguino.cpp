/**
 * @file gagguino.cpp
 * @brief Gagguino firmware: ESP32 control for Gaggia Classic.
 *
 * High-level responsibilities:
 * - Temperature control using a MAX31865 RTD amplifier (PT100) and PID.
 * - Heater PWM drive.
 * - Flow, pressure and shot timing measurement with debounced ISR counters.
 * - ESP-NOW link to the display for control/telemetry.
 * - Brief Wi-Fi use to synchronize time over NTP.
 *
 * Hardware pins (ESP32 default board mapping):
 * - FLOW_PIN (26)   : Flow sensor input (interrupt on CHANGE)
 * - ZC_PIN (25)     : Triac Zero Crossing output (interrupt on RISING)
 * - HEAT_PIN (27)   : Heater SSR control (PWM windowing)
 * - PUMP_PIN (17)   : Triac PWM output (Arduino D4)
 * - AC_SENS (14)    : Steam switch sense (digital input)
 * - MAX_CS (16)     : MAX31865 SPI chip-select
 * - PRESS_PIN (35)  : Analog pressure sensor input
 */
#include "gagguino.h"

#include <Adafruit_MAX31865.h>
#include <Arduino.h>
#include <RBDdimmer.h>
#include <WiFi.h>
#include <ctype.h>
#include <esp_now.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include <cstdarg>

#include "espnow_packet.h"
#include "secrets.h"  // WIFI_*
#include "version.h"
#define STARTUP_WAIT 1000
#define SERIAL_BAUD 115200

/**
 * @brief Lightweight printf-style logger to the serial console.
 */
static inline void LOG(const char* fmt, ...) {
    static char buf[192];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    struct tm tm;
    localtime_r(&tv.tv_sec, &tm);
    char tbuf[32];
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", &tm);
    Serial.printf("[%s.%03ld] %s\n", tbuf, tv.tv_usec / 1000, buf);
}

/**
 * @brief Log a significant error.
 *
 * Maintains a small rolling buffer of recent error messages to avoid
 * unbounded growth while still providing context for debugging.
 */

namespace {
constexpr int FLOW_PIN = 26;  // Flowmeter Pulses (Arduino D2)
constexpr int ZC_PIN = 25;    // Triac Zero Crossing output (Arduino D3)
constexpr int PUMP_PIN = 17;  // Triac PWM output (Arduino D4)
constexpr int MAX_CS = 16;    // MAX31865 CS (Arduino D5)
constexpr int HEAT_PIN = 27;  // Heater SSR control (Arduino D6)
constexpr int AC_SENS = 14;   // Steam AC sense (Arduino D7)

constexpr int PRESS_PIN = 35;

constexpr unsigned long PRESS_CYCLE = 100, PID_CYCLE = 250, PWM_CYCLE = 250, ESP_CYCLE = 500,
                        LOG_CYCLE = 2000;

// Simple handshake bytes for ESP-NOW link-up (values defined in shared/espnow_protocol.h)

constexpr unsigned long DISPLAY_TIMEOUT_MS = 5000;      // ms without ACK before fallback
constexpr unsigned long ESPNOW_CHANNEL_HOLD_MS = 1100;  // dwell time per channel when scanning
constexpr uint8_t ESPNOW_FIRST_CHANNEL = 1;
constexpr uint8_t ESPNOW_LAST_CHANNEL = 13;
constexpr uint8_t ESPNOW_BROADCAST_ADDR[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Brew & Steam setpoint limits
constexpr float BREW_MIN = 87.0f, BREW_MAX = 97.0f;
constexpr float STEAM_MIN_C = 145.0f, STEAM_MAX_C = 155.0f;

// Default steam setpoint (within limits)
constexpr float STEAM_DEFAULT = 152.0f;

constexpr float RREF = 430.0f, RNOMINAL = 100.0f;
// Default PID params (overridable via ESP-NOW control packets)
// Default PID parameters tuned for stability
// Kp: 15-16 [out/degC]
// Ki: 0.3-0.5 [out/(degC*s)] -> start at 0.35
// Kd: 50-70 [out*s/degC] -> start at 60
// guard: +/-8-+/-12% integral clamp on 0-100% heater
constexpr float P_GAIN_TEMP = 8.0f, I_GAIN_TEMP = 0.60f, D_GAIN_TEMP = 10.5f, DTAU_TEMP = 0.8f,
                WINDUP_GUARD_TEMP = 25.0f;

// Derivative filter time constant (seconds), exposed to HA

dimmerLamp pumpDimmer(PUMP_PIN, ZC_PIN);

// Pressure calibration constants
constexpr float PRESSURE_TOL = 1.0f, PRESS_GRAD = 0.00903f, PRESS_INT_0 = -4.0f;
constexpr int PRESS_BUFF_SIZE = 14;
constexpr float PRESS_THRESHOLD = 9.0f;

// FLOW_CAL in mL per pulse (1 cc == 1 mL)
constexpr float FLOW_CAL = 0.246f;
constexpr unsigned long PULSE_MIN = 3;  // ms debounce (bounce + double-edges)

constexpr unsigned ZC_MIN = 4;
// Duration thresholds for zero-cross (pump) activity
constexpr unsigned long ZC_WAIT = 2000, ZC_OFF = 1000, SHOT_RESET = 60000;
constexpr unsigned long AC_WAIT = 100;
constexpr int STEAM_MIN = 20;
constexpr float PUMP_POWER_DEFAULT = 95.0f;
constexpr float PRESSURE_SETPOINT_DEFAULT = 9.0f;
constexpr float PRESSURE_SETPOINT_MIN = 0.0f;
constexpr float PRESSURE_SETPOINT_MAX = 12.0f;
constexpr float PRESSURE_LIMIT_TOL = 0.1f;
constexpr float PUMP_PRESSURE_RAMP_RATE = 50.0f;    // % per second when ramping up in pressure mode
constexpr float PUMP_PRESSURE_RAMP_MAX_DT = 0.2f;    // Max dt (s) considered for ramp calculations

const bool debugPrint = true;
}  // namespace

// ---------- Devices / globals ----------
namespace {
Adafruit_MAX31865 max31865(MAX_CS);
// Rolling buffer of recent significant error messages
static String g_errorLog;
// Tracks whether the RTC has successfully synchronized with NTP
static bool g_clockSynced = false;
static bool g_wifiNtpConnecting = false;

/**
 * @brief Log a significant error and persist it in memory.
 */
static inline void LOG_ERROR(const char* fmt, ...) {
    static char buf[192];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    struct tm tm;
    localtime_r(&tv.tv_sec, &tm);
    char tbuf[32];
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", &tm);
    Serial.printf("[%s.%03ld] %s\n", tbuf, tv.tv_usec / 1000, buf);

    if (!g_errorLog.isEmpty()) g_errorLog += '\n';
    g_errorLog += buf;

    constexpr size_t MAX_ERR_LOG = 512;
    if (g_errorLog.length() > MAX_ERR_LOG) {
        int cut = g_errorLog.length() - MAX_ERR_LOG;
        cut = g_errorLog.indexOf('\n', cut);
        if (cut >= 0) g_errorLog = g_errorLog.substring(cut + 1);
    }
}

static void syncClock() {
    configTime(0, 0, "pool.ntp.org");
    struct tm tm;
    if (getLocalTime(&tm, 5000)) {
        LOG("RTC: %04d-%02d-%02d %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec);
        g_clockSynced = true;
    } else {
        LOG_ERROR("RTC: sync failed");
    }
}

// Temps / PID
float currentTemp = 0.0f, lastTemp = 0.0f, pvFiltTemp = 0.0f;
float brewSetpoint = 92.0f;           // HA-controllable (90?99)
float steamSetpoint = STEAM_DEFAULT;  // HA-controllable (145?155)
float setTemp = brewSetpoint;         // active target (brew or steam)
float iStateTemp = 0.0f, heatPower = 0.0f;
// Live-tunable PID parameters (default to constexprs above)
float pGainTemp = P_GAIN_TEMP, iGainTemp = I_GAIN_TEMP, dGainTemp = D_GAIN_TEMP,
      dTauTemp = DTAU_TEMP, windupGuardTemp = WINDUP_GUARD_TEMP;
int heatCycles = 0;
bool heaterState = false;
bool heaterEnabled = true;             // HA switch default ON at boot
float pumpPower = PUMP_POWER_DEFAULT;  // Default pump power (%), overridden by display
float pressureSetpointBar = PRESSURE_SETPOINT_DEFAULT;  // Target brew pressure in bar
bool pumpPressureModeEnabled = false;  // When true limit pump power to pressure setpoint
float lastPumpApplied = 0.0f;          // Actual power sent to dimmer after ramp/limits
unsigned long lastPumpApplyMs = 0;     // Timestamp of last pump power application

// Pressure
int rawPress = 0;
float lastPress = 0.0f, pressNow = 0.0f, pressSum = 0.0f, pressGrad = PRESS_GRAD,
      pressInt = PRESS_INT_0;
float pressBuff[PRESS_BUFF_SIZE] = {0};
uint8_t pressBuffIdx = 0;

// Time/shot
unsigned long nLoop = 0, currentTime = 0, lastPidTime = 0, lastPwmTime = 0, lastEspNowTime = 0,
              lastLogTime = 0;
// microsecond timestamps for ISR debounce
volatile int64_t lastPulseTime = 0;
unsigned long shotStart = 0, startTime = 0;
float shotTime = 0;  //

// Flow / flags
volatile unsigned long pulseCount = 0;
volatile unsigned long zcCount = 0;
volatile unsigned long lastZcCount = 0;
volatile int64_t lastZcTime = 0;  // microsecond timestamp
float vol = 0.0f, preFlowVol = 0.0f, shotVol = 0.0f;
bool prevSteamFlag = false, ac = false;
int acCount = 0;
bool shotFlag = false, preFlow = false, steamFlag = false, steamDispFlag = false,
     steamHwFlag = false, steamResetPending = false, setupComplete = false, debugData = false;

// ESP-NOW diagnostics
static uint8_t g_espnowChannel = 0;
static String g_espnowStatus = "disabled";
static String g_espnowMac;
static bool g_espnowHandshake = false;
static uint8_t g_displayMac[ESP_NOW_ETH_ALEN] = {0};
static bool g_haveDisplayPeer = false;
static uint32_t g_lastControlRevision = 0;
static unsigned long g_lastDisplayAckMs = 0;
static EspNowPumpMode pumpMode = ESPNOW_PUMP_MODE_NORMAL;
static bool g_espnowCoreInit = false;
static bool g_espnowBroadcastPeerAdded = false;
static esp_now_peer_info_t g_broadcastPeerInfo{};
static bool g_espnowScanning = false;
static uint8_t g_nextScanChannel = ESPNOW_FIRST_CHANNEL;
static unsigned long g_lastChannelHopMs = 0;

static bool applyEspNowChannel(uint8_t channel, bool forceSetWifiChannel, bool silent);

// ---------- helpers ----------
/**
 * @brief Convert Wi-Fi status to a readable string.
 */
static inline const char* wifiStatusName(wl_status_t s) {
    switch (s) {
        case WL_IDLE_STATUS:
            return "IDLE";
        case WL_NO_SSID_AVAIL:
            return "NO_SSID";
        case WL_SCAN_COMPLETED:
            return "SCAN_DONE";
        case WL_CONNECTED:
            return "CONNECTED";
        case WL_CONNECT_FAILED:
            return "CONNECT_FAILED";
        case WL_CONNECTION_LOST:
            return "CONNECTION_LOST";
        case WL_DISCONNECTED:
            return "DISCONNECTED";
        default:
            return "UNKNOWN";
    }
}
// ------------------------------------------------------------------------------
//  PID: dt-scaled I & D, iTerm clamp, derivative LPF, conditional integration
// ------------------------------------------------------------------------------
static float calcPID(float Kp, float Ki, float Kd, float sp, float pv,
                     float dt,       // seconds (0.5 at 2 Hz)
                     float& pvFilt,  // filtered PV (state)
                     float& iSum,    // ?err dt  (state)
                     float guard,    // clamp on iTerm (output units)
                     float dTau = 0.8f) {
    // 1) Error
    ;                     // derivative LPF time const (s)
    float outMin = 0.0f;  // actuator limits (for cond. integration)
    float outMax = 100.0f;

    float err = sp - pv;

    // 2) Integral
    iSum += err * dt;

    // 3) Derivative on measurement with 1st-order filter (dirty derivative)
    //    LPF on pv: pvFilt' = (pv - pvFilt)/dTau
    float alpha = dt / (dTau + dt);  // 0<alpha<1
    float prevPvFilt = pvFilt;
    pvFilt += alpha * (pv - pvFilt);           // low-pass the measurement
    float dMeas = (pvFilt - prevPvFilt) / dt;  // derivative of filtered pv

    // 4) Terms
    float pTerm = Kp * err;
    float iTerm = Ki * iSum;
    // clamp the CONTRIBUTION of I (anti-windup)
    if (iTerm > guard) iTerm = guard;
    if (iTerm < -guard) iTerm = -guard;

    float dTerm = -Kd * dMeas;  // derivative on measurement

    // 5) Output (pre-clamp)
    float u = pTerm + iTerm + dTerm;

    // 6) Conditional integration: don't integrate when pushing into saturation
    if ((u >= outMax && err > 0.0f) || (u <= outMin && err < 0.0f)) {
        iSum -= err * dt;  // undo this step?s integral
        iTerm = Ki * iSum;
        if (iTerm > guard) iTerm = guard;
        if (iTerm < -guard) iTerm = -guard;
        u = pTerm + iTerm + dTerm;
    }
    return u;
}

// --------------- espresso logic ---------------
/**
 * @brief Detect shot start/stop based on zero-cross events and steam transitions.
 */
static void checkShotStartStop() {
    if ((zcCount >= ZC_MIN) && !shotFlag && setupComplete &&
        ((currentTime - startTime) > ZC_WAIT)) {
        shotStart = currentTime;
        shotTime = 0;
        shotFlag = true;
        pulseCount = 0;
        preFlow = true;
        preFlowVol = 0.0f;
    }
    unsigned long lastZcTimeMs = lastZcTime / 1000;
    if ((steamFlag && !prevSteamFlag) ||
        (currentTime - lastZcTimeMs >= SHOT_RESET && shotFlag && currentTime > lastZcTimeMs)) {
        pulseCount = 0;
        shotVol = 0.0f;
        shotTime = 0;
        lastPulseTime = esp_timer_get_time();
        shotFlag = false;
        preFlow = false;
    }
}

/**
 * @brief Read temperature and update heater PID and window length.
 */
static void updateTempPID() {
    currentTemp = max31865.temperature(RNOMINAL, RREF);
    if (currentTemp < 0) currentTemp = lastTemp;
    float dt = (currentTime - lastPidTime) / 1000.0f;
    lastPidTime = currentTime;
    if (!heaterEnabled) {
        // Pause PID calculations when heater is disabled
        heatPower = 0.0f;
        heatCycles = PWM_CYCLE;
        return;
    }

    // Active target picks between brew and steam setpoints
    setTemp = steamFlag ? steamSetpoint : brewSetpoint;

    heatPower = calcPID(pGainTemp, iGainTemp, dGainTemp, setTemp, currentTemp, dt, pvFiltTemp,
                        iStateTemp, windupGuardTemp, dTauTemp);

    if (heatPower > 100.0f) heatPower = 100.0f;
    if (heatPower < 0.0f) heatPower = 0.0f;
    heatCycles = (int)((100.0f - heatPower) / 100.0f * PWM_CYCLE);
    lastTemp = currentTemp;
}

/**
 * @brief Apply time-proportioning control to the heater output.
 */
static void updateTempPWM() {
    if (!heaterEnabled) {
        digitalWrite(HEAT_PIN, LOW);
        heaterState = false;
        return;
    }
    if (currentTime - lastPwmTime >= (unsigned long)heatCycles) {
        digitalWrite(HEAT_PIN, HIGH);
        heaterState = true;
    }
    if (currentTime - lastPwmTime >= PWM_CYCLE) {
        digitalWrite(HEAT_PIN, LOW);
        heaterState = false;
        lastPwmTime = currentTime;
        nLoop = 0;
    }
    nLoop++;
}

static inline float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/**
 * @brief Apply PWM to the pump triac dimmer based on `pumpPower`.
 */
static void applyPumpPower() {
    float requested = clampf(pumpPower, 0.0f, 100.0f);
    float applied = requested;

    if (pumpPressureModeEnabled) {
        float limit = clampf(pressureSetpointBar, PRESSURE_SETPOINT_MIN, PRESSURE_SETPOINT_MAX);
        float sensed = lastPress;
        if (limit <= 0.0f) {
            applied = 0.0f;
        } else if (sensed > (limit + PRESSURE_LIMIT_TOL)) {
            if (sensed > 0.1f) {
                float ratio = (limit + PRESSURE_LIMIT_TOL) / sensed;
                ratio = clampf(ratio, 0.0f, 1.0f);
                applied = requested * ratio;
            } else {
                applied = 0.0f;
            }
        }
    }

    unsigned long nowMs = millis();
    if (pumpPressureModeEnabled) {
        float dt = 0.0f;
        if (lastPumpApplyMs == 0) {
            dt = PRESS_CYCLE / 1000.0f;  // assume at least one pressure cycle
        } else {
            dt = (nowMs - lastPumpApplyMs) / 1000.0f;
        }
        dt = clampf(dt, 0.0f, PUMP_PRESSURE_RAMP_MAX_DT);
        float maxIncrease = PUMP_PRESSURE_RAMP_RATE * dt;
        float allowed = lastPumpApplied + maxIncrease;
        if (applied > allowed) {
            applied = allowed;
        }
    }

    applied = clampf(applied, 0.0f, 100.0f);

    lastPumpApplyMs = nowMs;
    lastPumpApplied = applied;

    int percent = static_cast<int>(lroundf(applied));
    pumpDimmer.setPower(percent);
    pumpDimmer.setState(percent > 0 ? ON : OFF);
}

/**
 * @brief Sample pressure ADC and maintain a moving average buffer.
 */
static void updatePressure() {
    rawPress = analogRead(PRESS_PIN);
    pressNow = rawPress * pressGrad + pressInt;
    uint8_t idx = pressBuffIdx;
    pressSum -= pressBuff[idx];
    pressBuff[idx] = pressNow;
    pressSum += pressNow;
    pressBuffIdx++;
    if (pressBuffIdx >= PRESS_BUFF_SIZE) pressBuffIdx = 0;
    lastPress = pressSum / PRESS_BUFF_SIZE;

    if (pumpPressureModeEnabled) {
        applyPumpPower();
    }
}

/**
 * @brief Infer steam mode based on AC sense and recent zero-cross activity.
 */
static void updateSteamFlag() {
    ac = !digitalRead(AC_SENS);
    prevSteamFlag = steamFlag;
    int64_t now = esp_timer_get_time();
    if ((now - lastZcTime) > ZC_OFF * 1000 && ac) {
        acCount++;
        if (acCount > STEAM_MIN) {
            if (!steamHwFlag && steamDispFlag) steamResetPending = true;
            steamHwFlag = true;
        }
    } else {
        if (steamHwFlag) {
            if (steamDispFlag && steamResetPending) {
                steamDispFlag = false;
                steamResetPending = false;
            }
            steamHwFlag = false;
        }
        acCount = 0;
    }
    steamFlag = steamDispFlag || steamHwFlag;
}

/**
 * @brief Track pre-infusion phase and capture volume up to threshold pressure.
 */
static void updatePreFlow() {
    if (preFlow && lastPress > PRESS_THRESHOLD) {
        preFlow = false;
        preFlowVol = vol;
    }
}

/**
 * @brief Convert pulse counts to volumes and maintain shot volume.
 */
static void updateVols() {
    unsigned long pulses = pulseCount;
    vol = pulses * FLOW_CAL;
    shotVol = (preFlow || !shotFlag) ? 0.0f : (vol - preFlowVol);
}

// ISRs
/**
 * @brief Flow sensor ISR with simple debounce using `PULSE_MIN`.
 */
static void IRAM_ATTR flowInt() {
    int64_t now = esp_timer_get_time();
    if (now - lastPulseTime >= PULSE_MIN * 1000) {
        pulseCount++;
        lastPulseTime = now;
    }
}
// Helper: immediately disable the heater output and prevent PID updates.
// Also disables steam unless hardware AC sense keeps it active.
static void forceHeaterOff() {
    heaterEnabled = false;
    heatPower = 0.0f;
    heatCycles = PWM_CYCLE;
    digitalWrite(HEAT_PIN, LOW);
    heaterState = false;
    if (!steamHwFlag) {
        steamDispFlag = false;
        steamFlag = false;
    }
}

static void revertToSafeDefaults() {
    if (!heaterEnabled) {
        heaterEnabled = true;
        LOG("ESP-NOW: Heater default -> ON");
    }
    pumpPower = PUMP_POWER_DEFAULT;
    pressureSetpointBar = PRESSURE_SETPOINT_DEFAULT;
    pumpPressureModeEnabled = false;
    applyPumpPower();
    pumpMode = ESPNOW_PUMP_MODE_NORMAL;
    steamDispFlag = false;
    steamResetPending = false;
    steamFlag = steamDispFlag || steamHwFlag;
    setTemp = steamFlag ? steamSetpoint : brewSetpoint;
}

static void sendEspNowPacket() {
    EspNowPacket pkt{};
    pkt.shotFlag = shotFlag ? 1 : 0;
    pkt.steamFlag = steamFlag ? 1 : 0;
    pkt.heaterSwitch = heaterEnabled ? 1 : 0;
    pkt.shotTimeMs = shotFlag ? static_cast<uint32_t>(shotTime * 1000.0f) : 0;
    pkt.shotVolumeMl = shotVol;
    pkt.setTempC = setTemp;
    pkt.currentTempC = currentTemp;
    pkt.pressureBar = pressNow;
    pkt.steamSetpointC = steamSetpoint;
    pkt.brewSetpointC = brewSetpoint;
    pkt.pressureSetpointBar = pressureSetpointBar;
    pkt.pumpPressureMode = pumpPressureModeEnabled ? 1 : 0;
    const uint8_t* dest = g_haveDisplayPeer ? g_displayMac : nullptr;
    esp_err_t err = esp_now_send(dest, reinterpret_cast<uint8_t*>(&pkt), sizeof(pkt));
    if (err != ESP_OK) {
        LOG_ERROR("ESP-NOW: telemetry send failed (%d)", (int)err);
    }
}

static void applyControlPacket(const EspNowControlPacket& pkt, const uint8_t* mac) {
    if (pkt.type != ESPNOW_CONTROL_PACKET) return;
    if (pkt.revision && pkt.revision <= g_lastControlRevision) return;
    g_lastControlRevision = pkt.revision;

    LOG("ESP-NOW: Control received rev %u: heater=%d steam=%d brew=%.1f steamSet=%.1f "
        "pidP=%.2f pidI=%.2f pidGuard=%.2f "
        "pidD=%.2f pump=%.1f mode=%u pressSet=%.1f pressMode=%d",
        static_cast<unsigned>(pkt.revision), (pkt.flags & ESPNOW_CONTROL_FLAG_HEATER) != 0 ? 1 : 0,
        (pkt.flags & ESPNOW_CONTROL_FLAG_STEAM) != 0 ? 1 : 0, pkt.brewSetpointC, pkt.steamSetpointC,
        pkt.pidP, pkt.pidI, pkt.pidGuard, pkt.pidD, pkt.dTau, pkt.pumpPowerPercent,
        static_cast<unsigned>(pkt.pumpMode), pkt.pressureSetpointBar,
        (pkt.flags & ESPNOW_CONTROL_FLAG_PUMP_PRESSURE) ? 1 : 0);

    bool hv = (pkt.flags & ESPNOW_CONTROL_FLAG_HEATER) != 0;
    if (hv != heaterEnabled) {
        heaterEnabled = hv;
        if (!heaterEnabled) forceHeaterOff();
        LOG("ESP-NOW: Heater -> %s", heaterEnabled ? "ON" : "OFF");
    }

    bool sv = (pkt.flags & ESPNOW_CONTROL_FLAG_STEAM) != 0;
    if (sv != steamDispFlag) {
        steamDispFlag = sv;
        steamResetPending = false;
        steamFlag = steamDispFlag || steamHwFlag;
        setTemp = steamFlag ? steamSetpoint : brewSetpoint;
        LOG("ESP-NOW: Steam -> %s", steamFlag ? "ON" : "OFF");
    }

    float newBrew = clampf(pkt.brewSetpointC, BREW_MIN, BREW_MAX);
    float newSteam = clampf(pkt.steamSetpointC, STEAM_MIN_C, STEAM_MAX_C);
    bool setChanged = false;
    if (fabsf(newBrew - brewSetpoint) > 0.01f) {
        brewSetpoint = newBrew;
        setChanged = true;
    }
    if (fabsf(newSteam - steamSetpoint) > 0.01f) {
        steamSetpoint = newSteam;
        setChanged = true;
    }
    if (setChanged) {
        setTemp = steamFlag ? steamSetpoint : brewSetpoint;
        LOG("ESP-NOW: Setpoints Brew=%.1f Steam=%.1f", brewSetpoint, steamSetpoint);
    }

    float newP = clampf(pkt.pidP, 0.0f, 100.0f);
    float newI = clampf(pkt.pidI, 0.0f, 2.0f);
    float newGuard = clampf(pkt.pidGuard, 0.0f, 100.0f);
    float newD = clampf(pkt.pidD, 0.0f, 500.0f);
    float newDTau = clampf(pkt.dTau, 0.0f, 2.0f);

    if (fabsf(newP - pGainTemp) > 0.01f) {
        pGainTemp = newP;
    }
    if (fabsf(newI - iGainTemp) > 0.01f) {
        iGainTemp = newI;
    }
    if (fabsf(newGuard - windupGuardTemp) > 0.01f) {
        windupGuardTemp = newGuard;
    }
    if (fabsf(newD - dGainTemp) > 0.1f) {
        dGainTemp = newD;
    }
    if (fabs(newDTau - dTauTemp) > 0.01f) {
        dTauTemp = newDTau;
    }

    float newPump = clampf(pkt.pumpPowerPercent, 0.0f, 100.0f);
    if (fabsf(newPump - pumpPower) > 0.1f) {
        pumpPower = newPump;
        applyPumpPower();
    }

    float newPressureSetpoint = clampf(pkt.pressureSetpointBar, 0.0f, 15.0f);
    if (fabsf(newPressureSetpoint - pressureSetpointBar) > 0.1f) {
        pressureSetpointBar = newPressureSetpoint;
    }

    pumpMode = static_cast<EspNowPumpMode>(pkt.pumpMode);

    float newPressureSet =
        clampf(pkt.pressureSetpointBar, PRESSURE_SETPOINT_MIN, PRESSURE_SETPOINT_MAX);
    if (fabsf(newPressureSet - pressureSetpointBar) > 0.01f) {
        pressureSetpointBar = newPressureSet;
        LOG("ESP-NOW: Pressure setpoint -> %.1f bar", pressureSetpointBar);
        if (pumpPressureModeEnabled) applyPumpPower();
    }

    bool newPressureMode = (pkt.flags & ESPNOW_CONTROL_FLAG_PUMP_PRESSURE) != 0;
    if (newPressureMode != pumpPressureModeEnabled) {
        pumpPressureModeEnabled = newPressureMode;
        LOG("ESP-NOW: Pump pressure mode -> %s", pumpPressureModeEnabled ? "ON" : "OFF");
        applyPumpPower();
    }

    if (mac) {
        memcpy(g_displayMac, mac, ESP_NOW_ETH_ALEN);
        g_haveDisplayPeer = true;
    }
}

static void espNowRecv(const uint8_t* mac, const uint8_t* data, int len) {
    if (!data || len <= 0) return;

    if (len >= 2 && data[0] == ESPNOW_HANDSHAKE_REQ) {
        uint8_t requestedChannel = data[1];
        if (mac) {
            esp_now_peer_info_t peer{};
            memcpy(peer.peer_addr, mac, ESP_NOW_ETH_ALEN);
            peer.channel = requestedChannel;
            peer.ifidx = WIFI_IF_STA;
            peer.encrypt = false;
            if (esp_now_is_peer_exist(peer.peer_addr)) {
                esp_now_mod_peer(&peer);
            } else {
                esp_now_add_peer(&peer);
            }
            memcpy(g_displayMac, mac, ESP_NOW_ETH_ALEN);
            g_haveDisplayPeer = true;
        }

        if (!applyEspNowChannel(requestedChannel, true, false)) {
            LOG_ERROR("ESP-NOW: failed to switch to channel %u", requestedChannel);
            return;
        }
        g_espnowHandshake = true;
        g_espnowStatus = "linked";
        unsigned long nowMs = millis();
        g_lastDisplayAckMs = nowMs;
        g_lastChannelHopMs = nowMs;
        g_espnowScanning = false;
        g_nextScanChannel = requestedChannel;
        uint8_t ack[2] = {ESPNOW_HANDSHAKE_ACK, g_espnowChannel};
        if (mac) {
            esp_err_t ackErr = esp_now_send(mac, ack, sizeof(ack));
            if (ackErr != ESP_OK) {
                LOG_ERROR("ESP-NOW: handshake ack send failed (%d)", static_cast<int>(ackErr));
            }
        }
        return;
    }

    if (len == sizeof(EspNowControlPacket) && data[0] == ESPNOW_CONTROL_PACKET) {
        applyControlPacket(*reinterpret_cast<const EspNowControlPacket*>(data), mac);
        g_lastDisplayAckMs = millis();
        g_espnowHandshake = true;
        g_espnowStatus = "linked";
        return;
    }

    if (len == 1 && data[0] == ESPNOW_SENSOR_ACK) {
        g_lastDisplayAckMs = millis();
        return;
    }
}

static bool ensureEspNowCore(bool silent = false) {
    if (g_espnowCoreInit) return true;

    esp_err_t err = esp_now_init();
    if (err != ESP_OK) {
        if (!silent) LOG_ERROR("ESP-NOW: init failed (%d)", static_cast<int>(err));
        g_espnowStatus = "error";
        return false;
    }

    memset(&g_broadcastPeerInfo, 0, sizeof(g_broadcastPeerInfo));
    memcpy(g_broadcastPeerInfo.peer_addr, ESPNOW_BROADCAST_ADDR, ESP_NOW_ETH_ALEN);
    g_broadcastPeerInfo.ifidx = WIFI_IF_STA;
    g_broadcastPeerInfo.encrypt = false;

    esp_now_register_recv_cb(espNowRecv);
    g_espnowHandshake = false;
    g_haveDisplayPeer = false;
    g_lastDisplayAckMs = 0;
    g_lastControlRevision = 0;

    g_espnowCoreInit = true;
    g_espnowBroadcastPeerAdded = false;
    return true;
}

static bool applyEspNowChannel(uint8_t channel, bool forceSetWifiChannel, bool silent) {
    if (channel < ESPNOW_FIRST_CHANNEL || channel > ESPNOW_LAST_CHANNEL) return false;

    if (!ensureEspNowCore(silent)) return false;

    if (forceSetWifiChannel && WiFi.status() != WL_CONNECTED) {
        esp_err_t err = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
        if (err != ESP_OK) {
            if (!silent) {
                LOG_ERROR("ESP-NOW: failed to set channel %u (%d)", channel, static_cast<int>(err));
            }
            return false;
        }
    }

    g_broadcastPeerInfo.channel = channel;
    esp_err_t res;
    if (!g_espnowBroadcastPeerAdded) {
        res = esp_now_add_peer(&g_broadcastPeerInfo);
        if (res == ESP_OK || res == ESP_ERR_ESPNOW_EXIST) {
            g_espnowBroadcastPeerAdded = true;
        } else {
            if (!silent) LOG_ERROR("ESP-NOW: add peer failed (%d)", static_cast<int>(res));
            return false;
        }
    } else {
        res = esp_now_mod_peer(&g_broadcastPeerInfo);
        if (res == ESP_ERR_ESPNOW_NOT_FOUND) {
            g_espnowBroadcastPeerAdded = false;
            return applyEspNowChannel(channel, forceSetWifiChannel, silent);
        }
        if (res != ESP_OK) {
            if (!silent) LOG_ERROR("ESP-NOW: update peer failed (%d)", static_cast<int>(res));
            return false;
        }
    }

    g_espnowChannel = channel;
    if (!silent) {
        bool manual = forceSetWifiChannel && WiFi.status() != WL_CONNECTED;
        LOG("ESP-NOW: using channel %u%s", channel, manual ? " (manual)" : "");
    }
    return true;
}

static void initEspNow() {
    wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
    uint8_t channel = 0;
    if (esp_wifi_get_channel(&channel, &second) != ESP_OK || channel < ESPNOW_FIRST_CHANNEL ||
        channel > ESPNOW_LAST_CHANNEL) {
        LOG_ERROR("ESP-NOW: invalid channel %u", channel);
        g_espnowStatus = "error";
        g_espnowChannel = 0;
        return;
    }

    if (!applyEspNowChannel(channel, false, true)) {
        g_espnowStatus = "error";
        return;
    }

    g_nextScanChannel = channel;
    g_espnowScanning = false;
    if (!g_espnowHandshake) g_espnowStatus = "enabled";

    uint8_t mac[6];
    if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK) {
        char buf[18];
        snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3],
                 mac[4], mac[5]);
        g_espnowMac = buf;
    }

    LOG("ESP-NOW: initialized on channel %u - awaiting handshake", channel);
}

static void maybeHopEspNowChannel() {
    if (g_espnowHandshake) {
        if (g_espnowScanning) {
            g_espnowScanning = false;
            g_nextScanChannel =
                g_espnowChannel >= ESPNOW_FIRST_CHANNEL ? g_espnowChannel : ESPNOW_FIRST_CHANNEL;
        }
        return;
    }

    if (g_wifiNtpConnecting) return;

    if (WiFi.status() == WL_CONNECTED) {
        return;
    }

    unsigned long now = millis();
    if ((now - g_lastChannelHopMs) < ESPNOW_CHANNEL_HOLD_MS) return;

    if (!ensureEspNowCore(true)) return;

    uint8_t channel = g_nextScanChannel;
    g_nextScanChannel = (g_nextScanChannel >= ESPNOW_LAST_CHANNEL)
                            ? ESPNOW_FIRST_CHANNEL
                            : static_cast<uint8_t>(g_nextScanChannel + 1);

    if (applyEspNowChannel(channel, true, true)) {
        g_lastChannelHopMs = now;
        if (!g_espnowScanning || channel == ESPNOW_FIRST_CHANNEL) {
            LOG("ESP-NOW: scanning on channel %u", channel);
        }
        g_espnowScanning = true;
        g_espnowStatus = "scanning";
    }
}
static void syncClockFromWifi() {
    static bool attempted = false;
    static bool connecting = false;
    static unsigned long lastTry = 0;
    static unsigned long connectStart = 0;
    static bool loggedConnected = false;

    wl_status_t status = WiFi.status();

    if (g_clockSynced) {
        if (status == WL_CONNECTED) {
            LOG("WiFi: disconnecting after NTP sync");
            WiFi.disconnect(false, true);
        }
        connecting = false;
        g_wifiNtpConnecting = false;
        return;
    }

    if (status == WL_CONNECTED) {
        if (!loggedConnected) {
            LOG("WiFi: %s  IP=%s  GW=%s  RSSI=%d dBm", wifiStatusName(status),
                WiFi.localIP().toString().c_str(), WiFi.gatewayIP().toString().c_str(),
                WiFi.RSSI());
            loggedConnected = true;
        }

        syncClock();
        if (g_clockSynced) {
            WiFi.disconnect(false, true);
            LOG("WiFi: NTP sync complete; Wi-Fi disabled");
        }
        g_wifiNtpConnecting = false;
        return;
    }

    loggedConnected = false;

    unsigned long now = millis();
    if (!connecting && (!attempted || (now - lastTry) >= 10000)) {
        LOG("WiFi: connecting to '%s' for NTP sync", WIFI_SSID);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        connecting = true;
        attempted = true;
        connectStart = now;
        lastTry = now;
        g_wifiNtpConnecting = true;
        return;
    }

    if (connecting) {
        wl_status_t res = static_cast<wl_status_t>(WiFi.waitForConnectResult(100));
        if (res == WL_CONNECTED) {
            connecting = false;
            g_wifiNtpConnecting = false;
        } else if ((millis() - connectStart) > 10000) {
            connecting = false;
            g_wifiNtpConnecting = false;
            WiFi.disconnect(false, true);
        }
        delay(1);
    }
    if (!connecting) g_wifiNtpConnecting = false;
}

}  // namespace

// RBDdimmer uses the same ZC pin and installs its own ISR. To avoid
// conflicting attachInterrupt() calls, provide a global hook that the library
// can invoke from its ISR so we still record zero-cross events.
extern "C" void IRAM_ATTR user_zc_hook() {
    int64_t now = esp_timer_get_time();
    if (now - lastZcTime >= 6000) {
        lastZcTime = now;
        zcCount++;
    }
}

namespace gag {

/**
 * @copydoc gag::setup()
 */
void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(300);
    LOG("Booting? FW %s", VERSION);
#if defined(CONFIG_FREERTOS_TIMER_TASK_STACK_DEPTH)
    LOG("RTOS: Tmr Svc stack depth=%d (words)", (int)CONFIG_FREERTOS_TIMER_TASK_STACK_DEPTH);
#endif

#if defined(ARDUINO_ARCH_ESP32)
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
#endif

    pinMode(MAX_CS, OUTPUT);
    pinMode(HEAT_PIN, OUTPUT);
    pinMode(PRESS_PIN, INPUT);
    pinMode(FLOW_PIN, INPUT_PULLUP);
    pinMode(ZC_PIN, INPUT);
    pinMode(AC_SENS, INPUT_PULLUP);
    pinMode(PUMP_PIN, OUTPUT);
    pumpDimmer.begin(NORMAL_MODE, OFF);
    digitalWrite(HEAT_PIN, LOW);
    heaterState = false;
    applyPumpPower();
    max31865.begin(MAX31865_2WIRE);

    // Initialize filtered PV & lastTemp to avoid first-step D kick
    currentTemp = max31865.temperature(RNOMINAL, RREF);
    if (currentTemp < 0) currentTemp = 0.0f;
    pvFiltTemp = currentTemp;
    lastTemp = currentTemp;

    // zero pressure using a few samples to average noise
    float startP = 0.0f;
    const int samples = 4;
    for (int i = 0; i < samples; ++i) {
        startP += analogRead(PRESS_PIN);
    }
    startP = startP / samples * pressGrad + pressInt;
    if (fabsf(startP) <= PRESSURE_TOL) {
        pressInt -= startP;
        LOG("Pressure Intercept reset to %f", pressInt);
    }

    // Count both rising and falling edges from the flow sensor to
    // double the pulse resolution.  CHANGE triggers the ISR on any
    // transition and `PULSE_MIN` guards against spurious bounce.
    attachInterrupt(digitalPinToInterrupt(FLOW_PIN), flowInt, CHANGE);

    pulseCount = 0;
    startTime = millis();
    lastPidTime = startTime;
    lastPwmTime = startTime;
    lastPulseTime = esp_timer_get_time();
    setupComplete = true;

    WiFi.mode(WIFI_STA);
#if defined(ARDUINO_ARCH_ESP32)
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(false);
#endif

    initEspNow();

    LOG("Pins: FLOW=%d ZC=%d HEAT=%d AC_SENS=%d PRESS=%d  SPI{CS=%d}", FLOW_PIN, ZC_PIN, HEAT_PIN,
        AC_SENS, PRESS_PIN, MAX_CS);
}

/**
 * @copydoc gag::loop()
 */
void loop() {
    currentTime = millis();

    // If the display stops acknowledging, fall back to safe defaults
    if (g_espnowHandshake && g_lastDisplayAckMs &&
        (currentTime - g_lastDisplayAckMs) > DISPLAY_TIMEOUT_MS) {
        LOG("ESP-NOW: display timeout after %lu ms ? reverting to defaults",
            currentTime - g_lastDisplayAckMs);
        g_espnowHandshake = false;
        g_haveDisplayPeer = false;
        g_lastControlRevision = 0;
        g_espnowStatus = "timeout";
        revertToSafeDefaults();
    }

    checkShotStartStop();
    if (currentTime - lastPidTime >= PID_CYCLE) updateTempPID();
    updateTempPWM();
    updatePressure();
    updatePreFlow();
    updateVols();
    updateSteamFlag();

    syncClockFromWifi();
    maybeHopEspNowChannel();
    // Update shot time continuously while shot is active (seconds)
    if (shotFlag && (zcCount > lastZcCount)) {
        shotTime = (currentTime - shotStart) / 1000.0f;
    }
    lastZcCount = zcCount;

    if (g_espnowHandshake && (currentTime - lastEspNowTime) >= ESP_CYCLE) {
        sendEspNowPacket();
        lastEspNowTime = currentTime;
    }

    if (debugPrint && (currentTime - lastLogTime) > LOG_CYCLE) {
        LOG("Pressure: Raw=%d, Now=%0.2f Last=%0.2f", rawPress, pressNow, lastPress);
        LOG("Temp: Set=%0.1f, Current=%0.2f", setTemp, currentTemp);
        LOG("Heat: Power=%0.1f, Cycles=%d", heatPower, heatCycles);
        LOG("Vol: Pulses=%lu, Vol=%0.2f", pulseCount, vol);
        LOG("Pump: ZC Count =%lu", zcCount);
        LOG("Flags: Steam=%d, Shot=%d", steamFlag, shotFlag);
        LOG("AC Count=%d", acCount);
        LOG("PID: P=%0.1f, I=%0.2f, D=%0.1f, G=%0.1f", pGainTemp, iGainTemp, dGainTemp,
            windupGuardTemp);
        LOG("");
        lastLogTime = currentTime;
    }
}

}  // namespace gag
