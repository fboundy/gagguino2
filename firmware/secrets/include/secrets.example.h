// secrets.h
#pragma once

/* ===== Device ===== */
#define GAGGIA_ID "D94F94"

/* ===== Wi-Fi ===== */
#define WIFI_SSID "Dlink12"
#define WIFI_PASSWORD "4d9a4d4652"
#define WIFI_PASS WIFI_PASSWORD /* alias */

/* ===== MQTT (host/port + URI kept in sync) ===== */
#define MQTT_HOST "homeassistant.local"
#define MQTT_PORT 1883

/* helpers for URI building (avoid clash with ctype.h macros) */
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define MQTT_URI "mqtt://" MQTT_HOST ":" STR(MQTT_PORT)

/* auth */
#define MQTT_USERNAME "mqtt-user"
#define MQTT_PASSWORD_MQTT "0pl,mko9" /* avoid name clash with WIFI_PASSWORD */
#define MQTT_USER MQTT_USERNAME       /* aliases for legacy names */
#define MQTT_PASS MQTT_PASSWORD_MQTT

/* client id */
#define MQTT_CLIENT_ID "gaggia-display"
#define MQTT_CLIENTID MQTT_CLIENT_ID /* alias */

/* ===== Topics ===== */
#define MQTT_TOPIC "homeassistant/espresso/telemetry"
#define MQTT_STATUS "homeassistant/espresso/status"
#define MQTT_ERRORS "homeassistant/espresso/error"
