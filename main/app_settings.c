#include "app_settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nvs.h"

#define SETTINGS_NAMESPACE "clock"
#define SETTINGS_VERSION 4U

static void clamp_settings(app_settings_t *settings)
{
    if (settings->base_brightness < DISPLAY_BRIGHTNESS_MIN_PERCENT ||
        settings->base_brightness > DISPLAY_BRIGHTNESS_MAX_PERCENT) {
        settings->base_brightness = 50;
    }

    if (settings->alarm_volume > 100) {
        settings->alarm_volume = 70;
    }

    if (settings->snooze_minutes < 1 || settings->snooze_minutes > 60) {
        settings->snooze_minutes = 10;
    }

    if (!clock_face_is_valid(settings->current_face)) {
        settings->current_face = CLOCK_FACE_DIGITAL;
    }

    if (!clock_face_is_valid(settings->night_mode.face)) {
        settings->night_mode.face = CLOCK_FACE_DIGITAL;
    }

    if (settings->night_mode.brightness < DISPLAY_BRIGHTNESS_MIN_PERCENT ||
        settings->night_mode.brightness > DISPLAY_BRIGHTNESS_MAX_PERCENT) {
        settings->night_mode.brightness = DISPLAY_BRIGHTNESS_MIN_PERCENT;
    }
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
    }

    if (settings->skipped_alarm_index < -1 || settings->skipped_alarm_index >= MAX_ALARMS) {
        settings->skipped_alarm_index = -1;
        settings->skipped_alarm_epoch = 0;
    }
}

void app_settings_set_defaults(app_settings_t *settings)
{
    memset(settings, 0, sizeof(*settings));
    settings->version = SETTINGS_VERSION;
    settings->base_brightness = 50;
    settings->alarm_volume = 70;
    settings->snooze_minutes = 10;
    settings->current_face = CLOCK_FACE_DIGITAL;
    settings->wifi.timezone_offset_hours = 0;
    settings->night_mode.enabled = false;
    settings->night_mode.start_hour = 22;
    settings->night_mode.start_minute = 0;
    settings->night_mode.end_hour = 7;
    settings->night_mode.end_minute = 0;
    settings->night_mode.brightness = DISPLAY_BRIGHTNESS_MIN_PERCENT;
    settings->night_mode.face = CLOCK_FACE_SLAVA_DARK;
    settings->skipped_alarm_index = -1;

    for (size_t i = 0; i < MAX_ALARMS; ++i) {
        settings->alarms[i].enabled = false;
        settings->alarms[i].hour = 7;
        settings->alarms[i].minute = 0;
        settings->alarms[i].days_mask = 0x7F;
        settings->alarms[i].repeat_mode = ALARM_REPEAT_WEEKLY;
    }
}

esp_err_t app_settings_load(app_settings_t *settings)
{
    app_settings_t loaded;
    size_t size = sizeof(loaded);
    nvs_handle_t handle;

    app_settings_set_defaults(settings);

    esp_err_t err = nvs_open(SETTINGS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_get_blob(handle, "settings", &loaded, &size);
    nvs_close(handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }
    if (size != sizeof(loaded)) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (loaded.version != SETTINGS_VERSION) {
        return ESP_ERR_INVALID_VERSION;
    }

    *settings = loaded;
    clamp_settings(settings);
    return ESP_OK;
}

esp_err_t app_settings_save(const app_settings_t *settings)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_blob(handle, "settings", settings, sizeof(*settings));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
}

void app_settings_apply_timezone(const app_settings_t *settings)
{
    char tz_buf[16];
    int posix_offset = -settings->wifi.timezone_offset_hours;

    snprintf(tz_buf, sizeof(tz_buf), "UTC%+d", posix_offset);
    setenv("TZ", tz_buf, 1);
    tzset();
}

bool clock_face_is_valid(int face)
{
    return face >= 0 && face < CLOCK_FACE_COUNT;
}

const char *clock_face_name(clock_face_id_t face)
{
    switch (face) {
    case CLOCK_FACE_DIGITAL:
        return "Digital";
    case CLOCK_FACE_MATRIX:
        return "Matrix";
    case CLOCK_FACE_WHARTON:
        return "Wharton";
    case CLOCK_FACE_SLAVA:
        return "Slava";
    case CLOCK_FACE_SLAVA_DARK:
        return "Slava Dark";
    default:
        return "Unknown";
    }
}
