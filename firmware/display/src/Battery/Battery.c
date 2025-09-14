#include "Battery.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#define BAT_ADC_CHANNEL ADC1_CHANNEL_6
#define BAT_ADC_ATTEN   ADC_ATTEN_DB_11
#define DEFAULT_VREF    1100

static esp_adc_cal_characteristics_t adc_chars;

void Battery_Init(void)
{
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(BAT_ADC_CHANNEL, BAT_ADC_ATTEN);
    esp_adc_cal_characterize(ADC_UNIT_1, BAT_ADC_ATTEN,
                             ADC_WIDTH_BIT_12, DEFAULT_VREF, &adc_chars);
}

static inline int raw_to_percent(int raw)
{
    uint32_t mv = esp_adc_cal_raw_to_voltage(raw, &adc_chars);
    float voltage = (float)mv / 1000.0f * 2.0f; // adjust for divider
    float pct = (voltage - 3.0f) * 100.0f / (4.2f - 3.0f);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return (int)pct;
}

int Battery_GetPercentage(void)
{
    int raw = adc1_get_raw(BAT_ADC_CHANNEL);
    return raw_to_percent(raw);
}
