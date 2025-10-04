/**
 * @file main.c
 * @brief Entry point for hardware initialization and LVGL demonstration.
 */

#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "esp_rom_sys.h"

static int log_vprintf(const char *fmt, va_list args)
{
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, args);
    char *msg = strstr(buf, ") ");
    if (msg)
        msg += 2; // skip ") "
    else
        msg = buf;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm tm;
    localtime_r(&tv.tv_sec, &tm);
    char tbuf[32];
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", &tm);
    return esp_rom_printf("[%s.%03ld] %s", tbuf, tv.tv_usec / 1000, msg);
}
#include "TCA9554PWR.h"
#include "ST7701S.h"
#include "CST820.h"
#include "SD_MMC.h"
#include "LVGL_Driver.h"
#include "LVGL_UI.h"
#include "Wireless.h"
#include "Battery.h"

// Track interaction and machine activity for LCD backlight control.
// g_last_touch_tick is updated by the touch driver whenever the screen is
// touched.  last_heater_on_tick records when the heater was most recently on,
// and last_zc_change_tick tracks the last observed zero-cross count change.
volatile TickType_t g_last_touch_tick = 0;
static TickType_t last_heater_on_tick = 0;
static TickType_t last_zc_change_tick = 0;
static uint32_t last_zc_count = 0;
#define LCD_STANDBY_BACKLIGHT 5

#define LCD_INACTIVITY_TIMEOUT  pdMS_TO_TICKS(300000)  /* 5 min inactivity window */

/**
 * @brief Initialize peripheral drivers and start background tasks.
 */
void Driver_Init(void)
{
    Flash_Searching();   // Detect storage devices
    I2C_Init();          // Initialize I2C bus for sensors
    EXIO_Init();         // Example: initialize external IO expander
    Battery_Init();      // Setup battery monitoring
}

/**
 * @brief Application entry point initializing subsystems and launching LVGL demo.
 */
void app_main(void)
{
    // Install custom logger to prepend RTC timestamp
    esp_log_set_vprintf(log_vprintf);

    // Give host time to open the serial port (USB/UART) before logs start
    const int boot_delay_ms = 1500; // adjust if needed
    ESP_LOGI("BOOT", "Delaying %d ms to let serial start", boot_delay_ms);
    vTaskDelay(pdMS_TO_TICKS(boot_delay_ms));

    Wireless_Init();  // Configure Wi-Fi/BLE modules
    Driver_Init();    // Initialize hardware drivers

    LCD_Init();      // Prepare LCD display
    Touch_Init();    // Initialize touch controller
    SD_Init();       // Mount SD card
    LVGL_Init();     // Initialize graphics library
/********************* Demo *********************/
    Lvgl_Example1();
    LVGL_UI_PollTelemetry();

    // Alternative demos:
    // lv_demo_widgets();
    // lv_demo_keypad_encoder();
    // lv_demo_benchmark();
    // lv_demo_stress();
    // lv_demo_music();

    // Initialize timers
    g_last_touch_tick = xTaskGetTickCount();

    TickType_t start_tick = g_last_touch_tick;
    last_heater_on_tick = start_tick;
    last_zc_change_tick = start_tick;
    last_zc_count = MQTT_GetZcCount();

    while (1) {
        // Run lv_timer_handler every 250 ms
        vTaskDelay(pdMS_TO_TICKS(250));
        LVGL_UI_PollTelemetry();
        lv_timer_handler();

        TickType_t now = xTaskGetTickCount();
        bool heater_on = MQTT_GetHeaterState();
        if (heater_on) {
            last_heater_on_tick = now;
        }

        uint32_t current_zc = MQTT_GetZcCount();
        bool zc_changed = (current_zc != last_zc_count);
        if (zc_changed) {
            last_zc_count = current_zc;
            last_zc_change_tick = now;
        }

        bool touch_inactive = (now - g_last_touch_tick) >= LCD_INACTIVITY_TIMEOUT;
        bool heater_inactive = !heater_on && (now - last_heater_on_tick) >= LCD_INACTIVITY_TIMEOUT;
        bool zc_inactive = (now - last_zc_change_tick) >= LCD_INACTIVITY_TIMEOUT;

        if (!LVGL_Is_Standby_Active()) {
            if (touch_inactive && (heater_inactive || zc_inactive)) {
                LVGL_Show_Standby();
                Set_Backlight(LCD_STANDBY_BACKLIGHT);
            }
        } else {
            bool touch_recent = !touch_inactive;
            if (heater_on || zc_changed || touch_recent) {
                LVGL_Exit_Standby();
                Set_Backlight(LCD_Backlight);
            }
        }
    }
}
