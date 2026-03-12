#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#include "domain/app_settings_types.h"

typedef struct {
    void *ctx;
    esp_err_t (*get_u32)(void *ctx, const char *key, uint32_t *value);
    esp_err_t (*get_u8)(void *ctx, const char *key, uint8_t *value);
    esp_err_t (*get_i8)(void *ctx, const char *key, int8_t *value);
    esp_err_t (*get_i64)(void *ctx, const char *key, int64_t *value);
    esp_err_t (*get_str)(void *ctx, const char *key, char *buffer, size_t *size);
    esp_err_t (*get_blob)(void *ctx, const char *key, void *buffer, size_t *size);
    esp_err_t (*set_u32)(void *ctx, const char *key, uint32_t value);
    esp_err_t (*set_u8)(void *ctx, const char *key, uint8_t value);
    esp_err_t (*set_i8)(void *ctx, const char *key, int8_t value);
    esp_err_t (*set_i64)(void *ctx, const char *key, int64_t value);
    esp_err_t (*set_str)(void *ctx, const char *key, const char *value);
    esp_err_t (*set_blob)(void *ctx, const char *key, const void *value, size_t size);
    esp_err_t (*erase_key)(void *ctx, const char *key);
    esp_err_t (*commit)(void *ctx);
} app_settings_storage_t;

esp_err_t app_settings_load_from_storage(const app_settings_storage_t *storage, app_settings_t *settings);
esp_err_t app_settings_save_to_storage(const app_settings_storage_t *storage, const app_settings_t *settings);
