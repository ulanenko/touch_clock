#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

esp_err_t alarm_audio_init(uint8_t initial_volume);
void alarm_audio_set_volume(uint8_t volume);
void alarm_audio_set_ascending_enabled(bool enabled);
/* sound: alarm_sound_t. Synthesised sounds are rendered on first use. */
esp_err_t alarm_audio_start_alarm(uint8_t volume, uint8_t sound);
esp_err_t alarm_audio_start_test(uint8_t volume, uint8_t sound);
esp_err_t alarm_audio_play_ui_click(uint8_t volume);
/* The start chime, once (rendered on first use). Not over an alarm or a preview. */
esp_err_t alarm_audio_play_start_chime(uint8_t volume);
esp_err_t alarm_audio_play_sunrise_chime(uint8_t volume);
void alarm_audio_stop(void);
bool alarm_audio_is_alarm_active(void);
bool alarm_audio_is_test_active(void);
/* The sounds (bit per alarm_sound_t) the alarms use, kept rendered (about 1 MB each). The rest
 * are rendered when played and freed when they stop. Until it is called, all are kept. */
void alarm_audio_keep_sounds(uint8_t mask);
