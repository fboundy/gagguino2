#pragma once
#include_next "sdkconfig.h"
#undef CONFIG_FREERTOS_TIMER_TASK_STACK_DEPTH
// Increase Timer Service task stack to handle heavier callbacks/logging.
// Note: Value is in words (not bytes) on ESP32 FreeRTOS.
#define CONFIG_FREERTOS_TIMER_TASK_STACK_DEPTH 8192

#undef CONFIG_ARDUINO_LOOP_STACK_SIZE
// Give the main Arduino task additional stack headroom to avoid overflow when
// running the full control loop with Wi-Fi/ESP-NOW handling.
#define CONFIG_ARDUINO_LOOP_STACK_SIZE 16384
