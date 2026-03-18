#include <string.h>

#include "domain/face_catalog.h"
#include "domain/settings_policy.h"
#include "test_support.h"

static int test_defaults(void)
{
    app_settings_t settings;

    settings_policy_set_defaults(&settings);

    EXPECT_EQ_INT(50, settings.base_brightness);
    EXPECT_EQ_INT(70, settings.alarm_volume);
    EXPECT_TRUE(settings.ui_click_sound_enabled);
    EXPECT_FALSE(settings.ascending_alarm_enabled);
    EXPECT_EQ_INT(10, settings.snooze_minutes);
    EXPECT_TRUE(face_catalog_is_enabled(settings.current_face));
    EXPECT_EQ_INT(0, settings.face_themes[CLOCK_FACE_DIGITAL]);
    EXPECT_TRUE(face_catalog_is_enabled(settings.night_mode.face));
    EXPECT_TRUE(settings.night_mode.sunrise_brightness_enabled);
    EXPECT_EQ_INT(-1, settings.skipped_alarm_index);
    EXPECT_EQ_INT(0, settings.wifi.timezone_offset_hours);
    EXPECT_EQ_INT(ALARM_REPEAT_ONCE, settings.alarms[0].repeat_mode);
    EXPECT_EQ_INT(0x7F, settings.alarms[0].days_mask);
    EXPECT_FALSE(settings.alarms[0].math_unlock_enabled);
    return 0;
}

static int test_sanitize_invalid_values(void)
{
    app_settings_t settings;

    memset(&settings, 0xFF, sizeof(settings));
    settings.base_brightness = 0;
    settings.alarm_volume = 255;
    settings.snooze_minutes = 0;
    settings.current_face = CLOCK_FACE_SLAVA_DARK;
    settings.face_themes[CLOCK_FACE_DIGITAL] = 99;
    settings.night_mode.face = CLOCK_FACE_SLAVA;
    settings.night_mode.brightness = 255;
    settings.night_mode.start_hour = 255;
    settings.night_mode.end_hour = 255;
    settings.wifi.timezone_offset_hours = 42;
    settings.alarms[0].hour = 99;
    settings.alarms[0].minute = 99;
    settings.alarms[0].repeat_mode = 99;
    settings.alarms[0].days_mask = 0;
    settings.alarms[0].math_unlock_enabled = true;
    settings.skipped_alarm_index = 99;
    settings.skipped_alarm_epoch = 12345;

    settings_policy_sanitize(&settings);

    EXPECT_EQ_INT(0, settings.base_brightness);
    EXPECT_EQ_INT(70, settings.alarm_volume);
    EXPECT_TRUE(settings.ui_click_sound_enabled);
    EXPECT_TRUE(settings.ascending_alarm_enabled);
    EXPECT_EQ_INT(10, settings.snooze_minutes);
    EXPECT_EQ_INT(face_catalog_first_enabled(), settings.current_face);
    EXPECT_EQ_INT(0, settings.face_themes[CLOCK_FACE_DIGITAL]);
    EXPECT_EQ_INT(face_catalog_first_enabled(), settings.night_mode.face);
    EXPECT_EQ_INT(0, settings.night_mode.brightness);
    EXPECT_TRUE(settings.night_mode.sunrise_brightness_enabled);
    EXPECT_EQ_INT(22, settings.night_mode.start_hour);
    EXPECT_EQ_INT(7, settings.night_mode.end_hour);
    EXPECT_EQ_INT(0, settings.wifi.timezone_offset_hours);
    EXPECT_EQ_INT(7, settings.alarms[0].hour);
    EXPECT_EQ_INT(0, settings.alarms[0].minute);
    EXPECT_EQ_INT(ALARM_REPEAT_WEEKLY, settings.alarms[0].repeat_mode);
    EXPECT_EQ_INT(0x7F, settings.alarms[0].days_mask);
    EXPECT_TRUE(settings.alarms[0].math_unlock_enabled);
    EXPECT_EQ_INT(-1, settings.skipped_alarm_index);
    EXPECT_EQ_INT(0, settings.skipped_alarm_epoch);
    return 0;
}

int main(void)
{
    int status;

    test_use_utc();

    status = test_defaults();
    if (status != 0) {
        return status;
    }

    return test_sanitize_invalid_values();
}
