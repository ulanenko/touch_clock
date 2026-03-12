#include "app/app_settings_storage.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "domain/settings_policy.h"

#if defined(ESP_ERR_NVS_NOT_FOUND)
#define SETTINGS_STORAGE_ERR_NOT_FOUND ESP_ERR_NVS_NOT_FOUND
#elif defined(ESP_ERR_NOT_FOUND)
#define SETTINGS_STORAGE_ERR_NOT_FOUND ESP_ERR_NOT_FOUND
#else
#error "Storage backend must define a not-found error code"
#endif

#define SETTINGS_VERSION_V4 4U
#define SETTINGS_VERSION_V5 5U
#define SETTINGS_KEY_LEGACY_BLOB "settings"
#define SETTINGS_KEY_VERSION "ver"
#define SETTINGS_KEY_BASE_BRIGHTNESS "base_bri"
#define SETTINGS_KEY_ALARM_VOLUME "alarm_vol"
#define SETTINGS_KEY_SNOOZE "snooze"
#define SETTINGS_KEY_CURRENT_FACE "face"
#define SETTINGS_KEY_WIFI_SSID "wifi_ssid"
#define SETTINGS_KEY_WIFI_PASSWORD "wifi_pass"
#define SETTINGS_KEY_TIMEZONE "tz"
#define SETTINGS_KEY_NIGHT_ENABLED "n_en"
#define SETTINGS_KEY_NIGHT_START_HOUR "n_sh"
#define SETTINGS_KEY_NIGHT_START_MINUTE "n_sm"
#define SETTINGS_KEY_NIGHT_END_HOUR "n_eh"
#define SETTINGS_KEY_NIGHT_END_MINUTE "n_em"
#define SETTINGS_KEY_NIGHT_BRIGHTNESS "n_bri"
#define SETTINGS_KEY_NIGHT_FACE "n_face"
#define SETTINGS_KEY_ALARMS "alarms"
#define SETTINGS_KEY_LAST_SYNCED "last_sync"
#define SETTINGS_KEY_SKIPPED_EPOCH "skip_epoch"
#define SETTINGS_KEY_SKIPPED_INDEX "skip_idx"

static void set_defaults(app_settings_t *settings)
{
    settings_policy_set_defaults(settings);
    settings->version = SETTINGS_VERSION_V5;
}

static void load_u8_if_present(const app_settings_storage_t *storage, const char *key, uint8_t *value)
{
    uint8_t loaded = 0;

    if (storage->get_u8 != NULL && storage->get_u8(storage->ctx, key, &loaded) == ESP_OK) {
        *value = loaded;
    }
}

static void load_i8_if_present(const app_settings_storage_t *storage, const char *key, int8_t *value)
{
    int8_t loaded = 0;

    if (storage->get_i8 != NULL && storage->get_i8(storage->ctx, key, &loaded) == ESP_OK) {
        *value = loaded;
    }
}

static void load_i64_if_present(const app_settings_storage_t *storage, const char *key, time_t *value)
{
    int64_t loaded = 0;

    if (storage->get_i64 != NULL && storage->get_i64(storage->ctx, key, &loaded) == ESP_OK) {
        *value = (time_t)loaded;
    }
}

static void load_string_if_present(const app_settings_storage_t *storage,
                                   const char *key,
                                   char *buffer,
                                   size_t size)
{
    size_t required_size = size;

    if (storage->get_str == NULL ||
        storage->get_str(storage->ctx, key, buffer, &required_size) != ESP_OK) {
        buffer[0] = '\0';
    }
}

static esp_err_t load_v5_settings(const app_settings_storage_t *storage, app_settings_t *settings)
{
    size_t alarms_size = sizeof(settings->alarms);
    app_settings_t defaults;

    set_defaults(settings);
    defaults = *settings;
    settings->version = SETTINGS_VERSION_V5;
    load_u8_if_present(storage, SETTINGS_KEY_BASE_BRIGHTNESS, &settings->base_brightness);
    load_u8_if_present(storage, SETTINGS_KEY_ALARM_VOLUME, &settings->alarm_volume);
    load_u8_if_present(storage, SETTINGS_KEY_SNOOZE, &settings->snooze_minutes);
    load_u8_if_present(storage, SETTINGS_KEY_CURRENT_FACE, (uint8_t *)&settings->current_face);
    load_string_if_present(storage, SETTINGS_KEY_WIFI_SSID, settings->wifi.ssid, sizeof(settings->wifi.ssid));
    load_string_if_present(storage, SETTINGS_KEY_WIFI_PASSWORD, settings->wifi.password, sizeof(settings->wifi.password));
    load_i8_if_present(storage, SETTINGS_KEY_TIMEZONE, &settings->wifi.timezone_offset_hours);
    load_u8_if_present(storage, SETTINGS_KEY_NIGHT_ENABLED, (uint8_t *)&settings->night_mode.enabled);
    load_u8_if_present(storage, SETTINGS_KEY_NIGHT_START_HOUR, &settings->night_mode.start_hour);
    load_u8_if_present(storage, SETTINGS_KEY_NIGHT_START_MINUTE, &settings->night_mode.start_minute);
    load_u8_if_present(storage, SETTINGS_KEY_NIGHT_END_HOUR, &settings->night_mode.end_hour);
    load_u8_if_present(storage, SETTINGS_KEY_NIGHT_END_MINUTE, &settings->night_mode.end_minute);
    load_u8_if_present(storage, SETTINGS_KEY_NIGHT_BRIGHTNESS, &settings->night_mode.brightness);
    load_u8_if_present(storage, SETTINGS_KEY_NIGHT_FACE, (uint8_t *)&settings->night_mode.face);
    if (storage->get_blob == NULL ||
        storage->get_blob(storage->ctx, SETTINGS_KEY_ALARMS, settings->alarms, &alarms_size) != ESP_OK ||
        alarms_size != sizeof(settings->alarms)) {
        memcpy(settings->alarms, defaults.alarms, sizeof(settings->alarms));
    }
    load_i64_if_present(storage, SETTINGS_KEY_LAST_SYNCED, &settings->last_synced_epoch);
    load_i64_if_present(storage, SETTINGS_KEY_SKIPPED_EPOCH, &settings->skipped_alarm_epoch);
    load_i8_if_present(storage, SETTINGS_KEY_SKIPPED_INDEX, &settings->skipped_alarm_index);
    settings_policy_sanitize(settings);
    settings->version = SETTINGS_VERSION_V5;
    return ESP_OK;
}

static esp_err_t load_legacy_v4_settings(const app_settings_storage_t *storage, app_settings_t *settings)
{
    app_settings_t loaded;
    size_t size = sizeof(loaded);
    esp_err_t err;

    if (storage->get_blob == NULL) {
        return ESP_OK;
    }

    err = storage->get_blob(storage->ctx, SETTINGS_KEY_LEGACY_BLOB, &loaded, &size);
    if (err == SETTINGS_STORAGE_ERR_NOT_FOUND) {
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }
    if (size != sizeof(loaded)) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (loaded.version != SETTINGS_VERSION_V4) {
        return ESP_ERR_INVALID_VERSION;
    }

    *settings = loaded;
    settings_policy_sanitize(settings);
    settings->version = SETTINGS_VERSION_V5;
    return ESP_OK;
}

esp_err_t app_settings_load_from_storage(const app_settings_storage_t *storage, app_settings_t *settings)
{
    uint32_t version = 0;
    esp_err_t err;

    set_defaults(settings);
    if (storage == NULL || storage->get_u32 == NULL) {
        return ESP_OK;
    }

    err = storage->get_u32(storage->ctx, SETTINGS_KEY_VERSION, &version);
    if (err == ESP_OK) {
        if (version != SETTINGS_VERSION_V5) {
            return ESP_ERR_INVALID_VERSION;
        }
        return load_v5_settings(storage, settings);
    }
    if (err == SETTINGS_STORAGE_ERR_NOT_FOUND) {
        return load_legacy_v4_settings(storage, settings);
    }

    return err;
}

esp_err_t app_settings_save_to_storage(const app_settings_storage_t *storage, const app_settings_t *settings)
{
    app_settings_t copy = *settings;
    esp_err_t err;

    if (storage == NULL) {
        return ESP_OK;
    }

    settings_policy_sanitize(&copy);
    copy.version = SETTINGS_VERSION_V5;

    err = storage->set_u32(storage->ctx, SETTINGS_KEY_VERSION, SETTINGS_VERSION_V5);
    if (err == ESP_OK) {
        err = storage->set_u8(storage->ctx, SETTINGS_KEY_BASE_BRIGHTNESS, copy.base_brightness);
    }
    if (err == ESP_OK) {
        err = storage->set_u8(storage->ctx, SETTINGS_KEY_ALARM_VOLUME, copy.alarm_volume);
    }
    if (err == ESP_OK) {
        err = storage->set_u8(storage->ctx, SETTINGS_KEY_SNOOZE, copy.snooze_minutes);
    }
    if (err == ESP_OK) {
        err = storage->set_u8(storage->ctx, SETTINGS_KEY_CURRENT_FACE, (uint8_t)copy.current_face);
    }
    if (err == ESP_OK) {
        err = storage->set_str(storage->ctx, SETTINGS_KEY_WIFI_SSID, copy.wifi.ssid);
    }
    if (err == ESP_OK) {
        err = storage->set_str(storage->ctx, SETTINGS_KEY_WIFI_PASSWORD, copy.wifi.password);
    }
    if (err == ESP_OK) {
        err = storage->set_i8(storage->ctx, SETTINGS_KEY_TIMEZONE, copy.wifi.timezone_offset_hours);
    }
    if (err == ESP_OK) {
        err = storage->set_u8(storage->ctx, SETTINGS_KEY_NIGHT_ENABLED, (uint8_t)copy.night_mode.enabled);
    }
    if (err == ESP_OK) {
        err = storage->set_u8(storage->ctx, SETTINGS_KEY_NIGHT_START_HOUR, copy.night_mode.start_hour);
    }
    if (err == ESP_OK) {
        err = storage->set_u8(storage->ctx, SETTINGS_KEY_NIGHT_START_MINUTE, copy.night_mode.start_minute);
    }
    if (err == ESP_OK) {
        err = storage->set_u8(storage->ctx, SETTINGS_KEY_NIGHT_END_HOUR, copy.night_mode.end_hour);
    }
    if (err == ESP_OK) {
        err = storage->set_u8(storage->ctx, SETTINGS_KEY_NIGHT_END_MINUTE, copy.night_mode.end_minute);
    }
    if (err == ESP_OK) {
        err = storage->set_u8(storage->ctx, SETTINGS_KEY_NIGHT_BRIGHTNESS, copy.night_mode.brightness);
    }
    if (err == ESP_OK) {
        err = storage->set_u8(storage->ctx, SETTINGS_KEY_NIGHT_FACE, (uint8_t)copy.night_mode.face);
    }
    if (err == ESP_OK) {
        err = storage->set_blob(storage->ctx, SETTINGS_KEY_ALARMS, copy.alarms, sizeof(copy.alarms));
    }
    if (err == ESP_OK) {
        err = storage->set_i64(storage->ctx, SETTINGS_KEY_LAST_SYNCED, (int64_t)copy.last_synced_epoch);
    }
    if (err == ESP_OK) {
        err = storage->set_i64(storage->ctx, SETTINGS_KEY_SKIPPED_EPOCH, (int64_t)copy.skipped_alarm_epoch);
    }
    if (err == ESP_OK) {
        err = storage->set_i8(storage->ctx, SETTINGS_KEY_SKIPPED_INDEX, copy.skipped_alarm_index);
    }
    if (err == ESP_OK || err == SETTINGS_STORAGE_ERR_NOT_FOUND) {
        err = storage->erase_key(storage->ctx, SETTINGS_KEY_LEGACY_BLOB);
        if (err == SETTINGS_STORAGE_ERR_NOT_FOUND) {
            err = ESP_OK;
        }
    }
    if (err == ESP_OK) {
        err = storage->commit(storage->ctx);
    }

    return err;
}
