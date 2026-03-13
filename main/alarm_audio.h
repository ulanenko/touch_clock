#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

esp_err_t alarm_audio_init(uint8_t initial_volume);
void alarm_audio_set_volume(uint8_t volume);
void alarm_audio_set_ascending_enabled(bool enabled);
esp_err_t alarm_audio_start_alarm(uint8_t volume);
esp_err_t alarm_audio_start_test(uint8_t volume);
void alarm_audio_stop(void);
bool alarm_audio_is_alarm_active(void);
bool alarm_audio_is_test_active(void);
