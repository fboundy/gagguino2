#include "WebServer.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "BrewProfileStore.h"

static const char *TAG = "WebServer";

static httpd_handle_t s_server = NULL;
static bool s_initialized = false;

static const char INDEX_HTML[] =
    "<!DOCTYPE html>\n"
    "<html lang=\"en\">\n"
    "<head>\n"
    "<meta charset=\"utf-8\">\n"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
    "<title>Gagguino Brew Profiles</title>\n"
    "<style>\n"
    "body{font-family:Arial,sans-serif;margin:20px;background:#f4f4f4;color:#333;}\n"
    "h1{margin-bottom:16px;}\n"
    "#messages{margin-bottom:16px;min-height:1.2em;}\n"
    ".card{background:#fff;padding:16px;margin-bottom:16px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1);}\n"
    "label{display:block;margin-top:8px;font-weight:bold;}\n"
    "label input{display:block;margin-top:4px;padding:6px;width:100%;box-sizing:border-box;}\n"
    "textarea{width:100%;height:160px;margin-top:4px;font-family:monospace;padding:8px;box-sizing:border-box;}\n"
    "button{margin-top:12px;padding:8px 16px;border:none;border-radius:4px;background:#1976d2;color:#fff;cursor:pointer;}\n"
    "button:hover{background:#125a9c;}\n"
    ".error{color:#b00020;}\n"
    ".success{color:#2e7d32;}\n"
    "</style>\n"
    "</head>\n"
    "<body>\n"
    "<h1>Brew Profiles</h1>\n"
    "<div id=\"messages\"></div>\n"
    "<div id=\"profiles\"></div>\n"
    "<div class=\"card\">\n"
    "  <h2>Add Profile</h2>\n"
    "  <form id=\"add-form\">\n"
    "    <label>Name<input name=\"name\" type=\"text\" required></label>\n"
    "    <label>Phases (JSON array)</label>\n"
    "    <textarea name=\"phases\" placeholder='[{\"name\":\"Phase\",\"durationMode\":\"time\",\"durationValue\":30,\"pumpMode\":\"power\",\"pumpValue\":95,\"temperatureC\":92}]'></textarea>\n"
    "    <button type=\"submit\">Add Profile</button>\n"
    "  </form>\n"
    "</div>\n"
    "<script>\n"
    "const messages=document.getElementById('messages');\n"
    "function showMessage(text,isError=false){messages.textContent=text;messages.className=isError?'error':'success';if(text){setTimeout(()=>{messages.textContent='';messages.className='';},5000);}}\n"
    "function renderProfiles(data){const container=document.getElementById('profiles');container.innerHTML='';data.forEach((profile,index)=>{const card=document.createElement('div');card.className='card';const title=document.createElement('h2');title.textContent=`Profile ${index+1}: ${profile.name}`;card.appendChild(title);const form=document.createElement('form');form.dataset.index=index;const nameLabel=document.createElement('label');nameLabel.textContent='Name';const nameInput=document.createElement('input');nameInput.name='name';nameInput.required=true;nameInput.value=profile.name;nameLabel.appendChild(nameInput);form.appendChild(nameLabel);const phaseLabel=document.createElement('label');phaseLabel.textContent='Phases (JSON array)';const textarea=document.createElement('textarea');textarea.name='phases';textarea.value=JSON.stringify(profile.phases,null,2);phaseLabel.appendChild(textarea);form.appendChild(phaseLabel);const submit=document.createElement('button');submit.type='submit';submit.textContent='Save Changes';form.appendChild(submit);form.addEventListener('submit',async(event)=>{event.preventDefault();try{const payload={name:nameInput.value.trim(),phases:JSON.parse(textarea.value)};if(!payload.name)throw new Error('Name is required');if(!Array.isArray(payload.phases)||!payload.phases.length)throw new Error('At least one phase is required');const response=await fetch(`/api/profiles/${index}`,{method:'PUT',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)});if(!response.ok){const text=await response.text();throw new Error(text||'Failed to update');}showMessage('Profile updated');loadProfiles();}catch(err){showMessage(err.message,true);}});card.appendChild(form);container.appendChild(card);});}\n"
    "async function loadProfiles(){try{const response=await fetch('/api/profiles');if(!response.ok)throw new Error('Failed to load profiles');const data=await response.json();renderProfiles(data);}catch(err){showMessage(err.message,true);}}\n"
    "document.getElementById('add-form').addEventListener('submit',async(event)=>{event.preventDefault();const form=event.target;try{const payload={name:form.name.value.trim(),phases:JSON.parse(form.phases.value||'[]')};if(!payload.name)throw new Error('Name is required');if(!Array.isArray(payload.phases)||!payload.phases.length)throw new Error('At least one phase is required');const response=await fetch('/api/profiles',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)});if(!response.ok){const text=await response.text();throw new Error(text||'Failed to add profile');}form.reset();showMessage('Profile added');loadProfiles();}catch(err){showMessage(err.message,true);}});\n"
    "loadProfiles();\n"
    "</script>\n"
    "</body>\n"
    "</html>\n";

static const char *duration_mode_to_string(BrewDurationMode mode)
{
    switch (mode)
    {
    case BREW_DURATION_TIME:
        return "time";
    case BREW_DURATION_VOLUME:
        return "volume";
    case BREW_DURATION_MASS:
        return "mass";
    default:
        return "unknown";
    }
}

static const char *pump_mode_to_string(BrewPumpMode mode)
{
    switch (mode)
    {
    case BREW_PUMP_POWER:
        return "power";
    case BREW_PUMP_PRESSURE:
        return "pressure";
    default:
        return "unknown";
    }
}

static bool parse_duration_mode(const cJSON *item, BrewDurationMode *out)
{
    if (cJSON_IsString(item) && item->valuestring)
    {
        if (strcasecmp(item->valuestring, "time") == 0)
        {
            *out = BREW_DURATION_TIME;
            return true;
        }
        if (strcasecmp(item->valuestring, "volume") == 0)
        {
            *out = BREW_DURATION_VOLUME;
            return true;
        }
        if (strcasecmp(item->valuestring, "mass") == 0)
        {
            *out = BREW_DURATION_MASS;
            return true;
        }
        return false;
    }
    if (cJSON_IsNumber(item))
    {
        int value = (int)item->valuedouble;
        if (value >= BREW_DURATION_TIME && value <= BREW_DURATION_MASS)
        {
            *out = (BrewDurationMode)value;
            return true;
        }
    }
    return false;
}

static bool parse_pump_mode(const cJSON *item, BrewPumpMode *out)
{
    if (cJSON_IsString(item) && item->valuestring)
    {
        if (strcasecmp(item->valuestring, "power") == 0)
        {
            *out = BREW_PUMP_POWER;
            return true;
        }
        if (strcasecmp(item->valuestring, "pressure") == 0)
        {
            *out = BREW_PUMP_PRESSURE;
            return true;
        }
        return false;
    }
    if (cJSON_IsNumber(item))
    {
        int value = (int)item->valuedouble;
        if (value >= BREW_PUMP_POWER && value <= BREW_PUMP_PRESSURE)
        {
            *out = (BrewPumpMode)value;
            return true;
        }
    }
    return false;
}

static esp_err_t parse_profile_json(const char *json, BrewProfileConfig *out, char *errbuf, size_t errlen)
{
    if (!json || !out)
        return ESP_ERR_INVALID_ARG;
    cJSON *root = cJSON_Parse(json);
    if (!root)
    {
        if (errbuf && errlen)
            strlcpy(errbuf, "Invalid JSON body", errlen);
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t result = ESP_OK;
    if (!cJSON_IsObject(root))
    {
        if (errbuf && errlen)
            strlcpy(errbuf, "Expected JSON object", errlen);
        result = ESP_ERR_INVALID_ARG;
        goto cleanup;
    }
    cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");
    cJSON *phases = cJSON_GetObjectItemCaseSensitive(root, "phases");
    if (!cJSON_IsString(name) || !name->valuestring)
    {
        if (errbuf && errlen)
            strlcpy(errbuf, "Profile name must be a string", errlen);
        result = ESP_ERR_INVALID_ARG;
        goto cleanup;
    }
    if (!cJSON_IsArray(phases))
    {
        if (errbuf && errlen)
            strlcpy(errbuf, "Phases must be an array", errlen);
        result = ESP_ERR_INVALID_ARG;
        goto cleanup;
    }
    size_t phase_count = cJSON_GetArraySize(phases);
    if (phase_count == 0)
    {
        if (errbuf && errlen)
            strlcpy(errbuf, "At least one phase is required", errlen);
        result = ESP_ERR_INVALID_ARG;
        goto cleanup;
    }
    if (phase_count > BREW_PROFILE_STORE_MAX_PHASES)
    {
        if (errbuf && errlen)
            snprintf(errbuf, errlen, "Maximum %u phases supported", BREW_PROFILE_STORE_MAX_PHASES);
        result = ESP_ERR_INVALID_ARG;
        goto cleanup;
    }
    memset(out, 0, sizeof(*out));
    strlcpy(out->name, name->valuestring, sizeof(out->name));
    out->phaseCount = (uint32_t)phase_count;
    for (uint32_t i = 0; i < out->phaseCount; ++i)
    {
        cJSON *phase_obj = cJSON_GetArrayItem(phases, i);
        if (!cJSON_IsObject(phase_obj))
        {
            if (errbuf && errlen)
                snprintf(errbuf, errlen, "Phase %u must be an object", (unsigned)i);
            result = ESP_ERR_INVALID_ARG;
            goto cleanup;
        }
        BrewPhaseConfig *phase = &out->phases[i];
        cJSON *phase_name = cJSON_GetObjectItemCaseSensitive(phase_obj, "name");
        cJSON *duration_mode = cJSON_GetObjectItemCaseSensitive(phase_obj, "durationMode");
        cJSON *duration_value = cJSON_GetObjectItemCaseSensitive(phase_obj, "durationValue");
        cJSON *pump_mode = cJSON_GetObjectItemCaseSensitive(phase_obj, "pumpMode");
        cJSON *pump_value = cJSON_GetObjectItemCaseSensitive(phase_obj, "pumpValue");
        cJSON *temperature = cJSON_GetObjectItemCaseSensitive(phase_obj, "temperatureC");
        if (!cJSON_IsString(phase_name) || !phase_name->valuestring)
        {
            if (errbuf && errlen)
                snprintf(errbuf, errlen, "Phase %u name must be a string", (unsigned)i);
            result = ESP_ERR_INVALID_ARG;
            goto cleanup;
        }
        if (!parse_duration_mode(duration_mode, &phase->durationMode))
        {
            if (errbuf && errlen)
                snprintf(errbuf, errlen, "Phase %u has invalid durationMode", (unsigned)i);
            result = ESP_ERR_INVALID_ARG;
            goto cleanup;
        }
        if (!cJSON_IsNumber(duration_value) || duration_value->valuedouble < 0.0)
        {
            if (errbuf && errlen)
                snprintf(errbuf, errlen, "Phase %u durationValue must be non-negative number", (unsigned)i);
            result = ESP_ERR_INVALID_ARG;
            goto cleanup;
        }
        if (!parse_pump_mode(pump_mode, &phase->pumpMode))
        {
            if (errbuf && errlen)
                snprintf(errbuf, errlen, "Phase %u has invalid pumpMode", (unsigned)i);
            result = ESP_ERR_INVALID_ARG;
            goto cleanup;
        }
        if (!cJSON_IsNumber(pump_value))
        {
            if (errbuf && errlen)
                snprintf(errbuf, errlen, "Phase %u pumpValue must be a number", (unsigned)i);
            result = ESP_ERR_INVALID_ARG;
            goto cleanup;
        }
        if (!cJSON_IsNumber(temperature))
        {
            if (errbuf && errlen)
                snprintf(errbuf, errlen, "Phase %u temperatureC must be a number", (unsigned)i);
            result = ESP_ERR_INVALID_ARG;
            goto cleanup;
        }
        strlcpy(phase->name, phase_name->valuestring, sizeof(phase->name));
        phase->durationValue = (uint32_t)(duration_value->valuedouble + 0.5);
        phase->pumpValue = (float)pump_value->valuedouble;
        phase->temperatureC = (float)temperature->valuedouble;
    }
cleanup:
    cJSON_Delete(root);
    return result;
}

static esp_err_t read_request_body(httpd_req_t *req, char **out_buf)
{
    if (!req || !out_buf)
        return ESP_ERR_INVALID_ARG;
    size_t total = req->content_len;
    if (total == 0)
        return ESP_ERR_INVALID_SIZE;
    char *buf = calloc(1, total + 1);
    if (!buf)
        return ESP_ERR_NO_MEM;
    size_t received = 0;
    while (received < total)
    {
        int ret = httpd_req_recv(req, buf + received, total - received);
        if (ret <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
                continue;
            free(buf);
            return ESP_FAIL;
        }
        received += ret;
    }
    buf[received] = '\0';
    *out_buf = buf;
    return ESP_OK;
}

static esp_err_t send_json_response(httpd_req_t *req, cJSON *json)
{
    char *payload = cJSON_PrintUnformatted(json);
    if (!payload)
        return ESP_ERR_NO_MEM;
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_send(req, payload, HTTPD_RESP_USE_STRLEN);
    free(payload);
    return err;
}

static esp_err_t handle_get_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t handle_get_profiles(httpd_req_t *req)
{
    BrewProfileSnapshot snapshot;
    esp_err_t err = BrewProfileStore_GetSnapshot(&snapshot);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get profiles: %s", esp_err_to_name(err));
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to load profiles");
    }
    cJSON *root = cJSON_CreateArray();
    if (!root)
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    for (uint32_t i = 0; i < snapshot.profileCount; ++i)
    {
        const BrewProfileConfig *profile = &snapshot.profiles[i];
        cJSON *profile_obj = cJSON_CreateObject();
        if (!profile_obj)
            goto error;
        cJSON_AddStringToObject(profile_obj, "name", profile->name);
        cJSON *phases = cJSON_CreateArray();
        if (!phases)
        {
            cJSON_Delete(profile_obj);
            goto error;
        }
        cJSON_AddItemToObject(profile_obj, "phases", phases);
        for (uint32_t p = 0; p < profile->phaseCount; ++p)
        {
            const BrewPhaseConfig *phase = &profile->phases[p];
            cJSON *phase_obj = cJSON_CreateObject();
            if (!phase_obj)
                goto error;
            cJSON_AddStringToObject(phase_obj, "name", phase->name);
            cJSON_AddStringToObject(phase_obj, "durationMode", duration_mode_to_string(phase->durationMode));
            cJSON_AddNumberToObject(phase_obj, "durationValue", phase->durationValue);
            cJSON_AddStringToObject(phase_obj, "pumpMode", pump_mode_to_string(phase->pumpMode));
            cJSON_AddNumberToObject(phase_obj, "pumpValue", phase->pumpValue);
            cJSON_AddNumberToObject(phase_obj, "temperatureC", phase->temperatureC);
            cJSON_AddItemToArray(phases, phase_obj);
        }
        cJSON_AddItemToArray(root, profile_obj);
    }
    err = send_json_response(req, root);
    cJSON_Delete(root);
    return err;
error:
    cJSON_Delete(root);
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to encode profiles");
}

static esp_err_t handle_post_profiles(httpd_req_t *req)
{
    char *body = NULL;
    esp_err_t err = read_request_body(req, &body);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_NO_MEM)
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request body");
    }
    BrewProfileConfig profile;
    char error_msg[96];
    err = parse_profile_json(body, &profile, error_msg, sizeof(error_msg));
    free(body);
    if (err != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, error_msg);
    }
    uint32_t index = 0;
    err = BrewProfileStore_AddProfile(&profile, &index);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to add profile: %s", esp_err_to_name(err));
        if (err == ESP_ERR_NO_MEM)
        {
            httpd_resp_set_status(req, "507 Insufficient Storage");
            httpd_resp_set_type(req, "text/plain");
            return httpd_resp_send(req, "Profile storage full", HTTPD_RESP_USE_STRLEN);
        }
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save profile");
    }
    cJSON *response = cJSON_CreateObject();
    if (!response)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }
    cJSON_AddNumberToObject(response, "index", index);
    err = send_json_response(req, response);
    cJSON_Delete(response);
    return err;
}

static esp_err_t handle_put_profiles(httpd_req_t *req)
{
    const char *uri = req->uri;
    const char *prefix = "/api/profiles/";
    size_t prefix_len = strlen(prefix);
    if (strncmp(uri, prefix, prefix_len) != 0)
    {
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
    }
    uint32_t index = (uint32_t)strtoul(uri + prefix_len, NULL, 10);
    char *body = NULL;
    esp_err_t err = read_request_body(req, &body);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_NO_MEM)
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request body");
    }
    BrewProfileConfig profile;
    char error_msg[96];
    err = parse_profile_json(body, &profile, error_msg, sizeof(error_msg));
    free(body);
    if (err != ESP_OK)
    {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, error_msg);
    }
    err = BrewProfileStore_UpdateProfile(index, &profile);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to update profile %u: %s", (unsigned)index, esp_err_to_name(err));
        if (err == ESP_ERR_INVALID_ARG)
            return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Profile not found");
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to update profile");
    }
    cJSON *response = cJSON_CreateObject();
    if (!response)
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    cJSON_AddStringToObject(response, "status", "ok");
    err = send_json_response(req, response);
    cJSON_Delete(response);
    return err;
}

esp_err_t WebServer_Init(void)
{
    if (s_initialized)
        return ESP_OK;
    esp_err_t err = BrewProfileStore_Init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialise profile store: %s", esp_err_to_name(err));
        return err;
    }
    s_initialized = true;
    return ESP_OK;
}

esp_err_t WebServer_Start(void)
{
    if (!s_initialized)
        return ESP_ERR_INVALID_STATE;
    if (s_server)
        return ESP_OK;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;
    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
        s_server = NULL;
        return err;
    }
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = handle_get_root,
        .user_ctx = NULL,
    };
    httpd_uri_t index_uri = {
        .uri = "/index.html",
        .method = HTTP_GET,
        .handler = handle_get_root,
        .user_ctx = NULL,
    };
    httpd_uri_t profiles_get = {
        .uri = "/api/profiles",
        .method = HTTP_GET,
        .handler = handle_get_profiles,
        .user_ctx = NULL,
    };
    httpd_uri_t profiles_post = {
        .uri = "/api/profiles",
        .method = HTTP_POST,
        .handler = handle_post_profiles,
        .user_ctx = NULL,
    };
    httpd_uri_t profiles_put = {
        .uri = "/api/profiles/*",
        .method = HTTP_PUT,
        .handler = handle_put_profiles,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(s_server, &root_uri);
    httpd_register_uri_handler(s_server, &index_uri);
    httpd_register_uri_handler(s_server, &profiles_get);
    httpd_register_uri_handler(s_server, &profiles_post);
    httpd_register_uri_handler(s_server, &profiles_put);
    ESP_LOGI(TAG, "HTTP server started");
    return ESP_OK;
}

void WebServer_Stop(void)
{
    if (s_server)
    {
        httpd_stop(s_server);
        s_server = NULL;
        ESP_LOGI(TAG, "HTTP server stopped");
    }
}

