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
    uint8_t brightness_fade_start;
    uint8_t brightness_fade_target;
    uint8_t temporary_brightness_floor;
    time_t alarm_auto_stop_deadline;
    bool audio_available;
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
