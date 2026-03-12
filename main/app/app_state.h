#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "domain/app_settings_types.h"

typedef struct {
    app_settings_t settings;
    app_runtime_state_t runtime;
    int64_t save_deadline_ms;
    int64_t cancel_revert_deadline_ms;
    uint8_t applied_brightness;
    bool audio_available;
    bool settings_dirty;
    bool cancel_revert_available;
    bool cancel_revert_was_one_time;
    bool cancel_revert_was_enabled;
    int8_t cancel_revert_alarm_index;
    time_t cancel_revert_alarm_epoch;
} app_state_t;
