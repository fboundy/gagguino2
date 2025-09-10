// secrets.h
#pragma once

/* ===== Device ===== */
#define GAGGIA_ID "D94F94"

/* ===== Wi-Fi ===== */
#define WIFI_SSID "Dlink12"
#define WIFI_PASSWORD "4d9a4d4652"
#define WIFI_PASS WIFI_PASSWORD /* alias */

/* ===== ESP-NOW ===== */
/* Set the MAC addresses for both devices and the channel */
#define CONTROLLER_MAC {0x00,0x00,0x00,0x00,0x00,0x00}
#define DISPLAY_MAC    {0x00,0x00,0x00,0x00,0x00,0x00}
#define ESPNOW_CHANNEL 6
