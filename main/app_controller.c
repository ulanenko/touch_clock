#include "app_controller.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "app/app_actions.h"
#include "app/app_controller_core.h"
#include "clock_ui.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "platform/platform_esp_services.h"

typedef struct {
    app_controller_core_t core;
    lv_timer_t *tick_timer;
} app_controller_t;

static const char *TAG = "clock_app";
static app_controller_t s_app = {0};

static int64_t monotonic_ms(void *ctx)
{
    (void)ctx;
    return esp_timer_get_time() / 1000;
}

static void ui_refresh(void *ctx)
{
    LV_UNUSED(ctx);
    clock_ui_refresh();
}

static void on_set_base_brightness(void *user_ctx, uint8_t hw_percent)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    time_t now = app->core.config.clock_service->now();
    app_action_result_t result = app_action_set_base_brightness(&app->core.state, hw_percent, now);

    app_controller_core_apply_action_result(&app->core, &result, now);
}

static void on_set_runtime_night_brightness(void *user_ctx, uint8_t hw_percent)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    time_t now = app->core.config.clock_service->now();
    app_action_result_t result =
        app_action_set_runtime_night_brightness_override(&app->core.state, hw_percent, now);

    app_controller_core_apply_action_result(&app->core, &result, now);
}

static void on_set_temporary_brightness_floor(void *user_ctx, bool enabled, uint8_t hw_percent)
{
    app_controller_t *app = (app_controller_t *)user_ctx;

    app_controller_core_set_temporary_brightness_floor(&app->core, enabled, hw_percent);
}

static void on_set_current_face(void *user_ctx, clock_face_id_t face)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    app_action_result_t result = app_action_set_current_face(&app->core.state, face);

    app_controller_core_apply_action_result(&app->core, &result, app->core.config.clock_service->now());
}

static void on_set_face_theme(void *user_ctx, clock_face_id_t face, uint8_t theme)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    app_action_result_t result = app_action_set_face_theme(&app->core.state, face, theme);

    app_controller_core_apply_action_result(&app->core, &result, app->core.config.clock_service->now());
}

static void on_set_night_face(void *user_ctx, clock_face_id_t face)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    app_action_result_t result = app_action_set_night_face(&app->core.state, face);

    app_controller_core_apply_action_result(&app->core, &result, app->core.config.clock_service->now());
}

static void on_set_timezone(void *user_ctx, uint8_t timezone_id)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    time_t now = app->core.config.clock_service->now();
    app_action_result_t result = app_action_set_timezone(&app->core.state, timezone_id, now);

    app_controller_core_apply_action_result(&app->core, &result, now);
}

static void on_set_time_sync_mode(void *user_ctx, time_sync_mode_t mode)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    time_t now = app->core.config.clock_service->now();
    app_action_result_t result = app_action_set_time_sync_mode(&app->core.state, mode, now);

    app_controller_core_apply_action_result(&app->core, &result, now);
}

static void on_set_manual_time(void *user_ctx, uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    struct tm local = {
        .tm_year = (int)year - 1900,
        .tm_mon = (int)month - 1,
        .tm_mday = day,
        .tm_hour = hour,
        .tm_min = minute,
        .tm_sec = 0,
        .tm_isdst = -1,
    };
    time_t epoch = mktime(&local);
    app_action_result_t result = app_action_set_manual_time(&app->core.state, epoch);

    app_controller_core_apply_action_result(&app->core, &result, epoch);
}

static void on_save_wifi_credentials(void *user_ctx, const char *ssid, const char *password)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    app_action_result_t result = app_action_save_wifi_credentials(&app->core.state, ssid, password);

    app_controller_core_apply_action_result(&app->core, &result, app->core.config.clock_service->now());
}

static void on_wifi_scan_requested(void *user_ctx)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    app_action_result_t result = app_action_request_wifi_scan(&app->core.state);

    app_controller_core_apply_action_result(&app->core, &result, app->core.config.clock_service->now());
}

static void on_wifi_forget_requested(void *user_ctx)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    app_action_result_t result = app_action_forget_wifi(&app->core.state);

    app_controller_core_apply_action_result(&app->core, &result, app->core.config.clock_service->now());
}

static void on_wifi_sync_requested(void *user_ctx)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    app_action_result_t result = app_action_request_time_sync(&app->core.state);

    app_controller_core_apply_action_result(&app->core, &result, app->core.config.clock_service->now());
}

static void on_ota_check_requested(void *user_ctx)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    app_action_result_t result = app_action_request_ota_check(&app->core.state);

    app_controller_core_apply_action_result(&app->core, &result, app->core.config.clock_service->now());
}

static void on_ota_install_requested(void *user_ctx)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    app_action_result_t result = app_action_request_ota_install(&app->core.state);

    app_controller_core_apply_action_result(&app->core, &result, app->core.config.clock_service->now());
}

static uint8_t ui_click_volume(const app_controller_t *app)
{
    return app->core.state.settings.ui_click_volume;
}

static void on_ui_click_feedback(void *user_ctx)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    const audio_service_t *audio = app->core.config.audio_service;

    if (audio == NULL ||
        audio->play_ui_click == NULL ||
        !app->core.state.audio_available ||
        !app->core.state.settings.ui_click_sound_enabled ||
        app->core.state.runtime.in_night_mode ||
        app->core.state.settings.ui_click_volume == 0U) {
        return;
    }

    (void)audio->play_ui_click(ui_click_volume(app));
}

static void on_set_ui_click_sound_enabled(void *user_ctx, bool enabled)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    app_action_result_t result = app_action_set_ui_click_sound_enabled(&app->core.state, enabled);

    app_controller_core_apply_action_result(&app->core, &result, app->core.config.clock_service->now());
}

static void on_set_ui_click_volume(void *user_ctx, uint8_t volume)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    app_action_result_t result = app_action_set_ui_click_volume(&app->core.state, volume);

    app_controller_core_apply_action_result(&app->core, &result, app->core.config.clock_service->now());
}

static void on_set_night_mode_enabled(void *user_ctx, bool enabled)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    time_t now = app->core.config.clock_service->now();
    app_action_result_t result = app_action_set_night_mode_enabled(&app->core.state, enabled, now);

    app_controller_core_apply_action_result(&app->core, &result, now);
}

static void on_set_night_schedule(void *user_ctx,
                                  uint8_t start_hour,
                                  uint8_t start_minute,
                                  uint8_t end_hour,
                                  uint8_t end_minute)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    time_t now = app->core.config.clock_service->now();
    app_action_result_t result = app_action_set_night_schedule(&app->core.state,
                                                               start_hour,
                                                               start_minute,
                                                               end_hour,
                                                               end_minute,
                                                               now);

    app_controller_core_apply_action_result(&app->core, &result, now);
}

static void on_set_night_brightness(void *user_ctx, uint8_t hw_percent)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    time_t now = app->core.config.clock_service->now();
    app_action_result_t result = app_action_set_night_brightness(&app->core.state, hw_percent, now);

    app_controller_core_apply_action_result(&app->core, &result, now);
}

static void on_set_night_sunrise_brightness_enabled(void *user_ctx, bool enabled)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    time_t now = app->core.config.clock_service->now();
    app_action_result_t result =
        app_action_set_night_sunrise_brightness_enabled(&app->core.state, enabled, now);

    app_controller_core_apply_action_result(&app->core, &result, now);
}

static void on_set_alarm_volume(void *user_ctx, uint8_t volume)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    time_t now = app->core.config.clock_service->now();
    app_action_result_t result = app_action_set_alarm_volume(&app->core.state, volume, now);

    app_controller_core_apply_action_result(&app->core, &result, now);
}

static void on_set_ascending_alarm_enabled(void *user_ctx, bool enabled)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    app_action_result_t result = app_action_set_ascending_alarm_enabled(&app->core.state, enabled);

    app_controller_core_apply_action_result(&app->core, &result, app->core.config.clock_service->now());
}

static void on_set_snooze_minutes(void *user_ctx, uint8_t minutes)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    app_action_result_t result = app_action_set_snooze_minutes(&app->core.state, minutes);

    app_controller_core_apply_action_result(&app->core, &result, app->core.config.clock_service->now());
}

static void on_set_alarm_enabled(void *user_ctx, uint8_t alarm_index, bool enabled)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    time_t now = app->core.config.clock_service->now();
    app_action_result_t result = app_action_set_alarm_enabled(&app->core.state, alarm_index, enabled, now);

    app_controller_core_apply_action_result(&app->core, &result, now);
}

static void on_save_alarm(void *user_ctx, uint8_t alarm_index, const alarm_config_t *alarm)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    time_t now = app->core.config.clock_service->now();
    app_action_result_t result = app_action_save_alarm(&app->core.state, alarm_index, alarm, now);

    app_controller_core_apply_action_result(&app->core, &result, now);
}

static void on_delete_alarm(void *user_ctx, uint8_t alarm_index)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    time_t now = app->core.config.clock_service->now();
    app_action_result_t result = app_action_delete_alarm(&app->core.state, alarm_index, now);

    app_controller_core_apply_action_result(&app->core, &result, now);
}

static void on_alarm_snooze_requested(void *user_ctx)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    time_t now = app->core.config.clock_service->now();
    app_action_result_t result = app_action_alarm_snooze(&app->core.state, now);

    app_controller_core_apply_action_result(&app->core, &result, now);
}

static void on_alarm_stop_requested(void *user_ctx)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    time_t now = app->core.config.clock_service->now();
    app_action_result_t result = app_action_alarm_stop(&app->core.state, now);

    app_controller_core_apply_action_result(&app->core, &result, now);
}

static void on_alarm_test_requested(void *user_ctx)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    app_controller_core_toggle_alarm_test(&app->core, app->core.config.clock_service->now());
}

static void on_alarm_test_stop_requested(void *user_ctx)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    app_controller_core_stop_alarm_test(&app->core);
}

static void on_next_alarm_cancel_requested(void *user_ctx)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    time_t now = app->core.config.clock_service->now();
    app_action_result_t result = app_action_cancel_next_alarm(&app->core.state, now);

    if (app_action_has_effect(&result, APP_EFFECT_SETTINGS_CHANGED)) {
        app_controller_core_arm_cancel_revert_window(&app->core, 2500);
    }
    app_controller_core_apply_action_result(&app->core, &result, now);
}

static void on_next_alarm_cancel_undo_requested(void *user_ctx)
{
    app_controller_t *app = (app_controller_t *)user_ctx;
    app_action_result_t result;

    if (!app_controller_core_cancel_revert_window_active(&app->core)) {
        return;
    }

    result = app_action_undo_cancel_next_alarm(&app->core.state, app->core.config.clock_service->now());
    app_controller_core_apply_action_result(&app->core, &result, app->core.config.clock_service->now());
}

static void clock_tick_cb(lv_timer_t *timer)
{
    app_controller_t *app = &s_app;
    time_t now;

    LV_UNUSED(timer);

    now = app_controller_core_tick(&app->core);
    clock_ui_tick(now);
}

esp_err_t app_controller_start(const bsp_display_cfg_t *display_cfg)
{
    clock_ui_callbacks_t ui_callbacks = {
        .on_set_base_brightness = on_set_base_brightness,
        .on_set_runtime_night_brightness = on_set_runtime_night_brightness,
        .on_set_temporary_brightness_floor = on_set_temporary_brightness_floor,
        .on_set_current_face = on_set_current_face,
        .on_set_face_theme = on_set_face_theme,
        .on_set_night_face = on_set_night_face,
        .on_set_timezone = on_set_timezone,
        .on_set_time_sync_mode = on_set_time_sync_mode,
        .on_set_manual_time = on_set_manual_time,
        .on_save_wifi_credentials = on_save_wifi_credentials,
        .on_wifi_scan_requested = on_wifi_scan_requested,
        .on_wifi_forget_requested = on_wifi_forget_requested,
        .on_wifi_sync_requested = on_wifi_sync_requested,
        .on_ota_check_requested = on_ota_check_requested,
        .on_ota_install_requested = on_ota_install_requested,
        .on_ui_click_feedback = on_ui_click_feedback,
        .on_set_ui_click_sound_enabled = on_set_ui_click_sound_enabled,
        .on_set_ui_click_volume = on_set_ui_click_volume,
        .on_set_night_mode_enabled = on_set_night_mode_enabled,
        .on_set_night_schedule = on_set_night_schedule,
        .on_set_night_brightness = on_set_night_brightness,
        .on_set_night_sunrise_brightness_enabled = on_set_night_sunrise_brightness_enabled,
        .on_set_alarm_volume = on_set_alarm_volume,
        .on_set_ascending_alarm_enabled = on_set_ascending_alarm_enabled,
        .on_set_snooze_minutes = on_set_snooze_minutes,
        .on_set_alarm_enabled = on_set_alarm_enabled,
        .on_save_alarm = on_save_alarm,
        .on_delete_alarm = on_delete_alarm,
        .on_alarm_snooze_requested = on_alarm_snooze_requested,
        .on_alarm_stop_requested = on_alarm_stop_requested,
        .on_alarm_test_requested = on_alarm_test_requested,
        .on_alarm_test_stop_requested = on_alarm_test_stop_requested,
        .on_next_alarm_cancel_requested = on_next_alarm_cancel_requested,
        .on_next_alarm_cancel_undo_requested = on_next_alarm_cancel_undo_requested,
    };
    bsp_display_cfg_t display_cfg_copy = *display_cfg;
    app_controller_core_config_t core_config = {
        .clock_service = platform_esp_clock_time_service(),
        .audio_service = platform_esp_audio_service(),
        .wifi_service = platform_esp_wifi_service(),
        .display_service = platform_esp_display_service(),
        .ota_service = platform_esp_ota_service(),
        .telemetry_service = platform_esp_telemetry_service(),
        .settings_store = platform_esp_settings_store(),
        .monotonic_ms = monotonic_ms,
        .monotonic_ctx = NULL,
        .ui_refresh = ui_refresh,
        .ui_refresh_ctx = NULL,
    };
    esp_err_t err;

    memset(&s_app, 0, sizeof(s_app));
    app_controller_core_init(&s_app.core, &core_config);

    bsp_display_start_with_config(&display_cfg_copy);
    bsp_display_backlight_off();

    err = (esp_err_t)app_controller_core_bootstrap(&s_app.core, 1741500000);
    ESP_RETURN_ON_ERROR(err, TAG, "Failed to initialize controller core");

    ESP_RETURN_ON_ERROR(bsp_display_lock(1000), TAG, "Failed to lock display");
    err = clock_ui_begin_boot(&s_app.core.state.settings, &s_app.core.state.runtime, &ui_callbacks, &s_app);
    if (err == ESP_OK) {
        bsp_display_backlight_on();
        err = clock_ui_finish_boot();
    }
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

    bsp_display_backlight_on();

    return ESP_OK;
}
