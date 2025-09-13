#include "Wireless.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_log.h"
#include "freertos/timers.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "secrets.h"
#include "mqtt_topics.h"
#include "esp_sntp.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>  // strcmp, memcpy, strncpy
#include <strings.h> // strcasecmp
#include <time.h>

#include "EspNowPacket.h"

#define ESPNOW_TIMEOUT_MS 15000
#define ESPNOW_PING_PERIOD_MS 500
#define ESPNOW_HANDSHAKE_REQ 0xAA
#define ESPNOW_HANDSHAKE_ACK 0x55
#define ESPNOW_CMD_HEATER_ON 0xA1
#define ESPNOW_CMD_HEATER_OFF 0xA0
#define ESPNOW_CMD_STEAM_ON 0xB1
#define ESPNOW_CMD_STEAM_OFF 0xB0

// --- B: exact topic strings ---------------------------------------------------
static char TOPIC_HEATER[128];
static char TOPIC_HEATER_SET[128];
static char TOPIC_STEAM[128];
static char TOPIC_STEAM_SET[128];
static char TOPIC_CURTEMP[128];
static char TOPIC_SETTEMP[128];
static char TOPIC_PRESSURE[128];
static char TOPIC_SHOTVOL[128];
static char TOPIC_SHOT[128];
static char TOPIC_ESPNOW_CHAN[128];
static char TOPIC_ESPNOW_MAC[128];
static char TOPIC_ESPNOW_CMD[128];
static char TOPIC_ESPNOW_LAST[128];

static inline void build_topics(void)
{
    snprintf(TOPIC_HEATER, sizeof TOPIC_HEATER, "%s/%s/heater/state", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_HEATER_SET, sizeof TOPIC_HEATER_SET,
             "%s/%s/heater/set", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_STEAM, sizeof TOPIC_STEAM, "%s/%s/steam/state", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_STEAM_SET, sizeof TOPIC_STEAM_SET,
             "%s/%s/steam/set", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_CURTEMP, sizeof TOPIC_CURTEMP, "%s/%s/current_temp/state", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_SETTEMP, sizeof TOPIC_SETTEMP, "%s/%s/set_temp/state", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_PRESSURE, sizeof TOPIC_PRESSURE, "%s/%s/pressure/state", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_SHOTVOL, sizeof TOPIC_SHOTVOL, "%s/%s/shot_volume/state", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_SHOT, sizeof TOPIC_SHOT, "%s/%s/shot/state", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_ESPNOW_CHAN, sizeof TOPIC_ESPNOW_CHAN, "%s/%s/espnow/channel", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_ESPNOW_MAC, sizeof TOPIC_ESPNOW_MAC, "%s/%s/espnow/mac", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_ESPNOW_CMD, sizeof TOPIC_ESPNOW_CMD, "%s/%s/espnow/cmd", GAG_TOPIC_ROOT, GAGGIA_ID);
    snprintf(TOPIC_ESPNOW_LAST, sizeof TOPIC_ESPNOW_LAST, "%s/%s/espnow/last", GAG_TOPIC_ROOT, GAGGIA_ID);
}

// tolerant bool parse: "1"/"true"/"on" => true
static inline bool parse_bool_str(const char *s)
{
    return (strcmp(s, "1") == 0) || (strcasecmp(s, "true") == 0) || (strcasecmp(s, "on") == 0);
}

static void espnow_timeout_cb(TimerHandle_t xTimer);
static void espnow_ping_cb(TimerHandle_t xTimer);
static void espnow_log_timer_cb(TimerHandle_t xTimer);
static volatile bool s_espnow_timeout_req = false;
static volatile bool s_espnow_ping_req = false;
static void try_start_espnow(void);
static volatile bool s_espnow_start_req = false; // request to start espnow (deferred)
static void espnow_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data,
                           int data_len);
static void Wireless_Task(void *arg);

void Wireless_Init(void)
{
    // Initialize NVS.
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    // WiFi
    xTaskCreatePinnedToCore(WIFI_Init, "WIFI task", 4096, NULL, 3, NULL, 0);
    // Background task for deferred wireless work
    xTaskCreatePinnedToCore(Wireless_Task, "wireless", 4096, NULL, 3, NULL, 0);
}

static volatile bool s_wifi_got_ip = false;
static void on_got_ip(void *arg, esp_event_base_t base, int32_t id,
                      void *data)
{
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        ESP_LOGI("WiFi", "Got IP: %d.%d.%d.%d", IP2STR(&event->ip_info.ip));
        s_wifi_got_ip = true;
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

    // Apply credentials from secrets.h
    wifi_config_t sta_cfg = {0};
    strncpy((char *)sta_cfg.sta.ssid, WIFI_SSID, sizeof(sta_cfg.sta.ssid));
    strncpy((char *)sta_cfg.sta.password, WIFI_PASSWORD,
            sizeof(sta_cfg.sta.password));
    sta_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL, NULL));

    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());

    // Wait up to ~10s for IP
    const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(10000);
    while (!s_wifi_got_ip && xTaskGetTickCount() < deadline)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (!s_wifi_got_ip)
    {
        ESP_LOGW("WiFi", "Connect timeout for SSID '%s'", WIFI_SSID);
    }
    else
    {
        // Sync clock via NTP when WiFi connects
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "pool.ntp.org");
        esp_sntp_init();
        time_t now = 0;
        struct tm timeinfo = {0};
        int retry = 0;
        while (timeinfo.tm_year < (2016 - 1900) && ++retry < 10)
        {
            vTaskDelay(pdMS_TO_TICKS(2000));
            time(&now);
            localtime_r(&now, &timeinfo);
        }
        if (timeinfo.tm_year >= (2016 - 1900))
        {
            ESP_LOGI("TIME", "RTC synced: %04d-%02d-%02d %02d:%02d:%02d",
                     timeinfo.tm_year + 1900, timeinfo.tm_mon + 1,
                     timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min,
                     timeinfo.tm_sec);
        }
        else
        {
            ESP_LOGW("TIME", "RTC sync failed");
        }
    }

    // Start MQTT client once network is up
    extern void MQTT_Start(void);
    MQTT_Start();
    try_start_espnow();

    vTaskDelete(NULL);
}
// -------------------- MQTT client (subscriber/publisher) --------------------
static esp_mqtt_client_handle_t s_mqtt = NULL;
static float s_current_temp = 0.0f;
static float s_set_temp = 0.0f;
static float s_pressure = 0.0f;
static float s_shot_time = 0.0f;
static float s_shot_volume = 0.0f;
static bool s_heater = false;
static bool s_steam = false;
static bool s_use_espnow = false;
static bool s_mqtt_connected = false;
static bool s_mqtt_stopping = false; // avoid repeated stop requests
static uint8_t s_espnow_peer[ESP_NOW_ETH_ALEN];
static volatile bool s_espnow_packet = false;
static TimerHandle_t s_espnow_timer = NULL;
static TimerHandle_t s_espnow_ping_timer = NULL;
static TimerHandle_t s_espnow_log_timer = NULL;
static volatile uint32_t s_espnow_packet_count = 0;
static bool s_espnow_active = false;
static bool s_espnow_handshake = false;
static int s_espnow_channel = 0;
static bool s_have_espnow_mac = false;
static bool s_have_espnow_chan = false;
static time_t s_espnow_last_rx = 0;
static time_t s_mqtt_espnow_last = 0;
static const char *s_mqtt_topics[] = {
    "brew_setpoint",
    "steam_setpoint",
    "heater",
    "shot_volume",
    "set_temp",
    "current_temp",
    "shot",
    "steam",
    "pressure",
};

static void mqtt_subscribe_all(bool log)
{
    if (!s_mqtt)
        return;
    char topic_buf[128];
    for (size_t i = 0; i < (sizeof(s_mqtt_topics) / sizeof(s_mqtt_topics[0]));
         ++i)
    {
        int n = snprintf(topic_buf, sizeof(topic_buf), "%s/%s/%s/state",
                         GAG_TOPIC_ROOT, GAGGIA_ID, s_mqtt_topics[i]);
        if (n > 0 && n < (int)sizeof(topic_buf))
        {
            esp_mqtt_client_subscribe(s_mqtt, topic_buf, 1);
            if (log)
            {
                ESP_LOGI("MQTT", "Subscribed: %s", topic_buf);
            }
        }
    }
}

// --- A: disable periodic re-subscribe ---------------------------------------
#if 0
static TimerHandle_t s_mqtt_update_timer = NULL;
static void mqtt_update_timer_cb(TimerHandle_t xTimer) {
  (void)xTimer;
  mqtt_subscribe_all(false);
}
#endif

// --- B: mqtt_event_handler with exact topic matches ---------------------------
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    char t_copy[128];
    char d_copy[256];

    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI("MQTT", "Connected");
        s_mqtt_connected = true;
        s_mqtt_stopping = false;
        mqtt_subscribe_all(true);
        esp_mqtt_client_subscribe(event->client, TOPIC_ESPNOW_CHAN, 1);
        esp_mqtt_client_subscribe(event->client, TOPIC_ESPNOW_MAC, 1);
        esp_mqtt_client_subscribe(event->client, TOPIC_ESPNOW_LAST, 1);
#ifdef MQTT_STATUS
        esp_mqtt_client_publish(event->client, MQTT_STATUS, "online", 0, 1, true);
#endif
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI("MQTT", "Disconnected");
        s_mqtt_connected = false;
        s_mqtt_stopping = false;
        break;

    case MQTT_EVENT_DATA:
    {
        int tl = event->topic_len < (int)sizeof(t_copy) - 1 ? event->topic_len : (int)sizeof(t_copy) - 1;
        int dl = event->data_len < (int)sizeof(d_copy) - 1 ? event->data_len : (int)sizeof(d_copy) - 1;
        memcpy(t_copy, event->topic, tl);
        t_copy[tl] = '\0';
        memcpy(d_copy, event->data, dl);
        d_copy[dl] = '\0';
        ESP_LOGI("MQTT", "State [%s] = %s", t_copy, d_copy);

        if (strcmp(t_copy, TOPIC_CURTEMP) == 0)
            s_current_temp = strtof(d_copy, NULL);
        else if (strcmp(t_copy, TOPIC_SETTEMP) == 0)
            s_set_temp = strtof(d_copy, NULL);
        else if (strcmp(t_copy, TOPIC_PRESSURE) == 0)
            s_pressure = strtof(d_copy, NULL);
        else if (strcmp(t_copy, TOPIC_SHOTVOL) == 0)
            s_shot_volume = strtof(d_copy, NULL);
        else if (strcmp(t_copy, TOPIC_SHOT) == 0)
            s_shot_time = strtof(d_copy, NULL);
        else if (strcmp(t_copy, TOPIC_HEATER) == 0)
            s_heater = parse_bool_str(d_copy);
        else if (strcmp(t_copy, TOPIC_STEAM) == 0)
            s_steam = parse_bool_str(d_copy);
        else if (strcmp(t_copy, TOPIC_ESPNOW_CHAN) == 0)
        {
            s_espnow_channel = atoi(d_copy);
            s_have_espnow_chan = true;
            // Auto-start when both MAC and channel are known
            if (!s_espnow_active && s_have_espnow_mac)
                s_espnow_start_req = true;
        }
        else if (strcmp(t_copy, TOPIC_ESPNOW_MAC) == 0)
        {
            unsigned int b[6];
            if (sscanf(d_copy, "%02x:%02x:%02x:%02x:%02x:%02x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) == 6)
            {
                for (int i = 0; i < 6; ++i)
                    s_espnow_peer[i] = (uint8_t)b[i];
                s_have_espnow_mac = true;
                // Auto-start when both MAC and channel are known
                if (!s_espnow_active && s_have_espnow_chan)
                    s_espnow_start_req = true;
            }
        }
        else if (strcmp(t_copy, TOPIC_ESPNOW_LAST) == 0)
        {
            s_mqtt_espnow_last = (time_t)strtoul(d_copy, NULL, 10);
        }
        break;
    }

    default:
        break;
    }
}

// --- A: MQTT_Start (no periodic re-subscribe timer) --------------------------
void MQTT_Start(void)
{
#if defined(MQTT_HOST) && defined(MQTT_PORT)
    if (s_mqtt || !s_wifi_got_ip)
        return;

    char mqtt_uri[64];
    snprintf(mqtt_uri, sizeof(mqtt_uri), "mqtt://%s:%d", MQTT_HOST, MQTT_PORT);

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = mqtt_uri,
        .session.last_will = {
#ifdef MQTT_STATUS
            .topic = MQTT_STATUS,
            .msg = "offline",
            .msg_len = 7,
            .qos = 1,
            .retain = true,
#endif
        },
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

    // inside MQTT_Start(), before esp_mqtt_client_init():
    build_topics();

    s_mqtt = esp_mqtt_client_init(&cfg);
    if (!s_mqtt)
    {
        ESP_LOGE("MQTT", "Init failed");
        return;
    }
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(s_mqtt, ESP_EVENT_ANY_ID,
                                                   mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(s_mqtt));
#else
    (void)s_mqtt;
    if (!s_wifi_got_ip)
        return;
    ESP_LOGW("MQTT", "MQTT_HOST or MQTT_PORT not defined in secrets.h; disabled");
#endif
}

float MQTT_GetCurrentTemp(void) { return s_current_temp; }

float MQTT_GetSetTemp(void) { return s_set_temp; }

float MQTT_GetCurrentPressure(void) { return s_pressure; }

float MQTT_GetShotTime(void) { return s_shot_time; }

float MQTT_GetShotVolume(void) { return s_shot_volume; }

bool MQTT_GetHeaterState(void) { return s_heater; }

void MQTT_SetHeaterState(bool heater)
{
    if (heater != s_heater)
    {
        s_heater = heater;
        if (s_mqtt)
        {
            MQTT_Publish(TOPIC_HEATER_SET, s_heater ? "ON" : "OFF", 1, true);
        }
        if (s_use_espnow)
        {
            uint8_t cmd = s_heater ? ESPNOW_CMD_HEATER_ON : ESPNOW_CMD_HEATER_OFF;
            esp_now_send(s_espnow_peer, &cmd, 1);
        }
    }
}

bool MQTT_GetSteamState(void) { return s_steam; }

void MQTT_SetSteamState(bool steam)
{
    if (steam != s_steam)
    {
        s_steam = steam;
        if (s_mqtt)
        {
            MQTT_Publish(TOPIC_STEAM_SET, s_steam ? "ON" : "OFF", 1, true);
        }
        if (s_use_espnow)
        {
            uint8_t cmd = s_steam ? ESPNOW_CMD_STEAM_ON : ESPNOW_CMD_STEAM_OFF;
            esp_now_send(s_espnow_peer, &cmd, 1);
        }
    }
}

esp_mqtt_client_handle_t MQTT_GetClient(void) { return s_mqtt; }

int MQTT_Publish(const char *topic, const char *payload, int qos, bool retain)
{
    if (!s_mqtt)
        return -1;
    return esp_mqtt_client_publish(s_mqtt, topic, payload, 0, qos, retain);
}

static void espnow_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data,
                           int data_len)
{
    s_espnow_last_rx = time(NULL);
    if (data_len == 1 && data[0] == ESPNOW_HANDSHAKE_ACK)
    {
        if (info)
            ESP_LOGI("ESP-NOW", "Handshake ack from %02X:%02X:%02X:%02X:%02X:%02X",
                     info->src_addr[0], info->src_addr[1], info->src_addr[2],
                     info->src_addr[3], info->src_addr[4], info->src_addr[5]);
        else
            ESP_LOGI("ESP-NOW", "Handshake acknowledged");
        s_espnow_handshake = true;
        s_use_espnow = true;
        s_espnow_packet = true;
        if (info)
            memcpy(s_espnow_peer, info->src_addr, ESP_NOW_ETH_ALEN);
        if (s_espnow_timer)
            xTimerReset(s_espnow_timer, 0);
        if (s_espnow_ping_timer)
            xTimerReset(s_espnow_ping_timer, 0);
        if (!s_espnow_log_timer)
            s_espnow_log_timer =
                xTimerCreate("espnow_log", pdMS_TO_TICKS(10000), pdTRUE, NULL,
                             espnow_log_timer_cb);
        if (s_espnow_log_timer)
            xTimerReset(s_espnow_log_timer, 0);
        s_espnow_packet_count = 0;
        return;
    }
    if (s_espnow_handshake)
    {
        s_espnow_packet_count++;
    }
    if (data_len == sizeof(struct EspNowPacket))
    {
        const struct EspNowPacket *pkt = (const struct EspNowPacket *)data;

        bool heater = pkt->heaterSwitch != 0;
        if (heater != s_heater)
        {
            s_heater = heater;
            if (s_mqtt)
            {
                MQTT_Publish(TOPIC_HEATER_SET, s_heater ? "ON" : "OFF", 1, true);
            }
        }
        s_steam = pkt->steamFlag != 0;
        s_shot_time = (float)pkt->shotTimeMs / 1000.0f;
        s_shot_volume = pkt->shotVolumeMl;
        s_set_temp = pkt->setTempC;
        s_current_temp = pkt->currentTempC;
        s_pressure = pkt->pressureBar;
    }
    if (info)
    {
        memcpy(s_espnow_peer, info->src_addr, ESP_NOW_ETH_ALEN);
    }
    if (!s_use_espnow)
    {
        s_use_espnow = true;
    }
    if (s_espnow_timer)
    {
        xTimerStop(s_espnow_timer, 0);
    }
    if (s_espnow_ping_timer)
    {
        xTimerStop(s_espnow_ping_timer, 0);
    }
    s_espnow_packet = true;
}

static void try_start_espnow(void)
{
    if (s_espnow_active || !s_have_espnow_mac || !s_have_espnow_chan)
    {
        ESP_LOGI("ESP-NOW", "Start check: active=%d have_mac=%d have_chan=%d",
                 (int)s_espnow_active, (int)s_have_espnow_mac, (int)s_have_espnow_chan);
        return;
    }
    // Ensure prerequisites met: MQTT stopped and STA disconnected
    if (s_mqtt && s_mqtt_connected && !s_mqtt_stopping)
    {
        ESP_LOGI("ESP-NOW", "Waiting for MQTT stop before init");
        return;
    }
    if (s_wifi_got_ip)
    {
        ESP_LOGI("ESP-NOW", "Waiting for STA disconnect before init");
        return;
    }
    s_espnow_active = true;
    ESP_LOGI("ESP-NOW", "Initializing on channel %d", s_espnow_channel);
    if (esp_now_init() != ESP_OK)
    {
        s_espnow_active = false;
        ESP_LOGE("ESP-NOW", "esp_now_init failed");
        return;
    }
    esp_now_register_recv_cb(espnow_recv_cb);
    esp_wifi_set_channel(s_espnow_channel, WIFI_SECOND_CHAN_NONE);
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, s_espnow_peer, ESP_NOW_ETH_ALEN);
    peer.ifidx = ESP_IF_WIFI_STA;
    peer.channel = s_espnow_channel;
    peer.encrypt = false;
    if (esp_now_is_peer_exist(peer.peer_addr))
    {
        esp_now_peer_info_t old_peer = {0};
        bool have_old = (esp_now_get_peer(peer.peer_addr, &old_peer) == ESP_OK);
        esp_err_t err = esp_now_mod_peer(&peer);
        if (err != ESP_OK)
        {
            ESP_LOGW("ESP-NOW", "esp_now_mod_peer failed: %d", err);
        }
        else
        {
            ESP_LOGI("ESP-NOW", "Peer modified");
            if (have_old)
            {
                if (old_peer.channel != peer.channel)
                    ESP_LOGI("ESP-NOW", " channel %d->%d", old_peer.channel, peer.channel);
                if (old_peer.ifidx != peer.ifidx)
                    ESP_LOGI("ESP-NOW", " ifidx %d->%d", old_peer.ifidx, peer.ifidx);
                if (old_peer.encrypt != peer.encrypt)
                    ESP_LOGI("ESP-NOW", " encrypt %d->%d", old_peer.encrypt, peer.encrypt);
            }
        }
    }
    else
    {
        esp_err_t err = esp_now_add_peer(&peer);
        if (err != ESP_OK)
        {
            ESP_LOGW("ESP-NOW", "esp_now_add_peer failed: %d", err);
        }
        else
        {
            ESP_LOGI("ESP-NOW", "Peer added");
        }
    }
    ESP_LOGI("ESP-NOW", "Initialized on channel %d", s_espnow_channel);
    s_espnow_packet = false;
    if (!s_espnow_timer)
    {
        s_espnow_timer = xTimerCreate("espnow", pdMS_TO_TICKS(ESPNOW_TIMEOUT_MS),
                                      pdFALSE, NULL, espnow_timeout_cb);
    }
    if (s_espnow_timer)
    {
        ESP_LOGI("ESP-NOW", "Starting timeout timer: %d ms", (int)ESPNOW_TIMEOUT_MS);
        xTimerStart(s_espnow_timer, 0);
    }

    // Start periodic small probe to wake/validate link
    if (!s_espnow_ping_timer)
    {
        s_espnow_ping_timer = xTimerCreate("espnow_ping", pdMS_TO_TICKS(ESPNOW_PING_PERIOD_MS),
                                           pdTRUE, NULL, espnow_ping_cb);
    }
    if (s_espnow_ping_timer)
    {
        ESP_LOGI("ESP-NOW", "Starting ping timer: %d ms", (int)ESPNOW_PING_PERIOD_MS);
        xTimerStart(s_espnow_ping_timer, 0);
    }

    // Kick off handshake
    s_espnow_handshake = false;
    uint8_t hs = ESPNOW_HANDSHAKE_REQ;
    esp_err_t err = esp_now_send(s_espnow_peer, &hs, 1);
    if (err != ESP_OK)
    {
        ESP_LOGW("ESP-NOW", "handshake send failed: %d", (int)err);
    }
    else
    {
        ESP_LOGI("ESP-NOW", "Handshake request sent");
    }
}

static void espnow_ping_cb(TimerHandle_t xTimer)
{
    (void)xTimer;
    // Defer ping/handshake send to task context
    s_espnow_ping_req = true;
}

static void espnow_timeout_cb(TimerHandle_t xTimer)
{
    (void)xTimer;
    // Defer heavy work to a normal task context
    s_espnow_timeout_req = true;
}

static void espnow_log_timer_cb(TimerHandle_t xTimer)
{
    (void)xTimer;
    ESP_LOGI("ESP-NOW", "Packets received in last 10s: %u",
             (unsigned int)s_espnow_packet_count);
    s_espnow_packet_count = 0;
}

bool Wireless_UsingEspNow(void)
{
    return s_use_espnow;
}

bool Wireless_IsMQTTConnected(void)
{
    return s_mqtt_connected;
}

bool Wireless_ControllerStillSendingEspNow(void)
{
    return s_mqtt_espnow_last > s_espnow_last_rx;
}

bool Wireless_IsEspNowActive(void)
{
    return s_espnow_active;
}

static void Wireless_Poll(void)
{
    // Handle deferred ESP-NOW timeout actions
    if (s_espnow_timeout_req)
    {
        s_espnow_timeout_req = false;
        if (!s_espnow_packet && s_espnow_active)
        {
            esp_now_deinit();
            if (s_espnow_ping_timer)
            {
                xTimerStop(s_espnow_ping_timer, 0);
            }
            if (s_espnow_log_timer)
            {
                xTimerStop(s_espnow_log_timer, 0);
            }
            s_espnow_packet_count = 0;
            s_use_espnow = false;
            s_espnow_last_rx = 0; // clear stale timestamp so MQTT comparison is valid
            esp_wifi_connect();
            if (s_mqtt)
            {
                esp_mqtt_client_start(s_mqtt);
                MQTT_Publish(TOPIC_ESPNOW_CMD, "ON", 1, false);
            }
            s_espnow_active = false;
            s_espnow_handshake = false;
        }
    }

    // Handle pending ESP-NOW ping/handshake sends
    if (s_espnow_ping_req)
    {
        s_espnow_ping_req = false;
        uint8_t b = s_espnow_handshake ? 0xA5 : ESPNOW_HANDSHAKE_REQ;
        esp_err_t err = esp_now_send(s_espnow_peer, &b, 1);
        if (err != ESP_OK)
        {
            ESP_LOGW("ESP-NOW", "ping/handshake send failed: %d", (int)err);
        }
        else if (!s_espnow_handshake)
        {
            ESP_LOGI("ESP-NOW", "Handshake request sent");
        }
    }

    // Ensure sequence: 1) stop MQTT, 2) disconnect STA, 3) init ESP-NOW
    if (s_espnow_start_req)
    {
        // If already active or missing params, wait
        if (!s_espnow_active && s_have_espnow_mac && s_have_espnow_chan)
        {
            // Step 1: stop MQTT client if running
            if (s_mqtt && s_mqtt_connected && !s_mqtt_stopping)
            {
                // Notify controller to switch to ESP-NOW
                MQTT_Publish(TOPIC_ESPNOW_CMD, "ON", 1, false);
                esp_mqtt_client_stop(s_mqtt);
                // Mark stopping to prevent repeated stop calls; wait for DISCONNECTED event
                s_mqtt_stopping = true;
                // Be permissive: proceed even if DISCONNECTED event is delayed
                s_mqtt_connected = false;
                return;
            }

            // Step 2: disconnect WiFi STA if still connected (we use s_wifi_got_ip as proxy)
            if (s_wifi_got_ip)
            {
                esp_wifi_disconnect();
                s_wifi_got_ip = false;
                return;
            }

            // Step 3: init ESP-NOW
            try_start_espnow();
            // Clear request once begun
            s_espnow_start_req = false;
        }
    }
}

static void Wireless_Task(void *arg)
{
    (void)arg;
    const TickType_t delay = pdMS_TO_TICKS(50);
    while (1)
    {
        Wireless_Poll();
        vTaskDelay(delay);
    }
}
