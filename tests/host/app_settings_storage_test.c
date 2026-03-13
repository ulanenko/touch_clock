#include <string.h>

#include "app/app_settings_storage.h"
#include "domain/settings_policy.h"
#include "test_support.h"

typedef enum {
    STORE_TYPE_U32 = 1,
    STORE_TYPE_U8,
    STORE_TYPE_I8,
    STORE_TYPE_I64,
    STORE_TYPE_STR,
    STORE_TYPE_BLOB,
} store_type_t;

typedef struct {
    bool used;
    char key[32];
    store_type_t type;
    size_t size;
    union {
        uint32_t u32;
        uint8_t u8;
        int8_t i8;
        int64_t i64;
        char str[128];
        uint8_t blob[512];
    } value;
} store_entry_t;

typedef struct {
    store_entry_t entries[32];
    int commit_calls;
} fake_store_t;

static store_entry_t *find_entry(fake_store_t *store, const char *key)
{
    for (size_t i = 0; i < (sizeof(store->entries) / sizeof(store->entries[0])); ++i) {
        if (store->entries[i].used && strcmp(store->entries[i].key, key) == 0) {
            return &store->entries[i];
        }
    }

    return NULL;
}

static store_entry_t *upsert_entry(fake_store_t *store, const char *key, store_type_t type)
{
    store_entry_t *entry = find_entry(store, key);

    if (entry != NULL) {
        entry->type = type;
        return entry;
    }

    for (size_t i = 0; i < (sizeof(store->entries) / sizeof(store->entries[0])); ++i) {
        if (!store->entries[i].used) {
            store->entries[i].used = true;
            snprintf(store->entries[i].key, sizeof(store->entries[i].key), "%s", key);
            store->entries[i].type = type;
            return &store->entries[i];
        }
    }

    return NULL;
}

static esp_err_t store_get_u32(void *ctx, const char *key, uint32_t *value)
{
    fake_store_t *store = (fake_store_t *)ctx;
    store_entry_t *entry = find_entry(store, key);

    if (entry == NULL || entry->type != STORE_TYPE_U32) {
        return ESP_ERR_NVS_NOT_FOUND;
    }

    *value = entry->value.u32;
    return ESP_OK;
}

static esp_err_t store_get_u8(void *ctx, const char *key, uint8_t *value)
{
    fake_store_t *store = (fake_store_t *)ctx;
    store_entry_t *entry = find_entry(store, key);

    if (entry == NULL || entry->type != STORE_TYPE_U8) {
        return ESP_ERR_NVS_NOT_FOUND;
    }

    *value = entry->value.u8;
    return ESP_OK;
}

static esp_err_t store_get_i8(void *ctx, const char *key, int8_t *value)
{
    fake_store_t *store = (fake_store_t *)ctx;
    store_entry_t *entry = find_entry(store, key);

    if (entry == NULL || entry->type != STORE_TYPE_I8) {
        return ESP_ERR_NVS_NOT_FOUND;
    }

    *value = entry->value.i8;
    return ESP_OK;
}

static esp_err_t store_get_i64(void *ctx, const char *key, int64_t *value)
{
    fake_store_t *store = (fake_store_t *)ctx;
    store_entry_t *entry = find_entry(store, key);

    if (entry == NULL || entry->type != STORE_TYPE_I64) {
        return ESP_ERR_NVS_NOT_FOUND;
    }

    *value = entry->value.i64;
    return ESP_OK;
}

static esp_err_t store_get_str(void *ctx, const char *key, char *buffer, size_t *size)
{
    fake_store_t *store = (fake_store_t *)ctx;
    store_entry_t *entry = find_entry(store, key);
    size_t required_size;

    if (entry == NULL || entry->type != STORE_TYPE_STR) {
        return ESP_ERR_NVS_NOT_FOUND;
    }

    required_size = strlen(entry->value.str) + 1;
    if (*size < required_size) {
        return ESP_ERR_INVALID_SIZE;
    }
    memcpy(buffer, entry->value.str, required_size);
    *size = required_size;
    return ESP_OK;
}

static esp_err_t store_get_blob(void *ctx, const char *key, void *buffer, size_t *size)
{
    fake_store_t *store = (fake_store_t *)ctx;
    store_entry_t *entry = find_entry(store, key);

    if (entry == NULL || entry->type != STORE_TYPE_BLOB) {
        return ESP_ERR_NVS_NOT_FOUND;
    }
    if (*size < entry->size) {
        return ESP_ERR_INVALID_SIZE;
    }
    memcpy(buffer, entry->value.blob, entry->size);
    *size = entry->size;
    return ESP_OK;
}

static esp_err_t store_set_u32(void *ctx, const char *key, uint32_t value)
{
    store_entry_t *entry = upsert_entry((fake_store_t *)ctx, key, STORE_TYPE_U32);

    entry->value.u32 = value;
    entry->size = sizeof(value);
    return ESP_OK;
}

static esp_err_t store_set_u8(void *ctx, const char *key, uint8_t value)
{
    store_entry_t *entry = upsert_entry((fake_store_t *)ctx, key, STORE_TYPE_U8);

    entry->value.u8 = value;
    entry->size = sizeof(value);
    return ESP_OK;
}

static esp_err_t store_set_i8(void *ctx, const char *key, int8_t value)
{
    store_entry_t *entry = upsert_entry((fake_store_t *)ctx, key, STORE_TYPE_I8);

    entry->value.i8 = value;
    entry->size = sizeof(value);
    return ESP_OK;
}

static esp_err_t store_set_i64(void *ctx, const char *key, int64_t value)
{
    store_entry_t *entry = upsert_entry((fake_store_t *)ctx, key, STORE_TYPE_I64);

    entry->value.i64 = value;
    entry->size = sizeof(value);
    return ESP_OK;
}

static esp_err_t store_set_str(void *ctx, const char *key, const char *value)
{
    store_entry_t *entry = upsert_entry((fake_store_t *)ctx, key, STORE_TYPE_STR);

    snprintf(entry->value.str, sizeof(entry->value.str), "%s", value);
    entry->size = strlen(entry->value.str) + 1;
    return ESP_OK;
}

static esp_err_t store_set_blob(void *ctx, const char *key, const void *value, size_t size)
{
    store_entry_t *entry = upsert_entry((fake_store_t *)ctx, key, STORE_TYPE_BLOB);

    memcpy(entry->value.blob, value, size);
    entry->size = size;
    return ESP_OK;
}

static esp_err_t store_erase_key(void *ctx, const char *key)
{
    store_entry_t *entry = find_entry((fake_store_t *)ctx, key);

    if (entry == NULL) {
        return ESP_ERR_NVS_NOT_FOUND;
    }

    memset(entry, 0, sizeof(*entry));
    return ESP_OK;
}

static esp_err_t store_commit(void *ctx)
{
    ((fake_store_t *)ctx)->commit_calls += 1;
    return ESP_OK;
}

static app_settings_storage_t make_storage(fake_store_t *store)
{
    app_settings_storage_t storage = {
        .ctx = store,
        .get_u32 = store_get_u32,
        .get_u8 = store_get_u8,
        .get_i8 = store_get_i8,
        .get_i64 = store_get_i64,
        .get_str = store_get_str,
        .get_blob = store_get_blob,
        .set_u32 = store_set_u32,
        .set_u8 = store_set_u8,
        .set_i8 = store_set_i8,
        .set_i64 = store_set_i64,
        .set_str = store_set_str,
        .set_blob = store_set_blob,
        .erase_key = store_erase_key,
        .commit = store_commit,
    };

    return storage;
}

static int test_fresh_defaults(void)
{
    fake_store_t store = {0};
    app_settings_storage_t storage = make_storage(&store);
    app_settings_t settings;

    EXPECT_EQ_INT(ESP_OK, app_settings_load_from_storage(&storage, &settings));
    EXPECT_EQ_INT(50, settings.base_brightness);
    EXPECT_EQ_INT(70, settings.alarm_volume);
    EXPECT_FALSE(settings.ascending_alarm_enabled);
    EXPECT_EQ_INT(-1, settings.skipped_alarm_index);
    return 0;
}

static int test_v5_round_trip(void)
{
    fake_store_t store = {0};
    app_settings_storage_t storage = make_storage(&store);
    app_settings_t saved;
    app_settings_t loaded;

    settings_policy_set_defaults(&saved);
    saved.version = 5;
    saved.base_brightness = 77;
    saved.alarm_volume = 61;
    saved.ascending_alarm_enabled = true;
    saved.snooze_minutes = 12;
    saved.current_face = CLOCK_FACE_MATRIX;
    snprintf(saved.wifi.ssid, sizeof(saved.wifi.ssid), "Office");
    snprintf(saved.wifi.password, sizeof(saved.wifi.password), "secret");
    saved.wifi.timezone_offset_hours = 3;
    saved.night_mode.enabled = true;
    saved.night_mode.start_hour = 21;
    saved.night_mode.end_hour = 6;
    saved.night_mode.brightness = 25;
    saved.night_mode.face = CLOCK_FACE_MODERN_SILVER;
    saved.last_synced_epoch = make_utc_time(2026, 3, 12, 7, 0, 0);

    EXPECT_EQ_INT(ESP_OK, app_settings_save_to_storage(&storage, &saved));
    EXPECT_EQ_INT(1, store.commit_calls);
    EXPECT_TRUE(find_entry(&store, "ver") != NULL);
    EXPECT_TRUE(find_entry(&store, "base_bri") != NULL);
    EXPECT_TRUE(find_entry(&store, "alarm_vol") != NULL);
    EXPECT_TRUE(find_entry(&store, "alarm_ramp") != NULL);
    EXPECT_TRUE(find_entry(&store, "snooze") != NULL);
    EXPECT_TRUE(find_entry(&store, "face") != NULL);
    EXPECT_TRUE(find_entry(&store, "wifi_ssid") != NULL);
    EXPECT_TRUE(find_entry(&store, "wifi_pass") != NULL);
    EXPECT_TRUE(find_entry(&store, "tz") != NULL);
    EXPECT_TRUE(find_entry(&store, "n_en") != NULL);
    EXPECT_TRUE(find_entry(&store, "n_face") != NULL);
    EXPECT_TRUE(find_entry(&store, "alarms") != NULL);
    EXPECT_TRUE(find_entry(&store, "last_sync") != NULL);
    EXPECT_EQ_INT(ESP_OK, app_settings_load_from_storage(&storage, &loaded));
    EXPECT_EQ_INT(77, loaded.base_brightness);
    EXPECT_EQ_INT(61, loaded.alarm_volume);
    EXPECT_TRUE(loaded.ascending_alarm_enabled);
    EXPECT_EQ_INT(CLOCK_FACE_MATRIX, loaded.current_face);
    EXPECT_STR_EQ("Office", loaded.wifi.ssid);
    EXPECT_EQ_INT(3, loaded.wifi.timezone_offset_hours);
    EXPECT_TRUE(loaded.night_mode.enabled);
    EXPECT_EQ_INT(CLOCK_FACE_MODERN_SILVER, loaded.night_mode.face);
    return 0;
}

static int test_legacy_migration_and_sanitize(void)
{
    fake_store_t store = {0};
    app_settings_storage_t storage = make_storage(&store);
    app_settings_t legacy;
    app_settings_t loaded;

    memset(&legacy, 0, sizeof(legacy));
    legacy.version = 4;
    legacy.base_brightness = 0;
    legacy.current_face = CLOCK_FACE_SLAVA_DARK;
    legacy.night_mode.face = CLOCK_FACE_SLAVA;
    legacy.wifi.timezone_offset_hours = 99;

    EXPECT_EQ_INT(ESP_OK, store_set_blob(&store, "settings", &legacy, sizeof(legacy)));
    EXPECT_EQ_INT(ESP_OK, app_settings_load_from_storage(&storage, &loaded));
    EXPECT_EQ_INT(0, loaded.base_brightness);
    EXPECT_EQ_INT(CLOCK_FACE_DIGITAL, loaded.current_face);
    EXPECT_EQ_INT(CLOCK_FACE_DIGITAL, loaded.night_mode.face);
    EXPECT_EQ_INT(0, loaded.wifi.timezone_offset_hours);
    EXPECT_EQ_INT(5, loaded.version);
    return 0;
}

static int test_partial_v5_and_invalid_values(void)
{
    fake_store_t store = {0};
    app_settings_storage_t storage = make_storage(&store);
    app_settings_t loaded;

    EXPECT_EQ_INT(ESP_OK, store_set_u32(&store, "ver", 5));
    EXPECT_EQ_INT(ESP_OK, store_set_u8(&store, "base_bri", 0));
    EXPECT_EQ_INT(ESP_OK, store_set_u8(&store, "alarm_vol", 255));
    EXPECT_EQ_INT(ESP_OK, store_set_u8(&store, "alarm_ramp", 1));
    EXPECT_EQ_INT(ESP_OK, store_set_i8(&store, "tz", 42));
    EXPECT_EQ_INT(ESP_OK, store_set_u8(&store, "n_face", CLOCK_FACE_SLAVA_DARK));

    EXPECT_EQ_INT(ESP_OK, app_settings_load_from_storage(&storage, &loaded));
    EXPECT_EQ_INT(0, loaded.base_brightness);
    EXPECT_EQ_INT(70, loaded.alarm_volume);
    EXPECT_TRUE(loaded.ascending_alarm_enabled);
    EXPECT_EQ_INT(0, loaded.wifi.timezone_offset_hours);
    EXPECT_EQ_INT(CLOCK_FACE_DIGITAL, loaded.night_mode.face);
    return 0;
}

int main(void)
{
    int status;

    test_use_utc();

    status = test_fresh_defaults();
    if (status != 0) {
        return status;
    }

    status = test_v5_round_trip();
    if (status != 0) {
        return status;
    }

    status = test_legacy_migration_and_sanitize();
    if (status != 0) {
        return status;
    }

    return test_partial_v5_and_invalid_values();
}
