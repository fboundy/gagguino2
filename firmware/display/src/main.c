/**
 * @file main.c
 * @brief Entry point for hardware initialization and LVGL demonstration.
 */

#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "TCA9554PWR.h"
#include "PCF85063.h"
#include "QMI8658.h"
#include "ST7701S.h"
#include "CST820.h"
#include "SD_MMC.h"
#include "LVGL_Driver.h"
#include "LVGL_Example.h"
#include "Wireless.h"

/**
 * @brief Driver task loop running sensor polling and RTC updates.
 *
 * @param parameter Unused task parameter required by FreeRTOS.
 */
void Driver_Loop(void *parameter)
{
    while(1)
    {
        QMI8658_Loop();   // Accelerometer/Gyroscope polling
        RTC_Loop();       // Update real-time clock
        BAT_Get_Volts();  // Read battery voltage
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelete(NULL);
}

/**
 * @brief Initialize peripheral drivers and start background tasks.
 */
void Driver_Init(void)
{
    Flash_Searching();   // Detect storage devices
    BAT_Init();          // Configure battery monitoring
    I2C_Init();          // Initialize I2C bus for sensors
    PCF85063_Init();     // Set up real-time clock
    QMI8658_Init();      // Initialize IMU sensor
    EXIO_Init();         // Example: initialize external IO expander
    xTaskCreatePinnedToCore(
        Driver_Loop,
        "Other Driver task",
        4096,
        NULL,
        3,
        NULL,
        0);
}

/**
 * @brief Application entry point initializing subsystems and launching LVGL demo.
 */
void app_main(void)
{
    Wireless_Init();  // Configure Wi-Fi/BLE modules
    Driver_Init();    // Initialize hardware drivers

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

    while (1) {
        // Raise task priority or reduce handler period to improve performance
        // Run lv_timer_handler every 250 ms
        vTaskDelay(pdMS_TO_TICKS(250));
        // Task running lv_timer_handler should have lower priority than that running `lv_tick_inc`
        lv_timer_handler();
    }
}
