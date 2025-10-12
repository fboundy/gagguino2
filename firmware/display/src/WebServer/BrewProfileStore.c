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
#define BREW_PROFILE_STORE_VERSION   (1U)

static const char *TAG = "BrewProfileStore";

typedef struct
{
    uint32_t version;
    int32_t activeIndex;
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
    bool retried = false;
    s_storage.version = BREW_PROFILE_STORE_VERSION;
    while (true)
    {
        err = nvs_set_blob(handle, BREW_PROFILE_STORE_KEY, &s_storage, sizeof(s_storage));
        if (err == ESP_OK)
            break;
        bool out_of_space = (err == ESP_ERR_NVS_NOT_ENOUGH_SPACE);
#ifdef ESP_ERR_NVS_PART_NOT_ENOUGH_SPACE
        out_of_space = out_of_space || (err == ESP_ERR_NVS_PART_NOT_ENOUGH_SPACE);
#endif
        if (!retried && out_of_space)
        {
            ESP_LOGW(TAG, "NVS out of space, erasing key before retry: %s", esp_err_to_name(err));
            esp_err_t erase_err = nvs_erase_key(handle, BREW_PROFILE_STORE_KEY);
            if (erase_err != ESP_OK && erase_err != ESP_ERR_NVS_NOT_FOUND)
            {
                nvs_close(handle);
                return erase_err;
            }
            esp_err_t commit_err = nvs_commit(handle);
            if (commit_err != ESP_OK)
            {
                nvs_close(handle);
                return commit_err;
            }
            retried = true;
            continue;
        }
        nvs_close(handle);
        return err;
    }
    err = nvs_commit(handle);
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
    s_storage.version = BREW_PROFILE_STORE_VERSION;
    s_storage.activeIndex = BREW_PROFILE_STORE_ACTIVE_NONE;
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

    size_t required = 0;
    err = nvs_get_blob(handle, BREW_PROFILE_STORE_KEY, NULL, &required);
    bool persist_updated_blob = false;
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "No stored profiles found, loading defaults");
        load_default_locked();
        persist_updated_blob = true;
        err = ESP_OK;
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to query profiles blob: %s", esp_err_to_name(err));
        nvs_close(handle);
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return err;
    }
    else if (required == sizeof(s_storage))
    {
        size_t len = sizeof(s_storage);
        err = nvs_get_blob(handle, BREW_PROFILE_STORE_KEY, &s_storage, &len);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to read profile blob: %s", esp_err_to_name(err));
            nvs_close(handle);
            vSemaphoreDelete(s_mutex);
            s_mutex = NULL;
            return err;
        }
    }
    else if (required == sizeof(BrewProfileSnapshot))
    {
        // Read directly into the persistent storage buffer to avoid large stack allocations
        size_t len = sizeof(s_storage.snapshot);
        err = nvs_get_blob(handle, BREW_PROFILE_STORE_KEY, &s_storage.snapshot, &len);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to read legacy profile blob: %s", esp_err_to_name(err));
            nvs_close(handle);
            vSemaphoreDelete(s_mutex);
            s_mutex = NULL;
            return err;
        }
        // version and activeIndex were zeroed earlier when s_storage was cleared
        s_storage.version = BREW_PROFILE_STORE_VERSION;
        s_storage.activeIndex = BREW_PROFILE_STORE_ACTIVE_NONE;
        persist_updated_blob = true;
    }
    else
    {
        ESP_LOGW(TAG, "Stored profile blob unexpected size %u, loading defaults", (unsigned)required);
        load_default_locked();
        persist_updated_blob = true;
    }

    if (s_storage.version != BREW_PROFILE_STORE_VERSION)
    {
        ESP_LOGW(TAG, "Profile store version %u unexpected, resetting metadata", (unsigned)s_storage.version);
        s_storage.version = BREW_PROFILE_STORE_VERSION;
        persist_updated_blob = true;
    }

    if (s_storage.snapshot.profileCount > BREW_PROFILE_STORE_MAX_PROFILES)
    {
        ESP_LOGW(TAG, "Stored profile count %u exceeds max, truncating", (unsigned)s_storage.snapshot.profileCount);
        s_storage.snapshot.profileCount = BREW_PROFILE_STORE_MAX_PROFILES;
        persist_updated_blob = true;
    }
    for (uint32_t i = 0; i < s_storage.snapshot.profileCount; ++i)
    {
        if (s_storage.snapshot.profiles[i].phaseCount > BREW_PROFILE_STORE_MAX_PHASES)
        {
            ESP_LOGW(TAG, "Profile %u phase count %u exceeds max, truncating", (unsigned)i,
                     (unsigned)s_storage.snapshot.profiles[i].phaseCount);
            s_storage.snapshot.profiles[i].phaseCount = BREW_PROFILE_STORE_MAX_PHASES;
            persist_updated_blob = true;
        }
        s_storage.snapshot.profiles[i].name[sizeof(s_storage.snapshot.profiles[i].name) - 1] = '\0';
        for (uint32_t p = 0; p < s_storage.snapshot.profiles[i].phaseCount; ++p)
        {
            s_storage.snapshot.profiles[i].phases[p].name[sizeof(s_storage.snapshot.profiles[i].phases[p].name) - 1] = '\0';
        }
    }

    if (s_storage.activeIndex < 0 || (uint32_t)s_storage.activeIndex >= s_storage.snapshot.profileCount)
    {
        if (s_storage.activeIndex != BREW_PROFILE_STORE_ACTIVE_NONE)
        {
            ESP_LOGW(TAG, "Active profile index %d out of range, clearing selection", (int)s_storage.activeIndex);
            persist_updated_blob = true;
        }
        s_storage.activeIndex = BREW_PROFILE_STORE_ACTIVE_NONE;
    }

    if (persist_updated_blob)
    {
        esp_err_t save_err = nvs_set_blob(handle, BREW_PROFILE_STORE_KEY, &s_storage, sizeof(s_storage));
        if (save_err == ESP_OK)
        {
            save_err = nvs_commit(handle);
        }
        if (save_err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to persist profiles: %s", esp_err_to_name(save_err));
            nvs_close(handle);
            vSemaphoreDelete(s_mutex);
            s_mutex = NULL;
            return save_err;
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

esp_err_t BrewProfileStore_DeleteProfile(uint32_t index)
{
    if (!s_initialized)
        return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        return ESP_ERR_TIMEOUT;
    uint32_t count = s_storage.snapshot.profileCount;
    if (index >= count)
    {
        xSemaphoreGive(s_mutex);
        return ESP_ERR_INVALID_ARG;
    }

    if (s_storage.activeIndex == (int32_t)index)
    {
        s_storage.activeIndex = BREW_PROFILE_STORE_ACTIVE_NONE;
    }
    else if (s_storage.activeIndex > (int32_t)index)
    {
        s_storage.activeIndex -= 1;
    }

    if (index + 1 < count)
    {
        memmove(&s_storage.snapshot.profiles[index], &s_storage.snapshot.profiles[index + 1],
                (count - index - 1) * sizeof(BrewProfileConfig));
    }
    memset(&s_storage.snapshot.profiles[count - 1], 0, sizeof(BrewProfileConfig));
    s_storage.snapshot.profileCount = count - 1;

    esp_err_t err = save_locked();
    xSemaphoreGive(s_mutex);
    return err;
}

esp_err_t BrewProfileStore_GetActiveProfile(int32_t *index)
{
    if (!index)
        return ESP_ERR_INVALID_ARG;
    if (!s_initialized)
        return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        return ESP_ERR_TIMEOUT;
    *index = s_storage.activeIndex;
    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

esp_err_t BrewProfileStore_SetActiveProfile(int32_t index)
{
    if (!s_initialized)
        return ESP_ERR_INVALID_STATE;
    if (index != BREW_PROFILE_STORE_ACTIVE_NONE && index < 0)
        return ESP_ERR_INVALID_ARG;
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
        return ESP_ERR_TIMEOUT;
    if (index != BREW_PROFILE_STORE_ACTIVE_NONE && (uint32_t)index >= s_storage.snapshot.profileCount)
    {
        xSemaphoreGive(s_mutex);
        return ESP_ERR_INVALID_ARG;
    }
    if (s_storage.activeIndex == index)
    {
        xSemaphoreGive(s_mutex);
        return ESP_OK;
    }
    s_storage.activeIndex = index;
    esp_err_t err = save_locked();
    xSemaphoreGive(s_mutex);
    return err;
}

