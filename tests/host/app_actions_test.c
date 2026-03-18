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
    EXPECT_TRUE(app_action_has_effect(&result, APP_EFFECT_SETTINGS_CHANGED));
    EXPECT_TRUE(app_action_has_effect(&result, APP_EFFECT_UI_REFRESH));
    EXPECT_FALSE(app_action_has_effect(&result, APP_EFFECT_RUNTIME_CHANGED));

    result = app_action_save_wifi_credentials(&state, "Office WiFi", "secret");
    EXPECT_TRUE(app_action_has_effect(&result, APP_EFFECT_SETTINGS_CHANGED));
    EXPECT_TRUE(app_action_has_effect(&result, APP_EFFECT_WIFI_COMMAND));
    EXPECT_EQ_INT(APP_WIFI_COMMAND_CONNECT, result.wifi_command);
    EXPECT_STR_EQ("Office WiFi", state.settings.wifi.ssid);
    EXPECT_STR_EQ("secret", result.wifi_password);

    result = app_action_set_ui_click_sound_enabled(&state, false);
    EXPECT_FALSE(state.settings.ui_click_sound_enabled);
    EXPECT_TRUE(app_action_has_effect(&result, APP_EFFECT_SETTINGS_CHANGED));
    EXPECT_TRUE(app_action_has_effect(&result, APP_EFFECT_UI_REFRESH));
    return 0;
}

static int test_alarm_enable_delete_and_invalid_save(void)
{
    app_state_t state = make_state();
    app_action_result_t result;
    time_t now = make_utc_time(2026, 3, 12, 6, 0, 0);

    result = app_action_set_alarm_enabled(&state, 0, true, now);
    EXPECT_TRUE(app_action_has_effect(&result, APP_EFFECT_SETTINGS_CHANGED));
    EXPECT_TRUE(app_action_has_effect(&result, APP_EFFECT_RUNTIME_CHANGED));
    EXPECT_TRUE(app_action_has_effect(&result, APP_EFFECT_BRIGHTNESS_APPLY));
    EXPECT_EQ_INT(0, state.runtime.next_alarm_index);

    result = app_action_delete_alarm(&state, 0, now);
    EXPECT_TRUE(app_action_has_effect(&result, APP_EFFECT_SETTINGS_CHANGED));
    EXPECT_FALSE(state.settings.alarms[0].enabled);

    result = app_action_save_alarm(&state, MAX_ALARMS, &state.settings.alarms[0], now);
    EXPECT_FALSE(app_action_has_effect(&result, APP_EFFECT_SETTINGS_CHANGED));
    EXPECT_FALSE(app_action_has_effect(&result, APP_EFFECT_RUNTIME_CHANGED));
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
    EXPECT_TRUE(app_action_has_effect(&result, APP_EFFECT_SETTINGS_CHANGED));
    EXPECT_TRUE(app_action_has_effect(&result, APP_EFFECT_RUNTIME_CHANGED));
    EXPECT_TRUE(state.cancel_revert_available);
    EXPECT_TRUE(state.runtime.next_alarm_epoch > original_epoch);

    result = app_action_undo_cancel_next_alarm(&state, now);
    EXPECT_TRUE(app_action_has_effect(&result, APP_EFFECT_SETTINGS_CHANGED));
    EXPECT_TRUE(app_action_has_effect(&result, APP_EFFECT_RUNTIME_CHANGED));
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
