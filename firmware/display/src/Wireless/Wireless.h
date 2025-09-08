#pragma once

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "nvs_flash.h" 
#include "esp_log.h"

#include <stdio.h>
#include <string.h>  // For memcpy
#include "esp_system.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_main.h"

#include "mqtt_client.h"



extern uint16_t BLE_NUM;
extern uint16_t WIFI_NUM;
extern bool Scan_finish;

void Wireless_Init(void);
void WIFI_Init(void *arg);
uint16_t WIFI_Scan(void);
void BLE_Init(void *arg);
uint16_t BLE_Scan(void);

// MQTT
void MQTT_Start(void);
esp_mqtt_client_handle_t MQTT_GetClient(void);
int MQTT_Publish(const char *topic, const char *payload, int qos, bool retain);
float MQTT_GetCurrentTemp(void);
float MQTT_GetSetTemp(void);
