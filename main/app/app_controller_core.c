#include "app/app_controller_core.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "domain/alarm_scheduler.h"
#include "domain/brightness_policy.h"
#include "domain/settings_policy.h"

#define APP_SETTINGS_VERSION_V6 6U

static int64_t monotonic_ms(const app_controller_core_t *core)
{
    if (core->config.monotonic_ms == NULL) {
        return 0;
    }

    return core->config.monotonic_ms(core->config.monotonic_ctx);
}

static void mark_settings_dirty(app_controller_core_t *core)
{
    core->state.settings_dirty = true;
    core->state.save_deadline_ms = monotonic_ms(core) + 1000;
}

static void apply_runtime_brightness(app_controller_core_t *core)
{
    uint8_t target_brightness;
    uint8_t applied_brightness;

    if (core->config.display_service == NULL || core->config.display_service->set_brightness == NULL) {
        return;
    }

    target_brightness = core->state.runtime.effective_brightness;
    applied_brightness = target_brightness;
    if (applied_brightness < DISPLAY_BRIGHTNESS_MIN_PERCENT) {
        applied_brightness = DISPLAY_BRIGHTNESS_MIN_PERCENT;
    }
    if (applied_brightness > DISPLAY_BRIGHTNESS_MAX_PERCENT) {
        applied_brightness = DISPLAY_BRIGHTNESS_MAX_PERCENT;
    }

    if (core->state.applied_brightness == applied_brightness) {
        return;
    }

    if (core->config.display_service->set_brightness(applied_brightness) != 0) {
        return;
    }

    core->state.applied_brightness = applied_brightness;
}

static void maybe_save_settings(app_controller_core_t *core, bool force)
{
    if (!core->state.settings_dirty ||
        core->config.settings_store == NULL ||
        core->config.settings_store->save == NULL) {
        return;
    }

    if (!force && monotonic_ms(core) < core->state.save_deadline_ms) {
        return;
    }

    if (core->config.settings_store->save(&core->state.settings) != 0) {
        core->state.save_deadline_ms = monotonic_ms(core) + 2000;
        return;
    }

    core->state.settings_dirty = false;
}

static void set_default_settings(app_settings_t *settings)
{
    settings_policy_set_defaults(settings);
    settings->version = APP_SETTINGS_VERSION_V6;
}

static void execute_wifi_command(app_controller_core_t *core, const app_action_result_t *result)
{
    if (!app_action_has_effect(result, APP_EFFECT_WIFI_COMMAND) || core->config.wifi_service == NULL) {
        return;
    }

    switch (result->wifi_command) {
    case APP_WIFI_COMMAND_SCAN:
        if (core->config.wifi_service->start_scan != NULL) {
            core->config.wifi_service->start_scan();
        }
        break;
    case APP_WIFI_COMMAND_CONNECT:
        if (core->config.wifi_service->connect != NULL) {
            core->config.wifi_service->connect(result->wifi_ssid, result->wifi_password);
        }
        break;
    case APP_WIFI_COMMAND_FORGET:
        if (core->config.wifi_service->forget != NULL) {
            core->config.wifi_service->forget();
        }
        break;
    case APP_WIFI_COMMAND_SYNC:
        if (core->config.wifi_service->request_sync != NULL) {
            core->config.wifi_service->request_sync();
        }
        break;
    case APP_WIFI_COMMAND_NONE:
    default:
        break;
    }
}

static void reconcile_alarm_audio(app_controller_core_t *core)
{
    const audio_service_t *audio = core->config.audio_service;

    if (!core->state.audio_available || audio == NULL) {
        core->state.runtime.alarm_test_active = false;
        return;
    }

    if (core->state.runtime.alarm_ringing) {
        if (audio->is_alarm_active != NULL &&
            !audio->is_alarm_active() &&
            audio->start_alarm != NULL) {
            audio->start_alarm(core->state.settings.alarm_volume);
        }
    } else if (audio->is_alarm_active != NULL && audio->is_alarm_active() && audio->stop != NULL) {
        audio->stop();
    }

    if (audio->is_test_active != NULL) {
        core->state.runtime.alarm_test_active = audio->is_test_active();
    } else {
        core->state.runtime.alarm_test_active = false;
    }
}

static void apply_audio_preferences(app_controller_core_t *core)
{
    const audio_service_t *audio = core->config.audio_service;

    if (!core->state.audio_available || audio == NULL) {
        return;
    }

    if (audio->set_volume != NULL) {
        audio->set_volume(core->state.settings.alarm_volume);
    }
    if (audio->set_ascending_enabled != NULL) {
        audio->set_ascending_enabled(core->state.settings.ascending_alarm_enabled);
    }
}

static void reconcile_settings_side_effects(app_controller_core_t *core, time_t now)
{
    if (core->config.clock_service != NULL && core->config.clock_service->apply_timezone != NULL) {
        core->config.clock_service->apply_timezone(core->state.settings.wifi.timezone_offset_hours);
    }

    core->state.runtime.effective_brightness =
        brightness_policy_get_target(&core->state.runtime, &core->state.settings, now);
}

void app_controller_core_init(app_controller_core_t *core, const app_controller_core_config_t *config)
{
    memset(core, 0, sizeof(*core));
    if (config != NULL) {
        core->config = *config;
    }
}

int app_controller_core_bootstrap(app_controller_core_t *core, time_t fallback_boot_epoch)
{
    time_t boot_epoch = fallback_boot_epoch;
    int err = 0;

    set_default_settings(&core->state.settings);
    if (core->config.settings_store != NULL && core->config.settings_store->load != NULL) {
        err = core->config.settings_store->load(&core->state.settings);
        if (err != 0) {
            set_default_settings(&core->state.settings);
        }
    }

    if (core->config.clock_service != NULL && core->config.clock_service->apply_timezone != NULL) {
        core->config.clock_service->apply_timezone(core->state.settings.wifi.timezone_offset_hours);
    }

    if (core->state.settings.last_synced_epoch > 1700000000) {
        boot_epoch = core->state.settings.last_synced_epoch;
    }
    if (core->config.clock_service != NULL && core->config.clock_service->set_epoch != NULL) {
        core->config.clock_service->set_epoch(boot_epoch);
    }

    alarm_scheduler_init(&core->state.runtime, &core->state.settings);
    core->state.applied_brightness = UCHAR_MAX;

    if (core->config.audio_service != NULL && core->config.audio_service->init != NULL) {
        if (core->config.audio_service->init(core->state.settings.alarm_volume) == 0) {
            core->state.audio_available = true;
            if (core->config.audio_service->set_ascending_enabled != NULL) {
                core->config.audio_service->set_ascending_enabled(core->state.settings.ascending_alarm_enabled);
            }
        }
    }

    if (core->config.wifi_service != NULL && core->config.wifi_service->init != NULL) {
        err = core->config.wifi_service->init(&core->state.settings);
        if (err != 0) {
            return err;
        }
    }

    return 0;
}

void app_controller_core_apply_action_result(app_controller_core_t *core,
                                             const app_action_result_t *result,
                                             time_t now)
{
    if (app_action_has_effect(result, APP_EFFECT_SETTINGS_CHANGED)) {
        reconcile_settings_side_effects(core, now);
        mark_settings_dirty(core);
    }

    if (app_action_has_effect(result, APP_EFFECT_WIFI_COMMAND)) {
        execute_wifi_command(core, result);
    }

    if (app_action_has_effect(result, APP_EFFECT_AUDIO_VOLUME)) {
        apply_audio_preferences(core);
    }

    if (app_action_has_effect(result, APP_EFFECT_AUDIO_RECONCILE)) {
        reconcile_alarm_audio(core);
    }

    if (app_action_has_effect(result, APP_EFFECT_BRIGHTNESS_APPLY)) {
        apply_runtime_brightness(core);
    }

    if (app_action_has_effect(result, APP_EFFECT_UI_REFRESH) && core->config.ui_refresh != NULL) {
        core->config.ui_refresh(core->config.ui_refresh_ctx);
    }
}

void app_controller_core_toggle_alarm_test(app_controller_core_t *core, time_t now)
{
    app_action_result_t result = app_action_alarm_test_toggle(&core->state);
    const audio_service_t *audio = core->config.audio_service;

    if (!core->state.audio_available || audio == NULL) {
        return;
    }

    if (audio->is_test_active != NULL && audio->is_test_active()) {
        if (audio->stop != NULL) {
            audio->stop();
        }
    } else if (audio->start_test != NULL) {
        audio->start_test(core->state.settings.alarm_volume);
    }

    if (audio->is_test_active != NULL) {
        core->state.runtime.alarm_test_active = audio->is_test_active();
    }
    app_controller_core_apply_action_result(core, &result, now);
}

void app_controller_core_arm_cancel_revert_window(app_controller_core_t *core, int64_t duration_ms)
{
    core->state.cancel_revert_deadline_ms = monotonic_ms(core) + duration_ms;
}

bool app_controller_core_cancel_revert_window_active(const app_controller_core_t *core)
{
    return core->state.cancel_revert_available &&
           monotonic_ms(core) <= core->state.cancel_revert_deadline_ms;
}

time_t app_controller_core_tick(app_controller_core_t *core)
{
    time_t now = 0;

    if (core->config.clock_service != NULL && core->config.clock_service->now != NULL) {
        now = core->config.clock_service->now();
    }

    if (core->config.wifi_service != NULL && core->config.wifi_service->snapshot != NULL) {
        core->config.wifi_service->snapshot(&core->state.runtime);
    }
    if (alarm_scheduler_tick(&core->state.runtime, &core->state.settings, now)) {
        mark_settings_dirty(core);
    }
    reconcile_alarm_audio(core);

    if (core->state.runtime.time_synced &&
        now > 1700000000 &&
        (core->state.settings.last_synced_epoch == 0 ||
         (now - core->state.settings.last_synced_epoch) >= 300)) {
        core->state.settings.last_synced_epoch = now;
        mark_settings_dirty(core);
    }

    apply_runtime_brightness(core);
    maybe_save_settings(core, false);
    return now;
}

void app_controller_core_force_save(app_controller_core_t *core)
{
    maybe_save_settings(core, true);
}
