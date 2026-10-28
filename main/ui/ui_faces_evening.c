/*
 * The evening faces: dim, low-glare faces for the bedside (design: the "Evening clock faces"
 * canvas). Each is one RGB565 canvas over a static layer:
 *
 *   Ember     red-shifted analog, no numerals, no seconds.
 *   Nocturne  today's moon over a faint star field and a low hill; the time in a light serif.
 *   Filament  four dark nixie tubes with the cathode digits as faint outlines.
 *
 * The static layer (base) is drawn once per theme and day. A tick restores the base only
 * where something moves (face_hands_begin) and draws there: Filament's colon every second,
 * everything else once a minute. Shapes are drawn
 * per pixel with antialiased distances; text with LVGL's label drawing.
 */
#include "ui/clock_ui_private.h"
#include "ui/ui_areas.h"
#include "ui/ui_format.h"
#include "ui/kit/ui_kit.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "assets/skin_fonts/skin_fonts.h"
#include "localization.h"

#define EV_CENTER (SCREEN_SIZE / 2)
#define EV_PI 3.14159265f

/* --- Pixels ------------------------------------------------------------------------------- */

typedef struct {
    lv_draw_buf_t *buf;
    lv_area_t clip; /* screen coordinates, inclusive */
} ev_target_t;

static float ev_clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Blends colour c at coverage a (0..1) into one RGB565 pixel. */
static void ev_px(const ev_target_t *t, int32_t x, int32_t y, uint32_t c, float a)
{
    uint16_t *p;
    uint32_t m;
    uint32_t inv;
    uint32_t v;

    if (a <= 0.0f || x < t->clip.x1 || x > t->clip.x2 || y < t->clip.y1 || y > t->clip.y2) {
        return;
    }
    p = (uint16_t *)(t->buf->data + (size_t)y * t->buf->header.stride) + x;
    m = (uint32_t)(ev_clampf(a, 0.0f, 1.0f) * 255.0f + 0.5f);
    if (m == 0) {
        return;
    }
    if (m == 255) {
        *p = (uint16_t)(((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x001F));
        return;
    }
    inv = 255 - m;
    v = *p;
    *p = (uint16_t)(((((c >> 19) & 0x1F) * m + (v >> 11) * inv + 127) / 255) << 11 |
                    ((((c >> 10) & 0x3F) * m + ((v >> 5) & 0x3F) * inv + 127) / 255) << 5 |
                    ((((c >> 3) & 0x1F) * m + (v & 0x1F) * inv + 127) / 255));
}

static void ev_fill(const ev_target_t *t, uint32_t c)
{
    uint16_t v = (uint16_t)(((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x001F));

    for (int32_t y = t->clip.y1; y <= t->clip.y2; y++) {
        uint16_t *row = (uint16_t *)(t->buf->data + (size_t)y * t->buf->header.stride);

        for (int32_t x = t->clip.x1; x <= t->clip.x2; x++) {
            row[x] = v;
        }
    }
}

/* Iterates the pixels of a box (clipped to the target), pixel centres in px, py. */
#define EV_FOR_BOX(tgt, bx0, by0, bx1, by1)                                                                     \
    for (int32_t y = LV_MAX((tgt)->clip.y1, (int32_t)floorf(by0)); y <= LV_MIN((tgt)->clip.y2, (int32_t)ceilf(by1)); \
         y++)                                                                                                       \
        for (int32_t x = LV_MAX((tgt)->clip.x1, (int32_t)floorf(bx0)); x <= LV_MIN((tgt)->clip.x2, (int32_t)ceilf(bx1)); \
             x++)

static void ev_disc(const ev_target_t *t, float cx, float cy, float r, uint32_t c, float opa)
{
    EV_FOR_BOX(t, cx - r - 1, cy - r - 1, cx + r + 1, cy + r + 1)
    {
        float d = hypotf((float)x + 0.5f - cx, (float)y + 0.5f - cy);

        if (r < 1.0f) {
            /* A sub-pixel star: its area, spread over the pixels it touches. */
            ev_px(t, x, y, c, opa * ev_clampf(1.0f - d, 0.0f, 1.0f) * r * r * EV_PI);
        } else {
            ev_px(t, x, y, c, opa * ev_clampf(r + 0.5f - d, 0.0f, 1.0f));
        }
    }
}

static void ev_ring(const ev_target_t *t, float cx, float cy, float r, float w, uint32_t c, float opa)
{
    EV_FOR_BOX(t, cx - r - w, cy - r - w, cx + r + w, cy + r + w)
    {
        float d = hypotf((float)x + 0.5f - cx, (float)y + 0.5f - cy);

        ev_px(t, x, y, c, opa * ev_clampf(w / 2.0f + 0.5f - fabsf(d - r), 0.0f, 1.0f));
    }
}

/* A bar from a to b, w wide, with round or square (butt) ends. */
static void ev_bar(const ev_target_t *t, float ax, float ay, float bx, float by, float w, bool round, uint32_t c)
{
    float dx = bx - ax;
    float dy = by - ay;
    float len = hypotf(dx, dy);
    float ux = len > 0.0f ? dx / len : 0.0f;
    float uy = len > 0.0f ? dy / len : 0.0f;
    float pad = w / 2.0f + 1.0f;

    EV_FOR_BOX(t, LV_MIN(ax, bx) - pad, LV_MIN(ay, by) - pad, LV_MAX(ax, bx) + pad, LV_MAX(ay, by) + pad)
    {
        float px = (float)x + 0.5f - ax;
        float py = (float)y + 0.5f - ay;
        float along = px * ux + py * uy;
        float across = fabsf(px * uy - py * ux);
        float a;

        if (round) {
            float tcl = ev_clampf(along, 0.0f, len);
            float d = hypotf(px - tcl * ux, py - tcl * uy);

            a = ev_clampf(w / 2.0f + 0.5f - d, 0.0f, 1.0f);
        } else {
            a = ev_clampf(w / 2.0f + 0.5f - across, 0.0f, 1.0f) *
                ev_clampf(LV_MIN(along, len - along) + 0.5f, 0.0f, 1.0f);
        }
        ev_px(t, x, y, c, a);
    }
}

/* Point at angle deg (0 = twelve o'clock, clockwise) and radius r from the centre. */
static void ev_polar(float deg, float r, float *x, float *y)
{
    float a = deg * EV_PI / 180.0f;

    *x = (float)EV_CENTER + r * sinf(a);
    *y = (float)EV_CENTER - r * cosf(a);
}

/* Text centred on x, its baseline at y. */
static void ev_text(lv_layer_t *layer, const lv_font_t *font, uint32_t c, int32_t letter_space, const char *text,
                    int32_t cx, int32_t baseline)
{
    lv_draw_label_dsc_t d;
    int32_t top = baseline - (lv_font_get_line_height(font) - font->base_line);
    lv_area_t a = {cx - EV_CENTER, top, cx + EV_CENTER - 1, top + lv_font_get_line_height(font) - 1};

    lv_draw_label_dsc_init(&d);
    d.font = font;
    d.color = lv_color_hex(c);
    d.text = text;
    d.text_local = 1;
    d.letter_space = letter_space;
    d.align = LV_TEXT_ALIGN_CENTER;
    lv_draw_label(layer, &d, &a);
}

/* --- Common ---------------------------------------------------------------------------- */

static uint8_t ev_theme(const clock_ui_context_t *ctx, clock_face_id_t face)
{
    uint8_t theme = ctx->settings->face_themes[face];

    return theme < CLOCK_FACE_THEME_COUNT ? theme : 0;
}

static ui_evening_face_t *ev_state(clock_ui_context_t *ctx, clock_face_id_t face)
{
    switch (face) {
    case CLOCK_FACE_EMBER:
        return &ctx->faces.ember;
    case CLOCK_FACE_NOCTURNE:
        return &ctx->faces.nocturne;
    case CLOCK_FACE_FILAMENT:
        return &ctx->faces.filament;
    default:
        return NULL;
    }
}

static void ev_create(clock_ui_context_t *ctx, clock_face_id_t face, lv_obj_t *parent, uint32_t ground)
{
    ui_evening_face_t *ev = ev_state(ctx, face);

    lv_obj_set_style_bg_color(parent, lv_color_hex(ground), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    ev->base_key = UINT32_MAX;
    ev->shown_min = -1;
    ev->shown_sec = -1;
    if (ctx->faces.evening_base_buf == NULL) {
        ctx->faces.evening_owner = CLOCK_FACE_COUNT;
    }
    face_hands_create(&ev->view, parent);
}

/* Draws into the base layer with LVGL (text); the base must exist. */
static lv_obj_t *ev_base_canvas(clock_ui_context_t *ctx, clock_face_id_t face, lv_layer_t *layer)
{
    lv_obj_t *canvas = lv_canvas_create(ctx->tiles[face]);

    lv_obj_add_flag(canvas, LV_OBJ_FLAG_HIDDEN);
    lv_canvas_set_draw_buf(canvas, ev_state(ctx, face)->base);
    lv_canvas_init_layer(canvas, layer);
    return canvas;
}

static void ev_base_canvas_done(lv_obj_t *canvas, lv_layer_t *layer)
{
    lv_canvas_finish_layer(canvas, layer);
    lv_obj_delete(canvas);
}

/* Makes sure the shared base belongs to this face and is current for key; true when it has
 * to be (re)drawn. Taking the buffers over from another evening face leaves that face to
 * repaint in full when it next shows. */
static bool ev_base_begin(clock_ui_context_t *ctx, clock_face_id_t face, uint32_t key, ev_target_t *t)
{
    ui_evening_face_t *ev = ev_state(ctx, face);

    if (ctx->faces.evening_base_buf == NULL) {
        ctx->faces.evening_base_buf =
            lv_draw_buf_create(SCREEN_SIZE, SCREEN_SIZE, LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
        if (ctx->faces.evening_base_buf == NULL) {
            return false;
        }
    }
    ev->base = ctx->faces.evening_base_buf;
    if (ctx->faces.evening_owner != face) {
        ui_evening_face_t *previous = ev_state(ctx, ctx->faces.evening_owner);

        if (previous != NULL) {
            previous->base_key = UINT32_MAX;
            previous->view.valid = false;
        }
        ctx->faces.evening_owner = face;
        ev->base_key = UINT32_MAX;
    }
    if (ev->base_key == key) {
        return false;
    }
    ev->base_key = key;
    ev->view.valid = false; /* repaint everything over the new base */
    t->buf = ev->base;
    t->clip = (lv_area_t) {0, 0, SCREEN_SIZE - 1, SCREEN_SIZE - 1};
    return true;
}

/* The upper-case form of a localised word. */
static void ev_upper(char *out, size_t size, const char *text)
{
    kit_text_upper(out, size, text);
}

/* --- Ember: red-shifted minimal analog ------------------------------------------------- */

/* Palettes are the design's colours snapped to the nearest RGB565 colour by perceived
 * difference (CIE Lab): truncated, near-blacks this dark turn green or teal on the panel. */
typedef struct {
    uint32_t ground, min_tick, hour_tick, hour_hand, min_hand, accent, date, hub, ink;
} ember_palette_t;

static ember_palette_t ember_palette(uint8_t theme)
{
    static const ember_palette_t themes[CLOCK_FACE_THEME_COUNT] = {
        {0x080808, 0x301408, 0x682818, 0x803420, 0xA84428, 0xC85830, 0x783420, 0x180400, 0x883C28},
        {0x080408, 0x301820, 0x602C40, 0x703448, 0x984C68, 0xB85878, 0x783850, 0x180810, 0x884460},
        {0x080400, 0x302008, 0x604418, 0x705018, 0xA07428, 0xB88428, 0x785820, 0x181008, 0x886828},
    };

    return themes[theme < CLOCK_FACE_THEME_COUNT ? theme : 0];
}

static void ember_base(clock_ui_context_t *ctx, const ember_palette_t *p, const struct tm *ti)
{
    ui_evening_face_t *ev = &ctx->faces.ember;
    ev_target_t t = {ev->base, {0, 0, SCREEN_SIZE - 1, SCREEN_SIZE - 1}};
    lv_layer_t layer;
    lv_obj_t *canvas;
    char date[32];
    char upper[48];

    ev_fill(&t, p->ground);
    /* 60 minute ticks (10 px, 2 wide), then the 12 hour ticks over them (26 px, 4 wide). */
    for (int i = 0; i < 60; i++) {
        float x0, y0, x1, y1;

        ev_polar((float)i * 6.0f, 321.0f, &x0, &y0);
        ev_polar((float)i * 6.0f, 331.0f, &x1, &y1);
        ev_bar(&t, x0, y0, x1, y1, 2.0f, false, p->min_tick);
    }
    for (int i = 0; i < 12; i++) {
        float x0, y0, x1, y1;

        ev_polar((float)i * 30.0f, 305.0f, &x0, &y0);
        ev_polar((float)i * 30.0f, 331.0f, &x1, &y1);
        ev_bar(&t, x0, y0, x1, y1, 4.0f, false, p->hour_tick);
    }
    ev_disc(&t, (float)EV_CENTER, 76.0f, 4.0f, p->hour_tick, 1.0f);

    snprintf(date, sizeof(date), "%s %d", l10n_day_short((clock_language_t)ctx->settings->language, ti->tm_wday),
             ti->tm_mday);
    ev_upper(upper, sizeof(upper), date);
    canvas = ev_base_canvas(ctx, CLOCK_FACE_EMBER, &layer);
    ev_text(&layer, &ev_sans_16, p->date, 5, upper, EV_CENTER, 222);
    ev_base_canvas_done(canvas, &layer);
}

static void ember_draw(const ev_target_t *t, const ember_palette_t *p, const struct tm *ti)
{
    float hour_deg = (float)(ti->tm_hour % 12) * 30.0f + (float)ti->tm_min * 0.5f;
    float min_deg = (float)ti->tm_min * 6.0f;
    float x0, y0, x1, y1;

    /* Hour hand (10 wide, 170 long, a 20 px tail), minute hand (6 wide, 286, 30), hub. */
    ev_polar(hour_deg + 180.0f, 20.0f, &x0, &y0);
    ev_polar(hour_deg, 170.0f, &x1, &y1);
    ev_bar(t, x0, y0, x1, y1, 10.0f, true, p->hour_hand);
    ev_polar(min_deg + 180.0f, 30.0f, &x0, &y0);
    ev_polar(min_deg, 286.0f, &x1, &y1);
    ev_bar(t, x0, y0, x1, y1, 6.0f, true, p->min_hand);
    ev_disc(t, (float)EV_CENTER, (float)EV_CENTER, 9.0f, p->hub, 1.0f);
    ev_ring(t, (float)EV_CENTER, (float)EV_CENTER, 9.0f, 2.0f, p->hour_hand, 1.0f);
}

void create_ember_face(clock_ui_context_t *ctx, lv_obj_t *parent)
{
    ev_create(ctx, CLOCK_FACE_EMBER, parent, ember_palette(ev_theme(ctx, CLOCK_FACE_EMBER)).ground);
}

static void update_ember(clock_ui_context_t *ctx)
{
    ui_evening_face_t *ev = &ctx->faces.ember;
    uint8_t theme = ev_theme(ctx, CLOCK_FACE_EMBER);
    ember_palette_t p = ember_palette(theme);
    struct tm ti = get_local_time_now();
    int16_t minute = (int16_t)(ti.tm_hour * 60 + ti.tm_min);
    ev_target_t t;
    lv_layer_t layer;
    lv_area_t dirty;
    lv_area_t box = {0, 0, SCREEN_SIZE - 1, SCREEN_SIZE - 1};

    if (ev_base_begin(ctx, CLOCK_FACE_EMBER, (uint32_t)theme << 16 | (uint32_t)ti.tm_yday, &t)) {
        lv_obj_set_style_bg_color(ctx->tiles[CLOCK_FACE_EMBER], lv_color_hex(p.ground), 0);
        ember_base(ctx, &p, &ti);
    }
    if (ev->base == NULL || (ev->view.valid && minute == ev->shown_min)) {
        return; /* no seconds: the hands move once a minute */
    }
    ev->view.valid = false;
    if (!face_hands_begin(&ev->view, ev->base, box, 0, 0, 0, &layer, &dirty)) {
        return;
    }
    t.buf = ev->view.buf;
    t.clip = dirty;
    ember_draw(&t, &p, &ti);
    face_hands_finish(&ev->view, NULL, &layer, dirty, 0);
    ev->shown_min = minute;
}

/* --- Nocturne: the moon over a dim night sky ------------------------------------------- */

typedef struct {
    uint32_t sky, stars, moon, time, labels, phase, shadow, hill, hill_line, ink;
    float hill_width;
} nocturne_palette_t;

static nocturne_palette_t nocturne_palette(uint8_t theme)
{
    static const nocturne_palette_t themes[CLOCK_FACE_THEME_COUNT] = {
        {0x080810, 0x687490, 0xA09C88, 0x9094A8, 0x586078, 0x485468, 0x101018, 0x000408, 0x101828, 0x788098, 1.0f},
        {0x080808, 0x786850, 0xB09870, 0x988468, 0x685840, 0x605040, 0x181410, 0x000000, 0x181008, 0x887860, 1.5f},
        {0x000808, 0x507470, 0x90A8A0, 0x789890, 0x506460, 0x485C58, 0x101818, 0x000000, 0x102020, 0x708C88, 1.5f},
    };

    return themes[theme < CLOCK_FACE_THEME_COUNT ? theme : 0];
}

/* The design's star field: x, y, radius, opacity. */
static const float s_stars[][4] = {
    {118, 210, 1.2f, 0.5f},  {160, 140, 0.9f, 0.4f}, {212, 96, 1.4f, 0.55f}, {268, 176, 0.8f, 0.35f},
    {300, 72, 1.1f, 0.45f},  {436, 88, 0.9f, 0.4f},  {474, 168, 1.3f, 0.5f}, {520, 104, 0.8f, 0.35f},
    {566, 196, 1.1f, 0.45f}, {604, 250, 0.9f, 0.4f}, {636, 330, 1.2f, 0.35f}, {96, 300, 0.9f, 0.35f},
    {70, 390, 1.1f, 0.3f},   {132, 462, 0.8f, 0.3f}, {182, 252, 1.0f, 0.3f}, {250, 122, 0.7f, 0.3f},
    {400, 40, 0.9f, 0.35f},  {330, 104, 0.7f, 0.3f}, {596, 424, 0.9f, 0.3f}, {650, 462, 0.8f, 0.25f},
    {110, 540, 0.9f, 0.25f}, {612, 520, 1.0f, 0.25f}, {560, 556, 0.7f, 0.25f}, {170, 560, 0.8f, 0.22f},
    {50, 470, 0.7f, 0.25f},  {680, 390, 0.8f, 0.3f}, {508, 236, 0.7f, 0.3f}, {222, 210, 0.7f, 0.3f},
    {150, 350, 0.6f, 0.25f}, {584, 300, 0.7f, 0.3f}, {88, 232, 0.6f, 0.3f},  {640, 232, 0.7f, 0.3f},
    {190, 64, 0.6f, 0.25f},  {372, 60, 0.6f, 0.3f},
};

static const ui_l10n_str_t s_phases[8] = {
    UI_L10N_STR("New moon", "Nów", "Молодик", "Новолуние", "Nieuwe maan"),
    UI_L10N_STR("Waxing crescent", "Przybywający sierp", "Молодий місяць", "Растущий серп", "Wassende sikkel"),
    UI_L10N_STR("First quarter", "Pierwsza kwadra", "Перша чверть", "Первая четверть", "Eerste kwartier"),
    UI_L10N_STR("Waxing gibbous", "Przybywający garb", "Прибуваючий місяць", "Растущая луна", "Wassende maan"),
    UI_L10N_STR("Full moon", "Pełnia", "Повня", "Полнолуние", "Volle maan"),
    UI_L10N_STR("Waning gibbous", "Ubywający garb", "Спадаючий місяць", "Убывающая луна", "Afnemende maan"),
    UI_L10N_STR("Last quarter", "Ostatnia kwadra", "Остання чверть", "Последняя четверть", "Laatste kwartier"),
    UI_L10N_STR("Waning crescent", "Ubywający sierp", "Старий місяць", "Убывающий серп", "Afnemende sikkel"),
};

#define MOON_X 360.0f
#define MOON_Y 150.0f
#define MOON_R 38.0f

/* Moon age as a fraction of the synodic month (0 new, 0.5 full), from a known new moon. */
static float moon_phase(time_t now)
{
    const double new_moon = 947182440.0; /* 2000-01-06 18:14 UTC */
    const double synodic = 29.530588853 * 86400.0;
    double f = fmod(((double)now - new_moon) / synodic, 1.0);

    return (float)(f < 0.0 ? f + 1.0 : f);
}

/* The moon as a lit sphere with the near side's real features (north up, as seen from the
 * northern hemisphere). Positions are in units of the moon's radius, x right, y down. */
typedef struct {
    float x, y, rx, ry, depth;
} moon_mare_t;

static const moon_mare_t s_maria[] = {
    {-0.56f, -0.02f, 0.27f, 0.46f, 0.34f}, /* Oceanus Procellarum */
    {-0.30f, -0.42f, 0.25f, 0.20f, 0.38f}, /* Imbrium */
    {0.18f, -0.38f, 0.15f, 0.14f, 0.38f},  /* Serenitatis */
    {0.36f, -0.08f, 0.19f, 0.15f, 0.38f},  /* Tranquillitatis */
    {0.70f, -0.30f, 0.10f, 0.09f, 0.40f},  /* Crisium */
    {0.62f, 0.16f, 0.11f, 0.17f, 0.32f},   /* Fecunditatis */
    {0.42f, 0.30f, 0.08f, 0.08f, 0.28f},   /* Nectaris */
    {-0.18f, 0.36f, 0.17f, 0.12f, 0.28f},  /* Nubium */
    {-0.48f, 0.38f, 0.09f, 0.09f, 0.32f},  /* Humorum */
    {0.02f, -0.71f, 0.40f, 0.08f, 0.20f},  /* Frigoris */
    {0.04f, -0.16f, 0.09f, 0.08f, 0.20f},  /* Vaporum, Sinus Medii */
};

typedef struct {
    float x, y, r, albedo; /* albedo: + a young bright crater, - a dark floor */
} moon_crater_t;

static const moon_crater_t s_named_craters[] = {
    {-0.12f, 0.72f, 0.05f, 0.40f},  /* Tycho */
    {-0.26f, -0.18f, 0.05f, 0.28f}, /* Copernicus */
    {-0.53f, -0.12f, 0.03f, 0.22f}, /* Kepler */
    {-0.68f, -0.38f, 0.025f, 0.45f}, /* Aristarchus */
    {-0.08f, -0.70f, 0.045f, -0.30f}, /* Plato */
    {-0.90f, 0.06f, 0.06f, -0.30f}, /* Grimaldi */
};

static float moon_hash(int32_t x, int32_t y)
{
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u;

    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)((h ^ (h >> 16)) & 0xFFFFu) / 65535.0f;
}

/* Smooth value noise in 0..1. */
static float moon_noise(float x, float y)
{
    int32_t xi = (int32_t)floorf(x);
    int32_t yi = (int32_t)floorf(y);
    float fx = x - (float)xi;
    float fy = y - (float)yi;
    float a = moon_hash(xi, yi);
    float b = moon_hash(xi + 1, yi);
    float c = moon_hash(xi, yi + 1);
    float d = moon_hash(xi + 1, yi + 1);

    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
}

static float moon_fbm(float x, float y)
{
    return (0.5f * moon_noise(x, y) + 0.3f * moon_noise(x * 2.1f, y * 2.1f) + 0.2f * moon_noise(x * 4.3f, y * 4.3f));
}

static float moon_smooth(float edge0, float edge1, float v)
{
    float t = ev_clampf((v - edge0) / (edge1 - edge0), 0.0f, 1.0f);

    return t * t * (3.0f - 2.0f * t);
}

/* Bright rays round a young crater. */
static float moon_rays(float x, float y, const moon_crater_t *c, float reach, int32_t seed)
{
    float dx = x - c->x;
    float dy = y - c->y;
    float d = sqrtf(dx * dx + dy * dy);
    float streak = moon_noise((atan2f(dy, dx) + EV_PI) * 7.0f, (float)seed);

    if (d < c->r) {
        return 0.0f;
    }
    return moon_smooth(0.5f, 0.85f, streak) * expf(-(d - c->r) / reach);
}

/* The surface's brightness at x, y (1 for the highlands) and its relief under a light coming
 * from (lx, 0) in the picture plane: craters shade their sunward inner wall and light their
 * outer rim, more so the more obliquely the light falls. */
static float moon_albedo(float x, float y, float lx, float relief)
{
    float a = 1.0f + 0.16f * (moon_fbm(x * 7.0f + 3.0f, y * 7.0f) - 0.5f);
    float clear = 1.0f;
    float shade = 1.0f;

    for (size_t i = 0; i < sizeof(s_maria) / sizeof(s_maria[0]); i++) {
        const moon_mare_t *m = &s_maria[i];
        float ex = (x - m->x) / m->rx;
        float ey = (y - m->y) / m->ry;
        /* Ragged shores: the distance is warped by noise at two scales. */
        float e = sqrtf(ex * ex + ey * ey) + 0.55f * (moon_fbm(x * 3.5f + (float)i * 7.1f, y * 3.5f) - 0.5f) +
                  0.25f * (moon_noise(x * 11.0f, y * 11.0f + (float)i * 3.3f) - 0.5f);

        clear *= 1.0f - m->depth * moon_smooth(1.2f, 0.55f, e);
    }
    /* Mare floors are not flat grey either. */
    a *= clear * (1.0f + (1.0f - clear) * 0.5f * (moon_fbm(x * 9.0f, y * 9.0f + 5.0f) - 0.5f));
    a += 0.20f * moon_rays(x, y, &s_named_craters[0], 0.55f, 11) * (0.4f + 0.6f * clear);
    a += 0.12f * moon_rays(x, y, &s_named_craters[1], 0.22f, 23);

    for (int i = 0; i < 46; i++) {
        moon_crater_t c;
        float dx;
        float dy;
        float d;

        if (i < (int)(sizeof(s_named_craters) / sizeof(s_named_craters[0]))) {
            c = s_named_craters[i];
        } else {
            /* Small craters strewn over the disc, more of them in the southern highlands. */
            float u = moon_hash(i, 1) * 2.0f - 1.0f;
            float v = moon_hash(i, 2) * 1.6f - 0.6f;

            c = (moon_crater_t) {u * 0.9f, v, 0.02f + 0.03f * moon_hash(i, 3), 0.0f};
        }
        dx = (x - c.x) / c.r;
        dy = (y - c.y) / c.r;
        d = sqrtf(dx * dx + dy * dy);
        if (d > 1.35f) {
            continue;
        }
        if (c.albedo > 0.0f) {
            a += c.albedo * moon_smooth(1.5f, 0.5f, d);
        } else if (c.albedo < 0.0f) {
            a *= 1.0f + c.albedo * moon_smooth(1.1f, 0.7f, d);
        }
        if (d < 1.0f) {
            shade *= 1.0f - relief * 0.4f * ev_clampf(dx * lx, -1.0f, 1.0f) * (1.0f - d * d * 0.5f);
        } else {
            shade *= 1.0f + relief * 0.25f * ev_clampf(dx * lx, -1.0f, 1.0f) * moon_smooth(1.35f, 1.05f, d);
        }
    }
    return LV_MAX(0.0f, a * shade);
}

static void nocturne_moon(const ev_target_t *t, const nocturne_palette_t *p, float phase)
{
    /* The sun's direction in the moon's frame (x right, y down, z towards us): behind the
     * moon at new moon, to the right at first quarter (waxing lights the right side). */
    float angle = 2.0f * EV_PI * phase;
    float sx = sinf(angle);
    float sz = -cosf(angle);
    float lit_fraction = (1.0f - cosf(angle)) / 2.0f;
    /* Relief shows best near the terminator; at full moon the light falls straight on. */
    float relief = 0.3f + 0.7f * fabsf(sx);
    float lx = sx >= 0.0f ? 1.0f : -1.0f;
    float glow_r = MOON_R + 14.0f;
    float mr = (float)((p->moon >> 16) & 0xFF);
    float mg = (float)((p->moon >> 8) & 0xFF);
    float mb = (float)(p->moon & 0xFF);
    float hr = (float)((p->shadow >> 16) & 0xFF);
    float hg = (float)((p->shadow >> 8) & 0xFF);
    float hb = (float)(p->shadow & 0xFF);

    EV_FOR_BOX(t, MOON_X - glow_r, MOON_Y - glow_r, MOON_X + glow_r, MOON_Y + glow_r)
    {
        float px = (float)x + 0.5f - MOON_X;
        float py = (float)y + 0.5f - MOON_Y;
        float dist = hypotf(px, py);
        float disc = ev_clampf(MOON_R + 0.5f - dist, 0.0f, 1.0f);
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;

        if (dist > MOON_R - 1.0f) {
            /* A faint glow round the lit moon. */
            float glow = 0.10f * lit_fraction * expf(-(dist - MOON_R) / 5.0f);

            ev_px(t, x, y, p->moon, ev_clampf(glow, 0.0f, 1.0f) * (1.0f - disc));
        }
        if (disc <= 0.0f) {
            continue;
        }
        /* Average the surface over 2x2 samples: the features are only a few pixels wide. */
        for (int sy = 0; sy < 2; sy++) {
            for (int sxi = 0; sxi < 2; sxi++) {
                float u = (px - 0.25f + 0.5f * (float)sxi) / MOON_R;
                float v = (py - 0.25f + 0.5f * (float)sy) / MOON_R;
                float rr = LV_MIN(1.0f, u * u + v * v);
                float mu = sqrtf(1.0f - rr);
                float ns = u * sx + mu * sz;
                float albedo = moon_albedo(u, v, lx, relief);
                /* Lommel-Seeliger: an evenly bright full moon, a soft fall-off to the terminator. */
                float light = ns > 0.0f ? ev_clampf(2.0f * ns / (ns + mu + 0.05f), 0.0f, 1.1f) : 0.0f;
                float lit = ev_clampf(ns * MOON_R * 1.5f + 0.5f, 0.0f, 1.0f);
                float day = albedo * light * lit;
                /* Earthshine: the night side shows its maria faintly. */
                float night = (1.0f - lit) * (0.8f + 0.25f * (albedo - 0.8f));

                r += mr * day + hr * night;
                g += mg * day + hg * night;
                b += mb * day + hb * night;
            }
        }
        r = ev_clampf(r / 4.0f, 0.0f, 255.0f);
        g = ev_clampf(g / 4.0f, 0.0f, 255.0f);
        b = ev_clampf(b / 4.0f, 0.0f, 255.0f);
        ev_px(t, x, y, (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b, disc);
    }
}

/* The hill's top edge: M0 612 C140 574, 250 604, 372 590 S604 560, 720 592. */
static void nocturne_hill(const ev_target_t *t, const nocturne_palette_t *p)
{
    static const float seg[2][4][2] = {
        {{0, 612}, {140, 574}, {250, 604}, {372, 590}},
        {{372, 590}, {494, 576}, {604, 560}, {720, 592}},
    };
    static float top[SCREEN_SIZE];
    static bool built;

    if (!built) {
        /* Sampled densely enough that every column gets a value (the curve runs up to about
         * 1.1 px per 1/400 of a segment). */
        for (int x = 0; x < SCREEN_SIZE; x++) {
            top[x] = -1.0f;
        }
        for (int s = 0; s < 2; s++) {
            for (int i = 0; i <= 2000; i++) {
                float u = (float)i / 2000.0f;
                float v = 1.0f - u;
                float bx = v * v * v * seg[s][0][0] + 3 * v * v * u * seg[s][1][0] + 3 * v * u * u * seg[s][2][0] +
                           u * u * u * seg[s][3][0];
                float by = v * v * v * seg[s][0][1] + 3 * v * v * u * seg[s][1][1] + 3 * v * u * u * seg[s][2][1] +
                           u * u * u * seg[s][3][1];
                int32_t xi = (int32_t)bx;

                if (xi >= 0 && xi < SCREEN_SIZE) {
                    top[xi] = by;
                }
            }
        }
        for (int x = 1; x < SCREEN_SIZE; x++) {
            if (top[x] < 0.0f) {
                top[x] = top[x - 1];
            }
        }
        built = true;
    }
    for (int32_t x = t->clip.x1; x <= t->clip.x2; x++) {
        float edge = top[x];
        float slope = x + 1 < SCREEN_SIZE ? top[x + 1] - edge : 0.0f;
        float norm = 1.0f / sqrtf(1.0f + slope * slope);

        for (int32_t y = LV_MAX(t->clip.y1, (int32_t)edge - 2); y <= t->clip.y2; y++) {
            float below = (float)y + 0.5f - edge;

            ev_px(t, x, y, p->hill, ev_clampf(below + 0.5f, 0.0f, 1.0f));
            ev_px(t, x, y, p->hill_line, ev_clampf(p->hill_width / 2.0f + 0.5f - fabsf(below) * norm, 0.0f, 1.0f));
        }
    }
}

/* The moon's box as drawn over the sky and stars, kept so a base rebuilt for the same theme
 * and day (after another evening face had the shared base) copies it instead of shading
 * the sphere again. */
#define MOON_BOX_R ((int32_t)MOON_R + 15)
#define MOON_BOX (2 * MOON_BOX_R)
static uint16_t *s_moon_cache;
static uint32_t s_moon_key = UINT32_MAX;

static void nocturne_moon_cached(const ev_target_t *t, const nocturne_palette_t *p, float phase, uint32_t key)
{
    int32_t x0 = (int32_t)MOON_X - MOON_BOX_R;
    int32_t y0 = (int32_t)MOON_Y - MOON_BOX_R;
    bool hit = s_moon_cache != NULL && s_moon_key == key;

    if (s_moon_cache == NULL) {
        s_moon_cache = lv_malloc(MOON_BOX * MOON_BOX * sizeof(uint16_t));
    }
    if (!hit) {
        nocturne_moon(t, p, phase);
    }
    if (s_moon_cache == NULL) {
        return;
    }
    for (int32_t y = 0; y < MOON_BOX; y++) {
        uint16_t *row = (uint16_t *)(t->buf->data + (size_t)(y0 + y) * t->buf->header.stride) + x0;

        if (hit) {
            memcpy(row, s_moon_cache + y * MOON_BOX, MOON_BOX * sizeof(uint16_t));
        } else {
            memcpy(s_moon_cache + y * MOON_BOX, row, MOON_BOX * sizeof(uint16_t));
        }
    }
    s_moon_key = key;
}

static void nocturne_base(clock_ui_context_t *ctx, const nocturne_palette_t *p, const struct tm *ti, time_t now,
                          uint32_t key)
{
    ui_evening_face_t *ev = &ctx->faces.nocturne;
    ev_target_t t = {ev->base, {0, 0, SCREEN_SIZE - 1, SCREEN_SIZE - 1}};
    float phase = moon_phase(now);
    lv_layer_t layer;
    lv_obj_t *canvas;
    char text[64];
    char upper[96];

    ev_fill(&t, p->sky);
    for (size_t i = 0; i < sizeof(s_stars) / sizeof(s_stars[0]); i++) {
        ev_disc(&t, s_stars[i][0], s_stars[i][1], s_stars[i][2], p->stars, s_stars[i][3]);
    }
    ev_disc(&t, 548.0f, 128.0f, 1.9f, p->time, 0.7f);
    nocturne_moon_cached(&t, p, phase, key);
    nocturne_hill(&t, p);

    canvas = ev_base_canvas(ctx, CLOCK_FACE_NOCTURNE, &layer);
    ev_upper(upper, sizeof(upper), ui_l10n_str(ctx, &s_phases[(int)floorf(phase * 8.0f + 0.5f) % 8]));
    ev_text(&layer, &ev_sans_12, p->phase, 3, upper, EV_CENTER, 210);
    snprintf(text, sizeof(text), "%s %d %s", ui_weekday_name(ctx, ti->tm_wday), ti->tm_mday,
             ui_month_name(ctx, ti->tm_mon, true));
    ev_upper(upper, sizeof(upper), text);
    ev_text(&layer, &ev_sans_16, p->labels, 3, upper, EV_CENTER, 432);
    ev_base_canvas_done(canvas, &layer);
}

void create_nocturne_face(clock_ui_context_t *ctx, lv_obj_t *parent)
{
    ev_create(ctx, CLOCK_FACE_NOCTURNE, parent, nocturne_palette(ev_theme(ctx, CLOCK_FACE_NOCTURNE)).sky);
}

static void update_nocturne(clock_ui_context_t *ctx)
{
    ui_evening_face_t *ev = &ctx->faces.nocturne;
    uint8_t theme = ev_theme(ctx, CLOCK_FACE_NOCTURNE);
    nocturne_palette_t p = nocturne_palette(theme);
    time_t now = time(NULL);
    struct tm ti = get_local_time_now();
    int16_t minute = (int16_t)(ti.tm_hour * 60 + ti.tm_min);
    ev_target_t t;
    lv_layer_t layer;
    lv_area_t dirty;
    lv_area_t box = {0, 0, SCREEN_SIZE - 1, SCREEN_SIZE - 1};
    char clock[8];

    if (ev_base_begin(ctx, CLOCK_FACE_NOCTURNE, (uint32_t)theme << 16 | (uint32_t)ti.tm_yday, &t)) {
        lv_obj_set_style_bg_color(ctx->tiles[CLOCK_FACE_NOCTURNE], lv_color_hex(p.sky), 0);
        nocturne_base(ctx, &p, &ti, now, (uint32_t)theme << 16 | (uint32_t)ti.tm_yday);
    }
    if (ev->base == NULL || (ev->view.valid && minute == ev->shown_min)) {
        return; /* no seconds: nothing moves within a minute */
    }
    ev->view.valid = false;
    if (!face_hands_begin(&ev->view, ev->base, box, 0, 0, 0, &layer, &dirty)) {
        return;
    }
    snprintf(clock, sizeof(clock), "%02d:%02d", ti.tm_hour, ti.tm_min);
    ev_text(&layer, &ev_serif_light_176, p.time, 0, clock, EV_CENTER, 372);
    face_hands_finish(&ev->view, NULL, &layer, dirty, 0);
    ev->shown_min = minute;
}

/* --- Filament: low-glow nixie tubes --------------------------------------------------------- */

typedef struct {
    uint32_t ground, glass, rim, ghost, lit, labels, mesh, socket, ink;
} filament_palette_t;

static filament_palette_t filament_palette(uint8_t theme)
{
    static const filament_palette_t themes[CLOCK_FACE_THEME_COUNT] = {
        {0x080400, 0x100C08, 0x281C10, 0x281810, 0xC87430, 0x784C28, 0x181008, 0x181008, 0x885830},
        {0x080408, 0x080810, 0x201C30, 0x201C38, 0x786CC0, 0x585088, 0x181028, 0x080818, 0x6860A8},
        {0x000808, 0x000C10, 0x182C30, 0x102830, 0x50A8B0, 0x306468, 0x101C20, 0x081418, 0x408488},
    };

    return themes[theme < CLOCK_FACE_THEME_COUNT ? theme : 0];
}

#define TUBE_W 124
#define TUBE_H 210
#define TUBE_Y 230
#define TUBE_BASELINE 160 /* digits' baseline in the tube */
static const int32_t s_tube_x[4] = {76, 214, 382, 520};
#define COLON_X 360.0f
static const float s_colon_y[2] = {306.0f, 366.0f};

/* Signed distance to the tube outline (negative inside): a dome of radius 62 over a body
 * with 10 px rounded bottom corners. */
static float tube_sdf(float px, float py)
{
    float dome = hypotf(px - 62.0f, py - 62.0f) - 62.0f;
    float body;

    if (py > 200.0f && (px < 10.0f || px > 114.0f)) {
        body = hypotf(px - (px < 10.0f ? 10.0f : 114.0f), py - 200.0f) - 10.0f;
    } else {
        float qx = LV_MAX(-px, px - (float)TUBE_W);
        float qy = LV_MAX(62.0f - py, py - (float)TUBE_H);

        body = (qx > 0.0f || qy > 0.0f) ? hypotf(LV_MAX(qx, 0.0f), LV_MAX(qy, 0.0f)) : LV_MAX(qx, qy);
    }
    return LV_MIN(dome, body);
}

/* The outlines of the ten cathode digits, overlaid, as A8 coverage (theme independent). */
static uint8_t *filament_ghosts(clock_ui_context_t *ctx)
{
    lv_draw_buf_t *buf;
    lv_obj_t *canvas;
    uint8_t *ghosts;

    if (ctx->faces.filament_ghosts != NULL) {
        return ctx->faces.filament_ghosts;
    }
    ghosts = lv_malloc_zeroed(TUBE_W * TUBE_H);
    buf = lv_draw_buf_create(TUBE_W, TUBE_H, LV_COLOR_FORMAT_ARGB8888, LV_STRIDE_AUTO);
    if (ghosts == NULL || buf == NULL) {
        lv_free(ghosts);
        if (buf != NULL) {
            lv_draw_buf_destroy(buf);
        }
        return NULL;
    }
    canvas = lv_canvas_create(ctx->tiles[CLOCK_FACE_FILAMENT]);
    lv_obj_add_flag(canvas, LV_OBJ_FLAG_HIDDEN);
    lv_canvas_set_draw_buf(canvas, buf);
    for (int digit = 0; digit < 10; digit++) {
        lv_layer_t layer;
        char text[2] = {(char)('0' + digit), '\0'};
        lv_draw_label_dsc_t d;
        int32_t top = TUBE_BASELINE - (lv_font_get_line_height(&ev_sans_light_150) - ev_sans_light_150.base_line);
        lv_area_t a = {0, top, TUBE_W - 1, top + lv_font_get_line_height(&ev_sans_light_150) - 1};

        lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_TRANSP);
        lv_canvas_init_layer(canvas, &layer);
        lv_draw_label_dsc_init(&d);
        d.font = &ev_sans_light_150;
        d.color = lv_color_white();
        d.text = text;
        d.text_local = 1;
        d.align = LV_TEXT_ALIGN_CENTER;
        lv_draw_label(&layer, &d, &a);
        lv_canvas_finish_layer(canvas, &layer);
        /* The edge: coverage minus its 3x3 minimum, a line about a pixel wide. */
        for (int32_t y = 0; y < TUBE_H; y++) {
            for (int32_t x = 0; x < TUBE_W; x++) {
                const lv_color32_t *row = (const lv_color32_t *)(buf->data + (size_t)y * buf->header.stride);
                int32_t a0 = row[x].alpha;
                int32_t lo = a0;

                for (int32_t dy = -1; dy <= 1 && lo > 0; dy++) {
                    for (int32_t dx = -1; dx <= 1; dx++) {
                        int32_t nx = x + dx;
                        int32_t ny = y + dy;
                        int32_t na = 0;

                        if (nx >= 0 && nx < TUBE_W && ny >= 0 && ny < TUBE_H) {
                            na = ((const lv_color32_t *)(buf->data + (size_t)ny * buf->header.stride))[nx].alpha;
                        }
                        lo = LV_MIN(lo, na);
                    }
                }
                ghosts[y * TUBE_W + x] = (uint8_t)LV_MAX(ghosts[y * TUBE_W + x], a0 - lo);
            }
        }
    }
    lv_obj_delete(canvas);
    lv_draw_buf_destroy(buf);
    ctx->faces.filament_ghosts = ghosts;
    return ghosts;
}

static void filament_tube(const ev_target_t *t, const filament_palette_t *p, const uint8_t *ghosts, int32_t tx)
{
    for (int32_t ly = 0; ly < TUBE_H + 12; ly++) {
        for (int32_t lx = 0; lx < TUBE_W; lx++) {
            int32_t x = tx + lx;
            int32_t y = TUBE_Y + ly;
            float px = (float)lx + 0.5f;
            float py = (float)ly + 0.5f;
            float sdf = tube_sdf(px, py);
            float inside = ev_clampf(0.5f - sdf, 0.0f, 1.0f);

            if (inside > 0.0f) {
                /* Glass, its mesh (dots on an 8 x 7 lattice), the ghost digits. */
                float mx = fmodf(px, 8.0f);
                float my = fmodf(py, 7.0f);
                float dot = LV_MIN(hypotf(mx - 2.0f, my - 1.75f), hypotf(mx - 6.0f, my - 5.25f));

                ev_px(t, x, y, p->glass, inside);
                ev_px(t, x, y, p->mesh, inside * ev_clampf(1.4f - dot, 0.0f, 1.0f));
                if (ghosts != NULL && ly < TUBE_H) {
                    ev_px(t, x, y, p->ghost, inside * 0.7f * (float)ghosts[ly * TUBE_W + lx] / 255.0f);
                }
            }
            ev_px(t, x, y, p->rim, ev_clampf(1.25f - fabsf(sdf), 0.0f, 1.0f));
            /* The socket under the tube: 108 x 8 at y 214, radius 2. */
            if (ly >= 213 && ly <= 223 && lx >= 7 && lx <= 117) {
                float qx = LV_MAX(10.0f - px, px - 114.0f);
                float qy = LV_MAX(216.0f - py, py - 220.0f);
                float d = (qx > 0.0f || qy > 0.0f) ? hypotf(LV_MAX(qx, 0.0f), LV_MAX(qy, 0.0f)) - 2.0f
                                                   : LV_MAX(qx, qy) - 2.0f;

                ev_px(t, x, y, p->socket, ev_clampf(0.5f - d, 0.0f, 1.0f));
            }
        }
    }
}

static void filament_base(clock_ui_context_t *ctx, const filament_palette_t *p, const struct tm *ti)
{
    ui_evening_face_t *ev = &ctx->faces.filament;
    ev_target_t t = {ev->base, {0, 0, SCREEN_SIZE - 1, SCREEN_SIZE - 1}};
    const uint8_t *ghosts = filament_ghosts(ctx);
    lv_layer_t layer;
    lv_obj_t *canvas;
    char text[32];
    char upper[48];

    ev_fill(&t, p->ground);
    for (int i = 0; i < 4; i++) {
        filament_tube(&t, p, ghosts, s_tube_x[i]);
    }
    for (int i = 0; i < 2; i++) {
        ev_disc(&t, COLON_X, s_colon_y[i], 11.0f, p->glass, 1.0f);
        ev_ring(&t, COLON_X, s_colon_y[i], 11.0f, 1.5f, p->rim, 1.0f);
    }
    snprintf(text, sizeof(text), "%s %d %s", l10n_day_short((clock_language_t)ctx->settings->language, ti->tm_wday),
             ti->tm_mday, ui_month_name(ctx, ti->tm_mon, false));
    ev_upper(upper, sizeof(upper), text);
    canvas = ev_base_canvas(ctx, CLOCK_FACE_FILAMENT, &layer);
    ev_text(&layer, &it_mono_18, p->labels, 5, upper, EV_CENTER, 186);
    ev_base_canvas_done(canvas, &layer);
}

void create_filament_face(clock_ui_context_t *ctx, lv_obj_t *parent)
{
    ev_create(ctx, CLOCK_FACE_FILAMENT, parent, filament_palette(ev_theme(ctx, CLOCK_FACE_FILAMENT)).ground);
}

static void update_filament(clock_ui_context_t *ctx)
{
    ui_evening_face_t *ev = &ctx->faces.filament;
    uint8_t theme = ev_theme(ctx, CLOCK_FACE_FILAMENT);
    filament_palette_t p = filament_palette(theme);
    struct tm ti = get_local_time_now();
    int16_t minute = (int16_t)(ti.tm_hour * 60 + ti.tm_min);
    bool colon_on = (ti.tm_sec % 2) == 0;
    bool full;
    ev_target_t t;
    lv_layer_t layer;
    lv_area_t dirty;
    lv_area_t box = {(int32_t)COLON_X - 8, (int32_t)s_colon_y[0] - 8, (int32_t)COLON_X + 8, (int32_t)s_colon_y[1] + 8};

    if (ev_base_begin(ctx, CLOCK_FACE_FILAMENT, (uint32_t)theme << 16 | (uint32_t)ti.tm_yday, &t)) {
        lv_obj_set_style_bg_color(ctx->tiles[CLOCK_FACE_FILAMENT], lv_color_hex(p.ground), 0);
        filament_base(ctx, &p, &ti);
    }
    if (ev->base == NULL || (ev->view.valid && minute == ev->shown_min && ti.tm_sec == ev->shown_sec)) {
        return;
    }
    if (minute != ev->shown_min) {
        ev->view.valid = false; /* new digits: repaint the face */
    }
    full = !ev->view.valid; /* also after a new base (theme, day) */
    if (!face_hands_begin(&ev->view, ev->base, box, 0, 0, 0, &layer, &dirty)) {
        return;
    }
    t.buf = ev->view.buf;
    t.clip = dirty;
    if (colon_on) {
        for (int i = 0; i < 2; i++) {
            ev_disc(&t, COLON_X, s_colon_y[i], 5.0f, p.lit, 1.0f);
        }
    }
    if (full) {
        char digit[2] = {0};
        int digits[4] = {ti.tm_hour / 10, ti.tm_hour % 10, ti.tm_min / 10, ti.tm_min % 10};

        for (int i = 0; i < 4; i++) {
            digit[0] = (char)('0' + digits[i]);
            ev_text(&layer, &ev_sans_light_150, p.lit, 0, digit, s_tube_x[i] + TUBE_W / 2, TUBE_Y + TUBE_BASELINE);
        }
    }
    face_hands_finish(&ev->view, NULL, &layer, dirty, 0);
    ev->shown_min = minute;
    ev->shown_sec = (int8_t)ti.tm_sec;
}

/* --- Hooks ------------------------------------------------------------------------------------ */

void update_evening_face(clock_ui_context_t *ctx, clock_face_id_t face)
{
    switch (face) {
    case CLOCK_FACE_EMBER:
        update_ember(ctx);
        break;
    case CLOCK_FACE_NOCTURNE:
        update_nocturne(ctx);
        break;
    case CLOCK_FACE_FILAMENT:
        update_filament(ctx);
        break;
    default:
        break;
    }
}

const void *evening_face_preview_source(clock_ui_context_t *ctx, clock_face_id_t face)
{
    ui_evening_face_t *ev = ev_state(ctx, face);

    update_evening_face(ctx, face);
    return ev != NULL && ev->view.valid ? ev->view.buf : NULL;
}

bool evening_face_ink(const clock_ui_context_t *ctx, clock_face_id_t face, ui_face_ink_t *ink)
{
    switch (face) {
    case CLOCK_FACE_EMBER: {
        ember_palette_t p = ember_palette(ev_theme(ctx, face));

        ink->text = lv_color_hex(p.ink);
        ink->accent = lv_color_hex(p.accent);
        ink->soft = lv_color_hex(p.date);
        ink->paper = lv_color_hex(p.ground);
        return true;
    }
    case CLOCK_FACE_NOCTURNE: {
        nocturne_palette_t p = nocturne_palette(ev_theme(ctx, face));

        ink->text = lv_color_hex(p.ink);
        ink->accent = lv_color_hex(p.moon);
        ink->soft = lv_color_hex(p.labels);
        ink->paper = lv_color_hex(p.sky);
        return true;
    }
    case CLOCK_FACE_FILAMENT: {
        filament_palette_t p = filament_palette(ev_theme(ctx, face));

        ink->text = lv_color_hex(p.ink);
        ink->accent = lv_color_hex(p.lit);
        ink->soft = lv_color_hex(p.labels);
        ink->paper = lv_color_hex(p.ground);
        return true;
    }
    default:
        return false;
    }
}

bool evening_face_preview_colors(clock_face_id_t face, uint8_t theme, uint32_t colors[3])
{
    switch (face) {
    case CLOCK_FACE_EMBER: {
        ember_palette_t p = ember_palette(theme);

        colors[0] = p.ground;
        colors[1] = p.hour_tick;
        colors[2] = p.accent;
        return true;
    }
    case CLOCK_FACE_NOCTURNE: {
        nocturne_palette_t p = nocturne_palette(theme);

        colors[0] = p.sky;
        colors[1] = p.labels;
        colors[2] = p.moon;
        return true;
    }
    case CLOCK_FACE_FILAMENT: {
        filament_palette_t p = filament_palette(theme);

        colors[0] = p.ground;
        colors[1] = p.rim;
        colors[2] = p.lit;
        return true;
    }
    default:
        return false;
    }
}
