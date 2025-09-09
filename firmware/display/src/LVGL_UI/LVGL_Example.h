#pragma once

#include "lvgl.h"
#include "demos/lv_demos.h"

#include "LVGL_Driver.h"
#include "TCA9554PWR.h"
#include "Wireless.h"
#include "Buzzer.h"
#include "ST7701S.h"

#define EXAMPLE1_LVGL_TICK_PERIOD_MS 1000
#define TEMP_ARC_START 120
#define TEMP_ARC_SIZE 120
#define TEMP_ARC_MIN 60
#define TEMP_ARC_MAX 160
#define TEMP_ARC_TICK 10

void Backlight_adjustment_event_cb(lv_event_t *e);

void Lvgl_Example1(void);
void LVGL_Backlight_adjustment(uint8_t Backlight);
