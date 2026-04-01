#include "domain/settings_policy.h"

#include <string.h>

#include "domain/face_catalog.h"

static clock_face_id_t default_night_face(void)
{
    return face_catalog_default_face();
}

void settings_policy_sanitize(app_settings_t *settings)
{
    if (settings->base_brightness > DISPLAY_BRIGHTNESS_MAX_PERCENT) {
        settings->base_brightness = 50;
    }

    if (settings->alarm_volume > 100) {
        settings->alarm_volume = 70;
    }
    settings->ui_click_sound_enabled = settings->ui_click_sound_enabled ? true : false;
    if (settings->ui_click_volume > 100) {
        settings->ui_click_volume = 70;
    }
    settings->ascending_alarm_enabled = settings->ascending_alarm_enabled ? true : false;

    if (settings->snooze_minutes < 1 || settings->snooze_minutes > 60) {
        settings->snooze_minutes = 10;
    }

    if (!face_catalog_is_valid(settings->current_face)) {
        settings->current_face = face_catalog_default_face();
    }
    if (!face_catalog_is_enabled(settings->current_face)) {
        settings->current_face = face_catalog_first_enabled();
    }
    for (size_t i = 0; i < CLOCK_FACE_COUNT; ++i) {
        if (settings->face_themes[i] >= CLOCK_FACE_THEME_COUNT) {
            settings->face_themes[i] = 0;
        }
    }

    if (!face_catalog_is_valid(settings->night_mode.face)) {
        settings->night_mode.face = default_night_face();
    }
    if (!face_catalog_is_enabled(settings->night_mode.face)) {
        settings->night_mode.face = face_catalog_first_enabled();
    }

    if (settings->night_mode.brightness > DISPLAY_BRIGHTNESS_MAX_PERCENT) {
        settings->night_mode.brightness = 0;
    }
    settings->night_mode.sunrise_brightness_enabled =
        settings->night_mode.sunrise_brightness_enabled ? true : false;
    if (settings->night_mode.start_hour > 23) {
        settings->night_mode.start_hour = 22;
    }
    if (settings->night_mode.start_minute > 59) {
        settings->night_mode.start_minute = 0;
    }
    if (settings->night_mode.end_hour > 23) {
        settings->night_mode.end_hour = 7;
    }
    if (settings->night_mode.end_minute > 59) {
        settings->night_mode.end_minute = 0;
    }

    if (settings->wifi.timezone_offset_hours < -12 || settings->wifi.timezone_offset_hours > 14) {
        settings->wifi.timezone_offset_hours = 0;
    }

    for (size_t i = 0; i < MAX_ALARMS; ++i) {
        if (settings->alarms[i].hour > 23) {
            settings->alarms[i].hour = 7;
        }
        if (settings->alarms[i].minute > 59) {
            settings->alarms[i].minute = 0;
        }
        if (settings->alarms[i].repeat_mode > ALARM_REPEAT_ONCE) {
            settings->alarms[i].repeat_mode = ALARM_REPEAT_WEEKLY;
        }
        if (settings->alarms[i].days_mask == 0) {
            settings->alarms[i].days_mask = 0x7F;
        }
        settings->alarms[i].math_unlock_enabled = settings->alarms[i].math_unlock_enabled ? true : false;
    }

    if (settings->skipped_alarm_index < -1 || settings->skipped_alarm_index >= MAX_ALARMS) {
        settings->skipped_alarm_index = -1;
        settings->skipped_alarm_epoch = 0;
    }
}

void settings_policy_set_defaults(app_settings_t *settings)
{
    memset(settings, 0, sizeof(*settings));
    settings->base_brightness = 50;
    settings->alarm_volume = 70;
    settings->ui_click_sound_enabled = true;
    settings->ui_click_volume = 70;
    settings->ascending_alarm_enabled = false;
    settings->snooze_minutes = 10;
    settings->current_face = face_catalog_default_face();
    settings->wifi.timezone_offset_hours = 0;
    settings->night_mode.enabled = false;
    settings->night_mode.sunrise_brightness_enabled = true;
    settings->night_mode.start_hour = 22;
    settings->night_mode.start_minute = 0;
    settings->night_mode.end_hour = 7;
    settings->night_mode.end_minute = 0;
    settings->night_mode.brightness = 0;
    settings->night_mode.face = default_night_face();
    settings->skipped_alarm_index = -1;

    for (size_t i = 0; i < MAX_ALARMS; ++i) {
        settings->alarms[i].enabled = false;
        settings->alarms[i].hour = 7;
        settings->alarms[i].minute = 0;
        settings->alarms[i].days_mask = 0x7F;
        settings->alarms[i].repeat_mode = ALARM_REPEAT_ONCE;
        settings->alarms[i].math_unlock_enabled = false;
    }
}
