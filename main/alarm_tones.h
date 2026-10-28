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
