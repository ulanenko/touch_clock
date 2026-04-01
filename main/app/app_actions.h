#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "app/app_state.h"

typedef enum {
    APP_WIFI_COMMAND_NONE = 0,
    APP_WIFI_COMMAND_SCAN,
    APP_WIFI_COMMAND_CONNECT,
    APP_WIFI_COMMAND_FORGET,
    APP_WIFI_COMMAND_SYNC,
} app_wifi_command_type_t;

typedef enum {
    APP_EFFECT_NONE = 0,
    APP_EFFECT_SETTINGS_CHANGED = 1U << 0,
    APP_EFFECT_RUNTIME_CHANGED = 1U << 1,
    APP_EFFECT_UI_REFRESH = 1U << 2,
    APP_EFFECT_BRIGHTNESS_APPLY = 1U << 3,
    APP_EFFECT_AUDIO_RECONCILE = 1U << 4,
    APP_EFFECT_AUDIO_VOLUME = 1U << 5,
    APP_EFFECT_WIFI_COMMAND = 1U << 6,
} app_effect_flags_t;

typedef struct {
    uint32_t effects;
    app_wifi_command_type_t wifi_command;
    char wifi_ssid[33];
    char wifi_password[65];
} app_action_result_t;

static inline bool app_action_has_effect(const app_action_result_t *result, app_effect_flags_t effect)
{
    return result != NULL && (result->effects & (uint32_t)effect) != 0;
}

void app_action_result_init(app_action_result_t *result);
app_action_result_t app_action_set_base_brightness(app_state_t *state, uint8_t hw_percent, time_t now);
app_action_result_t app_action_set_runtime_night_brightness_override(app_state_t *state,
                                                                     uint8_t hw_percent,
                                                                     time_t now);
app_action_result_t app_action_set_night_brightness(app_state_t *state, uint8_t hw_percent, time_t now);
app_action_result_t app_action_set_night_sunrise_brightness_enabled(app_state_t *state, bool enabled, time_t now);
app_action_result_t app_action_set_ui_click_sound_enabled(app_state_t *state, bool enabled);
app_action_result_t app_action_set_ui_click_volume(app_state_t *state, uint8_t volume);
app_action_result_t app_action_set_current_face(app_state_t *state, clock_face_id_t face);
app_action_result_t app_action_set_face_theme(app_state_t *state, clock_face_id_t face, uint8_t theme);
app_action_result_t app_action_set_night_face(app_state_t *state, clock_face_id_t face);
app_action_result_t app_action_set_timezone(app_state_t *state, int8_t utc_offset_hours, time_t now);
app_action_result_t app_action_set_night_mode_enabled(app_state_t *state, bool enabled, time_t now);
app_action_result_t app_action_set_night_schedule(app_state_t *state,
                                                  uint8_t start_hour,
                                                  uint8_t start_minute,
                                                  uint8_t end_hour,
                                                  uint8_t end_minute,
                                                  time_t now);
app_action_result_t app_action_save_wifi_credentials(app_state_t *state, const char *ssid, const char *password);
app_action_result_t app_action_forget_wifi(app_state_t *state);
app_action_result_t app_action_request_wifi_scan(app_state_t *state);
app_action_result_t app_action_request_time_sync(app_state_t *state);
app_action_result_t app_action_set_alarm_volume(app_state_t *state, uint8_t volume, time_t now);
app_action_result_t app_action_set_ascending_alarm_enabled(app_state_t *state, bool enabled);
app_action_result_t app_action_set_snooze_minutes(app_state_t *state, uint8_t minutes);
app_action_result_t app_action_set_alarm_enabled(app_state_t *state, uint8_t index, bool enabled, time_t now);
app_action_result_t app_action_save_alarm(app_state_t *state, uint8_t index, const alarm_config_t *alarm, time_t now);
app_action_result_t app_action_delete_alarm(app_state_t *state, uint8_t index, time_t now);
app_action_result_t app_action_alarm_snooze(app_state_t *state, time_t now);
app_action_result_t app_action_alarm_stop(app_state_t *state);
app_action_result_t app_action_alarm_test_toggle(app_state_t *state);
app_action_result_t app_action_cancel_next_alarm(app_state_t *state, time_t now);
app_action_result_t app_action_undo_cancel_next_alarm(app_state_t *state, time_t now);
