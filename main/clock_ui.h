#pragma once

#include <stdbool.h>
#include <time.h>

#include "esp_err.h"

#include "app_settings.h"

typedef struct {
    void (*on_set_base_brightness)(void *user_ctx, uint8_t hw_percent);
    void (*on_set_runtime_night_brightness)(void *user_ctx, uint8_t hw_percent);
    void (*on_set_current_face)(void *user_ctx, clock_face_id_t face);
    void (*on_set_face_theme)(void *user_ctx, clock_face_id_t face, uint8_t theme);
    void (*on_set_night_face)(void *user_ctx, clock_face_id_t face);
    void (*on_set_timezone)(void *user_ctx, int8_t utc_offset_hours);
    void (*on_save_wifi_credentials)(void *user_ctx, const char *ssid, const char *password);
    void (*on_wifi_scan_requested)(void *user_ctx);
    void (*on_wifi_forget_requested)(void *user_ctx);
    void (*on_wifi_sync_requested)(void *user_ctx);
    void (*on_set_night_mode_enabled)(void *user_ctx, bool enabled);
    void (*on_set_night_schedule)(void *user_ctx,
                                  uint8_t start_hour,
                                  uint8_t start_minute,
                                  uint8_t end_hour,
                                  uint8_t end_minute);
    void (*on_set_night_brightness)(void *user_ctx, uint8_t hw_percent);
    void (*on_set_alarm_volume)(void *user_ctx, uint8_t volume);
    void (*on_set_ascending_alarm_enabled)(void *user_ctx, bool enabled);
    void (*on_set_snooze_minutes)(void *user_ctx, uint8_t minutes);
    void (*on_set_alarm_enabled)(void *user_ctx, uint8_t alarm_index, bool enabled);
    void (*on_save_alarm)(void *user_ctx, uint8_t alarm_index, const alarm_config_t *alarm);
    void (*on_delete_alarm)(void *user_ctx, uint8_t alarm_index);
    void (*on_alarm_snooze_requested)(void *user_ctx);
    void (*on_alarm_stop_requested)(void *user_ctx);
    void (*on_alarm_test_requested)(void *user_ctx);
    void (*on_next_alarm_cancel_requested)(void *user_ctx);
    void (*on_next_alarm_cancel_undo_requested)(void *user_ctx);
} clock_ui_callbacks_t;

esp_err_t clock_ui_begin_boot(const app_settings_t *settings,
                              const app_runtime_state_t *runtime,
                              const clock_ui_callbacks_t *callbacks,
                              void *user_ctx);
esp_err_t clock_ui_finish_boot(void);
void clock_ui_tick(time_t now);
void clock_ui_refresh(void);
