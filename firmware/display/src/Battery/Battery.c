#include "Battery.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#define BAT_ADC_CHANNEL ADC_CHANNEL_6
#define BAT_ADC_ATTEN   ADC_ATTEN_DB_12

static adc_oneshot_unit_handle_t adc_handle;
static adc_cali_handle_t cali_handle;

void Battery_Init(void)
{
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_new_unit(&init_cfg, &adc_handle);

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = BAT_ADC_ATTEN,
    };
    adc_oneshot_config_channel(adc_handle, BAT_ADC_CHANNEL, &chan_cfg);

    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .atten = BAT_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    adc_cali_create_scheme_curve_fitting(&cali_cfg, &cali_handle);
}

static inline int raw_to_percent(int raw)
{
    int mv = 0;
    adc_cali_raw_to_voltage(cali_handle, raw, &mv);
    float voltage = (float)mv / 1000.0f * 2.0f; // adjust for divider
    float pct = (voltage - 3.0f) * 100.0f / (4.2f - 3.0f);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return (int)pct;
}

int Battery_GetPercentage(void)
{
    int raw = 0;
    adc_oneshot_read(adc_handle, BAT_ADC_CHANNEL, &raw);
    return raw_to_percent(raw);
}
