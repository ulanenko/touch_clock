#include "alarm_logic.h"

#include "domain/alarm_scheduler.h"
#include "domain/brightness_policy.h"

uint8_t alarm_logic_get_target_brightness(const app_runtime_state_t *runtime,
                                          const app_settings_t *settings,
                                          time_t now)
{
    return brightness_policy_get_target(runtime, settings, now);
}

void alarm_logic_init(app_runtime_state_t *runtime, const app_settings_t *settings)
{
    alarm_scheduler_init(runtime, settings);
}

bool alarm_logic_tick(app_runtime_state_t *runtime, app_settings_t *settings, time_t now)
{
    return alarm_scheduler_tick(runtime, settings, now);
}

void alarm_logic_snooze(app_runtime_state_t *runtime, const app_settings_t *settings, time_t now)
{
    alarm_scheduler_snooze(runtime, settings, now);
}

void alarm_logic_stop(app_runtime_state_t *runtime)
{
    alarm_scheduler_stop(runtime);
}

bool alarm_logic_cancel_next_alarm(app_runtime_state_t *runtime, app_settings_t *settings, time_t now)
{
    return alarm_scheduler_cancel_next(runtime, settings, now);
}
