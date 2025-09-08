#include "Wireless.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "secrets.h"
#include "mqtt_client.h"
#include "freertos/timers.h"
#include <stdlib.h>

uint16_t BLE_NUM = 0;
uint16_t WIFI_NUM = 0;
bool Scan_finish = 0;

bool WiFi_Scan_Finish = 0;
bool BLE_Scan_Finish = 0;
void Wireless_Init(void)
{
    // Initialize NVS.
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
    // WiFi
    xTaskCreatePinnedToCore(
        WIFI_Init, 
        "WIFI task",
        4096, 
        NULL, 
        3, 
        NULL, 
        0);
    // BLE
    xTaskCreatePinnedToCore(
        BLE_Init, 
        "BLE task",
        4096, 
        NULL, 
        2, 
        NULL, 
        0);
}

static volatile bool s_wifi_got_ip = false;
static void on_got_ip(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        printf("Got IP: %d.%d.%d.%d\r\n",
               IP2STR(&event->ip_info.ip));
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
    strncpy((char *)sta_cfg.sta.password, WIFI_PASS, sizeof(sta_cfg.sta.password));
    sta_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL, NULL));

    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());

    // Wait up to ~10s for IP
    const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(10000);
    while (!s_wifi_got_ip && xTaskGetTickCount() < deadline) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (!s_wifi_got_ip) {
        printf("WiFi connect timeout for SSID '%s'\r\n", WIFI_SSID);
    }

    // Start MQTT client once network is up
    extern void MQTT_Start(void);
    MQTT_Start();

    vTaskDelete(NULL);
}
uint16_t WIFI_Scan(void)
{
    uint16_t ap_count = 0;
    esp_wifi_scan_start(NULL, true);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    WiFi_Scan_Finish =1;
    if(BLE_Scan_Finish == 1)
        Scan_finish = 1;
    return ap_count;
}


#define GATTC_TAG "GATTC_TAG"
#define SCAN_DURATION 5  
#define MAX_DISCOVERED_DEVICES 100 

typedef struct {
    uint8_t address[6];
    bool is_valid;
} discovered_device_t;

static discovered_device_t discovered_devices[MAX_DISCOVERED_DEVICES];
static size_t num_discovered_devices = 0;
static size_t num_devices_with_name = 0;


static bool is_device_discovered(const uint8_t *addr) {
    for (size_t i = 0; i < num_discovered_devices; i++) {
        if (memcmp(discovered_devices[i].address, addr, 6) == 0) {
            return true;
        }
    }
    return false;
}


static void add_device_to_list(const uint8_t *addr) {
    if (num_discovered_devices < MAX_DISCOVERED_DEVICES) {
        memcpy(discovered_devices[num_discovered_devices].address, addr, 6);
        discovered_devices[num_discovered_devices].is_valid = true;
        num_discovered_devices++;
    }
}

static bool extract_device_name(const uint8_t *adv_data, uint8_t adv_data_len, char *device_name, size_t max_name_len) {
    size_t offset = 0;
    while (offset < adv_data_len) {
        if (adv_data[offset] == 0) break;

        uint8_t length = adv_data[offset];
        if (length == 0 || offset + length > adv_data_len) break; 

        uint8_t type = adv_data[offset + 1];
        if (type == ESP_BLE_AD_TYPE_NAME_CMPL || type == ESP_BLE_AD_TYPE_NAME_SHORT) {
            if (length > 1 && length - 1 < max_name_len) {
                memcpy(device_name, &adv_data[offset + 2], length - 1);
                device_name[length - 1] = '\0'; 
                return true;
            } else {
                return false;
            }
        }
        offset += length + 1;
    }
    return false;
}

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    static char device_name[100];

    switch (event) {
        case ESP_GAP_BLE_SCAN_RESULT_EVT:
            if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
                if (!is_device_discovered(param->scan_rst.bda)) {
                    add_device_to_list(param->scan_rst.bda);
                    BLE_NUM++; 

                    if (extract_device_name(param->scan_rst.ble_adv, param->scan_rst.adv_data_len, device_name, sizeof(device_name))) {
                        num_devices_with_name++;
                        // printf("Found device: %02X:%02X:%02X:%02X:%02X:%02X\n        Name: %s\n        RSSI: %d\r\n",
                        //          param->scan_rst.bda[0], param->scan_rst.bda[1],
                        //          param->scan_rst.bda[2], param->scan_rst.bda[3],
                        //          param->scan_rst.bda[4], param->scan_rst.bda[5],
                        //          device_name, param->scan_rst.rssi);
                        // printf("\r\n");
                    } else {
                        // printf("Found device: %02X:%02X:%02X:%02X:%02X:%02X\n        Name: Unknown\n        RSSI: %d\r\n",
                        //          param->scan_rst.bda[0], param->scan_rst.bda[1],
                        //          param->scan_rst.bda[2], param->scan_rst.bda[3],
                        //          param->scan_rst.bda[4], param->scan_rst.bda[5],
                        //          param->scan_rst.rssi);
                        // printf("\r\n");
                    }
                }
            }
            break;
        case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
            ESP_LOGI(GATTC_TAG, "Scan complete. Total devices found: %d (with names: %d)", BLE_NUM, num_devices_with_name);
            break;
        default:
            break;
    }
}

void BLE_Init(void *arg)
{
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_bt_controller_init(&bt_cfg);                                            
    if (ret) {
        printf("%s initialize controller failed: %s\n", __func__, esp_err_to_name(ret));        
        return;}
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);                                           
    if (ret) {
        printf("%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));            
        return;}
    ret = esp_bluedroid_init();                                                                 
    if (ret) {
        printf("%s init bluetooth failed: %s\n", __func__, esp_err_to_name(ret));               
        return;}
    ret = esp_bluedroid_enable();                                                               
    if (ret) {
        printf("%s enable bluetooth failed: %s\n", __func__, esp_err_to_name(ret));             
        return;}

    //register the  callback function to the gap module
    ret = esp_ble_gap_register_callback(esp_gap_cb);                                            
    if (ret){
        printf("%s gap register error, error code = %x\n", __func__, ret);                      
        return;
    }
    BLE_Scan();
    vTaskDelete(NULL);

}
uint16_t BLE_Scan(void)
{
    esp_ble_scan_params_t scan_params = {
        .scan_type = BLE_SCAN_TYPE_ACTIVE,
        .own_addr_type = BLE_ADDR_TYPE_RPA_PUBLIC,
        .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
        .scan_interval = 0x50,   
        .scan_window = 0x30,       
        .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
    };
    ESP_ERROR_CHECK(esp_ble_gap_set_scan_params(&scan_params));

    printf("Starting BLE scan...\n");
    ESP_ERROR_CHECK(esp_ble_gap_start_scanning(SCAN_DURATION));
    
    // Set scanning duration
    vTaskDelay(SCAN_DURATION * 1000 / portTICK_PERIOD_MS);
    
    printf("Stopping BLE scan...\n");
    // ESP_ERROR_CHECK(esp_ble_gap_stop_scanning());
    ESP_ERROR_CHECK(esp_ble_dtm_stop());
    BLE_Scan_Finish = 1;
    if(WiFi_Scan_Finish == 1)
        Scan_finish = 1;
    return BLE_NUM;
}

// -------------------- MQTT client (subscriber/publisher) --------------------
static esp_mqtt_client_handle_t s_mqtt = NULL;
static TimerHandle_t s_mqtt_update_timer = NULL;
static float s_current_temp = 0.0f;
static float s_set_temp = 0.0f;
static const char *s_mqtt_topics[] = {
    "brew_setpoint",
    "steam_setpoint",
    "heater",
    "shot_volume",
    "set_temp",
    "current_temp",
    "pressure",
    "shot_state",
    "steam_state",
};

static void mqtt_subscribe_all(bool log)
{
    if (!s_mqtt) return;
    char topic_buf[128];
    for (size_t i = 0; i < (sizeof(s_mqtt_topics) / sizeof(s_mqtt_topics[0])); ++i) {
        int n = snprintf(topic_buf, sizeof(topic_buf), "gaggia_classic/%s/%s/state", GAGGIA_ID, s_mqtt_topics[i]);
        if (n > 0 && n < (int)sizeof(topic_buf)) {
            esp_mqtt_client_subscribe(s_mqtt, topic_buf, 1);
            if (log) {
                printf("MQTT subscribed: %s\r\n", topic_buf);
            }
        }
    }
}

static void mqtt_update_timer_cb(TimerHandle_t xTimer)
{
    (void)xTimer;
    mqtt_subscribe_all(false);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    char t_copy[128];
    char d_copy[256];
    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        printf("MQTT connected\r\n");
        mqtt_subscribe_all(true);
        if (s_mqtt_update_timer) {
            xTimerStart(s_mqtt_update_timer, 0);
        }
        #ifdef MQTT_LWT_TOPIC
        // Optionally clear retained LWT by publishing online
        esp_mqtt_client_publish(event->client, MQTT_LWT_TOPIC, "online", 0, 1, true);
        #endif
        break;
    case MQTT_EVENT_DISCONNECTED:
        printf("MQTT disconnected\r\n");
        if (s_mqtt_update_timer) {
            xTimerStop(s_mqtt_update_timer, 0);
        }
        break;
    case MQTT_EVENT_DATA:
        // Copy topic and data into local, null-terminated buffers for safe printing
        {
            int tl = event->topic_len < (int)sizeof(t_copy) - 1 ? event->topic_len : (int)sizeof(t_copy) - 1;
            int dl = event->data_len < (int)sizeof(d_copy) - 1 ? event->data_len : (int)sizeof(d_copy) - 1;
            memcpy(t_copy, event->topic, tl); t_copy[tl] = '\0';
            memcpy(d_copy, event->data, dl); d_copy[dl] = '\0';
            printf("MQTT state [%s] = %s\r\n", t_copy, d_copy);
            if (strstr(t_copy, "current_temp")) {
                s_current_temp = strtof(d_copy, NULL);
            } else if (strstr(t_copy, "set_temp")) {
                s_set_temp = strtof(d_copy, NULL);
            }
        }
        break;
    default:
        break;
    }
}

void MQTT_Start(void)
{
#if defined(MQTT_URI)
    if (s_mqtt || !s_wifi_got_ip) return;
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = MQTT_URI,
        .session.last_will = {
            #ifdef MQTT_LWT_TOPIC
            .topic = MQTT_LWT_TOPIC,
            .msg = "offline",
            .msg_len = 7,
            .qos = 1,
            .retain = true,
            #endif
        },
        .credentials = {
            #ifdef MQTT_USERNAME
            .username = MQTT_USERNAME,
            #endif
            #ifdef MQTT_PASSWORD
            .authentication.password = MQTT_PASSWORD,
            #endif
            #ifdef MQTT_CLIENT_ID
            .client_id = MQTT_CLIENT_ID,
            #endif
        },
    };

    s_mqtt = esp_mqtt_client_init(&cfg);
    if (!s_mqtt) {
        printf("MQTT init failed\r\n");
        return;
    }
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(s_mqtt, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(s_mqtt));
    if (!s_mqtt_update_timer) {
        s_mqtt_update_timer = xTimerCreate("mqtt_update", pdMS_TO_TICKS(1000), pdTRUE, NULL, mqtt_update_timer_cb);
    }
#else
    (void)s_mqtt;
    if (!s_wifi_got_ip) return;
    printf("MQTT: MQTT_URI not defined in secrets.h; disabled\r\n");
#endif
}

float MQTT_GetCurrentTemp(void)
{
    return s_current_temp;
}

float MQTT_GetSetTemp(void)
{
    return s_set_temp;
}

esp_mqtt_client_handle_t MQTT_GetClient(void)
{
    return s_mqtt;
}

int MQTT_Publish(const char *topic, const char *payload, int qos, bool retain)
{
    if (!s_mqtt) return -1;
    return esp_mqtt_client_publish(s_mqtt, topic, payload, 0, qos, retain);
}
