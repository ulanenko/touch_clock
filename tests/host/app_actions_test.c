#include <string.h>

#include "app/app_actions.h"
#include "domain/alarm_scheduler.h"
#include "domain/settings_policy.h"
#include "test_support.h"

static app_state_t make_state(void)
{
    app_state_t state;

    memset(&state, 0, sizeof(state));
    settings_policy_set_defaults(&state.settings);
    alarm_scheduler_init(&state.runtime, &state.settings);
    return state;
}

static int test_face_and_wifi_actions(void)
{
    app_state_t state = make_state();
    app_action_result_t result;

    result = app_action_set_current_face(&state, CLOCK_FACE_MATRIX);
    EXPECT_EQ_INT(CLOCK_FACE_MATRIX, state.settings.current_face);
    EXPECT_TRUE(result.settings_changed);
    EXPECT_TRUE(result.needs_save);
    EXPECT_TRUE(result.needs_ui_refresh);
    EXPECT_FALSE(result.runtime_changed);

    result = app_action_save_wifi_credentials(&state, "Office WiFi", "secret");
    EXPECT_TRUE(result.settings_changed);
    EXPECT_TRUE(result.needs_wifi_command);
    EXPECT_EQ_INT(APP_WIFI_COMMAND_CONNECT, result.wifi_command);
    EXPECT_STR_EQ("Office WiFi", state.settings.wifi.ssid);
    EXPECT_STR_EQ("secret", result.wifi_password);
    return 0;
}

static int test_alarm_enable_delete_and_invalid_save(void)
{
    app_state_t state = make_state();
    app_action_result_t result;
    time_t now = make_utc_time(2026, 3, 12, 6, 0, 0);

    result = app_action_set_alarm_enabled(&state, 0, true, now);
    EXPECT_TRUE(result.settings_changed);
    EXPECT_TRUE(result.runtime_changed);
    EXPECT_TRUE(result.needs_brightness_apply);
    EXPECT_EQ_INT(0, state.runtime.next_alarm_index);

    result = app_action_delete_alarm(&state, 0, now);
    EXPECT_TRUE(result.settings_changed);
    EXPECT_FALSE(state.settings.alarms[0].enabled);

    result = app_action_save_alarm(&state, MAX_ALARMS, &state.settings.alarms[0], now);
    EXPECT_FALSE(result.settings_changed);
    EXPECT_FALSE(result.runtime_changed);
    return 0;
}

static int test_cancel_next_and_undo(void)
{
    app_state_t state = make_state();
    app_action_result_t result;
    time_t now = make_utc_time(2026, 3, 12, 7, 0, 0);
    time_t original_epoch;

    state.settings.alarms[0].enabled = true;
    state.settings.alarms[0].hour = 8;
    state.settings.alarms[0].minute = 30;
    state.settings.alarms[0].repeat_mode = ALARM_REPEAT_WEEKLY;
    state.settings.alarms[0].days_mask = (uint8_t)(1U << 5);
    alarm_scheduler_tick(&state.runtime, &state.settings, now);
    original_epoch = state.runtime.next_alarm_epoch;

    result = app_action_cancel_next_alarm(&state, now);
    EXPECT_TRUE(result.settings_changed);
    EXPECT_TRUE(result.runtime_changed);
    EXPECT_TRUE(state.cancel_revert_available);
    EXPECT_TRUE(state.runtime.next_alarm_epoch > original_epoch);

    result = app_action_undo_cancel_next_alarm(&state, now);
    EXPECT_TRUE(result.settings_changed);
    EXPECT_TRUE(result.runtime_changed);
    EXPECT_EQ_INT(original_epoch, state.runtime.next_alarm_epoch);
    EXPECT_FALSE(state.cancel_revert_available);
    return 0;
}

int main(void)
{
    int status;

    test_use_utc();

    status = test_face_and_wifi_actions();
    if (status != 0) {
        return status;
    }

    status = test_alarm_enable_delete_and_invalid_save();
    if (status != 0) {
        return status;
    }

    return test_cancel_next_and_undo();
}
