#pragma once

#include "esp_err.h"

#include "domain/app_settings_types.h"

void app_settings_set_defaults(app_settings_t *settings);
esp_err_t app_settings_load(app_settings_t *settings);
esp_err_t app_settings_save(const app_settings_t *settings);
void app_settings_apply_timezone(const app_settings_t *settings);
