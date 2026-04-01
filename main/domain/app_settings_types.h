#pragma once

#include <stdint.h>
#include <time.h>

#include "domain/clock_types.h"

typedef struct {
    uint32_t version;
    uint8_t base_brightness;
    uint8_t alarm_volume;
    bool ui_click_sound_enabled;
    bool ascending_alarm_enabled;
    uint8_t snooze_minutes;
    clock_face_id_t current_face;
    uint8_t face_themes[CLOCK_FACE_COUNT];
    wifi_settings_t wifi;
    night_mode_config_t night_mode;
    alarm_config_t alarms[MAX_ALARMS];
    time_t last_synced_epoch;
    time_t skipped_alarm_epoch;
    int8_t skipped_alarm_index;
    uint8_t ui_click_volume;
} app_settings_t;
