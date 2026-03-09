#pragma once

#include <time.h>

#include "app_settings.h"

void alarm_logic_init(app_runtime_state_t *runtime, const app_settings_t *settings);
void alarm_logic_tick(app_runtime_state_t *runtime, const app_settings_t *settings, time_t now);
void alarm_logic_snooze(app_runtime_state_t *runtime, const app_settings_t *settings, time_t now);
void alarm_logic_stop(app_runtime_state_t *runtime);
time_t alarm_logic_find_next_alarm(const app_settings_t *settings, time_t now);
