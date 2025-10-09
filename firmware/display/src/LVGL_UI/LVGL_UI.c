#include "LVGL_UI.h"
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#ifdef CONFIG_ESP_TASK_WDT
#include "esp_task_wdt.h"
#endif
#include "version.h"
#include "Battery.h"

#define BREW_SETPOINT_MIN 87.0f
#define BREW_SETPOINT_MAX 97.0f
#define BREW_SETPOINT_STEP 0.5f
#define BREW_SETPOINT_DEFAULT 92.0f
#define STEAM_SETPOINT_MIN 145.0f
#define STEAM_SETPOINT_MAX 155.0f
#define STEAM_SETPOINT_STEP 1.0f
#define STEAM_SETPOINT_DEFAULT 152.0f
#define PRESSURE_SETPOINT_MIN 0.0f
#define PRESSURE_SETPOINT_MAX 12.0f
#define PRESSURE_SETPOINT_STEP 0.1f
#define PRESSURE_SETPOINT_DEFAULT 9.0f
#define PUMP_POWER_MIN 0.0f
#define PUMP_POWER_MAX 100.0f
#define PUMP_POWER_STEP 1.0f
#define PUMP_POWER_DEFAULT 95.0f

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
static void brew_button_event_cb(lv_event_t *e);
static void log_brew_button_diagnostics(lv_event_t *e);
static void open_menu_event_cb(lv_event_t *e);
static void draw_ticks_cb(lv_event_t *e);
static void init_tick_cache(void);
static lv_obj_t *create_comm_status_row(lv_obj_t *parent, lv_coord_t y_offset);
static lv_obj_t *create_menu_button(lv_obj_t *grid, uint8_t col, uint8_t row,
                                    const char *icon, const char *label);
static void add_version_label(lv_obj_t *parent);
static void __attribute__((unused)) shot_def_dd_event_cb(lv_event_t *e);
static void __attribute__((unused)) beep_on_shot_btn_event_cb(lv_event_t *e);
static void buzzer_timer_cb(lv_timer_t *t);
static int roller_get_int_value(lv_obj_t *roller);
static float roller_get_float_value(lv_obj_t *roller);
static void load_screen(lv_obj_t *screen);
static void update_standby_time(void);
static void standby_timer_cb(lv_timer_t *t);
static bool uk_is_bst_active(const struct tm *utc_tm);
static bool is_leap_year(int year);
static int days_in_month(int year, int month);
static int day_of_week(int year, int month, int day);
static int last_sunday_of_month(int year, int month);
static lv_obj_t *create_settings_row(lv_obj_t *parent, const char *label);
static void build_roller_options(char *buf, size_t buf_size, float min, float max, float step,
                                 uint8_t decimals);
static void set_switch_state(lv_obj_t *sw, bool enabled);
static void set_roller_value(lv_obj_t *roller, float value, float min, float max, float step);
static void settings_sync_from_state(void);
static void settings_update_pump_controls(bool pressure_mode);
static void heater_switch_event_cb(lv_event_t *e);
static void steam_switch_event_cb(lv_event_t *e);
static void brew_setpoint_event_cb(lv_event_t *e);
static void steam_setpoint_event_cb(lv_event_t *e);
static void pump_pressure_mode_event_cb(lv_event_t *e);
static void pressure_setpoint_event_cb(lv_event_t *e);
static void pump_power_event_cb(lv_event_t *e);
/**********************
 *  STATIC VARIABLES
 **********************/
static const char *TAG_UI = "UI";

static disp_size_t disp_size;

lv_style_t style_text_muted;
lv_style_t style_title;
static lv_style_t style_icon;
static lv_style_t style_bullet;

static const lv_font_t *font_large;
static const lv_font_t *font_normal;

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
static lv_obj_t *heater_switch;
static lv_obj_t *steam_switch;
static lv_obj_t *brew_setpoint_roller;
static lv_obj_t *steam_setpoint_roller;
static lv_obj_t *pump_pressure_switch;
static lv_obj_t *pressure_setpoint_roller;
static lv_obj_t *pump_power_roller;
static lv_obj_t *pressure_row;
static lv_obj_t *pump_power_row;
static bool s_syncing_settings_controls;
static char brew_setpoint_options[256];
static char steam_setpoint_options[256];
static char pressure_setpoint_options[1024];
static char pump_power_options[512];
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

typedef struct
{
  float cosv;
  float sinv;
  char label[8];
} tick_cache_t;

#define TEMP_TICK_COUNT ((TEMP_ARC_MAX - TEMP_ARC_MIN) / TEMP_ARC_TICK + 1)
#define PRESSURE_TICK_COUNT ((PRESSURE_ARC_MAX - PRESSURE_ARC_MIN) / PRESSURE_ARC_TICK + 1)

static tick_cache_t temp_ticks[TEMP_TICK_COUNT];
static size_t temp_tick_count;
static tick_cache_t pressure_ticks[PRESSURE_TICK_COUNT];
static size_t pressure_tick_count;
static float temp_angle_scale;
static float pressure_angle_scale;

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

static lv_obj_t *create_settings_row(lv_obj_t *parent, const char *label)
{
  if (!parent || !label)
    return NULL;

  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_remove_style_all(row);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_style_height(row, LV_SIZE_CONTENT, 0);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_style_pad_column(row, 24, 0);
  lv_obj_set_style_text_color(row, lv_color_white(), 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  lv_obj_t *lbl = lv_label_create(row);
  lv_label_set_text(lbl, label);
  lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
  lv_obj_set_style_text_font(lbl, font_normal, 0);
  lv_obj_set_flex_grow(lbl, 1);

  return row;
}

static bool append_char_to_buffer(char *buf, size_t buf_size, size_t *offset, char c)
{
  if (!buf || !offset || buf_size == 0)
    return false;

  if (*offset + 1 >= buf_size)
  {
    buf[buf_size - 1] = '\0';
    return false;
  }

  buf[*offset] = c;
  (*offset)++;
  buf[*offset] = '\0';
  return true;
}

static bool append_unsigned_to_buffer(char *buf, size_t buf_size, size_t *offset, unsigned long value)
{
  char digits[21];
  size_t len = 0;

  do
  {
    digits[len++] = (char)('0' + (value % 10));
    value /= 10;
  } while (value > 0 && len < sizeof(digits));

  while (len > 0)
  {
    if (!append_char_to_buffer(buf, buf_size, offset, digits[--len]))
      return false;
  }

  return true;
}

static bool append_fraction_to_buffer(char *buf, size_t buf_size, size_t *offset, unsigned long fraction,
                                      unsigned long scale, uint8_t decimals)
{
  if (decimals == 0)
    return true;

  if (!append_char_to_buffer(buf, buf_size, offset, '.'))
    return false;

  unsigned long divisor = scale;
  for (uint8_t i = 0; i < decimals; ++i)
  {
    if (divisor > 1)
      divisor /= 10;
    else
      divisor = 1;

    unsigned long digit = divisor ? fraction / divisor : 0;
    fraction = divisor ? fraction % divisor : 0;

    if (!append_char_to_buffer(buf, buf_size, offset, (char)('0' + digit)))
      return false;
  }

  return true;
}

static void build_roller_options(char *buf, size_t buf_size, float min, float max,
                                 float step, uint8_t decimals)
{
  if (!buf || buf_size == 0 || step <= 0.0f)
    return;

  buf[0] = '\0';
  size_t offset = 0;
  unsigned long scale = 1;
  for (uint8_t d = 0; d < decimals; ++d)
    scale *= 10;

  long min_scaled = lround((double)min * scale);
  long max_scaled = lround((double)max * scale);
  long step_scaled = lround((double)step * scale);
  if (step_scaled <= 0)
    return;

  for (long value_scaled = min_scaled;; value_scaled += step_scaled)
  {
    if (value_scaled > max_scaled)
      value_scaled = max_scaled;

    bool is_last = value_scaled >= max_scaled;
    bool negative = value_scaled < 0;
    unsigned long abs_value = (unsigned long)(negative ? -value_scaled : value_scaled);
    unsigned long integer_part = abs_value / scale;
    unsigned long fractional_part = abs_value % scale;

    if (negative && !append_char_to_buffer(buf, buf_size, &offset, '-'))
      break;

    if (!append_unsigned_to_buffer(buf, buf_size, &offset, integer_part))
      break;

    if (!append_fraction_to_buffer(buf, buf_size, &offset, fractional_part, scale, decimals))
      break;

    if (!is_last)
    {
      if (!append_char_to_buffer(buf, buf_size, &offset, '\n'))
        break;
    }
    else
    {
      if (offset < buf_size)
        buf[offset] = '\0';
      break;
    }
  }
}

static void set_switch_state(lv_obj_t *sw, bool enabled)
{
  if (!sw)
    return;
  if (enabled)
    lv_obj_add_state(sw, LV_STATE_CHECKED);
  else
    lv_obj_clear_state(sw, LV_STATE_CHECKED);
}

static void set_roller_value(lv_obj_t *roller, float value, float min, float max,
                             float step)
{
  if (!roller || step <= 0.0f)
    return;
  if (value < min)
    value = min;
  else if (value > max)
    value = max;
  int max_index = (int)roundf((max - min) / step);
  int index = (int)roundf((value - min) / step);
  if (index < 0)
    index = 0;
  if (index > max_index)
    index = max_index;
  lv_roller_set_selected(roller, index, LV_ANIM_OFF);
}

static float roller_get_float_value(lv_obj_t *roller)
{
  if (!roller)
    return NAN;
  char buf[16];
  lv_roller_get_selected_str(roller, buf, sizeof buf);
  return strtof(buf, NULL);
}

static void settings_update_pump_controls(bool pressure_mode)
{
  if (!pressure_row || !pump_power_row)
    return;
  if (pressure_mode)
  {
    lv_obj_clear_flag(pressure_row, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pump_power_row, LV_OBJ_FLAG_HIDDEN);
  }
  else
  {
    lv_obj_clear_flag(pump_power_row, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pressure_row, LV_OBJ_FLAG_HIDDEN);
  }
}

static void settings_sync_from_state(void)
{
  if (!settings_scr)
    return;

  bool heater = MQTT_GetHeaterState();
  bool steam = MQTT_GetSteamState();
  bool pump_pressure_mode = MQTT_GetPumpPressureMode();
  float brew = MQTT_GetBrewSetpoint();
  float steam_set = MQTT_GetSteamSetpoint();
  float pressure = MQTT_GetSetPressure();
  float pump_power = MQTT_GetPumpPower();

  if (isnan(brew))
    brew = BREW_SETPOINT_DEFAULT;
  if (isnan(steam_set))
    steam_set = STEAM_SETPOINT_DEFAULT;
  if (isnan(pressure))
    pressure = PRESSURE_SETPOINT_DEFAULT;
  if (isnan(pump_power))
    pump_power = PUMP_POWER_DEFAULT;

  s_syncing_settings_controls = true;

  set_switch_state(heater_switch, heater);
  set_switch_state(steam_switch, steam);
  set_switch_state(pump_pressure_switch, pump_pressure_mode);

  set_roller_value(brew_setpoint_roller, brew, BREW_SETPOINT_MIN,
                   BREW_SETPOINT_MAX, BREW_SETPOINT_STEP);
  set_roller_value(steam_setpoint_roller, steam_set, STEAM_SETPOINT_MIN,
                   STEAM_SETPOINT_MAX, STEAM_SETPOINT_STEP);
  set_roller_value(pressure_setpoint_roller, pressure, PRESSURE_SETPOINT_MIN,
                   PRESSURE_SETPOINT_MAX, PRESSURE_SETPOINT_STEP);
  set_roller_value(pump_power_roller, pump_power, PUMP_POWER_MIN,
                   PUMP_POWER_MAX, PUMP_POWER_STEP);

  settings_update_pump_controls(pump_pressure_mode);

  s_syncing_settings_controls = false;
}

static void heater_switch_event_cb(lv_event_t *e)
{
  if (s_syncing_settings_controls)
    return;
  lv_obj_t *sw = lv_event_get_target(e);
  bool enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
  MQTT_SetHeaterState(enabled);
  settings_sync_from_state();
}

static void steam_switch_event_cb(lv_event_t *e)
{
  if (s_syncing_settings_controls)
    return;
  lv_obj_t *sw = lv_event_get_target(e);
  bool enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
  MQTT_SetSteamState(enabled);
  settings_sync_from_state();
}

static void brew_setpoint_event_cb(lv_event_t *e)
{
  if (s_syncing_settings_controls)
    return;
  float value = roller_get_float_value(lv_event_get_target(e));
  if (!isnan(value))
  {
    MQTT_SetBrewSetpoint(value);
    settings_sync_from_state();
  }
}

static void steam_setpoint_event_cb(lv_event_t *e)
{
  if (s_syncing_settings_controls)
    return;
  float value = roller_get_float_value(lv_event_get_target(e));
  if (!isnan(value))
  {
    MQTT_SetSteamSetpoint(value);
    settings_sync_from_state();
  }
}

static void pump_pressure_mode_event_cb(lv_event_t *e)
{
  if (s_syncing_settings_controls)
    return;
  lv_obj_t *sw = lv_event_get_target(e);
  bool enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
  MQTT_SetPumpPressureMode(enabled);
  settings_update_pump_controls(enabled);
  settings_sync_from_state();
}

static void pressure_setpoint_event_cb(lv_event_t *e)
{
  if (s_syncing_settings_controls)
    return;
  float value = roller_get_float_value(lv_event_get_target(e));
  if (!isnan(value))
  {
    MQTT_SetPressureSetpoint(value);
    settings_sync_from_state();
  }
}

static void pump_power_event_cb(lv_event_t *e)
{
  if (s_syncing_settings_controls)
    return;
  float value = roller_get_float_value(lv_event_get_target(e));
  if (!isnan(value))
  {
    MQTT_SetPumpPower(value);
    settings_sync_from_state();
  }
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
  lv_obj_set_flex_flow(settings_scr, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(settings_scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  lv_obj_t *title_label = lv_label_create(settings_scr);
  lv_label_set_text(title_label, "Settings");
  lv_obj_add_style(title_label, &style_title, 0);
  lv_obj_set_style_text_color(title_label, lv_color_white(), 0);

  lv_obj_t *content = lv_obj_create(settings_scr);
  lv_obj_remove_style_all(content);
  lv_obj_set_width(content, LV_PCT(100));
  lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(content, 0, 0);
  lv_obj_set_style_pad_all(content, 0, 0);
  lv_obj_set_style_pad_row(content, 24, 0);
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  lv_obj_t *heater_row = create_settings_row(content, "Heater");
  heater_switch = lv_switch_create(heater_row);
  lv_obj_add_event_cb(heater_switch, heater_switch_event_cb,
                      LV_EVENT_VALUE_CHANGED, NULL);

  lv_obj_t *steam_row = create_settings_row(content, "Steam");
  steam_switch = lv_switch_create(steam_row);
  lv_obj_add_event_cb(steam_switch, steam_switch_event_cb,
                      LV_EVENT_VALUE_CHANGED, NULL);

  build_roller_options(brew_setpoint_options, sizeof brew_setpoint_options,
                       BREW_SETPOINT_MIN, BREW_SETPOINT_MAX,
                       BREW_SETPOINT_STEP, 1);
  lv_obj_t *brew_row = create_settings_row(content, "Brew setpoint (°C)");
  brew_setpoint_roller = lv_roller_create(brew_row);
  lv_roller_set_options(brew_setpoint_roller, brew_setpoint_options,
                        LV_ROLLER_MODE_NORMAL);
  lv_roller_set_visible_row_count(brew_setpoint_roller, 3);
  lv_obj_set_width(brew_setpoint_roller, 120);
  lv_obj_set_style_bg_opa(brew_setpoint_roller, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_text_color(brew_setpoint_roller, lv_color_white(),
                              LV_PART_MAIN);
  lv_obj_set_style_text_color(brew_setpoint_roller, lv_color_white(),
                              LV_PART_SELECTED);
  lv_obj_add_event_cb(brew_setpoint_roller, brew_setpoint_event_cb,
                      LV_EVENT_VALUE_CHANGED, NULL);

  build_roller_options(steam_setpoint_options, sizeof steam_setpoint_options,
                       STEAM_SETPOINT_MIN, STEAM_SETPOINT_MAX,
                       STEAM_SETPOINT_STEP, 0);
  lv_obj_t *steam_set_row = create_settings_row(content, "Steam setpoint (°C)");
  steam_setpoint_roller = lv_roller_create(steam_set_row);
  lv_roller_set_options(steam_setpoint_roller, steam_setpoint_options,
                        LV_ROLLER_MODE_NORMAL);
  lv_roller_set_visible_row_count(steam_setpoint_roller, 3);
  lv_obj_set_width(steam_setpoint_roller, 120);
  lv_obj_set_style_bg_opa(steam_setpoint_roller, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_text_color(steam_setpoint_roller, lv_color_white(),
                              LV_PART_MAIN);
  lv_obj_set_style_text_color(steam_setpoint_roller, lv_color_white(),
                              LV_PART_SELECTED);
  lv_obj_add_event_cb(steam_setpoint_roller, steam_setpoint_event_cb,
                      LV_EVENT_VALUE_CHANGED, NULL);

  lv_obj_t *pump_mode_row = create_settings_row(content, "Pump pressure mode");
  pump_pressure_switch = lv_switch_create(pump_mode_row);
  lv_obj_add_event_cb(pump_pressure_switch, pump_pressure_mode_event_cb,
                      LV_EVENT_VALUE_CHANGED, NULL);

  build_roller_options(pressure_setpoint_options, sizeof pressure_setpoint_options,
                       PRESSURE_SETPOINT_MIN, PRESSURE_SETPOINT_MAX,
                       PRESSURE_SETPOINT_STEP, 1);
  pressure_row = create_settings_row(content, "Pressure setpoint (bar)");
  pressure_setpoint_roller = lv_roller_create(pressure_row);
  lv_roller_set_options(pressure_setpoint_roller, pressure_setpoint_options,
                        LV_ROLLER_MODE_NORMAL);
  lv_roller_set_visible_row_count(pressure_setpoint_roller, 3);
  lv_obj_set_width(pressure_setpoint_roller, 120);
  lv_obj_set_style_bg_opa(pressure_setpoint_roller, LV_OPA_TRANSP,
                          LV_PART_MAIN);
  lv_obj_set_style_text_color(pressure_setpoint_roller, lv_color_white(),
                              LV_PART_MAIN);
  lv_obj_set_style_text_color(pressure_setpoint_roller, lv_color_white(),
                              LV_PART_SELECTED);
  lv_obj_add_event_cb(pressure_setpoint_roller, pressure_setpoint_event_cb,
                      LV_EVENT_VALUE_CHANGED, NULL);

  build_roller_options(pump_power_options, sizeof pump_power_options,
                       PUMP_POWER_MIN, PUMP_POWER_MAX, PUMP_POWER_STEP, 0);
  pump_power_row = create_settings_row(content, "Pump power (%)");
  pump_power_roller = lv_roller_create(pump_power_row);
  lv_roller_set_options(pump_power_roller, pump_power_options,
                        LV_ROLLER_MODE_NORMAL);
  lv_roller_set_visible_row_count(pump_power_roller, 3);
  lv_obj_set_width(pump_power_roller, 120);
  lv_obj_set_style_bg_opa(pump_power_roller, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_text_color(pump_power_roller, lv_color_white(),
                              LV_PART_MAIN);
  lv_obj_set_style_text_color(pump_power_roller, lv_color_white(),
                              LV_PART_SELECTED);
  lv_obj_add_event_cb(pump_power_roller, pump_power_event_cb,
                      LV_EVENT_VALUE_CHANGED, NULL);

  lv_obj_t *back_btn = lv_btn_create(settings_scr);
  lv_obj_set_size(back_btn, 160, 70);
  lv_obj_set_style_border_width(back_btn, 0, 0);
  lv_obj_set_style_bg_color(back_btn, lv_palette_main(LV_PALETTE_GREY), 0);
  lv_obj_add_event_cb(back_btn, open_menu_event_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *back_label = lv_label_create(back_btn);
  lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");
  lv_obj_center(back_label);

  Backlight_slider = NULL;
  beep_on_shot_btn = NULL;
  beep_on_shot_label = NULL;
  shot_def_dd = NULL;
  shot_duration_label = NULL;
  shot_duration_roller = NULL;
  shot_volume_label = NULL;
  shot_volume_roller = NULL;

  settings_sync_from_state();
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
  lv_obj_add_event_cb(brew_btn, brew_button_event_cb, LV_EVENT_CLICKED, brew_screen);

  lv_obj_t *steam_btn = create_menu_button(button_grid, 1, 0, MDI_STEAM, "Steam");
  lv_obj_add_event_cb(steam_btn, open_screen_event_cb, LV_EVENT_CLICKED, steam_screen);

  lv_obj_t *profiles_btn = create_menu_button(button_grid, 0, 1, MDI_MENU, "Profiles");
  lv_obj_add_event_cb(profiles_btn, open_screen_event_cb, LV_EVENT_CLICKED, profiles_screen);

  lv_obj_t *settings_btn = create_menu_button(button_grid, 1, 1, MDI_COG, "Settings");
  lv_obj_add_event_cb(settings_btn, open_settings_event_cb, LV_EVENT_CLICKED, NULL);

  add_version_label(menu_screen);
}

static void log_brew_button_diagnostics(lv_event_t *e)
{
  lv_obj_t *target = lv_event_get_target(e);
  lv_obj_t *user_data = lv_event_get_user_data(e);
  size_t free_heap = esp_get_free_heap_size();
  size_t min_free_heap = esp_get_minimum_free_heap_size();
  UBaseType_t stack_high_water_mark = uxTaskGetStackHighWaterMark(NULL);

  ESP_LOGI(TAG_UI,
           "Brew button pressed (event=%d target=%p user_data=%p)",
           (int)lv_event_get_code(e), (void *)target, (void *)user_data);
  ESP_LOGI(TAG_UI,
           "Heap free=%zu bytes (minimum=%zu), UI task stack high watermark=%u words",
           free_heap, min_free_heap, (unsigned)stack_high_water_mark);

#ifdef CONFIG_ESP_TASK_WDT
  esp_err_t wdt_status = esp_task_wdt_status(NULL);
  if (wdt_status == ESP_OK)
  {
    ESP_LOGI(TAG_UI, "Task WDT status: registered and healthy");
  }
  else if (wdt_status == ESP_ERR_NOT_FOUND)
  {
    ESP_LOGW(TAG_UI, "Task WDT status: UI task not registered");
  }
  else
  {
    ESP_LOGW(TAG_UI, "Task WDT status error: %s", esp_err_to_name(wdt_status));
  }
#endif
}

static void brew_button_event_cb(lv_event_t *e)
{
  log_brew_button_diagnostics(e);

  lv_obj_t *target = lv_event_get_user_data(e);
  if (target)
  {
    load_screen(target);
  }
  else
  {
    ESP_LOGW(TAG_UI, "Brew button pressed without associated screen");
  }
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
  steam_switch = NULL;
  brew_setpoint_roller = NULL;
  steam_setpoint_roller = NULL;
  pump_pressure_switch = NULL;
  pressure_setpoint_roller = NULL;
  pump_power_roller = NULL;
  pressure_row = NULL;
  pump_power_row = NULL;
  s_syncing_settings_controls = false;
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
  init_tick_cache();

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
  MAKE_FIELD(row_bottom, 0, MDI_CLOCK, shot_time_icon, shot_time_label, shot_time_units_label, "0.0", "s");
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

  /* UI updates are driven from the main application task instead of an LVGL timer. */
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
    lv_coord_t len = 20;
    lv_coord_t text_r = radius - len - 10;

    for (size_t i = 0; i < temp_tick_count; ++i)
    {
      const tick_cache_t *tick = &temp_ticks[i];

      lv_point_t p1 = {cx + (lv_coord_t)((radius - len) * tick->cosv),
                       cy + (lv_coord_t)((radius - len) * tick->sinv)};
      lv_point_t p2 = {cx + (lv_coord_t)(radius * tick->cosv),
                       cy + (lv_coord_t)(radius * tick->sinv)};

      lv_draw_line(draw_ctx, &line_dsc, &p1, &p2);

      lv_point_t tp = {cx + (lv_coord_t)(text_r * tick->cosv),
                       cy + (lv_coord_t)(text_r * tick->sinv)};
      lv_area_t a = {tp.x - 20, tp.y - 10, tp.x + 20, tp.y + 10};
      lv_draw_label(draw_ctx, &label_dsc, &a, tick->label, NULL);
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
        float angle = TEMP_ARC_START + (v - TEMP_ARC_MIN) * temp_angle_scale;
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
    lv_coord_t len = 20;
    lv_coord_t text_r = radius - len - 10;

    for (size_t i = 0; i < pressure_tick_count; ++i)
    {
      const tick_cache_t *tick = &pressure_ticks[i];

      lv_point_t p1 = {cx + (lv_coord_t)((radius - len) * tick->cosv),
                       cy + (lv_coord_t)((radius - len) * tick->sinv)};
      lv_point_t p2 = {cx + (lv_coord_t)(radius * tick->cosv),
                       cy + (lv_coord_t)(radius * tick->sinv)};

      lv_draw_line(draw_ctx, &line_dsc, &p1, &p2);

      lv_point_t tp = {cx + (lv_coord_t)(text_r * tick->cosv),
                       cy + (lv_coord_t)(text_r * tick->sinv)};
      lv_area_t a = {tp.x - 20, tp.y - 10, tp.x + 20, tp.y + 10};
      lv_draw_label(draw_ctx, &label_dsc, &a, tick->label, NULL);
    }
  }
}

static void init_tick_cache(void)
{
  temp_tick_count = 0;
  pressure_tick_count = 0;

  if (TEMP_ARC_MAX > TEMP_ARC_MIN)
    temp_angle_scale = (float)TEMP_ARC_SIZE / (float)(TEMP_ARC_MAX - TEMP_ARC_MIN);
  else
    temp_angle_scale = 0.0f;

  if (PRESSURE_ARC_MAX > PRESSURE_ARC_MIN)
    pressure_angle_scale = (float)PRESSURE_ARC_SIZE / (float)(PRESSURE_ARC_MAX - PRESSURE_ARC_MIN);
  else
    pressure_angle_scale = 0.0f;

  for (int val = TEMP_ARC_MIN;
       val <= TEMP_ARC_MAX && temp_tick_count < (sizeof temp_ticks / sizeof temp_ticks[0]);
       val += TEMP_ARC_TICK)
  {
    float angle = TEMP_ARC_START + (val - TEMP_ARC_MIN) * temp_angle_scale;
    float rad = angle * 3.14159265f / 180.0f;
    temp_ticks[temp_tick_count].cosv = cosf(rad);
    temp_ticks[temp_tick_count].sinv = sinf(rad);
    lv_snprintf(temp_ticks[temp_tick_count].label, sizeof(temp_ticks[temp_tick_count].label), "%d", val);
    temp_tick_count++;
  }

  for (int val = PRESSURE_ARC_MIN;
       val <= PRESSURE_ARC_MAX && pressure_tick_count < (sizeof pressure_ticks / sizeof pressure_ticks[0]);
       val += PRESSURE_ARC_TICK)
  {
    float angle = PRESSURE_ARC_START + PRESSURE_ARC_SIZE -
                  (val - PRESSURE_ARC_MIN) * pressure_angle_scale;
    float rad = angle * 3.14159265f / 180.0f;
    pressure_ticks[pressure_tick_count].cosv = cosf(rad);
    pressure_ticks[pressure_tick_count].sinv = sinf(rad);
    lv_snprintf(pressure_ticks[pressure_tick_count].label,
                sizeof(pressure_ticks[pressure_tick_count].label), "%d", val / 10);
    pressure_tick_count++;
  }
}

void LVGL_UI_Update(void)
{
  float current = MQTT_GetCurrentTemp();
  float set = MQTT_GetSetTemp();
  float current_p = MQTT_GetCurrentPressure();
  float shot_time = MQTT_GetShotTime();
  float shot_vol = MQTT_GetShotVolume();
  bool heater = MQTT_GetHeaterState();
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

  settings_sync_from_state();

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

static void __attribute__((unused)) shot_def_dd_event_cb(lv_event_t *e)
{
  uint16_t sel = lv_dropdown_get_selected(lv_event_get_target(e));
  switch (sel)
  {
  case 1: /* Time */
    lv_obj_clear_flag(shot_duration_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(shot_duration_roller, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(shot_volume_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(shot_volume_roller, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(beep_on_shot_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(beep_on_shot_btn, LV_OBJ_FLAG_HIDDEN);
    break;
  case 2: /* Volume */
    lv_obj_clear_flag(shot_volume_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(shot_volume_roller, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(shot_duration_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(shot_duration_roller, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(beep_on_shot_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(beep_on_shot_btn, LV_OBJ_FLAG_HIDDEN);
    break;
  default:
    lv_obj_add_flag(shot_duration_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(shot_duration_roller, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(shot_volume_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(shot_volume_roller, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(beep_on_shot_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(beep_on_shot_btn, LV_OBJ_FLAG_HIDDEN);
    break;
  }
}

static int roller_get_int_value(lv_obj_t *roller)
{
  if (!roller)
    return 0;

  char buf[8];
  lv_roller_get_selected_str(roller, buf, sizeof buf);
  return atoi(buf);
}

static void __attribute__((unused)) beep_on_shot_btn_event_cb(lv_event_t *e)
{
  lv_obj_t *btn = lv_event_get_target(e);
  lv_obj_t *label = lv_obj_get_child(btn, 0);
  if (lv_obj_has_state(btn, LV_STATE_CHECKED))
  {
    lv_label_set_text(label, "On");
  }
  else
  {
    lv_label_set_text(label, "Off");
  }
}

static void buzzer_timer_cb(lv_timer_t *t)
{
  Buzzer_Off();
  lv_timer_del(t);
  buzzer_timer = NULL;
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

    lv_obj_t *title = lv_label_create(standby_screen);
    lv_label_set_text(title, "Standby");
    lv_obj_add_style(title, &style_title, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

    standby_time_label = lv_label_create(standby_screen);
    lv_obj_add_style(standby_time_label, &style_title, 0);
    lv_obj_set_style_text_font(standby_time_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(standby_time_label, lv_color_white(), 0);
    const uint16_t zoom = (uint16_t)((80 * LV_IMG_ZOOM_NONE + 24) / 48);
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
