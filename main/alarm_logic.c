#include "alarm_logic.h"

#include <string.h>

typedef struct {
    time_t epoch;
    int8_t alarm_index;
} next_alarm_info_t;

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

static bool is_skipped_occurrence(const app_settings_t *settings, int alarm_index, time_t candidate)
{
    return settings->skipped_alarm_index == alarm_index && settings->skipped_alarm_epoch == candidate;
}

static time_t find_next_alarm_for_entry(const app_settings_t *settings,
                                        const alarm_config_t *alarm,
                                        int alarm_index,
                                        time_t now)
{
    struct tm now_tm;

    localtime_r(&now, &now_tm);

    if (alarm->repeat_mode == ALARM_REPEAT_ONCE) {
        for (int day_offset = 0; day_offset < 2; ++day_offset) {
            struct tm candidate_tm = now_tm;

            candidate_tm.tm_mday += day_offset;
            candidate_tm.tm_hour = alarm->hour;
            candidate_tm.tm_min = alarm->minute;
            candidate_tm.tm_sec = 0;

            time_t candidate = mktime(&candidate_tm);
            if (candidate <= now || is_skipped_occurrence(settings, alarm_index, candidate)) {
                continue;
            }
            return candidate;
        }
        return 0;
    }

    for (int day_offset = 0; day_offset < 8; ++day_offset) {
        struct tm candidate_tm = now_tm;

        candidate_tm.tm_mday += day_offset;
        candidate_tm.tm_hour = alarm->hour;
        candidate_tm.tm_min = alarm->minute;
        candidate_tm.tm_sec = 0;

        time_t candidate = mktime(&candidate_tm);
        struct tm normalized;

        if (candidate <= now || is_skipped_occurrence(settings, alarm_index, candidate)) {
            continue;
        }

        localtime_r(&candidate, &normalized);
        if (!alarm_matches_weekday(alarm, normalized.tm_wday)) {
            continue;
        }

        return candidate;
    }

    return 0;
}

static next_alarm_info_t compute_next_alarm(const app_settings_t *settings, time_t now)
{
    next_alarm_info_t result = {
        .epoch = 0,
        .alarm_index = -1,
    };

    for (int i = 0; i < MAX_ALARMS; ++i) {
        const alarm_config_t *alarm = &settings->alarms[i];
        time_t candidate;

        if (!alarm->enabled) {
            continue;
        }

        candidate = find_next_alarm_for_entry(settings, alarm, i, now);
        if (candidate == 0) {
            continue;
        }

        if (result.epoch == 0 || candidate < result.epoch) {
            result.epoch = candidate;
            result.alarm_index = (int8_t)i;
        }
    }

    return result;
}

static int find_alarm_to_trigger_now(const app_settings_t *settings, time_t now)
{
    struct tm now_tm;

    localtime_r(&now, &now_tm);

    for (int i = 0; i < MAX_ALARMS; ++i) {
        const alarm_config_t *alarm = &settings->alarms[i];
        struct tm candidate_tm = now_tm;
        time_t candidate;

        if (!alarm->enabled) {
            continue;
        }
        if (alarm->hour != now_tm.tm_hour || alarm->minute != now_tm.tm_min) {
            continue;
        }
        if (alarm->repeat_mode == ALARM_REPEAT_WEEKLY && !alarm_matches_weekday(alarm, now_tm.tm_wday)) {
            continue;
        }

        candidate_tm.tm_sec = 0;
        candidate = mktime(&candidate_tm);
        if (is_skipped_occurrence(settings, i, candidate)) {
            continue;
        }

        return i;
    }

    return -1;
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
    runtime->next_alarm_index = -1;
    runtime->active_alarm_index = -1;
}

bool alarm_logic_tick(app_runtime_state_t *runtime, app_settings_t *settings, time_t now)
{
    struct tm now_tm;
    time_t epoch_minute = now / 60;
    bool settings_changed = false;
    next_alarm_info_t next_alarm;
    int trigger_alarm_index = -1;

    localtime_r(&now, &now_tm);
    runtime->in_night_mode = settings->night_mode.enabled &&
                             is_in_night_window(&settings->night_mode, &now_tm);

    if (settings->skipped_alarm_epoch > 0 && now > (settings->skipped_alarm_epoch + 60)) {
        settings->skipped_alarm_epoch = 0;
        settings->skipped_alarm_index = -1;
        settings_changed = true;
    }

    next_alarm = compute_next_alarm(settings, now);
    runtime->next_alarm_epoch = next_alarm.epoch;
    runtime->next_alarm_index = next_alarm.alarm_index;

    if (runtime->snooze_active && now >= runtime->snooze_deadline) {
        runtime->snooze_active = false;
        runtime->alarm_ringing = true;
    }

    if (!runtime->alarm_ringing && !runtime->snooze_active) {
        trigger_alarm_index = find_alarm_to_trigger_now(settings, now);
    }

    if (!runtime->alarm_ringing &&
        !runtime->snooze_active &&
        runtime->last_trigger_epoch_minute != epoch_minute &&
        trigger_alarm_index >= 0) {
        int alarm_index = trigger_alarm_index;

        runtime->last_trigger_epoch_minute = epoch_minute;
        runtime->alarm_ringing = true;
        runtime->active_alarm_index = (int8_t)alarm_index;

        if (settings->alarms[alarm_index].repeat_mode == ALARM_REPEAT_ONCE) {
            settings->alarms[alarm_index].enabled = false;
            settings_changed = true;
        }

        next_alarm = compute_next_alarm(settings, now);
        runtime->next_alarm_epoch = next_alarm.epoch;
        runtime->next_alarm_index = next_alarm.alarm_index;
    }

    if (!runtime->alarm_ringing && !runtime->snooze_active) {
        runtime->active_alarm_index = -1;
    }

    runtime->sunrise_active = false;
    if (!runtime->alarm_ringing &&
        !runtime->snooze_active &&
        runtime->next_alarm_epoch > now &&
        (runtime->next_alarm_epoch - now) <= 1800) {
        runtime->sunrise_active = true;
    }

    runtime->effective_brightness = compute_effective_brightness(runtime, settings, now);
    return settings_changed;
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
    runtime->active_alarm_index = -1;
}

bool alarm_logic_cancel_next_alarm(app_runtime_state_t *runtime, app_settings_t *settings, time_t now)
{
    int alarm_index = runtime->next_alarm_index;
    next_alarm_info_t next_alarm;

    if (alarm_index < 0 || alarm_index >= MAX_ALARMS || runtime->next_alarm_epoch <= now) {
        return false;
    }

    if (settings->alarms[alarm_index].repeat_mode == ALARM_REPEAT_ONCE) {
        settings->alarms[alarm_index].enabled = false;
    } else {
        settings->skipped_alarm_epoch = runtime->next_alarm_epoch;
        settings->skipped_alarm_index = (int8_t)alarm_index;
    }

    next_alarm = compute_next_alarm(settings, now);
    runtime->next_alarm_epoch = next_alarm.epoch;
    runtime->next_alarm_index = next_alarm.alarm_index;
    runtime->sunrise_active = false;
    return true;
}
