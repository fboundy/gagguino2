// secrets.h
#pragma once

#include "mqtt_topics.h"

/* ===== Device ===== */
#define GAGGIA_ID "D94F94"

/* ===== Wi-Fi ===== */
#define WIFI_SSID "Dlink12"
#define WIFI_PASSWORD "4d9a4d4652"
#define WIFI_PASS WIFI_PASSWORD /* alias */

/* ===== MQTT ===== */
#define MQTT_HOST "homeassistant.local"
#define MQTT_PORT 1883

/* auth */
#define MQTT_USERNAME "mqtt-user"
#define MQTT_PASSWORD "0pl,mko9"
#define MQTT_USER MQTT_USERNAME /* aliases for legacy names */
#define MQTT_PASS MQTT_PASSWORD

/* client id */
#define MQTT_CLIENT_ID "gaggia-device"
#define MQTT_CLIENTID MQTT_CLIENT_ID /* alias */

/* ===== Topics ===== */
#define MQTT_STATUS GAG_TOPIC_ROOT "/" GAGGIA_ID "/status"
#define MQTT_ERRORS GAG_TOPIC_ROOT "/" GAGGIA_ID "/error"
