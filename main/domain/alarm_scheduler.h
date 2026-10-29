#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "domain/app_settings_types.h"

void alarm_scheduler_init(app_runtime_state_t *runtime, const app_settings_t *settings);
bool alarm_scheduler_tick(app_runtime_state_t *runtime, app_settings_t *settings, time_t now);
void alarm_scheduler_snooze(app_runtime_state_t *runtime, const app_settings_t *settings, time_t now);
void alarm_scheduler_stop(app_runtime_state_t *runtime);
bool alarm_scheduler_cancel_next(app_runtime_state_t *runtime, app_settings_t *settings, time_t now);
/* The sunrise before an alarm: its length (s), and how far through it now is (0..1000). */
uint32_t alarm_scheduler_sunrise_seconds(const app_settings_t *settings);
uint32_t alarm_scheduler_sunrise_progress(const app_runtime_state_t *runtime, const app_settings_t *settings, time_t now);
