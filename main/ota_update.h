#pragma once

#include <stdbool.h>

#include "esp_err.h"

#include "domain/app_settings_types.h"

/* network_available: the Wi-Fi started. A new image waiting to be confirmed is confirmed only
 * then; without it, it is rolled back (one reboot, into the image it replaced). */
esp_err_t ota_update_init(bool network_available);
void ota_update_snapshot(app_runtime_state_t *runtime);
esp_err_t ota_update_request_check(void);
esp_err_t ota_update_request_install(void);
