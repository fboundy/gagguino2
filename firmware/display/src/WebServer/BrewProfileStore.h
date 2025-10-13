#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "brew_profile.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BREW_PROFILE_STORE_MAX_PROFILES 8U
#define BREW_PROFILE_STORE_MAX_PHASES   12U
#define BREW_PROFILE_NAME_MAX_LEN       128U
#define BREW_PROFILE_DESCRIPTION_MAX_LEN 256U
#define BREW_PHASE_NAME_MAX_LEN         128U

typedef struct
{
    char name[BREW_PHASE_NAME_MAX_LEN];
    BrewDurationMode durationMode;
    uint32_t durationValue;
    BrewPumpMode pumpMode;
    float pumpValue;
    float temperatureC;
} BrewPhaseConfig;

typedef struct
{
    char name[BREW_PROFILE_NAME_MAX_LEN];
    char description[BREW_PROFILE_DESCRIPTION_MAX_LEN];
    uint32_t phaseCount;
    BrewPhaseConfig phases[BREW_PROFILE_STORE_MAX_PHASES];
} BrewProfileConfig;

typedef struct
{
    uint32_t profileCount;
    BrewProfileConfig profiles[BREW_PROFILE_STORE_MAX_PROFILES];
} BrewProfileSnapshot;

#define BREW_PROFILE_STORE_ACTIVE_NONE (-1)

esp_err_t BrewProfileStore_Init(void);
esp_err_t BrewProfileStore_GetSnapshot(BrewProfileSnapshot *snapshot);
esp_err_t BrewProfileStore_AddProfile(const BrewProfileConfig *profile, uint32_t *out_index);
esp_err_t BrewProfileStore_UpdateProfile(uint32_t index, const BrewProfileConfig *profile);
esp_err_t BrewProfileStore_DeleteProfile(uint32_t index);
esp_err_t BrewProfileStore_GetActiveProfile(int32_t *index);
esp_err_t BrewProfileStore_SetActiveProfile(int32_t index);

#ifdef __cplusplus
}
#endif

