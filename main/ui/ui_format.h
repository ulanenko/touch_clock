#pragma once

/* Shared text formatting for the chrome areas (next alarm, durations, sync times). */

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "ui/clock_ui_internal.h"

/* "Today 06:30", "Tomorrow 06:30" or "Sat 06:30" for an epoch, relative to now. */
void ui_format_day_time(const clock_ui_context_t *ctx, time_t now, time_t when, char *out, size_t out_size);
/* "in 8 h 12 min", "in 12 min", "in 3 d" (localised). */
void ui_format_countdown(const clock_ui_context_t *ctx, time_t now, time_t when, char *out, size_t out_size);
/* Next enabled alarm (snooze excluded). Returns false when no alarm is scheduled. */
bool ui_next_alarm(const clock_ui_context_t *ctx, time_t now, time_t *when, int *alarm_index);
/* "14:13" for an epoch in local time. */
void ui_format_clock(time_t when, char *out, size_t out_size);
/* "Tuesday" (localised; wday 0 = Sunday). */
const char *ui_weekday_name(const clock_ui_context_t *ctx, int wday);
/* "October" as used after a day number ("28 października"), or "Oct" (mon 0 = January). */
const char *ui_month_name(const clock_ui_context_t *ctx, int mon, bool long_form);
/* "Tuesday, 28 October" (localised word order). */
void ui_format_long_date(const clock_ui_context_t *ctx, time_t when, char *out, size_t size);
