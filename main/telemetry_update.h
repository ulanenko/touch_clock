#pragma once

#include "esp_err.h"

#include "domain/app_settings_types.h"

esp_err_t telemetry_update_init(void);
void telemetry_update_tick(const app_runtime_state_t *runtime, const app_settings_t *settings, time_t now);
