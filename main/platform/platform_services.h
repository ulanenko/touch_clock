#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "domain/app_settings_types.h"

typedef struct {
    time_t (*now)(void);
    void (*set_epoch)(time_t epoch);
    void (*apply_timezone)(uint8_t timezone_id);
} clock_time_service_t;

typedef struct {
    int (*init)(uint8_t volume);
    void (*set_volume)(uint8_t volume);
    void (*set_ascending_enabled)(bool enabled);
    int (*start_alarm)(uint8_t volume, uint8_t sound);
    int (*start_test)(uint8_t volume, uint8_t sound);
    int (*play_ui_click)(uint8_t volume);
    int (*play_start_chime)(uint8_t volume);
    int (*play_sunrise_chime)(uint8_t volume); /* Dawn, a minute before a sunrise's alarm */
    void (*stop)(void);
    bool (*is_alarm_active)(void);
    bool (*is_test_active)(void);
    /* The synthesised sounds (bit per alarm_sound_t) the alarms use: kept rendered; the rest
     * are rendered when previewed and freed after. */
    void (*keep_sounds)(uint8_t mask);
} audio_service_t;

typedef struct {
    int (*init)(const app_settings_t *settings);
    /* snapshot must populate both connection/runtime state and the latest scan cache */
    void (*snapshot)(app_runtime_state_t *runtime);
    int (*start_scan)(void);
    int (*connect)(const char *ssid, const char *password);
    int (*forget)(void);
    int (*request_sync)(void);
    int (*set_auto_sync)(bool enabled);
} wifi_service_t;

typedef struct {
    int (*set_brightness)(uint8_t hw_percent);
    /* The backlight from off up to hw_percent over ms (0: off at once), smoothly and off the
     * UI thread. set_brightness ends one under way. */
    void (*fade_in)(uint8_t hw_percent, uint32_t ms);
    /* The backlight in per mille of full (finer than set_brightness's percent). */
    int (*set_level_permille)(uint16_t permille);
} display_service_t;

typedef struct {
    /* network_available: the Wi-Fi started. A new image that came up without it could never be
     * updated again, so it is not kept. */
    int (*init)(bool network_available);
    void (*snapshot)(app_runtime_state_t *runtime);
    int (*request_check)(void);
    int (*request_install)(void);
} ota_service_t;

typedef struct {
    int (*init)(void);
    void (*tick)(const app_runtime_state_t *runtime, const app_settings_t *settings, time_t now);
} telemetry_service_t;

typedef struct {
    int (*load)(app_settings_t *settings);
    int (*save)(const app_settings_t *settings);
} settings_store_t;
