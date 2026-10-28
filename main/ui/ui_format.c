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

static const ui_l10n_str_t s_month_short[12] = {
    UI_L10N_STR("Jan", "sty", "січ.", "янв.", "jan"), UI_L10N_STR("Feb", "lut", "лют.", "февр.", "feb"),
    UI_L10N_STR("Mar", "mar", "бер.", "мар.", "mrt"), UI_L10N_STR("Apr", "kwi", "квіт.", "апр.", "apr"),
    UI_L10N_STR("May", "maj", "трав.", "мая", "mei"), UI_L10N_STR("Jun", "cze", "черв.", "июн.", "jun"),
    UI_L10N_STR("Jul", "lip", "лип.", "июл.", "jul"), UI_L10N_STR("Aug", "sie", "серп.", "авг.", "aug"),
    UI_L10N_STR("Sep", "wrz", "вер.", "сент.", "sep"), UI_L10N_STR("Oct", "paź", "жовт.", "окт.", "okt"),
    UI_L10N_STR("Nov", "lis", "лист.", "нояб.", "nov"), UI_L10N_STR("Dec", "gru", "груд.", "дек.", "dec"),
};

/* Month names as used after a day number ("28 October", "28 października"). */
static const ui_l10n_str_t s_month_long[12] = {
    UI_L10N_STR("January", "stycznia", "січня", "января", "januari"),
    UI_L10N_STR("February", "lutego", "лютого", "февраля", "februari"),
    UI_L10N_STR("March", "marca", "березня", "марта", "maart"),
    UI_L10N_STR("April", "kwietnia", "квітня", "апреля", "april"),
    UI_L10N_STR("May", "maja", "травня", "мая", "mei"),
    UI_L10N_STR("June", "czerwca", "червня", "июня", "juni"),
    UI_L10N_STR("July", "lipca", "липня", "июля", "juli"),
    UI_L10N_STR("August", "sierpnia", "серпня", "августа", "augustus"),
    UI_L10N_STR("September", "września", "вересня", "сентября", "september"),
    UI_L10N_STR("October", "października", "жовтня", "октября", "oktober"),
    UI_L10N_STR("November", "listopada", "листопада", "ноября", "november"),
    UI_L10N_STR("December", "grudnia", "грудня", "декабря", "december"),
};

static const ui_l10n_str_t s_weekday_long[7] = {
    UI_L10N_STR("Sunday", "Niedziela", "Неділя", "Воскресенье", "Zondag"),
    UI_L10N_STR("Monday", "Poniedziałek", "Понеділок", "Понедельник", "Maandag"),
    UI_L10N_STR("Tuesday", "Wtorek", "Вівторок", "Вторник", "Dinsdag"),
    UI_L10N_STR("Wednesday", "Środa", "Середа", "Среда", "Woensdag"),
    UI_L10N_STR("Thursday", "Czwartek", "Четвер", "Четверг", "Donderdag"),
    UI_L10N_STR("Friday", "Piątek", "П'ятниця", "Пятница", "Vrijdag"),
    UI_L10N_STR("Saturday", "Sobota", "Субота", "Суббота", "Zaterdag"),
};

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

const char *ui_weekday_name(const clock_ui_context_t *ctx, int wday)
{
    return ui_l10n_str(ctx, &s_weekday_long[((wday % 7) + 7) % 7]);
}

const char *ui_month_name(const clock_ui_context_t *ctx, int mon, bool long_form)
{
    int m = ((mon % 12) + 12) % 12;

    return ui_l10n_str(ctx, long_form ? &s_month_long[m] : &s_month_short[m]);
}

void ui_format_long_date(const clock_ui_context_t *ctx, time_t when, char *out, size_t size)
{
    struct tm tm;
    bool dutch = ctx->settings->language == CLOCK_LANGUAGE_DUTCH;

    localtime_r(&when, &tm);
    snprintf(out, size, dutch ? "%s %d %s" : "%s, %d %s", ui_weekday_name(ctx, tm.tm_wday), tm.tm_mday,
             ui_month_name(ctx, tm.tm_mon, true));
}
