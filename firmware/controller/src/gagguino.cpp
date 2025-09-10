/**
 * @file gagguino.cpp
 * @brief Gagguino firmware: ESP32 control for Gaggia Classic.
 *
 * High‑level responsibilities:
 * - Temperature control using a MAX31865 RTD amplifier (PT100) and PID.
 * - Heater PWM drive.
 * - Flow, pressure and shot timing measurement with debounced ISR counters.
 * - Wi‑Fi with optional OTA updates and ESP‑NOW telemetry.
 * - Robust OTA (ArduinoOTA) that throttles other work while an update runs.
 *
 * Hardware pins (ESP32 default board mapping):
 * - FLOW_PIN (26)   : Flow sensor input (interrupt on CHANGE)
 * - ZC_PIN (25)     : AC zero‑cross detect (interrupt on RISING)
 * - HEAT_PIN (27)   : Boiler relay/SSR output (PWM windowing)
 * - AC_SENS (14)    : Steam switch sense (digital input)
 * - MAX_CS (16)     : MAX31865 SPI chip‑select
 * - PRESS_PIN (35)  : Analog pressure sensor input
 */
#include "gagguino.h"

#include <Adafruit_MAX31865.h>
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <ctype.h>
#include <esp_now.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <math.h>

#include <cstdarg>
#include <string.h>
#include <map>

#include "secrets.h"  // WIFI_*
#include "espnow_packet.h"

#define VERSION "7.0"
#define STARTUP_WAIT 1000
#define SERIAL_BAUD 115200

/**
 * @brief Lightweight printf‑style logger to the serial console.
 */
static inline void LOG(const char* fmt, ...) {
    static char buf[192];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Serial.println(buf);
}

/**
 * @brief Log a significant error.
 *
 * Maintains a small rolling buffer of recent error messages to avoid
 * unbounded growth while still providing context for debugging.
 */

namespace {
constexpr int FLOW_PIN = 26, ZC_PIN = 25, HEAT_PIN = 27, AC_SENS = 14;
constexpr int MAX_CS = 16;
constexpr int PRESS_PIN = 35;

constexpr unsigned long PRESS_CYCLE = 100, PID_CYCLE = 250, PWM_CYCLE = 250,
                   ESP_CYCLE = 500, LOG_CYCLE = 2000;

constexpr unsigned long IDLE_CYCLE = 5000;       // ms between publishes when idle (reduced chatter)
constexpr unsigned long SHOT_CYCLE = 1000;       // ms between publishes during a shot
constexpr unsigned long OTA_ENABLE_MS = 300000;  // ms OTA window after enabling

// Brew & Steam setpoint limits
constexpr float BREW_MIN = 90.0f, BREW_MAX = 99.0f;
constexpr float STEAM_MIN_C = 145.0f, STEAM_MAX_C = 155.0f;

// Default steam setpoint (within limits)
constexpr float STEAM_DEFAULT = 152.0f;

constexpr float RREF = 430.0f, RNOMINAL = 100.0f;
// Default PID params
// Default PID parameters tuned for stability
// Kp: 15–16 [out/°C]
// Ki: 0.3–0.5 [out/(°C·s)] → start at 0.35
// Kd: 50–70 [out·s/°C] → start at 60
// guard: ±8–±12% integral clamp on 0–100% heater
constexpr float P_GAIN_TEMP = 15.0f, I_GAIN_TEMP = 0.35f, D_GAIN_TEMP = 60.0f,
                WINDUP_GUARD_TEMP = 10.0f;
// Derivative filter time constant (seconds), exposed to HA
constexpr float D_TAU_TEMP = 0.8f;

constexpr float PRESS_TOL = 0.4f, PRESS_GRAD = 0.00903f, PRESS_INT_0 = -4.0f;
constexpr int PRESS_BUFF_SIZE = 14;
constexpr float PRESS_THRESHOLD = 9.0f;

// FLOW_CAL in mL per pulse (1 cc == 1 mL)
constexpr float FLOW_CAL = 0.246f;
constexpr unsigned long PULSE_MIN = 3;  // ms debounce (bounce + double-edges)

constexpr unsigned ZC_MIN = 4;
constexpr unsigned long ZC_WAIT = 2000, ZC_OFF = 1000, SHOT_RESET = 30000;
constexpr unsigned long AC_WAIT = 100;
constexpr int STEAM_MIN = 20;

const bool streamData = true, debugPrint = false;
}  // namespace

// ---------- Devices / globals ----------
namespace {
Adafruit_MAX31865 max31865(MAX_CS);

// Rolling buffer of recent significant error messages
static String g_errorLog;

/**
 * @brief Log a significant error.
 */
static inline void LOG_ERROR(const char* fmt, ...) {
    static char buf[192];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Serial.println(buf);

    if (!g_errorLog.isEmpty()) g_errorLog += '\n';
    g_errorLog += buf;

    constexpr size_t MAX_ERR_LOG = 512;
    if (g_errorLog.length() > MAX_ERR_LOG) {
        int cut = g_errorLog.length() - MAX_ERR_LOG;
        cut = g_errorLog.indexOf('\n', cut);
        if (cut >= 0) g_errorLog = g_errorLog.substring(cut + 1);
    }

}

// Temps / PID
float currentTemp = 0.0f, lastTemp = 0.0f, pvFiltTemp = 0.0f;
float brewSetpoint = 95.0f;           // HA-controllable (90–99)
float steamSetpoint = STEAM_DEFAULT;  // HA-controllable (145–155)
float setTemp = brewSetpoint;         // active target (brew or steam)
float iStateTemp = 0.0f, heatPower = 0.0f;
// Live-tunable PID parameters (default to constexprs above)
float pGainTemp = P_GAIN_TEMP, iGainTemp = I_GAIN_TEMP, dGainTemp = D_GAIN_TEMP,
      windupGuardTemp = WINDUP_GUARD_TEMP, dTauTemp = D_TAU_TEMP;
int heatCycles = 0;
bool heaterState = false;
bool heaterEnabled = true;  // HA switch default ON at boot

// Pressure
int rawPress = 0;
float lastPress = 0.0f, pressNow = 0.0f, pressSum = 0.0f, pressGrad = PRESS_GRAD,
      pressInt = PRESS_INT_0;
float pressBuff[PRESS_BUFF_SIZE] = {0};
uint8_t pressBuffIdx = 0;

// Time/shot
unsigned long nLoop = 0, currentTime = 0, lastPidTime = 0, lastPwmTime = 0,
              lastEspNowTime = 0, lastLogTime = 0;
// microsecond timestamps for ISR debounce
volatile int64_t lastPulseTime = 0;
unsigned long shotStart = 0, startTime = 0;
float shotTime = 0;  //

// Flow / flags
volatile unsigned long pulseCount = 0;
volatile unsigned long zcCount = 0;
volatile int64_t lastZcTime = 0;  // microsecond timestamp
int vol = 0, preFlowVol = 0, shotVol = 0;
unsigned int lastVol = 0;
bool prevSteamFlag = false, ac = false;
int acCount = 0;
bool shotFlag = false, preFlow = false, steamFlag = false, setupComplete = false, debugData = false;

// OTA
static bool otaInitialized = false;
static bool otaActive = false;      // minimize other work while OTA is in progress
static unsigned long otaStart = 0;  // millis when OTA was enabled

// ---------- OTA ----------
/**
 * @brief Initialize ArduinoOTA once Wi‑Fi is connected.
 *
 * Notes:
 * - Hostname is derived from MAC address for stability.
 * - If `OTA_PASSWORD` or `OTA_PASSWORD_HASH` is defined (via build flags),
 *   OTA authentication is enforced.
 * - During OTA, the heater output is disabled and main work throttled.
 */
static void ensureOta() {
    if (otaActive && otaStart && (millis() - otaStart >= OTA_ENABLE_MS)) {
        otaActive = false;
        otaStart = 0;
        // OTA status idle
        LOG("OTA: window expired");
    }

    if (otaInitialized || WiFi.status() != WL_CONNECTED) return;

    // Derive a stable hostname from MAC
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char host[32];
    snprintf(host, sizeof(host), "gaggia-%02X%02X%02X", mac[3], mac[4], mac[5]);

    ArduinoOTA.setHostname(host);
#if defined(OTA_PASSWORD_HASH)
    ArduinoOTA.setPasswordHash(OTA_PASSWORD_HASH);
#elif defined(OTA_PASSWORD)
    ArduinoOTA.setPassword(OTA_PASSWORD);
#endif

    ArduinoOTA.onStart([]() {
        LOG("OTA: Start (%s)", ArduinoOTA.getCommand() == U_FLASH ? "flash" : "fs");
        otaActive = true;  // signal loop to reduce load
        otaStart = millis();
        // OTA status active
        // Turn off heater to reduce power/noise during OTA
        digitalWrite(HEAT_PIN, LOW);
        heaterState = false;
    });
    ArduinoOTA.onEnd([]() {
        LOG("OTA: End");
        otaActive = false;
        otaStart = 0;
        // OTA status idle
        // (Optional) re-attach interrupts if you detached them on start
        // attachInterrupt(digitalPinToInterrupt(FLOW_PIN), flowInt, CHANGE);
        // attachInterrupt(digitalPinToInterrupt(ZC_PIN), zcInt, RISING);
    });
    ArduinoOTA.setTimeout(120000);  // 120s OTA socket timeout (default is shorter)
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        static unsigned int lastPct = 101;
        unsigned int pct = (total ? (progress * 100u / total) : 0u);
        if (pct != lastPct && (pct % 10u == 0u)) {
            LOG("OTA: %u%%", pct);
            lastPct = pct;
        }
    });
    ArduinoOTA.onError([](ota_error_t error) {
        const char* msg = "UNKNOWN";
        switch (error) {
            case OTA_AUTH_ERROR:
                msg = "AUTH";
                break;
            case OTA_BEGIN_ERROR:
                msg = "BEGIN";
                break;
            case OTA_CONNECT_ERROR:
                msg = "CONNECT";
                break;
            case OTA_RECEIVE_ERROR:
                msg = "RECEIVE";
                break;
            case OTA_END_ERROR:
                msg = "END";
                break;
        }
        LOG_ERROR("OTA: Error %d (%s)", (int)error, msg);
        otaActive = false;
        otaStart = 0;
        // OTA status error
    });

    ArduinoOTA.begin();
    otaInitialized = true;
    LOG("OTA: Ready as %s.local", host);
}

/**
 * @brief PID controller with integral windup guard and filtered derivative.
 * @param Kp Proportional gain
 * @param Ki Integral gain
 * @param Kd Derivative gain
 * @param sp Setpoint (target)
 * @param pv Process value (measured)
 * @param dt Timestep in seconds
 * @param pvFilt Storage for filtered process variable (updated)
 * @param iSum Integral accumulator (updated)
 * @param guard Absolute limit for iTerm contribution (anti-windup)
 * @param dTau Derivative low-pass filter time constant (seconds)
 * @return Control output (unclamped)
 */
// Continuous-time gains: Kp [out/°C], Ki [out/(°C·s)], Kd [out·s/°C]

// ──────────────────────────────────────────────────────────────────────────────
//  PID: dt-scaled I & D, iTerm clamp, derivative LPF, conditional integration
// ──────────────────────────────────────────────────────────────────────────────
static float calcPID(float Kp, float Ki, float Kd, float sp, float pv,
                     float dt,             // seconds (0.5 at 2 Hz)
                     float& pvFilt,        // filtered PV (state)
                     float& iSum,          // ∫err dt  (state)
                     float guard,          // clamp on iTerm (output units)
                     float dTau = 1.0f,    // derivative LPF time const (s)
                     float outMin = 0.0f,  // actuator limits (for cond. integration)
                     float outMax = 100.0f) {
    // 1) Error
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

    // 6) Conditional integration: don’t integrate when pushing into saturation
    if ((u >= outMax && err > 0.0f) || (u <= outMin && err < 0.0f)) {
        iSum -= err * dt;  // undo this step’s integral
        iTerm = Ki * iSum;
        if (iTerm > guard) iTerm = guard;
        if (iTerm < -guard) iTerm = -guard;
        u = pTerm + iTerm + dTerm;
    }
    return u;
}

// --------------- espresso logic ---------------
/**
 * @brief Detect shot start/stop based on zero‑cross events and steam transitions.
 */
static void checkShotStartStop() {
    if ((zcCount >= ZC_MIN) && !shotFlag && setupComplete &&
        ((currentTime - startTime) > ZC_WAIT)) {
        shotStart = currentTime;
        shotTime = 0;
        shotFlag = true;
        pulseCount = 0;
        preFlow = true;
        preFlowVol = 0;
    }
    unsigned long lastZcTimeMs = lastZcTime / 1000;
    if ((steamFlag && !prevSteamFlag) ||
        (currentTime - lastZcTimeMs >= SHOT_RESET && shotFlag && currentTime > lastZcTimeMs)) {
        pulseCount = 0;
        lastVol = 0;
        shotVol = 0;
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
                        iStateTemp, windupGuardTemp, /*dTau=*/dTauTemp, /*outMin=*/0.0f,
                        /*outMax=*/100.0f);

    if (heatPower > 100.0f) heatPower = 100.0f;
    if (heatPower < 0.0f) heatPower = 0.0f;
    heatCycles = (int)((100.0f - heatPower) / 100.0f * PWM_CYCLE);
    lastTemp = currentTemp;
}

/**
 * @brief Apply time‑proportioning control to the heater output.
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
}

/**
 * @brief Infer steam mode based on AC sense and recent zero‑cross activity.
 */
static void updateSteamFlag() {
    ac = !digitalRead(AC_SENS);
    prevSteamFlag = steamFlag;
    int64_t now = esp_timer_get_time();
    if ((now - lastZcTime) > ZC_OFF * 1000 && ac) {
        acCount++;
        if (acCount > STEAM_MIN) {
            steamFlag = true;
        }
    } else {
        steamFlag = false;
        acCount = 0;
    }
}

/**
 * @brief Track pre‑infusion phase and capture volume up to threshold pressure.
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
    vol = (int)(pulseCount * FLOW_CAL);
    lastVol = vol;
    shotVol = (preFlow || !shotFlag) ? 0 : (vol - preFlowVol);
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
/**
 * @brief Zero‑cross detect ISR: records pump activity timing.
 */
static void IRAM_ATTR zcInt() {
    int64_t now = esp_timer_get_time();
    // Guard against spurious re-triggers/noise (<~6 ms @ 50 Hz)
    if (now - lastZcTime < 6000) {
        return;
    }
    lastZcTime = now;
    zcCount++;
}

static void sendEspNowPacket() {
    EspNowPacket pkt{};
    pkt.shotFlag = shotFlag ? 1 : 0;
    pkt.steamFlag = steamFlag ? 1 : 0;
    pkt.heaterSwitch = heaterEnabled ? 1 : 0;
    pkt.shotTimeMs = shotFlag ? static_cast<uint32_t>(currentTime - shotStart) : 0;
    pkt.shotVolumeMl = static_cast<float>(shotVol);
    pkt.setTempC = setTemp;
    pkt.currentTempC = currentTemp;
    pkt.pressureBar = pressNow;
    pkt.steamSetpointC = steamSetpoint;
    pkt.brewSetpointC = brewSetpoint;
    esp_now_send(nullptr, reinterpret_cast<uint8_t*>(&pkt), sizeof(pkt));
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(300);
    LOG("Booting… FW %s", VERSION);

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
    digitalWrite(HEAT_PIN, LOW);
    heaterState = false;

    // Initialize MAX31865 (set wiring: 2/3/4-wire as appropriate)
    max31865.begin(MAX31865_2WIRE);

    // Initialize filtered PV & lastTemp to avoid first-step D kick
    currentTemp = max31865.temperature(RNOMINAL, RREF);
    if (currentTemp < 0) currentTemp = 0.0f;
    pvFiltTemp = currentTemp;
    lastTemp = currentTemp;

    // zero pressure
    float startP = analogRead(PRESS_PIN) * pressGrad + pressInt;
    if (startP > -PRESS_TOL && startP < PRESS_TOL) {
        pressInt -= startP;
        LOG("Pressure Intercept reset to %f", pressInt);
    }

    attachInterrupt(digitalPinToInterrupt(FLOW_PIN), flowInt, RISING);
    attachInterrupt(digitalPinToInterrupt(ZC_PIN), zcInt, RISING);

    pulseCount = 0;
    startTime = millis();
    lastPidTime = startTime;
    lastPwmTime = startTime;
    lastPulseTime = esp_timer_get_time();
    setupComplete = true;

    WiFi.mode(WIFI_STA);
    esp_wifi_set_channel(ESPNOW_CHAN, WIFI_SECOND_CHAN_NONE);
    if (esp_now_init() == ESP_OK) {
        esp_now_peer_info_t peer{};
        memcpy(peer.peer_addr, DISPLAY_MAC_ADDR, ESP_NOW_ETH_ALEN);
        peer.ifidx = ESP_IF_WIFI_STA;
        peer.channel = ESPNOW_CHAN;
        peer.encrypt = false;
        esp_now_add_peer(&peer);
    }
    LOG("Pins: FLOW=%d ZC=%d HEAT=%d AC_SENS=%d PRESS=%d  SPI{CS=%d}", FLOW_PIN, ZC_PIN, HEAT_PIN,
        AC_SENS, PRESS_PIN, MAX_CS);
    LOG("ESP-NOW: channel %u", ESPNOW_CHAN);
}

/**
 * @copydoc gag::loop()
 */
void loop() {
    currentTime = millis();

    // Handle OTA as early and as often as possible
    ensureOta();
    if (WiFi.status() == WL_CONNECTED) ArduinoOTA.handle();

    // If OTA is active, keep loop lean
    if (otaActive) {
        // (Optional) Detach high-rate ISRs during OTA to reduce CPU/latency spikes
        // static bool isrDetached = false;
        // if (!isrDetached) {
        //     detachInterrupt(digitalPinToInterrupt(FLOW_PIN));
        //     detachInterrupt(digitalPinToInterrupt(ZC_PIN));
        //     isrDetached = true;
        // }
        return;
    }

    // Normal work (only when NOT doing OTA)
    {
        checkShotStartStop();
        if (currentTime - lastPidTime >= PID_CYCLE) updateTempPID();
        updateTempPWM();
        updatePressure();
        updatePreFlow();
        updateVols();
        updateSteamFlag();
    }

    // Update shot time continuously while shot is active (seconds)
    if (shotFlag) {
        shotTime = (currentTime - shotStart) / 1000.0f;
    }

    if ((currentTime - lastEspNowTime) >= ESP_CYCLE) {
        sendEspNowPacket();
        lastEspNowTime = currentTime;
    }

    if (!otaActive && debugPrint &&
        (currentTime - lastLogTime) > LOG_CYCLE) { /* optional debug printing */
        LOG("Pressure: Raw=%d, Now=%0.2f Last=%0.2f", rawPress, pressNow, lastPress);
        LOG("Temp: Set=%0.1f, Current=%0.2f", setTemp, currentTemp);
        LOG("Heat: Power=%0.1f, Cycles=%d", heatPower, heatCycles);
        LOG("Vol: Pulses=%lu, Vol=%d", pulseCount, vol);
        LOG("Pump: ZC Count =%lu", zcCount);
        LOG("Flags: Steam=%d, Shot=%d", steamFlag, shotFlag);
        LOG("AC Count=%d", acCount);
        LOG("");
        lastLogTime = currentTime;
    }
}

}  // namespace gag
