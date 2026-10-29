/*
 * Liquid Glass: the backdrop (frost and dim), the glass material, refracted glass arcs, and
 * the hook that draws chrome objects as glass. See ui_glass.h.
 *
 * Cost: building the backdrop reads the face once and writes two full-screen images (when a
 * glass surface comes up). A glass shape on screen is one draw task: its baked RGB565 image,
 * corners clipped. Baking runs per pixel once per shape and backdrop; only the bezel (the
 * outer few px) samples bilinearly.
 */
#include "ui/kit/ui_glass.h"

#include <math.h>
#include <string.h>

#include "esp_heap_caps.h"

#if defined(ESP_PLATFORM)
#include "sdkconfig.h"
#endif
#if CONFIG_TOUCH_CLOCK_PERF_TOUR
#include <stdio.h>
#include "esp_timer.h"
#define GLASS_PERF_START() int64_t perf_t0 = esp_timer_get_time()
#define GLASS_PERF_STEP(what) printf("PERF glass_step %s us=%d\n", what, (int)(esp_timer_get_time() - perf_t0))
#define GLASS_PERF_END(what, w, h)                                                                                      \
    printf("PERF glass_%s w=%d h=%d us=%d\n", what, (int)(w), (int)(h), (int)(esp_timer_get_time() - perf_t0))
#else
#define GLASS_PERF_START()
#define GLASS_PERF_STEP(what)
#define GLASS_PERF_END(what, w, h)
#endif

#include "src/misc/lv_area_private.h"
#include "ui/kit/ui_skin.h"

/* From ui_ppa_blend.c (clock_ui_private.h). */
void ui_sparse_image_register(const lv_draw_buf_t *buf);
void ui_sparse_image_unregister(const lv_draw_buf_t *buf);

#define SCREEN 720
#define LOW 180   /* the blur works at a quarter of the resolution */
#define FROST 360 /* and is kept at half: it is soft enough */
#define DEEP 90   /* sheets and large panels: blurred again at an eighth (about 30 px at full size) */
#define VIEWS 256
#define COVER_MIN 24 /* the least coverage a blurred colour is divided by */
#define PAGE_GROUND 80 /* a page's ground: the frost at 80 / 256 */

/* --- Backdrop ---------------------------------------------------------------------------- */

static lv_draw_buf_t *s_frost;      /* the backdrop blurred and graded, at half resolution (FROST) */
static lv_draw_buf_t *s_back;       /* what the glass refracts: the face (FACE), the dimmed face (PAGE) */
static const lv_draw_buf_t *s_sharp; /* the face the backdrop was built from (the screen scratch) */
static float s_ground_fade = 1.0f;   /* 0 the sharp face, 1 the dimmed ground */
static uint8_t *s_deep;             /* the deep frost: DEEP x DEEP, 8 bit RGB, graded like the frost */
static bool s_deep_ok;
static bool s_use_deep;             /* while baking a sheet or a large panel */
static uint16_t s_deep_rows[4][FROST]; /* deep frost rows at the frost's size, as they are asked for */
static int16_t s_deep_row_y[4] = {-1, -1, -1, -1};
static bool s_frost_ok;
static bool s_back_ok;
static bool s_sharp_ok;
static bool s_light;
static ui_glass_mode_t s_mode;
static uint32_t s_generation; /* counts changes to what the glass refracts over the face */
static uint32_t s_identity;   /* which face (and theme) the backdrop shows */

static uint32_t s_unweight[256]; /* c * 255 / max(w, COVER_MIN) as (c * s_unweight[w]) >> 16, exact */
static const uint8_t s_bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static lv_draw_buf_t *screen_buf(lv_draw_buf_t **buf)
{
    if (*buf == NULL) {
        *buf = lv_draw_buf_create(SCREEN, SCREEN, LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
    }
    return *buf;
}

/* Three box passes each way over an n x n RGBA image, in place: about a Gaussian of radius
 * 1.7 r. Rows run along each row with a sum per channel; columns run down the image a row at a
 * time with a sum per byte of the row, so both read memory in order, and the edges (clamped)
 * come from index tables rather than tests in the loops. False without memory. */
static bool blur_rgba(uint8_t *img, int n, int r)
{
    const int recip = (65536 + 2 * r) / (2 * r + 1); /* sum / (2r + 1) as a multiply */
    const int stride = n * 4;
    uint8_t *tmp = lv_malloc((size_t)n * (size_t)stride);
    uint16_t *sums = lv_malloc((size_t)stride * sizeof(uint16_t));
    int16_t *add = lv_malloc((size_t)n * 2 * sizeof(int16_t));
    int16_t *sub = add + n;

    if (tmp == NULL || sums == NULL || add == NULL) {
        lv_free(tmp);
        lv_free(sums);
        lv_free(add);
        return false;
    }
    for (int i = 0; i < n; i++) {
        add[i] = (int16_t)LV_MIN(i + r + 1, n - 1); /* the sample entering the sum */
        sub[i] = (int16_t)LV_MAX(i - r, 0);         /* and the one leaving it */
    }
    for (int pass = 0; pass < 3; pass++) {
        for (int y = 0; y < n; y++) {
            const uint8_t *in = img + (size_t)y * stride;
            uint8_t *out = tmp + (size_t)y * stride;
            int s0 = 0;
            int s1 = 0;
            int s2 = 0;
            int s3 = 0;

            for (int i = -r; i <= r; i++) {
                const uint8_t *p = in + LV_CLAMP(0, i, n - 1) * 4;

                s0 += p[0];
                s1 += p[1];
                s2 += p[2];
                s3 += p[3];
            }
            for (int x = 0; x < n; x++) {
                const uint8_t *pa = in + add[x] * 4;
                const uint8_t *pb = in + sub[x] * 4;

                out[0] = (uint8_t)((s0 * recip) >> 16);
                out[1] = (uint8_t)((s1 * recip) >> 16);
                out[2] = (uint8_t)((s2 * recip) >> 16);
                out[3] = (uint8_t)((s3 * recip) >> 16);
                out += 4;
                s0 += pa[0] - pb[0];
                s1 += pa[1] - pb[1];
                s2 += pa[2] - pb[2];
                s3 += pa[3] - pb[3];
            }
        }
        memset(sums, 0, (size_t)stride * sizeof(uint16_t));
        for (int i = -r; i <= r; i++) {
            const uint8_t *row = tmp + (size_t)LV_CLAMP(0, i, n - 1) * stride;

            for (int j = 0; j < stride; j++) {
                sums[j] += row[j];
            }
        }
        for (int y = 0; y < n; y++) {
            const uint8_t *ra = tmp + (size_t)add[y] * stride;
            const uint8_t *rb = tmp + (size_t)sub[y] * stride;
            uint8_t *out = img + (size_t)y * stride;

            for (int j = 0; j < stride; j++) {
                out[j] = (uint8_t)((sums[j] * recip) >> 16);
                sums[j] = (uint16_t)(sums[j] + ra[j] - rb[j]);
            }
        }
    }
    lv_free(tmp);
    lv_free(sums);
    lv_free(add);
    return true;
}

/* Colour over coverage, then graded: n x n RGBA (weighted) to RGB, in place. */
static void unweight_grade(uint8_t *img, int n, float gain, float lift, float sat)
{
    for (int i = 0; i < n * n; i++) {
        /* Far outside the circle the coverage is a few counts, its colours rounding noise:
         * dividing by at least COVER_MIN fades them to black instead. */
        uint32_t u = s_unweight[img[i * 4 + 3]];
        float r = (float)LV_MIN(255u, (img[i * 4] * u) >> 16);
        float g = (float)LV_MIN(255u, (img[i * 4 + 1] * u) >> 16);
        float b = (float)LV_MIN(255u, (img[i * 4 + 2] * u) >> 16);
        float y = 0.21f * r + 0.72f * g + 0.07f * b;

        img[i * 3] = (uint8_t)clampf((y + (r - y) * sat) * gain + lift, 0.0f, 255.0f);
        img[i * 3 + 1] = (uint8_t)clampf((y + (g - y) * sat) * gain + lift, 0.0f, 255.0f);
        img[i * 3 + 2] = (uint8_t)clampf((y + (b - y) * sat) * gain + lift, 0.0f, 255.0f);
    }
}

/* RGB565 <-> 8 bit per channel, and the dithered quantisation back (per Bayer level), as
 * tables: the per-pixel loops below have no divisions. */
static uint8_t s_e5[32];
static uint8_t s_e6[64];
static uint8_t s_q5[16][256];
static uint8_t s_q6[16][256];
static bool s_tables;

static void tables_init(void)
{
    if (s_tables) {
        return;
    }
    for (int i = 0; i < 32; i++) {
        s_e5[i] = (uint8_t)(i * 255 / 31);
    }
    for (int i = 0; i < 64; i++) {
        s_e6[i] = (uint8_t)(i * 255 / 63);
    }
    for (int d = 0; d < 16; d++) {
        for (int c = 0; c < 256; c++) {
            s_q5[d][c] = (uint8_t)LV_MIN(31, (c * 31 + d * 16) / 255);
            s_q6[d][c] = (uint8_t)LV_MIN(63, (c * 63 + d * 16) / 255);
        }
    }
    for (int w = 0; w < 256; w++) {
        uint32_t d = (uint32_t)LV_MAX(w, COVER_MIN);

        s_unweight[w] = ((255u << 16) + d - 1) / d;
    }
    s_tables = true;
}

/* Red and blue in the 16-bit lanes of one 32-bit word (the clock has no 64-bit ops). */
#define RBLANES(p) ((uint32_t)(p)[0] | (uint32_t)(p)[2] << 16)

static inline uint16_t pack(int r, int g, int b, int x, int y)
{
    int d = s_bayer[y & 3][x & 3];

    return (uint16_t)(s_q5[d][LV_CLAMP(0, r, 255)] << 11 | s_q6[d][LV_CLAMP(0, g, 255)] << 5 | s_q5[d][LV_CLAMP(0, b, 255)]);
}

/* Half width (x from the centre, in px) of the screen's circle less 3 px, on row y; -1 off it. */
static int circle_half(int y)
{
    float dy = (float)y + 0.5f - SCREEN / 2.0f;
    float r = SCREEN / 2.0f - 3.0f;

    return dy * dy >= r * r ? -1 : (int)sqrtf(r * r - dy * dy);
}

/* What the glass refracts: the face itself over the face, the face frosted and dimmed on pages
 * (also their ground). A copy: the snapshot's buffer is the screen scratch, soon reused. */
static void back_build(const lv_draw_buf_t *face, ui_glass_mode_t mode)
{
    s_back_ok = false;
    if (screen_buf(&s_back) == NULL) {
        return;
    }
    for (int y = 0; y < SCREEN; y++) {
        const uint16_t *in = (const uint16_t *)(face->data + (size_t)y * face->header.stride);
        uint16_t *out = (uint16_t *)(s_back->data + (size_t)y * s_back->header.stride);

        if (mode == UI_GLASS_FACE) {
            memcpy(out, in, SCREEN * 2);
            continue;
        }
        if (s_frost_ok) {
            /* Pages stand on the face frosted and dimmed (PAGE_GROUND / 256): its shapes and
             * colour stay, none of its detail competes with what the page says. */
            const uint16_t *f = (const uint16_t *)(s_frost->data + (size_t)(y >> 1) * s_frost->header.stride);

            for (int x = 0; x < SCREEN; x++) {
                uint16_t v = f[x >> 1];

                out[x] = pack((s_e5[v >> 11] * PAGE_GROUND) >> 8, (s_e6[(v >> 5) & 63] * PAGE_GROUND) >> 8,
                              (s_e5[v & 31] * PAGE_GROUND) >> 8, x, y);
            }
            continue;
        }
        /* No frost yet: v * 3/8 per channel on the packed pixel, half less an eighth. */
        for (int x = 0; x < SCREEN; x++) {
            uint16_t v = in[x];

            out[x] = (uint16_t)(((v >> 1) & 0x7BEF) - ((v >> 3) & 0x18E3));
        }
    }
    s_back_ok = true;
    s_mode = mode;
    if (mode == UI_GLASS_FACE) {
        s_generation++; /* the sharp face behind: a new snapshot is a new look */
    }
}

bool ui_glass_backdrop_to_page(lv_draw_buf_t *keep_sharp)
{
    if (!s_back_ok || s_mode != UI_GLASS_FACE || keep_sharp == NULL || keep_sharp->header.w < SCREEN ||
        keep_sharp->header.h < SCREEN) {
        return false;
    }
    for (int y = 0; y < SCREEN; y++) {
        memcpy(keep_sharp->data + (size_t)y * keep_sharp->header.stride,
               s_back->data + (size_t)y * s_back->header.stride, SCREEN * 2);
    }
    s_sharp = keep_sharp;
    s_sharp_ok = true;
    back_build(keep_sharp, UI_GLASS_PAGE);
    return true;
}

/* The frost of face into frost (FROST x FROST): down to a quarter, blurred, graded, up to
 * half. With deep, the deep frost (s_deep) too. light: whether the face is mostly light. */
static bool frost_make(const lv_draw_buf_t *face, lv_draw_buf_t *frost, bool deep, bool *light)
{
    uint8_t *low = NULL;
    uint32_t *hrow = NULL;
    float gain;
    float lift;
    float sat;
    uint32_t luma = 0;

    GLASS_PERF_START();

    tables_init();
    low = lv_malloc(LOW * LOW * 4);
    /* Three widened rows (red and blue, then green), small and hot: internal RAM if there is room. */
    hrow = heap_caps_malloc(3 * 2 * FROST * sizeof(uint32_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (hrow == NULL) {
        hrow = heap_caps_malloc(3 * 2 * FROST * sizeof(uint32_t), MALLOC_CAP_DEFAULT);
    }
    if (low == NULL || hrow == NULL) {
        lv_free(low);
        heap_caps_free(hrow);
        return false;
    }
    /* Down to a quarter: the sum of each 4x4 block's pixels inside the screen's circle, and
     * how many there are as a fourth channel. Colours stay weighted by it through the blur, so
     * blurring near the rim spreads the face outward instead of pulling the black outside the
     * circle in. */
    {
        /* Raw sums per cell over its four rows (at most 16 x 63), divided once. */
        uint16_t *acc = lv_malloc(LOW * 4 * sizeof(uint16_t));

        if (acc == NULL) {
            lv_free(low);
            heap_caps_free(hrow);
            return false;
        }
        for (int ly = 0; ly < LOW; ly++) {
            memset(acc, 0, LOW * 4 * sizeof(uint16_t));
            /* Every other pixel of every other row: the blur that follows hides the difference. */
            for (int y = ly * 4; y < ly * 4 + 4; y += 2) {
                const uint16_t *row = (const uint16_t *)(face->data + (size_t)y * face->header.stride);
                int half = circle_half(y);

                for (int x = (SCREEN / 2 - half + 1) & ~1; half >= 0 && x < SCREEN / 2 + half; x += 2) {
                    uint16_t v = row[x];
                    uint16_t *c = acc + (x >> 2) * 4;

                    c[0] += (v >> 11) & 0x1F;
                    c[1] += (v >> 5) & 0x3F;
                    c[2] += v & 0x1F;
                    c[3]++;
                }
            }
            for (int lx = 0; lx < LOW; lx++) {
                const uint16_t *c = acc + lx * 4;
                uint8_t *o = low + ((size_t)ly * LOW + lx) * 4;

                /* Four samples: x 255 / (31 x 4) and x 255 / (63 x 4) as multiply and shift. */
                o[0] = (uint8_t)LV_MIN(255u, (c[0] * 134774u) >> 16);
                o[1] = (uint8_t)LV_MIN(255u, (c[1] * 66313u) >> 16);
                o[2] = (uint8_t)LV_MIN(255u, (c[2] * 134774u) >> 16);
                o[3] = (uint8_t)LV_MIN(255, (c[3] * 255u) >> 2);
            }
        }
        lv_free(acc);
    }
    GLASS_PERF_STEP("down");
    /* Three box passes each way (close to a Gaussian of about 13 px at full size);
     * then, for the deep frost, down by two more and three passes again (about 30 px): a sheet
     * as wide as the screen shows no tick or numeral through it, and the picture it carries
     * along while dragged has nothing in it to give the move away. */
    if (deep) {
        s_deep_ok = false;
        if (s_deep == NULL) {
            s_deep = lv_malloc(DEEP * DEEP * 4);
        }
    }
    if (blur_rgba(low, LOW, 3) && deep && s_deep != NULL) {
        for (int dy = 0; dy < DEEP; dy++) {
            for (int dx = 0; dx < DEEP; dx++) {
                const uint8_t *a = low + ((size_t)dy * 2 * LOW + (size_t)dx * 2) * 4;

                for (int c = 0; c < 4; c++) {
                    s_deep[(dy * DEEP + dx) * 4 + c] =
                        (uint8_t)((a[c] + a[4 + c] + a[LOW * 4 + c] + a[LOW * 4 + 4 + c] + 2) >> 2);
                }
            }
        }
        if (!blur_rgba(s_deep, DEEP, 3)) {
            lv_free(s_deep);
            s_deep = NULL;
        }
    }
    GLASS_PERF_STEP("blur");
    /* Undo the weighting: colour over coverage. */
    for (int i = 0; i < LOW * LOW; i++) {
        uint32_t u = s_unweight[low[i * 4 + 3]];

        for (int c = 0; c < 3; c++) {
            low[i * 3 + c] = (uint8_t)LV_MIN(255u, (low[i * 4 + c] * u) >> 16);
        }
    }
    for (int i = 0; i < LOW * LOW; i++) {
        luma += (uint32_t)(low[i * 3] * 54 + low[i * 3 + 1] * 183 + low[i * 3 + 2] * 19) >> 8;
    }
    /* Light: Sternglas and Modern Silver (white dials), not Avenir's orange. */
    *light = luma / (LOW * LOW) > 165;
    if (*light) {
        gain = 0.90f;
        lift = 22.0f;
        sat = 1.05f;
    } else {
        gain = 0.80f;
        lift = 20.0f;
        sat = 1.18f;
    }
    /* Graded in place at the low resolution. */
    for (int i = 0; i < LOW * LOW; i++) {
        float r = low[i * 3];
        float g = low[i * 3 + 1];
        float b = low[i * 3 + 2];
        float y = 0.21f * r + 0.72f * g + 0.07f * b;

        low[i * 3] = (uint8_t)clampf((y + (r - y) * sat) * gain + lift, 0.0f, 255.0f);
        low[i * 3 + 1] = (uint8_t)clampf((y + (g - y) * sat) * gain + lift, 0.0f, 255.0f);
        low[i * 3 + 2] = (uint8_t)clampf((y + (b - y) * sat) * gain + lift, 0.0f, 255.0f);
    }
    if (deep && s_deep != NULL) {
        unweight_grade(s_deep, DEEP, gain, lift, sat);
        s_deep_ok = true;
        for (int k = 0; k < 4; k++) {
            s_deep_row_y[k] = -1;
        }
    }
    GLASS_PERF_STEP("grade");
    /* Up to half resolution bilinearly. A frost pixel sits a quarter of the way between two
     * low ones each way, so it is (3 near + 1 far) / 4 across and down: shifts and adds, red
     * and blue at once in 16-bit lanes, green on its own. Each low row is widened once (three
     * are kept: an output row needs its own and the one above or below); dithered so
     * gradients do not band. */
    {
        int cached[3] = {-1, -1, -1};

        for (int y = 0; y < FROST; y++) {
            int near = y >> 1;
            int far = LV_CLAMP(0, (y & 1) ? near + 1 : near - 1, LOW - 1);
            uint16_t *out = (uint16_t *)(frost->data + (size_t)y * frost->header.stride);
            const uint32_t *hn;
            const uint32_t *hf;

            for (int k = 0; k < 2; k++) {
                int ly = k == 0 ? near : far;
                int slot = ly % 3;

                if (cached[slot] != ly) {
                    const uint8_t *lr = low + (size_t)ly * LOW * 3;
                    uint32_t *rb = hrow + (size_t)slot * 2 * FROST;
                    uint32_t *g = rb + FROST;
                    uint32_t prev = RBLANES(lr);
                    uint32_t cur = prev;
                    uint32_t gprev = lr[1];
                    uint32_t gcur = gprev;

                    for (int lx = 0; lx < LOW; lx++) {
                        const uint8_t *np = lr + (lx + 1 < LOW ? lx + 1 : lx) * 3;
                        uint32_t next = RBLANES(np);
                        uint32_t gnext = np[1];
                        uint32_t c3 = cur * 3;
                        uint32_t g3 = gcur * 3;

                        rb[lx * 2] = c3 + prev;
                        rb[lx * 2 + 1] = c3 + next;
                        g[lx * 2] = g3 + gprev;
                        g[lx * 2 + 1] = g3 + gnext;
                        prev = cur;
                        cur = next;
                        gprev = gcur;
                        gcur = gnext;
                    }
                    cached[slot] = ly;
                }
            }
            hn = hrow + (size_t)(near % 3) * 2 * FROST;
            hf = hrow + (size_t)(far % 3) * 2 * FROST;
            for (int x = 0; x < FROST; x++) {
                uint32_t v = hn[x] * 3 + hf[x];                 /* 16 x red and blue */
                uint32_t g = hn[FROST + x] * 3 + hf[FROST + x]; /* 16 x green */

                out[x] = pack((int)(v >> 4) & 0xFFF, (int)(g >> 4), (int)(v >> 20) & 0xFFF, x, y);
            }
        }
    }
    GLASS_PERF_STEP("up");
    lv_free(low);
    heap_caps_free(hrow);
    return true;
}

void ui_glass_backdrop_build(const lv_draw_buf_t *face, ui_glass_mode_t mode, bool frost_too)
{
    GLASS_PERF_START();

    if (s_frost == NULL) {
        s_frost = lv_draw_buf_create(FROST, FROST, LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
    }
    if (face == NULL || face->data == NULL || s_frost == NULL || face->header.cf != LV_COLOR_FORMAT_RGB565) {
        return;
    }
    tables_init();
    s_sharp = face;
    s_sharp_ok = true;
    if (!frost_too && s_frost_ok) {
        back_build(face, mode);
        GLASS_PERF_END("backdrop_back", SCREEN, SCREEN);
        return;
    }
    if (!frost_make(face, s_frost, true, &s_light)) {
        return;
    }
    s_frost_ok = true;
    s_generation++;

    back_build(face, mode);
    GLASS_PERF_END(mode == UI_GLASS_FACE ? "backdrop_face" : "backdrop_page", SCREEN, SCREEN);
}

void ui_glass_backdrop_identity(uint32_t identity)
{
    s_identity = identity;
}

bool ui_glass_backdrop_ready(void)
{
    return s_frost_ok;
}

ui_glass_mode_t ui_glass_backdrop_mode(void)
{
    return s_mode;
}

uint32_t ui_glass_backdrop_generation(void)
{
    return s_generation;
}

bool ui_glass_backdrop_light(void)
{
    return s_frost_ok && s_light && s_mode == UI_GLASS_FACE;
}

/* --- The glass model ---------------------------------------------------------------------- */

/*
 * After the reconstructions of Apple's Liquid Glass (kube.io, "Liquid Glass in the Browser:
 * Refraction with CSS and SVG", and liquid-glass-js): a slab of glass (n = 1.5) whose bezel
 * rises from the edge as a convex squircle, h(x) = (1 - (1 - x)^4)^(1/4), into a flat top. A ray
 * from straight above meets the tilted bezel at angle a, bends to asin(sin(a) / n), and lands
 * h tan(a - asin(sin(a) / n)) further in: the background is displaced towards the inside, so
 * the bezel magnifies like a lens while the flat middle shows it undisturbed. The three colour
 * channels bend a little differently (dispersion). The specular rim is strongest where the
 * bezel is steepest and faces the light (top left), with a weaker reflection opposite.
 */
#define GLASS_N 1.5f
#define LUT 48

typedef struct {
    float bezel;          /* px from the edge to the flat top */
    float disp[LUT + 1];  /* displacement (px) by distance from the edge, 0..bezel */
    float slope[LUT + 1]; /* sin of the surface tilt: how steep the bezel is there */
} bezel_t;

static float squircle(float x)
{
    x = clampf(x, 0.0f, 1.0f);
    return powf(1.0f - powf(1.0f - x, 4.0f), 0.25f);
}

static void bezel_build(bezel_t *b, float bezel, float thickness, float strength)
{
    b->bezel = bezel;
    for (int i = 0; i <= LUT; i++) {
        float x = LV_MAX(0.004f, (float)i / (float)LUT);
        float d = 0.002f;
        float deriv = (squircle(x + d) - squircle(LV_MAX(0.0f, x - d))) / (x + d - LV_MAX(0.0f, x - d)) * thickness / bezel;
        float a = atanf(deriv);
        float t = asinf(sinf(a) / GLASS_N);

        b->disp[i] = squircle(x) * thickness * tanf(a - t) * strength;
        b->slope[i] = sinf(a);
    }
}

static void bezel_at(const bezel_t *b, float s, float *disp, float *slope)
{
    float f;
    int i;

    if (s >= b->bezel) {
        *disp = 0.0f;
        *slope = 0.0f;
        return;
    }
    f = LV_MAX(0.0f, s) / b->bezel * (float)LUT;
    i = LV_MIN((int)f, LUT - 1);
    f -= (float)i;
    *disp = b->disp[i] + (b->disp[i + 1] - b->disp[i]) * f;
    *slope = b->slope[i] + (b->slope[i + 1] - b->slope[i]) * f;
}

/* How each kind of glass looks: how frosted, its tint, its rim. */
typedef struct {
    float frost;     /* 0 clear (the sharp backdrop) .. 1 the frost */
    float tint_a;
    float tint[3];
    float spec;      /* specular rim strength */
    float vibrance;  /* saturation of what is seen through */
    float edge_dark; /* light glass on a light face: a darker edge to stand off it */
} look_t;

static look_t look_for(ui_glass_kind_t kind, lv_color_t tint, ui_glass_mode_t mode, bool light_face);

static look_t look_of(ui_glass_kind_t kind, lv_color_t tint, ui_glass_mode_t mode)
{
    return look_for(kind, tint, mode, s_light);
}

/* The look of glass of kind over a light (white dial) or dark face. */
static look_t look_for(ui_glass_kind_t kind, lv_color_t tint, ui_glass_mode_t mode, bool light_face)
{
    bool face = mode == UI_GLASS_FACE;
    bool light = face && light_face;
    look_t l = {face ? 0.55f : 0.85f, 0.0f, {0, 0, 0}, 0.62f, 1.18f, light ? 0.30f : 0.0f};

    switch (kind) {
    case UI_GLASS_TINT:
        l.frost = 0.7f;
        l.tint_a = 0.56f;
        l.tint[0] = tint.red;
        l.tint[1] = tint.green;
        l.tint[2] = tint.blue;
        l.spec = 0.7f;
        break;
    case UI_GLASS_RAISED:
        l.tint_a = light ? 0.34f : 0.10f;
        l.tint[0] = l.tint[1] = l.tint[2] = light ? 214.0f : 255.0f;
        l.tint[2] = light ? 222.0f : 255.0f;
        break;
    case UI_GLASS_WELL:
        l.frost = 0.95f;
        l.tint_a = 0.38f;
        l.spec = 0.3f;
        break;
    case UI_GLASS_SHEET:
        l.frost = 0.62f; /* clear enough to see the face behind, which stays put as it moves */
        l.tint_a = light ? 0.12f : 0.20f;
        if (light) {
            l.tint[0] = l.tint[1] = l.tint[2] = 255.0f;
        }
        break;
    case UI_GLASS_CLEAR:
    default:
        if (light) {
            l.tint_a = 0.32f;
            l.tint[0] = l.tint[1] = 214.0f;
            l.tint[2] = 222.0f;
        } else {
            l.tint_a = face ? 0.12f : 0.06f;
        }
        break;
    }
    if (light && kind != UI_GLASS_TINT) {
        /* On a white face light glass all but vanishes: smoked glass instead, frosted enough
         * that the dial's ticks do not speckle it, with a darker edge to stand off the face. */
        l.frost = LV_MAX(l.frost, 0.86f);
        l.tint[0] = 112.0f;
        l.tint[1] = 116.0f;
        l.tint[2] = 126.0f;
        l.tint_a = kind == UI_GLASS_WELL ? 0.52f : 0.44f;
        l.edge_dark = 0.40f;
        if (kind == UI_GLASS_SHEET) {
            /* Sheets keep their white type: darker smoke under it. */
            l.frost = 0.92f;
            l.tint[0] = 44.0f;
            l.tint[1] = 46.0f;
            l.tint[2] = 54.0f;
            l.tint_a = 0.62f;
            l.edge_dark = 0.0f;
        }
    }
    return l;
}

/* Signed distance to a rounded rectangle (negative inside) and the outward normal. */
static float rrect_sdf(float px, float py, float hx, float hy, float r, float *nx, float *ny)
{
    float qx = fabsf(px) - (hx - r);
    float qy = fabsf(py) - (hy - r);
    float sx = px < 0 ? -1.0f : 1.0f;
    float sy = py < 0 ? -1.0f : 1.0f;

    if (qx > 0 && qy > 0) {
        float l = sqrtf(qx * qx + qy * qy);

        *nx = sx * qx / l;
        *ny = sy * qy / l;
        return l - r;
    }
    if (qx > qy) {
        *nx = sx;
        *ny = 0;
    } else {
        *nx = 0;
        *ny = sy;
    }
    return LV_MAX(qx, qy) - r;
}

/* The glass at screen point (x, y), s px inside the edge with outward normal (nx, ny). */
static inline const uint16_t *frost_row(int32_t y);

static inline uint16_t fetch_frost(int32_t x, int32_t y)
{
    return frost_row(y)[LV_CLAMP(0, x, SCREEN - 1) >> 1];
}

static inline uint16_t fetch(const lv_draw_buf_t *img, int32_t x, int32_t y)
{
    x = LV_CLAMP(0, x, SCREEN - 1);
    y = LV_CLAMP(0, y, SCREEN - 1);
    return ((const uint16_t *)(img->data + (size_t)y * img->header.stride))[x];
}

/* A look in integers, for the per-pixel loops. */
typedef struct {
    int frost; /* 0..256 */
    int fgain; /* the frost's brightness: darker on pages, whose ground is the dimmed face */
    int bgain; /* the back's: a page's glass baked while the back is still the bright face */
    int vib;   /* 256 = as is */
    int ta;    /* tint amount, 0..256 */
    int tint[3];
} ilook_t;

static ilook_t ilook_of(const look_t *l, ui_glass_mode_t mode)
{
    ilook_t i = {(int)(l->frost * 256.0f), mode == UI_GLASS_PAGE ? 150 : 256,
                 mode == UI_GLASS_PAGE && s_mode == UI_GLASS_FACE ? PAGE_GROUND : 256, (int)(l->vibrance * 256.0f),
                 (int)(l->tint_a * 256.0f), {(int)l->tint[0], (int)l->tint[1], (int)l->tint[2]}};

    return i;
}

/* The glass body over backdrop colour b and frost colour f (8 bit): frosted, saturated,
 * tinted. */
static inline void body(const ilook_t *il, int br, int bg, int bb, int fr, int fg, int fb, int *c)
{
    int lum;

    br = (br * il->bgain) >> 8;
    bg = (bg * il->bgain) >> 8;
    bb = (bb * il->bgain) >> 8;
    fr = (fr * il->fgain) >> 8;
    fg = (fg * il->fgain) >> 8;
    fb = (fb * il->fgain) >> 8;
    c[0] = br + (((fr - br) * il->frost) >> 8);
    c[1] = bg + (((fg - bg) * il->frost) >> 8);
    c[2] = bb + (((fb - bb) * il->frost) >> 8);
    lum = (c[0] * 54 + c[1] * 183 + c[2] * 19) >> 8;
    for (int k = 0; k < 3; k++) {
        c[k] = lum + (((c[k] - lum) * il->vib) >> 8);
        c[k] += ((il->tint[k] - c[k]) * il->ta) >> 8;
    }
}

/* The glass at screen point (x, y), s px inside the edge with outward normal (nx, ny): the
 * backdrop bent through the bezel (each channel a little differently: dispersion), the body,
 * then the specular rim. Nearest samples: the bend is smooth, and this runs per pixel. */
static void glass_pixel(float x, float y, float s, float nx, float ny, const bezel_t *bz, const look_t *l,
                        const ilook_t *ilp, float *out)
{
    const ilook_t il = *ilp;
    float disp;
    float slope;
    uint16_t vr;
    uint16_t vg;
    uint16_t vb;
    uint16_t vf;
    int c[3];
    float lit;
    float refl;
    float spec;

    bezel_at(bz, s, &disp, &slope);
    vr = fetch(s_back, (int32_t)(x - nx * disp * 0.94f), (int32_t)(y - ny * disp * 0.94f));
    vg = fetch(s_back, (int32_t)(x - nx * disp), (int32_t)(y - ny * disp));
    vb = fetch(s_back, (int32_t)(x - nx * disp * 1.06f), (int32_t)(y - ny * disp * 1.06f));
    vf = fetch_frost((int32_t)(x - nx * disp), (int32_t)(y - ny * disp));
    body(&il, s_e5[vr >> 11], s_e6[(vg >> 5) & 63], s_e5[vb & 31], s_e5[vf >> 11], s_e6[(vf >> 5) & 63],
         s_e5[vf & 31], c);
    /* The rim: light from the top left, a reflection opposite; a hairline at the very edge. */
    lit = LV_MAX(0.0f, -0.6f * nx - 0.8f * ny);
    refl = LV_MAX(0.0f, 0.6f * nx + 0.8f * ny);
    spec = slope == 0.0f ? 0.0f : l->spec * slope * slope * (0.25f + 0.85f * lit * lit + 0.45f * refl * refl);
    spec += l->spec * 0.35f * clampf(1.4f - s, 0.0f, 1.0f);
    spec = clampf(spec, 0.0f, 1.0f);
    for (int k = 0; k < 3; k++) {
        float v = (float)LV_CLAMP(0, c[k], 255);
        /* The highlight takes some of the colour behind it. */
        float h = 191.0f + 0.25f * LV_MIN(255.0f, v * 1.6f);

        v += (h - v) * spec;
        if (l->edge_dark > 0.0f) {
            /* Light glass on a light face: a darker band inside the edge, most at the bottom. */
            v *= 1.0f - l->edge_dark * clampf(1.0f - s / 5.0f, 0.0f, 1.0f) * (0.55f + 0.45f * LV_MAX(0.0f, ny));
        }
        out[k] = v;
    }
}

static void bezel_for(bezel_t *bz, int32_t w, int32_t h)
{
    float bezel = clampf((float)LV_MIN(w, h) * 0.30f, 9.0f, 22.0f);

    bezel_build(bz, bezel, bezel * 0.95f, 1.6f);
}

/* --- Baked glass ------------------------------------------------------------------------- */

/* Each glass shape is baked once at its place: the backdrop refracted through it, tinted and
 * lit, as an RGB565 image drawn with its corners clipped. A shape that moved (a scrolled row)
 * keeps its image while moving and is baked again once it has stayed put for a moment. */
#define BAKE_MAX 48
#define BAKE_BYTES (3200u * 1024u)
#define BAKE_SETTLE_MS 120

typedef struct {
    const void *key;
    int16_t x, y, w, h, r;
    uint8_t kind;
    uint32_t tint;
    uint32_t gen;      /* over the face: the generation it was baked from */
    uint32_t identity; /* on pages: the face it was baked from (the minute does not show) */
    uint8_t mode;
    bool deep;         /* on a sheet or a large panel: frosted like it, not seeing past it */
    lv_draw_buf_t *buf;
    uint32_t stamp;
    int16_t pend_x, pend_y;
    uint32_t pend_since;
    bool opaque; /* corners baked against the ground: drawn as a plain image */
    int16_t bez, corner; /* the rim's width and the corners' size (a sheet bakes only these) */
} bake_t;

static bake_t s_bakes[BAKE_MAX];
static uint32_t s_bake_stamp;
static size_t s_bake_bytes;
static lv_timer_t *s_settle_timer;

/* Baked images dropped from the cache wait here until the frame being drawn is done: its
 * draw tasks may still read them. */
#define GRAVE_MAX 16
static lv_draw_buf_t *s_grave[GRAVE_MAX];
static bool s_grave_hooked;

static void grave_free(void)
{
    for (int i = 0; i < GRAVE_MAX; i++) {
        if (s_grave[i] != NULL) {
            lv_draw_buf_destroy(s_grave[i]);
            s_grave[i] = NULL;
        }
    }
}

static void render_ready_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    grave_free();
}

static void bake_drop(bake_t *b)
{
    if (b->buf == NULL) {
        return;
    }
    s_bake_bytes -= b->buf->data_size;
    for (int i = 0; i < GRAVE_MAX; i++) {
        if (s_grave[i] == NULL) {
            s_grave[i] = b->buf;
            b->buf = NULL;
            b->key = NULL;
            return;
        }
    }
    lv_draw_wait_for_finish();
    grave_free();
    s_grave[0] = b->buf;
    b->buf = NULL;
    b->key = NULL;
}

/* The bezel at one distance from a straight edge, in integers: how far each channel is bent
 * (px, along the inward normal), the highlight (0..256) and the light glass's edge shade. */
typedef struct {
    int16_t off[3];
    int16_t off_f;
    int16_t spec;
    int16_t dark; /* 256 = none */
} rim_t;

static rim_t rim_at(const bezel_t *bz, const look_t *l, float s, float nx, float ny)
{
    rim_t r;
    float disp;
    float slope;
    float lit = LV_MAX(0.0f, -0.6f * nx - 0.8f * ny);
    float refl = LV_MAX(0.0f, 0.6f * nx + 0.8f * ny);
    float spec;

    bezel_at(bz, s, &disp, &slope);
    for (int k = 0; k < 3; k++) {
        r.off[k] = (int16_t)lroundf(disp * (0.94f + 0.06f * (float)k));
    }
    r.off_f = (int16_t)lroundf(disp);
    spec = slope == 0.0f ? 0.0f : l->spec * slope * slope * (0.25f + 0.85f * lit * lit + 0.45f * refl * refl);
    spec += l->spec * 0.35f * clampf(1.4f - s, 0.0f, 1.0f);
    r.spec = (int16_t)lroundf(clampf(spec, 0.0f, 1.0f) * 256.0f);
    r.dark = (int16_t)lroundf(256.0f * (1.0f - l->edge_dark * clampf(1.0f - s / 5.0f, 0.0f, 1.0f) *
                                                    (0.55f + 0.45f * LV_MAX(0.0f, ny))));
    return r;
}

/* Body, highlight and edge shade of one pixel, packed. */
static inline uint16_t shade_px(const ilook_t *il, const rim_t *rp, uint16_t vr, uint16_t vg, uint16_t vb,
                                uint16_t vf, int32_t sx, int32_t sy)
{
    int c[3];

    body(il, s_e5[vr >> 11], s_e6[(vg >> 5) & 63], s_e5[vb & 31], s_e5[vf >> 11], s_e6[(vf >> 5) & 63], s_e5[vf & 31],
         c);
    if (rp != NULL) {
        for (int k = 0; k < 3; k++) {
            int v = LV_CLAMP(0, c[k], 255);
            /* The highlight takes some of the colour behind it. */
            int hl = 191 + LV_MIN(64, (v * 2) / 5);

            v += ((hl - v) * rp->spec) >> 8;
            c[k] = (v * rp->dark) >> 8;
        }
    }
    return pack(c[0], c[1], c[2], sx, sy);
}

static inline const uint16_t *back_row(int32_t y)
{
    return (const uint16_t *)(s_back->data + (size_t)LV_CLAMP(0, y, SCREEN - 1) * s_back->header.stride);
}

/* A deep frost row at the frost's size (bilinear from DEEP, x 4), made when first asked for: a
 * bake asks for the same few rows over and over. */
static const uint16_t *deep_row(int32_t fy)
{
    static int16_t x0[FROST];
    static uint8_t wx[FROST];
    static bool xs;
    int slot = fy & 3;
    uint16_t *out = s_deep_rows[slot];
    int f = (fy * 2 + 1) * 32 - 128; /* ((fy + 0.5) / 4 - 0.5) * 256 */
    int y0 = f < 0 ? 0 : LV_MIN(f >> 8, DEEP - 2);
    int wy = f < 0 ? 0 : ((f >> 8) >= DEEP - 1 ? 255 : (f & 255));
    const uint8_t *r0 = s_deep + (size_t)y0 * DEEP * 3;
    const uint8_t *r1 = r0 + DEEP * 3;

    if (s_deep_row_y[slot] == fy) {
        return out;
    }
    if (!xs) {
        for (int x = 0; x < FROST; x++) {
            int fx = (x * 2 + 1) * 32 - 128;
            int i = fx < 0 ? 0 : fx >> 8;

            x0[x] = (int16_t)LV_MIN(i, DEEP - 2);
            wx[x] = (uint8_t)(fx < 0 ? 0 : (i >= DEEP - 1 ? 255 : (fx & 255)));
        }
        xs = true;
    }
    for (int x = 0; x < FROST; x++) {
        const uint8_t *a = r0 + x0[x] * 3;
        const uint8_t *b = r1 + x0[x] * 3;
        int w = wx[x];
        int c[3];

        for (int k = 0; k < 3; k++) {
            int top = a[k] * (256 - w) + a[3 + k] * w;
            int bot = b[k] * (256 - w) + b[3 + k] * w;

            c[k] = (top * (256 - wy) + bot * wy) >> 16;
        }
        out[x] = pack(c[0], c[1], c[2], x, fy);
    }
    s_deep_row_y[slot] = (int16_t)fy;
    return out;
}

/* The deep frost at the frost's size, made whole the first time a bake asks for it: a bake's
 * rim reads rows above and below the one it is on, more than a few cached rows hold. */
static lv_draw_buf_t *s_deep_full;
static uint32_t s_deep_full_gen;
static bool s_deep_full_ok;

static const uint16_t *deep_full_row(int32_t fy)
{
    if (!s_deep_full_ok || s_deep_full_gen != s_generation) {
        if (s_deep_full == NULL) {
            s_deep_full = lv_draw_buf_create(FROST, FROST, LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
        }
        if (s_deep_full == NULL) {
            return deep_row(fy);
        }
        for (int32_t y = 0; y < FROST; y++) {
            memcpy(s_deep_full->data + (size_t)y * s_deep_full->header.stride, deep_row(y), FROST * 2);
        }
        s_deep_full_gen = s_generation;
        s_deep_full_ok = true;
    }
    return (const uint16_t *)(s_deep_full->data + (size_t)fy * s_deep_full->header.stride);
}

static inline const uint16_t *frost_row(int32_t y)
{
    int32_t fy = LV_CLAMP(0, y, SCREEN - 1) >> 1;

    if (s_use_deep) {
        return deep_full_row(fy);
    }
    return (const uint16_t *)(s_frost->data + (size_t)fy * s_frost->header.stride);
}

/* --- Sheet ground ------------------------------------------------------------------------ */

/* A sheet's body over the whole screen: the face as the sheet's glass shows it, some of it
 * sharp through the frost. It stays put while a sheet moves over it (the face behind does not
 * move with the glass); only the sheet's rim, keys and labels move. Built a band of rows at a
 * time (at idle). */
static lv_draw_buf_t *s_sheet_ground;
static uint32_t s_sheet_ground_gen;
static int32_t s_sheet_ground_row = -1; /* rows done; -1 none */
static bool s_hollow;                   /* drawing a moving sheet's snapshot: no body */

static bool sheet_ground_build(uint32_t budget_ms)
{
    uint32_t start = lv_tick_get();
    look_t l;
    ilook_t il;

    if (!s_frost_ok || !s_back_ok || s_mode != UI_GLASS_FACE || screen_buf(&s_sheet_ground) == NULL) {
        return false;
    }
    if (s_sheet_ground_gen != s_generation || s_sheet_ground_row < 0) {
        s_sheet_ground_gen = s_generation;
        s_sheet_ground_row = 0;
    }
    if (s_sheet_ground_row >= SCREEN) {
        return true;
    }
    l = look_of(UI_GLASS_SHEET, lv_color_black(), UI_GLASS_FACE);
    il = ilook_of(&l, UI_GLASS_FACE);
    GLASS_PERF_START();
    int32_t row_from = s_sheet_ground_row;

    (void)row_from;
    while (s_sheet_ground_row < SCREEN) {
        int32_t y = s_sheet_ground_row++;
        const uint16_t *bk = back_row(y);
        const uint16_t *f = (const uint16_t *)(s_frost->data + (size_t)(y >> 1) * s_frost->header.stride);
        uint16_t *out = (uint16_t *)(s_sheet_ground->data + (size_t)y * s_sheet_ground->header.stride);
        int half = circle_half(y);

        if (half < 0) {
            memset(out, 0, SCREEN * 2);
            continue;
        }
        for (int32_t x = 0; x < SCREEN; x++) {
            uint16_t v = bk[x];
            uint16_t fv = f[x >> 1];
            int c[3];

            body(&il, s_e5[v >> 11], s_e6[(v >> 5) & 63], s_e5[v & 31], s_e5[fv >> 11], s_e6[(fv >> 5) & 63],
                 s_e5[fv & 31], c);
            out[x] = pack(c[0], c[1], c[2], x, y);
        }
        if ((y & 15) == 15 && lv_tick_elaps(start) >= budget_ms) {
            break;
        }
    }
    GLASS_PERF_END("sheet_ground", SCREEN, s_sheet_ground_row - row_from);
    return s_sheet_ground_row >= SCREEN;
}

/* One pixel the slow way (corners): exact distance and normal, the ground outside the shape. */
static uint16_t corner_px(const bake_t *b, const bezel_t *bz, const look_t *l, const ilook_t *il, float hx, float hy,
                          float rr, int32_t x, int32_t y, int32_t sx, int32_t sy, uint16_t ground)
{
    float nx;
    float ny;
    float d = rrect_sdf((float)x + 0.5f - hx, (float)y + 0.5f - hy, hx, hy, rr, &nx, &ny);
    float c[3];

    if (d >= 0.5f) {
        return ground;
    }
    glass_pixel((float)sx + 0.5f, (float)sy + 0.5f, LV_MAX(0.0f, -d), nx, ny, bz, l, il, c);
    if (d > -0.5f && b->opaque) {
        /* The antialiased edge over the ground. */
        float cov = 0.5f - d;

        c[0] = c[0] * cov + (float)s_e5[ground >> 11] * (1.0f - cov);
        c[1] = c[1] * cov + (float)s_e6[(ground >> 5) & 63] * (1.0f - cov);
        c[2] = c[2] * cov + (float)s_e5[ground & 31] * (1.0f - cov);
    }
    return pack((int)c[0], (int)c[1], (int)c[2], sx, sy);
}

/* Bakes the shape. Away from the corners every row is spans: along the top and bottom bezel the
 * bend and the light are the same across the row, along the sides the same down a column, and
 * the flat top has none, so all of them run in integers; only the corners (radius x radius)
 * take the exact path. */
static void bake_render(bake_t *b, const lv_area_t *area, int32_t radius, ui_glass_kind_t kind, lv_color_t tint)
{
    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);
    float hx = (float)w / 2.0f;
    float hy = (float)h / 2.0f;
    look_t l = look_of(kind, tint, (ui_glass_mode_t)b->mode);
    ilook_t il;
    bezel_t bz;
    float rr = (float)radius;
    int32_t bez;
    int32_t corner;
    rim_t side[2][32];
    GLASS_PERF_START();

    /* Large panels are material: the deep frost only, the face bent at their edge but nowhere
     * seen sharp through them. Glass on them and on sheets (the sheet's keys) sees the same: the
     * panel is what is behind it, not the face under it. A sheet itself is clearer glass: its
     * body is the sheet ground (drawn under it, where it is), so only its rim is baked. */
    s_use_deep = s_deep_ok && (b->deep || (kind != UI_GLASS_SHEET && w * h > 100000));
    if (s_use_deep) {
        l.frost = 1.0f;
    }
    il = ilook_of(&l, (ui_glass_mode_t)b->mode);
    bezel_for(&bz, w, h);
    bez = LV_MIN((int32_t)ceilf(bz.bezel), 31);
    /* The straight spans start past the corner arcs (and the bezel, if the radius is smaller). */
    corner = LV_MAX(radius, bez);
    corner = LV_MIN(corner, LV_MIN(w, h) / 2);
    for (int32_t i = 0; i < bez; i++) {
        side[0][i] = rim_at(&bz, &l, (float)i + 0.5f, -1.0f, 0.0f);
        side[1][i] = rim_at(&bz, &l, (float)i + 0.5f, 1.0f, 0.0f);
    }
    for (int32_t y = 0; y < h; y++) {
        uint16_t *row = (uint16_t *)(b->buf->data + (size_t)y * b->buf->header.stride);
        int32_t sy = area->y1 + y;
        int32_t dy = LV_MIN(y, h - 1 - y); /* rows from the nearer edge */
        int32_t ny = y < h / 2 ? -1 : 1;
        const uint16_t *brow = back_row(sy);
        const uint16_t *frow = frost_row(sy);
        bool corner_row = dy < corner;
        int32_t x0 = corner_row ? corner : 0;
        int32_t x1 = corner_row ? w - corner : w;

        /* The corners, exactly. */
        if (corner_row) {
            for (int32_t x = 0; x < x0; x++) {
                int32_t sx = area->x1 + x;

                row[x] = corner_px(b, &bz, &l, &il, hx, hy, rr, x, y, sx, sy, brow[LV_CLAMP(0, sx, SCREEN - 1)]);
            }
            for (int32_t x = x1; x < w; x++) {
                int32_t sx = area->x1 + x;

                row[x] = corner_px(b, &bz, &l, &il, hx, hy, rr, x, y, sx, sy, brow[LV_CLAMP(0, sx, SCREEN - 1)]);
            }
        }
        if (dy < bez) {
            /* The top or bottom bezel: one bend and one light for the whole span. */
            rim_t rp = rim_at(&bz, &l, (float)dy + 0.5f, 0.0f, (float)ny);
            const uint16_t *rr_row = back_row(sy - ny * rp.off[0]);
            const uint16_t *rg_row = back_row(sy - ny * rp.off[1]);
            const uint16_t *rb_row = back_row(sy - ny * rp.off[2]);
            const uint16_t *rf_row = frost_row(sy - ny * rp.off_f);

            for (int32_t x = x0; x < x1; x++) {
                int32_t sx = LV_CLAMP(0, area->x1 + x, SCREEN - 1);

                row[x] = shade_px(&il, &rp, rr_row[sx], rg_row[sx], rb_row[sx], rf_row[sx >> 1], area->x1 + x, sy);
            }
            continue;
        }
        /* A middle row: the side bezels (one bend per column) and the flat top between. */
        for (int32_t x = x0; x < x1; x++) {
            int32_t sx = area->x1 + x;
            int32_t dx = LV_MIN(x, w - 1 - x);

            if (dx >= bez && kind == UI_GLASS_SHEET) {
                x = w - 1 - bez; /* the body: the sheet ground; on to the right rim */
                continue;
            }
            if (dx < bez) {
                const rim_t *rp = &side[x < w / 2 ? 0 : 1][dx];
                int32_t dir = x < w / 2 ? 1 : -1; /* inward */

                row[x] = shade_px(&il, rp, brow[LV_CLAMP(0, sx + dir * rp->off[0], SCREEN - 1)],
                                  brow[LV_CLAMP(0, sx + dir * rp->off[1], SCREEN - 1)],
                                  brow[LV_CLAMP(0, sx + dir * rp->off[2], SCREEN - 1)],
                                  frow[LV_CLAMP(0, sx + dir * rp->off_f, SCREEN - 1) >> 1], sx, sy);
            } else {
                int32_t cx = LV_CLAMP(0, sx, SCREEN - 1);

                row[x] = shade_px(&il, NULL, brow[cx], brow[cx], brow[cx], frow[cx >> 1], sx, sy);
            }
        }
    }
    b->x = (int16_t)area->x1;
    b->y = (int16_t)area->y1;
    b->bez = (int16_t)bez;
    b->corner = (int16_t)corner;
    b->gen = s_generation;
    b->identity = s_identity;
    b->pend_x = b->x;
    b->pend_y = b->y;
    s_use_deep = false;
    GLASS_PERF_END("bake", w, h);
}

static void settle_timer_cb(lv_timer_t *t)
{
    bool waiting = false;

    for (int i = 0; i < BAKE_MAX; i++) {
        bake_t *b = &s_bakes[i];

        if (b->buf != NULL && (b->pend_x != b->x || b->pend_y != b->y)) {
            waiting = true;
            if (lv_tick_elaps(b->pend_since) >= BAKE_SETTLE_MS && lv_obj_is_valid((lv_obj_t *)b->key)) {
                lv_obj_invalidate((lv_obj_t *)b->key);
            }
        }
    }
    if (!waiting) {
        lv_timer_pause(t);
    }
}

/* Whether a bake still shows its shape's backdrop: page glass (and sheets and large panels,
 * frosted enough) as long as the face is the same; glass over the face for each new backdrop. */
static bool bake_current(const bake_t *b, ui_glass_kind_t kind, int32_t w, int32_t h)
{
    return (b->mode == UI_GLASS_PAGE || kind == UI_GLASS_SHEET || w * h > 100000) ? b->identity == s_identity
                                                                                   : b->gen == s_generation;
}

static bake_t *bake_get(const void *key, const lv_area_t *area, int32_t radius, ui_glass_kind_t kind, lv_color_t tint,
                        bool opaque, ui_glass_mode_t mode, bool deep)
{
    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);
    uint32_t t = kind == UI_GLASS_TINT ? lv_color_to_u32(tint) : 0;
    size_t need = (size_t)w * (size_t)h * 2;
    bake_t *slot = NULL;

    for (int i = 0; i < BAKE_MAX; i++) {
        bake_t *b = &s_bakes[i];

        if (b->buf == NULL || b->key != key || b->w != w || b->h != h || b->r != radius || b->kind != kind ||
            b->tint != t || b->opaque != opaque || b->mode != (uint8_t)mode) {
            continue;
        }
        b->stamp = ++s_bake_stamp;
        if (!bake_current(b, kind, w, h)) {
            bake_render(b, area, radius, kind, tint);
        } else if (b->x != area->x1 || b->y != area->y1) {
            /* Moved: bake again once it has settled; meanwhile the old image moves with it. */
            if (b->pend_x != area->x1 || b->pend_y != area->y1) {
                b->pend_x = (int16_t)area->x1;
                b->pend_y = (int16_t)area->y1;
                b->pend_since = lv_tick_get();
                if (s_settle_timer == NULL) {
                    s_settle_timer = lv_timer_create(settle_timer_cb, 60, NULL);
                }
                lv_timer_resume(s_settle_timer);
            } else if (lv_tick_elaps(b->pend_since) >= BAKE_SETTLE_MS) {
                bake_render(b, area, radius, kind, tint);
            }
        }
        return b;
    }
    if (!s_grave_hooked && lv_display_get_default() != NULL) {
        lv_display_add_event_cb(lv_display_get_default(), render_ready_cb, LV_EVENT_REFR_READY, NULL);
        s_grave_hooked = true;
    }
    /* Room: the least recently drawn go first. */
    while (true) {
        bake_t *oldest = NULL;

        slot = NULL;
        for (int i = 0; i < BAKE_MAX; i++) {
            if (s_bakes[i].buf == NULL) {
                slot = slot != NULL ? slot : &s_bakes[i];
            } else if (oldest == NULL || s_bakes[i].stamp < oldest->stamp) {
                oldest = &s_bakes[i];
            }
        }
        if (oldest == NULL || (slot != NULL && s_bake_bytes + need <= BAKE_BYTES)) {
            break;
        }
        bake_drop(oldest);
    }
    if (slot == NULL) {
        return NULL;
    }
    slot->buf = lv_draw_buf_create((uint32_t)w, (uint32_t)h, LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
    if (slot->buf == NULL) {
        return NULL;
    }
    s_bake_bytes += slot->buf->data_size;
    slot->key = key;
    slot->w = (int16_t)w;
    slot->h = (int16_t)h;
    slot->r = (int16_t)radius;
    slot->kind = (uint8_t)kind;
    slot->tint = t;
    slot->opaque = opaque;
    slot->mode = (uint8_t)mode;
    slot->deep = deep;
    slot->stamp = ++s_bake_stamp;
    bake_render(slot, area, radius, kind, tint);
    return slot;
}

static void shells_release(void);

void ui_glass_release(void)
{
    for (int i = 0; i < BAKE_MAX; i++) {
        bake_drop(&s_bakes[i]);
    }
    lv_draw_wait_for_finish();
    grave_free();
    if (s_frost != NULL) {
        lv_draw_buf_destroy(s_frost);
        s_frost = NULL;
    }
    if (s_back != NULL) {
        lv_draw_buf_destroy(s_back);
        s_back = NULL;
    }
    if (s_sheet_ground != NULL) {
        lv_draw_buf_destroy(s_sheet_ground);
        s_sheet_ground = NULL;
    }
    if (s_deep_full != NULL) {
        lv_draw_buf_destroy(s_deep_full);
        s_deep_full = NULL;
    }
    shells_release();
    s_deep_full_ok = false;
    s_sheet_ground_row = -1;
    lv_free(s_deep);
    s_deep = NULL;
    s_deep_ok = false;
    s_frost_ok = false;
    s_back_ok = false;
}

/* --- Drawing ------------------------------------------------------------------------------ */

/* Views into the backdrop images, one per draw: a draw task reads its image when it runs, so
 * each must stay valid until the frame is drawn. A frame uses far fewer than VIEWS. */
static lv_draw_buf_t s_views[VIEWS];
static uint32_t s_view_next;

static lv_draw_buf_t *view_of(const lv_draw_buf_t *src, const lv_area_t *vis)
{
    lv_draw_buf_t *v = &s_views[s_view_next++ % VIEWS];

    *v = *src;
    v->header.w = (uint32_t)lv_area_get_width(vis);
    v->header.h = (uint32_t)lv_area_get_height(vis);
    v->data = src->data + (size_t)vis->y1 * src->header.stride + (size_t)vis->x1 * 2;
    v->unaligned_data = v->data;
    v->data_size = (uint32_t)src->header.stride * v->header.h;
    return v;
}

static void draw_view(lv_layer_t *layer, const lv_draw_buf_t *src, const lv_area_t *area, int32_t radius)
{
    lv_area_t screen = {0, 0, SCREEN - 1, SCREEN - 1};
    lv_area_t vis;
    lv_draw_image_dsc_t d;

    if (!lv_area_intersect(&vis, area, &screen)) {
        return;
    }
    lv_draw_image_dsc_init(&d);
    d.src = view_of(src, &vis);
    d.image_area = vis;
    d.clip_radius = radius;
    lv_draw_image(layer, &d, &vis);
}

void ui_glass_draw_sheet_ground(lv_layer_t *layer, const lv_area_t *area, int32_t radius)
{
    if (s_frost_ok && sheet_ground_build(UINT32_MAX)) {
        draw_view(layer, s_sheet_ground, area, radius);
    }
}

void ui_glass_set_hollow(bool hollow)
{
    s_hollow = hollow;
}

void ui_glass_hollow_corners(const void *key, lv_draw_buf_t *snap, int32_t ext)
{
    const bake_t *b = NULL;

    for (int i = 0; i < BAKE_MAX && b == NULL; i++) {
        if (s_bakes[i].buf != NULL && s_bakes[i].key == key && s_bakes[i].kind == UI_GLASS_SHEET) {
            b = &s_bakes[i];
        }
    }
    if (b == NULL || snap == NULL || snap->header.cf != LV_COLOR_FORMAT_ARGB8888) {
        return;
    }
    /* Inside the corner squares the bake has the body too, as it was where the sheet rests:
     * cleared past the rim, so the ground shows there as everywhere else. */
    for (int32_t y = 0; y < b->corner; y++) {
        for (int32_t x = 0; x < b->corner; x++) {
            float dx = LV_MAX(0.0f, (float)b->r - ((float)x + 0.5f));
            float dy = LV_MAX(0.0f, (float)b->r - ((float)y + 0.5f));
            float inside;

            if (dx > 0.0f && dy > 0.0f) {
                inside = (float)b->r - sqrtf(dx * dx + dy * dy);
            } else {
                inside = (float)LV_MIN(x, y) + 0.5f;
            }
            if (inside <= (float)b->bez + 1.0f) {
                continue;
            }
            for (int c = 0; c < 4; c++) {
                int32_t px = ext + ((c & 1) ? b->w - 1 - x : x);
                int32_t py = ext + ((c & 2) ? b->h - 1 - y : y);

                if (px >= 0 && py >= 0 && px < (int32_t)snap->header.w && py < (int32_t)snap->header.h) {
                    snap->data[(size_t)py * snap->header.stride + (size_t)px * 4 + 3] = 0;
                }
            }
        }
    }
}

/* A sheet: the ground where it is, then its baked rim (the bands along its edges and its
 * corners) over it. A moving sheet's snapshot is taken hollow: the rim alone, the ground drawn
 * under the snapshot wherever it is. */
static void sheet_draw(lv_layer_t *layer, const bake_t *b, const lv_area_t *area, int32_t radius)
{
    int32_t e = b->bez;
    int32_t k = b->corner;
    lv_area_t clip_was = layer->_clip_area;
    /* The bands top and bottom, the corners between them and the sides (what the bake has). */
    lv_area_t parts[8] = {
        {area->x1, area->y1, area->x2, area->y1 + e - 1},
        {area->x1, area->y2 - e + 1, area->x2, area->y2},
        {area->x1, area->y1 + e, area->x1 + k - 1, area->y1 + k - 1},
        {area->x2 - k + 1, area->y1 + e, area->x2, area->y1 + k - 1},
        {area->x1, area->y2 - k + 1, area->x1 + k - 1, area->y2 - e},
        {area->x2 - k + 1, area->y2 - k + 1, area->x2, area->y2 - e},
        {area->x1, area->y1 + k, area->x1 + e - 1, area->y2 - k},
        {area->x2 - e + 1, area->y1 + k, area->x2, area->y2 - k},
    };

    if (!s_hollow) {
        ui_glass_draw_sheet_ground(layer, area, radius);
    }
    for (int i = 0; i < 8; i++) {
        lv_draw_image_dsc_t d;

        if (!lv_area_intersect(&layer->_clip_area, &clip_was, &parts[i])) {
            continue;
        }
        lv_draw_image_dsc_init(&d);
        d.src = b->buf;
        d.image_area = *area;
        d.clip_radius = radius;
        lv_draw_image(layer, &d, area);
    }
    layer->_clip_area = clip_was;
}

/* Whether a glass object sits inside another glass object (its corners must show that glass,
 * not the ground). */
static bool nested_in_glass(lv_obj_t *obj, bool *material);

/* Whether an object sits on a page (its glass refracts the dimmed face) rather than over the
 * face. */
static bool on_page(lv_obj_t *obj);
static bool on_scrim(lv_obj_t *obj);

/* How an object's glass is baked: where it rests (an object gliding into place, translated,
 * keeps one image while it moves), whether its corners are baked against the ground (large, not
 * round, not inside other glass: drawn without LVGL's per-pixel corner mask), and its context. */
static void plan(const void *key, const lv_area_t *area, int32_t radius, ui_glass_kind_t kind, lv_area_t *rest,
                 bool *opaque, ui_glass_mode_t *mode, bool *deep)
{
    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);
    bool nested;

    *rest = *area;
    *opaque = kind != UI_GLASS_SHEET && radius * 2 < LV_MIN(w, h) - 2 && w * h <= 250000;
    *mode = s_mode;
    *deep = false;
    if (key == NULL) {
        return;
    }
    lv_area_move(rest, -lv_obj_get_style_translate_x((lv_obj_t *)key, 0),
                 -lv_obj_get_style_translate_y((lv_obj_t *)key, 0));
    /* Asked once per object: an existing bake already knows. */
    for (int i = 0; i < BAKE_MAX; i++) {
        if (s_bakes[i].buf != NULL && s_bakes[i].key == key && s_bakes[i].w == w && s_bakes[i].h == h) {
            *opaque = s_bakes[i].opaque;
            *mode = (ui_glass_mode_t)s_bakes[i].mode;
            *deep = s_bakes[i].deep;
            return;
        }
    }
    *mode = on_page((lv_obj_t *)key) ? UI_GLASS_PAGE : UI_GLASS_FACE;
    nested = nested_in_glass((lv_obj_t *)key, deep);
    *opaque = *opaque && !nested && !on_scrim((lv_obj_t *)key);
}

/* A bake can be made when the backdrop has what it needs: page glass from either back (the
 * bright face is dimmed as it is read), glass over the face only from the face itself. */
static bool can_bake(ui_glass_mode_t mode)
{
    return s_frost_ok && s_back_ok && (mode == UI_GLASS_PAGE || s_mode == UI_GLASS_FACE);
}

/* --- Face controls: smoked glass ---------------------------------------------------------- */

/* The controls over the face (its keys, pills and rim tabs) are smoked glass: a dark tint
 * over the face as it is, a rim highlight and a hairline edge, the same on every face. They
 * hold nothing of the face, so the faces slide under them as they are and nothing changes
 * when a slide stops; and white type and icons read on light and dark dials alike. Each shape
 * is drawn once into an ARGB image (a shell) and kept. */
#define SHELLS 24

typedef struct {
    const void *key;
    int16_t w, h, r;
    uint8_t kind;
    uint32_t tint;
    lv_draw_buf_t *img;
} shell_t;

static shell_t s_shells[SHELLS];
static int s_shell_next;

/* The smoke: the tint and how much, and the rim's highlight. */
static look_t control_look(ui_glass_kind_t kind, lv_color_t tint)
{
    look_t l = {0.0f, 0.50f, {20.0f, 22.0f, 26.0f}, 0.62f, 1.0f, 0.0f};

    switch (kind) {
    case UI_GLASS_TINT:
        l.tint[0] = tint.red;
        l.tint[1] = tint.green;
        l.tint[2] = tint.blue;
        l.tint_a = 0.78f;
        l.spec = 0.7f;
        break;
    case UI_GLASS_RAISED:
        l.tint_a = 0.46f;
        break;
    case UI_GLASS_WELL:
        l.tint_a = 0.60f;
        l.spec = 0.3f;
        break;
    default:
        break;
    }
    return l;
}

/* A shell pixel: the tint, then the highlight over whatever is behind, as one colour and alpha
 * (cov: how much of the pixel the shape covers; s: how far in from the edge). */
static uint32_t shell_px(const look_t *l, const bezel_t *bz, float s, float nx, float ny, float cov)
{
    float disp;
    float slope;
    float lit = LV_MAX(0.0f, -0.6f * nx - 0.8f * ny);
    float refl = LV_MAX(0.0f, 0.6f * nx + 0.8f * ny);
    float spec;
    float a;
    uint32_t c[3];

    if (cov <= 0.0f) {
        return 0;
    }
    bezel_at(bz, s, &disp, &slope);
    spec = slope == 0.0f ? 0.0f : l->spec * slope * slope * (0.25f + 0.85f * lit * lit + 0.45f * refl * refl);
    spec += l->spec * 0.35f * clampf(1.4f - s, 0.0f, 1.0f);
    spec = clampf(spec, 0.0f, 1.0f);
    a = 1.0f - (1.0f - l->tint_a) * (1.0f - spec);
    for (int k = 0; k < 3; k++) {
        c[k] = (uint32_t)clampf((l->tint[k] * l->tint_a * (1.0f - spec) + 235.0f * spec) / a, 0.0f, 255.0f);
    }
    return (uint32_t)lroundf(clampf(a * cov, 0.0f, 1.0f) * 255.0f) << 24 | c[0] << 16 | c[1] << 8 | c[2];
}

static lv_draw_buf_t *shell_make(int32_t w, int32_t h, int32_t r, ui_glass_kind_t kind, lv_color_t tint)
{
    look_t l = control_look(kind, tint);
    lv_draw_buf_t *buf = lv_draw_buf_create((uint32_t)w, (uint32_t)h, LV_COLOR_FORMAT_ARGB8888, LV_STRIDE_AUTO);
    bezel_t bz;
    float hx = (float)w / 2.0f;
    float hy = (float)h / 2.0f;

    if (buf == NULL) {
        return NULL;
    }
    bezel_for(&bz, w, h);
    for (int32_t y = 0; y < h; y++) {
        uint32_t *row = (uint32_t *)(buf->data + (size_t)y * buf->header.stride);

        for (int32_t x = 0; x < w; x++) {
            float nx;
            float ny;
            float d = rrect_sdf((float)x + 0.5f - hx, (float)y + 0.5f - hy, hx, hy, (float)r, &nx, &ny);

            row[x] = shell_px(&l, &bz, LV_MAX(0.0f, -d), nx, ny, clampf(0.5f - d, 0.0f, 1.0f));
        }
    }
    ui_sparse_image_register(buf);
    return buf;
}

static const lv_draw_buf_t *shell_get(const void *key, int32_t w, int32_t h, int32_t r, ui_glass_kind_t kind,
                                      lv_color_t tint)
{
    uint32_t t = kind == UI_GLASS_TINT ? lv_color_to_u32(tint) : 0;
    shell_t *e;

    for (int i = 0; i < SHELLS; i++) {
        e = &s_shells[i];
        if (e->img != NULL && e->key == key && e->w == w && e->h == h && e->r == r && e->kind == (uint8_t)kind &&
            e->tint == t) {
            return e->img;
        }
    }
    e = &s_shells[s_shell_next];
    s_shell_next = (s_shell_next + 1) % SHELLS;
    ui_glass_image_free(e->img);
    e->img = shell_make(w, h, r, kind, tint);
    e->key = key;
    e->w = (int16_t)w;
    e->h = (int16_t)h;
    e->r = (int16_t)r;
    e->kind = (uint8_t)kind;
    e->tint = t;
    return e->img;
}

static void shells_release(void)
{
    for (int i = 0; i < SHELLS; i++) {
        ui_glass_image_free(s_shells[i].img);
        s_shells[i].img = NULL;
    }
}

/* Whether an object is a face control: glass over the face itself, on nothing else of glass. */
static bool is_face_control(const void *key, ui_glass_kind_t kind, ui_glass_mode_t mode)
{
    bool material;

    return key != NULL && mode == UI_GLASS_FACE && kind != UI_GLASS_SHEET &&
           !nested_in_glass((lv_obj_t *)key, &material);
}

static void shell_draw(lv_layer_t *layer, const void *key, const lv_area_t *area, int32_t radius,
                       ui_glass_kind_t kind, lv_color_t tint)
{
    const lv_draw_buf_t *img = shell_get(key, lv_area_get_width(area), lv_area_get_height(area), radius, kind, tint);
    lv_draw_image_dsc_t d;

    if (img == NULL) {
        return;
    }
    lv_draw_image_dsc_init(&d);
    d.src = img;
    d.image_area = *area;
    lv_draw_image(layer, &d, area);
}

void ui_glass_draw(lv_layer_t *layer, const void *key, const lv_area_t *area, int32_t radius, ui_glass_kind_t kind,
                   lv_color_t tint)
{
    lv_area_t screen = {0, 0, SCREEN - 1, SCREEN - 1};
    lv_area_t rest;
    lv_area_t vis;
    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);
    bake_t *b = NULL;
    ui_glass_mode_t mode;
    bool opaque;
    bool deep;

    if (!lv_area_intersect(&vis, area, &screen)) {
        return;
    }
    radius = LV_MIN(radius, LV_MIN(w, h) / 2);
    plan(key, area, radius, kind, &rest, &opaque, &mode, &deep);
    if (is_face_control(key, kind, mode)) {
        shell_draw(layer, key, area, radius, kind, tint);
        return;
    }
    if (can_bake(mode)) {
        b = bake_get(key, &rest, radius, kind, tint, opaque, mode, deep);
    }
    if (b != NULL && kind == UI_GLASS_SHEET) {
        sheet_draw(layer, b, area, radius);
    } else if (b != NULL) {
        lv_draw_image_dsc_t d;

        lv_draw_image_dsc_init(&d);
        d.src = b->buf;
        d.image_area = *area;
        d.clip_radius = b->opaque ? 0 : radius;
        lv_draw_image(layer, &d, area);
    } else {
        lv_draw_rect_dsc_t d;

        lv_draw_rect_dsc_init(&d);
        d.bg_color = kind == UI_GLASS_TINT ? tint : lv_color_hex(0x1C1E22);
        d.bg_opa = 220;
        d.radius = radius;
        lv_draw_rect(layer, &d, area);
    }
}

void ui_glass_draw_ground(lv_layer_t *layer, const lv_area_t *area)
{
    if (s_back_ok && s_mode == UI_GLASS_PAGE && s_ground_fade < 1.0f && s_sharp_ok) {
        /* Coming up from the face: the face, then the ground over it, fading in. */
        lv_area_t screen = {0, 0, SCREEN - 1, SCREEN - 1};
        lv_area_t vis;
        lv_draw_image_dsc_t d;

        draw_view(layer, s_sharp, area, 0);
        if (lv_area_intersect(&vis, area, &screen)) {
            lv_draw_image_dsc_init(&d);
            d.src = view_of(s_back, &vis);
            d.image_area = vis;
            d.opa = (lv_opa_t)lroundf(s_ground_fade * 255.0f);
            lv_draw_image(layer, &d, &vis);
        }
        return;
    }
    if (s_back_ok && s_mode == UI_GLASS_PAGE) {
        draw_view(layer, s_back, area, 0);
    } else {
        lv_draw_rect_dsc_t d;

        lv_draw_rect_dsc_init(&d);
        d.bg_color = lv_color_black();
        lv_draw_rect(layer, &d, area);
    }
}

/* --- Arcs --------------------------------------------------------------------------------- */

/* Distance to a band along an arc with round ends (negative inside) and its outward normal.
 * Angles in radians, 0 = three o'clock, clockwise (screen y down). */
static float arc_sdf(float px, float py, float rm, float hw, float a0, float a1, float *nx, float *ny)
{
    float r = sqrtf(px * px + py * py);
    float th = atan2f(py, px);
    float mid = (a0 + a1) / 2.0f;
    float rel = th - mid;

    while (rel > 3.14159265f) {
        rel -= 6.2831853f;
    }
    while (rel < -3.14159265f) {
        rel += 6.2831853f;
    }
    if (fabsf(rel) <= (a1 - a0) / 2.0f) {
        float s = r >= rm ? 1.0f : -1.0f;

        *nx = r > 0 ? s * px / r : 0;
        *ny = r > 0 ? s * py / r : 0;
        return fabsf(r - rm) - hw;
    } else {
        float a = rel > 0 ? a1 : a0;
        float ex = px - rm * cosf(a);
        float ey = py - rm * sinf(a);
        float l = sqrtf(ex * ex + ey * ey);

        *nx = l > 0 ? ex / l : 0;
        *ny = l > 0 ? ey / l : 0;
        return l - hw;
    }
}

/* The colour of the band at screen point (x, y), and its coverage. */
static bool arc_pixel(int32_t x, int32_t y, int32_t cx, int32_t cy, float rm, float hw, float a0, float a1,
                      const bezel_t *bz, const look_t *l, float *rgb, float *coverage)
{
    float px = (float)x + 0.5f - (float)cx;
    float py = (float)y + 0.5f - (float)cy;
    float nx;
    float ny;
    float d = arc_sdf(px, py, rm, hw, a0, a1, &nx, &ny);

    if (d > 0.5f) {
        return false;
    }
    ilook_t il = ilook_of(l, s_mode);

    glass_pixel((float)x + 0.5f, (float)y + 0.5f, LV_MAX(0.0f, -d), nx, ny, bz, l, &il, rgb);
    *coverage = clampf(0.5f - d, 0.0f, 1.0f);
    return true;
}

static void arc_bounds(int32_t cx, int32_t cy, int32_t r_mid, int32_t width, float a0, float a1, lv_area_t *box)
{
    float r_out = (float)r_mid + (float)width / 2.0f + 2.0f;

    box->x1 = cx + (int32_t)floorf(r_out * cosf(a0));
    box->x2 = box->x1;
    box->y1 = cy + (int32_t)floorf(r_out * sinf(a0));
    box->y2 = box->y1;
    for (float a = a0; a <= a1 + 0.001f; a += (a1 - a0) / 32.0f) {
        for (int k = 0; k < 2; k++) {
            float rr = k == 0 ? r_out : (float)r_mid - (float)width / 2.0f - 2.0f;
            int32_t x = cx + (int32_t)floorf(rr * cosf(a));
            int32_t y = cy + (int32_t)floorf(rr * sinf(a));

            box->x1 = LV_MIN(box->x1, x - (int32_t)width / 2 - 2);
            box->x2 = LV_MAX(box->x2, x + (int32_t)width / 2 + 2);
            box->y1 = LV_MIN(box->y1, y - (int32_t)width / 2 - 2);
            box->y2 = LV_MAX(box->y2, y + (int32_t)width / 2 + 2);
        }
    }
}

lv_draw_buf_t *ui_glass_arc_shell(int32_t cx, int32_t cy, int32_t r_mid, int32_t width, float a0, float a1,
                                  ui_glass_kind_t kind, lv_color_t tint, lv_area_t *area)
{
    float r0 = a0 * 3.14159265f / 180.0f;
    float r1 = a1 * 3.14159265f / 180.0f;
    look_t l = control_look(kind, tint);
    bezel_t bz;
    lv_draw_buf_t *buf;
    lv_area_t box;

    bezel_build(&bz, (float)width * 0.42f, (float)width * 0.42f, 1.6f);
    arc_bounds(cx, cy, r_mid, width, r0, r1, &box);
    buf = lv_draw_buf_create((uint32_t)lv_area_get_width(&box), (uint32_t)lv_area_get_height(&box),
                             LV_COLOR_FORMAT_ARGB8888, LV_STRIDE_AUTO);
    if (buf == NULL) {
        return NULL;
    }
    for (int32_t y = box.y1; y <= box.y2; y++) {
        uint32_t *row = (uint32_t *)(buf->data + (size_t)(y - box.y1) * buf->header.stride);

        for (int32_t x = box.x1; x <= box.x2; x++) {
            float nx;
            float ny;
            float d = arc_sdf((float)x + 0.5f - (float)cx, (float)y + 0.5f - (float)cy, (float)r_mid,
                              (float)width / 2.0f, r0, r1, &nx, &ny);

            row[x - box.x1] = shell_px(&l, &bz, LV_MAX(0.0f, -d), nx, ny, clampf(0.5f - d, 0.0f, 1.0f));
        }
    }
    ui_sparse_image_register(buf);
    *area = box;
    return buf;
}

lv_draw_buf_t *ui_glass_arc_image(int32_t cx, int32_t cy, int32_t r_mid, int32_t width, float a0, float a1,
                                  ui_glass_kind_t kind, lv_color_t tint, lv_area_t *area)
{
    float r0 = a0 * 3.14159265f / 180.0f;
    float r1 = a1 * 3.14159265f / 180.0f;
    look_t l = look_of(kind, tint, s_mode);
    bezel_t bz;
    lv_draw_buf_t *buf;
    lv_area_t box;

    if (!s_frost_ok || !s_back_ok) {
        return NULL;
    }
    bezel_build(&bz, (float)width * 0.42f, (float)width * 0.42f, 1.6f);
    arc_bounds(cx, cy, r_mid, width, r0, r1, &box);
    buf = lv_draw_buf_create((uint32_t)lv_area_get_width(&box), (uint32_t)lv_area_get_height(&box),
                             LV_COLOR_FORMAT_ARGB8888, LV_STRIDE_AUTO);
    if (buf == NULL) {
        return NULL;
    }
    for (int32_t y = box.y1; y <= box.y2; y++) {
        uint32_t *row = (uint32_t *)(buf->data + (size_t)(y - box.y1) * buf->header.stride);

        for (int32_t x = box.x1; x <= box.x2; x++) {
            float rgb[3];
            float cov;

            if (!arc_pixel(x, y, cx, cy, (float)r_mid, (float)width / 2.0f, r0, r1, &bz, &l, rgb, &cov)) {
                row[x - box.x1] = 0;
                continue;
            }
            row[x - box.x1] = (uint32_t)lroundf(cov * 255.0f) << 24 |
                              (uint32_t)clampf(rgb[0], 0, 255) << 16 | (uint32_t)clampf(rgb[1], 0, 255) << 8 |
                              (uint32_t)clampf(rgb[2], 0, 255);
        }
    }
    ui_sparse_image_register(buf);
    *area = box;
    return buf;
}

void ui_glass_image_free(lv_draw_buf_t *image)
{
    if (image != NULL) {
        ui_sparse_image_unregister(image);
        lv_draw_buf_destroy(image);
    }
}

void ui_glass_arc_blend(lv_draw_buf_t *dst, int32_t dst_x, int32_t dst_y, int32_t cx, int32_t cy, int32_t r_mid,
                        int32_t width, float a0, float a1, ui_glass_kind_t kind, lv_color_t tint)
{
    float r0 = a0 * 3.14159265f / 180.0f;
    float r1 = a1 * 3.14159265f / 180.0f;
    look_t l = look_of(kind, tint, s_mode);
    bezel_t bz;
    lv_area_t box;

    if (!s_frost_ok || !s_back_ok || dst == NULL || dst->header.cf != LV_COLOR_FORMAT_RGB565) {
        return;
    }
    bezel_build(&bz, (float)width * 0.42f, (float)width * 0.42f, 1.6f);
    arc_bounds(cx, cy, r_mid, width, r0, r1, &box);
    for (int32_t y = box.y1; y <= box.y2; y++) {
        int32_t by = y - dst_y;
        uint16_t *row;

        if (by < 0 || by >= (int32_t)dst->header.h) {
            continue;
        }
        row = (uint16_t *)(dst->data + (size_t)by * dst->header.stride);
        for (int32_t x = box.x1; x <= box.x2; x++) {
            int32_t bx = x - dst_x;
            float rgb[3];
            float cov;
            uint16_t v;
            int d;

            if (bx < 0 || bx >= (int32_t)dst->header.w ||
                !arc_pixel(x, y, cx, cy, (float)r_mid, (float)width / 2.0f, r0, r1, &bz, &l, rgb, &cov)) {
                continue;
            }
            v = row[bx];
            d = s_bayer[y & 3][x & 3];
            rgb[0] = rgb[0] * cov + (float)((v >> 11) & 0x1F) * (255.0f / 31.0f) * (1.0f - cov);
            rgb[1] = rgb[1] * cov + (float)((v >> 5) & 0x3F) * (255.0f / 63.0f) * (1.0f - cov);
            rgb[2] = rgb[2] * cov + (float)(v & 0x1F) * (255.0f / 31.0f) * (1.0f - cov);
            row[bx] = (uint16_t)(LV_MIN(31, ((int)clampf(rgb[0], 0, 255) * 31 + d * 16) / 255) << 11 |
                                 LV_MIN(63, ((int)clampf(rgb[1], 0, 255) * 63 + d * 16) / 255) << 5 |
                                 LV_MIN(31, ((int)clampf(rgb[2], 0, 255) * 31 + d * 16) / 255));
        }
    }
}

void ui_glass_frost_blit(lv_draw_buf_t *dst, int32_t dst_x, int32_t dst_y)
{
    if (!s_frost_ok || dst == NULL || dst->header.cf != LV_COLOR_FORMAT_RGB565) {
        return;
    }
    for (int32_t by = 0; by < (int32_t)dst->header.h; by++) {
        int32_t y = by + dst_y;
        const uint16_t *in;
        uint16_t *out = (uint16_t *)(dst->data + (size_t)by * dst->header.stride);

        if (y < 0 || y >= SCREEN) {
            continue;
        }
        in = (const uint16_t *)(s_frost->data + (size_t)(y >> 1) * s_frost->header.stride);
        for (int32_t bx = 0; bx < (int32_t)dst->header.w; bx++) {
            out[bx] = in[LV_CLAMP(0, bx + dst_x, SCREEN - 1) >> 1];
        }
    }
}

/* --- Page entrance ----------------------------------------------------------------------- */

#define ENTER_MAX 24
#define ENTER_RISE 28      /* px each control rises */
#define ENTER_STAGGER 32   /* ms between one control and the next */
#define ENTER_MS 380       /* each control's own rise */
#define ENTER_GROUND_MS 260

static lv_obj_t *s_enter_root;
static lv_obj_t *s_enter_targets[ENTER_MAX];
static int s_enter_count;
static bool s_enter_ground;

/* Ease-out with a small overshoot. */
static float back_out(float x)
{
    float u = x - 1.0f;

    return 1.0f + 2.2f * u * u * u + 1.2f * u * u;
}

static void enter_apply(int32_t t)
{
    for (int i = 0; i < s_enter_count; i++) {
        float x = clampf(((float)t - (float)(LV_MIN(i, 8) * ENTER_STAGGER)) / (float)ENTER_MS, 0.0f, 1.0f);

        lv_obj_set_style_translate_y(s_enter_targets[i], (int32_t)lroundf((1.0f - back_out(x)) * ENTER_RISE), 0);
    }
    if (s_enter_ground) {
        s_ground_fade = clampf((float)t / (float)ENTER_GROUND_MS, 0.0f, 1.0f);
        s_ground_fade = s_ground_fade * s_ground_fade * (3.0f - 2.0f * s_ground_fade);
        /* Every page draws the ground, not only this one: another may already be over it. */
        lv_obj_invalidate(lv_obj_get_screen(s_enter_root));
    }
}

static void enter_anim_cb(void *var, int32_t value)
{
    LV_UNUSED(var);
    enter_apply(value);
}

static void enter_done_cb(lv_anim_t *a)
{
    LV_UNUSED(a);
    for (int i = 0; i < s_enter_count; i++) {
        lv_obj_set_style_translate_y(s_enter_targets[i], 0, 0);
    }
    if (s_ground_fade < 1.0f && s_enter_root != NULL) {
        lv_obj_invalidate(lv_obj_get_screen(s_enter_root));
    }
    s_ground_fade = 1.0f;
    s_enter_count = 0;
    s_enter_root = NULL;
}

void ui_glass_page_enter(lv_obj_t *root, lv_obj_t *const *targets, int count, bool from_face)
{
    lv_anim_t a;
    int32_t total;

    if (!ui_skin_is_glass() || root == NULL) {
        return;
    }
    if (s_enter_root != NULL) {
        lv_anim_delete(&s_enter_root, enter_anim_cb);
        enter_done_cb(NULL);
    }
    s_enter_root = root;
    s_enter_count = 0;
    for (int i = 0; i < count && s_enter_count < ENTER_MAX; i++) {
        if (targets[i] != NULL) {
            s_enter_targets[s_enter_count++] = targets[i];
        }
    }
    s_enter_ground = from_face && s_back_ok && s_sharp_ok && s_mode == UI_GLASS_PAGE;
    total = LV_MIN(s_enter_count - 1, 8) * ENTER_STAGGER + ENTER_MS;
    enter_apply(0);
    lv_anim_init(&a);
    lv_anim_set_var(&a, &s_enter_root);
    lv_anim_set_exec_cb(&a, enter_anim_cb);
    lv_anim_set_values(&a, 0, total);
    lv_anim_set_duration(&a, (uint32_t)total);
    lv_anim_set_path_cb(&a, lv_anim_path_linear);
    lv_anim_set_completed_cb(&a, enter_done_cb);
    lv_anim_start(&a);
}

/* --- The hook ----------------------------------------------------------------------------- */

typedef enum {
    MAT_NONE,
    MAT_GROUND,
    MAT_GLASS,
} material_t;

static material_t classify(lv_obj_t *obj, lv_color_t color, int32_t w, int32_t h, int32_t radius,
                           ui_glass_kind_t *kind)
{
    const ui_skin_t *sk = ui_skin();
    int32_t m = LV_MIN(w, h);

    if (lv_color_eq(color, sk->bg)) {
        if (w >= SCREEN - 40 && h >= SCREEN - 40) {
            return MAT_GROUND;
        }
        *kind = UI_GLASS_CLEAR;
        return MAT_GLASS;
    }
    if (lv_color_eq(color, sk->surface)) {
        *kind = UI_GLASS_CLEAR;
        return MAT_GLASS;
    }
    if (lv_color_eq(color, sk->raised)) {
        *kind = UI_GLASS_RAISED;
        return MAT_GLASS;
    }
    if (lv_color_eq(color, sk->well)) {
        *kind = UI_GLASS_WELL;
        return MAT_GLASS;
    }
    if (lv_color_eq(color, sk->sheet)) {
        *kind = UI_GLASS_SHEET;
        return MAT_GLASS;
    }
    /* Coloured keys and beads (accent buttons, row badges, day toggles) are tinted glass. */
    if (m >= 36 && (lv_obj_check_type(obj, &lv_button_class) || radius * 2 >= m - 2)) {
        *kind = UI_GLASS_TINT;
        return MAT_GLASS;
    }
    return MAT_NONE;
}

/* Whether obj sits on other glass; material: on a sheet or a large panel. */
static bool nested_in_glass(lv_obj_t *obj, bool *material)
{
    *material = false;
    for (lv_obj_t *p = lv_obj_get_parent(obj); p != NULL; p = lv_obj_get_parent(p)) {
        ui_glass_kind_t kind;
        lv_area_t c;
        int32_t w;
        int32_t h;

        if (lv_obj_get_style_bg_opa(p, LV_PART_MAIN) < LV_OPA_50) {
            continue;
        }
        lv_obj_get_coords(p, &c);
        w = lv_area_get_width(&c);
        h = lv_area_get_height(&c);
        if (classify(p, lv_obj_get_style_bg_color(p, LV_PART_MAIN), w, h,
                     LV_MIN(lv_obj_get_style_radius(p, LV_PART_MAIN), LV_MIN(w, h) / 2), &kind) == MAT_GLASS) {
            *material = kind == UI_GLASS_SHEET || w * h > 100000;
            return true;
        }
    }
    return false;
}

/* Whether obj sits on a scrim (a dialog's dim layer): its corners show the dim, not the
 * ground they would be baked against. */
static bool on_scrim(lv_obj_t *obj)
{
    for (lv_obj_t *p = lv_obj_get_parent(obj); p != NULL; p = lv_obj_get_parent(p)) {
        ui_glass_kind_t kind;
        lv_area_t c;

        if (lv_obj_get_style_bg_opa(p, LV_PART_MAIN) < LV_OPA_50) {
            continue;
        }
        lv_obj_get_coords(p, &c);
        return classify(p, lv_obj_get_style_bg_color(p, LV_PART_MAIN), lv_area_get_width(&c), lv_area_get_height(&c), 0,
                        &kind) == MAT_NONE;
    }
    return false;
}

static bool on_page(lv_obj_t *obj)
{
    for (lv_obj_t *p = lv_obj_get_parent(obj); p != NULL; p = lv_obj_get_parent(p)) {
        ui_glass_kind_t kind;
        lv_area_t c;

        if (lv_obj_get_style_bg_opa(p, LV_PART_MAIN) < LV_OPA_50) {
            continue;
        }
        lv_obj_get_coords(p, &c);
        if (classify(p, lv_obj_get_style_bg_color(p, LV_PART_MAIN), lv_area_get_width(&c), lv_area_get_height(&c), 0,
                     &kind) == MAT_GROUND) {
            return true;
        }
    }
    return false;
}

static void glass_draw_cb(lv_event_t *e);

static bool has_glass_cb(lv_obj_t *obj)
{
    uint32_t n = lv_obj_get_event_count(obj);

    for (uint32_t i = 0; i < n; i++) {
        if (lv_event_dsc_get_cb(lv_obj_get_event_dsc(obj, i)) == glass_draw_cb) {
            return true;
        }
    }
    return false;
}

/* Bakes obj's glass now if it would draw some and has none current. */
static void prewarm_one(lv_obj_t *obj)
{
    lv_area_t screen = {0, 0, SCREEN - 1, SCREEN - 1};
    ui_glass_kind_t kind = UI_GLASS_CLEAR;
    ui_glass_mode_t mode;
    lv_area_t coords;
    lv_area_t rest;
    lv_area_t vis;
    lv_color_t color;
    int32_t w;
    int32_t h;
    int32_t radius;
    bool opaque;
    bool deep;

    if (!has_glass_cb(obj) || lv_obj_get_style_bg_opa(obj, LV_PART_MAIN) < LV_OPA_50 ||
        lv_obj_get_style_bg_grad_dir(obj, LV_PART_MAIN) != LV_GRAD_DIR_NONE) {
        return;
    }
    lv_obj_get_coords(obj, &coords);
    w = lv_area_get_width(&coords);
    h = lv_area_get_height(&coords);
    if (w <= 0 || h <= 0 || !lv_area_intersect(&vis, &coords, &screen)) {
        return;
    }
    color = lv_obj_get_style_bg_color(obj, LV_PART_MAIN);
    radius = LV_MIN(lv_obj_get_style_radius(obj, LV_PART_MAIN), LV_MIN(w, h) / 2);
    if (classify(obj, color, w, h, radius, &kind) != MAT_GLASS) {
        return;
    }
    plan(obj, &coords, radius, kind, &rest, &opaque, &mode, &deep);
    if (is_face_control(obj, kind, mode)) {
        shell_get(obj, w, h, radius, kind, color);
        return;
    }
    /* Only into free room: a prewarm that evicted would bake round and round at idle. */
    if (can_bake(mode) && s_bake_bytes + (size_t)w * (size_t)h * 2 <= BAKE_BYTES) {
        bake_get(obj, &rest, radius, kind, color, opaque, mode, deep);
    }
}

bool ui_glass_prewarm(lv_obj_t *root, uint32_t budget_ms)
{
    uint32_t start = lv_tick_get();
    lv_obj_t *stack[64];
    int depth = 0;

    if (!ui_skin_is_glass() || root == NULL || !s_frost_ok || !s_back_ok) {
        return true;
    }
    if (s_mode == UI_GLASS_FACE && !sheet_ground_build(budget_ms)) {
        return false;
    }

    stack[depth++] = root;
    while (depth > 0) {
        lv_obj_t *obj = stack[--depth];
        uint32_t n = lv_obj_get_child_count(obj);

        prewarm_one(obj);
        if (lv_tick_elaps(start) >= budget_ms) {
            return false;
        }
        for (uint32_t i = n; i > 0 && depth < 64; i--) {
            stack[depth++] = lv_obj_get_child(obj, (int32_t)i - 1);
        }
    }
    return true;
}

/* Draws the object as glass instead of LVGL's background (and border: the rim replaces it). */
static void glass_draw_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_current_target(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    ui_glass_kind_t kind = UI_GLASS_CLEAR;
    lv_area_t coords;
    int32_t radius;
    int32_t w;
    int32_t h;

    if (!ui_skin_is_glass() || lv_obj_get_style_bg_opa(obj, LV_PART_MAIN) < LV_OPA_50 ||
        lv_obj_get_style_bg_grad_dir(obj, LV_PART_MAIN) != LV_GRAD_DIR_NONE) {
        return;
    }
    lv_obj_get_coords(obj, &coords);
    w = lv_area_get_width(&coords);
    h = lv_area_get_height(&coords);
    radius = LV_MIN(lv_obj_get_style_radius(obj, LV_PART_MAIN), LV_MIN(w, h) / 2);
    switch (classify(obj, lv_obj_get_style_bg_color(obj, LV_PART_MAIN), w, h, radius, &kind)) {
    case MAT_GROUND:
        ui_glass_draw_ground(layer, &coords);
        break;
    case MAT_GLASS:
        ui_glass_draw(layer, obj, &coords, radius, kind, lv_obj_get_style_bg_color(obj, LV_PART_MAIN));
        break;
    default:
        return;
    }
    lv_event_stop_processing(e);
}

static void child_created_cb(lv_event_t *e)
{
    lv_obj_t *child = lv_event_get_param(e);

    if (child != NULL && (lv_obj_check_type(child, &lv_obj_class) || lv_obj_check_type(child, &lv_button_class))) {
        lv_obj_add_event_cb(child, glass_draw_cb, (lv_event_code_t)(LV_EVENT_DRAW_MAIN | LV_EVENT_PREPROCESS), NULL);
    }
}

void ui_glass_attach(lv_obj_t *root)
{
    lv_obj_add_event_cb(root, glass_draw_cb, (lv_event_code_t)(LV_EVENT_DRAW_MAIN | LV_EVENT_PREPROCESS), NULL);
    lv_obj_add_event_cb(root, child_created_cb, LV_EVENT_CHILD_CREATED, NULL);
}

void ui_glass_debug_bake(int32_t w, int32_t h, int32_t radius, ui_glass_kind_t kind)
{
    static int s_keys[4];
    const void *key = &s_keys[(w * 7 + h) & 3];
    lv_area_t area = {110, 74, 110 + w - 1, 74 + h - 1};
    bake_t *b;

    if (!s_frost_ok || !s_back_ok) {
        return;
    }
    b = bake_get(key, &area, radius, kind, lv_color_black(), true, UI_GLASS_PAGE, false);
    if (b != NULL) {
        bake_render(b, &area, radius, kind, lv_color_black()); /* the bake alone, buffer reused */
    }
}
