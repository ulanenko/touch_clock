#include "app/app_settings_storage.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "domain/settings_policy.h"
#include "domain/timezone_rules.h"

#if defined(ESP_ERR_NVS_NOT_FOUND)
#define SETTINGS_STORAGE_ERR_NOT_FOUND ESP_ERR_NVS_NOT_FOUND
#elif defined(ESP_ERR_NOT_FOUND)
#define SETTINGS_STORAGE_ERR_NOT_FOUND ESP_ERR_NOT_FOUND
#else
#error "Storage backend must define a not-found error code"
#endif

#define SETTINGS_VERSION_V4 4U
#define SETTINGS_VERSION_V5 5U
#define SETTINGS_VERSION_V6 6U
#define SETTINGS_VERSION_V7 7U
#define SETTINGS_VERSION_V8 8U
#define SETTINGS_VERSION_V9 9U
#define SETTINGS_VERSION_V10 10U
#define SETTINGS_KEY_LEGACY_BLOB "settings"
#define SETTINGS_KEY_VERSION "ver"
#define SETTINGS_KEY_BASE_BRIGHTNESS "base_bri"
#define SETTINGS_KEY_ALARM_VOLUME "alarm_vol"
#define SETTINGS_KEY_UI_CLICK_SOUND "ui_click"
#define SETTINGS_KEY_UI_CLICK_VOLUME "ui_clk_vol"
#define SETTINGS_KEY_ASCENDING_ALARM "alarm_ramp"
#define SETTINGS_KEY_SNOOZE "snooze"
#define SETTINGS_KEY_CURRENT_FACE "face"
#define SETTINGS_KEY_FACE_THEMES "face_themes"
#define SETTINGS_KEY_WIFI_SSID "wifi_ssid"
#define SETTINGS_KEY_WIFI_PASSWORD "wifi_pass"
#define SETTINGS_KEY_TIMEZONE "tz"
#define SETTINGS_KEY_TIMEZONE_ID "tz_id"
#define SETTINGS_KEY_TIME_SYNC_MODE "time_mode"
#define SETTINGS_KEY_NIGHT_ENABLED "n_en"
#define SETTINGS_KEY_NIGHT_START_HOUR "n_sh"
#define SETTINGS_KEY_NIGHT_START_MINUTE "n_sm"
#define SETTINGS_KEY_NIGHT_END_HOUR "n_eh"
#define SETTINGS_KEY_NIGHT_END_MINUTE "n_em"
#define SETTINGS_KEY_NIGHT_BRIGHTNESS "n_bri"
#define SETTINGS_KEY_NIGHT_FACE "n_face"
#define SETTINGS_KEY_NIGHT_SUNRISE "n_sun"
#define SETTINGS_KEY_ALARMS "alarms"
#define SETTINGS_KEY_LAST_SYNCED "last_sync"
#define SETTINGS_KEY_SKIPPED_EPOCH "skip_epoch"
#define SETTINGS_KEY_SKIPPED_INDEX "skip_idx"

typedef enum {
    SETTINGS_FIELD_U8,
    SETTINGS_FIELD_I8,
    SETTINGS_FIELD_I64,
    SETTINGS_FIELD_STR,
} settings_field_kind_t;

typedef struct {
    const char *key;
    settings_field_kind_t kind;
    size_t offset;
    size_t size;
} settings_field_descriptor_t;

typedef struct {
    bool enabled;
    uint8_t hour;
    uint8_t minute;
    uint8_t days_mask;
    uint8_t repeat_mode;
} legacy_alarm_config_t;

static const settings_field_descriptor_t s_settings_fields[] = {
    {
        .key = SETTINGS_KEY_BASE_BRIGHTNESS,
        .kind = SETTINGS_FIELD_U8,
        .offset = offsetof(app_settings_t, base_brightness),
    },
    {
        .key = SETTINGS_KEY_ALARM_VOLUME,
        .kind = SETTINGS_FIELD_U8,
        .offset = offsetof(app_settings_t, alarm_volume),
    },
    {
        .key = SETTINGS_KEY_UI_CLICK_SOUND,
        .kind = SETTINGS_FIELD_U8,
        .offset = offsetof(app_settings_t, ui_click_sound_enabled),
    },
    {
        .key = SETTINGS_KEY_UI_CLICK_VOLUME,
        .kind = SETTINGS_FIELD_U8,
        .offset = offsetof(app_settings_t, ui_click_volume),
    },
    {
        .key = SETTINGS_KEY_ASCENDING_ALARM,
        .kind = SETTINGS_FIELD_U8,
        .offset = offsetof(app_settings_t, ascending_alarm_enabled),
    },
    {
        .key = SETTINGS_KEY_SNOOZE,
        .kind = SETTINGS_FIELD_U8,
        .offset = offsetof(app_settings_t, snooze_minutes),
    },
    {
        .key = SETTINGS_KEY_CURRENT_FACE,
        .kind = SETTINGS_FIELD_U8,
        .offset = offsetof(app_settings_t, current_face),
    },
    {
        .key = SETTINGS_KEY_WIFI_SSID,
        .kind = SETTINGS_FIELD_STR,
        .offset = offsetof(app_settings_t, wifi.ssid),
        .size = sizeof(((app_settings_t *)0)->wifi.ssid),
    },
    {
        .key = SETTINGS_KEY_WIFI_PASSWORD,
        .kind = SETTINGS_FIELD_STR,
        .offset = offsetof(app_settings_t, wifi.password),
        .size = sizeof(((app_settings_t *)0)->wifi.password),
    },
    {
        .key = SETTINGS_KEY_TIMEZONE,
        .kind = SETTINGS_FIELD_I8,
        .offset = offsetof(app_settings_t, wifi.timezone_offset_hours),
    },
    {
        .key = SETTINGS_KEY_TIMEZONE_ID,
        .kind = SETTINGS_FIELD_U8,
        .offset = offsetof(app_settings_t, wifi.timezone_id),
    },
    {
        .key = SETTINGS_KEY_TIME_SYNC_MODE,
        .kind = SETTINGS_FIELD_U8,
        .offset = offsetof(app_settings_t, wifi.time_sync_mode),
    },
    {
        .key = SETTINGS_KEY_NIGHT_ENABLED,
        .kind = SETTINGS_FIELD_U8,
        .offset = offsetof(app_settings_t, night_mode.enabled),
    },
    {
        .key = SETTINGS_KEY_NIGHT_START_HOUR,
        .kind = SETTINGS_FIELD_U8,
        .offset = offsetof(app_settings_t, night_mode.start_hour),
    },
    {
        .key = SETTINGS_KEY_NIGHT_START_MINUTE,
        .kind = SETTINGS_FIELD_U8,
        .offset = offsetof(app_settings_t, night_mode.start_minute),
    },
    {
        .key = SETTINGS_KEY_NIGHT_END_HOUR,
        .kind = SETTINGS_FIELD_U8,
        .offset = offsetof(app_settings_t, night_mode.end_hour),
    },
    {
        .key = SETTINGS_KEY_NIGHT_END_MINUTE,
        .kind = SETTINGS_FIELD_U8,
        .offset = offsetof(app_settings_t, night_mode.end_minute),
    },
    {
        .key = SETTINGS_KEY_NIGHT_BRIGHTNESS,
        .kind = SETTINGS_FIELD_U8,
        .offset = offsetof(app_settings_t, night_mode.brightness),
    },
    {
        .key = SETTINGS_KEY_NIGHT_FACE,
        .kind = SETTINGS_FIELD_U8,
        .offset = offsetof(app_settings_t, night_mode.face),
    },
    {
        .key = SETTINGS_KEY_NIGHT_SUNRISE,
        .kind = SETTINGS_FIELD_U8,
        .offset = offsetof(app_settings_t, night_mode.sunrise_brightness_enabled),
    },
    {
        .key = SETTINGS_KEY_LAST_SYNCED,
        .kind = SETTINGS_FIELD_I64,
        .offset = offsetof(app_settings_t, last_synced_epoch),
    },
    {
        .key = SETTINGS_KEY_SKIPPED_EPOCH,
        .kind = SETTINGS_FIELD_I64,
        .offset = offsetof(app_settings_t, skipped_alarm_epoch),
    },
    {
        .key = SETTINGS_KEY_SKIPPED_INDEX,
        .kind = SETTINGS_FIELD_I8,
        .offset = offsetof(app_settings_t, skipped_alarm_index),
    },
};

static void set_defaults(app_settings_t *settings)
{
    settings_policy_set_defaults(settings);
    settings->version = SETTINGS_VERSION_V10;
}

static void *field_ptr(void *base, size_t offset)
{
    return ((uint8_t *)base) + offset;
}

static const void *field_const_ptr(const void *base, size_t offset)
{
    return ((const uint8_t *)base) + offset;
}

static void load_field_if_present(const app_settings_storage_t *storage,
                                  app_settings_t *settings,
                                  const settings_field_descriptor_t *field)
{
    switch (field->kind) {
    case SETTINGS_FIELD_U8: {
        uint8_t loaded = 0;

        if (storage->get_u8 != NULL && storage->get_u8(storage->ctx, field->key, &loaded) == ESP_OK) {
            *(uint8_t *)field_ptr(settings, field->offset) = loaded;
        }
        break;
    }
    case SETTINGS_FIELD_I8: {
        int8_t loaded = 0;

        if (storage->get_i8 != NULL && storage->get_i8(storage->ctx, field->key, &loaded) == ESP_OK) {
            *(int8_t *)field_ptr(settings, field->offset) = loaded;
        }
        break;
    }
    case SETTINGS_FIELD_I64: {
        int64_t loaded = 0;

        if (storage->get_i64 != NULL && storage->get_i64(storage->ctx, field->key, &loaded) == ESP_OK) {
            *(time_t *)field_ptr(settings, field->offset) = (time_t)loaded;
        }
        break;
    }
    case SETTINGS_FIELD_STR: {
        size_t required_size = field->size;
        char *buffer = (char *)field_ptr(settings, field->offset);

        if (storage->get_str == NULL ||
            storage->get_str(storage->ctx, field->key, buffer, &required_size) != ESP_OK) {
            buffer[0] = '\0';
        }
        break;
    }
    }
}

static esp_err_t save_field(const app_settings_storage_t *storage,
                            const app_settings_t *settings,
                            const settings_field_descriptor_t *field)
{
    switch (field->kind) {
    case SETTINGS_FIELD_U8:
        return storage->set_u8(storage->ctx,
                               field->key,
                               *(const uint8_t *)field_const_ptr(settings, field->offset));
    case SETTINGS_FIELD_I8:
        return storage->set_i8(storage->ctx,
                               field->key,
                               *(const int8_t *)field_const_ptr(settings, field->offset));
    case SETTINGS_FIELD_I64:
        return storage->set_i64(storage->ctx,
                                field->key,
                                (int64_t)*(const time_t *)field_const_ptr(settings, field->offset));
    case SETTINGS_FIELD_STR:
        return storage->set_str(storage->ctx,
                                field->key,
                                (const char *)field_const_ptr(settings, field->offset));
    }

    return ESP_FAIL;
}

static esp_err_t load_structured_settings(const app_settings_storage_t *storage,
                                          app_settings_t *settings,
                                          uint32_t version)
{
    size_t alarms_size = sizeof(settings->alarms);
    app_settings_t defaults;
    uint8_t alarm_buffer[(sizeof(alarm_config_t) * MAX_ALARMS) >
                                 (sizeof(legacy_alarm_config_t) * MAX_ALARMS)
                             ? (sizeof(alarm_config_t) * MAX_ALARMS)
                             : (sizeof(legacy_alarm_config_t) * MAX_ALARMS)];

    set_defaults(settings);
    defaults = *settings;

    for (size_t i = 0; i < sizeof(s_settings_fields) / sizeof(s_settings_fields[0]); ++i) {
        load_field_if_present(storage, settings, &s_settings_fields[i]);
    }
    if (version < SETTINGS_VERSION_V10) {
        settings->wifi.timezone_id =
            timezone_id_from_legacy_offset(settings->wifi.timezone_offset_hours);
        settings->wifi.time_sync_mode = TIME_SYNC_MODE_AUTO;
    }

    if (storage->get_blob == NULL ||
        storage->get_blob(storage->ctx, SETTINGS_KEY_ALARMS, alarm_buffer, &alarms_size) != ESP_OK) {
        memcpy(settings->alarms, defaults.alarms, sizeof(settings->alarms));
    } else if (alarms_size == sizeof(settings->alarms)) {
        memcpy(settings->alarms, alarm_buffer, sizeof(settings->alarms));
    } else if (alarms_size == sizeof(legacy_alarm_config_t) * MAX_ALARMS) {
        const legacy_alarm_config_t *legacy_alarms = (const legacy_alarm_config_t *)alarm_buffer;

        memcpy(settings->alarms, defaults.alarms, sizeof(settings->alarms));
        for (size_t i = 0; i < MAX_ALARMS; ++i) {
            settings->alarms[i].enabled = legacy_alarms[i].enabled;
            settings->alarms[i].hour = legacy_alarms[i].hour;
            settings->alarms[i].minute = legacy_alarms[i].minute;
            settings->alarms[i].days_mask = legacy_alarms[i].days_mask;
            settings->alarms[i].repeat_mode = legacy_alarms[i].repeat_mode;
            settings->alarms[i].math_unlock_enabled = false;
        }
    } else {
        memcpy(settings->alarms, defaults.alarms, sizeof(settings->alarms));
    }
    {
        size_t face_themes_size = sizeof(settings->face_themes);

        if (storage->get_blob == NULL ||
            storage->get_blob(storage->ctx,
                              SETTINGS_KEY_FACE_THEMES,
                              settings->face_themes,
                              &face_themes_size) != ESP_OK ||
            face_themes_size != sizeof(settings->face_themes)) {
            memcpy(settings->face_themes, defaults.face_themes, sizeof(settings->face_themes));
        }
    }

    settings_policy_sanitize(settings);
    settings->version = SETTINGS_VERSION_V10;
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
    settings->night_mode.sunrise_brightness_enabled = true;
    settings->ui_click_sound_enabled = true;
    settings->ui_click_volume = 70;
    settings->wifi.timezone_id = timezone_id_from_legacy_offset(settings->wifi.timezone_offset_hours);
    settings->wifi.time_sync_mode = TIME_SYNC_MODE_AUTO;
    settings_policy_sanitize(settings);
    settings->version = SETTINGS_VERSION_V10;
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
        /* Structured settings use stable per-key storage, so loading newer
         * versions is safer than resetting everything on downgrade. */
        if (version < SETTINGS_VERSION_V5) {
            return ESP_ERR_INVALID_VERSION;
        }
        return load_structured_settings(storage, settings, version);
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
    copy.version = SETTINGS_VERSION_V10;

    err = storage->set_u32(storage->ctx, SETTINGS_KEY_VERSION, SETTINGS_VERSION_V10);
    for (size_t i = 0;
         err == ESP_OK && i < sizeof(s_settings_fields) / sizeof(s_settings_fields[0]);
         ++i) {
        err = save_field(storage, &copy, &s_settings_fields[i]);
    }
    if (err == ESP_OK) {
        err = storage->set_blob(storage->ctx, SETTINGS_KEY_ALARMS, copy.alarms, sizeof(copy.alarms));
    }
    if (err == ESP_OK) {
        err = storage->set_blob(storage->ctx,
                                SETTINGS_KEY_FACE_THEMES,
                                copy.face_themes,
                                sizeof(copy.face_themes));
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
