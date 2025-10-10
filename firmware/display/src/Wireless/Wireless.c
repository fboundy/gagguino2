#include "Wireless.h"

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "mqtt_client.h"
#include "secrets.h"
#include "mqtt_topics.h"
#include "espnow_protocol.h"
#include "version.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#define ESPNOW_TIMEOUT_MS 5000
#define ESPNOW_PING_PERIOD_MS 1000

static const char *TAG_WIFI = "WiFi";
static const char *TAG_MQTT = "MQTT";
static const char *TAG_ESPNOW = "ESP-NOW";

// -----------------------------------------------------------------------------
// MQTT topics
// -----------------------------------------------------------------------------
static char TOPIC_HEATER[128];
static char TOPIC_HEATER_SET[128];
static char TOPIC_STEAM[128];
static char TOPIC_STEAM_SET[128];
static char TOPIC_CURTEMP[128];
static char TOPIC_SETTEMP[128];
static char TOPIC_PRESSURE[128];
static char TOPIC_SHOTVOL[128];
static char TOPIC_SHOT[128];
static char TOPIC_ZC_COUNT_STATE[128];
static char TOPIC_BREW_STATE[128];
static char TOPIC_BREW_SET_CMD[128];
static char TOPIC_STEAM_STATE[128];
static char TOPIC_STEAM_SET_CMD[128];
static char TOPIC_PIDP_STATE[128];
static char TOPIC_PIDP_CMD[128];
static char TOPIC_PIDI_STATE[128];
static char TOPIC_PIDI_CMD[128];
static char TOPIC_PIDD_STATE[128];
static char TOPIC_PIDD_CMD[128];
static char TOPIC_PIDG_STATE[128];
static char TOPIC_PIDG_CMD[128];
static char TOPIC_DTAU_STATE[128];
static char TOPIC_DTAU_CMD[128];

static char TOPIC_PUMP_POWER_STATE[128];
static char TOPIC_PUMP_POWER_CMD[128];
static char TOPIC_PRESSURE_SETPOINT_STATE[128];
static char TOPIC_PRESSURE_SETPOINT_CMD[128];
static char TOPIC_PUMP_MODE_STATE[128];
static char TOPIC_PUMP_MODE_CMD[128];
static char TOPIC_PRESSURE_SETPOINT_STATE[128];
static char TOPIC_PRESSURE_SETPOINT_CMD[128];
static char TOPIC_PUMP_PRESSURE_MODE_STATE[128];
static char TOPIC_PUMP_PRESSURE_MODE_CMD[128];

static inline void build_topics(void)
{
    snprintf(TOPIC_HEATER, sizeof TOPIC_HEATER, "%s/%s/heater/state", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_HEATER_SET, sizeof TOPIC_HEATER_SET, "%s/%s/heater/set", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_STEAM, sizeof TOPIC_STEAM, "%s/%s/steam/state", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_STEAM_SET, sizeof TOPIC_STEAM_SET, "%s/%s/steam/set", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_CURTEMP, sizeof TOPIC_CURTEMP, "%s/%s/current_temp/state", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_SETTEMP, sizeof TOPIC_SETTEMP, "%s/%s/set_temp/state", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_PRESSURE, sizeof TOPIC_PRESSURE, "%s/%s/pressure/state", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_SHOTVOL, sizeof TOPIC_SHOTVOL, "%s/%s/shot_volume/state", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_SHOT, sizeof TOPIC_SHOT, "%s/%s/shot/state", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_ZC_COUNT_STATE, sizeof TOPIC_ZC_COUNT_STATE, "%s/%s/zc_count/state", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_BREW_STATE, sizeof TOPIC_BREW_STATE, "%s/%s/brew_setpoint/state", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_BREW_SET_CMD, sizeof TOPIC_BREW_SET_CMD, "%s/%s/brew_setpoint/set", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_STEAM_STATE, sizeof TOPIC_STEAM_STATE, "%s/%s/steam_setpoint/state", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_STEAM_SET_CMD, sizeof TOPIC_STEAM_SET_CMD, "%s/%s/steam_setpoint/set", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_PIDP_STATE, sizeof TOPIC_PIDP_STATE, "%s/%s/pid_p/state", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_PIDP_CMD, sizeof TOPIC_PIDP_CMD, "%s/%s/pid_p/set", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_PIDI_STATE, sizeof TOPIC_PIDI_STATE, "%s/%s/pid_i/state", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_PIDI_CMD, sizeof TOPIC_PIDI_CMD, "%s/%s/pid_i/set", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_PIDD_STATE, sizeof TOPIC_PIDD_STATE, "%s/%s/pid_d/state", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_PIDD_CMD, sizeof TOPIC_PIDD_CMD, "%s/%s/pid_d/set", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_PIDG_STATE, sizeof TOPIC_PIDG_STATE, "%s/%s/pid_guard/state", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_PIDG_CMD, sizeof TOPIC_PIDG_CMD, "%s/%s/pid_guard/set", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_DTAU_STATE, sizeof TOPIC_DTAU_STATE, "%s/%s/pid_dtau/state", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_DTAU_CMD, sizeof TOPIC_DTAU_CMD, "%s/%s/pid_dtau/set", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_PUMP_POWER_STATE, sizeof TOPIC_PUMP_POWER_STATE, "%s/%s/pump_power/state", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_PUMP_POWER_CMD, sizeof TOPIC_PUMP_POWER_CMD, "%s/%s/pump_power/set", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_PUMP_MODE_STATE, sizeof TOPIC_PUMP_MODE_STATE, "%s/%s/pump_mode/state", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_PUMP_MODE_CMD, sizeof TOPIC_PUMP_MODE_CMD, "%s/%s/pump_mode/set", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_PRESSURE_SETPOINT_STATE, sizeof TOPIC_PRESSURE_SETPOINT_STATE, "%s/%s/pressure_setpoint/state", GAG_TOPIC_ROOT,
             GAGGIA_ID);
    snprintf(TOPIC_PRESSURE_SETPOINT_CMD, sizeof TOPIC_PRESSURE_SETPOINT_CMD, "%s/%s/pressure_setpoint/set", GAG_TOPIC_ROOT,
             GAGGIA_ID);
    snprintf(TOPIC_PUMP_PRESSURE_MODE_STATE, sizeof TOPIC_PUMP_PRESSURE_MODE_STATE,
             "%s/%s/pump_pressure_mode/state", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_PUMP_PRESSURE_MODE_CMD, sizeof TOPIC_PUMP_PRESSURE_MODE_CMD, "%s/%s/pump_pressure_mode/set", GAG_TOPIC_ROOT,
             GAGGIA_ID);
}

static inline bool parse_bool_str(const char *s)
{
    return (strcmp(s, "1") == 0) || (strcasecmp(s, "true") == 0) || (strcasecmp(s, "on") == 0) ||
           (strcasecmp(s, "yes") == 0) || (strcasecmp(s, "enable") == 0);
}

// -----------------------------------------------------------------------------
// State mirrors
// -----------------------------------------------------------------------------
typedef struct
{
    bool heater;
    bool steam;
    float brewSetpoint;
    float steamSetpoint;
    float pidP;
    float pidI;
    float pidD;
    float pidGuard;
    float dTau;
    float pumpPower;
    float pressureSetpoint;
    uint8_t pumpMode;
    bool pumpPressureMode;
} ControlState;

static const ControlState CONTROL_DEFAULTS = {
    .heater = true,
    .steam = false,
    .brewSetpoint = 92.0f,
    .steamSetpoint = 152.0f,
    .pidP = 8.0f,
    .pidI = 0.6,
    .pidD = 10.0f,
    .pidGuard = 25.0f,
    .dTau = 0.8f,
    .pumpPower = 95.0f,
    .pressureSetpoint = 9.0f,
    .pumpMode = ESPNOW_PUMP_MODE_NORMAL,
    .pumpPressureMode = false,
};

static ControlState s_control;

static float s_current_temp = NAN;
static float s_set_temp = NAN;
static float s_pressure = NAN;
static float s_shot_time = 0.0f;
static float s_shot_volume = 0.0f;
static float s_brew_setpoint = NAN;
static uint32_t s_zc_count = 0;
static float s_steam_setpoint = NAN;
static float s_pid_p = NAN;
static float s_pid_i = NAN;
static float s_pid_d = NAN;
static float s_pid_guard = NAN;
static float s_dtau = NAN;
static float s_pump_power = NAN;
static uint8_t s_pump_mode = ESPNOW_PUMP_MODE_NORMAL;
static float s_pressure_setpoint = NAN;
static bool s_pump_pressure_mode = false;
static bool s_heater = false;
static bool s_steam = false;

typedef enum
{
    CONTROL_BOOT_HEATER = 1u << 0,
    CONTROL_BOOT_STEAM = 1u << 1,
    CONTROL_BOOT_BREW = 1u << 2,
    CONTROL_BOOT_STEAM_SET = 1u << 3,
    CONTROL_BOOT_PID_P = 1u << 4,
    CONTROL_BOOT_PID_I = 1u << 5,
    CONTROL_BOOT_PID_D = 1u << 6,
    CONTROL_BOOT_PID_GUARD = 1u << 7,
    CONTROL_BOOT_DTAU = 1u << 8,
    CONTROL_BOOT_PUMP_POWER = 1u << 9,
    CONTROL_BOOT_PUMP_MODE = 1u << 10,
    CONTROL_BOOT_PRESSURE_SETPOINT = 1u << 11,
    CONTROL_BOOT_PUMP_PRESSURE_MODE = 1u << 12,
    CONTROL_BOOT_ALL = (1u << 13) - 1,
} ControlBootstrapBit;

static bool s_control_bootstrap_active = false;
static uint32_t s_control_bootstrap_mask = 0;

static void control_apply_defaults(void);
static void control_bootstrap_reset(void);
static void control_bootstrap_complete(void);
static bool control_bootstrap_ignore(ControlBootstrapBit bit, bool retained, bool matches);
static bool control_bootstrap_ignore_float(ControlBootstrapBit bit, bool retained, float value, float current, float tolerance);
static bool control_bootstrap_ignore_u8(ControlBootstrapBit bit, bool retained, uint8_t value, uint8_t current);
static bool control_bootstrap_ignore_bool(ControlBootstrapBit bit, bool retained, bool value, bool current);
static uint8_t apply_steam_request(bool steam);

static bool s_mqtt_connected = false;
static esp_mqtt_client_handle_t s_mqtt = NULL;
static bool s_mqtt_enabled = true;
static bool s_standby_suppressed = false;

static bool s_wifi_ready = false;
static uint8_t s_sta_channel = 0;
static bool s_sta_channel_valid = false;

static bool s_espnow_active = false;
static bool s_espnow_handshake = false;
static bool s_use_espnow = false;
static time_t s_espnow_last_rx = 0;
static uint32_t s_control_revision = 0;
static bool s_control_dirty = false;
static uint8_t s_last_espnow_channel = 0;

static esp_now_peer_info_t s_broadcast_peer = {0};
static esp_now_peer_info_t s_controller_peer = {0};
static bool s_controller_peer_valid = false;

static TimerHandle_t s_espnow_timer = NULL;
static TimerHandle_t s_espnow_ping_timer = NULL;
static volatile bool s_espnow_timeout_req = false;
static volatile bool s_espnow_ping_req = false;

static const uint8_t s_broadcast_addr[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#define CONTROL_TEMP_TOLERANCE 0.05f
#define CONTROL_PID_TOLERANCE 0.005f
#define CONTROL_PUMP_POWER_TOLERANCE 0.05f
#define CONTROL_PRESSURE_TOLERANCE 0.05f
#define CONTROL_PRESSURE_MIN 0.0f
#define CONTROL_PRESSURE_MAX 12.0f
#define STEAM_STATE_CHANGED_FLAG 0x01u
#define HEATER_STATE_CHANGED_FLAG 0x02u

static inline bool float_equals(float a, float b, float tolerance)
{
    return fabsf(a - b) <= tolerance;
}

static void control_apply_defaults(void)
{
    s_control = CONTROL_DEFAULTS;
    s_heater = s_control.heater;
    s_steam = s_control.steam;
    s_brew_setpoint = s_control.brewSetpoint;
    s_steam_setpoint = s_control.steamSetpoint;
    s_pid_p = s_control.pidP;
    s_pid_i = s_control.pidI;
    s_pid_d = s_control.pidD;
    s_pid_guard = s_control.pidGuard;
    s_dtau = s_control.dTau,
    s_pump_power = s_control.pumpPower;
    s_pump_mode = s_control.pumpMode;
    s_pressure_setpoint = s_control.pressureSetpoint;
    s_pump_pressure_mode = s_control.pumpPressureMode;
    s_set_temp = s_control.brewSetpoint;
    s_control_bootstrap_active = false;
    s_control_bootstrap_mask = 0;
}

static void control_bootstrap_reset(void)
{
    s_control_bootstrap_active = true;
    s_control_bootstrap_mask = CONTROL_BOOT_ALL;
    ESP_LOGI(TAG_MQTT, "Control bootstrap reset");
}

static void control_bootstrap_complete(void)
{
    if (!s_control_bootstrap_active)
        return;
    s_control_bootstrap_active = false;
    s_control_bootstrap_mask = 0;
    ESP_LOGI(TAG_MQTT, "Control bootstrap complete");
}

static bool control_bootstrap_ignore(ControlBootstrapBit bit, bool retained, bool matches)
{
    if (!s_control_bootstrap_active)
        return false;
    if (!retained)
    {
        control_bootstrap_complete();
        return false;
    }
    if (matches)
    {
        if (s_control_bootstrap_mask & bit)
        {
            s_control_bootstrap_mask &= ~bit;
            if (s_control_bootstrap_mask == 0)
            {
                control_bootstrap_complete();
            }
        }
        return false;
    }
    return true;
}

static bool control_bootstrap_ignore_float(ControlBootstrapBit bit, bool retained, float value, float current, float tolerance)
{
    return control_bootstrap_ignore(bit, retained, float_equals(value, current, tolerance));
}

static bool control_bootstrap_ignore_u8(ControlBootstrapBit bit, bool retained, uint8_t value, uint8_t current)
{
    return control_bootstrap_ignore(bit, retained, value == current);
}

static bool control_bootstrap_ignore_bool(ControlBootstrapBit bit, bool retained, bool value, bool current)
{
    return control_bootstrap_ignore(bit, retained, value == current);
}

static uint8_t apply_steam_request(bool steam)
{
    uint8_t changed = 0;
    if (steam && !s_control.heater)
    {
        s_control.heater = true;
        s_heater = true;
        changed |= HEATER_STATE_CHANGED_FLAG;
    }
    if (s_control.steam != steam)
    {
        s_control.steam = steam;
        s_steam = steam;
        changed |= STEAM_STATE_CHANGED_FLAG;
    }
    return changed;
}

// -----------------------------------------------------------------------------
// Forward declarations
// -----------------------------------------------------------------------------
static void publish_control_state(void);
static void schedule_control_send(void);
static void send_control_packet(void);
static void ensure_espnow_started(void);
static void stop_espnow(void);
static void espnow_timeout_cb(TimerHandle_t xTimer);
static void espnow_ping_cb(TimerHandle_t xTimer);
static void Wireless_Task(void *arg);
static void espnow_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int data_len);

// -----------------------------------------------------------------------------
// Wi-Fi initialisation and event handling
// -----------------------------------------------------------------------------
static void on_ip_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG_WIFI, "Got IP: %d.%d.%d.%d", IP2STR(&event->ip_info.ip));
        s_wifi_ready = true;
    }
}

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base != WIFI_EVENT)
        return;
    switch (id)
    {
    case WIFI_EVENT_STA_CONNECTED:
    {
        wifi_event_sta_connected_t *event = (wifi_event_sta_connected_t *)data;
        if (!s_sta_channel_valid || s_sta_channel != event->channel)
        {
            if (s_espnow_active)
            {
                stop_espnow();
            }
            s_sta_channel = event->channel;
            s_sta_channel_valid = true;
            ESP_LOGI(TAG_WIFI, "STA connected (channel %u)", (unsigned)s_sta_channel);
        }
        else
        {
            s_sta_channel = event->channel;
        }
        s_last_espnow_channel = s_sta_channel;
        ensure_espnow_started();
        break;
    }
    case WIFI_EVENT_STA_DISCONNECTED:
        ESP_LOGW(TAG_WIFI, "STA disconnected");
        s_wifi_ready = false;
        s_sta_channel_valid = false;
        s_last_espnow_channel = 0;
        stop_espnow();
        esp_wifi_connect();
        break;
    default:
        break;
    }
}

void WIFI_Init(void *arg)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t sta_cfg = {0};
    strncpy((char *)sta_cfg.sta.ssid, WIFI_SSID, sizeof(sta_cfg.sta.ssid));
    strncpy((char *)sta_cfg.sta.password, WIFI_PASSWORD, sizeof(sta_cfg.sta.password));
    sta_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_ip_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi_event, NULL));

    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());

    // Allow SNTP sync once Wi-Fi is up
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    vTaskDelete(NULL);
}

void Wireless_Init(void)
{
    control_apply_defaults();
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    xTaskCreatePinnedToCore(WIFI_Init, "wifi", 4096, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(Wireless_Task, "wireless", 4096, NULL, 3, NULL, 0);
}

// -----------------------------------------------------------------------------
// MQTT handling
// -----------------------------------------------------------------------------
static void mqtt_subscribe_all(void)
{
    if (!s_mqtt)
        return;
    esp_mqtt_client_subscribe(s_mqtt, TOPIC_HEATER_SET, 1);
    esp_mqtt_client_subscribe(s_mqtt, TOPIC_STEAM_SET, 1);
    esp_mqtt_client_subscribe(s_mqtt, TOPIC_BREW_SET_CMD, 1);
    esp_mqtt_client_subscribe(s_mqtt, TOPIC_STEAM_SET_CMD, 1);
    esp_mqtt_client_subscribe(s_mqtt, TOPIC_PIDP_CMD, 1);
    esp_mqtt_client_subscribe(s_mqtt, TOPIC_PIDI_CMD, 1);
    esp_mqtt_client_subscribe(s_mqtt, TOPIC_PIDD_CMD, 1);
    esp_mqtt_client_subscribe(s_mqtt, TOPIC_PIDG_CMD, 1);
    esp_mqtt_client_subscribe(s_mqtt, TOPIC_DTAU_CMD, 1);
    esp_mqtt_client_subscribe(s_mqtt, TOPIC_PUMP_POWER_CMD, 1);
    esp_mqtt_client_subscribe(s_mqtt, TOPIC_PRESSURE_SETPOINT_CMD, 1);
    esp_mqtt_client_subscribe(s_mqtt, TOPIC_PUMP_MODE_CMD, 1);
    esp_mqtt_client_subscribe(s_mqtt, TOPIC_PRESSURE_SETPOINT_CMD, 1);
    esp_mqtt_client_subscribe(s_mqtt, TOPIC_PUMP_PRESSURE_MODE_CMD, 1);
    // State mirrors for retained bootstrap
    esp_mqtt_client_subscribe(s_mqtt, TOPIC_HEATER, 1);
    esp_mqtt_client_subscribe(s_mqtt, TOPIC_STEAM, 1);
    esp_mqtt_client_subscribe(s_mqtt, TOPIC_BREW_STATE, 1);
    esp_mqtt_client_subscribe(s_mqtt, TOPIC_STEAM_STATE, 1);
    esp_mqtt_client_subscribe(s_mqtt, TOPIC_ZC_COUNT_STATE, 1);
    esp_mqtt_client_subscribe(s_mqtt, TOPIC_PIDP_STATE, 1);
    esp_mqtt_client_subscribe(s_mqtt, TOPIC_PIDI_STATE, 1);
    esp_mqtt_client_subscribe(s_mqtt, TOPIC_PIDD_STATE, 1);
    esp_mqtt_client_subscribe(s_mqtt, TOPIC_PIDG_STATE, 1);
    esp_mqtt_client_subscribe(s_mqtt, TOPIC_DTAU_STATE, 1);
    esp_mqtt_client_subscribe(s_mqtt, TOPIC_PUMP_POWER_STATE, 1);
    esp_mqtt_client_subscribe(s_mqtt, TOPIC_PRESSURE_SETPOINT_STATE, 1);
    esp_mqtt_client_subscribe(s_mqtt, TOPIC_PUMP_MODE_STATE, 1);
    esp_mqtt_client_subscribe(s_mqtt, TOPIC_PRESSURE_SETPOINT_STATE, 1);
    esp_mqtt_client_subscribe(s_mqtt, TOPIC_PUMP_PRESSURE_MODE_STATE, 1);
}

static void publish_float(const char *topic, float value, uint8_t decimals)
{
    if (!s_mqtt)
        return;
    char buf[32];
    snprintf(buf, sizeof(buf), "%.*f", decimals, value);
    esp_mqtt_client_publish(s_mqtt, topic, buf, 0, 1, true);
}

static void publish_bool_topic(const char *topic, bool value)
{
    if (!s_mqtt)
        return;
    esp_mqtt_client_publish(s_mqtt, topic, value ? "ON" : "OFF", 0, 1, true);
}

#if defined(MQTT_STATUS) && defined(GAGGIA_ID)
static bool s_pid_p_discovery_published = false;
static bool s_pid_i_discovery_published = false;
static bool s_pid_d_discovery_published = false;
static bool s_pid_g_discovery_published = false;
static bool s_dtau_discovery_published = false;
static bool s_pressure_setpoint_discovery_published = false;
static bool s_pump_power_discovery_published = false;
static bool s_pump_pressure_mode_discovery_published = false;

static bool publish_pid_number_discovery(const char *name, const char *suffix, const char *cmd_topic,
                                         const char *state_topic, float min, float max, float step,
                                         bool *published_flag)
{
    if (!s_mqtt || *published_flag)
        return false;

    char dev_id[64];
    snprintf(dev_id, sizeof dev_id, "%s-%s", GAG_TOPIC_ROOT, GAGGIA_ID);

    char topic[128];
    snprintf(topic, sizeof topic, "homeassistant/number/%s_%s/config", dev_id, suffix);

    const char *availability = MQTT_STATUS;
    const char *version = VERSION;

    char payload[512];
    int written = snprintf(payload, sizeof payload,
                           "{\"name\":\"%s\",\"uniq_id\":\"%s_%s\","
                           "\"cmd_t\":\"%s\",\"stat_t\":\"%s\",\"min\":%.3g,\"max\":%.3g,"
                           "\"step\":%.3g,\"mode\":\"auto\",\"avty_t\":\"%s\","
                           "\"pl_avail\":\"online\",\"pl_not_avail\":\"offline\","
                           "\"dev\":{\"identifiers\":[\"%s\"],\"name\":\"Gaggia Classic\","
                           "\"manufacturer\":\"Custom\",\"model\":\"Gagguino\",\"sw_version\":\"%s\"}}",
                           name, dev_id, suffix, cmd_topic, state_topic, min, max, step, availability, dev_id, version);

    if (written > 0 && written < (int)sizeof(payload))
    {
        int res = esp_mqtt_client_publish(s_mqtt, topic, payload, 0, 1, true);
        if (res >= 0)
        {
            *published_flag = true;
            ESP_LOGI(TAG_MQTT, "Published %s discovery with min=%.3g max=%.3g", name, min, max);
            return true;
        }
        ESP_LOGW(TAG_MQTT, "Failed to publish %s discovery: %d", name, res);
    }
    else
    {
        ESP_LOGW(TAG_MQTT, "%s discovery payload truncated", name);
    }

    return false;
}

static bool publish_switch_discovery(const char *name, const char *suffix, const char *cmd_topic,
                                     const char *state_topic, bool *published_flag)
{
    if (!s_mqtt || *published_flag)
        return false;

    char dev_id[64];
    snprintf(dev_id, sizeof dev_id, "%s-%s", GAG_TOPIC_ROOT, GAGGIA_ID);

    char topic[128];
    snprintf(topic, sizeof topic, "homeassistant/switch/%s_%s/config", dev_id, suffix);

    const char *availability = MQTT_STATUS;
    const char *version = VERSION;

    char payload[512];
    int written = snprintf(payload, sizeof payload,
                           "{\"name\":\"%s\",\"uniq_id\":\"%s_%s\",\"cmd_t\":\"%s\",\"stat_t\":\"%s\","
                           "\"pl_on\":\"ON\",\"pl_off\":\"OFF\",\"avty_t\":\"%s\",\"pl_avail\":\"online\","
                           "\"pl_not_avail\":\"offline\",\"dev\":{\"identifiers\":[\"%s\"],\"name\":\"Gaggia Classic\","
                           "\"manufacturer\":\"Custom\",\"model\":\"Gagguino\",\"sw_version\":\"%s\"}}",
                           name, dev_id, suffix, cmd_topic, state_topic, availability, dev_id, version);

    if (written > 0 && written < (int)sizeof(payload))
    {
        int res = esp_mqtt_client_publish(s_mqtt, topic, payload, 0, 1, true);
        if (res >= 0)
        {
            *published_flag = true;
            ESP_LOGI(TAG_MQTT, "Published %s discovery", name);
            return true;
        }
        ESP_LOGW(TAG_MQTT, "Failed to publish %s discovery: %d", name, res);
    }
    else
    {
        ESP_LOGW(TAG_MQTT, "%s discovery payload truncated", name);
    }

    return false;
}

static void publish_pid_discovery(void)
{
    publish_pid_number_discovery("PID P", "pid_p", TOPIC_PIDP_CMD, TOPIC_PIDP_STATE, 0.0f, 100.0f, 0.1f,
                                 &s_pid_p_discovery_published);
    publish_pid_number_discovery("PID I", "pid_i", TOPIC_PIDI_CMD, TOPIC_PIDI_STATE, 0.0f, 2.0f, 0.01f,
                                 &s_pid_i_discovery_published);
    publish_pid_number_discovery("PID D", "pid_d", TOPIC_PIDD_CMD, TOPIC_PIDD_STATE, 0.0f, 500.0f, 0.5f,
                                 &s_pid_d_discovery_published);
    publish_pid_number_discovery("PID Guard", "pid_guard", TOPIC_PIDG_CMD, TOPIC_PIDG_STATE, 0.0f, 100.0f, 0.5f,
                                 &s_pid_g_discovery_published);
    publish_pid_number_discovery("PID dTau", "pid_dtau", TOPIC_DTAU_CMD, TOPIC_DTAU_STATE, 0.0f, 2.0f, 0.05f,
                                 &s_dtau_discovery_published);
    publish_pid_number_discovery("Pressure Setpoint", "pressure_setpoint", TOPIC_PRESSURE_SETPOINT_CMD,
                                 TOPIC_PRESSURE_SETPOINT_STATE, CONTROL_PRESSURE_MIN, CONTROL_PRESSURE_MAX, 0.5f,
                                 &s_pressure_setpoint_discovery_published);
    publish_pid_number_discovery("Pump Power", "pump_power", TOPIC_PUMP_POWER_CMD, TOPIC_PUMP_POWER_STATE, 40.0f, 95.0f,
                                 5.0f, &s_pump_power_discovery_published);
    publish_switch_discovery("Pump Pressure Mode", "pump_pressure_mode", TOPIC_PUMP_PRESSURE_MODE_CMD,
                             TOPIC_PUMP_PRESSURE_MODE_STATE, &s_pump_pressure_mode_discovery_published);
}

static inline void reset_pid_discovery_flags(void)
{
    s_pid_p_discovery_published = false;
    s_pid_i_discovery_published = false;
    s_pid_d_discovery_published = false;
    s_pid_g_discovery_published = false;
    s_dtau_discovery_published = false;
    s_pressure_setpoint_discovery_published = false;
    s_pump_power_discovery_published = false;
    s_pump_pressure_mode_discovery_published = false;
}
#else
static inline void publish_pid_discovery(void) {}
static inline void reset_pid_discovery_flags(void) {}
#endif

static void publish_control_state(void)
{
    if (!s_mqtt_connected)
        return;
    publish_bool_topic(TOPIC_HEATER, s_control.heater);
    publish_bool_topic(TOPIC_STEAM, s_control.steam);
    publish_float(TOPIC_BREW_STATE, s_control.brewSetpoint, 1);
    publish_float(TOPIC_STEAM_STATE, s_control.steamSetpoint, 1);
    publish_float(TOPIC_PIDP_STATE, s_control.pidP, 2);
    publish_float(TOPIC_PIDI_STATE, s_control.pidI, 2);
    publish_float(TOPIC_PIDD_STATE, s_control.pidD, 2);
    publish_float(TOPIC_PIDG_STATE, s_control.pidGuard, 2);
    publish_float(TOPIC_DTAU_STATE, s_control.dTau, 2);
    publish_float(TOPIC_PUMP_POWER_STATE, s_control.pumpPower, 1);
    publish_float(TOPIC_PRESSURE_SETPOINT_STATE, s_control.pressureSetpoint, 1);
    char buf[16];
    snprintf(buf, sizeof buf, "%u", (unsigned)s_control.pumpMode);
    esp_mqtt_client_publish(s_mqtt, TOPIC_PUMP_MODE_STATE, buf, 0, 1, true);
    publish_float(TOPIC_PRESSURE_SETPOINT_STATE, s_control.pressureSetpoint, 1);
    publish_bool_topic(TOPIC_PUMP_PRESSURE_MODE_STATE, s_control.pumpPressureMode);
}

static void handle_control_change(void)
{
    publish_control_state();
    schedule_control_send();
}

static void log_control_bool(const char *name, bool value)
{
    ESP_LOGI(TAG_MQTT, "MQTT control %s -> %s", name, value ? "ON" : "OFF");
}

static void log_control_float(const char *name, float value, uint8_t precision)
{
    ESP_LOGI(TAG_MQTT, "MQTT control %s -> %.*f", name, precision, (double)value);
}

static void log_control_u8(const char *name, uint8_t value)
{
    ESP_LOGI(TAG_MQTT, "MQTT control %s -> %u", name, (unsigned)value);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    switch (event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG_MQTT, "Connected");
        s_mqtt_connected = true;
        control_bootstrap_reset();
        mqtt_subscribe_all();
#ifdef MQTT_STATUS
        esp_mqtt_client_publish(event->client, MQTT_STATUS, "online", 0, 1, true);
#endif
        publish_pid_discovery();
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG_MQTT, "Disconnected");
        s_mqtt_connected = false;
        reset_pid_discovery_flags();
        break;
    case MQTT_EVENT_DATA:
    {
        char topic[128];
        char payload[128];
        size_t tlen = (event->topic_len < sizeof(topic) - 1) ? event->topic_len : sizeof(topic) - 1;
        size_t plen = (event->data_len < sizeof(payload) - 1) ? event->data_len : sizeof(payload) - 1;
        memcpy(topic, event->topic, tlen);
        topic[tlen] = '\0';
        memcpy(payload, event->data, plen);
        payload[plen] = '\0';

        if (strcmp(topic, TOPIC_CURTEMP) == 0)
        {
            s_current_temp = strtof(payload, NULL);
        }
        else if (strcmp(topic, TOPIC_SETTEMP) == 0)
        {
            s_set_temp = strtof(payload, NULL);
        }
        else if (strcmp(topic, TOPIC_PRESSURE) == 0)
        {
            s_pressure = strtof(payload, NULL);
        }
        else if (strcmp(topic, TOPIC_SHOTVOL) == 0)
        {
            s_shot_volume = strtof(payload, NULL);
        }
        else if (strcmp(topic, TOPIC_SHOT) == 0)
        {
            s_shot_time = strtof(payload, NULL);
        }
        else if (strcmp(topic, TOPIC_ZC_COUNT_STATE) == 0)
        {
            s_zc_count = (uint32_t)strtoul(payload, NULL, 10);
        }
        else if (strcmp(topic, TOPIC_HEATER) == 0)
        {
            bool hv = parse_bool_str(payload);
            if (control_bootstrap_ignore_bool(CONTROL_BOOT_HEATER, event->retain, hv, s_control.heater))
            {
                ESP_LOGI(TAG_MQTT, "Bootstrap skip: heater -> %s", payload);
                break;
            }
            s_control.heater = hv;
            s_heater = s_control.heater;
        }
        else if (strcmp(topic, TOPIC_STEAM) == 0)
        {
            bool sv = parse_bool_str(payload);
            if (control_bootstrap_ignore_bool(CONTROL_BOOT_STEAM, event->retain, sv, s_control.steam))
            {
                ESP_LOGI(TAG_MQTT, "Bootstrap skip: steam -> %s", payload);
                break;
            }
            s_control.steam = sv;
            s_steam = s_control.steam;
        }
        else if (strcmp(topic, TOPIC_BREW_STATE) == 0)
        {
            float v = strtof(payload, NULL);
            if (control_bootstrap_ignore_float(CONTROL_BOOT_BREW, event->retain, v, s_control.brewSetpoint, CONTROL_TEMP_TOLERANCE))
            {
                ESP_LOGI(TAG_MQTT, "Bootstrap skip: brew_setpoint -> %s", payload);
                break;
            }
            s_control.brewSetpoint = v;
            s_brew_setpoint = s_control.brewSetpoint;
        }
        else if (strcmp(topic, TOPIC_STEAM_STATE) == 0)
        {
            float v = strtof(payload, NULL);
            if (control_bootstrap_ignore_float(CONTROL_BOOT_STEAM_SET, event->retain, v, s_control.steamSetpoint, CONTROL_TEMP_TOLERANCE))
            {
                ESP_LOGI(TAG_MQTT, "Bootstrap skip: steam_setpoint -> %s", payload);
                break;
            }
            s_control.steamSetpoint = v;
            s_steam_setpoint = s_control.steamSetpoint;
        }
        else if (strcmp(topic, TOPIC_PIDP_STATE) == 0)
        {
            float v = strtof(payload, NULL);
            if (v < 0.0f)
                v = 0.0f;
            if (control_bootstrap_ignore_float(CONTROL_BOOT_PID_P, event->retain, v, s_control.pidP, CONTROL_PID_TOLERANCE))
            {
                ESP_LOGI(TAG_MQTT, "Bootstrap skip: pid_p -> %s", payload);
                break;
            }
            s_control.pidP = v;
            s_pid_p = s_control.pidP;
            if (event->retain)
                schedule_control_send();
        }
        else if (strcmp(topic, TOPIC_PIDI_STATE) == 0)
        {
            float v = strtof(payload, NULL);
            if (v < 0.0f)
                v = 0.0f;
            if (control_bootstrap_ignore_float(CONTROL_BOOT_PID_I, event->retain, v, s_control.pidI, CONTROL_PID_TOLERANCE))
            {
                ESP_LOGI(TAG_MQTT, "Bootstrap skip: pid_i -> %s", payload);
                break;
            }
            s_control.pidI = v;
            s_pid_i = s_control.pidI;
            if (event->retain)
                schedule_control_send();
        }
        else if (strcmp(topic, TOPIC_PIDD_STATE) == 0)
        {
            float v = strtof(payload, NULL);
            if (v < 0.0f)
                v = 0.0f;
            if (control_bootstrap_ignore_float(CONTROL_BOOT_PID_D, event->retain, v, s_control.pidD, CONTROL_PID_TOLERANCE))
            {
                ESP_LOGI(TAG_MQTT, "Bootstrap skip: pid_d -> %s", payload);
                break;
            }
            s_control.pidD = v;
            s_pid_d = s_control.pidD;
            if (event->retain)
                schedule_control_send();
        }
        else if (strcmp(topic, TOPIC_PIDG_STATE) == 0)
        {
            float v = strtof(payload, NULL);
            if (v < 0.0f)
                v = 0.0f;
            if (control_bootstrap_ignore_float(CONTROL_BOOT_PID_GUARD, event->retain, v, s_control.pidGuard,
                                               CONTROL_PID_TOLERANCE))
            {
                ESP_LOGI(TAG_MQTT, "Bootstrap skip: pid_guard -> %s", payload);
                break;
            }
            s_control.pidGuard = v;
            s_pid_guard = s_control.pidGuard;
            if (event->retain)
                schedule_control_send();
        }
        else if (strcmp(topic, TOPIC_DTAU_STATE) == 0)
        {
            float v = strtof(payload, NULL);
            if (v < 0.0f)
                v = 0.0f;
            if (control_bootstrap_ignore_float(CONTROL_BOOT_DTAU, event->retain, v, s_control.dTau,
                                               CONTROL_PID_TOLERANCE))
            {
                ESP_LOGI(TAG_MQTT, "Bootstrap skip: pid_dtau -> %s", payload);
                break;
            }
            s_control.dTau = v;
            s_dtau = s_control.dTau;
            if (event->retain)
                schedule_control_send();
        }
        else if (strcmp(topic, TOPIC_PUMP_POWER_STATE) == 0)
        {
            float v = strtof(payload, NULL);
            if (control_bootstrap_ignore_float(CONTROL_BOOT_PUMP_POWER, event->retain, v, s_control.pumpPower, CONTROL_PUMP_POWER_TOLERANCE))
            {
                ESP_LOGI(TAG_MQTT, "Bootstrap skip: pump_power -> %s", payload);
                break;
            }
            s_control.pumpPower = v;
            s_pump_power = s_control.pumpPower;
        }
        else if (strcmp(topic, TOPIC_PRESSURE_SETPOINT_STATE) == 0)
        {
            float v = strtof(payload, NULL);
            if (control_bootstrap_ignore_float(CONTROL_BOOT_PRESSURE_SETPOINT, event->retain, v, s_control.pressureSetpoint, CONTROL_PRESSURE_TOLERANCE))
            {
                ESP_LOGI(TAG_MQTT, "Bootstrap skip: pressure_setpoint -> %s", payload);
                break;
            }
            s_control.pressureSetpoint = v;
            s_pressure_setpoint = s_control.pressureSetpoint;
        }
        else if (strcmp(topic, TOPIC_PUMP_MODE_STATE) == 0)
        {
            uint8_t v = (uint8_t)atoi(payload);
            if (control_bootstrap_ignore_u8(CONTROL_BOOT_PUMP_MODE, event->retain, v, s_control.pumpMode))
            {
                ESP_LOGI(TAG_MQTT, "Bootstrap skip: pump_mode -> %s", payload);
                break;
            }
            s_control.pumpMode = v;
            s_pump_mode = s_control.pumpMode;
        }
        else if (strcmp(topic, TOPIC_PRESSURE_SETPOINT_STATE) == 0)
        {
            float v = strtof(payload, NULL);
            if (v < CONTROL_PRESSURE_MIN)
                v = CONTROL_PRESSURE_MIN;
            if (v > CONTROL_PRESSURE_MAX)
                v = CONTROL_PRESSURE_MAX;
            if (control_bootstrap_ignore_float(CONTROL_BOOT_PRESSURE_SETPOINT, event->retain, v, s_control.pressureSetpoint,
                                               CONTROL_PRESSURE_TOLERANCE))
            {
                ESP_LOGI(TAG_MQTT, "Bootstrap skip: pressure_setpoint -> %s", payload);
                break;
            }
            s_control.pressureSetpoint = v;
            s_pressure_setpoint = s_control.pressureSetpoint;
        }
        else if (strcmp(topic, TOPIC_PUMP_PRESSURE_MODE_STATE) == 0)
        {
            bool v = parse_bool_str(payload);
            if (control_bootstrap_ignore_bool(CONTROL_BOOT_PUMP_PRESSURE_MODE, event->retain, v,
                                              s_control.pumpPressureMode))
            {
                ESP_LOGI(TAG_MQTT, "Bootstrap skip: pump_pressure_mode -> %s", payload);
                break;
            }
            s_control.pumpPressureMode = v;
            s_pump_pressure_mode = s_control.pumpPressureMode;
        }
        else if (strcmp(topic, TOPIC_HEATER_SET) == 0)
        {
            bool hv = parse_bool_str(payload);
            control_bootstrap_complete();
            if (hv != s_control.heater)
            {
                s_control.heater = hv;
                s_heater = hv;
                log_control_bool("heater", hv);
                handle_control_change();
            }
        }
        else if (strcmp(topic, TOPIC_STEAM_SET) == 0)
        {
            bool sv = parse_bool_str(payload);
            control_bootstrap_complete();
            uint8_t changed = apply_steam_request(sv);
            if (changed)
            {
                if (changed & HEATER_STATE_CHANGED_FLAG)
                    log_control_bool("heater", true);
                if (changed & STEAM_STATE_CHANGED_FLAG)
                    log_control_bool("steam", sv);
                handle_control_change();
            }
        }
        else if (strcmp(topic, TOPIC_BREW_SET_CMD) == 0)
        {
            float v = strtof(payload, NULL);
            control_bootstrap_complete();
            if (!float_equals(v, s_control.brewSetpoint, CONTROL_TEMP_TOLERANCE))
            {
                s_control.brewSetpoint = v;
                log_control_float("brew_setpoint", v, 1);
                handle_control_change();
            }
        }
        else if (strcmp(topic, TOPIC_STEAM_SET_CMD) == 0)
        {
            float v = strtof(payload, NULL);
            control_bootstrap_complete();
            if (!float_equals(v, s_control.steamSetpoint, CONTROL_TEMP_TOLERANCE))
            {
                s_control.steamSetpoint = v;
                log_control_float("steam_setpoint", v, 1);
                handle_control_change();
            }
        }
        else if (strcmp(topic, TOPIC_PIDP_CMD) == 0)
        {
            float v = strtof(payload, NULL);
            if (v < 0.0f)
                v = 0.0f;
            control_bootstrap_complete();
            if (!float_equals(v, s_control.pidP, CONTROL_PID_TOLERANCE))
            {
                s_control.pidP = v;
                log_control_float("pid_p", v, 2);
                handle_control_change();
            }
        }
        else if (strcmp(topic, TOPIC_PIDI_CMD) == 0)
        {
            float v = strtof(payload, NULL);
            if (v < 0.0f)
                v = 0.0f;
            control_bootstrap_complete();
            if (!float_equals(v, s_control.pidI, CONTROL_PID_TOLERANCE))
            {
                s_control.pidI = v;
                log_control_float("pid_i", v, 2);
                handle_control_change();
            }
        }
        else if (strcmp(topic, TOPIC_PIDD_CMD) == 0)
        {
            float v = strtof(payload, NULL);
            if (v < 0.0f)
                v = 0.0f;
            control_bootstrap_complete();
            if (!float_equals(v, s_control.pidD, CONTROL_PID_TOLERANCE))
            {
                s_control.pidD = v;
                log_control_float("pid_d", v, 2);
                handle_control_change();
            }
        }
        else if (strcmp(topic, TOPIC_PIDG_CMD) == 0)
        {
            float v = strtof(payload, NULL);
            if (v < 0.0f)
                v = 0.0f;
            control_bootstrap_complete();
            if (!float_equals(v, s_control.pidGuard, CONTROL_PID_TOLERANCE))
            {
                s_control.pidGuard = v;
                log_control_float("pid_guard", v, 2);
                handle_control_change();
            }
        }
        else if (strcmp(topic, TOPIC_DTAU_CMD) == 0)
        {
            float v = strtof(payload, NULL);
            if (v < 0.0f)
                v = 0.0f;
            else if (v > 2.0f)
                v = 2.0f;
            control_bootstrap_complete();
            if (!float_equals(v, s_control.dTau, CONTROL_PID_TOLERANCE))
            {
                s_control.dTau = v;
                s_dtau = v;
                log_control_float("pid_dtau", v, 2);
                handle_control_change();
            }
        }
        else if (strcmp(topic, TOPIC_PUMP_POWER_CMD) == 0)
        {
            float v = strtof(payload, NULL);
            if (v < 40.0f)
                v = 40.0f;
            else if (v > 95.0f)
                v = 95.0f;
            control_bootstrap_complete();
            if (!float_equals(v, s_control.pumpPower, CONTROL_PUMP_POWER_TOLERANCE))
            {
                s_control.pumpPower = v;
                s_pump_power = v;
                log_control_float("pump_power", v, 1);
                handle_control_change();
            }
        }
        else if (strcmp(topic, TOPIC_PRESSURE_SETPOINT_CMD) == 0)
        {
            float v = strtof(payload, NULL);
            control_bootstrap_complete();
            if (!float_equals(v, s_control.pressureSetpoint, CONTROL_PRESSURE_TOLERANCE))
            {
                s_control.pressureSetpoint = v;
                log_control_float("pressure_setpoint", v, 1);
                handle_control_change();
            }
        }
        else if (strcmp(topic, TOPIC_PUMP_MODE_CMD) == 0)
        {
            uint8_t v = (uint8_t)atoi(payload);
            control_bootstrap_complete();
            if (v != s_control.pumpMode)
            {
                s_control.pumpMode = v;
                log_control_u8("pump_mode", v);
                handle_control_change();
            }
        }
        else if (strcmp(topic, TOPIC_PRESSURE_SETPOINT_CMD) == 0)
        {
            float v = strtof(payload, NULL);
            if (v < CONTROL_PRESSURE_MIN)
                v = CONTROL_PRESSURE_MIN;
            else if (v > CONTROL_PRESSURE_MAX)
                v = CONTROL_PRESSURE_MAX;
            control_bootstrap_complete();
            if (!float_equals(v, s_control.pressureSetpoint, CONTROL_PRESSURE_TOLERANCE))
            {
                s_control.pressureSetpoint = v;
                s_pressure_setpoint = v;
                log_control_float("pressure_setpoint", v, 1);
                handle_control_change();
            }
        }
        else if (strcmp(topic, TOPIC_PUMP_PRESSURE_MODE_CMD) == 0)
        {
            bool v = parse_bool_str(payload);
            control_bootstrap_complete();
            if (v != s_control.pumpPressureMode)
            {
                s_control.pumpPressureMode = v;
                s_pump_pressure_mode = v;
                log_control_bool("pump_pressure_mode", v);
                handle_control_change();
            }
        }
        break;
    }
    default:
        break;
    }
}

void MQTT_Start(void)
{
#if defined(MQTT_HOST) && defined(MQTT_PORT)
    if (s_mqtt)
        return;
    if (!s_wifi_ready)
        return;

    build_topics();

    char uri[64];
    snprintf(uri, sizeof(uri), "mqtt://%s:%d", MQTT_HOST, MQTT_PORT);
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = uri,
        .credentials = {
#ifdef MQTT_USER
            .username = MQTT_USER,
#endif
#ifdef MQTT_PASSWORD
            .authentication.password = MQTT_PASSWORD,
#endif
#ifdef MQTT_DISPLAY_CLIENT_ID
            .client_id = MQTT_DISPLAY_CLIENT_ID,
#endif
        },
    };

    s_mqtt = esp_mqtt_client_init(&cfg);
    if (!s_mqtt)
    {
        ESP_LOGE(TAG_MQTT, "Init failed");
        return;
    }
    esp_mqtt_client_register_event(s_mqtt, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_mqtt);
#else
    (void)s_wifi_ready;
    ESP_LOGW(TAG_MQTT, "MQTT disabled (host/port missing)");
#endif
}

void MQTT_Stop(void)
{
#if defined(MQTT_HOST) && defined(MQTT_PORT)
    if (!s_mqtt)
        return;
    esp_mqtt_client_stop(s_mqtt);
    esp_mqtt_client_destroy(s_mqtt);
    s_mqtt = NULL;
    s_mqtt_connected = false;
    reset_pid_discovery_flags();
#endif
}

esp_mqtt_client_handle_t MQTT_GetClient(void) { return s_mqtt; }

int MQTT_Publish(const char *topic, const char *payload, int qos, bool retain)
{
    if (!s_mqtt)
        return -1;
    return esp_mqtt_client_publish(s_mqtt, topic, payload, 0, qos, retain);
}

// -----------------------------------------------------------------------------
// ESP-NOW
// -----------------------------------------------------------------------------
static void ensure_espnow_started(void)
{
    if (s_espnow_active || !s_sta_channel_valid)
        return;

    if (esp_now_init() != ESP_OK)
    {
        ESP_LOGE(TAG_ESPNOW, "esp_now_init failed");
        return;
    }

    esp_now_register_recv_cb(espnow_recv_cb);

    if (s_last_espnow_channel != s_sta_channel)
    {
        esp_err_t err = esp_wifi_set_channel(s_sta_channel, WIFI_SECOND_CHAN_NONE);
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG_ESPNOW, "Failed to set channel %u: %d", (unsigned)s_sta_channel, err);
        }
        s_last_espnow_channel = s_sta_channel;
    }

    memset(&s_broadcast_peer, 0, sizeof(s_broadcast_peer));
    memcpy(s_broadcast_peer.peer_addr, s_broadcast_addr, ESP_NOW_ETH_ALEN);
    s_broadcast_peer.ifidx = ESP_IF_WIFI_STA;
    s_broadcast_peer.channel = s_sta_channel;
    s_broadcast_peer.encrypt = false;

    if (esp_now_is_peer_exist(s_broadcast_peer.peer_addr))
    {
        esp_now_mod_peer(&s_broadcast_peer);
    }
    else
    {
        esp_now_add_peer(&s_broadcast_peer);
    }

    if (!s_espnow_timer)
    {
        s_espnow_timer = xTimerCreate("espnow_to", pdMS_TO_TICKS(ESPNOW_TIMEOUT_MS), pdFALSE, NULL, espnow_timeout_cb);
    }
    if (!s_espnow_ping_timer)
    {
        s_espnow_ping_timer = xTimerCreate("espnow_ping", pdMS_TO_TICKS(ESPNOW_PING_PERIOD_MS), pdTRUE, NULL, espnow_ping_cb);
    }

    if (s_espnow_timer)
        xTimerStart(s_espnow_timer, 0);
    if (s_espnow_ping_timer)
        xTimerStart(s_espnow_ping_timer, 0);

    s_espnow_active = true;
    s_espnow_handshake = false;
    s_use_espnow = false;
    s_controller_peer_valid = false;
    s_espnow_last_rx = 0;
    s_espnow_ping_req = true; // send handshake immediately
    ESP_LOGI(TAG_ESPNOW, "Initialised on channel %u", (unsigned)s_sta_channel);
}

static void stop_espnow(void)
{
    if (!s_espnow_active)
        return;
    if (s_espnow_timer)
        xTimerStop(s_espnow_timer, 0);
    if (s_espnow_ping_timer)
        xTimerStop(s_espnow_ping_timer, 0);
    esp_now_deinit();
    s_espnow_active = false;
    s_espnow_handshake = false;
    s_use_espnow = false;
    s_controller_peer_valid = false;
    ESP_LOGW(TAG_ESPNOW, "Stopped");
}

static void espnow_timeout_cb(TimerHandle_t xTimer)
{
    (void)xTimer;
    s_espnow_timeout_req = true;
}

static void espnow_ping_cb(TimerHandle_t xTimer)
{
    (void)xTimer;
    s_espnow_ping_req = true;
}

static void update_controller_peer(const uint8_t *addr)
{
    memcpy(s_controller_peer.peer_addr, addr, ESP_NOW_ETH_ALEN);
    s_controller_peer.ifidx = ESP_IF_WIFI_STA;
    s_controller_peer.channel = s_sta_channel;
    s_controller_peer.encrypt = false;
    if (esp_now_is_peer_exist(s_controller_peer.peer_addr))
    {
        esp_now_mod_peer(&s_controller_peer);
    }
    else
    {
        esp_now_add_peer(&s_controller_peer);
    }
    s_controller_peer_valid = true;
}

static void send_handshake_request(void)
{
    if (!s_espnow_active)
        return;
    uint8_t payload[2] = {ESPNOW_HANDSHAKE_REQ, s_sta_channel};
    esp_err_t err = esp_now_send(s_broadcast_addr, payload, sizeof(payload));
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG_ESPNOW, "Handshake send failed: %d", err);
    }
}

static void send_sensor_ack(const uint8_t *dest)
{
    if (!s_espnow_active)
        return;
    uint8_t ack = ESPNOW_SENSOR_ACK;
    esp_now_send(dest, &ack, 1);
}

static void send_control_packet(void)
{
    if (!s_espnow_active || !s_use_espnow)
        return;
    if (!s_controller_peer_valid)
        return;
    if (!s_control_dirty)
        return;

    uint32_t revision = ++s_control_revision;

    EspNowControlPacket pkt = {
        .type = ESPNOW_CONTROL_PACKET,
        .flags = 0,
        .pumpMode = s_control.pumpMode,
        .reserved = 0,
        .revision = revision,
        .brewSetpointC = s_control.brewSetpoint,
        .steamSetpointC = s_control.steamSetpoint,
        .pidP = s_control.pidP,
        .pidI = s_control.pidI,
        .pidD = s_control.pidD,
        .pidGuard = s_control.pidGuard,
        .dTau = s_control.dTau,
        .pumpPowerPercent = s_control.pumpPower,
        .pressureSetpointBar = s_control.pressureSetpoint,
    };
    if (s_control.heater)
        pkt.flags |= ESPNOW_CONTROL_FLAG_HEATER;
    if (s_control.steam)
        pkt.flags |= ESPNOW_CONTROL_FLAG_STEAM;
    if (s_control.pumpPressureMode)
        pkt.flags |= ESPNOW_CONTROL_FLAG_PUMP_PRESSURE;

    esp_err_t err = esp_now_send(s_controller_peer.peer_addr, (const uint8_t *)&pkt, sizeof(pkt));
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG_ESPNOW, "Control send failed: %d", err);
    }
    else
    {
        s_control_dirty = false;
        ESP_LOGI(TAG_ESPNOW,
                 "Control sent rev %u: heater=%d steam=%d brew=%.1f steamSet=%.1f pidP=%.2f pidI=%.2f "
                 "pidGuard=%.2f pidD=%.2f dTau=%0.2f pump=%.1f mode=%u pressSet=%.1f pressMode=%d",
                 (unsigned)revision, s_control.heater, s_control.steam,
                 (double)s_control.brewSetpoint, (double)s_control.steamSetpoint,
                 (double)s_control.pidP, (double)s_control.pidI, (double)s_control.pidGuard,
                 (double)s_control.pidD, (double)s_control.dTau, (double)s_control.pumpPower,
                 (unsigned)s_control.pumpMode, (double)s_control.pressureSetpoint,
                 s_control.pumpPressureMode ? 1 : 0);
    }
}

static void schedule_control_send(void)
{
    s_control_dirty = true;
}

static void publish_sensor_to_mqtt(const EspNowPacket *pkt)
{
    if (!s_mqtt_connected)
        return;
    publish_float(TOPIC_CURTEMP, pkt->currentTempC, 1);
    publish_float(TOPIC_SETTEMP, pkt->setTempC, 1);
    publish_float(TOPIC_PRESSURE, pkt->pressureBar, 1);
    publish_float(TOPIC_SHOTVOL, pkt->shotVolumeMl, 1);
    publish_float(TOPIC_SHOT, pkt->shotTimeMs / 1000.0f, 1);
    publish_bool_topic(TOPIC_HEATER, pkt->heaterSwitch != 0);
    publish_bool_topic(TOPIC_STEAM, pkt->steamFlag != 0);
    publish_float(TOPIC_BREW_STATE, pkt->brewSetpointC, 1);
    publish_float(TOPIC_STEAM_STATE, pkt->steamSetpointC, 1);
    publish_float(TOPIC_PRESSURE_SETPOINT_STATE, pkt->pressureSetpointBar, 1);
    publish_bool_topic(TOPIC_PUMP_PRESSURE_MODE_STATE, pkt->pumpPressureMode != 0);
}

static void espnow_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int data_len)
{
    if (data_len <= 0 || !data)
        return;
    if (data[0] == ESPNOW_HANDSHAKE_ACK)
    {
        if (info)
        {
            update_controller_peer(info->src_addr);
        }
        s_use_espnow = true;
        s_espnow_handshake = true;
        s_espnow_last_rx = time(NULL);
        if (s_espnow_timer)
            xTimerReset(s_espnow_timer, 0);
        if (s_control_revision == 0 && !s_control_dirty)
        {
            schedule_control_send();
        }
        return;
    }

    if (data_len == sizeof(EspNowPacket))
    {
        const EspNowPacket *pkt = (const EspNowPacket *)data;
        s_current_temp = pkt->currentTempC;
        s_set_temp = pkt->setTempC;
        s_pressure = pkt->pressureBar;
        s_shot_volume = pkt->shotVolumeMl;
        s_shot_time = pkt->shotTimeMs / 1000.0f;
        s_heater = pkt->heaterSwitch != 0;
        s_steam = pkt->steamFlag != 0;
        s_brew_setpoint = pkt->brewSetpointC;
        s_steam_setpoint = pkt->steamSetpointC;
        s_pressure_setpoint = pkt->pressureSetpointBar;
        s_pump_pressure_mode = pkt->pumpPressureMode != 0;
        publish_sensor_to_mqtt(pkt);
        if (info)
        {
            update_controller_peer(info->src_addr);
            send_sensor_ack(info->src_addr);
        }
        s_use_espnow = true;
        s_espnow_handshake = true;
        s_espnow_last_rx = time(NULL);
        if (s_espnow_timer)
            xTimerReset(s_espnow_timer, 0);
        return;
    }

    if (data[0] == ESPNOW_SENSOR_ACK)
    {
        // Controller acknowledged telemetry acknowledgement; nothing to do.
        return;
    }
}

static void Wireless_Task(void *arg)
{
    (void)arg;
    const TickType_t delay = pdMS_TO_TICKS(50);
    while (1)
    {
        if (s_wifi_ready && !s_mqtt && s_mqtt_enabled)
        {
            MQTT_Start();
        }

        if (s_espnow_timeout_req)
        {
            s_espnow_timeout_req = false;
            ESP_LOGW(TAG_ESPNOW, "Timeout waiting for packets");
            stop_espnow();
            ensure_espnow_started();
        }

        if (s_espnow_ping_req)
        {
            s_espnow_ping_req = false;
            if (!s_espnow_handshake)
            {
                send_handshake_request();
            }
            else if (s_controller_peer_valid)
            {
                // send a light keepalive handshake to ensure controller hears us
                send_handshake_request();
            }
        }

        if (s_use_espnow && s_control_dirty)
        {
            send_control_packet();
        }

        vTaskDelay(delay);
    }
}

void Wireless_SetStandbyMode(bool standby)
{
    if (standby)
    {
        if (s_standby_suppressed)
            return;
        s_standby_suppressed = true;
        s_mqtt_enabled = false;
        MQTT_ForceHeaterState(false);
        MQTT_Stop();
        return;
    }

    if (!s_standby_suppressed)
        return;

    s_standby_suppressed = false;
    s_mqtt_enabled = true;
    MQTT_Start();
    MQTT_ForceHeaterState(true);
}

// -----------------------------------------------------------------------------
// Public getters/setters used by the UI layer
// -----------------------------------------------------------------------------
float MQTT_GetCurrentTemp(void) { return s_current_temp; }
float MQTT_GetSetTemp(void) { return s_set_temp; }
float MQTT_GetCurrentPressure(void) { return s_pressure; }
float MQTT_GetSetPressure(void) { return s_pressure_setpoint; }
bool MQTT_GetPumpPressureMode(void) { return s_pump_pressure_mode; }
float MQTT_GetPumpPower(void) { return s_pump_power; }
float MQTT_GetShotTime(void) { return s_shot_time; }
float MQTT_GetShotVolume(void) { return s_shot_volume; }
uint32_t MQTT_GetZcCount(void) { return s_zc_count; }
bool MQTT_GetHeaterState(void) { return s_heater; }

void MQTT_ForceHeaterState(bool heater)
{
    s_control.heater = heater;
    s_heater = heater;
    handle_control_change();
}

void MQTT_SetHeaterState(bool heater)
{
    if (s_control.heater == heater)
        return;
    MQTT_ForceHeaterState(heater);
}

bool MQTT_GetSteamState(void) { return s_steam; }

void MQTT_SetSteamState(bool steam)
{
    uint8_t changed = apply_steam_request(steam);
    if (!changed)
        return;
    if (changed & HEATER_STATE_CHANGED_FLAG)
        log_control_bool("heater", true);
    if (changed & STEAM_STATE_CHANGED_FLAG)
        log_control_bool("steam", steam);
    handle_control_change();
}

void MQTT_SetPumpPressureMode(bool enabled)
{
    if (s_control.pumpPressureMode == enabled)
        return;
    s_control.pumpPressureMode = enabled;
    s_pump_pressure_mode = enabled;
    log_control_bool("pump_pressure_mode", enabled);
    handle_control_change();
}

void MQTT_SetPressureSetpoint(float pressure)
{
    if (pressure < CONTROL_PRESSURE_MIN)
        pressure = CONTROL_PRESSURE_MIN;
    else if (pressure > CONTROL_PRESSURE_MAX)
        pressure = CONTROL_PRESSURE_MAX;

    if (float_equals(pressure, s_control.pressureSetpoint, CONTROL_PRESSURE_TOLERANCE))
        return;

    s_control.pressureSetpoint = pressure;
    s_pressure_setpoint = pressure;
    log_control_float("pressure_setpoint", pressure, 1);
    handle_control_change();
}

void MQTT_SetPumpPower(float power)
{
    if (power < 40.0f)
        power = 40.0f;
    else if (power > 95.0f)
        power = 95.0f;

    if (float_equals(power, s_control.pumpPower, CONTROL_PUMP_POWER_TOLERANCE))
        return;

    s_control.pumpPower = power;
    s_pump_power = power;
    log_control_float("pump_power", power, 1);
    handle_control_change();
}

bool Wireless_UsingEspNow(void) { return s_use_espnow; }
bool Wireless_IsMQTTConnected(void) { return s_mqtt_connected; }
bool Wireless_IsWiFiConnected(void) { return s_wifi_ready; }

bool Wireless_ControllerStillSendingEspNow(void)
{
    if (!s_espnow_last_rx)
        return false;
    return (time(NULL) - s_espnow_last_rx) < 5;
}

bool Wireless_IsEspNowActive(void) { return s_espnow_active; }
