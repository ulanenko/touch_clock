#include <string.h>

#include "app/app_settings_storage.h"
#include "domain/settings_policy.h"
#include "domain/timezone_rules.h"
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
    store_entry_t entries[48]; /* more than the settings keys */
    int commit_calls;
} fake_store_t;

typedef struct {
    bool enabled;
    uint8_t hour;
    uint8_t minute;
    uint8_t days_mask;
    uint8_t repeat_mode;
} legacy_alarm_config_t;

typedef struct {
    bool enabled;
    uint8_t hour;
    uint8_t minute;
    uint8_t days_mask;
    uint8_t repeat_mode;
    bool math_unlock_enabled;
} legacy_alarm_config_v2_t;

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
    EXPECT_TRUE(settings.ui_click_sound_enabled);
    EXPECT_EQ_INT(70, settings.ui_click_volume);
    EXPECT_FALSE(settings.ascending_alarm_enabled);
    EXPECT_EQ_INT(0, settings.face_themes[CLOCK_FACE_DIGITAL]);
    EXPECT_EQ_INT(-1, settings.skipped_alarm_index);
    EXPECT_FALSE(settings.alarms[0].math_unlock_enabled);
    return 0;
}

static int test_v5_round_trip(void)
{
    fake_store_t store = {0};
    app_settings_storage_t storage = make_storage(&store);
    app_settings_t saved;
    app_settings_t loaded;

    settings_policy_set_defaults(&saved);
    saved.version = 10;
    saved.base_brightness = 77;
    saved.alarm_volume = 61;
    saved.ui_click_sound_enabled = false;
    saved.ui_click_volume = 42;
    saved.ui_style = CLOCK_UI_STYLE_ATELIER;
    saved.ascending_alarm_enabled = true;
    saved.snooze_minutes = 12;
    saved.current_face = CLOCK_FACE_MATRIX;
    saved.face_themes[CLOCK_FACE_DIGITAL] = 1;
    saved.face_themes[CLOCK_FACE_STERNGLAS] = 1;
    snprintf(saved.wifi.ssid, sizeof(saved.wifi.ssid), "Office");
    snprintf(saved.wifi.password, sizeof(saved.wifi.password), "secret");
    saved.wifi.timezone_offset_hours = 3;
    saved.wifi.timezone_id = timezone_id_from_legacy_offset(3);
    saved.wifi.time_sync_mode = TIME_SYNC_MODE_MANUAL;
    saved.night_mode.enabled = true;
    saved.night_mode.start_hour = 21;
    saved.night_mode.end_hour = 6;
    saved.night_mode.brightness = 25;
    saved.night_mode.face = CLOCK_FACE_MODERN_SILVER;
    saved.night_mode.sunrise_brightness_enabled = false;
    saved.alarms[0].enabled = true;
    saved.alarms[0].hour = 6;
    saved.alarms[0].minute = 45;
    saved.alarms[0].math_unlock_enabled = true;
    saved.last_synced_epoch = make_utc_time(2026, 3, 12, 7, 0, 0);

    EXPECT_EQ_INT(ESP_OK, app_settings_save_to_storage(&storage, &saved));
    EXPECT_EQ_INT(1, store.commit_calls);
    EXPECT_TRUE(find_entry(&store, "ver") != NULL);
    EXPECT_TRUE(find_entry(&store, "base_bri") != NULL);
    EXPECT_TRUE(find_entry(&store, "alarm_vol") != NULL);
    EXPECT_TRUE(find_entry(&store, "ui_click") != NULL);
    EXPECT_TRUE(find_entry(&store, "ui_clk_vol") != NULL);
    EXPECT_TRUE(find_entry(&store, "ui_style") != NULL);
    EXPECT_TRUE(find_entry(&store, "alarm_ramp") != NULL);
    EXPECT_TRUE(find_entry(&store, "snooze") != NULL);
    EXPECT_TRUE(find_entry(&store, "face") != NULL);
    EXPECT_TRUE(find_entry(&store, "face_themes") != NULL);
    EXPECT_TRUE(find_entry(&store, "wifi_ssid") != NULL);
    EXPECT_TRUE(find_entry(&store, "wifi_pass") != NULL);
    EXPECT_TRUE(find_entry(&store, "tz") != NULL);
    EXPECT_TRUE(find_entry(&store, "tz_id") != NULL);
    EXPECT_TRUE(find_entry(&store, "time_mode") != NULL);
    EXPECT_TRUE(find_entry(&store, "n_en") != NULL);
    EXPECT_TRUE(find_entry(&store, "n_face") != NULL);
    EXPECT_TRUE(find_entry(&store, "n_sun") != NULL);
    EXPECT_TRUE(find_entry(&store, "alarms") != NULL);
    EXPECT_TRUE(find_entry(&store, "last_sync") != NULL);
    EXPECT_EQ_INT(ESP_OK, app_settings_load_from_storage(&storage, &loaded));
    EXPECT_EQ_INT(77, loaded.base_brightness);
    EXPECT_EQ_INT(61, loaded.alarm_volume);
    EXPECT_FALSE(loaded.ui_click_sound_enabled);
    EXPECT_EQ_INT(42, loaded.ui_click_volume);
    EXPECT_EQ_INT(CLOCK_UI_STYLE_ATELIER, loaded.ui_style);
    EXPECT_TRUE(loaded.ascending_alarm_enabled);
    EXPECT_EQ_INT(CLOCK_FACE_MATRIX, loaded.current_face);
    EXPECT_EQ_INT(1, loaded.face_themes[CLOCK_FACE_DIGITAL]);
    EXPECT_EQ_INT(1, loaded.face_themes[CLOCK_FACE_STERNGLAS]);
    EXPECT_STR_EQ("Office", loaded.wifi.ssid);
    EXPECT_EQ_INT(3, loaded.wifi.timezone_offset_hours);
    EXPECT_EQ_INT(timezone_id_from_legacy_offset(3), loaded.wifi.timezone_id);
    EXPECT_EQ_INT(TIME_SYNC_MODE_MANUAL, loaded.wifi.time_sync_mode);
    EXPECT_TRUE(loaded.night_mode.enabled);
    EXPECT_EQ_INT(CLOCK_FACE_MODERN_SILVER, loaded.night_mode.face);
    EXPECT_FALSE(loaded.night_mode.sunrise_brightness_enabled);
    EXPECT_TRUE(loaded.alarms[0].math_unlock_enabled);
    return 0;
}

/* A clock that never saved (its first boot) starts from the defaults. */
static int test_nothing_saved_loads_defaults(void)
{
    fake_store_t store = {0};
    app_settings_storage_t storage = make_storage(&store);
    app_settings_t loaded;
    app_settings_t defaults;

    settings_policy_set_defaults(&defaults);
    EXPECT_EQ_INT(ESP_OK, app_settings_load_from_storage(&storage, &loaded));
    EXPECT_EQ_INT(defaults.base_brightness, loaded.base_brightness);
    EXPECT_EQ_INT(defaults.current_face, loaded.current_face);
    EXPECT_EQ_INT(defaults.alarm_volume, loaded.alarm_volume);
    EXPECT_EQ_INT(defaults.wifi.timezone_id, loaded.wifi.timezone_id);
    EXPECT_EQ_INT(11, loaded.version);
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
    EXPECT_TRUE(loaded.ui_click_sound_enabled);
    EXPECT_EQ_INT(70, loaded.ui_click_volume);
    EXPECT_TRUE(loaded.ascending_alarm_enabled);
    EXPECT_EQ_INT(0, loaded.face_themes[CLOCK_FACE_DIGITAL]);
    EXPECT_EQ_INT(0, loaded.wifi.timezone_offset_hours);
    EXPECT_EQ_INT(CLOCK_FACE_DIGITAL, loaded.night_mode.face);
    EXPECT_TRUE(loaded.night_mode.sunrise_brightness_enabled);
    EXPECT_FALSE(loaded.alarms[0].math_unlock_enabled);
    EXPECT_EQ_INT(11, loaded.version);
    return 0;
}

static int test_v7_alarm_blob_migration(void)
{
    fake_store_t store = {0};
    app_settings_storage_t storage = make_storage(&store);
    legacy_alarm_config_t legacy_alarms[MAX_ALARMS] = {0};
    app_settings_t loaded;

    legacy_alarms[0].enabled = true;
    legacy_alarms[0].hour = 7;
    legacy_alarms[0].minute = 30;
    legacy_alarms[0].days_mask = 0x7F;
    legacy_alarms[0].repeat_mode = ALARM_REPEAT_WEEKLY;

    EXPECT_EQ_INT(ESP_OK, store_set_u32(&store, "ver", 7));
    EXPECT_EQ_INT(ESP_OK, store_set_blob(&store, "alarms", legacy_alarms, sizeof(legacy_alarms)));
    EXPECT_EQ_INT(ESP_OK, app_settings_load_from_storage(&storage, &loaded));
    EXPECT_TRUE(loaded.alarms[0].enabled);
    EXPECT_EQ_INT(7, loaded.alarms[0].hour);
    EXPECT_EQ_INT(30, loaded.alarms[0].minute);
    EXPECT_FALSE(loaded.alarms[0].math_unlock_enabled);
    EXPECT_EQ_INT(11, loaded.version);
    return 0;
}

/* Alarms saved before they had a sound keep everything and get the original tone. */
static int test_alarm_blob_without_sound_migration(void)
{
    fake_store_t store = {0};
    app_settings_storage_t storage = make_storage(&store);
    legacy_alarm_config_v2_t v2[MAX_ALARMS] = {0};
    app_settings_t loaded;

    v2[1].enabled = true;
    v2[1].hour = 6;
    v2[1].minute = 45;
    v2[1].days_mask = 0x1F;
    v2[1].repeat_mode = ALARM_REPEAT_WEEKLY;
    v2[1].math_unlock_enabled = true;

    EXPECT_EQ_INT(ESP_OK, store_set_u32(&store, "ver", 11));
    EXPECT_EQ_INT(ESP_OK, store_set_blob(&store, "alarms", v2, sizeof(v2)));
    EXPECT_EQ_INT(ESP_OK, app_settings_load_from_storage(&storage, &loaded));
    EXPECT_TRUE(loaded.alarms[1].enabled);
    EXPECT_EQ_INT(6, loaded.alarms[1].hour);
    EXPECT_EQ_INT(45, loaded.alarms[1].minute);
    EXPECT_EQ_INT(0x1F, loaded.alarms[1].days_mask);
    EXPECT_TRUE(loaded.alarms[1].math_unlock_enabled);
    EXPECT_EQ_INT(ALARM_SOUND_MORNING_LIGHT, loaded.alarms[1].sound);
    return 0;
}

/* A chosen sound survives a save and load; an unknown one falls back to the original. */
static int test_alarm_sound_round_trip(void)
{
    fake_store_t store = {0};
    app_settings_storage_t storage = make_storage(&store);
    app_settings_t saved;
    app_settings_t loaded;

    EXPECT_EQ_INT(ESP_OK, app_settings_load_from_storage(&storage, &saved));
    saved.alarms[0].enabled = true;
    saved.alarms[0].sound = ALARM_SOUND_SINGING_BOWL;
    saved.alarms[2].sound = 200;
    EXPECT_EQ_INT(ESP_OK, app_settings_save_to_storage(&storage, &saved));
    EXPECT_EQ_INT(ESP_OK, app_settings_load_from_storage(&storage, &loaded));
    EXPECT_EQ_INT(ALARM_SOUND_SINGING_BOWL, loaded.alarms[0].sound);
    EXPECT_EQ_INT(ALARM_SOUND_MORNING_LIGHT, loaded.alarms[2].sound);
    return 0;
}

/* Face themes saved before the evening faces existed: a blob of 8 themes. */
static int test_face_themes_from_fewer_faces(void)
{
    fake_store_t store = {0};
    app_settings_storage_t storage = make_storage(&store);
    app_settings_t saved;
    app_settings_t loaded;
    const uint8_t old_themes[8] = {1, 2, 0, 0, 0, 2, 1, 2};

    EXPECT_EQ_INT(ESP_OK, app_settings_load_from_storage(&storage, &saved));
    EXPECT_EQ_INT(ESP_OK, app_settings_save_to_storage(&storage, &saved));
    EXPECT_EQ_INT(ESP_OK, store_set_blob(&store, "face_themes", old_themes, sizeof(old_themes)));
    EXPECT_EQ_INT(ESP_OK, app_settings_load_from_storage(&storage, &loaded));
    EXPECT_EQ_INT(1, loaded.face_themes[CLOCK_FACE_DIGITAL]);
    EXPECT_EQ_INT(2, loaded.face_themes[CLOCK_FACE_MATRIX]);
    EXPECT_EQ_INT(2, loaded.face_themes[CLOCK_FACE_STERNGLAS]);
    EXPECT_EQ_INT(2, loaded.face_themes[CLOCK_FACE_MODERN_SILVER]);
    EXPECT_EQ_INT(0, loaded.face_themes[CLOCK_FACE_EMBER]);
    EXPECT_EQ_INT(0, loaded.face_themes[CLOCK_FACE_FILAMENT]);

    loaded.face_themes[CLOCK_FACE_NOCTURNE] = 2;
    EXPECT_EQ_INT(ESP_OK, app_settings_save_to_storage(&storage, &loaded));
    EXPECT_EQ_INT(ESP_OK, app_settings_load_from_storage(&storage, &saved));
    EXPECT_EQ_INT(2, saved.face_themes[CLOCK_FACE_NOCTURNE]);
    EXPECT_EQ_INT(1, saved.face_themes[CLOCK_FACE_DIGITAL]);
    return 0;
}

static int test_future_structured_version_loads_without_reset(void)
{
    fake_store_t store = {0};
    app_settings_storage_t storage = make_storage(&store);
    app_settings_t loaded;

    EXPECT_EQ_INT(ESP_OK, store_set_u32(&store, "ver", 99));
    EXPECT_EQ_INT(ESP_OK, store_set_u8(&store, "base_bri", 64));
    EXPECT_EQ_INT(ESP_OK, store_set_u8(&store, "alarm_vol", 55));
    EXPECT_EQ_INT(ESP_OK, store_set_u8(&store, "ui_click", 1));
    EXPECT_EQ_INT(ESP_OK, store_set_u8(&store, "ui_clk_vol", 33));
    EXPECT_EQ_INT(ESP_OK, store_set_str(&store, "wifi_ssid", "Office"));

    EXPECT_EQ_INT(ESP_OK, app_settings_load_from_storage(&storage, &loaded));
    EXPECT_EQ_INT(64, loaded.base_brightness);
    EXPECT_EQ_INT(55, loaded.alarm_volume);
    EXPECT_TRUE(loaded.ui_click_sound_enabled);
    EXPECT_EQ_INT(33, loaded.ui_click_volume);
    EXPECT_STR_EQ("Office", loaded.wifi.ssid);
    EXPECT_EQ_INT(11, loaded.version);
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

    status = test_nothing_saved_loads_defaults();
    if (status != 0) {
        return status;
    }

    status = test_partial_v5_and_invalid_values();
    if (status != 0) {
        return status;
    }

    status = test_v7_alarm_blob_migration();
    if (status != 0) {
        return status;
    }

    status = test_alarm_blob_without_sound_migration();
    if (status != 0) {
        return status;
    }

    status = test_alarm_sound_round_trip();
    if (status != 0) {
        return status;
    }
    status = test_face_themes_from_fewer_faces();
    if (status != 0) {
        return status;
    }

    return test_future_structured_version_loads_without_reset();
}
