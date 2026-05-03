#pragma once

#include "esp_err.h"

#include "domain/app_settings_types.h"

esp_err_t ota_update_init(void);
void ota_update_snapshot(app_runtime_state_t *runtime);
esp_err_t ota_update_request_check(void);
esp_err_t ota_update_request_install(void);
