#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "domain/app_settings_types.h"

typedef struct {
    time_t (*now)(void);
    void (*set_epoch)(time_t epoch);
    void (*apply_timezone)(int8_t utc_offset_hours);
} clock_time_service_t;

typedef struct {
    int (*init)(uint8_t volume);
    void (*set_volume)(uint8_t volume);
    void (*set_ascending_enabled)(bool enabled);
    int (*start_alarm)(uint8_t volume);
    int (*start_test)(uint8_t volume);
    int (*play_ui_click)(uint8_t volume);
    void (*stop)(void);
    bool (*is_alarm_active)(void);
    bool (*is_test_active)(void);
} audio_service_t;

typedef struct {
    int (*init)(const app_settings_t *settings);
    /* snapshot must populate both connection/runtime state and the latest scan cache */
    void (*snapshot)(app_runtime_state_t *runtime);
    int (*start_scan)(void);
    int (*connect)(const char *ssid, const char *password);
    int (*forget)(void);
    int (*request_sync)(void);
} wifi_service_t;

typedef struct {
    int (*set_brightness)(uint8_t hw_percent);
} display_service_t;

typedef struct {
    int (*load)(app_settings_t *settings);
    int (*save)(const app_settings_t *settings);
} settings_store_t;
