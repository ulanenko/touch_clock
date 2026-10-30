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
#define APP_OTA_AUTO_CHECK_INITIAL_DELAY_MS 30000
#define APP_OTA_AUTO_CHECK_INTERVAL_MS (6 * 60 * 60 * 1000)

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

/* The sunrise rises in per-mille steps where the display can take them (whole percents would
 * show as steps in a dark room). True while it does. */
static bool apply_sunrise_brightness(app_controller_core_t *core, time_t now)
{
    const display_service_t *display = core->config.display_service;
    uint16_t permille;

    if (core->state.sunrise_preview) {
        permille = core->state.sunrise_preview_progress > 1000
                       ? DISPLAY_BRIGHTNESS_MAX_PERCENT * 10U
                       : brightness_policy_sunrise_permille(&core->state.runtime, &core->state.settings,
                                                            core->state.sunrise_preview_progress);
        if (display->set_level_permille == NULL) {
            permille = (uint16_t)((permille + 5U) / 10U * 10U);
            if (permille != core->state.applied_permille &&
                display->set_brightness((uint8_t)(permille / 10U)) == 0) {
                core->state.applied_permille = permille;
                core->state.applied_brightness = (uint8_t)(permille / 10U);
            }
            return true;
        }
    } else if (display->set_level_permille == NULL || core->state.runtime.alarm_ringing ||
        !core->state.runtime.sunrise_active || !core->state.runtime.sunrise_from_night ||
        core->state.temporary_brightness_floor_active) {
        if (core->state.applied_permille != 0) {
            core->state.applied_permille = 0;
            core->state.applied_brightness = UCHAR_MAX; /* set again, in percent */
        }
        return false;
    } else {
        permille = brightness_policy_get_target_permille(&core->state.runtime, &core->state.settings, now);
    }
    if (permille != core->state.applied_permille && display->set_level_permille(permille) == 0) {
        core->state.applied_permille = permille;
        core->state.applied_brightness = (uint8_t)((permille + 5U) / 10U);
    }
    return true;
}

static void apply_runtime_brightness(app_controller_core_t *core)
{
    uint8_t target_brightness;
    uint8_t applied_brightness;

    if (core->config.display_service == NULL || core->config.display_service->set_brightness == NULL) {
        return;
    }
    if (apply_sunrise_brightness(core, core->config.clock_service != NULL && core->config.clock_service->now != NULL
                                           ? core->config.clock_service->now()
                                           : 0)) {
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

static void execute_ota_command(app_controller_core_t *core, const app_action_result_t *result)
{
    if (!app_action_has_effect(result, APP_EFFECT_OTA_COMMAND) || core->config.ota_service == NULL) {
        return;
    }

    switch (result->ota_command) {
    case APP_OTA_COMMAND_CHECK:
        if (core->config.ota_service->request_check != NULL) {
            core->config.ota_service->request_check();
        }
        break;
    case APP_OTA_COMMAND_INSTALL:
        if (core->config.ota_service->request_install != NULL) {
            core->config.ota_service->request_install();
        }
        break;
    case APP_OTA_COMMAND_NONE:
    default:
        break;
    }

    if (core->config.ota_service->snapshot != NULL) {
        core->config.ota_service->snapshot(&core->state.runtime);
    }
}

static void maybe_request_periodic_ota_check(app_controller_core_t *core)
{
    int64_t now_ms;

    if (core->config.ota_service == NULL ||
        core->config.ota_service->request_check == NULL ||
        !core->state.runtime.wifi_connected ||
        !core->state.runtime.ota_configured ||
        core->state.runtime.ota_busy ||
        core->state.runtime.ota_update_available ||
        core->state.runtime.ota_reboot_pending) {
        return;
    }

    now_ms = monotonic_ms(core);
    if (now_ms < core->state.ota_next_check_ms) {
        return;
    }

    core->state.ota_next_check_ms = now_ms + APP_OTA_AUTO_CHECK_INTERVAL_MS;
    core->config.ota_service->request_check();
}

static void play_sunrise_chime(app_controller_core_t *core)
{
    const audio_service_t *audio = core->config.audio_service;

    if (core->state.audio_available && audio != NULL && audio->play_sunrise_chime != NULL &&
        core->state.settings.alarm_volume > 0) {
        /* Well under the alarm: it is there to be half heard. */
        (void)audio->play_sunrise_chime((uint8_t)(core->state.settings.alarm_volume / 3U));
    }
}

/* The sunrise's soft chime, once, a minute before its alarm. */
static void maybe_play_sunrise_chime(app_controller_core_t *core, time_t now)
{
    app_runtime_state_t *rt = &core->state.runtime;

    if (!rt->sunrise_active || !rt->sunrise_from_night || !core->state.settings.night_mode.sunrise_chime ||
        rt->next_alarm_epoch - now > 60 || rt->next_alarm_epoch <= now || rt->sunrise_chimed_epoch == rt->next_alarm_epoch) {
        return;
    }
    rt->sunrise_chimed_epoch = rt->next_alarm_epoch;
    play_sunrise_chime(core);
}

void app_controller_core_sunrise_preview(app_controller_core_t *core, int16_t progress)
{
    app_state_t *s = &core->state;
    uint32_t window = alarm_scheduler_sunrise_seconds(&s->settings);

    if (progress < 0) {
        if (!s->sunrise_preview) {
            return;
        }
        /* Back to the clock's own level, down from the preview's as after an alarm. */
        s->sunrise_preview = false;
        s->applied_permille = 0;
        start_brightness_fade_down(core, s->runtime.effective_brightness, 1500);
        apply_runtime_brightness(core);
        return;
    }
    if (!s->sunrise_preview) {
        s->sunrise_preview = true;
        s->sunrise_preview_chimed = false;
        s->brightness_fade_active = false;
    }
    s->sunrise_preview_progress = (uint16_t)progress;
    /* Its chime where a minute before the alarm falls in the real one. */
    if (!s->sunrise_preview_chimed && s->settings.night_mode.sunrise_chime && window > 60 &&
        (uint32_t)progress >= 1000U - 60U * 1000U / window) {
        s->sunrise_preview_chimed = true;
        play_sunrise_chime(core);
    }
    apply_runtime_brightness(core);
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
            int8_t idx = core->state.runtime.active_alarm_index;
            uint8_t sound = (idx >= 0 && idx < MAX_ALARMS) ? core->state.settings.alarms[idx].sound
                                                           : (uint8_t)ALARM_SOUND_MORNING_LIGHT;

            audio->start_alarm(core->state.settings.alarm_volume, sound);
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

/* The sounds the enabled alarms ring with: kept rendered (about 1 MB of PSRAM each), the rest
 * rendered only to be previewed. */
static void apply_kept_sounds(app_controller_core_t *core)
{
    const audio_service_t *audio = core->config.audio_service;
    uint8_t mask = 0;

    if (!core->state.audio_available || audio == NULL || audio->keep_sounds == NULL) {
        return;
    }
    for (int i = 0; i < MAX_ALARMS; i++) {
        const alarm_config_t *a = &core->state.settings.alarms[i];

        if (a->enabled && a->sound < ALARM_SOUND_COUNT) {
            mask |= (uint8_t)(1u << a->sound);
        }
    }
    if (!core->state.applied_sounds_known || core->state.applied_sound_mask != mask) {
        audio->keep_sounds(mask);
        core->state.applied_sound_mask = mask;
        core->state.applied_sounds_known = true;
    }
}

static void reconcile_settings_side_effects(app_controller_core_t *core, time_t now)
{
    app_state_t *s = &core->state;
    bool auto_sync = s->settings.wifi.time_sync_mode == TIME_SYNC_MODE_AUTO;

    if (core->config.clock_service != NULL && core->config.clock_service->apply_timezone != NULL &&
        (!s->applied_time_known || s->applied_timezone_id != s->settings.wifi.timezone_id)) {
        core->config.clock_service->apply_timezone(s->settings.wifi.timezone_id);
        s->applied_timezone_id = s->settings.wifi.timezone_id;
    }

    if (core->config.wifi_service != NULL && core->config.wifi_service->set_auto_sync != NULL &&
        (!s->applied_time_known || s->applied_auto_sync != auto_sync) &&
        core->config.wifi_service->set_auto_sync(auto_sync) == 0) {
        s->applied_auto_sync = auto_sync;
    }
    s->applied_time_known = true;
    apply_kept_sounds(core);

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

    /* The clock keeps its time through a restart (an update, Restart, a crash): it is set only
     * when it has none, to the last time it knew, or failing that to the build's. */
    if (core->config.clock_service != NULL && core->config.clock_service->set_epoch != NULL &&
        (core->config.clock_service->now == NULL || core->config.clock_service->now() <= CLOCK_EPOCH_VALID_MIN)) {
        if (core->state.settings.last_synced_epoch > CLOCK_EPOCH_VALID_MIN) {
            boot_epoch = core->state.settings.last_synced_epoch;
        }
        core->config.clock_service->set_epoch(boot_epoch);
    }

    alarm_scheduler_init(&core->state.runtime, &core->state.settings);
    core->state.applied_brightness = UCHAR_MAX;
    core->state.ota_next_check_ms = monotonic_ms(core) + APP_OTA_AUTO_CHECK_INITIAL_DELAY_MS;

    if (core->config.audio_service != NULL && core->config.audio_service->init != NULL) {
        if (core->config.audio_service->init(core->state.settings.alarm_volume) == 0) {
            core->state.audio_available = true;
            if (core->config.audio_service->set_ascending_enabled != NULL) {
                core->config.audio_service->set_ascending_enabled(core->state.settings.ascending_alarm_enabled);
            }
            apply_kept_sounds(core);
        }
    }

    if (core->config.wifi_service != NULL && core->config.wifi_service->init != NULL) {
        err = core->config.wifi_service->init(&core->state.settings);
        if (err != 0) {
            return err;
        }
    }
    /* Both are as the settings say now (the Wi-Fi takes its sync mode at init). */
    core->state.applied_timezone_id = core->state.settings.wifi.timezone_id;
    core->state.applied_auto_sync = core->state.settings.wifi.time_sync_mode == TIME_SYNC_MODE_AUTO;
    core->state.applied_time_known = true;

    if (core->config.ota_service != NULL && core->config.ota_service->init != NULL) {
        err = core->config.ota_service->init();
        if (err != 0) {
            return err;
        }
    }

    if (core->config.telemetry_service != NULL && core->config.telemetry_service->init != NULL) {
        err = core->config.telemetry_service->init();
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

    if (app_action_has_effect(result, APP_EFFECT_OTA_COMMAND)) {
        execute_ota_command(core, result);
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

void app_controller_core_toggle_alarm_test(app_controller_core_t *core, time_t now, uint8_t sound)
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
        audio->start_test(core->state.settings.alarm_volume, sound);
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
    if (core->config.ota_service != NULL && core->config.ota_service->snapshot != NULL) {
        core->config.ota_service->snapshot(&core->state.runtime);
    }
    maybe_request_periodic_ota_check(core);
    if (core->config.telemetry_service != NULL && core->config.telemetry_service->tick != NULL) {
        core->config.telemetry_service->tick(&core->state.runtime, &core->state.settings, now);
    }
    if (alarm_scheduler_tick(&core->state.runtime, &core->state.settings, now)) {
        mark_settings_dirty(core);
    }
    if (update_alarm_auto_stop(core, now) && core->config.ui_refresh != NULL) {
        core->config.ui_refresh(core->config.ui_refresh_ctx);
    }
    reconcile_alarm_audio(core);
    maybe_play_sunrise_chime(core, now);

    /* A checkpoint of a known time (synced, or set by hand), for a clock that loses power. */
    if ((core->state.runtime.time_synced || (core->state.settings.wifi.time_sync_mode == TIME_SYNC_MODE_MANUAL &&
                                             core->state.settings.last_synced_epoch > CLOCK_EPOCH_VALID_MIN)) &&
        now > CLOCK_EPOCH_VALID_MIN &&
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
