#include "LVGL_UI.h"
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "version.h"
#include "Battery.h"

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
typedef struct
{
  lv_obj_t *screen;
  lv_obj_t *content_area;
  lv_obj_t *back_btn;
  lv_obj_t *wifi_icon;
  lv_obj_t *mqtt_icon;
  lv_obj_t *esp_icon;
  lv_obj_t *battery_bar_obj;
  lv_obj_t *battery_label_obj;
} screen_template_t;

static void Brew_screen_create(void);
static void Menu_screen_create(void);
static void Steam_screen_create(void);
static void Profiles_screen_create(void);
static void Settings_screen_create(void);
static void Standby_screen_create(void);
static void template_init(screen_template_t *tmpl);
static void template_screen_loaded_cb(lv_event_t *e);
static void template_set_back_handler(screen_template_t *tmpl, lv_event_cb_t cb, void *user_data);
static void draw_ticks_cb(lv_event_t *e);
static void buzzer_timer_cb(lv_timer_t *t);
static int roller_get_int_value(lv_obj_t *roller);
static void menu_brew_event_cb(lv_event_t *e);
static void menu_steam_event_cb(lv_event_t *e);
static void menu_profiles_event_cb(lv_event_t *e);
static void menu_settings_event_cb(lv_event_t *e);
static void back_to_menu_event_cb(lv_event_t *e);
static void standby_timer_cb(lv_timer_t *t);
static void exit_standby_event_cb(lv_event_t *e);
static void update_standby_clock(void);

void example1_increase_lvgl_tick(lv_timer_t *t);
/**********************
 *  STATIC VARIABLES
 **********************/
static disp_size_t disp_size;

lv_style_t style_text_muted;
lv_style_t style_title;
static lv_style_t style_bullet;

static const lv_font_t *font_large;
static const lv_font_t *font_normal;

static lv_timer_t *auto_step_timer;
// static lv_color_t original_screen_bg_color;

static screen_template_t brew_template;
static screen_template_t menu_template;
static screen_template_t steam_template;
static screen_template_t profiles_template;
static screen_template_t settings_template;
static screen_template_t standby_template;

static screen_template_t *active_template;

static lv_obj_t *brew_screen;
static lv_obj_t *menu_screen;
static lv_obj_t *steam_screen;
static lv_obj_t *profiles_screen;
static lv_obj_t *settings_screen;
static lv_obj_t *standby_screen;
static lv_obj_t *standby_time_label;
static lv_timer_t *standby_timer;
static lv_obj_t *last_non_standby_screen;
static bool standby_active;

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
static lv_obj_t *wifi_status_icon;
static lv_obj_t *mqtt_status_icon;
static lv_obj_t *espnow_status_icon;
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
  // 设置字体

  lv_style_init(&style_text_muted);
  lv_style_set_text_opa(&style_text_muted, LV_OPA_90);
  lv_style_set_text_font(&style_text_muted, &lv_font_montserrat_20);

  lv_style_init(&style_title);
  lv_style_set_text_font(&style_title, &lv_font_montserrat_28);

  lv_style_init(&style_bullet);
  lv_style_set_border_width(&style_bullet, 0);
  lv_style_set_radius(&style_bullet, LV_RADIUS_CIRCLE);

  Backlight_slider = NULL;
  active_template = NULL;
  standby_timer = NULL;
  standby_time_label = NULL;
  standby_active = false;
  last_non_standby_screen = NULL;

  Brew_screen_create();
  Menu_screen_create();
  Steam_screen_create();
  Profiles_screen_create();
  Settings_screen_create();
  Standby_screen_create();

  lv_scr_load(menu_screen);
}

void Lvgl_Example1_close(void)
{
  /*Delete all animation*/
  lv_anim_del(NULL, NULL);

  if (auto_step_timer)
  {
    lv_timer_del(auto_step_timer);
    auto_step_timer = NULL;
  }
  if (standby_timer)
  {
    lv_timer_del(standby_timer);
    standby_timer = NULL;
  }

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
  wifi_status_icon = NULL;
  mqtt_status_icon = NULL;
  espnow_status_icon = NULL;
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
  set_temp_val = 0.0f;
  heater_on = false;
  standby_time_label = NULL;
  standby_active = false;
  brew_screen = NULL;
  menu_screen = NULL;
  steam_screen = NULL;
  profiles_screen = NULL;
  settings_screen = NULL;
  standby_screen = NULL;
  active_template = NULL;
  last_non_standby_screen = NULL;

  lv_style_reset(&style_text_muted);
  lv_style_reset(&style_title);
  lv_style_reset(&style_bullet);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void template_init(screen_template_t *tmpl)
{
  memset(tmpl, 0, sizeof(*tmpl));

  tmpl->screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(tmpl->screen, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(tmpl->screen, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(tmpl->screen, 0, 0);
  lv_obj_set_style_text_color(tmpl->screen, lv_color_white(), 0);
  lv_obj_set_style_text_font(tmpl->screen, font_normal, 0);

  lv_obj_t *status_container = lv_obj_create(tmpl->screen);
  lv_obj_remove_style_all(status_container);
  lv_obj_set_style_bg_opa(status_container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(status_container, 0, 0);
  lv_obj_set_style_pad_gap(status_container, 12, 0);
  lv_obj_set_style_border_width(status_container, 0, 0);
  lv_obj_set_flex_flow(status_container, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(status_container, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_align(status_container, LV_ALIGN_TOP_MID, 0, -30);

  tmpl->wifi_icon = lv_label_create(status_container);
  lv_obj_set_style_text_font(tmpl->wifi_icon, &mdi_icons_24, 0);
  lv_label_set_text(tmpl->wifi_icon, MDI_WIFI_OFF);
  lv_obj_set_style_text_color(tmpl->wifi_icon, lv_palette_main(LV_PALETTE_RED), 0);

  tmpl->mqtt_icon = lv_label_create(status_container);
  lv_obj_set_style_text_font(tmpl->mqtt_icon, &mdi_icons_24, 0);
  lv_label_set_text(tmpl->mqtt_icon, MDI_MQTT_OFF);
  lv_obj_set_style_text_color(tmpl->mqtt_icon, lv_palette_main(LV_PALETTE_RED), 0);

  tmpl->esp_icon = lv_label_create(status_container);
  lv_obj_set_style_text_font(tmpl->esp_icon, &mdi_icons_24, 0);
  lv_label_set_text(tmpl->esp_icon, MDI_ESP_NOW_OFF);
  lv_obj_set_style_text_color(tmpl->esp_icon, lv_palette_main(LV_PALETTE_RED), 0);

  tmpl->content_area = lv_obj_create(tmpl->screen);
  lv_obj_remove_style_all(tmpl->content_area);
  lv_obj_set_size(tmpl->content_area, LV_PCT(100), LV_PCT(100));
  lv_obj_add_flag(tmpl->content_area, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_style_bg_opa(tmpl->content_area, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(tmpl->content_area, 0, 0);
  lv_obj_align(tmpl->content_area, LV_ALIGN_CENTER, 0, 0);

  lv_obj_t *footer = lv_obj_create(tmpl->screen);
  lv_obj_remove_style_all(footer);
  lv_obj_set_size(footer, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_gap(footer, 8, 0);
  lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -10);

  tmpl->back_btn = lv_btn_create(footer);
  lv_obj_set_size(tmpl->back_btn, 120, 48);
  lv_obj_set_style_border_width(tmpl->back_btn, 0, 0);
  lv_obj_set_style_bg_color(tmpl->back_btn, lv_palette_main(LV_PALETTE_GREY), 0);
  lv_obj_t *back_label = lv_label_create(tmpl->back_btn);
  lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");
  lv_obj_center(back_label);

  tmpl->battery_bar_obj = lv_bar_create(footer);
  lv_obj_set_size(tmpl->battery_bar_obj, lv_disp_get_hor_res(NULL) / 3, 18);
  lv_bar_set_range(tmpl->battery_bar_obj, 0, 100);
  int batt_init = Battery_GetPercentage();
  lv_bar_set_value(tmpl->battery_bar_obj, batt_init, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(tmpl->battery_bar_obj, lv_palette_main(LV_PALETTE_GREY), 0);
  lv_obj_set_style_bg_color(tmpl->battery_bar_obj,
                            lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);

  tmpl->battery_label_obj = lv_label_create(tmpl->battery_bar_obj);
  lv_label_set_text_fmt(tmpl->battery_label_obj, "%d%%", batt_init);
  lv_obj_set_style_text_color(tmpl->battery_label_obj, lv_color_white(), 0);
  lv_obj_center(tmpl->battery_label_obj);

  lv_obj_t *version_label = lv_label_create(footer);
  lv_label_set_text_fmt(version_label, "v%s", VERSION);
  lv_obj_set_style_text_color(version_label, lv_color_white(), 0);

  lv_obj_move_foreground(footer);
  lv_obj_move_foreground(status_container);

  lv_obj_add_event_cb(tmpl->screen, template_screen_loaded_cb,
                      LV_EVENT_SCREEN_LOADED, tmpl);
}

static void template_set_back_handler(screen_template_t *tmpl, lv_event_cb_t cb,
                                      void *user_data)
{
  if (!tmpl || !tmpl->back_btn)
    return;

  if (cb)
  {
    lv_obj_clear_flag(tmpl->back_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(tmpl->back_btn, cb, LV_EVENT_CLICKED, user_data);
  }
  else
  {
    lv_obj_add_flag(tmpl->back_btn, LV_OBJ_FLAG_HIDDEN);
  }
}

static void template_screen_loaded_cb(lv_event_t *e)
{
  screen_template_t *tmpl = lv_event_get_user_data(e);
  if (!tmpl)
    return;

  active_template = tmpl;
  wifi_status_icon = tmpl->wifi_icon;
  mqtt_status_icon = tmpl->mqtt_icon;
  espnow_status_icon = tmpl->esp_icon;
  battery_bar = tmpl->battery_bar_obj;
  battery_label = tmpl->battery_label_obj;
  last_wifi_state = -1;
  last_mqtt_state = -1;
  last_esp_state = -1;
  last_battery = -1;

  if (tmpl != &standby_template)
    last_non_standby_screen = tmpl->screen;
}

static void Brew_screen_create(void)
{
  template_init(&brew_template);
  brew_screen = brew_template.screen;
  template_set_back_handler(&brew_template, back_to_menu_event_cb, NULL);

  lv_obj_t *parent = brew_template.content_area ? brew_template.content_area : brew_template.screen;
  lv_obj_set_style_border_width(parent, 0, 0);

  const lv_coord_t current_arc_width = 20;
  lv_coord_t disp_w = lv_disp_get_hor_res(NULL);
  lv_coord_t disp_h = lv_disp_get_ver_res(NULL);
  lv_coord_t meter_size = LV_MIN(disp_w, disp_h);
  if (meter_size > 20)
    meter_size -= 20;
  if (meter_size <= 0)
    meter_size = LV_MIN(disp_w, disp_h);
  lv_coord_t meter_radius = meter_size / 2;

  set_temp_arc = lv_arc_create(parent);
  lv_obj_set_size(set_temp_arc, meter_size, meter_size);
  lv_obj_align(set_temp_arc, LV_ALIGN_CENTER, 0, 0);
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
  lv_obj_align(current_temp_arc, LV_ALIGN_CENTER, 0, 0);
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
  lv_obj_align(current_pressure_arc, LV_ALIGN_CENTER, 0, 0);
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

  tick_layer = lv_obj_create(parent);
  lv_obj_set_size(tick_layer, meter_size, meter_size);
  lv_obj_align(tick_layer, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_opa(tick_layer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(tick_layer, 0, 0);
  lv_obj_add_event_cb(tick_layer, draw_ticks_cb, LV_EVENT_DRAW_POST, NULL);

  const lv_font_t *font_val = &lv_font_montserrat_40;
  const lv_font_t *font_units = &lv_font_montserrat_28;
  const lv_font_t *font_icon = &mdi_icons_40;

  static lv_coord_t row_cols[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  static lv_coord_t row_rows[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
  static lv_coord_t field_cols[] = {LV_GRID_CONTENT, LV_GRID_FR(1), LV_GRID_CONTENT,
                                    LV_GRID_TEMPLATE_LAST};
  static lv_coord_t field_rows[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

#define MAKE_FIELD(ROW, COL, ICON_TXT, OUT_ICON, OUT_VAL, OUT_UNITS, INIT_VAL_TXT, UNITS_TXT) \
  do                                                                                          \
  {                                                                                           \
    lv_obj_t *cell = lv_obj_create(ROW);                                                      \
    lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, 0);                                          \
    lv_obj_set_style_border_width(cell, 0, 0);                                                \
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);                                          \
    lv_obj_set_grid_dsc_array(cell, field_cols, field_rows);                                  \
    lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_STRETCH, (COL), 1, LV_GRID_ALIGN_CENTER, 0, 1);  \
    OUT_ICON = lv_label_create(cell);                                                         \
    lv_label_set_text(OUT_ICON, (ICON_TXT));                                                  \
    lv_obj_set_style_text_font(OUT_ICON, font_icon, 0);                                       \
    lv_obj_set_style_text_color(OUT_ICON, lv_color_white(), 0);                               \
    lv_obj_set_grid_cell(OUT_ICON, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);    \
    OUT_VAL = lv_label_create(cell);                                                          \
    lv_label_set_text(OUT_VAL, (INIT_VAL_TXT));                                               \
    lv_obj_set_style_text_font(OUT_VAL, font_val, 0);                                         \
    lv_obj_set_style_text_color(OUT_VAL, lv_color_white(), 0);                                \
    lv_obj_set_grid_cell(OUT_VAL, LV_GRID_ALIGN_END, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);       \
    OUT_UNITS = lv_label_create(cell);                                                        \
    lv_label_set_text(OUT_UNITS, (UNITS_TXT));                                                \
    lv_obj_set_style_text_font(OUT_UNITS, font_units, 0);                                     \
    lv_obj_set_style_text_color(OUT_UNITS, lv_color_white(), 0);                              \
    lv_obj_set_grid_cell(OUT_UNITS, LV_GRID_ALIGN_START, 2, 1, LV_GRID_ALIGN_CENTER, 0, 1);   \
  } while (0)

  lv_coord_t row_width = meter_size - 60;
  if (row_width <= 0)
    row_width = meter_size;
  if (row_width <= 0)
    row_width = LV_MIN(disp_w, disp_h);
  lv_coord_t top_margin = LV_MAX(current_arc_width, 60);
  if (top_margin > meter_radius)
    top_margin = meter_radius;
  lv_coord_t bottom_margin = LV_MAX(current_arc_width + 20, 110);
  if (bottom_margin > meter_radius)
    bottom_margin = meter_radius;

  lv_obj_t *row_bottom = lv_obj_create(parent);
  lv_obj_set_style_bg_opa(row_bottom, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row_bottom, 0, 0);
  lv_obj_clear_flag(row_bottom, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_grid_dsc_array(row_bottom, row_cols, row_rows);
  lv_obj_set_width(row_bottom, row_width);
  lv_obj_align(row_bottom, LV_ALIGN_CENTER, 0, meter_radius - bottom_margin);

  MAKE_FIELD(row_bottom, 0, MDI_CLOCK, shot_time_icon, shot_time_label,
             shot_time_units_label, "0.0", "s");
  MAKE_FIELD(row_bottom, 1, MDI_BEAKER, shot_volume_icon, shot_volume_label,
             shot_volume_units_label, "0.0", "ml");

  lv_obj_t *row_top = lv_obj_create(parent);
  lv_obj_set_style_bg_opa(row_top, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row_top, 0, 0);
  lv_obj_clear_flag(row_top, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_grid_dsc_array(row_top, row_cols, row_rows);
  lv_obj_set_width(row_top, row_width);
  lv_obj_align(row_top, LV_ALIGN_CENTER, 0, -meter_radius + top_margin);

  MAKE_FIELD(row_top, 0, MDI_THERMOMETER, temp_icon, temp_label, temp_units_label,
             "0.0", "°C");
  MAKE_FIELD(row_top, 1, MDI_GAUGE, pressure_icon, pressure_label,
             pressure_units_label, "0.0", "bar");

  lv_obj_move_foreground(row_bottom);
  lv_obj_move_foreground(row_top);

  if (!auto_step_timer)
    auto_step_timer = lv_timer_create(example1_increase_lvgl_tick, 100, NULL);
}

static lv_obj_t *create_menu_button(lv_obj_t *parent, const char *text,
                                    lv_event_cb_t cb)
{
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_width(btn, LV_PCT(80));
  lv_obj_set_height(btn, 54);
  lv_obj_set_style_border_width(btn, 0, 0);
  lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_GREY), 0);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *label = lv_label_create(btn);
  lv_label_set_text(label, text);
  lv_obj_center(label);
  return btn;
}

static void Menu_screen_create(void)
{
  template_init(&menu_template);
  menu_screen = menu_template.screen;
  template_set_back_handler(&menu_template, NULL, NULL);

  lv_obj_t *content = menu_template.content_area;
  lv_obj_set_style_pad_top(content, 120, 0);
  lv_obj_set_style_pad_bottom(content, 180, 0);
  lv_obj_set_style_pad_row(content, 16, 0);
  lv_obj_set_style_pad_column(content, 0, 0);
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_START);

  lv_obj_t *title = lv_label_create(content);
  lv_label_set_text(title, "Menu");
  lv_obj_add_style(title, &style_title, 0);

  create_menu_button(content, "Brew", menu_brew_event_cb);
  create_menu_button(content, "Steam", menu_steam_event_cb);
  create_menu_button(content, "Profiles", menu_profiles_event_cb);
  create_menu_button(content, "Settings", menu_settings_event_cb);
}

static void Steam_screen_create(void)
{
  template_init(&steam_template);
  steam_screen = steam_template.screen;
  template_set_back_handler(&steam_template, back_to_menu_event_cb, NULL);
}

static void Profiles_screen_create(void)
{
  template_init(&profiles_template);
  profiles_screen = profiles_template.screen;
  template_set_back_handler(&profiles_template, back_to_menu_event_cb, NULL);
}

static void Settings_screen_create(void)
{
  template_init(&settings_template);
  settings_screen = settings_template.screen;
  template_set_back_handler(&settings_template, back_to_menu_event_cb, NULL);
}

static void Standby_screen_create(void)
{
  template_init(&standby_template);
  standby_screen = standby_template.screen;
  template_set_back_handler(&standby_template, exit_standby_event_cb, NULL);

  lv_obj_t *content = standby_template.content_area;
  lv_obj_set_style_pad_top(content, 160, 0);
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  standby_time_label = lv_label_create(content);
  lv_label_set_text(standby_time_label, "--:--");
  lv_obj_set_style_text_font(standby_time_label, &lv_font_montserrat_40, 0);
}

static void menu_brew_event_cb(lv_event_t *e)
{
  (void)e;
  lv_scr_load(brew_screen);
}

static void menu_steam_event_cb(lv_event_t *e)
{
  (void)e;
  lv_scr_load(steam_screen);
}

static void menu_profiles_event_cb(lv_event_t *e)
{
  (void)e;
  lv_scr_load(profiles_screen);
}

static void menu_settings_event_cb(lv_event_t *e)
{
  (void)e;
  lv_scr_load(settings_screen);
}

static void back_to_menu_event_cb(lv_event_t *e)
{
  (void)e;
  lv_scr_load(menu_screen);
}

static void exit_standby_event_cb(lv_event_t *e)
{
  (void)e;
  LVGL_Exit_Standby();
}

static void update_standby_clock(void)
{
  if (!standby_time_label)
    return;

  time_t now = time(NULL);
  struct tm tm_info;
  localtime_r(&now, &tm_info);

  char buf[16];
  if (strftime(buf, sizeof buf, "%H:%M", &tm_info) == 0)
    strcpy(buf, "--:--");
  lv_label_set_text(standby_time_label, buf);
}

static void standby_timer_cb(lv_timer_t *t)
{
  (void)t;
  update_standby_clock();
}

void LVGL_Show_Standby(void)
{
  if (standby_active)
    return;

  last_non_standby_screen = lv_scr_act();
  standby_active = true;

  if (!standby_timer)
    standby_timer = lv_timer_create(standby_timer_cb, 1000, NULL);
  else
    lv_timer_resume(standby_timer);

  update_standby_clock();
  if (standby_screen)
    lv_scr_load(standby_screen);
}

void LVGL_Exit_Standby(void)
{
  if (!standby_active)
    return;

  standby_active = false;
  if (standby_timer)
    lv_timer_pause(standby_timer);

  lv_obj_t *target = last_non_standby_screen ? last_non_standby_screen : brew_screen;
  if (!target)
    target = brew_screen;
  if (target)
    lv_scr_load(target);
}

bool LVGL_Is_Standby_Active(void) { return standby_active; }

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
  bool steam = MQTT_GetSteamState();
  char buf[32];

  set_temp_val = set;
  heater_on = heater;

  if (tick_layer)
    lv_obj_invalidate(tick_layer);

  bool wifi_ok = Wireless_IsWiFiConnected();
  bool mqtt_ok = Wireless_IsMQTTConnected();
  bool espnow_active = Wireless_IsEspNowActive();
  bool espnow_link = Wireless_UsingEspNow();
  int esp_state = espnow_link ? 2 : (espnow_active ? 1 : 0);

  if (wifi_status_icon && wifi_ok != last_wifi_state)
  {
    lv_label_set_text(wifi_status_icon, wifi_ok ? MDI_WIFI_ON : MDI_WIFI_OFF);
    lv_color_t colour =
        lv_palette_main(wifi_ok ? LV_PALETTE_GREEN : LV_PALETTE_RED);
    lv_obj_set_style_text_color(wifi_status_icon, colour, 0);
    last_wifi_state = wifi_ok;
  }

  if (mqtt_status_icon && mqtt_ok != last_mqtt_state)
  {
    lv_label_set_text(mqtt_status_icon,
                      mqtt_ok ? MDI_MQTT_ON : MDI_MQTT_OFF);
    lv_color_t colour =
        lv_palette_main(mqtt_ok ? LV_PALETTE_GREEN : LV_PALETTE_RED);
    lv_obj_set_style_text_color(mqtt_status_icon, colour, 0);
    last_mqtt_state = mqtt_ok;
  }

  if (espnow_status_icon && esp_state != last_esp_state)
  {
    const char *icon = MDI_ESP_NOW_OFF;
    lv_color_t colour = lv_palette_main(LV_PALETTE_RED);
    if (esp_state == 1)
    {
      icon = MDI_ESP_NOW_PAIR;
      colour = lv_palette_main(LV_PALETTE_YELLOW);
    }
    else if (esp_state == 2)
    {
      icon = MDI_ESP_NOW_ON;
      colour = lv_palette_main(LV_PALETTE_GREEN);
    }
    lv_label_set_text(espnow_status_icon, icon);
    lv_obj_set_style_text_color(espnow_status_icon, colour, 0);
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
    snprintf(buf, sizeof buf, "%.1f", shot_time);
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

static int roller_get_int_value(lv_obj_t *roller)
{
  if (!roller)
    return 0;

  char buf[8];
  lv_roller_get_selected_str(roller, buf, sizeof buf);
  return atoi(buf);
}

static void buzzer_timer_cb(lv_timer_t *t)
{
  Buzzer_Off();
  lv_timer_del(t);
  buzzer_timer = NULL;
}

void LVGL_Backlight_adjustment(uint8_t Backlight) { Set_Backlight(Backlight); }

