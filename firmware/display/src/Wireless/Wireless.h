#pragma once

#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <string.h> // For memcpy

#include "mqtt_client.h"
#include <stdbool.h>

void Wireless_Init(void);
void WIFI_Init(void *arg);
// MQTT
void MQTT_Start(void);
esp_mqtt_client_handle_t MQTT_GetClient(void);
int MQTT_Publish(const char *topic, const char *payload, int qos, bool retain);
float MQTT_GetCurrentTemp(void);
float MQTT_GetSetTemp(void);
float MQTT_GetCurrentPressure(void);
float MQTT_GetSetPressure(void);
float MQTT_GetShotTime(void);
float MQTT_GetShotVolume(void);
bool MQTT_GetHeaterState(void);
bool MQTT_GetSteamState(void);

bool Wireless_UsingEspNow(void);
bool Wireless_IsMQTTConnected(void);
