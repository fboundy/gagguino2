/**
 * @file main.c
 * @brief Entry point for hardware initialization and LVGL demonstration.
 */

#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
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
// touched. last_zc_change_tick tracks the last observed zero-cross count
// change.
volatile TickType_t g_last_touch_tick = 0;
static TickType_t last_zc_change_tick = 0;
static uint32_t last_zc_count = 0;
static bool standby_mode = false;

#define LCD_INACTIVITY_TIMEOUT  pdMS_TO_TICKS(600000)  /* 10 min inactivity window */
#define LCD_STANDBY_BACKLIGHT_LEVEL 5

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

    bool wdt_registered = false;
    esp_err_t wdt_err = esp_task_wdt_add(NULL);
    if (wdt_err == ESP_OK) {
        wdt_registered = true;
        ESP_LOGI("BOOT", "Registered app_main with task watchdog");
    } else if (wdt_err == ESP_ERR_INVALID_STATE) {
        ESP_LOGW("BOOT", "Task watchdog not initialized; skipping manual feed");
    } else {
        ESP_LOGW("BOOT", "Failed to register app_main with task watchdog: %s", esp_err_to_name(wdt_err));
    }

    LCD_Init();      // Prepare LCD display
    Touch_Init();    // Initialize touch controller
    SD_Init();       // Mount SD card
    LVGL_Init();     // Initialize graphics library
/********************* Demo *********************/
    Lvgl_Example1();

    // Alternative demos:
    // lv_demo_widgets();
    // lv_demo_keypad_encoder();
    // lv_demo_benchmark();
    // lv_demo_stress();
    // lv_demo_music();

    // Initialize timers
    g_last_touch_tick = xTaskGetTickCount();

    TickType_t start_tick = g_last_touch_tick;
    last_zc_change_tick = start_tick;
    last_zc_count = MQTT_GetZcCount();

    const TickType_t loop_delay = pdMS_TO_TICKS(50);
    const TickType_t ui_update_period = pdMS_TO_TICKS(100);
    TickType_t last_ui_update = start_tick;

    while (1) {
        TickType_t tick_before = xTaskGetTickCount();

        if ((tick_before - last_ui_update) >= ui_update_period) {
            LVGL_UI_Update();
            last_ui_update = tick_before;
        }

        lv_timer_handler();

        TickType_t now = xTaskGetTickCount();
        uint32_t current_zc = MQTT_GetZcCount();
        bool zc_changed = (current_zc != last_zc_count);
        if (zc_changed) {
            last_zc_count = current_zc;
            last_zc_change_tick = now;
        }

        TickType_t control_tick = Wireless_GetLastControlChangeTick();
        TickType_t last_activity = g_last_touch_tick;
        if (last_zc_change_tick > last_activity) {
            last_activity = last_zc_change_tick;
        }
        if (control_tick > last_activity) {
            last_activity = control_tick;
        }

        bool inactive = (now - last_activity) >= LCD_INACTIVITY_TIMEOUT;

        if (!standby_mode) {
            if (inactive) {
                LVGL_EnterStandby();
                Wireless_SetStandbyMode(true);
                Set_Backlight(LCD_STANDBY_BACKLIGHT_LEVEL);
                standby_mode = true;
            }
        } else {
            if (!inactive) {
                LVGL_ExitStandby();
                Wireless_SetStandbyMode(false);
                Set_Backlight(LCD_Backlight);
                standby_mode = false;
            }
        }

        if (wdt_registered) {
            esp_task_wdt_reset();
        }

        /*
         * Yield to the scheduler so the idle task can run and feed the
         * watchdog.  Without this delay the loop becomes a tight spin that
         * prevents IDLE0 from executing, eventually triggering the task WDT
         * on the display build.  The fixed branch included an explicit delay
         * here; reintroduce it so we maintain a cooperative update cadence.
         */
        vTaskDelay(loop_delay);
    }
}
