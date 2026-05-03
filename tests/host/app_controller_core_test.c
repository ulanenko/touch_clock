#include <string.h>

#include "app/app_controller_core.h"
#include "domain/alarm_scheduler.h"
#include "domain/brightness_policy.h"
#include "domain/settings_policy.h"
#include "domain/timezone_rules.h"
#include "test_support.h"

typedef struct {
    time_t now;
    time_t set_epoch_value;
    uint8_t applied_timezone;
    bool auto_sync_enabled;
    int wifi_set_auto_sync_calls;
    uint8_t brightness_value;
    int display_set_calls;
    int audio_init_calls;
    int audio_set_volume_calls;
    int audio_set_ascending_calls;
    int audio_start_alarm_calls;
    int audio_start_test_calls;
    int audio_stop_calls;
    uint8_t audio_last_volume;
    bool audio_ascending_enabled;
    bool alarm_active;
    bool test_active;
    int wifi_init_calls;
    int wifi_scan_calls;
    int wifi_connect_calls;
    int wifi_forget_calls;
    int wifi_sync_calls;
    char wifi_connect_ssid[33];
    char wifi_connect_password[65];
    app_runtime_state_t wifi_snapshot;
    int load_calls;
    int save_calls;
    int load_result;
    int save_result;
    app_settings_t stored_settings;
    int64_t monotonic_ms;
    int ui_refresh_calls;
} fake_env_t;

static fake_env_t *g_env;

static time_t fake_now(void)
{
    return g_env->now;
}

static void fake_set_epoch(time_t epoch)
{
    g_env->set_epoch_value = epoch;
}

static void fake_apply_timezone(uint8_t timezone_id)
{
    g_env->applied_timezone = timezone_id;
}

static int fake_audio_init(uint8_t volume)
{
    g_env->audio_init_calls += 1;
    g_env->audio_last_volume = volume;
    return 0;
}

static void fake_audio_set_volume(uint8_t volume)
{
    g_env->audio_set_volume_calls += 1;
    g_env->audio_last_volume = volume;
}

static void fake_audio_set_ascending_enabled(bool enabled)
{
    g_env->audio_set_ascending_calls += 1;
    g_env->audio_ascending_enabled = enabled;
}

static int fake_audio_start_alarm(uint8_t volume)
{
    g_env->audio_start_alarm_calls += 1;
    g_env->audio_last_volume = volume;
    g_env->alarm_active = true;
    return 0;
}

static int fake_audio_start_test(uint8_t volume)
{
    g_env->audio_start_test_calls += 1;
    g_env->audio_last_volume = volume;
    g_env->test_active = true;
    return 0;
}

static int fake_audio_play_ui_click(uint8_t volume)
{
    g_env->audio_last_volume = volume;
    return 0;
}

static void fake_audio_stop(void)
{
    g_env->audio_stop_calls += 1;
    g_env->alarm_active = false;
    g_env->test_active = false;
}

static bool fake_audio_is_alarm_active(void)
{
    return g_env->alarm_active;
}

static bool fake_audio_is_test_active(void)
{
    return g_env->test_active;
}

static int fake_wifi_init(const app_settings_t *settings)
{
    g_env->wifi_init_calls += 1;
    EXPECT_STR_EQ(g_env->stored_settings.wifi.ssid, settings->wifi.ssid);
    return 0;
}

static void fake_wifi_snapshot(app_runtime_state_t *runtime)
{
    *runtime = g_env->wifi_snapshot;
}

static int fake_wifi_scan(void)
{
    g_env->wifi_scan_calls += 1;
    return 0;
}

static int fake_wifi_connect(const char *ssid, const char *password)
{
    g_env->wifi_connect_calls += 1;
    snprintf(g_env->wifi_connect_ssid, sizeof(g_env->wifi_connect_ssid), "%s", ssid);
    snprintf(g_env->wifi_connect_password, sizeof(g_env->wifi_connect_password), "%s", password);
    return 0;
}

static int fake_wifi_forget(void)
{
    g_env->wifi_forget_calls += 1;
    return 0;
}

static int fake_wifi_sync(void)
{
    g_env->wifi_sync_calls += 1;
    return 0;
}

static int fake_wifi_set_auto_sync(bool enabled)
{
    g_env->wifi_set_auto_sync_calls += 1;
    g_env->auto_sync_enabled = enabled;
    return 0;
}

static int fake_display_set_brightness(uint8_t hw_percent)
{
    g_env->display_set_calls += 1;
    g_env->brightness_value = hw_percent;
    return 0;
}

static int fake_settings_load(app_settings_t *settings)
{
    g_env->load_calls += 1;
    *settings = g_env->stored_settings;
    return g_env->load_result;
}

static int fake_settings_save(const app_settings_t *settings)
{
    g_env->save_calls += 1;
    g_env->stored_settings = *settings;
    return g_env->save_result;
}

static int64_t fake_monotonic_ms(void *ctx)
{
    (void)ctx;
    return g_env->monotonic_ms;
}

static void fake_ui_refresh(void *ctx)
{
    (void)ctx;
    g_env->ui_refresh_calls += 1;
}

static const clock_time_service_t s_clock_service = {
    .now = fake_now,
    .set_epoch = fake_set_epoch,
    .apply_timezone = fake_apply_timezone,
};

static const audio_service_t s_audio_service = {
    .init = fake_audio_init,
    .set_volume = fake_audio_set_volume,
    .set_ascending_enabled = fake_audio_set_ascending_enabled,
    .start_alarm = fake_audio_start_alarm,
    .start_test = fake_audio_start_test,
    .play_ui_click = fake_audio_play_ui_click,
    .stop = fake_audio_stop,
    .is_alarm_active = fake_audio_is_alarm_active,
    .is_test_active = fake_audio_is_test_active,
};

static const wifi_service_t s_wifi_service = {
    .init = fake_wifi_init,
    .snapshot = fake_wifi_snapshot,
    .start_scan = fake_wifi_scan,
    .connect = fake_wifi_connect,
    .forget = fake_wifi_forget,
    .request_sync = fake_wifi_sync,
    .set_auto_sync = fake_wifi_set_auto_sync,
};

static const display_service_t s_display_service = {
    .set_brightness = fake_display_set_brightness,
};

static const settings_store_t s_settings_store = {
    .load = fake_settings_load,
    .save = fake_settings_save,
};

static app_controller_core_t make_core(fake_env_t *env)
{
    app_controller_core_t core;
    app_controller_core_config_t config = {
        .clock_service = &s_clock_service,
        .audio_service = &s_audio_service,
        .wifi_service = &s_wifi_service,
        .display_service = &s_display_service,
        .settings_store = &s_settings_store,
        .monotonic_ms = fake_monotonic_ms,
        .monotonic_ctx = NULL,
        .ui_refresh = fake_ui_refresh,
        .ui_refresh_ctx = NULL,
    };

    g_env = env;
    app_controller_core_init(&core, &config);
    return core;
}

static int test_bootstrap_and_action_flow(void)
{
    fake_env_t env = {0};
    app_controller_core_t core;
    app_action_result_t result;

    settings_policy_set_defaults(&env.stored_settings);
    env.stored_settings.version = 5;
    snprintf(env.stored_settings.wifi.ssid, sizeof(env.stored_settings.wifi.ssid), "SavedWiFi");
    env.stored_settings.wifi.timezone_offset_hours = 2;
    env.stored_settings.wifi.timezone_id = timezone_id_from_legacy_offset(2);
    env.stored_settings.last_synced_epoch = make_utc_time(2026, 3, 12, 8, 0, 0);
    env.now = make_utc_time(2026, 3, 12, 8, 5, 0);
    env.monotonic_ms = 100;
    core = make_core(&env);

    EXPECT_EQ_INT(0, app_controller_core_bootstrap(&core, make_utc_time(2026, 3, 12, 7, 0, 0)));
    EXPECT_EQ_INT(1, env.load_calls);
    EXPECT_EQ_INT(1, env.audio_init_calls);
    EXPECT_EQ_INT(1, env.audio_set_ascending_calls);
    EXPECT_FALSE(env.audio_ascending_enabled);
    EXPECT_EQ_INT(1, env.wifi_init_calls);
    EXPECT_EQ_INT(timezone_id_from_legacy_offset(2), env.applied_timezone);
    EXPECT_EQ_INT(0, env.wifi_set_auto_sync_calls);
    EXPECT_EQ_INT(env.stored_settings.last_synced_epoch, env.set_epoch_value);

    result = app_action_set_base_brightness(&core.state, 77, env.now);
    app_controller_core_apply_action_result(&core, &result, env.now);
    EXPECT_TRUE(core.state.settings_dirty);
    EXPECT_EQ_INT(1100, core.state.save_deadline_ms);
    EXPECT_EQ_INT(1, env.display_set_calls);
    EXPECT_EQ_INT(0, env.audio_set_volume_calls);
    EXPECT_EQ_INT(1, env.ui_refresh_calls);
    EXPECT_EQ_INT(77, env.brightness_value);

    result = app_action_set_ascending_alarm_enabled(&core.state, true);
    app_controller_core_apply_action_result(&core, &result, env.now);
    EXPECT_EQ_INT(2, env.audio_set_ascending_calls);
    EXPECT_TRUE(env.audio_ascending_enabled);

    result = app_action_request_wifi_scan(&core.state);
    app_controller_core_apply_action_result(&core, &result, env.now);
    EXPECT_EQ_INT(1, env.wifi_scan_calls);
    return 0;
}

static int test_tick_and_save_debounce(void)
{
    fake_env_t env = {0};
    app_controller_core_t core;
    app_action_result_t result;

    settings_policy_set_defaults(&env.stored_settings);
    env.stored_settings.version = 5;
    env.stored_settings.alarm_volume = 55;
    env.now = make_utc_time(2026, 3, 12, 7, 0, 0);
    env.monotonic_ms = 0;
    env.wifi_snapshot.time_synced = true;
    env.wifi_snapshot.alarm_ringing = true;
    env.wifi_snapshot.wifi_scan_generation = 7;
    env.wifi_snapshot.wifi_scan_count = 2;
    snprintf(env.wifi_snapshot.wifi_scan_results[0].ssid,
             sizeof(env.wifi_snapshot.wifi_scan_results[0].ssid),
             "Office");
    env.wifi_snapshot.wifi_scan_results[0].rssi = -48;
    env.wifi_snapshot.wifi_scan_results[0].authmode = 3;
    snprintf(env.wifi_snapshot.wifi_scan_results[1].ssid,
             sizeof(env.wifi_snapshot.wifi_scan_results[1].ssid),
             "Guest");
    env.wifi_snapshot.wifi_scan_results[1].rssi = -67;
    env.wifi_snapshot.wifi_scan_results[1].authmode = 0;
    core = make_core(&env);

    EXPECT_EQ_INT(0, app_controller_core_bootstrap(&core, env.now));

    result = app_action_set_alarm_volume(&core.state, 66, env.now);
    app_controller_core_apply_action_result(&core, &result, env.now);
    EXPECT_EQ_INT(1, env.audio_set_volume_calls);
    EXPECT_EQ_INT(2, env.audio_set_ascending_calls);
    EXPECT_EQ_INT(0, env.save_calls);

    env.monotonic_ms = 1500;
    app_controller_core_tick(&core);
    EXPECT_EQ_INT(1, env.audio_start_alarm_calls);
    EXPECT_EQ_INT(0, env.save_calls);
    EXPECT_EQ_INT(env.now, core.state.settings.last_synced_epoch);
    EXPECT_EQ_INT(7, core.state.runtime.wifi_scan_generation);
    EXPECT_EQ_INT(2, (int)core.state.runtime.wifi_scan_count);
    EXPECT_STR_EQ("Office", core.state.runtime.wifi_scan_results[0].ssid);
    EXPECT_EQ_INT(-48, core.state.runtime.wifi_scan_results[0].rssi);

    env.monotonic_ms = 2600;
    app_controller_core_tick(&core);
    EXPECT_EQ_INT(1, env.save_calls);
    EXPECT_EQ_INT(env.now, env.stored_settings.last_synced_epoch);
    return 0;
}

static int test_alarm_test_and_cancel_window(void)
{
    fake_env_t env = {0};
    app_controller_core_t core;
    app_action_result_t result;
    time_t now = make_utc_time(2026, 3, 12, 7, 0, 0);

    settings_policy_set_defaults(&env.stored_settings);
    env.stored_settings.version = 5;
    env.now = now;
    core = make_core(&env);
    EXPECT_EQ_INT(0, app_controller_core_bootstrap(&core, now));

    app_controller_core_toggle_alarm_test(&core, now);
    EXPECT_EQ_INT(1, env.audio_start_test_calls);
    EXPECT_TRUE(core.state.runtime.alarm_test_active);

    app_controller_core_stop_alarm_test(&core);
    EXPECT_EQ_INT(1, env.audio_stop_calls);
    EXPECT_FALSE(core.state.runtime.alarm_test_active);

    app_controller_core_stop_alarm_test(&core);
    EXPECT_EQ_INT(1, env.audio_stop_calls);

    app_controller_core_toggle_alarm_test(&core, now);
    EXPECT_EQ_INT(2, env.audio_start_test_calls);
    EXPECT_TRUE(core.state.runtime.alarm_test_active);

    app_controller_core_toggle_alarm_test(&core, now);
    EXPECT_EQ_INT(2, env.audio_stop_calls);
    EXPECT_FALSE(core.state.runtime.alarm_test_active);

    core.state.settings.alarms[0].enabled = true;
    core.state.settings.alarms[0].hour = 8;
    core.state.settings.alarms[0].minute = 30;
    core.state.settings.alarms[0].repeat_mode = ALARM_REPEAT_WEEKLY;
    core.state.settings.alarms[0].days_mask = (uint8_t)(1U << 5);
    alarm_scheduler_tick(&core.state.runtime, &core.state.settings, now);
    result = app_action_cancel_next_alarm(&core.state, now);
    app_controller_core_arm_cancel_revert_window(&core, 2500);
    EXPECT_TRUE(app_action_has_effect(&result, APP_EFFECT_SETTINGS_CHANGED));
    EXPECT_TRUE(app_controller_core_cancel_revert_window_active(&core));

    env.monotonic_ms = 3000;
    EXPECT_FALSE(app_controller_core_cancel_revert_window_active(&core));
    return 0;
}

static int test_alarm_auto_stop_and_brightness_fade(void)
{
    fake_env_t env = {0};
    app_controller_core_t core;
    time_t now = make_utc_time(2026, 3, 12, 23, 0, 0);

    settings_policy_set_defaults(&env.stored_settings);
    env.stored_settings.version = 5;
    env.stored_settings.night_mode.enabled = true;
    env.stored_settings.night_mode.start_hour = 22;
    env.stored_settings.night_mode.start_minute = 0;
    env.stored_settings.night_mode.end_hour = 7;
    env.stored_settings.night_mode.end_minute = 0;
    env.stored_settings.night_mode.brightness = 10;
    env.now = now;
    env.monotonic_ms = 0;
    core = make_core(&env);
    EXPECT_EQ_INT(0, app_controller_core_bootstrap(&core, now));

    core.state.runtime.in_night_mode = true;
    core.state.runtime.alarm_ringing = true;
    core.state.runtime.effective_brightness = 100;
    core.state.applied_brightness = 100;
    env.alarm_active = true;
    env.wifi_snapshot = core.state.runtime;
    app_controller_core_tick(&core);
    EXPECT_TRUE(core.state.runtime.alarm_ringing);
    EXPECT_TRUE(core.state.alarm_auto_stop_armed);

    env.now = now + 601;
    env.monotonic_ms = 1000;
    env.wifi_snapshot = core.state.runtime;
    app_controller_core_tick(&core);
    EXPECT_FALSE(core.state.runtime.alarm_ringing);
    EXPECT_FALSE(core.state.alarm_auto_stop_armed);
    EXPECT_EQ_INT(1, env.audio_stop_calls);
    EXPECT_TRUE(core.state.brightness_fade_active);
    EXPECT_EQ_INT(10, core.state.runtime.effective_brightness);
    EXPECT_TRUE(env.brightness_value < 100);

    env.now = now + 605;
    env.monotonic_ms = 5000;
    env.wifi_snapshot = core.state.runtime;
    app_controller_core_tick(&core);
    EXPECT_FALSE(core.state.brightness_fade_active);
    EXPECT_EQ_INT(DISPLAY_BRIGHTNESS_MIN_PERCENT, env.brightness_value);
    return 0;
}

static int test_temporary_brightness_floor(void)
{
    fake_env_t env = {0};
    app_controller_core_t core;
    uint8_t badge_floor = brightness_policy_ui_to_hw(10);
    time_t now = make_utc_time(2026, 3, 12, 23, 0, 0);

    settings_policy_set_defaults(&env.stored_settings);
    env.stored_settings.version = 5;
    env.now = now;
    core = make_core(&env);
    EXPECT_EQ_INT(0, app_controller_core_bootstrap(&core, now));

    core.state.runtime.effective_brightness = 0;
    app_controller_core_set_temporary_brightness_floor(&core, true, badge_floor);
    EXPECT_EQ_INT(badge_floor, env.brightness_value);

    app_controller_core_set_temporary_brightness_floor(&core, false, badge_floor);
    EXPECT_EQ_INT(DISPLAY_BRIGHTNESS_MIN_PERCENT, env.brightness_value);
    return 0;
}

int main(void)
{
    int status;

    test_use_utc();

    status = test_bootstrap_and_action_flow();
    if (status != 0) {
        return status;
    }

    status = test_tick_and_save_debounce();
    if (status != 0) {
        return status;
    }

    status = test_alarm_test_and_cancel_window();
    if (status != 0) {
        return status;
    }

    status = test_alarm_auto_stop_and_brightness_fade();
    if (status != 0) {
        return status;
    }

    return test_temporary_brightness_floor();
}
