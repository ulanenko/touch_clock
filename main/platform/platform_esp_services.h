#pragma once

#include "platform/platform_services.h"

const clock_time_service_t *platform_esp_clock_time_service(void);
const audio_service_t *platform_esp_audio_service(void);
const wifi_service_t *platform_esp_wifi_service(void);
const display_service_t *platform_esp_display_service(void);
const ota_service_t *platform_esp_ota_service(void);
const telemetry_service_t *platform_esp_telemetry_service(void);
const settings_store_t *platform_esp_settings_store(void);
