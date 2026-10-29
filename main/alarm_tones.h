#pragma once

/*
 * Gentle alarm tones synthesised on the device: struck instruments (wind chimes, marimba,
 * singing bowls) rendered once into a seamless loop, then played like the recorded tone.
 *
 * Plain C with no platform dependencies, so the host can render the same loops to WAV
 * (scripts/alarm_tones/render_wav.c) to check them.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ALARM_TONE_SAMPLE_RATE 44100

/* Samples mixed at a time; the mixing buffer is this many floats. */
#define ALARM_TONE_BLOCK_SAMPLES 2048

/* True for the sounds rendered here (not the recorded Morning Light). */
bool alarm_tone_is_synth(uint8_t sound);

/* Length of one loop in samples, 0 for a sound that is not synthesised. */
size_t alarm_tone_loop_samples(uint8_t sound);

/* Renders one loop of `sound` into out (alarm_tone_loop_samples() samples), mixing in block
 * (ALARM_TONE_BLOCK_SAMPLES floats, fastest in internal RAM). Tails that run past the end
 * wrap to the start, so the loop repeats without a seam. Not reentrant. Returns false for a
 * sound that is not synthesised. */
bool alarm_tone_render(uint8_t sound, int16_t *out, float *block);

/* The start chime (played once as the face first opens): its length in samples, and the
 * render into out (that many samples), as alarm_tone_render. */
size_t alarm_tone_start_samples(void);
bool alarm_tone_render_start(int16_t *out, float *block);
/* The chimes by number (0 Mallet, the start chime; 1 Dawn, the sunrise's; 2 Warm bell). */
#define ALARM_TONE_CHIME_START 0U
#define ALARM_TONE_CHIME_DAWN 1U
size_t alarm_tone_chime_samples(unsigned variant);
bool alarm_tone_render_chime(unsigned variant, int16_t *out, float *block);
/* Which start chime renders: 0 Mallet (the one used), 1 Dawn, 2 Warm bell (to listen to). */
void alarm_tone_select_start(unsigned variant);
