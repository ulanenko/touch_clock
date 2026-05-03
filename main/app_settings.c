#include "app_settings.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/app_settings_storage.h"
#include "domain/settings_policy.h"
#include "domain/timezone_rules.h"
#include "nvs.h"

#define SETTINGS_NAMESPACE "clock"
#define SETTINGS_VERSION_V5 5U

static esp_err_t storage_get_u32(void *ctx, const char *key, uint32_t *value)
{
    return nvs_get_u32(*(nvs_handle_t *)ctx, key, value);
}

static esp_err_t storage_get_u8(void *ctx, const char *key, uint8_t *value)
{
    return nvs_get_u8(*(nvs_handle_t *)ctx, key, value);
}

static esp_err_t storage_get_i8(void *ctx, const char *key, int8_t *value)
{
    return nvs_get_i8(*(nvs_handle_t *)ctx, key, value);
}

static esp_err_t storage_get_i64(void *ctx, const char *key, int64_t *value)
{
    return nvs_get_i64(*(nvs_handle_t *)ctx, key, value);
}

static esp_err_t storage_get_str(void *ctx, const char *key, char *buffer, size_t *size)
{
    return nvs_get_str(*(nvs_handle_t *)ctx, key, buffer, size);
}

static esp_err_t storage_get_blob(void *ctx, const char *key, void *buffer, size_t *size)
{
    return nvs_get_blob(*(nvs_handle_t *)ctx, key, buffer, size);
}

static esp_err_t storage_set_u32(void *ctx, const char *key, uint32_t value)
{
    return nvs_set_u32(*(nvs_handle_t *)ctx, key, value);
}

static esp_err_t storage_set_u8(void *ctx, const char *key, uint8_t value)
{
    return nvs_set_u8(*(nvs_handle_t *)ctx, key, value);
}

static esp_err_t storage_set_i8(void *ctx, const char *key, int8_t value)
{
    return nvs_set_i8(*(nvs_handle_t *)ctx, key, value);
}

static esp_err_t storage_set_i64(void *ctx, const char *key, int64_t value)
{
    return nvs_set_i64(*(nvs_handle_t *)ctx, key, value);
}

static esp_err_t storage_set_str(void *ctx, const char *key, const char *value)
{
    return nvs_set_str(*(nvs_handle_t *)ctx, key, value);
}

static esp_err_t storage_set_blob(void *ctx, const char *key, const void *value, size_t size)
{
    return nvs_set_blob(*(nvs_handle_t *)ctx, key, value, size);
}

static esp_err_t storage_erase_key(void *ctx, const char *key)
{
    return nvs_erase_key(*(nvs_handle_t *)ctx, key);
}

static esp_err_t storage_commit(void *ctx)
{
    return nvs_commit(*(nvs_handle_t *)ctx);
}

static const app_settings_storage_t s_nvs_storage = {
    .ctx = NULL,
    .get_u32 = storage_get_u32,
    .get_u8 = storage_get_u8,
    .get_i8 = storage_get_i8,
    .get_i64 = storage_get_i64,
    .get_str = storage_get_str,
    .get_blob = storage_get_blob,
    .set_u32 = storage_set_u32,
    .set_u8 = storage_set_u8,
    .set_i8 = storage_set_i8,
    .set_i64 = storage_set_i64,
    .set_str = storage_set_str,
    .set_blob = storage_set_blob,
    .erase_key = storage_erase_key,
    .commit = storage_commit,
};

void app_settings_set_defaults(app_settings_t *settings)
{
    settings_policy_set_defaults(settings);
    settings->version = SETTINGS_VERSION_V5;
}

esp_err_t app_settings_load(app_settings_t *settings)
{
    nvs_handle_t handle;
    esp_err_t err;

    app_settings_set_defaults(settings);

    err = nvs_open(SETTINGS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }

    {
        app_settings_storage_t storage = s_nvs_storage;

        storage.ctx = &handle;
        err = app_settings_load_from_storage(&storage, settings);
    }

    nvs_close(handle);
    return err;
}

esp_err_t app_settings_save(const app_settings_t *settings)
{
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    {
        app_settings_storage_t storage = s_nvs_storage;

        storage.ctx = &handle;
        err = app_settings_save_to_storage(&storage, settings);
    }

    nvs_close(handle);
    return err;
}

void app_settings_apply_timezone(const app_settings_t *settings)
{
    char tz_buf[48];

    timezone_format_posix(tz_buf, sizeof(tz_buf), settings->wifi.timezone_id);
    setenv("TZ", tz_buf, 1);
    tzset();
}
