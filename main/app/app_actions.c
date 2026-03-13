#include "app/app_actions.h"

#include <stdio.h>
#include <string.h>

#include "domain/alarm_scheduler.h"
#include "domain/brightness_policy.h"
#include "domain/face_catalog.h"
#include "domain/settings_policy.h"

static void add_effect(app_action_result_t *result, app_effect_flags_t effect)
{
    result->effects |= (uint32_t)effect;
}

static void emit_settings_changed(app_action_result_t *result)
{
    add_effect(result, APP_EFFECT_SETTINGS_CHANGED);
    add_effect(result, APP_EFFECT_UI_REFRESH);
}

static void emit_runtime_changed(app_action_result_t *result)
{
    add_effect(result, APP_EFFECT_RUNTIME_CHANGED);
    add_effect(result, APP_EFFECT_UI_REFRESH);
}

static void recompute_runtime_state(app_state_t *state, app_action_result_t *result, time_t now)
{
    if (alarm_scheduler_tick(&state->runtime, &state->settings, now)) {
        emit_settings_changed(result);
    }

    state->runtime.effective_brightness =
        brightness_policy_get_target(&state->runtime, &state->settings, now);
}

static void refresh_runtime_after_settings_change(app_state_t *state, app_action_result_t *result, time_t now)
{
    recompute_runtime_state(state, result, now);
    emit_settings_changed(result);
    emit_runtime_changed(result);
    add_effect(result, APP_EFFECT_BRIGHTNESS_APPLY);
}

static void refresh_runtime_after_runtime_change(app_state_t *state, app_action_result_t *result, time_t now)
{
    recompute_runtime_state(state, result, now);
    emit_runtime_changed(result);
    add_effect(result, APP_EFFECT_BRIGHTNESS_APPLY);
}

static void clear_cancel_revert_state(app_state_t *state)
{
    state->cancel_revert_available = false;
    state->cancel_revert_alarm_index = -1;
    state->cancel_revert_alarm_epoch = 0;
    state->cancel_revert_was_one_time = false;
    state->cancel_revert_was_enabled = false;
}

static void clear_cancelled_occurrence_for_alarm(app_state_t *state, uint8_t alarm_index)
{
    if (alarm_index >= MAX_ALARMS) {
        return;
    }

    if (state->settings.skipped_alarm_index == (int8_t)alarm_index) {
        state->settings.skipped_alarm_index = -1;
        state->settings.skipped_alarm_epoch = 0;
    }

    if (state->cancel_revert_available && state->cancel_revert_alarm_index == (int8_t)alarm_index) {
        clear_cancel_revert_state(state);
    }
}

static void set_wifi_command(app_action_result_t *result, app_wifi_command_type_t command)
{
    add_effect(result, APP_EFFECT_WIFI_COMMAND);
    result->wifi_command = command;
}

void app_action_result_init(app_action_result_t *result)
{
    memset(result, 0, sizeof(*result));
}

app_action_result_t app_action_set_base_brightness(app_state_t *state, uint8_t hw_percent, time_t now)
{
    app_action_result_t result;

    app_action_result_init(&result);
    if (state->settings.base_brightness == hw_percent) {
        return result;
    }

    state->settings.base_brightness = hw_percent;
    settings_policy_sanitize(&state->settings);
    recompute_runtime_state(state, &result, now);
    emit_settings_changed(&result);
    emit_runtime_changed(&result);
    add_effect(&result, APP_EFFECT_BRIGHTNESS_APPLY);
    return result;
}

app_action_result_t app_action_set_runtime_night_brightness_override(app_state_t *state,
                                                                     uint8_t hw_percent,
                                                                     time_t now)
{
    app_action_result_t result;

    app_action_result_init(&result);
    state->runtime.night_brightness_override = hw_percent;
    state->runtime.night_brightness_override_active =
        (hw_percent != state->settings.night_mode.brightness);
    if (!state->runtime.night_brightness_override_active) {
        state->runtime.night_brightness_override = state->settings.night_mode.brightness;
    }
    refresh_runtime_after_runtime_change(state, &result, now);
    return result;
}

app_action_result_t app_action_set_night_brightness(app_state_t *state, uint8_t hw_percent, time_t now)
{
    app_action_result_t result;

    app_action_result_init(&result);
    if (state->settings.night_mode.brightness == hw_percent) {
        return result;
    }

    state->settings.night_mode.brightness = hw_percent;
    settings_policy_sanitize(&state->settings);
    if (state->runtime.night_brightness_override_active) {
        state->runtime.night_brightness_override = hw_percent;
    }
    recompute_runtime_state(state, &result, now);
    emit_settings_changed(&result);
    emit_runtime_changed(&result);
    if (state->runtime.in_night_mode) {
        add_effect(&result, APP_EFFECT_BRIGHTNESS_APPLY);
    }
    return result;
}

app_action_result_t app_action_set_current_face(app_state_t *state, clock_face_id_t face)
{
    app_action_result_t result;

    app_action_result_init(&result);
    if (!face_catalog_is_enabled(face) || state->settings.current_face == face) {
        return result;
    }

    state->settings.current_face = face;
    emit_settings_changed(&result);
    return result;
}

app_action_result_t app_action_set_night_face(app_state_t *state, clock_face_id_t face)
{
    app_action_result_t result;

    app_action_result_init(&result);
    if (!face_catalog_is_enabled(face) || state->settings.night_mode.face == face) {
        return result;
    }

    state->settings.night_mode.face = face;
    emit_settings_changed(&result);
    return result;
}

app_action_result_t app_action_set_timezone(app_state_t *state, int8_t utc_offset_hours, time_t now)
{
    app_action_result_t result;

    app_action_result_init(&result);
    if (state->settings.wifi.timezone_offset_hours == utc_offset_hours) {
        return result;
    }

    state->settings.wifi.timezone_offset_hours = utc_offset_hours;
    settings_policy_sanitize(&state->settings);
    recompute_runtime_state(state, &result, now);
    emit_settings_changed(&result);
    emit_runtime_changed(&result);
    add_effect(&result, APP_EFFECT_BRIGHTNESS_APPLY);
    return result;
}

app_action_result_t app_action_set_night_mode_enabled(app_state_t *state, bool enabled, time_t now)
{
    app_action_result_t result;

    app_action_result_init(&result);
    if (state->settings.night_mode.enabled == enabled) {
        return result;
    }

    state->settings.night_mode.enabled = enabled;
    settings_policy_sanitize(&state->settings);
    refresh_runtime_after_settings_change(state, &result, now);
    return result;
}

app_action_result_t app_action_set_night_schedule(app_state_t *state,
                                                  uint8_t start_hour,
                                                  uint8_t start_minute,
                                                  uint8_t end_hour,
                                                  uint8_t end_minute,
                                                  time_t now)
{
    app_action_result_t result;

    app_action_result_init(&result);
    if (state->settings.night_mode.start_hour == start_hour &&
        state->settings.night_mode.start_minute == start_minute &&
        state->settings.night_mode.end_hour == end_hour &&
        state->settings.night_mode.end_minute == end_minute) {
        return result;
    }

    state->settings.night_mode.start_hour = start_hour;
    state->settings.night_mode.start_minute = start_minute;
    state->settings.night_mode.end_hour = end_hour;
    state->settings.night_mode.end_minute = end_minute;
    settings_policy_sanitize(&state->settings);
    refresh_runtime_after_settings_change(state, &result, now);
    return result;
}

app_action_result_t app_action_save_wifi_credentials(app_state_t *state, const char *ssid, const char *password)
{
    app_action_result_t result;

    app_action_result_init(&result);
    snprintf(state->settings.wifi.ssid, sizeof(state->settings.wifi.ssid), "%s", ssid ? ssid : "");
    snprintf(state->settings.wifi.password, sizeof(state->settings.wifi.password), "%s", password ? password : "");
    emit_settings_changed(&result);
    set_wifi_command(&result, APP_WIFI_COMMAND_CONNECT);
    snprintf(result.wifi_ssid, sizeof(result.wifi_ssid), "%s", state->settings.wifi.ssid);
    snprintf(result.wifi_password, sizeof(result.wifi_password), "%s", state->settings.wifi.password);
    return result;
}

app_action_result_t app_action_forget_wifi(app_state_t *state)
{
    app_action_result_t result;

    app_action_result_init(&result);
    state->settings.wifi.ssid[0] = '\0';
    state->settings.wifi.password[0] = '\0';
    emit_settings_changed(&result);
    set_wifi_command(&result, APP_WIFI_COMMAND_FORGET);
    return result;
}

app_action_result_t app_action_request_wifi_scan(app_state_t *state)
{
    app_action_result_t result;

    (void)state;
    app_action_result_init(&result);
    set_wifi_command(&result, APP_WIFI_COMMAND_SCAN);
    return result;
}

app_action_result_t app_action_request_time_sync(app_state_t *state)
{
    app_action_result_t result;

    (void)state;
    app_action_result_init(&result);
    set_wifi_command(&result, APP_WIFI_COMMAND_SYNC);
    return result;
}

app_action_result_t app_action_set_alarm_volume(app_state_t *state, uint8_t volume, time_t now)
{
    app_action_result_t result;

    app_action_result_init(&result);
    if (state->settings.alarm_volume == volume) {
        return result;
    }

    state->settings.alarm_volume = volume;
    settings_policy_sanitize(&state->settings);
    recompute_runtime_state(state, &result, now);
    emit_settings_changed(&result);
    emit_runtime_changed(&result);
    add_effect(&result, APP_EFFECT_AUDIO_VOLUME);
    add_effect(&result, APP_EFFECT_BRIGHTNESS_APPLY);
    return result;
}

app_action_result_t app_action_set_snooze_minutes(app_state_t *state, uint8_t minutes)
{
    app_action_result_t result;

    app_action_result_init(&result);
    if (state->settings.snooze_minutes == minutes) {
        return result;
    }

    state->settings.snooze_minutes = minutes;
    settings_policy_sanitize(&state->settings);
    emit_settings_changed(&result);
    return result;
}

app_action_result_t app_action_set_alarm_enabled(app_state_t *state, uint8_t index, bool enabled, time_t now)
{
    app_action_result_t result;

    app_action_result_init(&result);
    if (index >= MAX_ALARMS || state->settings.alarms[index].enabled == enabled) {
        return result;
    }

    state->settings.alarms[index].enabled = enabled;
    clear_cancelled_occurrence_for_alarm(state, index);
    refresh_runtime_after_settings_change(state, &result, now);
    return result;
}

app_action_result_t app_action_save_alarm(app_state_t *state, uint8_t index, const alarm_config_t *alarm, time_t now)
{
    app_action_result_t result;

    app_action_result_init(&result);
    if (index >= MAX_ALARMS || alarm == NULL) {
        return result;
    }

    state->settings.alarms[index] = *alarm;
    settings_policy_sanitize(&state->settings);
    clear_cancelled_occurrence_for_alarm(state, index);
    refresh_runtime_after_settings_change(state, &result, now);
    return result;
}

app_action_result_t app_action_delete_alarm(app_state_t *state, uint8_t index, time_t now)
{
    alarm_config_t empty_alarm = {
        .enabled = false,
        .hour = 7,
        .minute = 0,
        .days_mask = 0x7F,
        .repeat_mode = ALARM_REPEAT_WEEKLY,
    };

    return app_action_save_alarm(state, index, &empty_alarm, now);
}

app_action_result_t app_action_alarm_snooze(app_state_t *state, time_t now)
{
    app_action_result_t result;

    app_action_result_init(&result);
    alarm_scheduler_snooze(&state->runtime, &state->settings, now);
    refresh_runtime_after_runtime_change(state, &result, now);
    add_effect(&result, APP_EFFECT_AUDIO_RECONCILE);
    return result;
}

app_action_result_t app_action_alarm_stop(app_state_t *state)
{
    app_action_result_t result;

    app_action_result_init(&result);
    alarm_scheduler_stop(&state->runtime);
    emit_runtime_changed(&result);
    add_effect(&result, APP_EFFECT_BRIGHTNESS_APPLY);
    add_effect(&result, APP_EFFECT_AUDIO_RECONCILE);
    return result;
}

app_action_result_t app_action_alarm_test_toggle(app_state_t *state)
{
    app_action_result_t result;

    app_action_result_init(&result);
    add_effect(&result, APP_EFFECT_AUDIO_RECONCILE);
    add_effect(&result, APP_EFFECT_UI_REFRESH);
    (void)state;
    return result;
}

app_action_result_t app_action_cancel_next_alarm(app_state_t *state, time_t now)
{
    app_action_result_t result;
    int8_t alarm_index = state->runtime.next_alarm_index;
    time_t alarm_epoch = state->runtime.next_alarm_epoch;

    app_action_result_init(&result);
    if (state->runtime.snooze_active) {
        alarm_scheduler_stop(&state->runtime);
        clear_cancel_revert_state(state);
        emit_settings_changed(&result);
        emit_runtime_changed(&result);
        add_effect(&result, APP_EFFECT_AUDIO_RECONCILE);
        add_effect(&result, APP_EFFECT_BRIGHTNESS_APPLY);
        return result;
    }

    if (alarm_index < 0 ||
        alarm_index >= MAX_ALARMS ||
        alarm_epoch <= now ||
        !alarm_scheduler_cancel_next(&state->runtime, &state->settings, now)) {
        return result;
    }

    state->cancel_revert_available = true;
    state->cancel_revert_alarm_index = alarm_index;
    state->cancel_revert_alarm_epoch = alarm_epoch;
    state->cancel_revert_was_one_time =
        (state->settings.alarms[alarm_index].repeat_mode == ALARM_REPEAT_ONCE);
    state->cancel_revert_was_enabled = true;
    recompute_runtime_state(state, &result, now);
    emit_settings_changed(&result);
    emit_runtime_changed(&result);
    add_effect(&result, APP_EFFECT_BRIGHTNESS_APPLY);
    return result;
}

app_action_result_t app_action_undo_cancel_next_alarm(app_state_t *state, time_t now)
{
    app_action_result_t result;

    app_action_result_init(&result);
    if (!state->cancel_revert_available ||
        state->cancel_revert_alarm_index < 0 ||
        state->cancel_revert_alarm_index >= MAX_ALARMS) {
        return result;
    }

    if (state->cancel_revert_was_one_time) {
        state->settings.alarms[state->cancel_revert_alarm_index].enabled = state->cancel_revert_was_enabled;
    } else if (state->settings.skipped_alarm_index == state->cancel_revert_alarm_index &&
               state->settings.skipped_alarm_epoch == state->cancel_revert_alarm_epoch) {
        state->settings.skipped_alarm_index = -1;
        state->settings.skipped_alarm_epoch = 0;
    }

    state->runtime.next_alarm_epoch = state->cancel_revert_alarm_epoch;
    state->runtime.next_alarm_index = state->cancel_revert_alarm_index;
    clear_cancel_revert_state(state);
    recompute_runtime_state(state, &result, now);
    emit_settings_changed(&result);
    emit_runtime_changed(&result);
    add_effect(&result, APP_EFFECT_BRIGHTNESS_APPLY);
    return result;
}
