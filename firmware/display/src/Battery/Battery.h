#pragma once

#include <stdint.h>

void Battery_Init(void);
int Battery_GetPercentage(void);

/* Align with Waveshare example configuration */
// ADC1 Channels
#define EXAMPLE_ADC1_CHAN3          ADC_CHANNEL_3      /* GPIO4 */
#define EXAMPLE_ADC_ATTEN           ADC_ATTEN_DB_12    /* 0..~3.9V range */

#define Measurement_offset          0.994500
