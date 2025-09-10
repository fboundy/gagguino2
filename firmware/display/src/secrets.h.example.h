// Replace with your WiFi + MQTT credentials. This file is .gitignored.
#pragma once

#define WIFI_SSID "SSID"
#define WIFI_PASS "********"

// MQTT client (subscriber + publisher)
// Examples:
//   mqtt://192.168.1.10:1883
//   mqtts://broker.example.com:8883  (TLS requires proper cert config)
#define MQTT_URI      "mqtt://192.168.1.10:1883"

// Optional auth; leave as empty strings if your broker allows anonymous
#define MQTT_USERNAME ""
#define MQTT_PASSWORD ""

// Optional client ID; leave empty to let esp-mqtt generate one
#define MQTT_CLIENT_ID "gaggia-display"

// Default topics to subscribe/publish (optional)
#define MQTT_SUB_TOPIC "gaggia/commands/#"
#define MQTT_LWT_TOPIC "gaggia/status"
