#pragma once

#include "domain/app_settings_types.h"

void settings_policy_set_defaults(app_settings_t *settings);
void settings_policy_sanitize(app_settings_t *settings);
