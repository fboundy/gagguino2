// secrets.h
#pragma once

#include "mqtt_topics.h"

/* ===== Device ===== */
#define GAGGIA_ID "D94F94"

/* ===== Wi-Fi ===== */
#define WIFI_SSID "Dlink12"
#define WIFI_PASSWORD "********"

/* ===== MQTT ===== */
#define MQTT_HOST "homeassistant.local"
#define MQTT_PORT 1883

/* auth */
#define MQTT_USER "mqtt-user"
#define MQTT_PASSWORD "********"

/* client ids */
#define MQTT_CONTROLLER_CLIENT_ID "gaggia-controller"
#define MQTT_DISPLAY_CLIENT_ID "gaggia-display"

/* ===== Topics ===== */
#define MQTT_STATUS GAG_TOPIC_ROOT "/" GAGGIA_ID "/status"
#define MQTT_ERRORS GAG_TOPIC_ROOT "/" GAGGIA_ID "/error"
