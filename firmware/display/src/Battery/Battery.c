#include "Battery.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "esp_timer.h"

// Align channel/atten macros with Waveshare example header
#define BAT_ADC_CHANNEL EXAMPLE_ADC1_CHAN3
#define BAT_ADC_ATTEN   EXAMPLE_ADC_ATTEN

static adc_oneshot_unit_handle_t adc_handle;
static adc_cali_handle_t cali_handle;
static bool do_calibration = false;
static const char *ADC_TAG = "ADC";

/* Calibrate ADC following Waveshare example pattern */
static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(ADC_TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = (adc_cali_curve_fitting_config_t){
            .unit_id = unit,
#ifdef SOC_ADC_CALIB_CHAN_COMPENS_SUPPORTED
            .chan = channel,
#endif
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(ADC_TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = (adc_cali_line_fitting_config_t){
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(ADC_TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(ADC_TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(ADC_TAG, "Invalid arg or no memory");
    }

    return calibrated;
}
static const char *BAT_TAG = "Battery";
static int64_t last_batt_sample_us = 0;
static float smoothed_pct = -1.0f; /* EWMA over ~30s */
static int64_t last_adc_read_us = 0; /* rate-limit actual ADC reads */
static int cached_pct = -1;          /* last returned percentage */
static int64_t last_log_us = 0;      /* throttle verbose logging */

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

    /* Calibration init matching Waveshare example */
    do_calibration = example_adc_calibration_init(ADC_UNIT_1, BAT_ADC_CHANNEL, BAT_ADC_ATTEN, &cali_handle);
}

static inline int raw_to_percent(int raw)
{
    int mv = 0; // millivolts at ADC pin
    adc_cali_raw_to_voltage(cali_handle, raw, &mv);
    // Align with Waveshare example: scale by 3 and compensate by Measurement_offset
    float batt_volts = ((float)mv * 3.0f / 1000.0f) / (float)Measurement_offset;
    float pct = (batt_volts - 3.0f) * 100.0f / (4.2f - 3.0f);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return (int)pct;
}

int Battery_GetPercentage(void)
{
    int64_t now_us = esp_timer_get_time();

    /* Rate-limit ADC sampling to at most once every 5 seconds */
    bool should_sample = (last_adc_read_us == 0) || ((now_us - last_adc_read_us) >= 5000000);
    if (!should_sample && cached_pct >= 0)
    {
        return cached_pct;
    }

    int raw = 0;
    adc_oneshot_read(adc_handle, BAT_ADC_CHANNEL, &raw);

    // Convert to mV at ADC pin, then compute battery side mV (Waveshare: x3 and offset compensation)
    int mv = 0;
    if (do_calibration && cali_handle) {
        adc_cali_raw_to_voltage(cali_handle, raw, &mv);
    } else {
        /* Fallback approximation for millivolts at the ADC pin (12-bit, 12 dB approx 0..~3900 mV) */
        mv = (raw * 3900) / 4095;
    }
    int batt_mv = (int)((float)mv * 3.0f / (float)Measurement_offset);
    bool should_log = (last_log_us == 0) || ((now_us - last_log_us) >= 30000000);
    if (should_log)
    {
        ESP_LOGI(BAT_TAG, "ADC raw=%d, at_pin=%dmV, battery=%d.%03dV",
                 raw, mv, batt_mv / 1000, batt_mv % 1000);
        last_log_us = now_us;
    }

    /* Compute instantaneous percentage from current sample */
    int pct_now = raw_to_percent(raw);

    /* Time-based exponential smoothing with 30s time constant */
    const float tau_us = 30.0f * 1000.0f * 1000.0f; /* 30 seconds */
    if (last_batt_sample_us == 0 || smoothed_pct < 0.0f)
    {
        smoothed_pct = (float)pct_now;
        last_batt_sample_us = now_us;
        cached_pct = pct_now;
        last_adc_read_us = now_us;
        return cached_pct;
    }

    int64_t dt_us = now_us - last_batt_sample_us;
    if (dt_us < 0) dt_us = 0;
    /* alpha = dt / (tau + dt) keeps stability for varying dt */
    float alpha = (float)dt_us / (tau_us + (float)dt_us);
    smoothed_pct += alpha * ((float)pct_now - smoothed_pct);
    last_batt_sample_us = now_us;
    cached_pct = (int)(smoothed_pct + 0.5f); /* round to nearest */
    last_adc_read_us = now_us;
    return cached_pct;
}
