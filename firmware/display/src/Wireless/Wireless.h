#pragma once

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "nvs_flash.h" 
#include <stdio.h>
#include <string.h>  // For memcpy
#include "esp_system.h"

#include "mqtt_client.h"

void Wireless_Init(void);
void WIFI_Init(void *arg);
uint16_t WIFI_Scan(void);

// MQTT
void MQTT_Start(void);
esp_mqtt_client_handle_t MQTT_GetClient(void);
int MQTT_Publish(const char *topic, const char *payload, int qos, bool retain);
float MQTT_GetCurrentTemp(void);
float MQTT_GetSetTemp(void);
float MQTT_GetPressure(void);
