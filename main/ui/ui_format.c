#include "ui/ui_format.h"

#include <stdio.h>

#include "ui/clock_ui_private.h"

static const ui_l10n_str_t s_today = UI_L10N_STR("Today", "Dziś", "Сьогодні", "Сегодня", "Vandaag");
static const ui_l10n_str_t s_tomorrow = UI_L10N_STR("Tomorrow", "Jutro", "Завтра", "Завтра", "Morgen");
static const ui_l10n_str_t s_in_hm = UI_L10N_STR("in %d h %d min", "za %d h %d min", "через %d год %d хв",
                                                 "через %d ч %d мин", "over %d u %d min");
static const ui_l10n_str_t s_in_m = UI_L10N_STR("in %d min", "za %d min", "через %d хв", "через %d мин",
                                                "over %d min");
static const ui_l10n_str_t s_in_d = UI_L10N_STR("in %d d", "za %d dni", "через %d дн", "через %d дн",
                                                "over %d d");

static int local_day_index(time_t t)
{
    struct tm tm;

    localtime_r(&t, &tm);
    return tm.tm_year * 400 + tm.tm_yday;
}

void ui_format_clock(time_t when, char *out, size_t out_size)
{
    struct tm tm;

    localtime_r(&when, &tm);
    snprintf(out, out_size, "%02d:%02d", tm.tm_hour, tm.tm_min);
}

void ui_format_day_time(const clock_ui_context_t *ctx, time_t now, time_t when, char *out, size_t out_size)
{
    char clock[8];
    int days = local_day_index(when) - local_day_index(now);
    struct tm tm;

    ui_format_clock(when, clock, sizeof(clock));
    localtime_r(&when, &tm);
    if (days == 0) {
        snprintf(out, out_size, "%s %s", ui_l10n_str(ctx, &s_today), clock);
    } else if (days == 1) {
        snprintf(out, out_size, "%s %s", ui_l10n_str(ctx, &s_tomorrow), clock);
    } else {
        snprintf(out, out_size, "%s %s",
                 l10n_day_short((clock_language_t)ctx->settings->language, tm.tm_wday), clock);
    }
}

void ui_format_countdown(const clock_ui_context_t *ctx, time_t now, time_t when, char *out, size_t out_size)
{
    long minutes = when > now ? (long)((when - now + 59) / 60) : 0;

    if (minutes >= 48 * 60) {
        snprintf(out, out_size, ui_l10n_str(ctx, &s_in_d), (int)(minutes / (24 * 60)));
    } else if (minutes >= 60) {
        snprintf(out, out_size, ui_l10n_str(ctx, &s_in_hm), (int)(minutes / 60), (int)(minutes % 60));
    } else {
        snprintf(out, out_size, ui_l10n_str(ctx, &s_in_m), (int)(minutes < 1 ? 1 : minutes));
    }
}

bool ui_next_alarm(const clock_ui_context_t *ctx, time_t now, time_t *when, int *alarm_index)
{
    const app_runtime_state_t *runtime = ctx->runtime;

    if (runtime == NULL || runtime->next_alarm_index < 0 || runtime->next_alarm_epoch <= now) {
        return false;
    }
    if (when != NULL) {
        *when = runtime->next_alarm_epoch;
    }
    if (alarm_index != NULL) {
        *alarm_index = runtime->next_alarm_index;
    }
    return true;
}
