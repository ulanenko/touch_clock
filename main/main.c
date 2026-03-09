#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include "bsp/esp-bsp.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"

#include "alarm_logic.h"
#include "app_settings.h"
#include "clock_ui.h"
#include "wifi_time.h"

static const char *TAG = "clock_app";

typedef struct {
    app_settings_t settings;
    app_runtime_state_t runtime;
    lv_timer_t *tick_timer;
    int64_t save_deadline_ms;
    uint8_t applied_brightness;
    bool settings_dirty;
} app_context_t;

static app_context_t s_app = {0};

static int64_t monotonic_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static void mark_settings_dirty(void)
{
    s_app.settings_dirty = true;
    s_app.save_deadline_ms = monotonic_ms() + 1000;
}

static void apply_runtime_brightness(void)
{
    if (s_app.applied_brightness == s_app.runtime.effective_brightness) {
        return;
    }

    esp_err_t err = bsp_display_brightness_set(s_app.runtime.effective_brightness);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set brightness to %u%%: %s",
                 s_app.runtime.effective_brightness, esp_err_to_name(err));
        return;
    }

    s_app.applied_brightness = s_app.runtime.effective_brightness;
}

static void maybe_save_settings(bool force)
{
    if (!s_app.settings_dirty) {
        return;
    }

    if (!force && monotonic_ms() < s_app.save_deadline_ms) {
        return;
    }

    esp_err_t err = app_settings_save(&s_app.settings);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to save settings: %s", esp_err_to_name(err));
        s_app.save_deadline_ms = monotonic_ms() + 2000;
        return;
    }

    s_app.settings_dirty = false;
}

static void on_settings_changed(void *user_ctx)
{
    app_context_t *app = (app_context_t *)user_ctx;

    app_settings_apply_timezone(&app->settings);
    mark_settings_dirty();
}

static void on_wifi_scan_requested(void *user_ctx)
{
    LV_UNUSED(user_ctx);
    ESP_ERROR_CHECK_WITHOUT_ABORT(wifi_time_start_scan());
}

static void on_wifi_connect_requested(void *user_ctx, const char *ssid, const char *password)
{
    LV_UNUSED(user_ctx);
    ESP_ERROR_CHECK_WITHOUT_ABORT(wifi_time_connect(ssid, password));
}

static void on_wifi_forget_requested(void *user_ctx)
{
    LV_UNUSED(user_ctx);
    ESP_ERROR_CHECK_WITHOUT_ABORT(wifi_time_forget());
}

static void on_wifi_sync_requested(void *user_ctx)
{
    LV_UNUSED(user_ctx);
    ESP_ERROR_CHECK_WITHOUT_ABORT(wifi_time_request_sync());
}

static void on_alarm_snooze_requested(void *user_ctx)
{
    app_context_t *app = (app_context_t *)user_ctx;
    time_t now;

    time(&now);
    alarm_logic_snooze(&app->runtime, &app->settings, now);
}

static void on_alarm_stop_requested(void *user_ctx)
{
    app_context_t *app = (app_context_t *)user_ctx;

    alarm_logic_stop(&app->runtime);
}

static void clock_tick_cb(lv_timer_t *timer)
{
    time_t now;

    LV_UNUSED(timer);

    time(&now);
    wifi_time_snapshot(&s_app.runtime);
    alarm_logic_tick(&s_app.runtime, &s_app.settings, now);

    if (s_app.runtime.time_synced &&
        now > 1700000000 &&
        (s_app.settings.last_synced_epoch == 0 || (now - s_app.settings.last_synced_epoch) >= 300)) {
        s_app.settings.last_synced_epoch = now;
        mark_settings_dirty();
    }

    apply_runtime_brightness();
    clock_ui_tick(now);
    maybe_save_settings(false);
}

void app_main(void)
{
    bsp_display_cfg_t cfg = {
        .lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG(),
        .rotation = ESP_LV_ADAPTER_ROTATE_0,
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL,
        .touch_flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        }
    };
    clock_ui_callbacks_t ui_callbacks = {
        .on_settings_changed = on_settings_changed,
        .on_wifi_scan_requested = on_wifi_scan_requested,
        .on_wifi_connect_requested = on_wifi_connect_requested,
        .on_wifi_forget_requested = on_wifi_forget_requested,
        .on_wifi_sync_requested = on_wifi_sync_requested,
        .on_alarm_snooze_requested = on_alarm_snooze_requested,
        .on_alarm_stop_requested = on_alarm_stop_requested,
    };
    struct timeval boot_time = {
        .tv_sec = 1741500000,
        .tv_usec = 0,
    };
    esp_err_t err;

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    app_settings_set_defaults(&s_app.settings);
    err = app_settings_load(&s_app.settings);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load saved settings: %s", esp_err_to_name(err));
        app_settings_set_defaults(&s_app.settings);
    }

    app_settings_apply_timezone(&s_app.settings);
    if (s_app.settings.last_synced_epoch > 1700000000) {
        boot_time.tv_sec = s_app.settings.last_synced_epoch;
    }
    settimeofday(&boot_time, NULL);

    alarm_logic_init(&s_app.runtime, &s_app.settings);
    s_app.applied_brightness = UINT8_MAX;

    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();

    ESP_ERROR_CHECK(wifi_time_init(&s_app.settings));

    ESP_ERROR_CHECK(bsp_display_lock(1000));
    ESP_ERROR_CHECK(clock_ui_init(&s_app.settings, &s_app.runtime, &ui_callbacks, &s_app));
    clock_ui_refresh();
    bsp_display_unlock();

    s_app.tick_timer = lv_timer_create(clock_tick_cb, 1000, NULL);

    ESP_ERROR_CHECK(bsp_display_lock(1000));
    clock_tick_cb(s_app.tick_timer);
    bsp_display_unlock();
}
