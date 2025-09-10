#pragma once

#include <stdbool.h>

void Wireless_Init(void);
void Wireless_Poll(void);
bool Wireless_UsingEspNow(void);

float Wireless_GetCurrentTemp(void);
float Wireless_GetSetTemp(void);
float Wireless_GetCurrentPressure(void);
float Wireless_GetShotTime(void);
float Wireless_GetShotVolume(void);
bool Wireless_GetHeaterState(void);
bool Wireless_GetSteamState(void);
