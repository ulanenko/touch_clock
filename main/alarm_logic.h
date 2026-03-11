#pragma once

#include <time.h>

#include "app_settings.h"

void alarm_logic_init(app_runtime_state_t *runtime, const app_settings_t *settings);
bool alarm_logic_tick(app_runtime_state_t *runtime, app_settings_t *settings, time_t now);
uint8_t alarm_logic_get_target_brightness(const app_runtime_state_t *runtime,
                                          const app_settings_t *settings,
                                          time_t now);
void alarm_logic_snooze(app_runtime_state_t *runtime, const app_settings_t *settings, time_t now);
void alarm_logic_stop(app_runtime_state_t *runtime);
bool alarm_logic_cancel_next_alarm(app_runtime_state_t *runtime, app_settings_t *settings, time_t now);
