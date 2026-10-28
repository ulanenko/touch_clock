#include "alarm_tones.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "domain/clock_types.h"

/*
 * Each tone is a list of strikes on an instrument. An instrument is a set of partials: a
 * frequency ratio to the struck note, a level and a decay time. Every partial is a damped
 * sine from a two-pole resonator (two multiplies per sample), so a loop renders in well
 * under a second on the device. A partial may have a slightly detuned twin; the two beat
 * against each other, which is what makes chimes and bowls shimmer.
 */

#define TONE_MAX_PARTIALS 4
#define TWO_PI 6.28318530717958647692
#define MIN_SZ(a, b) ((a) < (b) ? (a) : (b))

typedef struct {
    float ratio;   /* frequency over the struck note */
    float level;   /* relative to the fundamental */
    float decay_s; /* time to fall to 1/e */
    float twin_hz; /* detuned twin this many Hz above, 0 for none */
} tone_partial_t;

typedef struct {
    tone_partial_t partials[TONE_MAX_PARTIALS];
    float attack_s;     /* soft mallet: rise time */
    float decay_ref_hz; /* lower notes ring longer: decay scales with sqrt(ref / f) */
} tone_instrument_t;

typedef struct {
    float t_s;
    float hz;
    float velocity;
} tone_strike_t;

typedef struct {
    const tone_instrument_t *instrument;
    const tone_strike_t *strikes;
    size_t strike_count;
    float loop_s;
} tone_def_t;

/* Notes (equal temperament, A4 = 440 Hz). */
#define N_C4 261.63f
#define N_E4 329.63f
#define N_G4 392.00f
#define N_A4 440.00f
#define N_B4 493.88f
#define N_C5 523.25f
#define N_D5 587.33f
#define N_E5 659.26f
#define N_FS5 739.99f
#define N_G5 783.99f
#define N_A5 880.00f
#define N_B5 987.77f
#define N_C6 1046.50f
#define N_D6 1174.66f

/* --- Wind chimes: tuned tubes, D major pentatonic, a loose breezy cascade ---------------- */

static const tone_instrument_t s_chime = {
    .partials =
        {
            {1.000f, 1.00f, 3.2f, 0.9f},
            {2.756f, 0.42f, 1.4f, 1.7f},
            {5.404f, 0.16f, 0.6f, 0.0f},
            {8.933f, 0.05f, 0.3f, 0.0f},
        },
    .attack_s = 0.002f,
    .decay_ref_hz = N_A5,
};

static const tone_strike_t s_chime_strikes[] = {
    {0.00f, N_A5, 0.80f}, {0.42f, N_D6, 0.55f}, {0.95f, N_FS5, 0.70f}, {1.32f, N_B5, 0.50f},
    {2.10f, N_E5, 0.65f}, {2.56f, N_A5, 0.50f}, {3.40f, N_D5, 0.75f}, {4.22f, N_FS5, 0.50f},
    {4.61f, N_B5, 0.60f}, {5.05f, N_D6, 0.42f}, {6.40f, N_A5, 0.70f}, {6.86f, N_E5, 0.52f},
    {7.92f, N_D5, 0.62f}, {8.33f, N_FS5, 0.48f}, {9.70f, N_B5, 0.40f}, {10.1f, N_A5, 0.45f},
};

/* --- Marimba: warm wooden bars, a gentle two-phrase tune in C major pentatonic ----------- */

static const tone_instrument_t s_marimba = {
    .partials =
        {
            {1.000f, 1.00f, 0.60f, 0.0f},
            {3.932f, 0.30f, 0.13f, 0.0f},
            {9.170f, 0.07f, 0.05f, 0.0f},
            {2.000f, 0.06f, 0.30f, 0.0f}, /* the resonator tube's colour */
        },
    .attack_s = 0.004f,
    .decay_ref_hz = N_C5,
};

#define BEAT 0.30f
static const tone_strike_t s_marimba_strikes[] = {
    /* Call: rising, resting on A. */
    {0 * BEAT, N_C5, 0.70f}, {1 * BEAT, N_E5, 0.62f}, {2 * BEAT, N_G5, 0.68f}, {3 * BEAT, N_A5, 0.75f},
    {4 * BEAT, N_G5, 0.58f}, {6 * BEAT, N_E5, 0.66f}, {6 * BEAT, N_C5, 0.30f},
    /* Answer: falling home to C, with a low C under it. */
    {8 * BEAT, N_D5, 0.64f}, {9 * BEAT, N_E5, 0.58f}, {10 * BEAT, N_G5, 0.66f}, {11 * BEAT, N_E5, 0.55f},
    {12 * BEAT, N_D5, 0.62f}, {14 * BEAT, N_C5, 0.74f}, {14 * BEAT, N_G4, 0.36f}, {14 * BEAT, N_C4, 0.30f},
    /* A soft echo before the rest. */
    {18 * BEAT, N_G5, 0.34f}, {19 * BEAT, N_A5, 0.30f}, {20 * BEAT, N_C6, 0.36f},
};
#undef BEAT

/* --- Singing bowls: two bowls a fifth apart, struck softly in turn ----------------------- */

static const tone_instrument_t s_bowl = {
    .partials =
        {
            {1.000f, 1.00f, 8.0f, 0.8f},
            {2.710f, 0.55f, 4.5f, 1.6f},
            {5.070f, 0.24f, 2.2f, 2.4f},
            {8.210f, 0.09f, 1.1f, 3.1f},
        },
    .attack_s = 0.020f,
    .decay_ref_hz = N_E4,
};

static const tone_strike_t s_bowl_strikes[] = {
    {0.0f, N_E4, 1.00f},
    {7.0f, N_B4, 0.80f},
};

static const tone_def_t s_tones[ALARM_SOUND_COUNT] = {
    [ALARM_SOUND_WIND_CHIMES] = {&s_chime, s_chime_strikes, sizeof(s_chime_strikes) / sizeof(s_chime_strikes[0]), 13.0f},
    [ALARM_SOUND_MARIMBA] = {&s_marimba, s_marimba_strikes, sizeof(s_marimba_strikes) / sizeof(s_marimba_strikes[0]),
                             8.4f},
    [ALARM_SOUND_SINGING_BOWL] = {&s_bowl, s_bowl_strikes, sizeof(s_bowl_strikes) / sizeof(s_bowl_strikes[0]), 14.0f},
};

/* Loudness of the recorded tone (RMS about 2200 of 32767), a little gentler, with headroom
 * for the strikes' peaks. */
#define TONE_TARGET_RMS 1900.0f
#define TONE_PEAK_LIMIT 24000.0f

static const tone_def_t *tone_def(uint8_t sound)
{
    if (sound >= ALARM_SOUND_COUNT || s_tones[sound].instrument == NULL) {
        return NULL;
    }
    return &s_tones[sound];
}

bool alarm_tone_is_synth(uint8_t sound)
{
    return tone_def(sound) != NULL;
}

size_t alarm_tone_loop_samples(uint8_t sound)
{
    const tone_def_t *def = tone_def(sound);

    return def != NULL ? (size_t)(def->loop_s * (float)ALARM_TONE_SAMPLE_RATE) : 0;
}

/*
 * Rendering. Every partial of every strike is a voice: a damped sine that starts at the
 * strike and stops at -60 dB, faded over its last 30 ms so the cut cannot click. A voice
 * that runs past the end of the loop wraps to the start (at most twice round), so the loop
 * repeats without a seam.
 *
 * The loop is rendered a block at a time into a small mixing buffer (best in internal RAM):
 * every voice that sounds in the block adds itself there, then the block is scaled and
 * written out once. Each pass round the loop is a separate run of the voice, with its own
 * resonator state carried from block to block.
 */

#define TONE_MAX_VOICES 160
#define TONE_WRAPS 3 /* a voice lasts at most two loops, so it can touch one position thrice */

typedef struct {
    size_t start;  /* strike position in the loop */
    size_t length; /* samples until the cut */
    size_t attack;
    size_t fade;
    double w; /* radians per sample */
    double r; /* decay per sample */
    float level;
    float c1, c2;
    /* Resonator state per pass round the loop; valid when next_n[k] matches. */
    float y1[TONE_WRAPS], y2[TONE_WRAPS];
    size_t next_n[TONE_WRAPS];
} tone_voice_t;

static tone_voice_t s_voices[TONE_MAX_VOICES];

static size_t add_voice(size_t count, size_t loop, size_t start, double hz, float level, float decay_s, float attack_s)
{
    tone_voice_t *v;
    size_t length = (size_t)(decay_s * 6.9f * (float)ALARM_TONE_SAMPLE_RATE);

    if (count >= TONE_MAX_VOICES) {
        return count;
    }
    v = &s_voices[count];
    v->w = TWO_PI * hz / (double)ALARM_TONE_SAMPLE_RATE;
    v->r = exp(-1.0 / ((double)decay_s * (double)ALARM_TONE_SAMPLE_RATE));
    v->attack = (size_t)(attack_s * (float)ALARM_TONE_SAMPLE_RATE);
    v->fade = (size_t)(0.03f * (float)ALARM_TONE_SAMPLE_RATE);
    v->length = length > loop * 2 ? loop * 2 : length;
    if (v->w >= TWO_PI / 2.0 || v->length < v->attack + v->fade) {
        return count; /* above Nyquist, or too short to sound */
    }
    v->start = start % loop;
    v->level = level;
    v->c1 = (float)(2.0 * v->r * cos(v->w));
    v->c2 = (float)(-(v->r * v->r));
    /* The float coefficients ring at a slightly different pitch and decay; take those, so a
     * pass started part-way lines up with one that ran from the strike. */
    v->r = sqrt(-(double)v->c2);
    v->w = acos((double)v->c1 / (2.0 * v->r));
    for (int k = 0; k < TONE_WRAPS; k++) {
        v->next_n[k] = SIZE_MAX;
    }
    return count + 1;
}

/* The resonator y[n] = 2 r cos(w) y[n-1] - r^2 y[n-2], started at zero phase (no click),
 * gives y[n] = level r^(n+1) sin(w (n+1)). Sets the state to sample n directly. */
static void voice_seek(tone_voice_t *v, int k, size_t n)
{
    double rn = pow(v->r, (double)n);

    v->y1[k] = (float)((double)v->level * rn * v->r * sin(v->w * (double)(n + 1)));
    v->y2[k] = (float)((double)v->level * rn * sin(v->w * (double)n));
    v->next_n[k] = n;
}

/* Adds samples n .. n + count of one pass of a voice into mix. */
static void voice_run(tone_voice_t *v, int k, float *mix, size_t n, size_t count)
{
    size_t end = n + count;
    float y1, y2;
    const float c1 = v->c1, c2 = v->c2;

    if (v->next_n[k] != n) {
        voice_seek(v, k, n);
    }
    y1 = v->y1[k];
    y2 = v->y2[k];
    while (n < end) {
        /* A run that crosses neither the attack nor the fade: no branches inside. */
        size_t phase_end = n < v->attack ? v->attack : (n < v->length - v->fade ? v->length - v->fade : v->length);
        size_t run = MIN_SZ(phase_end, end) - n;

        if (n < v->attack || n >= v->length - v->fade) {
            float step = n < v->attack ? 1.0f / (float)v->attack : -1.0f / (float)v->fade;
            float env = n < v->attack ? (float)n / (float)v->attack : (float)(v->length - n) / (float)v->fade;

            for (size_t i = 0; i < run; i++) {
                float y = y1;

                mix[i] += env * y;
                env += step;
                y1 = c1 * y1 + c2 * y2;
                y2 = y;
            }
        } else {
            for (size_t i = 0; i < run; i++) {
                float y = y1;

                mix[i] += y;
                y1 = c1 * y1 + c2 * y2;
                y2 = y;
            }
        }
        mix += run;
        n += run;
    }
    v->y1[k] = y1;
    v->y2[k] = y2;
    v->next_n[k] = n;
}

/* Expected loudness of the loop, assuming the voices add as uncorrelated sines: a damped
 * sine of amplitude a and time constant tau samples carries a^2 tau / 4 of energy. */
static double estimate_rms(size_t voices, size_t loop)
{
    double energy = 0.0;

    for (size_t i = 0; i < voices; i++) {
        const tone_voice_t *v = &s_voices[i];
        double tau = -1.0 / log(v->r);

        energy += (double)v->level * (double)v->level * tau / 4.0 * (1.0 - pow(v->r, 2.0 * (double)v->length));
    }
    return sqrt(energy / (double)loop);
}

/* One pass over the loop, written out with `gain`. Reports the sum of squares and the peak
 * of the unscaled mix, and whether any sample had to be clipped. */
static bool render_pass(size_t voices, size_t loop, float gain, int16_t *out, float *block, double *sum_sq,
                        float *peak)
{
    bool clipped = false;

    *sum_sq = 0.0;
    *peak = 0.0f;
    for (size_t i = 0; i < voices; i++) {
        for (int k = 0; k < TONE_WRAPS; k++) {
            s_voices[i].next_n[k] = SIZE_MAX;
        }
    }
    for (size_t p0 = 0; p0 < loop; p0 += ALARM_TONE_BLOCK_SAMPLES) {
        size_t count = MIN_SZ((size_t)ALARM_TONE_BLOCK_SAMPLES, loop - p0);

        memset(block, 0, count * sizeof(float));
        for (size_t i = 0; i < voices; i++) {
            tone_voice_t *v = &s_voices[i];

            for (int k = 0; k < TONE_WRAPS; k++) {
                /* Samples of this pass that land in the block: n = p + k loop - start. */
                size_t pass_start = v->start;
                size_t lo = p0 + (size_t)k * loop;
                size_t hi = lo + count;
                size_t n_lo, n_hi;

                if (hi <= pass_start || lo >= pass_start + v->length) {
                    continue;
                }
                n_lo = lo > pass_start ? lo - pass_start : 0;
                n_hi = MIN_SZ(hi - pass_start, v->length);
                voice_run(v, k, &block[n_lo + pass_start - lo], n_lo, n_hi - n_lo);
            }
        }
        for (size_t i = 0; i < count; i++) {
            float x = block[i];
            float a = fabsf(x);
            float s = x * gain;

            *sum_sq += (double)(x * x);
            if (a > *peak) {
                *peak = a;
            }
            if (s > 32767.0f) {
                s = 32767.0f;
                clipped = true;
            } else if (s < -32768.0f) {
                s = -32768.0f;
                clipped = true;
            }
            out[p0 + i] = (int16_t)lrintf(s);
        }
    }
    return clipped;
}

bool alarm_tone_render(uint8_t sound, int16_t *out, float *block)
{
    const tone_def_t *def = tone_def(sound);
    size_t loop = alarm_tone_loop_samples(sound);
    size_t voices = 0;
    double sum_sq;
    float peak;
    float gain;
    float want;

    if (def == NULL || out == NULL || block == NULL || loop == 0) {
        return false;
    }
    for (size_t s = 0; s < def->strike_count; s++) {
        const tone_strike_t *k = &def->strikes[s];
        size_t start = (size_t)(k->t_s * (float)ALARM_TONE_SAMPLE_RATE);
        float decay_scale = sqrtf(def->instrument->decay_ref_hz / k->hz);

        for (int p = 0; p < TONE_MAX_PARTIALS; p++) {
            const tone_partial_t *part = &def->instrument->partials[p];
            float level = k->velocity * part->level;
            float decay = part->decay_s * decay_scale;
            float attack = def->instrument->attack_s;

            if (part->level <= 0.0f) {
                continue;
            }
            if (part->twin_hz > 0.0f) {
                /* The pair shares the level, so the beat swells and fades without adding up. */
                voices = add_voice(voices, loop, start, (double)(k->hz * part->ratio), level * 0.6f, decay, attack);
                voices = add_voice(voices, loop, start, (double)(k->hz * part->ratio + part->twin_hz), level * 0.4f,
                                   decay, attack);
            } else {
                voices = add_voice(voices, loop, start, (double)(k->hz * part->ratio), level, decay, attack);
            }
        }
    }
    if (voices == 0) {
        return false;
    }

    /* Render once at the estimated gain, then trim to the exact loudness in place. Only a
     * badly low estimate (a clipped sample) costs a second pass. */
    gain = TONE_TARGET_RMS / (float)estimate_rms(voices, loop);
    if (render_pass(voices, loop, gain, out, block, &sum_sq, &peak)) {
        gain = 0.0f;
    }
    if (peak <= 0.0f) {
        return false;
    }
    want = TONE_TARGET_RMS / (float)sqrt(sum_sq / (double)loop);
    if (want * peak > TONE_PEAK_LIMIT) {
        want = TONE_PEAK_LIMIT / peak;
    }
    if (gain == 0.0f) {
        render_pass(voices, loop, want, out, block, &sum_sq, &peak);
    } else if (fabsf(want / gain - 1.0f) > 0.001f) {
        float trim = want / gain;

        for (size_t i = 0; i < loop; i++) {
            out[i] = (int16_t)lrintf((float)out[i] * trim);
        }
    }
    return true;
}
