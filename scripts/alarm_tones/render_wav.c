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

int main(int argc, char **argv)
{
    static const char *const names[ALARM_SOUND_COUNT] = {"morning_light", "wind_chimes", "marimba", "singing_bowl"};
    int loops = argc > 2 ? atoi(argv[2]) : 2;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <out-dir> [loops]\n", argv[0]);
        return 2;
    }
    for (uint8_t s = 0; s < ALARM_SOUND_COUNT; s++) {
        size_t n = alarm_tone_loop_samples(s);
        int16_t *pcm;
        float *scratch;
        char path[512];
        FILE *f;
        uint32_t bytes;

        if (n == 0) {
            continue;
        }
        pcm = malloc(n * sizeof(int16_t));
        scratch = malloc(n * sizeof(float));
        if (pcm == NULL || scratch == NULL || !alarm_tone_render(s, pcm, scratch)) {
            fprintf(stderr, "render failed for %s\n", names[s]);
            return 1;
        }
        snprintf(path, sizeof(path), "%s/%s.wav", argv[1], names[s]);
        f = fopen(path, "wb");
        if (f == NULL) {
            perror(path);
            return 1;
        }
        bytes = (uint32_t)(n * sizeof(int16_t) * (size_t)loops);
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
        free(pcm);
        free(scratch);
    }
    return 0;
}
