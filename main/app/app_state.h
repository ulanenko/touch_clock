#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "domain/app_settings_types.h"

typedef struct {
    app_settings_t settings;
    app_runtime_state_t runtime;
    int64_t save_deadline_ms;
    int64_t ota_next_check_ms;
    int64_t cancel_revert_deadline_ms;
    int64_t brightness_fade_start_ms;
    int64_t brightness_fade_duration_ms;
    uint8_t applied_brightness;
    uint16_t applied_permille; /* the sunrise's finer level last set; 0 when not rising */
    /* The sunrise's preview (Settings > Night > Sunrise): how far through it (above 1000, the
     * alarm), and its chime played. */
    uint16_t sunrise_preview_progress;
    /* The time zone and automatic time sync last handed to the services: applied again only
     * when they change (a sync restart on every settings change, every slider step, sent the
     * network a time request each). */
    uint8_t applied_timezone_id;
    bool applied_auto_sync;
    bool applied_time_known;
    /* The alarm sounds last handed to the audio service to keep rendered. */
    uint8_t applied_sound_mask;
    bool applied_sounds_known;
    bool sunrise_preview;
    bool sunrise_preview_chimed;
    uint8_t brightness_fade_start;
    uint8_t brightness_fade_target;
    uint8_t temporary_brightness_floor;
    time_t alarm_auto_stop_deadline;
    bool audio_available;
    /* The services that started at boot; one that did not is left alone after. */
    bool wifi_available;
    bool ota_available;
    bool telemetry_available;
    bool settings_dirty;
    bool brightness_fade_active;
    bool temporary_brightness_floor_active;
    bool alarm_auto_stop_armed;
    bool cancel_revert_available;
    bool cancel_revert_was_one_time;
    bool cancel_revert_was_enabled;
    int8_t cancel_revert_alarm_index;
    time_t cancel_revert_alarm_epoch;
} app_state_t;
