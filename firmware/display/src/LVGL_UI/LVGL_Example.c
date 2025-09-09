#include "LVGL_Example.h"


/**********************
 *      TYPEDEFS
 **********************/
typedef enum {
    DISP_SMALL,
    DISP_MEDIUM,
    DISP_LARGE,
} disp_size_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void Status_create(lv_obj_t * parent);
static void Settings_create(void);
static void open_settings_event_cb(lv_event_t * e);
static void back_event_cb(lv_event_t * e);

static void ta_event_cb(lv_event_t * e);
void example1_increase_lvgl_tick(lv_timer_t * t);
/**********************
 *  STATIC VARIABLES
 **********************/
static disp_size_t disp_size;

static lv_obj_t * tv;
lv_style_t style_text_muted;
lv_style_t style_title;
static lv_style_t style_icon;
static lv_style_t style_bullet;

static const lv_font_t * font_large;
static const lv_font_t * font_normal;

static lv_timer_t * auto_step_timer;
// static lv_color_t original_screen_bg_color;

static lv_timer_t * meter2_timer;

static lv_obj_t * main_screen;
static lv_obj_t * settings_scr;
static lv_coord_t tab_h_global;

static lv_obj_t * temp_meter;
static lv_meter_indicator_t * current_temp_indic;
static lv_meter_indicator_t * set_temp_indic;
lv_obj_t * Backlight_slider;


void Lvgl_Example1(void){

  if(LV_HOR_RES <= 320) disp_size = DISP_SMALL;             
  else if(LV_HOR_RES < 720) disp_size = DISP_MEDIUM;       
  else disp_size = DISP_LARGE;    
  font_large = LV_FONT_DEFAULT;                             
  font_normal = LV_FONT_DEFAULT;                         
  
  lv_coord_t tab_h;
  if(disp_size == DISP_LARGE) {
    tab_h = 70;
    #if LV_FONT_MONTSERRAT_24
      font_large     = &lv_font_montserrat_24;
    #else
      LV_LOG_WARN("LV_FONT_MONTSERRAT_24 is not enabled for the widgets demo. Using LV_FONT_DEFAULT instead.");
    #endif
    #if LV_FONT_MONTSERRAT_16
      font_normal    = &lv_font_montserrat_16;
    #else
      LV_LOG_WARN("LV_FONT_MONTSERRAT_16 is not enabled for the widgets demo. Using LV_FONT_DEFAULT instead.");
    #endif
  }
  else if(disp_size == DISP_MEDIUM) {
    tab_h = 45;
    #if LV_FONT_MONTSERRAT_20
      font_large     = &lv_font_montserrat_20;
    #else
        LV_LOG_WARN("LV_FONT_MONTSERRAT_20 is not enabled for the widgets demo. Using LV_FONT_DEFAULT instead.");
    #endif
    #if LV_FONT_MONTSERRAT_14
      font_normal    = &lv_font_montserrat_14;
    #else
      LV_LOG_WARN("LV_FONT_MONTSERRAT_14 is not enabled for the widgets demo. Using LV_FONT_DEFAULT instead.");
    #endif
  }
  else {   /* disp_size == DISP_SMALL */
    tab_h = 45;
    #if LV_FONT_MONTSERRAT_18
      font_large     = &lv_font_montserrat_18;
    #else
      LV_LOG_WARN("LV_FONT_MONTSERRAT_18 is not enabled for the widgets demo. Using LV_FONT_DEFAULT instead.");
    #endif
    #if LV_FONT_MONTSERRAT_12
      font_normal    = &lv_font_montserrat_12;
    #else
      LV_LOG_WARN("LV_FONT_MONTSERRAT_12 is not enabled for the widgets demo. Using LV_FONT_DEFAULT instead.");
    #endif
  }
  /* Adjust the tab header so the common footer is 80% of its previous height */
  tab_h = tab_h * 2;
  tab_h_global = tab_h;

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

  tv = lv_tabview_create(lv_scr_act(), LV_DIR_BOTTOM, tab_h);
  main_screen = lv_scr_act();
  lv_obj_set_style_bg_color(main_screen, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(main_screen, LV_OPA_COVER, 0);
  settings_scr = NULL;
  Backlight_slider = NULL;

  // Stretch the tab button matrix and center the tab text
  lv_obj_t * tab_btns = lv_tabview_get_tab_btns(tv);
  lv_obj_set_width(tab_btns, LV_PCT(100));
  lv_btnmatrix_set_btn_width(tab_btns, 0, 8);
  lv_obj_set_style_pad_all(tab_btns, 0, 0);
  lv_obj_set_style_text_align(tab_btns, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_border_side(tab_btns, LV_BORDER_SIDE_TOP, 0);

  lv_obj_set_style_text_font(lv_scr_act(), font_normal, 0);

  if(disp_size == DISP_LARGE) {
    /* Large displays do not require additional header content. */
  }

  lv_obj_t * t1 = lv_tabview_add_tab(tv, "Status");
  lv_obj_set_style_bg_color(t1, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(t1, LV_OPA_COVER, 0);
  // lv_obj_t * t2 = lv_tabview_add_tab(tv, "Buzzer");
  // lv_obj_t * t3 = lv_tabview_add_tab(tv, "Shop");
  
  // lv_coord_t screen_width = lv_obj_get_width(lv_scr_act());
  // lv_obj_set_width(t1, screen_width);
  Status_create(t1);
  // Buzzer_create(t2);
  // shop_create(t3);

  // color_changer_create(tv);
}

static void led_event_cb(lv_event_t *e) {
    lv_obj_t *led = (lv_obj_t *)lv_event_get_user_data(e);
    lv_obj_t *sw = lv_event_get_target(e);
    if (lv_obj_get_state(sw) & LV_STATE_CHECKED) {
      lv_led_on(led);
      Buzzer_On();
    }
    else {
      lv_led_off(led);
      Buzzer_Off();
    }
}

static void back_event_cb(lv_event_t * e) {
  lv_scr_load(main_screen);
}

static void open_settings_event_cb(lv_event_t * e) {
  if(!settings_scr) Settings_create();
  lv_scr_load(settings_scr);
}

static void Settings_create(void) {
  settings_scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(settings_scr, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(settings_scr, LV_OPA_COVER, 0);

  lv_obj_t * tvs = lv_tabview_create(settings_scr, LV_DIR_BOTTOM, tab_h_global);
  lv_obj_set_style_bg_color(tvs, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(tvs, LV_OPA_COVER, 0);
  lv_obj_t * t = lv_tabview_add_tab(tvs, "Settings");
  lv_obj_set_style_bg_color(t, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);

  static lv_coord_t grid_main_col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  static lv_coord_t grid_main_row_dsc[] = {
    LV_GRID_CONTENT,
    LV_GRID_CONTENT,
    LV_GRID_CONTENT,
    LV_GRID_CONTENT,
    LV_GRID_FR(1),
    LV_GRID_CONTENT,
    LV_GRID_TEMPLATE_LAST
  };
  lv_obj_set_grid_dsc_array(t, grid_main_col_dsc, grid_main_row_dsc);

  lv_obj_t * back_btn = lv_btn_create(t);
  lv_obj_set_size(back_btn, 80, 80);
  lv_obj_set_grid_cell(back_btn, LV_GRID_ALIGN_CENTER, 0, 2, LV_GRID_ALIGN_CENTER, 5, 1);
  lv_obj_t * back_label = lv_label_create(back_btn);
  lv_label_set_text(back_label, LV_SYMBOL_LEFT);
  lv_obj_center(back_label);
  lv_obj_add_event_cb(back_btn, back_event_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t * Backlight_label = lv_label_create(t);
  lv_label_set_text(Backlight_label, "Backlight brightness");
  lv_obj_add_style(Backlight_label, &style_text_muted, 0);
  lv_obj_set_grid_cell(Backlight_label, LV_GRID_ALIGN_CENTER, 0, 2, LV_GRID_ALIGN_START, 0, 1);

  Backlight_slider = lv_slider_create(t);
  lv_obj_add_flag(Backlight_slider, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(Backlight_slider, 200, 35);
  lv_obj_set_style_radius(Backlight_slider, 3, LV_PART_KNOB);
  lv_obj_set_style_bg_opa(Backlight_slider, LV_OPA_TRANSP, LV_PART_KNOB);
  lv_obj_set_style_bg_color(Backlight_slider, lv_color_hex(0xAAAAAA), LV_PART_KNOB);
  lv_obj_set_style_bg_color(Backlight_slider, lv_color_hex(0xFFFFFF), LV_PART_INDICATOR);
  lv_obj_set_style_outline_width(Backlight_slider, 2, LV_PART_INDICATOR);
  lv_obj_set_style_outline_color(Backlight_slider, lv_color_hex(0xD3D3D3), LV_PART_INDICATOR);
  lv_slider_set_range(Backlight_slider, 5, Backlight_MAX);
  lv_slider_set_value(Backlight_slider, LCD_Backlight, LV_ANIM_ON);
  lv_obj_add_event_cb(Backlight_slider, Backlight_adjustment_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_set_grid_cell(Backlight_slider, LV_GRID_ALIGN_CENTER, 0, 2, LV_GRID_ALIGN_START, 1, 1);

  lv_obj_t * panel2_title = lv_label_create(t);
  lv_label_set_text(panel2_title, "The buzzer tes");
  lv_obj_add_style(panel2_title, &style_title, 0);
  lv_obj_set_grid_cell(panel2_title, LV_GRID_ALIGN_CENTER, 0, 2, LV_GRID_ALIGN_START, 2, 1);

  lv_obj_t *led = lv_led_create(t);
  lv_obj_set_size(led, 50, 50);
  lv_led_off(led);
  lv_obj_set_grid_cell(led, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_START, 3, 1);

  lv_obj_t *sw = lv_switch_create(t);
  lv_obj_set_size(sw, 65, 40);
  lv_obj_add_event_cb(sw, led_event_cb, LV_EVENT_VALUE_CHANGED, led);
  lv_obj_set_grid_cell(sw, LV_GRID_ALIGN_CENTER, 1, 1, LV_GRID_ALIGN_START, 3, 1);
}
// static void Buzzer_create(lv_obj_t * parent)
// {
//   lv_obj_t *label = lv_label_create(parent);
//   lv_label_set_text(label, "The buzzer tes");
//   lv_obj_set_size(label, LV_PCT(30), LV_PCT(5));
//   lv_obj_align(label, LV_ALIGN_CENTER, 0, -60);
//   lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0); 


//   lv_obj_t *led = lv_led_create(parent);
//   lv_obj_set_size(led, 50, 50);
//   lv_obj_align(led, LV_ALIGN_CENTER, -60, 0);
//   lv_led_off(led);

//   lv_obj_t *sw = lv_switch_create(parent);
//   lv_obj_align(sw, LV_ALIGN_CENTER, 60, 0);
//   lv_obj_add_event_cb(sw, led_event_cb, LV_EVENT_VALUE_CHANGED, led);
// }
void Lvgl_Example1_close(void)
{
  /*Delete all animation*/
  lv_anim_del(NULL, NULL);

  lv_timer_del(meter2_timer);
  meter2_timer = NULL;

  lv_obj_clean(lv_scr_act());

  lv_style_reset(&style_text_muted);
  lv_style_reset(&style_title);
  lv_style_reset(&style_icon);
  lv_style_reset(&style_bullet);
}


/**********************
*   STATIC FUNCTIONS
**********************/

static void Status_create(lv_obj_t * parent)
{
  static lv_coord_t grid_main_col_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
static lv_coord_t grid_main_row_dsc[] = {LV_GRID_FR(1), LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

  lv_obj_set_style_bg_color(parent, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
  lv_obj_set_grid_dsc_array(parent, grid_main_col_dsc, grid_main_row_dsc);

  temp_meter = lv_meter_create(parent);
  lv_obj_set_size(temp_meter, LV_PCT(100), LV_PCT(100));
  lv_obj_set_grid_cell(temp_meter, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 0, 1);

  lv_meter_scale_t * scale = lv_meter_add_scale(temp_meter);
  lv_meter_set_scale_ticks(temp_meter, scale, 11, 2, 10, lv_palette_main(LV_PALETTE_GREY));
  lv_meter_set_scale_range(temp_meter, scale, 60, 160, 270, 225);
  current_temp_indic = lv_meter_add_arc(temp_meter, scale, 10, lv_palette_main(LV_PALETTE_RED), 0);
  lv_meter_set_indicator_start_value(temp_meter, current_temp_indic, 60);
  set_temp_indic = lv_meter_add_scale_lines(temp_meter, scale,
                                            lv_palette_main(LV_PALETTE_BLUE),
                                            lv_palette_main(LV_PALETTE_BLUE),
                                            false, 2);

  auto_step_timer = lv_timer_create(example1_increase_lvgl_tick, 100, NULL);

  lv_obj_t * settings_btn = lv_btn_create(parent);
  lv_obj_set_size(settings_btn, 80, 80);
  lv_obj_set_grid_cell(settings_btn, LV_GRID_ALIGN_CENTER, 0, 1, LV_GRID_ALIGN_CENTER, 1, 1);
  lv_obj_t * settings_label = lv_label_create(settings_btn);
  lv_label_set_text(settings_label, LV_SYMBOL_SETTINGS);
  lv_obj_center(settings_label);
  lv_obj_add_event_cb(settings_btn, open_settings_event_cb, LV_EVENT_CLICKED, NULL);
}


void example1_increase_lvgl_tick(lv_timer_t * t)
{
  float current = MQTT_GetCurrentTemp();
  float set = MQTT_GetSetTemp();
  if(temp_meter) {
    lv_meter_set_indicator_end_value(temp_meter, current_temp_indic, (int32_t)current);
    lv_meter_set_indicator_start_value(temp_meter, set_temp_indic, (int32_t)set);
    lv_meter_set_indicator_end_value(temp_meter, set_temp_indic, (int32_t)set);
  }
  if(Backlight_slider)
    lv_slider_set_value(Backlight_slider, LCD_Backlight, LV_ANIM_ON);
  LVGL_Backlight_adjustment(LCD_Backlight);
}





static void ta_event_cb(lv_event_t * e)
{
}



void Backlight_adjustment_event_cb(lv_event_t * e) {
  uint8_t Backlight = lv_slider_get_value(lv_event_get_target(e));  
  if (Backlight <= Backlight_MAX)  {
    lv_slider_set_value(Backlight_slider, Backlight, LV_ANIM_ON); 
    LCD_Backlight = Backlight;
    LVGL_Backlight_adjustment(Backlight);
  }
  else
    printf("Volume out of range: %d\n", Backlight);

}

void LVGL_Backlight_adjustment(uint8_t Backlight) {
  Set_Backlight(Backlight);                                 
}






