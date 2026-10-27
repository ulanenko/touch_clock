#include "alarm_tones.h"

#include <math.h>
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

/* Adds one damped sine to the loop, wrapping past the end. It stops at -60 dB, faded over
 * the last 30 ms so the cut cannot click; overlapping tails wrap at most twice round. */
static void add_partial(float *mix, size_t loop, size_t start, double hz, float level, float decay_s, float attack_s)
{
    double w = TWO_PI * hz / (double)ALARM_TONE_SAMPLE_RATE;
    double r = exp(-1.0 / ((double)decay_s * (double)ALARM_TONE_SAMPLE_RATE));
    /* Resonator y[n] = 2 r cos(w) y[n-1] - r^2 y[n-2], started at zero phase so it
     * begins without a click. */
    float c1 = (float)(2.0 * r * cos(w));
    float c2 = (float)(-(r * r));
    float y1 = (float)(sin(w) * r) * level;
    float y2 = 0.0f;
    size_t attack = (size_t)(attack_s * (float)ALARM_TONE_SAMPLE_RATE);
    size_t fade = (size_t)(0.03f * (float)ALARM_TONE_SAMPLE_RATE);
    size_t length = (size_t)(decay_s * 6.9f * (float)ALARM_TONE_SAMPLE_RATE);
    size_t pos = start % loop;
    size_t n = 0;

    if (w >= TWO_PI / 2.0) {
        return; /* above Nyquist */
    }
    if (length > loop * 2) {
        length = loop * 2;
    }
    if (length < attack + fade) {
        return;
    }
    while (n < length) {
        /* A run that neither wraps nor crosses the attack or the fade: no branches inside. */
        size_t phase_end = n < attack ? attack : (n < length - fade ? length - fade : length);
        size_t run = MIN_SZ(phase_end - n, loop - pos);
        float *out = &mix[pos];

        if (n < attack || n >= length - fade) {
            float step = n < attack ? 1.0f / (float)attack : -1.0f / (float)fade;
            float env = n < attack ? (float)n / (float)attack : (float)(length - n) / (float)fade;

            for (size_t i = 0; i < run; i++) {
                float y = y1;

                out[i] += env * y;
                env += step;
                y1 = c1 * y1 + c2 * y2;
                y2 = y;
            }
        } else {
            for (size_t i = 0; i < run; i++) {
                float y = y1;

                out[i] += y;
                y1 = c1 * y1 + c2 * y2;
                y2 = y;
            }
        }
        n += run;
        pos += run;
        if (pos == loop) {
            pos = 0;
        }
    }
}

bool alarm_tone_render(uint8_t sound, int16_t *out, float *scratch)
{
    const tone_def_t *def = tone_def(sound);
    size_t loop = alarm_tone_loop_samples(sound);
    double sum_sq = 0.0;
    float peak = 0.0f;
    float gain;

    if (def == NULL || out == NULL || scratch == NULL || loop == 0) {
        return false;
    }
    memset(scratch, 0, loop * sizeof(float));
    for (size_t s = 0; s < def->strike_count; s++) {
        const tone_strike_t *k = &def->strikes[s];
        size_t start = (size_t)(k->t_s * (float)ALARM_TONE_SAMPLE_RATE);
        float decay_scale = sqrtf(def->instrument->decay_ref_hz / k->hz);

        for (int p = 0; p < TONE_MAX_PARTIALS; p++) {
            const tone_partial_t *part = &def->instrument->partials[p];
            float level = k->velocity * part->level;
            float decay = part->decay_s * decay_scale;

            if (part->level <= 0.0f) {
                continue;
            }
            if (part->twin_hz > 0.0f) {
                /* The pair shares the level, so the beat swells and fades without adding up. */
                add_partial(scratch, loop, start, (double)(k->hz * part->ratio), level * 0.6f, decay,
                            def->instrument->attack_s);
                add_partial(scratch, loop, start, (double)(k->hz * part->ratio + part->twin_hz), level * 0.4f, decay,
                            def->instrument->attack_s);
            } else {
                add_partial(scratch, loop, start, (double)(k->hz * part->ratio), level, decay, def->instrument->attack_s);
            }
        }
    }
    for (size_t i = 0; i < loop; i++) {
        float v = fabsf(scratch[i]);

        sum_sq += (double)scratch[i] * (double)scratch[i];
        if (v > peak) {
            peak = v;
        }
    }
    if (peak <= 0.0f) {
        return false;
    }
    gain = TONE_TARGET_RMS / (float)sqrt(sum_sq / (double)loop);
    if (gain * peak > TONE_PEAK_LIMIT) {
        gain = TONE_PEAK_LIMIT / peak;
    }
    for (size_t i = 0; i < loop; i++) {
        out[i] = (int16_t)lrintf(scratch[i] * gain);
    }
    return true;
}
