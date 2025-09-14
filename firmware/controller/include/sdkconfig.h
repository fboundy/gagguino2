#pragma once
#include_next "sdkconfig.h"
#undef CONFIG_FREERTOS_TIMER_TASK_STACK_DEPTH
// Increase Timer Service task stack to handle heavier callbacks/logging.
// Note: Value is in words (not bytes) on ESP32 FreeRTOS.
#define CONFIG_FREERTOS_TIMER_TASK_STACK_DEPTH 8192
