#include "LVGL_UI.h"
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "esp_system.h"
#include "version.h"
#include "Battery.h"

#define STANDBY_CLOCK_FONT_SIZE_PX 80  // Standby clock target text size (px)

/* Fallback symbol definitions for environments where newer LVGL symbols are
 * not provided. These values correspond to Font Awesome code points and allow
 * the project to compile even with older LVGL releases. */
#ifndef LV_SYMBOL_TEMPERATURE
#define LV_SYMBOL_TEMPERATURE "\xEF\x8B\x89"
#endif

#ifndef LV_SYMBOL_SPEED
#define LV_SYMBOL_SPEED "\xEF\x8F\xBD"
#endif

#ifndef LV_SYMBOL_TIMER
#define LV_SYMBOL_TIMER "\xEF\x80\x97"
#endif

#ifndef LV_SYMBOL_BRIGHTNESS
#define LV_SYMBOL_BRIGHTNESS "\xEF\x86\x85"
#endif

/**********************
 *      TYPEDEFS
 **********************/
typedef enum
{
  DISP_SMALL,
  DISP_MEDIUM,
  DISP_LARGE,
} disp_size_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void Status_create(lv_obj_t *parent);
static void Settings_create(void);
static void Menu_create(void);
static lv_obj_t *create_placeholder_screen(const char *title);
static void open_settings_event_cb(lv_event_t *e);
static void open_screen_event_cb(lv_event_t *e);
static void open_menu_event_cb(lv_event_t *e);
static void draw_ticks_cb(lv_event_t *e);
static lv_obj_t *create_comm_status_row(lv_obj_t *parent, lv_coord_t y_offset);
static lv_obj_t *create_menu_button(lv_obj_t *grid, uint8_t col, uint8_t row,
                                    const char *icon, const char *label);
static void add_version_label(lv_obj_t *parent);
// static void shot_def_dd_event_cb(lv_event_t *e);
// static void beep_on_shot_btn_event_cb(lv_event_t *e);
static void buzzer_timer_cb(lv_timer_t *t);
static int roller_get_int_value(lv_obj_t *roller);
static void load_screen(lv_obj_t *screen);
static void update_standby_time(void);
static void standby_timer_cb(lv_timer_t *t);
static bool uk_is_bst_active(const struct tm *utc_tm);
static bool is_leap_year(int year);
static int days_in_month(int year, int month);
static int day_of_week(int year, int month, int day);
static int last_sunday_of_month(int year, int month);
static void heater_switch_event_cb(lv_event_t *e);
static void update_heater_controls(bool heater_state);
static void pump_pressure_switch_event_cb(lv_event_t *e);
static void update_pump_pressure_controls(bool enabled);
static void pump_setting_roller_event_cb(lv_event_t *e);
static void refresh_pump_setting_ui(void);

void example1_increase_lvgl_tick(lv_timer_t *t);
/**********************
 *  STATIC VARIABLES
 **********************/
static disp_size_t disp_size;

lv_style_t style_text_muted;
lv_style_t style_title;
static lv_style_t style_icon;
static lv_style_t style_bullet;

static const lv_font_t *font_large;
static const lv_font_t *font_normal;

static lv_timer_t *auto_step_timer;
// static lv_color_t original_screen_bg_color;

static lv_timer_t *meter2_timer;

static lv_obj_t *menu_screen;
static lv_obj_t *brew_screen;
static lv_obj_t *settings_scr;
static lv_obj_t *steam_screen;
static lv_obj_t *profiles_screen;
static lv_obj_t *standby_screen;
static lv_coord_t tab_h_global;

static lv_obj_t *current_screen;
static lv_obj_t *last_active_screen;
static bool standby_active;
static lv_timer_t *standby_timer;
static lv_obj_t *standby_time_label;

static lv_obj_t *current_temp_arc;
static lv_obj_t *set_temp_arc;
static lv_obj_t *current_pressure_arc;
static lv_obj_t *tick_layer;
static lv_obj_t *temp_label;
static lv_obj_t *pressure_label;
static lv_obj_t *temp_icon;
static lv_obj_t *pressure_icon;
static lv_obj_t *shot_time_label;
static lv_obj_t *shot_volume_label;
static lv_obj_t *shot_time_icon;
static lv_obj_t *shot_volume_icon;
static lv_obj_t *temp_units_label;
static lv_obj_t *pressure_units_label;
static lv_obj_t *shot_time_units_label;
static lv_obj_t *shot_volume_units_label;
lv_obj_t *Backlight_slider;
static bool s_syncing_backlight = false; /* guard to avoid event from programmatic update */
static lv_obj_t *beep_on_shot_btn;
static lv_obj_t *beep_on_shot_label;
static lv_obj_t *shot_def_dd;
static lv_obj_t *shot_duration_label;
static lv_obj_t *shot_duration_roller;
static lv_obj_t *shot_volume_label;
static lv_obj_t *shot_volume_roller;
static lv_obj_t *comm_status_container;
typedef struct
{
  lv_obj_t *screen;
  lv_obj_t *wifi;
  lv_obj_t *mqtt;
  lv_obj_t *espnow;
} comm_status_set_t;

#define COMM_STATUS_MAX_SETS 4
static comm_status_set_t comm_status_sets[COMM_STATUS_MAX_SETS];
static size_t comm_status_set_count;
static lv_obj_t *battery_bar;
static lv_obj_t *battery_label;
static int last_wifi_state = -1;
static int last_mqtt_state = -1;
static int last_esp_state = -1;
static int last_battery = -1;
static lv_timer_t *buzzer_timer;
static bool shot_target_reached;
static float set_temp_val;
static bool heater_on;
static lv_obj_t *heater_switch;
static lv_obj_t *heater_status_label;
static bool s_syncing_heater_switch;
static bool pump_pressure_mode_on;
static lv_obj_t *pump_pressure_switch;
static lv_obj_t *pump_pressure_status_label;
static lv_obj_t *pump_setting_label;
static lv_obj_t *pump_setting_roller;
static bool s_syncing_pump_pressure_switch;
static bool s_syncing_pump_setting_roller;
static bool s_pump_setting_options_set;
static bool s_pump_setting_options_pressure_mode;

void Lvgl_Example1(void)
{

  if (LV_HOR_RES <= 320)
    disp_size = DISP_SMALL;
  else if (LV_HOR_RES < 720)
    disp_size = DISP_MEDIUM;
  else
    disp_size = DISP_LARGE;
  font_large = LV_FONT_DEFAULT;
  font_normal = LV_FONT_DEFAULT;

  if (disp_size == DISP_LARGE)
  {
#if LV_FONT_MONTSERRAT_40
    font_large = &lv_font_montserrat_40;
#elif LV_FONT_MONTSERRAT_24
    font_large = &lv_font_montserrat_24;
#else
    LV_LOG_WARN("LV_FONT_MONTSERRAT_40 and LV_FONT_MONTSERRAT_24 are not "
                "enabled for the widgets demo. "
                "Using LV_FONT_DEFAULT instead.");
#endif
#if LV_FONT_MONTSERRAT_16
    font_normal = &lv_font_montserrat_16;
#else
    LV_LOG_WARN("LV_FONT_MONTSERRAT_16 is not enabled for the widgets demo. "
                "Using LV_FONT_DEFAULT instead.");
#endif
  }
  else if (disp_size == DISP_MEDIUM)
  {
#if LV_FONT_MONTSERRAT_20
    font_large = &lv_font_montserrat_20;
#else
    LV_LOG_WARN("LV_FONT_MONTSERRAT_20 is not enabled for the widgets demo. "
                "Using LV_FONT_DEFAULT instead.");
#endif
#if LV_FONT_MONTSERRAT_14
    font_normal = &lv_font_montserrat_14;
#else
    LV_LOG_WARN("LV_FONT_MONTSERRAT_14 is not enabled for the widgets demo. "
                "Using LV_FONT_DEFAULT instead.");
#endif
  }
  else
  { /* disp_size == DISP_SMALL */
#if LV_FONT_MONTSERRAT_18
    font_large = &lv_font_montserrat_18;
#else
    LV_LOG_WARN("LV_FONT_MONTSERRAT_18 is not enabled for the widgets demo. "
                "Using LV_FONT_DEFAULT instead.");
#endif
#if LV_FONT_MONTSERRAT_12
    font_normal = &lv_font_montserrat_12;
#else
    LV_LOG_WARN("LV_FONT_MONTSERRAT_12 is not enabled for the widgets demo. "
                "Using LV_FONT_DEFAULT instead.");
#endif
  }
  tab_h_global = 0;

  // 设置字体

  lv_style_init(&style_text_muted);
  lv_style_set_text_opa(&style_text_muted, LV_OPA_90);
  lv_style_set_text_font(&style_text_muted, &lv_font_montserrat_20);

  lv_style_init(&style_title);
  lv_style_set_text_font(&style_title, &lv_font_montserrat_28);

  lv_style_init(&style_icon);
  lv_style_set_text_color(&style_icon, lv_theme_get_color_primary(NULL));
  lv_style_set_text_font(&style_icon, font_large);

  lv_style_init(&style_bullet);
  lv_style_set_border_width(&style_bullet, 0);
  lv_style_set_radius(&style_bullet, LV_RADIUS_CIRCLE);

  menu_screen = lv_scr_act();
  lv_obj_set_style_bg_color(menu_screen, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(menu_screen, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(menu_screen, lv_color_white(), 0);

  brew_screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(brew_screen, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(brew_screen, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(brew_screen, lv_color_white(), 0);

  settings_scr = NULL;
  steam_screen = NULL;
  profiles_screen = NULL;
  standby_screen = NULL;
  Backlight_slider = NULL;
  heater_switch = NULL;
  heater_status_label = NULL;
  s_syncing_heater_switch = false;
  pump_pressure_switch = NULL;
  pump_pressure_status_label = NULL;
  pump_setting_label = NULL;
  pump_setting_roller = NULL;
  s_syncing_pump_pressure_switch = false;
  s_syncing_pump_setting_roller = false;
  s_pump_setting_options_set = false;
  s_pump_setting_options_pressure_mode = false;
  pump_pressure_mode_on = false;
  standby_timer = NULL;
  standby_time_label = NULL;
  standby_active = false;
  current_screen = NULL;
  last_active_screen = NULL;
  comm_status_set_count = 0;
  memset(comm_status_sets, 0, sizeof(comm_status_sets));

  lv_obj_set_style_text_font(lv_scr_act(), font_normal, 0);

  if (disp_size == DISP_LARGE)
  {
    /* Large displays do not require additional header content. */
  }

  Status_create(brew_screen);
  Settings_create();
  steam_screen = create_placeholder_screen("Steam");
  profiles_screen = create_placeholder_screen("Profiles");
  Menu_create();
  load_screen(menu_screen);
}

static void open_settings_event_cb(lv_event_t *e)
{
  if (!settings_scr)
    Settings_create();
  load_screen(settings_scr);
}

static void Settings_create(void)
{
  if (settings_scr)
    return;

  settings_scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(settings_scr, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(settings_scr, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(settings_scr, 0, 0);
  lv_obj_set_style_text_color(settings_scr, lv_color_white(), 0);
  lv_obj_set_style_pad_all(settings_scr, 24, 0);
  lv_obj_set_style_pad_row(settings_scr, 24, 0);
  lv_obj_set_style_pad_bottom(settings_scr, 0, 0);
  lv_obj_set_flex_flow(settings_scr, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(settings_scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_START);

  lv_obj_t *title_label = lv_label_create(settings_scr);
  lv_label_set_text(title_label, "Settings");
  lv_obj_add_style(title_label, &style_title, 0);
  lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
  lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(title_label, LV_PCT(100));

  static lv_coord_t control_cols[] = {LV_GRID_FR(1), LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
  static lv_coord_t control_rows[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT,
                                      LV_GRID_TEMPLATE_LAST};

  lv_obj_t *control_grid = lv_obj_create(settings_scr);
  lv_obj_remove_style_all(control_grid);
  lv_obj_set_style_bg_opa(control_grid, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(control_grid, 0, 0);
  lv_obj_set_style_pad_all(control_grid, 0, 0);
  lv_obj_set_style_pad_row(control_grid, 16, 0);
  lv_obj_set_style_pad_column(control_grid, 16, 0);
  lv_obj_set_width(control_grid, LV_PCT(80));
  lv_obj_set_layout(control_grid, LV_LAYOUT_GRID);
  lv_obj_set_grid_dsc_array(control_grid, control_cols, control_rows);
  lv_obj_set_flex_grow(control_grid, 1);

  lv_obj_t *heater_label = lv_label_create(control_grid);
  lv_label_set_text(heater_label, "Heater");
  lv_obj_set_style_text_color(heater_label, lv_color_white(), 0);
  lv_obj_set_style_text_align(heater_label, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_grid_cell(heater_label, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 0,
                       1);
#if LV_FONT_MONTSERRAT_24
  lv_obj_set_style_text_font(heater_label, &lv_font_montserrat_24, 0);
#else
  lv_obj_set_style_text_font(heater_label, font_large, 0);
#endif

  heater_switch = lv_switch_create(control_grid);
  lv_obj_set_size(heater_switch, 130, 50);
  lv_obj_add_event_cb(heater_switch, heater_switch_event_cb, LV_EVENT_VALUE_CHANGED,
                      NULL);
  lv_obj_set_grid_cell(heater_switch, LV_GRID_ALIGN_END, 1, 1, LV_GRID_ALIGN_CENTER, 0,
                       1);

  // heater_status_label = lv_label_create(heater_card);
  // lv_obj_add_style(heater_status_label, &style_text_muted, 0);
  // lv_obj_set_style_text_align(heater_status_label, LV_TEXT_ALIGN_LEFT, 0);
  // lv_obj_set_style_text_color(heater_status_label, lv_color_white(), 0);
  // lv_obj_set_width(heater_status_label, LV_PCT(100));

  update_heater_controls(MQTT_GetHeaterState());

  lv_obj_t *pump_pressure_label = lv_label_create(control_grid);
  lv_label_set_text(pump_pressure_label, "Pump pressure control");
  lv_label_set_long_mode(pump_pressure_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_color(pump_pressure_label, lv_color_white(), 0);
  lv_obj_set_style_text_align(pump_pressure_label, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_grid_cell(pump_pressure_label, LV_GRID_ALIGN_STRETCH, 0, 1,
                       LV_GRID_ALIGN_CENTER, 1, 1);
#if LV_FONT_MONTSERRAT_24
  lv_obj_set_style_text_font(pump_pressure_label, &lv_font_montserrat_24, 0);
#else
  lv_obj_set_style_text_font(pump_pressure_label, font_large, 0);
#endif

  pump_pressure_switch = lv_switch_create(control_grid);
  lv_obj_set_size(pump_pressure_switch, 130, 50);
  lv_obj_add_event_cb(pump_pressure_switch, pump_pressure_switch_event_cb,
                      LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_set_grid_cell(pump_pressure_switch, LV_GRID_ALIGN_END, 1, 1, LV_GRID_ALIGN_CENTER,
                       1, 1);

  pump_setting_label = lv_label_create(control_grid);
  lv_label_set_text(pump_setting_label, "Pressure setpoint");
  lv_obj_set_style_text_color(pump_setting_label, lv_color_white(), 0);
  lv_obj_set_style_text_align(pump_setting_label, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_grid_cell(pump_setting_label, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER,
                       2, 1);
#if LV_FONT_MONTSERRAT_24
  lv_obj_set_style_text_font(pump_setting_label, &lv_font_montserrat_24, 0);
#else
  lv_obj_set_style_text_font(pump_setting_label, font_large, 0);
#endif

  pump_setting_roller = lv_roller_create(control_grid);
  lv_obj_set_size(pump_setting_roller, 160, 130);
  lv_obj_set_grid_cell(pump_setting_roller, LV_GRID_ALIGN_END, 1, 1, LV_GRID_ALIGN_CENTER, 2,
                       1);
  lv_roller_set_visible_row_count(pump_setting_roller, 4);
  lv_obj_add_event_cb(pump_setting_roller, pump_setting_roller_event_cb,
                      LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_set_style_bg_color(pump_setting_roller, lv_color_hex(0x1e1e1e),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(pump_setting_roller, LV_OPA_COVER,
                          LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(pump_setting_roller, 0,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(pump_setting_roller, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(pump_setting_roller, lv_color_white(),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(pump_setting_roller, lv_color_white(),
                              LV_PART_SELECTED | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(pump_setting_roller, lv_palette_main(LV_PALETTE_BLUE),
                            LV_PART_SELECTED | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(pump_setting_roller, LV_OPA_50,
                          LV_PART_SELECTED | LV_STATE_DEFAULT);

  // pump_pressure_status_label = lv_label_create(heater_card);
  // lv_obj_add_style(pump_pressure_status_label, &style_text_muted, 0);
  // lv_obj_set_style_text_align(pump_pressure_status_label, LV_TEXT_ALIGN_LEFT, 0);
  // lv_obj_set_style_text_color(pump_pressure_status_label, lv_color_white(), 0);
  // lv_obj_set_width(pump_pressure_status_label, LV_PCT(100));

  update_pump_pressure_controls(MQTT_GetPumpPressureMode());

  lv_obj_t *footer = lv_obj_create(settings_scr);
  lv_obj_remove_style_all(footer);
  lv_obj_set_width(footer, LV_PCT(100));
  lv_obj_set_style_bg_color(footer, lv_color_hex(0x111111), 0);
  lv_obj_set_style_bg_opa(footer, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_top(footer, 10, 0);
  lv_obj_set_style_pad_bottom(footer, 10, 0);
  lv_obj_set_style_pad_left(footer, 10, 0);
  lv_obj_set_style_pad_right(footer, 10, 0);
  lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  lv_obj_t *back_btn = lv_btn_create(footer);
  lv_obj_set_size(back_btn, 160, 70);
  lv_obj_set_style_border_width(back_btn, 0, 0);
  lv_obj_set_style_bg_color(back_btn, lv_palette_main(LV_PALETTE_GREY), 0);
  lv_obj_add_event_cb(back_btn, open_menu_event_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *back_label = lv_label_create(back_btn);
  lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");
  lv_obj_center(back_label);
}

static lv_obj_t *create_placeholder_screen(const char *title)
{
  lv_obj_t *scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(scr, 0, 0);
  lv_obj_set_style_text_color(scr, lv_color_white(), 0);
  lv_obj_set_style_pad_all(scr, 24, 0);
  lv_obj_set_style_pad_row(scr, 24, 0);
  lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  lv_obj_t *title_label = lv_label_create(scr);
  lv_label_set_text(title_label, title);
  lv_obj_add_style(title_label, &style_title, 0);
  lv_obj_set_style_text_color(title_label, lv_color_white(), 0);

  lv_obj_t *placeholder_label = lv_label_create(scr);
  lv_label_set_text(placeholder_label, "Coming soon");
  lv_obj_set_style_text_color(placeholder_label, lv_color_white(), 0);

  lv_obj_t *back_btn = lv_btn_create(scr);
  lv_obj_set_size(back_btn, 160, 70);
  lv_obj_set_style_border_width(back_btn, 0, 0);
  lv_obj_set_style_bg_color(back_btn, lv_palette_main(LV_PALETTE_GREY), 0);
  lv_obj_add_event_cb(back_btn, open_menu_event_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *back_label = lv_label_create(back_btn);
  lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");
  lv_obj_center(back_label);

  return scr;
}

static void Menu_create(void)
{
  if (!menu_screen)
    return;

  lv_obj_clean(menu_screen);
  lv_obj_set_style_bg_color(menu_screen, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(menu_screen, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(menu_screen, lv_color_white(), 0);

  (void)create_comm_status_row(menu_screen, -45);

  lv_obj_t *title = lv_label_create(menu_screen);
  lv_label_set_text(title, "Gaggia Classic");
  lv_obj_add_style(title, &style_title, 0);
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

  lv_obj_t *button_grid = lv_obj_create(menu_screen);
  lv_obj_remove_style_all(button_grid);
  lv_obj_set_style_bg_opa(button_grid, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(button_grid, 0, 0);
  lv_obj_set_style_pad_row(button_grid, 24, 0);
  lv_obj_set_style_pad_column(button_grid, 24, 0);
  static lv_coord_t grid_cols[] = {LV_GRID_CONTENT, LV_GRID_CONTENT,
                                   LV_GRID_TEMPLATE_LAST};
  static lv_coord_t grid_rows[] = {LV_GRID_CONTENT, LV_GRID_CONTENT,
                                   LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(button_grid, grid_cols, grid_rows);
  lv_obj_set_size(button_grid, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_align(button_grid, LV_ALIGN_CENTER, 0, 0);

  lv_obj_t *brew_btn = create_menu_button(button_grid, 0, 0, MDI_COFFEE, "Brew");
  lv_obj_add_event_cb(brew_btn, open_screen_event_cb, LV_EVENT_CLICKED, brew_screen);

  lv_obj_t *steam_btn = create_menu_button(button_grid, 1, 0, MDI_STEAM, "Steam");
  lv_obj_add_event_cb(steam_btn, open_screen_event_cb, LV_EVENT_CLICKED, steam_screen);

  lv_obj_t *profiles_btn = create_menu_button(button_grid, 0, 1, MDI_MENU, "Profiles");
  lv_obj_add_event_cb(profiles_btn, open_screen_event_cb, LV_EVENT_CLICKED, profiles_screen);

  lv_obj_t *settings_btn = create_menu_button(button_grid, 1, 1, MDI_COG, "Settings");
  lv_obj_add_event_cb(settings_btn, open_settings_event_cb, LV_EVENT_CLICKED, NULL);

  add_version_label(menu_screen);
}

static void open_screen_event_cb(lv_event_t *e)
{
  lv_obj_t *target = lv_event_get_user_data(e);
  if (target)
    load_screen(target);
}

static void open_menu_event_cb(lv_event_t *e)
{
  (void)e;
  load_screen(menu_screen);
}

static void load_screen(lv_obj_t *screen)
{
  if (!screen)
    return;

  if (screen != standby_screen)
    last_active_screen = screen;

  current_screen = screen;
  lv_scr_load(screen);
}

static void update_standby_time(void)
{
  if (!standby_time_label)
    return;

  time_t now = time(NULL);
  struct tm utc_tm;
  gmtime_r(&now, &utc_tm);

  bool in_bst = uk_is_bst_active(&utc_tm);
  time_t uk_epoch = now + (in_bst ? 3600 : 0);

  struct tm uk_tm;
  gmtime_r(&uk_epoch, &uk_tm);

  char buf[16];
  strftime(buf, sizeof(buf), "%H:%M", &uk_tm);
  lv_label_set_text(standby_time_label, buf);
}

static void standby_timer_cb(lv_timer_t *t)
{
  (void)t;
  update_standby_time();
}

static bool uk_is_bst_active(const struct tm *utc_tm)
{
  if (!utc_tm)
    return false;

  int year = utc_tm->tm_year + 1900;
  int month = utc_tm->tm_mon + 1;
  int day = utc_tm->tm_mday;
  int hour = utc_tm->tm_hour;

  if (month < 3 || month > 10)
    return false;
  if (month > 3 && month < 10)
    return true;

  int last_sunday = last_sunday_of_month(year, month);

  if (month == 3)
  {
    if (day > last_sunday)
      return true;
    if (day < last_sunday)
      return false;
    return hour >= 1;
  }

  /* month == 10 */
  if (day < last_sunday)
    return true;
  if (day > last_sunday)
    return false;
  return hour < 1;
}

static bool is_leap_year(int year)
{
  return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

static int days_in_month(int year, int month)
{
  static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2 && is_leap_year(year))
    return 29;
  if (month < 1 || month > 12)
    return 30;
  return days[month - 1];
}

static int day_of_week(int year, int month, int day)
{
  static const int offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (month < 3)
    year -= 1;
  return (year + year / 4 - year / 100 + year / 400 + offsets[month - 1] + day) % 7;
}

static int last_sunday_of_month(int year, int month)
{
  int dim = days_in_month(year, month);
  int dow = day_of_week(year, month, dim);
  return dim - dow;
}

void Lvgl_Example1_close(void)
{
  /*Delete all animation*/
  lv_anim_del(NULL, NULL);

  lv_timer_del(meter2_timer);
  meter2_timer = NULL;

  lv_obj_clean(lv_scr_act());

  current_temp_arc = NULL;
  set_temp_arc = NULL;
  current_pressure_arc = NULL;
  tick_layer = NULL;
  temp_label = NULL;
  pressure_label = NULL;
  temp_icon = NULL;
  pressure_icon = NULL;
  shot_time_label = NULL;
  shot_volume_label = NULL;
  shot_time_icon = NULL;
  shot_volume_icon = NULL;
  temp_units_label = NULL;
  pressure_units_label = NULL;
  shot_time_units_label = NULL;
  shot_volume_units_label = NULL;
  comm_status_container = NULL;
  comm_status_set_count = 0;
  memset(comm_status_sets, 0, sizeof(comm_status_sets));
  last_wifi_state = -1;
  last_mqtt_state = -1;
  last_esp_state = -1;
  Backlight_slider = NULL;
  beep_on_shot_btn = NULL;
  beep_on_shot_label = NULL;
  shot_def_dd = NULL;
  shot_duration_label = NULL;
  shot_duration_roller = NULL;
  shot_volume_label = NULL;
  shot_volume_roller = NULL;
  heater_switch = NULL;
  heater_status_label = NULL;
  s_syncing_heater_switch = false;
  set_temp_val = 0.0f;
  heater_on = false;

  if (standby_timer)
  {
    lv_timer_del(standby_timer);
    standby_timer = NULL;
  }
  standby_time_label = NULL;
  standby_screen = NULL;
  standby_active = false;
  current_screen = NULL;
  last_active_screen = NULL;
  menu_screen = NULL;
  brew_screen = NULL;
  settings_scr = NULL;
  steam_screen = NULL;
  profiles_screen = NULL;

  lv_style_reset(&style_text_muted);
  lv_style_reset(&style_title);
  lv_style_reset(&style_icon);
  lv_style_reset(&style_bullet);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void Status_create(lv_obj_t *parent)
{
  lv_obj_set_style_border_width(parent, 0, 0);

  comm_status_container = create_comm_status_row(parent, -45);

  const lv_coord_t current_arc_width = 20;
  lv_coord_t meter_base = LV_MIN(lv_obj_get_content_width(parent),
                                 lv_obj_get_content_height(parent)) -
                          tab_h_global;
  lv_coord_t meter_size = meter_base;

  /* ----------------- Arcs ----------------- */
  set_temp_arc = lv_arc_create(parent);
  lv_obj_set_size(set_temp_arc, meter_size, meter_size);
  lv_obj_align(set_temp_arc, LV_ALIGN_CENTER, 0, tab_h_global / 2);
  lv_arc_set_range(set_temp_arc, TEMP_ARC_MIN, TEMP_ARC_MAX);
  lv_arc_set_rotation(set_temp_arc, TEMP_ARC_START);
  lv_arc_set_bg_angles(set_temp_arc, 0, TEMP_ARC_SIZE);
  lv_obj_remove_style(set_temp_arc, NULL, LV_PART_KNOB);
  lv_obj_clear_flag(set_temp_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(set_temp_arc, 4, LV_PART_MAIN);
  lv_obj_set_style_arc_width(set_temp_arc, 4, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(set_temp_arc, lv_palette_darken(LV_PALETTE_GREY, 2), LV_PART_MAIN);
  lv_obj_set_style_arc_color(set_temp_arc, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(set_temp_arc, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(set_temp_arc, 0, 0);
  lv_arc_set_value(set_temp_arc, 80);

  current_temp_arc = lv_arc_create(parent);
  lv_obj_set_size(current_temp_arc, meter_size, meter_size);
  lv_obj_align(current_temp_arc, LV_ALIGN_CENTER, 0, tab_h_global / 2);
  lv_arc_set_range(current_temp_arc, TEMP_ARC_MIN, TEMP_ARC_MAX);
  lv_arc_set_rotation(current_temp_arc, TEMP_ARC_START);
  lv_arc_set_bg_angles(current_temp_arc, 0, TEMP_ARC_SIZE);
  lv_obj_remove_style(current_temp_arc, NULL, LV_PART_KNOB);
  lv_obj_clear_flag(current_temp_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(current_temp_arc, current_arc_width, LV_PART_MAIN);
  lv_obj_set_style_arc_width(current_temp_arc, current_arc_width, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(current_temp_arc, lv_palette_darken(LV_PALETTE_GREY, 2), LV_PART_MAIN);
  lv_obj_set_style_arc_color(current_temp_arc, lv_palette_main(LV_PALETTE_YELLOW), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(current_temp_arc, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(current_temp_arc, 0, 0);
  lv_arc_set_value(current_temp_arc, 80);

  current_pressure_arc = lv_arc_create(parent);
  lv_obj_set_size(current_pressure_arc, meter_size, meter_size);
  lv_obj_align(current_pressure_arc, LV_ALIGN_CENTER, 0, tab_h_global / 2);
  lv_arc_set_range(current_pressure_arc, PRESSURE_ARC_MIN, PRESSURE_ARC_MAX);
  lv_arc_set_rotation(current_pressure_arc, PRESSURE_ARC_START);
  lv_arc_set_bg_angles(current_pressure_arc, 0, PRESSURE_ARC_SIZE);
  lv_obj_remove_style(current_pressure_arc, NULL, LV_PART_KNOB);
  lv_obj_clear_flag(current_pressure_arc, LV_OBJ_FLAG_CLICKABLE);
  lv_arc_set_mode(current_pressure_arc, LV_ARC_MODE_REVERSE);
  lv_obj_set_style_arc_width(current_pressure_arc, current_arc_width, LV_PART_MAIN);
  lv_obj_set_style_arc_width(current_pressure_arc, current_arc_width, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(current_pressure_arc, lv_palette_darken(LV_PALETTE_GREY, 2), LV_PART_MAIN);
  lv_obj_set_style_arc_color(current_pressure_arc, lv_palette_main(LV_PALETTE_RED), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(current_pressure_arc, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(current_pressure_arc, 0, 0);
  lv_arc_set_value(current_pressure_arc, 50);

  /* Ticks above arcs */
  tick_layer = lv_obj_create(parent);
  lv_obj_set_size(tick_layer, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(tick_layer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(tick_layer, 0, 0);
  lv_obj_add_event_cb(tick_layer, draw_ticks_cb, LV_EVENT_DRAW_POST, NULL);

  /* ----------------- Fonts ----------------- */
  const lv_font_t *font_val = &lv_font_montserrat_40;
  const lv_font_t *font_units = &lv_font_montserrat_28;
  const lv_font_t *font_icon = &mdi_icons_40;

  /* ----------------- Helpers for rows/cells ----------------- */
  static lv_coord_t row_cols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  static lv_coord_t row_rows[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
  static lv_coord_t field_cols[] = {LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
  static lv_coord_t field_rows[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

/* Create one field (icon | value | units) in ROW at column COL (0 or 1) */
#define MAKE_FIELD(ROW, COL, ICON_TXT, OUT_ICON, OUT_VAL, OUT_UNITS, INIT_VAL_TXT, UNITS_TXT) \
  do                                                                                          \
  {                                                                                           \
    lv_obj_t *cell = lv_obj_create(ROW);                                                      \
    lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, 0);                                          \
    lv_obj_set_style_border_width(cell, 0, 0);                                                \
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);                                          \
    lv_obj_set_grid_dsc_array(cell, field_cols, field_rows);                                  \
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_STRETCH, (COL), 1, LV_GRID_ALIGN_CENTER, 0, 1);  \
    /* icon (left) */                                                                         \
    OUT_ICON = lv_label_create(cell);                                                         \
    lv_label_set_text(OUT_ICON, (ICON_TXT));                                                  \
    lv_obj_set_style_text_font(OUT_ICON, font_icon, 0);                                       \
    lv_obj_set_style_text_color(OUT_ICON, lv_color_white(), 0);                               \
    lv_obj_set_grid_cell(OUT_ICON, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);    \
    /* value (right) */                                                                       \
    OUT_VAL = lv_label_create(cell);                                                          \
    lv_label_set_text(OUT_VAL, (INIT_VAL_TXT));                                               \
    lv_obj_set_style_text_font(OUT_VAL, font_val, 0);                                         \
    lv_obj_set_style_text_color(OUT_VAL, lv_color_white(), 0);                                \
    lv_obj_set_grid_cell(OUT_VAL, LV_GRID_ALIGN_END, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);       \
    /* units (left in column) */                                                              \
    OUT_UNITS = lv_label_create(cell);                                                        \
    lv_label_set_text(OUT_UNITS, (UNITS_TXT));                                                \
    lv_obj_set_style_text_font(OUT_UNITS, font_units, 0);                                     \
    lv_obj_set_style_text_color(OUT_UNITS, lv_color_white(), 0);                              \
    lv_obj_set_grid_cell(OUT_UNITS, LV_GRID_ALIGN_START, 2, 1, LV_GRID_ALIGN_CENTER, 0, 1);   \
  } while (0)

  const lv_coord_t H = lv_disp_get_ver_res(NULL);

  /* ----------------- Bottom row @ 50% ----------------- */
  lv_obj_t *row_bottom = lv_obj_create(parent);
  lv_obj_set_style_bg_opa(row_bottom, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row_bottom, 0, 0);
  lv_obj_clear_flag(row_bottom, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_grid_dsc_array(row_bottom, row_cols, row_rows);
  lv_obj_set_width(row_bottom, LV_PCT(92)); /* give some side margin */
  lv_obj_align(row_bottom, LV_ALIGN_CENTER, 0, (H * 5) / 100);

  /* left: shot time, right: shot volume */
  MAKE_FIELD(row_bottom, 0, MDI_CLOCK, shot_time_icon, shot_time_label, shot_time_units_label, "0", "s");
  MAKE_FIELD(row_bottom, 1, MDI_BEAKER, shot_volume_icon, shot_volume_label, shot_volume_units_label, "0.0", "ml");

  /* ----------------- Top row @ 70% ----------------- */
  lv_obj_t *row_top = lv_obj_create(parent);
  lv_obj_set_style_bg_opa(row_top, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row_top, 0, 0);
  lv_obj_clear_flag(row_top, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_grid_dsc_array(row_top, row_cols, row_rows);
  lv_obj_set_width(row_top, LV_PCT(92));
  /* top row ABOVE center -> 30%: offset = -20% of screen height */
  lv_obj_align(row_top, LV_ALIGN_CENTER, 0, -(H * 10) / 100);

  /* left: temperature, right: pressure */
  MAKE_FIELD(row_top, 0, MDI_THERMOMETER, temp_icon, temp_label, temp_units_label, "0.0", "°C");
  MAKE_FIELD(row_top, 1, MDI_GAUGE, pressure_icon, pressure_label, pressure_units_label, "0.0", "bar");

  /* Keep rows above arcs/ticks */
  lv_obj_move_foreground(row_bottom);
  lv_obj_move_foreground(row_top);

  /* ----------------- Home button above battery ----------------- */
  lv_obj_t *menu_btn = lv_btn_create(parent);
  lv_obj_set_size(menu_btn, 80, 80);
  lv_obj_set_style_border_width(menu_btn, 0, 0);
  lv_obj_set_style_bg_color(menu_btn, lv_palette_main(LV_PALETTE_GREY), 0);
  lv_obj_add_flag(menu_btn, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_align(menu_btn, LV_ALIGN_BOTTOM_MID, 0, -70);
  lv_obj_add_event_cb(menu_btn, open_menu_event_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *menu_label = lv_label_create(menu_btn);
  lv_label_set_text(menu_label, LV_SYMBOL_HOME);
  lv_obj_center(menu_label);

  add_version_label(parent);

  /* Battery percentage bar above version text */
  battery_bar = lv_bar_create(parent);
  /* Make it shorter and thicker */
  lv_obj_set_size(battery_bar, lv_obj_get_width(parent) / 3, 18);
  lv_obj_align(battery_bar, LV_ALIGN_BOTTOM_MID, 0, -45);
  lv_bar_set_range(battery_bar, 0, 100);
  int batt_init = Battery_GetPercentage();
  lv_bar_set_value(battery_bar, batt_init, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(battery_bar, lv_palette_main(LV_PALETTE_GREY), 0);
  lv_obj_set_style_bg_color(battery_bar, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);

  /* Add centered percentage label on the bar */
  battery_label = lv_label_create(battery_bar);
  lv_label_set_text_fmt(battery_label, "%d%%", batt_init);
  lv_obj_set_style_text_color(battery_label, lv_color_white(), 0);
  lv_obj_center(battery_label);

  /* Timer to drive UI updates */
  auto_step_timer = lv_timer_create(example1_increase_lvgl_tick, 100, NULL);
}

static void draw_ticks_cb(lv_event_t *e)
{
  if (!current_temp_arc && !current_pressure_arc)
    return;

  lv_obj_t *ref = current_temp_arc ? current_temp_arc : current_pressure_arc;
  lv_draw_ctx_t *draw_ctx = lv_event_get_draw_ctx(e);
  lv_coord_t cx = lv_obj_get_x(ref) + lv_obj_get_width(ref) / 2;
  lv_coord_t cy = lv_obj_get_y(ref) + lv_obj_get_height(ref) / 2;
  lv_coord_t radius = lv_obj_get_width(ref) / 2;

  lv_draw_line_dsc_t line_dsc;
  lv_draw_line_dsc_init(&line_dsc);
  line_dsc.color = lv_color_white();
  line_dsc.width = 2;

  lv_draw_label_dsc_t label_dsc;
  lv_draw_label_dsc_init(&label_dsc);
  label_dsc.color = lv_color_white();
  label_dsc.font = font_normal;
  label_dsc.align = LV_TEXT_ALIGN_CENTER;

  if (current_temp_arc)
  {
    for (int val = TEMP_ARC_MIN; val <= TEMP_ARC_MAX; val += TEMP_ARC_TICK)
    {
      int angle = TEMP_ARC_START + (val - TEMP_ARC_MIN) * TEMP_ARC_SIZE /
                                       (TEMP_ARC_MAX - TEMP_ARC_MIN);
      float rad = angle * 3.14159265f / 180.0f;
      lv_coord_t len = 20;

      lv_point_t p1 = {cx + (lv_coord_t)((radius - len) * cosf(rad)),
                       cy + (lv_coord_t)((radius - len) * sinf(rad))};
      lv_point_t p2 = {cx + (lv_coord_t)(radius * cosf(rad)),
                       cy + (lv_coord_t)(radius * sinf(rad))};

      lv_draw_line(draw_ctx, &line_dsc, &p1, &p2);

      char buf[8];
      lv_snprintf(buf, sizeof(buf), "%d", val);
      lv_coord_t text_r = radius - len - 10;
      lv_point_t tp = {cx + (lv_coord_t)(text_r * cosf(rad)),
                       cy + (lv_coord_t)(text_r * sinf(rad))};
      lv_area_t a = {tp.x - 20, tp.y - 10, tp.x + 20, tp.y + 10};
      lv_draw_label(draw_ctx, &label_dsc, &a, buf, NULL);
    }

    if (heater_on)
    {
      float v = set_temp_val;
      if (!isnan(v))
      {
        if (v < TEMP_ARC_MIN)
          v = TEMP_ARC_MIN;
        else if (v > TEMP_ARC_MAX)
          v = TEMP_ARC_MAX;
        float angle = TEMP_ARC_START + (v - TEMP_ARC_MIN) *
                                           TEMP_ARC_SIZE /
                                           (float)(TEMP_ARC_MAX - TEMP_ARC_MIN);
        float rad = angle * 3.14159265f / 180.0f;
        lv_coord_t len = 20;
        lv_point_t p1 = {cx + (lv_coord_t)((radius - len) * cosf(rad)),
                         cy + (lv_coord_t)((radius - len) * sinf(rad))};
        lv_point_t p2 = {cx + (lv_coord_t)(radius * cosf(rad)),
                         cy + (lv_coord_t)(radius * sinf(rad))};
        lv_draw_line_dsc_t red_dsc = line_dsc;
        red_dsc.color = lv_palette_main(LV_PALETTE_RED);
        lv_draw_line(draw_ctx, &red_dsc, &p1, &p2);
      }
    }
  }

  if (current_pressure_arc)
  {
    for (int val = PRESSURE_ARC_MIN; val <= PRESSURE_ARC_MAX;
         val += PRESSURE_ARC_TICK)
    {
      int angle = PRESSURE_ARC_START + PRESSURE_ARC_SIZE -
                  (val - PRESSURE_ARC_MIN) * PRESSURE_ARC_SIZE /
                      (PRESSURE_ARC_MAX - PRESSURE_ARC_MIN);
      float rad = angle * 3.14159265f / 180.0f;
      lv_coord_t len = 20;

      lv_point_t p1 = {cx + (lv_coord_t)((radius - len) * cosf(rad)),
                       cy + (lv_coord_t)((radius - len) * sinf(rad))};
      lv_point_t p2 = {cx + (lv_coord_t)(radius * cosf(rad)),
                       cy + (lv_coord_t)(radius * sinf(rad))};

      lv_draw_line(draw_ctx, &line_dsc, &p1, &p2);

      char buf[8];
      lv_snprintf(buf, sizeof(buf), "%d", val / 10);
      lv_coord_t text_r = radius - len - 10;
      lv_point_t tp = {cx + (lv_coord_t)(text_r * cosf(rad)),
                       cy + (lv_coord_t)(text_r * sinf(rad))};
      lv_area_t a = {tp.x - 20, tp.y - 10, tp.x + 20, tp.y + 10};
      lv_draw_label(draw_ctx, &label_dsc, &a, buf, NULL);
    }
  }
}

void example1_increase_lvgl_tick(lv_timer_t *t)
{
  float current = MQTT_GetCurrentTemp();
  float set = MQTT_GetSetTemp();
  float current_p = MQTT_GetCurrentPressure();
  float shot_time = MQTT_GetShotTime();
  float shot_vol = MQTT_GetShotVolume();
  bool heater = MQTT_GetHeaterState();
  bool pump_pressure_mode = MQTT_GetPumpPressureMode();
  char buf[32];

  set_temp_val = set;
  heater_on = heater;
  update_heater_controls(heater);
  pump_pressure_mode_on = pump_pressure_mode;
  update_pump_pressure_controls(pump_pressure_mode);

  if (tick_layer)
    lv_obj_invalidate(tick_layer);

  bool wifi_ok = Wireless_IsWiFiConnected();
  bool mqtt_ok = Wireless_IsMQTTConnected();
  bool espnow_active = Wireless_IsEspNowActive();
  bool espnow_link = Wireless_UsingEspNow();
  int esp_state = espnow_link ? 2 : (espnow_active ? 1 : 0);

  if (wifi_ok != last_wifi_state)
  {
    lv_color_t colour = lv_palette_main(wifi_ok ? LV_PALETTE_GREEN : LV_PALETTE_RED);
    const char *icon_txt = wifi_ok ? MDI_WIFI_ON : MDI_WIFI_OFF;
    for (size_t i = 0; i < comm_status_set_count; ++i)
    {
      if (!comm_status_sets[i].wifi)
        continue;
      lv_label_set_text(comm_status_sets[i].wifi, icon_txt);
      lv_obj_set_style_text_color(comm_status_sets[i].wifi, colour, 0);
    }
    last_wifi_state = wifi_ok;
  }

  if (mqtt_ok != last_mqtt_state)
  {
    lv_color_t colour = lv_palette_main(mqtt_ok ? LV_PALETTE_GREEN : LV_PALETTE_RED);
    const char *icon_txt = mqtt_ok ? MDI_MQTT_ON : MDI_MQTT_OFF;
    for (size_t i = 0; i < comm_status_set_count; ++i)
    {
      if (!comm_status_sets[i].mqtt)
        continue;
      lv_label_set_text(comm_status_sets[i].mqtt, icon_txt);
      lv_obj_set_style_text_color(comm_status_sets[i].mqtt, colour, 0);
    }
    last_mqtt_state = mqtt_ok;
  }

  if (esp_state != last_esp_state)
  {
    const char *icon_txt = MDI_ESP_NOW_OFF;
    lv_color_t colour = lv_palette_main(LV_PALETTE_RED);
    if (esp_state == 1)
    {
      icon_txt = MDI_ESP_NOW_PAIR;
      colour = lv_palette_main(LV_PALETTE_YELLOW);
    }
    else if (esp_state == 2)
    {
      icon_txt = MDI_ESP_NOW_ON;
      colour = lv_palette_main(LV_PALETTE_GREEN);
    }
    for (size_t i = 0; i < comm_status_set_count; ++i)
    {
      if (!comm_status_sets[i].espnow)
        continue;
      lv_label_set_text(comm_status_sets[i].espnow, icon_txt);
      lv_obj_set_style_text_color(comm_status_sets[i].espnow, colour, 0);
    }
    last_esp_state = esp_state;
  }

  int batt = Battery_GetPercentage();
  if (battery_bar && batt != last_battery)
  {
    lv_bar_set_value(battery_bar, batt, LV_ANIM_OFF);
    lv_color_t col = lv_palette_main(LV_PALETTE_GREEN);
    if (batt < 20)
      col = lv_palette_main(LV_PALETTE_RED);
    else if (batt < 50)
      col = lv_palette_main(LV_PALETTE_YELLOW);
    lv_obj_set_style_bg_color(battery_bar, col, LV_PART_INDICATOR);
    if (battery_label)
    {
      lv_label_set_text_fmt(battery_label, "%d%%", batt);
      lv_obj_center(battery_label);
    }
    last_battery = batt;
  }

  if (isnan(current_p) || current_p < 0.0f)
    current_p = 0.0f;

  /* arcs */
  if (current_temp_arc)
  {
    int32_t v = LV_MIN(LV_MAX((int32_t)current, TEMP_ARC_MIN), TEMP_ARC_MAX);
    lv_arc_set_value(current_temp_arc, v);
  }
  if (set_temp_arc)
  {
    int32_t v = LV_MIN(LV_MAX((int32_t)set, TEMP_ARC_MIN), TEMP_ARC_MAX);
    lv_arc_set_value(set_temp_arc, v);
  }
  if (current_pressure_arc)
  {
    int32_t scaled = (int32_t)lroundf(current_p * 10.0f);
    int32_t clamped = LV_MIN(LV_MAX(scaled, PRESSURE_ARC_MIN), PRESSURE_ARC_MAX);
    int32_t reversed = PRESSURE_ARC_MAX - clamped + PRESSURE_ARC_MIN;
    lv_arc_set_value(current_pressure_arc, reversed);
  }

  /* temperature colour */
  if (temp_label && temp_icon && temp_units_label)
  {
    lv_color_t col = lv_color_white();
    if (!isnan(current) && !isnan(set))
    {
      if (current > set + TEMP_TOLERANCE)
        col = lv_palette_main(LV_PALETTE_RED);
      else if (current >= set - TEMP_TOLERANCE)
        col = lv_palette_main(LV_PALETTE_GREEN);
    }
    lv_obj_set_style_text_color(temp_label, col, 0);
    lv_obj_set_style_text_color(temp_icon, col, 0);
    lv_obj_set_style_text_color(temp_units_label, col, 0);
  }

  /* value labels ONLY (no units appended!) */
  if (temp_label)
  {
    snprintf(buf, sizeof buf, "%.1f", current);
    lv_label_set_text(temp_label, buf);
  }
  if (pressure_label)
  {
    snprintf(buf, sizeof buf, "%.1f", current_p);
    lv_label_set_text(pressure_label, buf);
  }
  if (shot_time_label)
  {
    float clamped_time = LV_MAX(shot_time, 0.0f);
    int shot_seconds = (int)floorf(clamped_time);
    snprintf(buf, sizeof buf, "%d", shot_seconds);
    lv_label_set_text(shot_time_label, buf);
  }
  if (shot_volume_label)
  {
    snprintf(buf, sizeof buf, "%.1f", shot_vol);
    lv_label_set_text(shot_volume_label, buf);
  }

  /* shot definition highlighting & buzzer */
  if (shot_def_dd)
  {
    uint16_t sel = lv_dropdown_get_selected(shot_def_dd);
    bool shot_active = (shot_time > 0.0f || shot_vol > 0.0f);
    if (!shot_active)
      shot_target_reached = false;
    lv_color_t white = lv_color_white();
    lv_color_t yellow = lv_palette_main(LV_PALETTE_YELLOW);
    bool beep_enabled = beep_on_shot_btn && lv_obj_has_state(beep_on_shot_btn, LV_STATE_CHECKED);
    if (sel == 1)
    {
      int target = roller_get_int_value(shot_duration_roller);
      lv_color_t col = white;
      if (shot_active && shot_time >= (float)target)
      {
        col = yellow;
        if (!shot_target_reached && beep_enabled)
        {
          Buzzer_On();
          if (!buzzer_timer)
            buzzer_timer = lv_timer_create(buzzer_timer_cb, 500, NULL);
        }
        shot_target_reached = true;
      }
      if (shot_time_icon)
        lv_obj_set_style_text_color(shot_time_icon, col, 0);
      if (shot_time_label)
        lv_obj_set_style_text_color(shot_time_label, col, 0);
      if (shot_time_units_label)
        lv_obj_set_style_text_color(shot_time_units_label, col, 0);
      if (shot_volume_icon)
        lv_obj_set_style_text_color(shot_volume_icon, white, 0);
      if (shot_volume_label)
        lv_obj_set_style_text_color(shot_volume_label, white, 0);
      if (shot_volume_units_label)
        lv_obj_set_style_text_color(shot_volume_units_label, white, 0);
    }
    else if (sel == 2)
    {
      int target = roller_get_int_value(shot_volume_roller);
      lv_color_t col = white;
      if (shot_active && shot_vol >= (float)target)
      {
        col = yellow;
        if (!shot_target_reached && beep_enabled)
        {
          Buzzer_On();
          if (!buzzer_timer)
            buzzer_timer = lv_timer_create(buzzer_timer_cb, 500, NULL);
        }
        shot_target_reached = true;
      }
      if (shot_volume_icon)
        lv_obj_set_style_text_color(shot_volume_icon, col, 0);
      if (shot_volume_label)
        lv_obj_set_style_text_color(shot_volume_label, col, 0);
      if (shot_volume_units_label)
        lv_obj_set_style_text_color(shot_volume_units_label, col, 0);
      if (shot_time_icon)
        lv_obj_set_style_text_color(shot_time_icon, white, 0);
      if (shot_time_label)
        lv_obj_set_style_text_color(shot_time_label, white, 0);
      if (shot_time_units_label)
        lv_obj_set_style_text_color(shot_time_units_label, white, 0);
    }
    else
    {
      if (shot_time_icon)
        lv_obj_set_style_text_color(shot_time_icon, white, 0);
      if (shot_time_label)
        lv_obj_set_style_text_color(shot_time_label, white, 0);
      if (shot_time_units_label)
        lv_obj_set_style_text_color(shot_time_units_label, white, 0);
      if (shot_volume_icon)
        lv_obj_set_style_text_color(shot_volume_icon, white, 0);
      if (shot_volume_label)
        lv_obj_set_style_text_color(shot_volume_label, white, 0);
      if (shot_volume_units_label)
        lv_obj_set_style_text_color(shot_volume_units_label, white, 0);
      shot_target_reached = false;
    }
  }

  /* backlight
   * Keep the slider UI in sync, but avoid forcing the backlight every cycle.
   * The main loop handles idle dim/off; Set_Backlight should only be called
   * from the slider event handler when the user changes the value.
   */
  if (Backlight_slider)
  {
    int v = lv_slider_get_value(Backlight_slider);
    if (v != LCD_Backlight)
    {
      s_syncing_backlight = true;
      lv_slider_set_value(Backlight_slider, LCD_Backlight, LV_ANIM_OFF);
      s_syncing_backlight = false;
    }
  }
}

void Backlight_adjustment_event_cb(lv_event_t *e)
{
  if (s_syncing_backlight)
    return; // ignore programmatic updates
  uint8_t Backlight = lv_slider_get_value(lv_event_get_target(e));
  if (Backlight <= Backlight_MAX)
  {
    lv_slider_set_value(Backlight_slider, Backlight, LV_ANIM_ON);
    LCD_Backlight = Backlight;
    LVGL_Backlight_adjustment(Backlight);
  }
  else
    ESP_LOGW("LVGL", "Volume out of range: %d", Backlight);
}

// static void shot_def_dd_event_cb(lv_event_t *e)
// {
//   uint16_t sel = lv_dropdown_get_selected(lv_event_get_target(e));
//   switch (sel)
//   {
//   case 1: /* Time */
//     lv_obj_clear_flag(shot_duration_label, LV_OBJ_FLAG_HIDDEN);
//     lv_obj_clear_flag(shot_duration_roller, LV_OBJ_FLAG_HIDDEN);
//     lv_obj_add_flag(shot_volume_label, LV_OBJ_FLAG_HIDDEN);
//     lv_obj_add_flag(shot_volume_roller, LV_OBJ_FLAG_HIDDEN);
//     lv_obj_clear_flag(beep_on_shot_label, LV_OBJ_FLAG_HIDDEN);
//     lv_obj_clear_flag(beep_on_shot_btn, LV_OBJ_FLAG_HIDDEN);
//     break;
//   case 2: /* Volume */
//     lv_obj_clear_flag(shot_volume_label, LV_OBJ_FLAG_HIDDEN);
//     lv_obj_clear_flag(shot_volume_roller, LV_OBJ_FLAG_HIDDEN);
//     lv_obj_add_flag(shot_duration_label, LV_OBJ_FLAG_HIDDEN);
//     lv_obj_add_flag(shot_duration_roller, LV_OBJ_FLAG_HIDDEN);
//     lv_obj_clear_flag(beep_on_shot_label, LV_OBJ_FLAG_HIDDEN);
//     lv_obj_clear_flag(beep_on_shot_btn, LV_OBJ_FLAG_HIDDEN);
//     break;
//   default:
//     lv_obj_add_flag(shot_duration_label, LV_OBJ_FLAG_HIDDEN);
//     lv_obj_add_flag(shot_duration_roller, LV_OBJ_FLAG_HIDDEN);
//     lv_obj_add_flag(shot_volume_label, LV_OBJ_FLAG_HIDDEN);
//     lv_obj_add_flag(shot_volume_roller, LV_OBJ_FLAG_HIDDEN);
//     lv_obj_add_flag(beep_on_shot_label, LV_OBJ_FLAG_HIDDEN);
//     lv_obj_add_flag(beep_on_shot_btn, LV_OBJ_FLAG_HIDDEN);
//     break;
//   }
// }

static int roller_get_int_value(lv_obj_t *roller)
{
  if (!roller)
    return 0;

  char buf[8];
  lv_roller_get_selected_str(roller, buf, sizeof buf);
  return atoi(buf);
}

// static void beep_on_shot_btn_event_cb(lv_event_t *e)
// {
//   lv_obj_t *btn = lv_event_get_target(e);
//   lv_obj_t *label = lv_obj_get_child(btn, 0);
//   if (lv_obj_has_state(btn, LV_STATE_CHECKED))
//   {
//     lv_label_set_text(label, "On");
//   }
//   else
//   {
//     lv_label_set_text(label, "Off");
//   }
// }

static void buzzer_timer_cb(lv_timer_t *t)
{
  Buzzer_Off();
  lv_timer_del(t);
  buzzer_timer = NULL;
}

static void heater_switch_event_cb(lv_event_t *e)
{
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED)
    return;

  if (s_syncing_heater_switch)
    return;

  lv_obj_t *sw = lv_event_get_target(e);
  bool enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
  MQTT_SetHeaterState(enabled, false);
  update_heater_controls(enabled);
}

static void update_heater_controls(bool heater_state)
{
  if (heater_switch)
  {
    bool checked = lv_obj_has_state(heater_switch, LV_STATE_CHECKED);
    if (checked != heater_state)
    {
      s_syncing_heater_switch = true;
      if (heater_state)
        lv_obj_add_state(heater_switch, LV_STATE_CHECKED);
      else
        lv_obj_clear_state(heater_switch, LV_STATE_CHECKED);
      s_syncing_heater_switch = false;
    }
  }

  // if (heater_status_label)
  // {
  //   lv_label_set_text(heater_status_label,
  //                     heater_state ? "Heater is on" : "Heater is off");
  // }
}

static void pump_pressure_switch_event_cb(lv_event_t *e)
{
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED)
    return;

  if (s_syncing_pump_pressure_switch)
    return;

  lv_obj_t *sw = lv_event_get_target(e);
  bool enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
  MQTT_SetPumpPressureMode(enabled);
  update_pump_pressure_controls(enabled);
}

static void update_pump_pressure_controls(bool enabled)
{
  pump_pressure_mode_on = enabled;

  if (pump_pressure_switch)
  {
    bool checked = lv_obj_has_state(pump_pressure_switch, LV_STATE_CHECKED);
    if (checked != enabled)
    {
      s_syncing_pump_pressure_switch = true;
      if (enabled)
        lv_obj_add_state(pump_pressure_switch, LV_STATE_CHECKED);
      else
        lv_obj_clear_state(pump_pressure_switch, LV_STATE_CHECKED);
      s_syncing_pump_pressure_switch = false;
    }
  }

  if (pump_setting_label)
  {
    lv_label_set_text(pump_setting_label,
                      enabled ? "Pressure setpoint" : "Pump power");
  }

  // if (pump_pressure_status_label)
  // {
  //   lv_label_set_text(pump_pressure_status_label,
  //                     enabled ? "Pump pressure control is on"
  //                             : "Pump pressure control is off");
  // }

  refresh_pump_setting_ui();
}

static const char *pressure_option_list(void)
{
  static char options[256];
  static bool cached = false;

  if (!cached)
  {
    char *ptr = options;
    size_t remaining = sizeof(options);

    for (int i = 0; i <= 24; ++i)
    {
      float value = i * 0.5f;
      int written =
          snprintf(ptr, remaining, "%.1f bar%s", value, i < 24 ? "\n" : "");
      if (written < 0 || (size_t)written >= remaining)
        break;
      ptr += written;
      remaining -= (size_t)written;
    }

    cached = true;
  }

  options[sizeof(options) - 1] = '\0';
  return options;
}

static const char *pump_power_option_list(void)
{
  static char options[128];
  static bool cached = false;

  if (!cached)
  {
    char *ptr = options;
    size_t remaining = sizeof(options);

    for (int value = 40; value <= 95; value += 5)
    {
      int written =
          snprintf(ptr, remaining, "%d%%%s", value, value < 95 ? "\n" : "");
      if (written < 0 || (size_t)written >= remaining)
        break;
      ptr += written;
      remaining -= (size_t)written;
    }

    cached = true;
  }

  options[sizeof(options) - 1] = '\0';
  return options;
}

static void refresh_pump_setting_ui(void)
{
  if (!pump_setting_roller)
    return;

  bool mode_changed =
      pump_pressure_mode_on != s_pump_setting_options_pressure_mode;

  bool guard_active = false;

  if (!s_pump_setting_options_set || mode_changed)
  {
    s_syncing_pump_setting_roller = true;
    guard_active = true;
    const char *options = pump_pressure_mode_on ? pressure_option_list()
                                                : pump_power_option_list();
    lv_roller_set_options(pump_setting_roller, options, LV_ROLLER_MODE_NORMAL);
    s_pump_setting_options_set = true;
    s_pump_setting_options_pressure_mode = pump_pressure_mode_on;
  }

  int desired_index = 0;
  if (pump_pressure_mode_on)
  {
    float pressure = MQTT_GetSetPressure();
    if (isnan(pressure))
      pressure = 0.0f;
    if (pressure < 0.0f)
      pressure = 0.0f;
    if (pressure > 12.0f)
      pressure = 12.0f;
    desired_index = (int)floorf((pressure * 2.0f) + 0.5f);
    if (desired_index < 0)
      desired_index = 0;
    if (desired_index > 24)
      desired_index = 24;
  }
  else
  {
    float power = MQTT_GetPumpPower();
    if (isnan(power))
      power = 40.0f;
    if (power < 40.0f)
      power = 40.0f;
    if (power > 95.0f)
      power = 95.0f;
    desired_index = (int)floorf(((power - 40.0f) / 5.0f) + 0.5f);
    if (desired_index < 0)
      desired_index = 0;
    if (desired_index > 11)
      desired_index = 11;
  }

  uint16_t current = lv_roller_get_selected(pump_setting_roller);
  if (current != (uint16_t)desired_index)
  {
    if (!guard_active)
    {
      s_syncing_pump_setting_roller = true;
      guard_active = true;
    }
    lv_roller_set_selected(pump_setting_roller, desired_index, LV_ANIM_OFF);
  }

  if (guard_active)
    s_syncing_pump_setting_roller = false;
}

static void pump_setting_roller_event_cb(lv_event_t *e)
{
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED)
    return;

  if (s_syncing_pump_setting_roller)
    return;

  lv_obj_t *roller = lv_event_get_target(e);
  uint16_t selected = lv_roller_get_selected(roller);

  if (pump_pressure_mode_on)
  {
    float value = selected * 0.5f;
    MQTT_SetPressureSetpoint(value);
  }
  else
  {
    float value = 40.0f + selected * 5.0f;
    MQTT_SetPumpPower(value);
  }
}

void LVGL_Backlight_adjustment(uint8_t Backlight) { Set_Backlight(Backlight); }

void LVGL_EnterStandby(void)
{
  if (standby_active)
    return;

  if (!standby_screen)
  {
    standby_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(standby_screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(standby_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(standby_screen, 0, 0);
    lv_obj_set_style_text_color(standby_screen, lv_color_white(), 0);
    lv_obj_set_style_pad_all(standby_screen, 0, 0);

    (void)create_comm_status_row(standby_screen, -45);

    // lv_obj_t *title = lv_label_create(standby_screen);
    // lv_label_set_text(title, "Standby");
    // lv_obj_add_style(title, &style_title, 0);
    // lv_obj_set_style_text_color(title, lv_color_white(), 0);
    // lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

    standby_time_label = lv_label_create(standby_screen);
    lv_obj_add_style(standby_time_label, &style_title, 0);
    lv_obj_set_style_text_font(standby_time_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(standby_time_label, lv_color_white(), 0);
    const uint16_t zoom = (uint16_t)((STANDBY_CLOCK_FONT_SIZE_PX * LV_IMG_ZOOM_NONE + 24) / 48);
    lv_obj_set_style_transform_zoom(standby_time_label, zoom, 0);
    lv_obj_align(standby_time_label, LV_ALIGN_CENTER, 0, 0);
  }

  if (!standby_timer)
    standby_timer = lv_timer_create(standby_timer_cb, 1000, NULL);
  else
    lv_timer_resume(standby_timer);

  if (current_screen && current_screen != standby_screen)
    last_active_screen = current_screen;

  standby_active = true;
  update_standby_time();
  Set_Backlight(5);
  lv_scr_load(standby_screen);
  current_screen = standby_screen;
}

void LVGL_ExitStandby(void)
{
  if (!standby_active)
    return;

  standby_active = false;
  if (standby_timer)
    lv_timer_pause(standby_timer);

  lv_obj_t *target = last_active_screen;
  if (!target)
    target = brew_screen ? brew_screen : menu_screen;
  if (!target)
    target = lv_scr_act();
  load_screen(target);
}

bool LVGL_IsStandbyActive(void) { return standby_active; }

static void add_version_label(lv_obj_t *parent)
{
  lv_obj_t *ver = lv_label_create(parent);
  lv_label_set_text_fmt(ver, "v%s", VERSION);
  lv_obj_set_style_text_color(ver, lv_color_white(), 0);
  lv_obj_align(ver, LV_ALIGN_BOTTOM_MID, 0, 0);
}
static void register_comm_status_icons(lv_obj_t *screen, lv_obj_t *wifi, lv_obj_t *mqtt,
                                       lv_obj_t *espnow)
{
  if (!screen)
    return;

  for (size_t i = 0; i < comm_status_set_count; ++i)
  {
    if (comm_status_sets[i].screen == screen)
    {
      comm_status_sets[i].wifi = wifi;
      comm_status_sets[i].mqtt = mqtt;
      comm_status_sets[i].espnow = espnow;
      return;
    }
  }

  if (comm_status_set_count >= COMM_STATUS_MAX_SETS)
    return;

  comm_status_sets[comm_status_set_count].screen = screen;
  comm_status_sets[comm_status_set_count].wifi = wifi;
  comm_status_sets[comm_status_set_count].mqtt = mqtt;
  comm_status_sets[comm_status_set_count].espnow = espnow;
  comm_status_set_count++;
}

static lv_obj_t *create_comm_status_row(lv_obj_t *parent, lv_coord_t y_offset)
{
  lv_obj_t *container = lv_obj_create(parent);
  lv_obj_remove_style_all(container);
  lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(container, 0, 0);
  lv_obj_set_style_pad_gap(container, 12, 0);
  lv_obj_set_style_border_width(container, 0, 0);
  lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_align(container, LV_ALIGN_TOP_MID, 0, y_offset);

  lv_obj_t *wifi = lv_label_create(container);
  lv_obj_set_style_text_font(wifi, &mdi_icons_24, 0);
  lv_label_set_text(wifi, MDI_WIFI_OFF);
  lv_obj_set_style_text_color(wifi, lv_palette_main(LV_PALETTE_RED), 0);

  lv_obj_t *mqtt = lv_label_create(container);
  lv_obj_set_style_text_font(mqtt, &mdi_icons_24, 0);
  lv_label_set_text(mqtt, MDI_MQTT_OFF);
  lv_obj_set_style_text_color(mqtt, lv_palette_main(LV_PALETTE_RED), 0);

  lv_obj_t *espnow = lv_label_create(container);
  lv_obj_set_style_text_font(espnow, &mdi_icons_24, 0);
  lv_label_set_text(espnow, MDI_ESP_NOW_OFF);
  lv_obj_set_style_text_color(espnow, lv_palette_main(LV_PALETTE_RED), 0);

  register_comm_status_icons(lv_obj_get_screen(parent), wifi, mqtt, espnow);
  return container;
}

static lv_obj_t *create_menu_button(lv_obj_t *grid, uint8_t col, uint8_t row,
                                    const char *icon, const char *label)
{
  lv_obj_t *btn = lv_btn_create(grid);
  lv_obj_set_size(btn, 135, 135);
  lv_obj_set_style_border_width(btn, 0, 0);
  lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_GREY), 0);
  lv_obj_set_style_radius(btn, 12, 0);
  lv_obj_set_style_pad_all(btn, 12, 0);
  lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, row, 1);
  lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  lv_obj_t *icon_label = lv_label_create(btn);
  lv_obj_set_style_text_font(icon_label, &mdi_icons_80, 0);
  lv_label_set_text(icon_label, icon);
  lv_obj_set_style_text_color(icon_label, lv_color_white(), 0);

  lv_obj_t *text_label = lv_label_create(btn);
  lv_label_set_text(text_label, label);
  lv_obj_set_style_text_color(text_label, lv_color_white(), 0);

  return btn;
}
