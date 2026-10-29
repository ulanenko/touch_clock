/*
 * Renders the synthesised alarm tones to WAV files on the host, to listen to and check.
 *
 *   cc -O2 -I main scripts/alarm_tones/render_wav.c main/alarm_tones.c -lm -o /tmp/render_wav
 *   /tmp/render_wav <out-dir> [loops]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alarm_tones.h"
#include "domain/clock_types.h"

static void put_u32(FILE *f, uint32_t v)
{
    fputc((int)(v & 0xFF), f);
    fputc((int)((v >> 8) & 0xFF), f);
    fputc((int)((v >> 16) & 0xFF), f);
    fputc((int)((v >> 24) & 0xFF), f);
}

static void put_u16(FILE *f, uint16_t v)
{
    fputc(v & 0xFF, f);
    fputc((v >> 8) & 0xFF, f);
}

static int write_wav(const char *path, const int16_t *pcm, size_t n, int loops)
{
    FILE *f = fopen(path, "wb");
    uint32_t bytes = (uint32_t)(n * sizeof(int16_t) * (size_t)loops);

    if (f == NULL) {
        perror(path);
        return 1;
    }
    fwrite("RIFF", 1, 4, f);
    put_u32(f, 36 + bytes);
    fwrite("WAVEfmt ", 1, 8, f);
    put_u32(f, 16);
    put_u16(f, 1);
    put_u16(f, 1);
    put_u32(f, ALARM_TONE_SAMPLE_RATE);
    put_u32(f, ALARM_TONE_SAMPLE_RATE * 2);
    put_u16(f, 2);
    put_u16(f, 16);
    fwrite("data", 1, 4, f);
    put_u32(f, bytes);
    for (int l = 0; l < loops; l++) {
        fwrite(pcm, sizeof(int16_t), n, f);
    }
    fclose(f);
    printf("%s: %zu samples (%.1f s)\n", path, n, (double)n / ALARM_TONE_SAMPLE_RATE);
    return 0;
}

int main(int argc, char **argv)
{
    static const char *const names[ALARM_SOUND_COUNT] = {"morning_light", "wind_chimes", "marimba", "singing_bowl"};
    int loops = argc > 2 ? atoi(argv[2]) : 2;
    float block[ALARM_TONE_BLOCK_SAMPLES];
    char path[512];
    int16_t *pcm;
    size_t n;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <out-dir> [loops]\n", argv[0]);
        return 2;
    }
    for (uint8_t s = 0; s < ALARM_SOUND_COUNT; s++) {
        n = alarm_tone_loop_samples(s);
        if (n == 0) {
            continue;
        }
        pcm = malloc(n * sizeof(int16_t));
        if (pcm == NULL || !alarm_tone_render(s, pcm, block)) {
            fprintf(stderr, "render failed for %s\n", names[s]);
            return 1;
        }
        snprintf(path, sizeof(path), "%s/%s.wav", argv[1], names[s]);
        if (write_wav(path, pcm, n, loops) != 0) {
            return 1;
        }
        free(pcm);
    }
    /* The start chimes play once: the one used (Mallet) and the others to compare. */
    for (unsigned v = 0; v < 3; v++) {
        static const char *const start_names[3] = {"start_mallet", "start_dawn", "start_warm_bell"};

        alarm_tone_select_start(v);
        n = alarm_tone_start_samples();
        pcm = malloc(n * sizeof(int16_t));
        if (pcm == NULL || !alarm_tone_render_start(pcm, block)) {
            fprintf(stderr, "render failed for %s\n", start_names[v]);
            return 1;
        }
        snprintf(path, sizeof(path), "%s/%s.wav", argv[1], start_names[v]);
        if (write_wav(path, pcm, n, 1) != 0) {
            return 1;
        }
        free(pcm);
    }
    alarm_tone_select_start(0);
    return 0;
}
