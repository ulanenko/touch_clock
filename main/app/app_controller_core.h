#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "app/app_actions.h"
#include "platform/platform_services.h"

typedef struct {
    const clock_time_service_t *clock_service;
    const audio_service_t *audio_service;
    const wifi_service_t *wifi_service;
    const display_service_t *display_service;
    const settings_store_t *settings_store;
    int64_t (*monotonic_ms)(void *ctx);
    void *monotonic_ctx;
    void (*ui_refresh)(void *ctx);
    void *ui_refresh_ctx;
} app_controller_core_config_t;

typedef struct {
    app_state_t state;
    app_controller_core_config_t config;
} app_controller_core_t;

void app_controller_core_init(app_controller_core_t *core, const app_controller_core_config_t *config);
int app_controller_core_bootstrap(app_controller_core_t *core, time_t fallback_boot_epoch);
void app_controller_core_apply_action_result(app_controller_core_t *core,
                                             const app_action_result_t *result,
                                             time_t now);
void app_controller_core_set_temporary_brightness_floor(app_controller_core_t *core,
                                                        bool enabled,
                                                        uint8_t floor_brightness);
void app_controller_core_toggle_alarm_test(app_controller_core_t *core, time_t now);
void app_controller_core_stop_alarm_test(app_controller_core_t *core);
void app_controller_core_arm_cancel_revert_window(app_controller_core_t *core, int64_t duration_ms);
bool app_controller_core_cancel_revert_window_active(const app_controller_core_t *core);
time_t app_controller_core_tick(app_controller_core_t *core);
void app_controller_core_force_save(app_controller_core_t *core);
