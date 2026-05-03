#include "app/app_controller_core.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "domain/alarm_scheduler.h"
#include "domain/brightness_policy.h"
#include "domain/settings_policy.h"

#define APP_SETTINGS_VERSION_V7 7U
#define APP_ALARM_AUTO_STOP_SECONDS 600
#define APP_BRIGHTNESS_FADE_DOWN_MS 3500

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

static uint8_t clamp_display_brightness(uint8_t brightness)
{
    if (brightness < DISPLAY_BRIGHTNESS_MIN_PERCENT) {
        return DISPLAY_BRIGHTNESS_MIN_PERCENT;
    }
    if (brightness > DISPLAY_BRIGHTNESS_MAX_PERCENT) {
        return DISPLAY_BRIGHTNESS_MAX_PERCENT;
    }
    return brightness;
}

static void start_brightness_fade_down(app_controller_core_t *core, uint8_t target_brightness, int64_t duration_ms)
{
    uint8_t target = clamp_display_brightness(target_brightness);
    uint8_t start = core->state.applied_brightness;

    if (start == UCHAR_MAX) {
        start = target;
    }
    start = clamp_display_brightness(start);

    if (duration_ms <= 0 || start <= target) {
        core->state.brightness_fade_active = false;
        return;
    }

    core->state.brightness_fade_active = true;
    core->state.brightness_fade_start = start;
    core->state.brightness_fade_target = target;
    core->state.brightness_fade_start_ms = monotonic_ms(core);
    core->state.brightness_fade_duration_ms = duration_ms;
}

static void apply_runtime_brightness(app_controller_core_t *core)
{
    uint8_t target_brightness;
    uint8_t applied_brightness;

    if (core->config.display_service == NULL || core->config.display_service->set_brightness == NULL) {
        return;
    }

    target_brightness = clamp_display_brightness(core->state.runtime.effective_brightness);
    if (core->state.temporary_brightness_floor_active &&
        target_brightness < core->state.temporary_brightness_floor) {
        target_brightness = core->state.temporary_brightness_floor;
    }
    applied_brightness = target_brightness;
    if (core->state.brightness_fade_active) {
        int64_t now_ms = monotonic_ms(core);
        int64_t elapsed_ms = now_ms - core->state.brightness_fade_start_ms;

        core->state.brightness_fade_target = target_brightness;
        if (elapsed_ms < 0) {
            elapsed_ms = 0;
        }
        if (elapsed_ms >= core->state.brightness_fade_duration_ms ||
            core->state.brightness_fade_duration_ms <= 0) {
            core->state.brightness_fade_active = false;
            applied_brightness = target_brightness;
        } else {
            int diff = (int)core->state.brightness_fade_target -
                       (int)core->state.brightness_fade_start;
            applied_brightness = (uint8_t)((int)core->state.brightness_fade_start +
                                           (int)((diff * elapsed_ms) /
                                                 core->state.brightness_fade_duration_ms));
        }
    }

    if (core->state.applied_brightness == applied_brightness) {
        return;
    }

    if (core->config.display_service->set_brightness(applied_brightness) != 0) {
        return;
    }

    core->state.applied_brightness = applied_brightness;
}

static bool update_alarm_auto_stop(app_controller_core_t *core, time_t now)
{
    if (!core->state.runtime.alarm_ringing) {
        core->state.alarm_auto_stop_armed = false;
        core->state.alarm_auto_stop_deadline = 0;
        return false;
    }

    if (!core->state.alarm_auto_stop_armed) {
        core->state.alarm_auto_stop_armed = true;
        core->state.alarm_auto_stop_deadline = now + APP_ALARM_AUTO_STOP_SECONDS;
        return false;
    }

    if (now < core->state.alarm_auto_stop_deadline) {
        return false;
    }

    alarm_scheduler_stop(&core->state.runtime);
    core->state.alarm_auto_stop_armed = false;
    core->state.alarm_auto_stop_deadline = 0;
    core->state.runtime.effective_brightness =
        brightness_policy_get_target(&core->state.runtime, &core->state.settings, now);
    start_brightness_fade_down(core,
                               core->state.runtime.effective_brightness,
                               APP_BRIGHTNESS_FADE_DOWN_MS);
    return true;
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
    settings->version = APP_SETTINGS_VERSION_V7;
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

    if (!core->state.runtime.alarm_ringing) {
        core->state.alarm_auto_stop_armed = false;
        core->state.alarm_auto_stop_deadline = 0;
    }

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
        core->config.clock_service->apply_timezone(core->state.settings.wifi.timezone_id);
    }

    if (core->config.wifi_service != NULL && core->config.wifi_service->set_auto_sync != NULL) {
        core->config.wifi_service->set_auto_sync(core->state.settings.wifi.time_sync_mode == TIME_SYNC_MODE_AUTO);
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
        core->config.clock_service->apply_timezone(core->state.settings.wifi.timezone_id);
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

    if (app_action_has_effect(result, APP_EFFECT_CLOCK_SET) &&
        core->config.clock_service != NULL &&
        core->config.clock_service->set_epoch != NULL) {
        core->config.clock_service->set_epoch(result->clock_epoch);
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

    if (app_action_has_effect(result, APP_EFFECT_BRIGHTNESS_FADE)) {
        start_brightness_fade_down(core,
                                   core->state.runtime.effective_brightness,
                                   APP_BRIGHTNESS_FADE_DOWN_MS);
    }

    if (app_action_has_effect(result, APP_EFFECT_BRIGHTNESS_APPLY)) {
        apply_runtime_brightness(core);
    }

    if (app_action_has_effect(result, APP_EFFECT_UI_REFRESH) && core->config.ui_refresh != NULL) {
        core->config.ui_refresh(core->config.ui_refresh_ctx);
    }
}

void app_controller_core_set_temporary_brightness_floor(app_controller_core_t *core,
                                                        bool enabled,
                                                        uint8_t floor_brightness)
{
    uint8_t floor = clamp_display_brightness(floor_brightness);

    if (core->state.temporary_brightness_floor_active == enabled &&
        core->state.temporary_brightness_floor == floor) {
        return;
    }

    core->state.temporary_brightness_floor_active = enabled;
    core->state.temporary_brightness_floor = floor;
    apply_runtime_brightness(core);
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

void app_controller_core_stop_alarm_test(app_controller_core_t *core)
{
    const audio_service_t *audio = core->config.audio_service;

    if (!core->state.audio_available || audio == NULL) {
        core->state.runtime.alarm_test_active = false;
        return;
    }

    if (audio->is_test_active != NULL && audio->is_test_active() && audio->stop != NULL) {
        audio->stop();
    }
    if (audio->is_test_active != NULL) {
        core->state.runtime.alarm_test_active = audio->is_test_active();
    } else {
        core->state.runtime.alarm_test_active = false;
    }

    if (core->config.ui_refresh != NULL) {
        core->config.ui_refresh(core->config.ui_refresh_ctx);
    }
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
    if (update_alarm_auto_stop(core, now) && core->config.ui_refresh != NULL) {
        core->config.ui_refresh(core->config.ui_refresh_ctx);
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
