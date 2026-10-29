#include "domain/brightness_policy.h"
#include "domain/settings_policy.h"
#include "test_support.h"

static int test_default_and_night_targets(void)
{
    app_settings_t settings;
    app_runtime_state_t runtime = {0};
    time_t now = make_utc_time(2026, 3, 12, 6, 0, 0);

    settings_policy_set_defaults(&settings);
    settings.base_brightness = 80;
    settings.night_mode.enabled = true;
    settings.night_mode.brightness = 25;

    EXPECT_EQ_INT(80, brightness_policy_get_target(&runtime, &settings, now));

    runtime.in_night_mode = true;
    EXPECT_EQ_INT(25, brightness_policy_get_target(&runtime, &settings, now));

    runtime.night_brightness_override_active = true;
    runtime.night_brightness_override = 33;
    EXPECT_EQ_INT(33, brightness_policy_get_target(&runtime, &settings, now));
    return 0;
}

static int test_sunrise_and_alarm_override(void)
{
    app_settings_t settings;
    app_runtime_state_t runtime = {0};
    time_t now = make_utc_time(2026, 3, 12, 6, 0, 0);
    int peak;

    settings_policy_set_defaults(&settings);
    settings.base_brightness = 80;
    settings.night_mode.enabled = true;
    settings.night_mode.brightness = 25;
    settings.night_mode.sunrise_minutes = 20;
    settings.night_mode.sunrise_peak = 60;
    peak = brightness_policy_ui_to_hw(60);

    /* A sunrise that began in the night: from the night's level, slowly at first. */
    runtime.sunrise_active = true;
    runtime.sunrise_from_night = true;
    runtime.in_night_mode = true;
    runtime.next_alarm_epoch = now + 1200;
    EXPECT_EQ_INT(25, brightness_policy_get_target(&runtime, &settings, now));
    runtime.next_alarm_epoch = now + 600; /* halfway: an eighth of the way up */
    EXPECT_EQ_INT((250 + (peak * 10 - 250) / 8 + 5) / 10, brightness_policy_get_target(&runtime, &settings, now));
    EXPECT_EQ_INT(250 + (peak * 10 - 250) / 8, brightness_policy_get_target_permille(&runtime, &settings, now));
    runtime.in_night_mode = false; /* handed over to day: it rises on all the same */
    runtime.next_alarm_epoch = now + 1;
    EXPECT_TRUE(brightness_policy_get_target(&runtime, &settings, now) >= peak - 1);

    /* One that began in the day leaves the brightness alone. */
    runtime.sunrise_from_night = false;
    runtime.next_alarm_epoch = now + 600;
    EXPECT_EQ_INT(80, brightness_policy_get_target(&runtime, &settings, now));

    runtime.alarm_ringing = true;
    EXPECT_EQ_INT(DISPLAY_BRIGHTNESS_MAX_PERCENT,
                  brightness_policy_get_target(&runtime, &settings, now));
    return 0;
}

static int test_night_sunrise_ramp_toggle(void)
{
    app_settings_t settings;
    app_runtime_state_t runtime = {0};
    time_t now = make_utc_time(2026, 3, 12, 6, 0, 0);

    settings_policy_set_defaults(&settings);
    settings.base_brightness = 80;
    settings.night_mode.enabled = true;
    settings.night_mode.brightness = 25;
    settings.night_mode.sunrise_peak = 40;

    /* Without a sunrise (the scheduler starts none when it is off), the night's level. */
    runtime.in_night_mode = true;
    runtime.next_alarm_epoch = now + 600;
    EXPECT_EQ_INT(25, brightness_policy_get_target(&runtime, &settings, now));

    /* A peak below the night's level never dims it. */
    settings.night_mode.brightness = 90;
    runtime.sunrise_active = true;
    runtime.sunrise_from_night = true;
    EXPECT_EQ_INT(90, brightness_policy_get_target(&runtime, &settings, now));
    return 0;
}

static int test_slider_mapping(void)
{
    int round_trip;

    EXPECT_EQ_INT(DISPLAY_BRIGHTNESS_MIN_PERCENT, brightness_policy_ui_to_hw(0));
    EXPECT_EQ_INT(DISPLAY_BRIGHTNESS_MAX_PERCENT, brightness_policy_ui_to_hw(100));
    EXPECT_EQ_INT(0, brightness_policy_hw_to_ui(DISPLAY_BRIGHTNESS_MIN_PERCENT));
    EXPECT_EQ_INT(100, brightness_policy_hw_to_ui(DISPLAY_BRIGHTNESS_MAX_PERCENT));
    round_trip = brightness_policy_hw_to_ui(brightness_policy_ui_to_hw(50));
    EXPECT_TRUE(round_trip >= 49 && round_trip <= 51);
    return 0;
}

int main(void)
{
    int status;

    test_use_utc();

    status = test_default_and_night_targets();
    if (status != 0) {
        return status;
    }

    status = test_sunrise_and_alarm_override();
    if (status != 0) {
        return status;
    }

    status = test_night_sunrise_ramp_toggle();
    if (status != 0) {
        return status;
    }

    return test_slider_mapping();
}
