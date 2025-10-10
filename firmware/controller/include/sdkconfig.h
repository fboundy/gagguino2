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

// The IDF "main" task hosts Arduino's app_main bootstrap.  Its default stack
// depth is tuned for lightweight sketches and can overflow once we start
// pulling in heavier subsystems (Wi-Fi, ESP-NOW, PID control, etc.).  Bump the
// stack allowance there as well so the core can create the Arduino loop task
// safely before handing execution to our code.
#undef CONFIG_ESP_MAIN_TASK_STACK_SIZE
#define CONFIG_ESP_MAIN_TASK_STACK_SIZE 12288

// Older IDF releases still key off CONFIG_MAIN_TASK_STACK_SIZE, so mirror the
// larger value to cover both variants.
#undef CONFIG_MAIN_TASK_STACK_SIZE
#define CONFIG_MAIN_TASK_STACK_SIZE 12288
