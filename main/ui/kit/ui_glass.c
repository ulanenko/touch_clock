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
#define VIEWS 256

/* --- Backdrop ---------------------------------------------------------------------------- */

static lv_draw_buf_t *s_frost;      /* the backdrop blurred and graded, at half resolution (FROST) */
static lv_draw_buf_t *s_back;       /* what the glass refracts: the face (FACE), the dimmed face (PAGE) */
static const lv_draw_buf_t *s_sharp; /* the face the backdrop was built from (the screen scratch) */
static float s_ground_fade = 1.0f;   /* 0 the sharp face, 1 the dimmed ground */
static bool s_frost_ok;
static bool s_back_ok;
static bool s_sharp_ok;
static bool s_light;
static ui_glass_mode_t s_mode;
static uint32_t s_generation; /* counts changes to what the glass refracts over the face */
static uint32_t s_identity;   /* which face (and theme) the backdrop shows */

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

/* One box blur pass of radius r along rows (step 1) or columns (step LOW), in place. */
static void box_pass(uint8_t *ch, uint8_t *tmp, int r, bool rows)
{
    const int recip = (65536 + 2 * r) / (2 * r + 1); /* sum / (2r + 1) as a multiply */

    for (int line = 0; line < LOW; line++) {
        int sum = 0;

        for (int i = -r; i <= r; i++) {
            int k = LV_CLAMP(0, i, LOW - 1);

            sum += rows ? ch[line * LOW + k] : ch[k * LOW + line];
        }
        for (int i = 0; i < LOW; i++) {
            int add = LV_MIN(i + r + 1, LOW - 1);
            int sub = LV_MAX(i - r, 0);

            tmp[i] = (uint8_t)((sum * recip) >> 16);
            sum += rows ? ch[line * LOW + add] - ch[line * LOW + sub] : ch[add * LOW + line] - ch[sub * LOW + line];
        }
        for (int i = 0; i < LOW; i++) {
            if (rows) {
                ch[line * LOW + i] = tmp[i];
            } else {
                ch[i * LOW + line] = tmp[i];
            }
        }
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
    s_tables = true;
}

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

/* What the glass refracts: the face itself over the face, the face at 3/8 on pages (also
 * their ground). A copy: the snapshot's buffer is the screen scratch, soon reused. */
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
        /* v * 3/8 per channel on the packed pixel: half less an eighth, no carries. */
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

void ui_glass_backdrop_build(const lv_draw_buf_t *face, ui_glass_mode_t mode, bool frost_too)
{
    uint8_t *low = NULL;
    uint8_t *tmp = NULL;
    uint16_t *hrow = NULL;
    lv_draw_buf_t *frost;
    float gain;
    float lift;
    float sat;
    uint32_t luma = 0;
    int16_t x0[SCREEN];
    uint8_t wx[SCREEN];

    GLASS_PERF_START();

    if (s_frost == NULL) {
        s_frost = lv_draw_buf_create(FROST, FROST, LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
    }
    frost = s_frost;
    if (face == NULL || face->data == NULL || frost == NULL || face->header.cf != LV_COLOR_FORMAT_RGB565) {
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
    low = lv_malloc(LOW * LOW * 4);
    tmp = lv_malloc(LOW);
    hrow = lv_malloc(2 * SCREEN * 3 * sizeof(uint16_t));
    if (low == NULL || tmp == NULL || hrow == NULL) {
        lv_free(low);
        lv_free(tmp);
        lv_free(hrow);
        return;
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
            lv_free(tmp);
            lv_free(hrow);
            return;
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
    /* Three box passes each way (close to a Gaussian of about 13 px at full size), per channel. */
    {
        uint8_t *planes = lv_malloc(LOW * LOW);

        if (planes != NULL) {
            for (int c = 0; c < 4; c++) {
                for (int i = 0; i < LOW * LOW; i++) {
                    planes[i] = low[i * 4 + c];
                }
                for (int pass = 0; pass < 3; pass++) {
                    box_pass(planes, tmp, 3, true);
                    box_pass(planes, tmp, 3, false);
                }
                for (int i = 0; i < LOW * LOW; i++) {
                    low[i * 4 + c] = planes[i];
                }
            }
            lv_free(planes);
        }
    }
    GLASS_PERF_STEP("blur");
    /* Undo the weighting: colour over coverage. */
    for (int i = 0; i < LOW * LOW; i++) {
        int w = low[i * 4 + 3];

        for (int c = 0; c < 3; c++) {
            low[i * 3 + c] = w > 0 ? (uint8_t)LV_MIN(255, low[i * 4 + c] * 255 / w) : 0;
        }
    }
    for (int i = 0; i < LOW * LOW; i++) {
        luma += (uint32_t)(low[i * 3] * 54 + low[i * 3 + 1] * 183 + low[i * 3 + 2] * 19) >> 8;
    }
    /* Light: Sternglas and Modern Silver (white dials), not Avenir's orange. */
    s_light = luma / (LOW * LOW) > 165;
    if (s_light) {
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
    /* Up to half resolution bilinearly: each low row is interpolated across once (it serves
     * two output rows), then each pixel only blends two of them; dithered so gradients do not
     * band. */
    for (int x = 0; x < FROST; x++) {
        int f = (x * 2 + 1) * 64 - 128; /* ((x + 0.5) / 2 - 0.5) * 256 */
        int i = f < 0 ? 0 : f >> 8;

        x0[x] = (int16_t)LV_MIN(i, LOW - 2);
        wx[x] = (uint8_t)(f < 0 ? 0 : (i >= LOW - 1 ? 255 : (f & 255)));
    }
    {
        int cached[2] = {-1, -1};

        for (int y = 0; y < FROST; y++) {
            int f = (y * 2 + 1) * 64 - 128;
            int y0 = f < 0 ? 0 : LV_MIN(f >> 8, LOW - 2);
            int wy = f < 0 ? 0 : ((f >> 8) >= LOW - 1 ? 255 : (f & 255));
            uint16_t *out = (uint16_t *)(frost->data + (size_t)y * frost->header.stride);
            uint16_t *h0;
            uint16_t *h1;

            for (int k = 0; k < 2; k++) {
                int ly = y0 + k;
                int slot = ly & 1;

                if (cached[slot] != ly) {
                    const uint8_t *lr = low + (size_t)ly * LOW * 3;
                    uint16_t *h = hrow + (size_t)slot * FROST * 3;

                    for (int x = 0; x < FROST; x++) {
                        const uint8_t *a = lr + x0[x] * 3;
                        int w = wx[x];

                        h[x * 3] = (uint16_t)(a[0] * (256 - w) + a[3] * w);
                        h[x * 3 + 1] = (uint16_t)(a[1] * (256 - w) + a[4] * w);
                        h[x * 3 + 2] = (uint16_t)(a[2] * (256 - w) + a[5] * w);
                    }
                    cached[slot] = ly;
                }
            }
            h0 = hrow + (size_t)(y0 & 1) * FROST * 3;
            h1 = hrow + (size_t)((y0 + 1) & 1) * FROST * 3;
            for (int x = 0; x < FROST; x++) {
                int r = (h0[x * 3] * (256 - wy) + h1[x * 3] * wy) >> 16;
                int g = (h0[x * 3 + 1] * (256 - wy) + h1[x * 3 + 1] * wy) >> 16;
                int b = (h0[x * 3 + 2] * (256 - wy) + h1[x * 3 + 2] * wy) >> 16;

                out[x] = pack(r, g, b, x, y);
            }
        }
    }
    GLASS_PERF_STEP("up");
    lv_free(low);
    lv_free(tmp);
    lv_free(hrow);
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

static look_t look_of(ui_glass_kind_t kind, lv_color_t tint)
{
    bool face = s_mode == UI_GLASS_FACE;
    bool light = face && s_light;
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
        l.frost = 0.92f;
        l.tint_a = s_light && face ? 0.12f : 0.20f;
        if (s_light && face) {
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
static inline uint16_t fetch_frost(int32_t x, int32_t y)
{
    x = LV_CLAMP(0, x, SCREEN - 1) >> 1;
    y = LV_CLAMP(0, y, SCREEN - 1) >> 1;
    return ((const uint16_t *)(s_frost->data + (size_t)y * s_frost->header.stride))[x];
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
    int vib;   /* 256 = as is */
    int ta;    /* tint amount, 0..256 */
    int tint[3];
} ilook_t;

static ilook_t ilook_of(const look_t *l)
{
    ilook_t i = {(int)(l->frost * 256.0f), s_mode == UI_GLASS_PAGE ? 150 : 256, (int)(l->vibrance * 256.0f),
                 (int)(l->tint_a * 256.0f), {(int)l->tint[0], (int)l->tint[1], (int)l->tint[2]}};

    return i;
}

/* The glass body over backdrop colour b and frost colour f (8 bit): frosted, saturated,
 * tinted. */
static inline void body(const ilook_t *il, int br, int bg, int bb, int fr, int fg, int fb, int *c)
{
    int lum;

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
    lv_draw_buf_t *buf;
    uint32_t stamp;
    int16_t pend_x, pend_y;
    uint32_t pend_since;
    bool opaque; /* corners baked against the ground: drawn as a plain image */
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

static void bake_render(bake_t *b, const lv_area_t *area, int32_t radius, ui_glass_kind_t kind, lv_color_t tint)
{
    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);
    float hx = (float)w / 2.0f;
    float hy = (float)h / 2.0f;
    look_t l = look_of(kind, tint);
    ilook_t il = ilook_of(&l);
    bezel_t bz;
    float rr = (float)radius;
    GLASS_PERF_START();

    bezel_for(&bz, w, h);
    for (int32_t y = 0; y < h; y++) {
        uint16_t *row = (uint16_t *)(b->buf->data + (size_t)y * b->buf->header.stride);
        int32_t sy = area->y1 + y;
        float dye = (float)LV_MIN(y, h - 1 - y) + 0.5f;
        const uint16_t *brow = (const uint16_t *)(s_back->data + (size_t)LV_CLAMP(0, sy, SCREEN - 1) * s_back->header.stride);
        const uint16_t *frow = (const uint16_t *)(s_frost->data + (size_t)(LV_CLAMP(0, sy, SCREEN - 1) >> 1) * s_frost->header.stride);

        for (int32_t x = 0; x < w; x++) {
            int32_t sx = area->x1 + x;
            int32_t cx = LV_CLAMP(0, sx, SCREEN - 1);
            float dxe = (float)LV_MIN(x, w - 1 - x) + 0.5f;
            float d;
            float nx;
            float ny;
            float c[3];

            if (dxe >= bz.bezel && dye >= bz.bezel && (dxe >= rr || dye >= rr)) {
                /* The flat top, away from the bezel: integers only (most of every shape). */
                uint16_t vb = brow[cx];
                uint16_t vf = frow[cx >> 1];
                int ci[3];

                body(&il, s_e5[vb >> 11], s_e6[(vb >> 5) & 63], s_e5[vb & 31], s_e5[vf >> 11], s_e6[(vf >> 5) & 63],
                     s_e5[vf & 31], ci);
                row[x] = pack(ci[0], ci[1], ci[2], sx, sy);
                continue;
            }
            if (dxe < rr && dye < rr) {
                d = rrect_sdf((float)x + 0.5f - hx, (float)y + 0.5f - hy, hx, hy, rr, &nx, &ny);
            } else if (dxe < dye) {
                d = -dxe;
                nx = x < w / 2 ? -1.0f : 1.0f;
                ny = 0.0f;
            } else {
                d = -dye;
                nx = 0.0f;
                ny = y < h / 2 ? -1.0f : 1.0f;
            }
            if (d >= 0.5f) {
                /* Outside the shape: the ground (or anything, where the corners are clipped). */
                row[x] = brow[cx];
                continue;
            }
            glass_pixel((float)sx + 0.5f, (float)sy + 0.5f, LV_MAX(0.0f, -d), nx, ny, &bz, &l, &il, c);
            if (d > -0.5f && b->opaque) {
                /* The antialiased edge over the ground. */
                uint16_t g = brow[cx];
                float cov = 0.5f - d;

                c[0] = c[0] * cov + (float)s_e5[g >> 11] * (1.0f - cov);
                c[1] = c[1] * cov + (float)s_e6[(g >> 5) & 63] * (1.0f - cov);
                c[2] = c[2] * cov + (float)s_e5[g & 31] * (1.0f - cov);
            }
            row[x] = pack((int)c[0], (int)c[1], (int)c[2], sx, sy);
        }
    }
    b->x = (int16_t)area->x1;
    b->y = (int16_t)area->y1;
    b->gen = s_generation;
    b->identity = s_identity;
    b->pend_x = b->x;
    b->pend_y = b->y;
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

static bake_t *bake_get(const void *key, const lv_area_t *area, int32_t radius, ui_glass_kind_t kind, lv_color_t tint,
                        bool opaque)
{
    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);
    uint32_t t = kind == UI_GLASS_TINT ? lv_color_to_u32(tint) : 0;
    size_t need = (size_t)w * (size_t)h * 2;
    bake_t *slot = NULL;

    for (int i = 0; i < BAKE_MAX; i++) {
        bake_t *b = &s_bakes[i];

        if (b->buf == NULL || b->key != key || b->w != w || b->h != h || b->r != radius || b->kind != kind ||
            b->tint != t || b->opaque != opaque || b->mode != (uint8_t)s_mode) {
            continue;
        }
        b->stamp = ++s_bake_stamp;
        /* Pages, sheets and large panels are frosted enough that a minute's change behind does
         * not show: they follow the face, not each new backdrop. */
        if ((s_mode == UI_GLASS_PAGE || kind == UI_GLASS_SHEET || w * h > 100000) ? b->identity != s_identity
                                                                                 : b->gen != s_generation) {
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
    slot->mode = (uint8_t)s_mode;
    slot->stamp = ++s_bake_stamp;
    bake_render(slot, area, radius, kind, tint);
    return slot;
}

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

/* Whether a glass object sits inside another glass object (its corners must show that glass,
 * not the ground). */
static bool nested_in_glass(lv_obj_t *obj);

void ui_glass_draw(lv_layer_t *layer, const void *key, const lv_area_t *area, int32_t radius, ui_glass_kind_t kind,
                   lv_color_t tint)
{
    lv_area_t screen = {0, 0, SCREEN - 1, SCREEN - 1};
    lv_area_t rest = *area;
    lv_area_t vis;
    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);
    bake_t *b = NULL;
    bool opaque;

    if (!lv_area_intersect(&vis, area, &screen)) {
        return;
    }
    radius = LV_MIN(radius, LV_MIN(w, h) / 2);
    /* Baked where it rests: an object gliding into place (the page entrance, the controls'
     * sweep) is translated, and keeps one image while it moves. */
    if (key != NULL) {
        lv_area_move(&rest, -lv_obj_get_style_translate_x((lv_obj_t *)key, 0),
                     -lv_obj_get_style_translate_y((lv_obj_t *)key, 0));
    }
    /* Large, not round, on the ground: corners baked against the ground, drawn without a
     * corner mask (the mask costs per pixel per frame). Round keys and beads, sheets and glass
     * inside glass keep the mask. */
    opaque = kind != UI_GLASS_SHEET && radius * 2 < LV_MIN(w, h) - 2 && w * h <= 250000 && s_back_ok;
    if (opaque && key != NULL) {
        /* Asked once per object: an existing bake already knows. */
        bool known = false;

        for (int i = 0; i < BAKE_MAX && !known; i++) {
            if (s_bakes[i].buf != NULL && s_bakes[i].key == key && s_bakes[i].w == w && s_bakes[i].h == h) {
                opaque = s_bakes[i].opaque;
                known = true;
            }
        }
        if (!known) {
            opaque = !nested_in_glass((lv_obj_t *)key);
        }
    }
    if (s_frost_ok && s_back_ok) {
        b = bake_get(key, &rest, radius, kind, tint, opaque);
    }
    if (b != NULL) {
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
    ilook_t il = ilook_of(l);

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

lv_draw_buf_t *ui_glass_arc_image(int32_t cx, int32_t cy, int32_t r_mid, int32_t width, float a0, float a1,
                                  ui_glass_kind_t kind, lv_color_t tint, lv_area_t *area)
{
    float r0 = a0 * 3.14159265f / 180.0f;
    float r1 = a1 * 3.14159265f / 180.0f;
    look_t l = look_of(kind, tint);
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
    look_t l = look_of(kind, tint);
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
        lv_obj_invalidate(s_enter_root);
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

static bool nested_in_glass(lv_obj_t *obj)
{
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
            return true;
        }
    }
    return false;
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
