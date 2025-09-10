#include "LVGL_Example.h"
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

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
static void open_settings_event_cb(lv_event_t *e);
static void back_event_cb(lv_event_t *e);
static void draw_ticks_cb(lv_event_t *e);
static void set_label_value(lv_obj_t *label, float value, const char *suffix);
static void heater_event_cb(lv_event_t *e);

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

static lv_obj_t *main_screen;
static lv_obj_t *settings_scr;
static lv_obj_t *heater_btn;
static lv_obj_t *steam_btn;
static lv_obj_t *settings_btn;
static lv_coord_t tab_h_global;

static lv_obj_t *current_temp_arc;
static lv_obj_t *set_temp_arc;
static lv_obj_t *current_pressure_arc;
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
static lv_obj_t *conn_label;
static int last_conn_type = -1;

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

  lv_style_init(&style_title);
  lv_style_set_text_font(&style_title, font_large);

  lv_style_init(&style_icon);
  lv_style_set_text_color(&style_icon, lv_theme_get_color_primary(NULL));
  lv_style_set_text_font(&style_icon, font_large);

  lv_style_init(&style_bullet);
  lv_style_set_border_width(&style_bullet, 0);
  lv_style_set_radius(&style_bullet, LV_RADIUS_CIRCLE);

  main_screen = lv_scr_act();
  lv_obj_set_style_bg_color(main_screen, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(main_screen, LV_OPA_COVER, 0);
  settings_scr = NULL;
  Backlight_slider = NULL;

  lv_obj_set_style_text_font(lv_scr_act(), font_normal, 0);

  if (disp_size == DISP_LARGE)
  {
    /* Large displays do not require additional header content. */
  }

  Status_create(main_screen);
}

static void led_event_cb(lv_event_t *e)
{
  lv_obj_t *led = (lv_obj_t *)lv_event_get_user_data(e);
  lv_obj_t *sw = lv_event_get_target(e);
  if (lv_obj_get_state(sw) & LV_STATE_CHECKED)
  {
    lv_led_on(led);
    Buzzer_On();
  }
  else
  {
    lv_led_off(led);
    Buzzer_Off();
  }
}

static void heater_event_cb(lv_event_t *e)
{
  bool heater = MQTT_GetHeaterState();
  MQTT_SetHeaterState(!heater);
}

static void back_event_cb(lv_event_t *e) { lv_scr_load(main_screen); }

static void open_settings_event_cb(lv_event_t *e)
{
  if (!settings_scr)
    Settings_create();
  lv_scr_load(settings_scr);
}

static void Settings_create(void)
{
  settings_scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(settings_scr, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(settings_scr, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(settings_scr, 0, 0);

  static lv_coord_t grid_main_col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1),
                                           LV_GRID_TEMPLATE_LAST};
  static lv_coord_t grid_main_row_dsc[] = {
      LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT,
      LV_GRID_FR(1), LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(settings_scr, grid_main_col_dsc, grid_main_row_dsc);

  lv_obj_t *back_btn = lv_btn_create(settings_scr);
  lv_obj_set_size(back_btn, 80, 80);
  lv_obj_set_grid_cell(back_btn, LV_GRID_ALIGN_CENTER, 0, 2,
                       LV_GRID_ALIGN_CENTER, 5, 1);
  lv_obj_t *back_label = lv_label_create(back_btn);
  lv_label_set_text(back_label, LV_SYMBOL_LEFT);
  lv_obj_center(back_label);
  lv_obj_add_event_cb(back_btn, back_event_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t *Backlight_label = lv_label_create(settings_scr);
  lv_label_set_text(Backlight_label, "Backlight brightness");
  lv_obj_add_style(Backlight_label, &style_text_muted, 0);
  lv_obj_set_grid_cell(Backlight_label, LV_GRID_ALIGN_CENTER, 0, 2,
                       LV_GRID_ALIGN_START, 0, 1);

  Backlight_slider = lv_slider_create(settings_scr);
  lv_obj_add_flag(Backlight_slider, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(Backlight_slider, 200, 35);
  lv_obj_set_style_radius(Backlight_slider, 3, LV_PART_KNOB);
  lv_obj_set_style_bg_opa(Backlight_slider, LV_OPA_TRANSP, LV_PART_KNOB);
  lv_obj_set_style_bg_color(Backlight_slider, lv_color_hex(0xAAAAAA),
                            LV_PART_KNOB);
  lv_obj_set_style_bg_color(Backlight_slider, lv_color_hex(0xFFFFFF),
                            LV_PART_INDICATOR);
  lv_obj_set_style_outline_width(Backlight_slider, 2, LV_PART_INDICATOR);
  lv_obj_set_style_outline_color(Backlight_slider, lv_color_hex(0xD3D3D3),
                                 LV_PART_INDICATOR);
  lv_slider_set_range(Backlight_slider, 5, Backlight_MAX);
  lv_slider_set_value(Backlight_slider, LCD_Backlight, LV_ANIM_ON);
  lv_obj_add_event_cb(Backlight_slider, Backlight_adjustment_event_cb,
                      LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_set_grid_cell(Backlight_slider, LV_GRID_ALIGN_CENTER, 0, 2,
                       LV_GRID_ALIGN_START, 1, 1);

  lv_obj_t *panel2_title = lv_label_create(settings_scr);
  lv_label_set_text(panel2_title, "The buzzer tes");
  lv_obj_add_style(panel2_title, &style_title, 0);
  lv_obj_set_grid_cell(panel2_title, LV_GRID_ALIGN_CENTER, 0, 2,
                       LV_GRID_ALIGN_START, 2, 1);

  lv_obj_t *led = lv_led_create(settings_scr);
  lv_obj_set_size(led, 50, 50);
  lv_led_off(led);
  lv_obj_set_grid_cell(led, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_START, 3,
                       1);

  lv_obj_t *sw = lv_switch_create(settings_scr);
  lv_obj_set_size(sw, 65, 40);
  lv_obj_add_event_cb(sw, led_event_cb, LV_EVENT_VALUE_CHANGED, led);
  lv_obj_set_grid_cell(sw, LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_START, 3,
                       1);
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

  conn_label = lv_label_create(parent);
#if LV_FONT_MONTSERRAT_24
  lv_obj_set_style_text_font(conn_label, &lv_font_montserrat_24, 0);
#else
  lv_obj_set_style_text_font(conn_label, LV_FONT_DEFAULT, 0);
#endif
  lv_obj_align(conn_label, LV_ALIGN_TOP_MID, 0, 0);
  lv_label_set_text(conn_label, "");

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
  lv_obj_t *tick_layer = lv_obj_create(parent);
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
  const lv_coord_t W = lv_disp_get_hor_res(NULL);

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

  /* ----------------- Buttons @ 70% with 1% spacing ----------------- */
  lv_obj_t *ctrl_container = lv_obj_create(parent);
  lv_obj_set_style_bg_opa(ctrl_container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(ctrl_container, 0, 0);
  lv_obj_clear_flag(ctrl_container, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(ctrl_container, LV_SIZE_CONTENT, 80);

  static lv_coord_t btn_cols[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
  static lv_coord_t btn_rows[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  lv_obj_set_grid_dsc_array(ctrl_container, btn_cols, btn_rows);
  lv_obj_set_style_pad_column(ctrl_container, W / 100, 0);          /* 1% spacing */
  lv_obj_align(ctrl_container, LV_ALIGN_CENTER, 0, (H * 20) / 100); /* 70% */

  heater_btn = lv_btn_create(ctrl_container);
  lv_obj_set_size(heater_btn, 80, 80);
  lv_obj_set_style_border_width(heater_btn, 0, 0);
  lv_obj_set_style_bg_color(heater_btn, lv_palette_main(LV_PALETTE_GREY), 0);
  lv_obj_set_grid_cell(heater_btn, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);
  lv_obj_t *heater_label = lv_label_create(heater_btn);
  lv_obj_set_style_text_font(heater_label, &mdi_icons_40, 0);
  lv_label_set_text(heater_label, MDI_POWER);
  lv_obj_center(heater_label);
  lv_obj_add_event_cb(heater_btn, heater_event_cb, LV_EVENT_CLICKED, NULL);

  steam_btn = lv_btn_create(ctrl_container);
  lv_obj_set_size(steam_btn, 80, 80);
  lv_obj_set_style_border_width(steam_btn, 0, 0);
  lv_obj_set_style_bg_color(steam_btn, lv_palette_main(LV_PALETTE_GREY), 0);
  lv_obj_set_grid_cell(steam_btn, LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);
  lv_obj_t *steam_label = lv_label_create(steam_btn);
  lv_obj_set_style_text_font(steam_label, &mdi_icons_40, 0);
  lv_label_set_text(steam_label, MDI_STEAM);
  lv_obj_center(steam_label);

  settings_btn = lv_btn_create(ctrl_container);
  lv_obj_set_size(settings_btn, 80, 80);
  lv_obj_set_style_border_width(settings_btn, 0, 0);
  lv_obj_set_style_bg_color(settings_btn, lv_palette_main(LV_PALETTE_GREY), 0);
  lv_obj_set_grid_cell(settings_btn, LV_GRID_ALIGN_CENTER, 2, 1, LV_GRID_ALIGN_CENTER, 0, 1);
  lv_obj_t *settings_label = lv_label_create(settings_btn);
  lv_obj_set_style_text_font(settings_label, &mdi_icons_40, 0);
  lv_label_set_text(settings_label, MDI_COG);
  lv_obj_center(settings_label);
  lv_obj_add_event_cb(settings_btn, open_settings_event_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_move_foreground(ctrl_container);

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

  enum
  {
    CONN_NONE,
    CONN_MQTT,
    CONN_ESPNOW
  };
  int conn = CONN_NONE;
  if (Wireless_UsingEspNow())
    conn = CONN_ESPNOW;
  else if (Wireless_IsMQTTConnected())
    conn = CONN_MQTT;

  if (conn_label && conn != last_conn_type)
  {
    switch (conn)
    {
    case CONN_MQTT:
      lv_label_set_text(conn_label, "MQTT");
      lv_obj_set_style_text_color(conn_label, lv_palette_main(LV_PALETTE_ORANGE), 0);
      break;
    case CONN_ESPNOW:
      lv_label_set_text(conn_label, "ESP-NOW");
      lv_obj_set_style_text_color(conn_label, lv_palette_main(LV_PALETTE_CYAN), 0);
      break;
    default:
      lv_label_set_text(conn_label, "");
      break;
    }
    last_conn_type = conn;
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
  char buf[32];
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

  /* backlight */
  if (Backlight_slider)
    lv_slider_set_value(Backlight_slider, LCD_Backlight, LV_ANIM_ON);
  LVGL_Backlight_adjustment(LCD_Backlight);

  /* buttons */
  lv_color_t off = lv_palette_main(LV_PALETTE_GREY);
  lv_color_t on = lv_palette_main(LV_PALETTE_YELLOW);
  if (heater_btn)
    lv_obj_set_style_bg_color(heater_btn, heater ? on : off, 0);
  if (steam_btn)
    lv_obj_set_style_bg_color(steam_btn, steam ? on : off, 0);
  if (settings_btn)
    lv_obj_set_style_bg_color(settings_btn, off, 0);
}

void Backlight_adjustment_event_cb(lv_event_t *e)
{
  uint8_t Backlight = lv_slider_get_value(lv_event_get_target(e));
  if (Backlight <= Backlight_MAX)
  {
    lv_slider_set_value(Backlight_slider, Backlight, LV_ANIM_ON);
    LCD_Backlight = Backlight;
    LVGL_Backlight_adjustment(Backlight);
  }
  else
    printf("Volume out of range: %d\n", Backlight);
}

void LVGL_Backlight_adjustment(uint8_t Backlight) { Set_Backlight(Backlight); }
