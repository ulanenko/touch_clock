#include "alarm_logic.h"

#include <string.h>

static bool is_in_night_window(const night_mode_config_t *cfg, const struct tm *local_tm)
{
    int current = local_tm->tm_hour * 60 + local_tm->tm_min;
    int start = cfg->start_hour * 60 + cfg->start_minute;
    int end = cfg->end_hour * 60 + cfg->end_minute;

    if (start == end) {
        return true;
    }
    if (start < end) {
        return current >= start && current < end;
    }
    return current >= start || current < end;
}

static bool alarm_matches_weekday(const alarm_config_t *alarm, int tm_wday)
{
    uint8_t mask = (uint8_t)(1U << tm_wday);
    return (alarm->days_mask & mask) != 0;
}

time_t alarm_logic_find_next_alarm(const app_settings_t *settings, time_t now)
{
    struct tm now_tm;
    time_t best = 0;

    localtime_r(&now, &now_tm);

    for (size_t i = 0; i < MAX_ALARMS; ++i) {
        const alarm_config_t *alarm = &settings->alarms[i];

        if (!alarm->enabled) {
            continue;
        }

        for (int day_offset = 0; day_offset < 8; ++day_offset) {
            struct tm candidate_tm = now_tm;
            candidate_tm.tm_mday += day_offset;
            candidate_tm.tm_hour = alarm->hour;
            candidate_tm.tm_min = alarm->minute;
            candidate_tm.tm_sec = 0;

            time_t candidate = mktime(&candidate_tm);
            if (candidate <= now) {
                continue;
            }

            struct tm normalized;
            localtime_r(&candidate, &normalized);
            if (!alarm_matches_weekday(alarm, normalized.tm_wday)) {
                continue;
            }

            if (best == 0 || candidate < best) {
                best = candidate;
            }
            break;
        }
    }

    return best;
}

static bool should_trigger_alarm_now(const app_settings_t *settings, time_t now)
{
    struct tm now_tm;
    localtime_r(&now, &now_tm);

    for (size_t i = 0; i < MAX_ALARMS; ++i) {
        const alarm_config_t *alarm = &settings->alarms[i];

        if (!alarm->enabled) {
            continue;
        }
        if (!alarm_matches_weekday(alarm, now_tm.tm_wday)) {
            continue;
        }
        if (alarm->hour == now_tm.tm_hour && alarm->minute == now_tm.tm_min) {
            return true;
        }
    }

    return false;
}

static uint8_t compute_effective_brightness(app_runtime_state_t *runtime,
                                            const app_settings_t *settings,
                                            time_t now)
{
    if (runtime->alarm_ringing) {
        return settings->base_brightness;
    }

    if (runtime->sunrise_active && runtime->next_alarm_epoch > now) {
        int64_t seconds_left = (int64_t)(runtime->next_alarm_epoch - now);
        int64_t progress = 1800 - seconds_left;
        int minimum = settings->night_mode.enabled ? settings->night_mode.brightness : DISPLAY_BRIGHTNESS_MIN_PERCENT;
        int span;

        if (minimum < DISPLAY_BRIGHTNESS_MIN_PERCENT) {
            minimum = DISPLAY_BRIGHTNESS_MIN_PERCENT;
        }
        if (progress < 0) {
            progress = 0;
        }
        if (progress > 1800) {
            progress = 1800;
        }

        span = (int)settings->base_brightness - minimum;
        return (uint8_t)(minimum + ((progress * span) / 1800));
    }

    if (runtime->in_night_mode) {
        return settings->night_mode.brightness;
    }

    return settings->base_brightness;
}

void alarm_logic_init(app_runtime_state_t *runtime, const app_settings_t *settings)
{
    memset(runtime, 0, sizeof(*runtime));
    runtime->effective_brightness = settings->base_brightness;
}

void alarm_logic_tick(app_runtime_state_t *runtime, const app_settings_t *settings, time_t now)
{
    struct tm now_tm;
    time_t epoch_minute = now / 60;

    localtime_r(&now, &now_tm);
    runtime->next_alarm_epoch = alarm_logic_find_next_alarm(settings, now);
    runtime->in_night_mode = settings->night_mode.enabled &&
                             is_in_night_window(&settings->night_mode, &now_tm);

    if (runtime->snooze_active && now >= runtime->snooze_deadline) {
        runtime->snooze_active = false;
        runtime->alarm_ringing = true;
    }

    if (!runtime->alarm_ringing &&
        runtime->last_trigger_epoch_minute != epoch_minute &&
        should_trigger_alarm_now(settings, now)) {
        runtime->last_trigger_epoch_minute = epoch_minute;
        runtime->alarm_ringing = true;
        runtime->snooze_active = false;
    }

    runtime->sunrise_active = false;
    if (!runtime->alarm_ringing &&
        !runtime->snooze_active &&
        runtime->next_alarm_epoch > now &&
        (runtime->next_alarm_epoch - now) <= 1800) {
        runtime->sunrise_active = true;
    }

    runtime->effective_brightness = compute_effective_brightness(runtime, settings, now);
}

void alarm_logic_snooze(app_runtime_state_t *runtime, const app_settings_t *settings, time_t now)
{
    runtime->alarm_ringing = false;
    runtime->snooze_active = true;
    runtime->sunrise_active = false;
    runtime->snooze_deadline = now + settings->snooze_minutes * 60;
}

void alarm_logic_stop(app_runtime_state_t *runtime)
{
    runtime->alarm_ringing = false;
    runtime->snooze_active = false;
    runtime->sunrise_active = false;
    runtime->snooze_deadline = 0;
}
