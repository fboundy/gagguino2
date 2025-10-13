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
    ${DEMO_MAIN_DIR}/LVGL_UI/LVGL_UI.c
    ${DEMO_MAIN_DIR}/Wireless/Wireless.c
    ${DEMO_MAIN_DIR}/Buzzer/Buzzer.c
    ${DEMO_MAIN_DIR}/fonts/mdi_icons_24.c
    ${DEMO_MAIN_DIR}/fonts/mdi_icons_40.c
    ${DEMO_MAIN_DIR}/fonts/mdi_icons_80.c
    ${DEMO_MAIN_DIR}/Battery/Battery.c
    ${DEMO_MAIN_DIR}/WebServer/WebServer.c
    ${DEMO_MAIN_DIR}/WebServer/BrewProfileStore.c
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
        ${DEMO_MAIN_DIR}/Battery
        ${DEMO_MAIN_DIR}/fonts
        ${DEMO_MAIN_DIR}/WebServer
        ${CMAKE_SOURCE_DIR}/../shared/include
    REQUIRES
        lvgl__lvgl
        bt
        esp_wifi
        esp_netif
        esp_event
        nvs_flash
        mdns
        driver
        esp_lcd
        mqtt
        esp_http_server
        json
)
