#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t WebServer_Init(void);
esp_err_t WebServer_Start(void);
void WebServer_Stop(void);

#ifdef __cplusplus
}
#endif

