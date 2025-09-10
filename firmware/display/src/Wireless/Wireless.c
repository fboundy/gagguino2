#include "Wireless.h"

#include "EspNowPacket.h"
#include "secrets.h"

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include <string.h>

static float s_current_temp = 0.0f;
static float s_set_temp = 0.0f;
static float s_pressure = 0.0f;
static float s_shot_time = 0.0f;
static float s_shot_volume = 0.0f;
static bool s_heater = false;
static bool s_steam = false;
static bool s_use_espnow = false;

static void espnow_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data,
                           int data_len) {
    if (data_len != sizeof(EspNowPacket)) {
        return;
    }
    const EspNowPacket *pkt = (const EspNowPacket *)data;
    s_shot_time = pkt->shotTimeMs / 1000.0f;
    s_shot_volume = pkt->shotVolumeMl;
    s_set_temp = pkt->setTempC;
    s_current_temp = pkt->currentTempC;
    s_pressure = pkt->pressureBar;
    s_heater = pkt->heaterSwitch;
    s_steam = pkt->steamFlag;
    s_use_espnow = true;
}

void Wireless_Init(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_now_init());
    esp_now_register_recv_cb(espnow_recv_cb);

    const uint8_t peer_addr[ESP_NOW_ETH_ALEN] = CONTROLLER_MAC;
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, peer_addr, ESP_NOW_ETH_ALEN);
    peer.ifidx = ESP_IF_WIFI_STA;
    peer.channel = ESPNOW_CHANNEL;
    peer.encrypt = false;
    esp_now_add_peer(&peer);
}

bool Wireless_UsingEspNow(void) { return s_use_espnow; }

void Wireless_Poll(void) {
    // nothing needed; receive callback handles updates
}

float Wireless_GetCurrentTemp(void) { return s_current_temp; }
float Wireless_GetSetTemp(void) { return s_set_temp; }
float Wireless_GetCurrentPressure(void) { return s_pressure; }
float Wireless_GetShotTime(void) { return s_shot_time; }
float Wireless_GetShotVolume(void) { return s_shot_volume; }
bool Wireless_GetHeaterState(void) { return s_heater; }
bool Wireless_GetSteamState(void) { return s_steam; }
