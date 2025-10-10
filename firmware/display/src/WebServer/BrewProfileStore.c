#include "BrewProfileStore.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"

#define BREW_PROFILE_STORE_NAMESPACE "brew_profiles"
#define BREW_PROFILE_STORE_KEY       "profiles"

static const char *TAG = "BrewProfileStore";

typedef struct
{
    BrewProfileSnapshot snapshot;
} BrewProfileStorage;

static BrewProfileStorage s_storage;
static bool s_initialized = false;
static SemaphoreHandle_t s_mutex = NULL;

static esp_err_t validate_profile(const BrewProfileConfig *profile)
{
    if (!profile)
        return ESP_ERR_INVALID_ARG;
    size_t name_len = strnlen(profile->name, sizeof(profile->name));
    if (name_len == 0 || name_len >= sizeof(profile->name))
    {
        ESP_LOGE(TAG, "Profile name is invalid");
        return ESP_ERR_INVALID_ARG;
    }
    if (profile->phaseCount == 0 || profile->phaseCount > BREW_PROFILE_STORE_MAX_PHASES)
    {
        ESP_LOGE(TAG, "Profile phase count %u out of range", (unsigned)profile->phaseCount);
        return ESP_ERR_INVALID_ARG;
    }
    for (uint32_t i = 0; i < profile->phaseCount; ++i)
    {
        const BrewPhaseConfig *phase = &profile->phases[i];
        size_t phase_name_len = strnlen(phase->name, sizeof(phase->name));
        if (phase_name_len == 0 || phase_name_len >= sizeof(phase->name))
        {
            ESP_LOGE(TAG, "Phase %u has invalid name", (unsigned)i);
            return ESP_ERR_INVALID_ARG;
        }
        if (phase->durationMode > BREW_DURATION_MASS)
        {
            ESP_LOGE(TAG, "Phase %u has invalid duration mode %u", (unsigned)i, (unsigned)phase->durationMode);
            return ESP_ERR_INVALID_ARG;
        }
        if (phase->pumpMode > BREW_PUMP_PRESSURE)
        {
            ESP_LOGE(TAG, "Phase %u has invalid pump mode %u", (unsigned)i, (unsigned)phase->pumpMode);
            return ESP_ERR_INVALID_ARG;
        }
    }
    return ESP_OK;
}

static esp_err_t save_locked(void)
{
    if (!s_initialized)
        return ESP_ERR_INVALID_STATE;
    nvs_handle_t handle;
    esp_err_t err = nvs_open(BREW_PROFILE_STORE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
        return err;
    err = nvs_set_blob(handle, BREW_PROFILE_STORE_KEY, &s_storage.snapshot, sizeof(s_storage.snapshot));
    if (err == ESP_OK)
    {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

static void copy_phase(BrewPhaseConfig *dst, const BrewPhaseConfig *src)
{
    memcpy(dst, src, sizeof(*dst));
    dst->name[sizeof(dst->name) - 1] = '\0';
}

static void copy_profile(BrewProfileConfig *dst, const BrewProfileConfig *src)
{
    memset(dst, 0, sizeof(*dst));
    strlcpy(dst->name, src->name, sizeof(dst->name));
    dst->phaseCount = src->phaseCount;
    for (uint32_t i = 0; i < dst->phaseCount && i < BREW_PROFILE_STORE_MAX_PHASES; ++i)
    {
        copy_phase(&dst->phases[i], &src->phases[i]);
    }
}

static void load_default_locked(void)
{
    memset(&s_storage, 0, sizeof(s_storage));
    BrewProfileConfig def = {0};
    strlcpy(def.name, BREW_PROFILE_DEFAULT.name, sizeof(def.name));
    def.phaseCount = BREW_PROFILE_DEFAULT.phaseCount;
    if (def.phaseCount > BREW_PROFILE_STORE_MAX_PHASES)
    {
        def.phaseCount = BREW_PROFILE_STORE_MAX_PHASES;
    }
    for (uint32_t i = 0; i < def.phaseCount; ++i)
    {
        const BrewPhase *src = &BREW_PROFILE_DEFAULT.phases[i];
        BrewPhaseConfig *dst = &def.phases[i];
        memset(dst, 0, sizeof(*dst));
        if (src->name)
        {
            strlcpy(dst->name, src->name, sizeof(dst->name));
        }
        dst->durationMode = src->durationMode;
        dst->durationValue = src->durationValue;
        dst->pumpMode = src->pumpMode;
        dst->pumpValue = src->pumpValue;
        dst->temperatureC = src->temperatureC;
    }
    s_storage.snapshot.profileCount = 1;
    copy_profile(&s_storage.snapshot.profiles[0], &def);
}

esp_err_t BrewProfileStore_Init(void)
{
    if (s_initialized)
        return ESP_OK;
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex)
    {
        return ESP_ERR_NO_MEM;
    }
    memset(&s_storage, 0, sizeof(s_storage));

    nvs_handle_t handle;
    esp_err_t err = nvs_open(BREW_PROFILE_STORE_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return err;
    }

    size_t required = sizeof(s_storage.snapshot);
    err = nvs_get_blob(handle, BREW_PROFILE_STORE_KEY, &s_storage.snapshot, &required);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "No stored profiles found, loading defaults");
        load_default_locked();
        esp_err_t save_err = nvs_set_blob(handle, BREW_PROFILE_STORE_KEY, &s_storage.snapshot, sizeof(s_storage.snapshot));
        if (save_err == ESP_OK)
        {
            save_err = nvs_commit(handle);
        }
        if (save_err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to persist default profiles: %s", esp_err_to_name(save_err));
            nvs_close(handle);
            vSemaphoreDelete(s_mutex);
            s_mutex = NULL;
            return save_err;
        }
        err = ESP_OK;
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read profiles: %s", esp_err_to_name(err));
        nvs_close(handle);
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return err;
    }
    else
    {
        if (s_storage.snapshot.profileCount > BREW_PROFILE_STORE_MAX_PROFILES)
        {
            ESP_LOGW(TAG, "Stored profile count %u exceeds max, truncating", (unsigned)s_storage.snapshot.profileCount);
            s_storage.snapshot.profileCount = BREW_PROFILE_STORE_MAX_PROFILES;
        }
        for (uint32_t i = 0; i < s_storage.snapshot.profileCount; ++i)
        {
            if (s_storage.snapshot.profiles[i].phaseCount > BREW_PROFILE_STORE_MAX_PHASES)
            {
                ESP_LOGW(TAG, "Profile %u phase count %u exceeds max, truncating", (unsigned)i,
                         (unsigned)s_storage.snapshot.profiles[i].phaseCount);
                s_storage.snapshot.profiles[i].phaseCount = BREW_PROFILE_STORE_MAX_PHASES;
            }
            s_storage.snapshot.profiles[i].name[sizeof(s_storage.snapshot.profiles[i].name) - 1] = '\0';
            for (uint32_t p = 0; p < s_storage.snapshot.profiles[i].phaseCount; ++p)
            {
                s_storage.snapshot.profiles[i].phases[p].name[sizeof(s_storage.snapshot.profiles[i].phases[p].name) - 1] = '\0';
            }
        }
    }

    nvs_close(handle);
    s_initialized = true;
    ESP_LOGI(TAG, "Loaded %u brew profiles", (unsigned)s_storage.snapshot.profileCount);
    return ESP_OK;
}

esp_err_t BrewProfileStore_GetSnapshot(BrewProfileSnapshot *snapshot)
{
    if (!snapshot)
        return ESP_ERR_INVALID_ARG;
    if (!s_initialized)
        return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        return ESP_ERR_TIMEOUT;
    memcpy(snapshot, &s_storage.snapshot, sizeof(*snapshot));
    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

esp_err_t BrewProfileStore_AddProfile(const BrewProfileConfig *profile, uint32_t *out_index)
{
    if (!s_initialized)
        return ESP_ERR_INVALID_STATE;
    esp_err_t err = validate_profile(profile);
    if (err != ESP_OK)
        return err;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        return ESP_ERR_TIMEOUT;
    if (s_storage.snapshot.profileCount >= BREW_PROFILE_STORE_MAX_PROFILES)
    {
        xSemaphoreGive(s_mutex);
        return ESP_ERR_NO_MEM;
    }
    uint32_t index = s_storage.snapshot.profileCount;
    copy_profile(&s_storage.snapshot.profiles[index], profile);
    s_storage.snapshot.profileCount++;
    err = save_locked();
    xSemaphoreGive(s_mutex);
    if (err == ESP_OK && out_index)
    {
        *out_index = index;
    }
    return err;
}

esp_err_t BrewProfileStore_UpdateProfile(uint32_t index, const BrewProfileConfig *profile)
{
    if (!s_initialized)
        return ESP_ERR_INVALID_STATE;
    esp_err_t err = validate_profile(profile);
    if (err != ESP_OK)
        return err;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        return ESP_ERR_TIMEOUT;
    if (index >= s_storage.snapshot.profileCount)
    {
        xSemaphoreGive(s_mutex);
        return ESP_ERR_INVALID_ARG;
    }
    copy_profile(&s_storage.snapshot.profiles[index], profile);
    err = save_locked();
    xSemaphoreGive(s_mutex);
    return err;
}

