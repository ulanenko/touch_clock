#include "domain/alarm_scheduler.h"

#include <string.h>

#include "domain/brightness_policy.h"

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
            time_t candidate;

            candidate_tm.tm_mday += day_offset;
            candidate_tm.tm_hour = alarm->hour;
            candidate_tm.tm_min = alarm->minute;
            candidate_tm.tm_sec = 0;
            candidate_tm.tm_isdst = -1; /* that day's, not today's: a DST change between */

            candidate = mktime(&candidate_tm);
            if (candidate <= now || is_skipped_occurrence(settings, alarm_index, candidate)) {
                continue;
            }
            return candidate;
        }

        return 0;
    }

    for (int day_offset = 0; day_offset < 15; ++day_offset) {
        struct tm candidate_tm = now_tm;
        struct tm normalized;
        time_t candidate;

        candidate_tm.tm_mday += day_offset;
        candidate_tm.tm_hour = alarm->hour;
        candidate_tm.tm_min = alarm->minute;
        candidate_tm.tm_sec = 0;
        candidate_tm.tm_isdst = -1; /* that day's, not today's: a DST change between */

        candidate = mktime(&candidate_tm);
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

/* The alarm due now: an occurrence in the last minute. Occurrences are times, so a DST change
 * neither moves one nor loses it (an alarm in the hour the clocks skip rings an hour on), and
 * a tick late by under a minute still finds it. */
static next_alarm_info_t find_alarm_due(const app_settings_t *settings, time_t now)
{
    next_alarm_info_t due = compute_next_alarm(settings, now - 60);

    if (due.epoch == 0 || due.epoch > now) {
        due.epoch = 0;
        due.alarm_index = -1;
    }
    return due;
}

void alarm_scheduler_init(app_runtime_state_t *runtime, const app_settings_t *settings)
{
    memset(runtime, 0, sizeof(*runtime));
    runtime->effective_brightness = settings->base_brightness;
    runtime->night_brightness_override = settings->night_mode.brightness;
    runtime->next_alarm_index = -1;
    runtime->active_alarm_index = -1;
}

bool alarm_scheduler_tick(app_runtime_state_t *runtime, app_settings_t *settings, time_t now)
{
    struct tm now_tm;
    bool settings_changed = false;
    bool was_in_night_mode = runtime->in_night_mode;
    next_alarm_info_t next_alarm;
    next_alarm_info_t due = {.epoch = 0, .alarm_index = -1};

    localtime_r(&now, &now_tm);
    if (runtime->night_now != 0 && now >= runtime->night_now_until) {
        runtime->night_now = 0;
    }
    if (runtime->night_now != 0) {
        runtime->in_night_mode = runtime->night_now > 0;
    } else {
        runtime->in_night_mode = settings->night_mode.enabled && is_in_night_window(&settings->night_mode, &now_tm);
    }
    if (!runtime->in_night_mode && was_in_night_mode) {
        runtime->night_brightness_override_active = false;
        runtime->night_brightness_override = settings->night_mode.brightness;
    }

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
        due = find_alarm_due(settings, now);
    }

    /* Once per occurrence: stopped within its minute, it does not ring again. */
    if (due.alarm_index >= 0 && runtime->last_trigger_epoch_minute != due.epoch / 60) {
        int alarm_index = due.alarm_index;

        runtime->last_trigger_epoch_minute = due.epoch / 60;
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

    /* The sunrise: the last minutes before an alarm, when it is on and the night is on. */
    runtime->sunrise_active = false;
    if (!runtime->alarm_ringing && !runtime->snooze_active && settings->night_mode.enabled &&
        settings->night_mode.sunrise_brightness_enabled && runtime->next_alarm_epoch > now &&
        (runtime->next_alarm_epoch - now) <= alarm_scheduler_sunrise_seconds(settings)) {
        runtime->sunrise_active = true;
        if (runtime->sunrise_alarm_epoch != runtime->next_alarm_epoch) {
            /* It lights the screen only if it begins in the night. */
            runtime->sunrise_alarm_epoch = runtime->next_alarm_epoch;
            runtime->sunrise_from_night = runtime->in_night_mode;
        }
        /* Halfway, the night face gives way to the day face, and the day holds until the night
         * window ends (the quick Night key's "day now"), so the alarm does not wake to night. */
        if (runtime->sunrise_from_night && runtime->in_night_mode && runtime->night_now == 0 &&
            alarm_scheduler_sunrise_progress(runtime, settings, now) >= 500) {
            struct tm until_tm = now_tm;
            time_t until;

            until_tm.tm_hour = settings->night_mode.end_hour;
            until_tm.tm_min = settings->night_mode.end_minute;
            until_tm.tm_sec = 0;
            until_tm.tm_isdst = -1;
            until = mktime(&until_tm);
            if (until <= now) {
                until_tm.tm_mday += 1;
                until_tm.tm_isdst = -1;
                until = mktime(&until_tm);
            }
            runtime->night_now = -1;
            runtime->night_now_until = until;
            runtime->in_night_mode = false;
        }
    }

    runtime->effective_brightness = brightness_policy_get_target(runtime, settings, now);
    return settings_changed;
}

uint32_t alarm_scheduler_sunrise_seconds(const app_settings_t *settings)
{
    return (uint32_t)settings->night_mode.sunrise_minutes * 60U;
}

uint32_t alarm_scheduler_sunrise_progress(const app_runtime_state_t *runtime, const app_settings_t *settings, time_t now)
{
    uint32_t window = alarm_scheduler_sunrise_seconds(settings);
    int64_t left = (int64_t)runtime->next_alarm_epoch - (int64_t)now;

    if (window == 0 || left >= (int64_t)window) {
        return 0;
    }
    if (left <= 0) {
        return 1000;
    }
    return (uint32_t)(((int64_t)window - left) * 1000 / (int64_t)window);
}

void alarm_scheduler_snooze(app_runtime_state_t *runtime, const app_settings_t *settings, time_t now)
{
    runtime->alarm_ringing = false;
    runtime->snooze_active = true;
    runtime->sunrise_active = false;
    runtime->snooze_deadline = now + settings->snooze_minutes * 60;
}

void alarm_scheduler_stop(app_runtime_state_t *runtime)
{
    runtime->alarm_ringing = false;
    runtime->snooze_active = false;
    runtime->sunrise_active = false;
    runtime->snooze_deadline = 0;
    runtime->active_alarm_index = -1;
}

bool alarm_scheduler_cancel_next(app_runtime_state_t *runtime, app_settings_t *settings, time_t now)
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
