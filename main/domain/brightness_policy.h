#pragma once

#include <stdint.h>
#include <time.h>

#include "domain/app_settings_types.h"

uint8_t brightness_policy_get_target(const app_runtime_state_t *runtime,
                                     const app_settings_t *settings,
                                     time_t now);
/* The sunrise's backlight (per mille of full) p (0..1000) of the way through it. */
uint16_t brightness_policy_sunrise_permille(const app_runtime_state_t *runtime,
                                           const app_settings_t *settings,
                                           uint32_t p);
/* The same, finer (per mille of full): the sunrise rises in steps a percent would show. */
uint16_t brightness_policy_get_target_permille(const app_runtime_state_t *runtime,
                                               const app_settings_t *settings,
                                               time_t now);
uint8_t brightness_policy_ui_to_hw(uint8_t ui_percent);
uint8_t brightness_policy_hw_to_ui(uint8_t hw_percent);
