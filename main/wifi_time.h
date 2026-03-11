#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_wifi_types_generic.h"

#include "app_settings.h"

#define WIFI_TIME_MAX_SCAN_RESULTS 16

typedef struct {
    char ssid[33];
    int rssi;
    wifi_auth_mode_t authmode;
} wifi_scan_result_t;

esp_err_t wifi_time_init(const app_settings_t *settings);
void wifi_time_snapshot(app_runtime_state_t *runtime);
esp_err_t wifi_time_start_scan(void);
esp_err_t wifi_time_connect(const char *ssid, const char *password);
esp_err_t wifi_time_forget(void);
esp_err_t wifi_time_request_sync(void);
uint32_t wifi_time_get_scan_generation(void);
bool wifi_time_is_scanning(void);
size_t wifi_time_get_scan_results(wifi_scan_result_t *results, size_t max_results, uint32_t *generation);
