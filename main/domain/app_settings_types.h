#pragma once

#include <stdint.h>
#include <time.h>

#include "domain/clock_types.h"

typedef struct {
    uint32_t version;
    uint8_t base_brightness;
    uint8_t alarm_volume;
    uint8_t snooze_minutes;
    clock_face_id_t current_face;
    wifi_settings_t wifi;
    night_mode_config_t night_mode;
    alarm_config_t alarms[MAX_ALARMS];
    time_t last_synced_epoch;
    time_t skipped_alarm_epoch;
    int8_t skipped_alarm_index;
} app_settings_t;
