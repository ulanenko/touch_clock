#pragma once

#include <time.h>

#include "esp_err.h"

#include "app_settings.h"

typedef struct {
    void (*on_settings_changed)(void *user_ctx);
    void (*on_runtime_brightness_changed)(void *user_ctx);
    void (*on_wifi_scan_requested)(void *user_ctx);
    void (*on_wifi_connect_requested)(void *user_ctx, const char *ssid, const char *password);
    void (*on_wifi_forget_requested)(void *user_ctx);
    void (*on_wifi_sync_requested)(void *user_ctx);
    void (*on_alarm_snooze_requested)(void *user_ctx);
    void (*on_alarm_stop_requested)(void *user_ctx);
    void (*on_alarm_test_requested)(void *user_ctx);
    void (*on_next_alarm_cancel_requested)(void *user_ctx);
    void (*on_next_alarm_cancel_undo_requested)(void *user_ctx);
} clock_ui_callbacks_t;

esp_err_t clock_ui_init(app_settings_t *settings,
                        app_runtime_state_t *runtime,
                        const clock_ui_callbacks_t *callbacks,
                        void *user_ctx);
void clock_ui_tick(time_t now);
void clock_ui_refresh(void);
