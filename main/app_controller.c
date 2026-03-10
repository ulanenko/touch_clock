#include "app_controller.h"

#include <stdbool.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>

#include "alarm_audio.h"
#include "alarm_logic.h"
#include "app_settings.h"
#include "clock_ui.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "wifi_time.h"

typedef struct {
    app_settings_t settings;
    app_runtime_state_t runtime;
    lv_timer_t *tick_timer;
    int64_t save_deadline_ms;
    int64_t cancel_revert_deadline_ms;
    uint8_t applied_brightness;
    bool audio_available;
    bool settings_dirty;
    bool cancel_revert_available;
    bool cancel_revert_was_one_time;
    bool cancel_revert_was_enabled;
    int8_t cancel_revert_alarm_index;
    time_t cancel_revert_alarm_epoch;
} app_controller_t;

static const char *TAG = "clock_app";
static app_controller_t s_app = {0};

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
    uint8_t target_brightness = s_app.settings.base_brightness;

    s_app.runtime.effective_brightness = target_brightness;

    if (s_app.applied_brightness == target_brightness) {
        return;
    }

    esp_err_t err = bsp_display_brightness_set(target_brightness);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set brightness to %u%%: %s",
                 target_brightness, esp_err_to_name(err));
        return;
    }

    s_app.applied_brightness = target_brightness;
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
    app_controller_t *app = (app_controller_t *)user_ctx;

    app_settings_apply_timezone(&app->settings);
    if (app->audio_available) {
        alarm_audio_set_volume(app->settings.alarm_volume);
    }
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
    app_controller_t *app = (app_controller_t *)user_ctx;
    time_t now;

    time(&now);
    alarm_logic_snooze(&app->runtime, &app->settings, now);
    if (app->audio_available) {
        alarm_audio_stop();
        app->runtime.alarm_test_active = alarm_audio_is_test_active();
    }
}

static void on_alarm_stop_requested(void *user_ctx)
{
    app_controller_t *app = (app_controller_t *)user_ctx;

    alarm_logic_stop(&app->runtime);
    if (app->audio_available) {
        alarm_audio_stop();
        app->runtime.alarm_test_active = alarm_audio_is_test_active();
    }
}

static void on_alarm_test_requested(void *user_ctx)
{
    app_controller_t *app = (app_controller_t *)user_ctx;

    if (!app->audio_available) {
        return;
    }

    if (alarm_audio_is_test_active()) {
        alarm_audio_stop();
    } else {
        ESP_ERROR_CHECK_WITHOUT_ABORT(alarm_audio_start_test(app->settings.alarm_volume));
    }

    app->runtime.alarm_test_active = alarm_audio_is_test_active();
}

static void on_next_alarm_cancel_requested(void *user_ctx)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    time_t now;
    int8_t alarm_index = app->runtime.next_alarm_index;
    time_t alarm_epoch = app->runtime.next_alarm_epoch;

    time(&now);
    if (app->runtime.snooze_active) {
        alarm_logic_stop(&app->runtime);
        app->cancel_revert_available = false;
        mark_settings_dirty();
    } else if (alarm_index >= 0 &&
               alarm_index < MAX_ALARMS &&
               alarm_epoch > now &&
               alarm_logic_cancel_next_alarm(&app->runtime, &app->settings, now)) {
        app->cancel_revert_available = true;
        app->cancel_revert_alarm_index = alarm_index;
        app->cancel_revert_alarm_epoch = alarm_epoch;
        app->cancel_revert_was_one_time = (app->settings.alarms[alarm_index].repeat_mode == ALARM_REPEAT_ONCE);
        app->cancel_revert_was_enabled = true;
        app->cancel_revert_deadline_ms = monotonic_ms() + 2500;
        mark_settings_dirty();
    }
}

static void on_next_alarm_cancel_undo_requested(void *user_ctx)
{
    app_controller_t *app = (app_controller_t *)user_ctx;

    if (!app->cancel_revert_available ||
        monotonic_ms() > app->cancel_revert_deadline_ms ||
        app->cancel_revert_alarm_index < 0 ||
        app->cancel_revert_alarm_index >= MAX_ALARMS) {
        return;
    }

    if (app->cancel_revert_was_one_time) {
        app->settings.alarms[app->cancel_revert_alarm_index].enabled = app->cancel_revert_was_enabled;
    } else if (app->settings.skipped_alarm_index == app->cancel_revert_alarm_index &&
               app->settings.skipped_alarm_epoch == app->cancel_revert_alarm_epoch) {
        app->settings.skipped_alarm_index = -1;
        app->settings.skipped_alarm_epoch = 0;
    }

    app->runtime.next_alarm_epoch = app->cancel_revert_alarm_epoch;
    app->runtime.next_alarm_index = app->cancel_revert_alarm_index;
    app->cancel_revert_available = false;
    mark_settings_dirty();
}

static void clock_tick_cb(lv_timer_t *timer)
{
    time_t now;

    LV_UNUSED(timer);

    time(&now);
    wifi_time_snapshot(&s_app.runtime);
    if (alarm_logic_tick(&s_app.runtime, &s_app.settings, now)) {
        mark_settings_dirty();
    }
    if (s_app.audio_available) {
        if (s_app.runtime.alarm_ringing) {
            if (!alarm_audio_is_alarm_active()) {
                ESP_ERROR_CHECK_WITHOUT_ABORT(alarm_audio_start_alarm(s_app.settings.alarm_volume));
            }
        } else if (alarm_audio_is_alarm_active()) {
            alarm_audio_stop();
        }
        s_app.runtime.alarm_test_active = alarm_audio_is_test_active();
    } else {
        s_app.runtime.alarm_test_active = false;
    }

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

esp_err_t app_controller_start(const bsp_display_cfg_t *display_cfg)
{
    clock_ui_callbacks_t ui_callbacks = {
        .on_settings_changed = on_settings_changed,
        .on_wifi_scan_requested = on_wifi_scan_requested,
        .on_wifi_connect_requested = on_wifi_connect_requested,
        .on_wifi_forget_requested = on_wifi_forget_requested,
        .on_wifi_sync_requested = on_wifi_sync_requested,
        .on_alarm_snooze_requested = on_alarm_snooze_requested,
        .on_alarm_stop_requested = on_alarm_stop_requested,
        .on_alarm_test_requested = on_alarm_test_requested,
        .on_next_alarm_cancel_requested = on_next_alarm_cancel_requested,
        .on_next_alarm_cancel_undo_requested = on_next_alarm_cancel_undo_requested,
    };
    struct timeval boot_time = {
        .tv_sec = 1741500000,
        .tv_usec = 0,
    };
    bsp_display_cfg_t display_cfg_copy = *display_cfg;
    esp_err_t err;

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

    bsp_display_start_with_config(&display_cfg_copy);
    bsp_display_backlight_on();

    err = alarm_audio_init(s_app.settings.alarm_volume);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to initialize alarm audio: %s", esp_err_to_name(err));
    } else {
        s_app.audio_available = true;
    }

    ESP_RETURN_ON_ERROR(wifi_time_init(&s_app.settings), TAG, "Failed to initialize Wi-Fi time");

    ESP_RETURN_ON_ERROR(bsp_display_lock(1000), TAG, "Failed to lock display");
    err = clock_ui_init(&s_app.settings, &s_app.runtime, &ui_callbacks, &s_app);
    if (err == ESP_OK) {
        clock_ui_refresh();
    }
    bsp_display_unlock();
    ESP_RETURN_ON_ERROR(err, TAG, "Failed to initialize UI");

    s_app.tick_timer = lv_timer_create(clock_tick_cb, 1000, NULL);
    if (s_app.tick_timer == NULL) {
        return ESP_ERR_NO_MEM;
    }

    err = bsp_display_lock(1000);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Skipping first tick because display lock timed out: %s", esp_err_to_name(err));
    } else {
        clock_tick_cb(s_app.tick_timer);
        bsp_display_unlock();
    }

    return ESP_OK;
}
