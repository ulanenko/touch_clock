#include "domain/alarm_scheduler.h"
#include "domain/settings_policy.h"
#include "test_support.h"

static void init_state(app_settings_t *settings, app_runtime_state_t *runtime)
{
    settings_policy_set_defaults(settings);
    alarm_scheduler_init(runtime, settings);
}

static int test_weekly_next_occurrence(void)
{
    app_settings_t settings;
    app_runtime_state_t runtime;
    time_t now = make_utc_time(2026, 3, 12, 7, 0, 0);
    time_t expected = make_utc_time(2026, 3, 13, 8, 30, 0);

    init_state(&settings, &runtime);
    settings.alarms[0].enabled = true;
    settings.alarms[0].hour = 8;
    settings.alarms[0].minute = 30;
    settings.alarms[0].repeat_mode = ALARM_REPEAT_WEEKLY;
    settings.alarms[0].days_mask = (uint8_t)(1U << 5);

    EXPECT_FALSE(alarm_scheduler_tick(&runtime, &settings, now));
    EXPECT_EQ_INT(0, runtime.next_alarm_index);
    EXPECT_EQ_INT(expected, runtime.next_alarm_epoch);
    return 0;
}

static int test_one_time_rollover(void)
{
    app_settings_t settings;
    app_runtime_state_t runtime;
    time_t now = make_utc_time(2026, 3, 12, 7, 0, 0);
    time_t expected = make_utc_time(2026, 3, 13, 6, 30, 0);

    init_state(&settings, &runtime);
    settings.alarms[0].enabled = true;
    settings.alarms[0].hour = 6;
    settings.alarms[0].minute = 30;
    settings.alarms[0].repeat_mode = ALARM_REPEAT_ONCE;

    EXPECT_FALSE(alarm_scheduler_tick(&runtime, &settings, now));
    EXPECT_EQ_INT(expected, runtime.next_alarm_epoch);
    return 0;
}

static int test_trigger_once_per_minute_and_snooze(void)
{
    app_settings_t settings;
    app_runtime_state_t runtime;
    time_t first_tick = make_utc_time(2026, 3, 12, 7, 0, 5);
    time_t second_tick = make_utc_time(2026, 3, 12, 7, 0, 40);
    time_t snooze_tick = make_utc_time(2026, 3, 12, 7, 10, 5);

    init_state(&settings, &runtime);
    settings.alarms[0].enabled = true;
    settings.alarms[0].hour = 7;
    settings.alarms[0].minute = 0;
    settings.alarms[0].repeat_mode = ALARM_REPEAT_WEEKLY;
    settings.alarms[0].days_mask = 0x7F;
    settings.snooze_minutes = 10;

    EXPECT_FALSE(alarm_scheduler_tick(&runtime, &settings, first_tick));
    EXPECT_TRUE(runtime.alarm_ringing);
    EXPECT_EQ_INT(0, runtime.active_alarm_index);

    alarm_scheduler_stop(&runtime);
    EXPECT_FALSE(alarm_scheduler_tick(&runtime, &settings, second_tick));
    EXPECT_FALSE(runtime.alarm_ringing);

    runtime.alarm_ringing = true;
    alarm_scheduler_snooze(&runtime, &settings, first_tick);
    EXPECT_TRUE(runtime.snooze_active);
    EXPECT_EQ_INT(first_tick + 600, runtime.snooze_deadline);

    EXPECT_FALSE(alarm_scheduler_tick(&runtime, &settings, snooze_tick));
    EXPECT_TRUE(runtime.alarm_ringing);
    EXPECT_FALSE(runtime.snooze_active);
    return 0;
}

static int test_cancel_next_and_skip_expiry(void)
{
    app_settings_t settings;
    app_runtime_state_t runtime;
    time_t now = make_utc_time(2026, 3, 12, 7, 0, 0);
    time_t skipped_epoch = make_utc_time(2026, 3, 13, 8, 30, 0);

    init_state(&settings, &runtime);
    settings.alarms[0].enabled = true;
    settings.alarms[0].hour = 8;
    settings.alarms[0].minute = 30;
    settings.alarms[0].repeat_mode = ALARM_REPEAT_WEEKLY;
    settings.alarms[0].days_mask = (uint8_t)(1U << 5);

    EXPECT_FALSE(alarm_scheduler_tick(&runtime, &settings, now));
    EXPECT_TRUE(alarm_scheduler_cancel_next(&runtime, &settings, now));
    EXPECT_EQ_INT(0, settings.skipped_alarm_index);
    EXPECT_EQ_INT(skipped_epoch, settings.skipped_alarm_epoch);
    EXPECT_TRUE(runtime.next_alarm_epoch > skipped_epoch);

    EXPECT_TRUE(alarm_scheduler_tick(&runtime, &settings, skipped_epoch + 61));
    EXPECT_EQ_INT(-1, settings.skipped_alarm_index);
    EXPECT_EQ_INT(0, settings.skipped_alarm_epoch);
    return 0;
}

static int test_cancel_next_one_time_disables_alarm(void)
{
    app_settings_t settings;
    app_runtime_state_t runtime;
    time_t now = make_utc_time(2026, 3, 12, 7, 0, 0);

    init_state(&settings, &runtime);
    settings.alarms[0].enabled = true;
    settings.alarms[0].hour = 8;
    settings.alarms[0].minute = 0;
    settings.alarms[0].repeat_mode = ALARM_REPEAT_ONCE;

    EXPECT_FALSE(alarm_scheduler_tick(&runtime, &settings, now));
    EXPECT_TRUE(alarm_scheduler_cancel_next(&runtime, &settings, now));
    EXPECT_FALSE(settings.alarms[0].enabled);
    return 0;
}

int main(void)
{
    int status;

    test_use_utc();

    status = test_weekly_next_occurrence();
    if (status != 0) {
        return status;
    }

    status = test_one_time_rollover();
    if (status != 0) {
        return status;
    }

    status = test_trigger_once_per_minute_and_snooze();
    if (status != 0) {
        return status;
    }

    status = test_cancel_next_and_skip_expiry();
    if (status != 0) {
        return status;
    }

    return test_cancel_next_one_time_disables_alarm();
}
