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
#include <stdint.h>

void Wireless_Init(void);
void WIFI_Init(void *arg);
// MQTT
void MQTT_Start(void);
void MQTT_Stop(void);
esp_mqtt_client_handle_t MQTT_GetClient(void);
int MQTT_Publish(const char *topic, const char *payload, int qos, bool retain);
float MQTT_GetCurrentTemp(void);
float MQTT_GetSetTemp(void);
float MQTT_GetCurrentPressure(void);
float MQTT_GetSetPressure(void);
bool MQTT_GetPumpPressureMode(void);
float MQTT_GetBrewSetpoint(void);
float MQTT_GetSteamSetpoint(void);
float MQTT_GetPumpPower(void);
float MQTT_GetShotTime(void);
float MQTT_GetShotVolume(void);
uint32_t MQTT_GetZcCount(void);
bool MQTT_GetHeaterState(void);
void MQTT_SetHeaterState(bool state);
void MQTT_SetBrewSetpoint(float temp_c);
void MQTT_SetSteamSetpoint(float temp_c);
void MQTT_SetPressureSetpoint(float pressure_bar);
void MQTT_SetPumpPower(float percent);
void MQTT_SetPumpPressureMode(bool enabled);
bool MQTT_GetSteamState(void);
void MQTT_SetSteamState(bool state);

void Wireless_SetStandbyMode(bool standby);
TickType_t Wireless_GetLastControlChangeTick(void);

bool Wireless_UsingEspNow(void);
bool Wireless_IsMQTTConnected(void);
bool Wireless_IsWiFiConnected(void);
bool Wireless_ControllerStillSendingEspNow(void);
bool Wireless_IsEspNowActive(void);
