# Import demo sources copied into src/

set(DEMO_MAIN_DIR ${CMAKE_SOURCE_DIR}/src)

set(app_sources
    ${DEMO_MAIN_DIR}/main.c
    ${DEMO_MAIN_DIR}/EXIO/TCA9554PWR.c
    ${DEMO_MAIN_DIR}/LCD_Driver/ST7701S.c
    ${DEMO_MAIN_DIR}/Touch_Driver/CST820.c
    ${DEMO_MAIN_DIR}/Touch_Driver/esp_lcd_touch/esp_lcd_touch.c
    ${DEMO_MAIN_DIR}/LVGL_Driver/LVGL_Driver.c
    ${DEMO_MAIN_DIR}/I2C_Driver/I2C_Driver.c
    ${DEMO_MAIN_DIR}/SD_Card/SD_MMC.c
    ${DEMO_MAIN_DIR}/LVGL_UI/LVGL_Example.c
    ${DEMO_MAIN_DIR}/Wireless/Wireless.c
    ${DEMO_MAIN_DIR}/Buzzer/Buzzer.c
)

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS
        .
        ${DEMO_MAIN_DIR}
        ${DEMO_MAIN_DIR}/EXIO
        ${DEMO_MAIN_DIR}/LCD_Driver
        ${DEMO_MAIN_DIR}/Touch_Driver
        ${DEMO_MAIN_DIR}/Touch_Driver/esp_lcd_touch
        ${DEMO_MAIN_DIR}/LVGL_Driver
        ${DEMO_MAIN_DIR}/I2C_Driver
        ${DEMO_MAIN_DIR}/SD_Card
        ${DEMO_MAIN_DIR}/LVGL_UI
        ${DEMO_MAIN_DIR}/Wireless
        ${DEMO_MAIN_DIR}/Buzzer
    REQUIRES
        lvgl__lvgl
        bt
        esp_wifi
        esp_netif
        esp_event
        nvs_flash
        driver
        esp_lcd
        mqtt
)
