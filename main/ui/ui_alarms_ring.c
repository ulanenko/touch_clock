/*
 * Alarm ringing (screen 11), the math challenge, and the alarm banner on the face.
 *
 *   Instrument: minute rim with an orange second mark, Snooze as the big orange key, stop
 *               by press-and-hold (the ring fills in 1.5 s).
 *   Orbit:      sunrise glow, Snooze as a 184 px orange circle, stop by sliding the knob
 *               along a band on the lower rim ("slide to stop" set on the curve).
 *   Atelier:    sunray dial with a warm wash, Snooze as a framed pill, stop by drawing the
 *               jewel along the groove in the lower bezel.
 *
 * The glow and the sunray are computed once into a PSRAM canvas (dithered to RGB565, so the
 * gradients do not band) when the screen is first shown; the knob is the only thing that
 * moves over them.
 */
#include "ui/ui_alarms_internal.h"

#include <inttypes.h>
#include <math.h>
#include <stdlib.h>

#include "assets/skin_fonts/skin_fonts.h"
#include "assets/ui_assets/alarms_assets.h"
#include "esp_heap_caps.h"

#define MATH_MAX_INPUT 2U
#define HOLD_MS 1500
#define BANNER_MIN_BRIGHTNESS_UI_PERCENT 10
#define BANNER_CENTER_Y 548

/* Slide geometry: polar angles in 0.1 degree, 0 = twelve o'clock, clockwise. */
#define O_SLIDE_R 292
#define O_SLIDE_START 2300
#define O_SLIDE_END 1300
#define A_SLIDE_R 300
#define A_SLIDE_START 2260
#define A_SLIDE_END 1340
#define O_GLOW_Y 340

enum { KEY_BKSP = 10, KEY_OK = 11 };
enum { BANNER_NONE, BANNER_WAKING, BANNER_SNOOZE, BANNER_FEEDBACK };

static void ring_stop_requested(clock_ui_context_t *ctx);

/* --- Background art (computed once) --------------------------------------------------------- */

static const uint8_t s_bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

static inline uint16_t rgb565_dither(float r, float g, float b, int x, int y)
{
    float d = ((float)s_bayer[y & 3][x & 3] + 0.5f) / 16.0f;
    int ri = (int)(r / 8.0f + d);
    int gi = (int)(g / 4.0f + d);
    int bi = (int)(b / 8.0f + d);

    ri = ri > 31 ? 31 : ri < 0 ? 0 : ri;
    gi = gi > 63 ? 63 : gi < 0 ? 0 : gi;
    bi = bi > 31 ? 31 : bi < 0 ? 0 : bi;
    return (uint16_t)((ri << 11) | (gi << 5) | bi);
}

static void mix(float *c, float r, float g, float b, float a)
{
    c[0] = c[0] * (1.0f - a) + r * a;
    c[1] = c[1] * (1.0f - a) + g * a;
    c[2] = c[2] * (1.0f - a) + b * a;
}

/* Orbit: radial-gradient(130% 75% at 50% 104%, #6A3500, #2E1700 40%, #000 74%). */
static void render_glow(uint16_t *px, int32_t w, int32_t h, int32_t y0)
{
    for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
            float dx = ((float)x - 360.0f) / 936.0f;
            float dy = ((float)(y + y0) - 748.8f) / 540.0f;
            float t = sqrtf(dx * dx + dy * dy);
            float c[3] = {0, 0, 0};

            if (t < 0.40f) {
                float k = t / 0.40f;

                c[0] = 0x6A + (0x2E - 0x6A) * k;
                c[1] = 0x35 + (0x17 - 0x35) * k;
            } else if (t < 0.74f) {
                float k = (t - 0.40f) / 0.34f;

                c[0] = 0x2E * (1.0f - k);
                c[1] = 0x17 * (1.0f - k);
            }
            px[y * w + x] = rgb565_dither(c[0], c[1], c[2], x, y + y0);
        }
    }
}

/* Atelier: sunray dial (480 rays), centre vignette and a warm wash from six o'clock. */
static void render_sunray(uint16_t *px)
{
    for (int32_t y = 0; y < KIT_SCREEN; y++) {
        for (int32_t x = 0; x < KIT_SCREEN; x++) {
            float dx = (float)x + 0.5f - 360.0f;
            float dy = (float)y + 0.5f - 360.0f;
            float deg = atan2f(dx, -dy) * 57.29578f + 360.0f;
            bool ray = ((int)(deg / 0.75f) & 1) == 0;
            float c[3];
            float t = sqrtf(dx * dx + dy * dy) / 509.0f;
            float a;
            float ex = dx / 504.0f;
            float ey = ((float)y - 720.0f) / 324.0f;
            float te = sqrtf(ex * ex + ey * ey);

            c[0] = ray ? 0x1A : 0x10;
            c[1] = ray ? 0x16 : 0x0C;
            c[2] = ray ? 0x22 : 0x18;
            a = t < 0.55f ? 0.85f - 0.5f * (t / 0.55f) : t < 0.75f ? 0.35f * (1.0f - (t - 0.55f) / 0.2f) : 0.0f;
            mix(c, 0x10, 0x0C, 0x18, a);
            if (te < 1.0f) {
                mix(c, 0xA0, 0x68, 0x30, 0.28f * (1.0f - te));
            }
            px[y * KIT_SCREEN + x] = rgb565_dither(c[0], c[1], c[2], x, y);
        }
    }
}

static void draw_arc(lv_layer_t *layer, int32_t cx, int32_t cy, int32_t r_mid, int32_t width, int32_t start,
                     int32_t end, lv_color_t color, lv_opa_t opa, bool rounded)
{
    lv_draw_arc_dsc_t d;

    lv_draw_arc_dsc_init(&d);
    d.center.x = cx;
    d.center.y = cy;
    d.radius = (uint16_t)(r_mid + width / 2);
    d.width = width;
    d.start_angle = start;
    d.end_angle = end;
    d.color = color;
    d.opa = opa;
    d.rounded = rounded;
    kit_draw_arc(layer, &d);
}

static uint32_t utf8_next(const char *s, uint32_t *i)
{
    const uint8_t *u = (const uint8_t *)s + *i;
    uint32_t cp;
    uint32_t n;

    if (u[0] == 0) {
        return 0;
    }
    if (u[0] < 0x80) {
        cp = u[0];
        n = 1;
    } else if ((u[0] & 0xE0) == 0xC0 && u[1] != 0) {
        cp = ((uint32_t)(u[0] & 0x1F) << 6) | (u[1] & 0x3F);
        n = 2;
    } else if ((u[0] & 0xF0) == 0xE0 && u[1] != 0 && u[2] != 0) {
        cp = ((uint32_t)(u[0] & 0x0F) << 12) | ((uint32_t)(u[1] & 0x3F) << 6) | (u[2] & 0x3F);
        n = 3;
    } else {
        cp = '?';
        n = 1;
    }
    *i += n;
    return cp;
}

/* Text set on the lower rim, glyph by glyph, each turned to the curve. */
static void draw_curved_text(lv_layer_t *layer, const char *text, const lv_font_t *font, lv_color_t color, int32_t cx,
                             int32_t cy, int32_t r, int32_t spacing)
{
    float total = 0;
    float s = 0;
    const char *p;
    uint32_t i = 0;

    for (p = text; *p != '\0';) {
        uint32_t cp = utf8_next(text, &i);

        total += (float)lv_font_get_glyph_width(font, cp, 0) + (float)spacing;
        p = text + i;
        if (cp == 0) {
            break;
        }
    }
    total -= (float)spacing;
    i = 0;
    for (p = text; *p != '\0';) {
        uint32_t idx = i;
        uint32_t cp = utf8_next(text, &i);
        float gw = (float)lv_font_get_glyph_width(font, cp, 0);
        float mid = s + gw / 2.0f;
        float theta = 3.14159265f + (total / 2.0f - mid) / (float)r; /* polar, 0 = twelve */
        float gx = (float)cx + (float)r * sinf(theta);
        float gy = (float)cy - (float)r * cosf(theta);
        int32_t lh = lv_font_get_line_height(font);
        lv_draw_label_dsc_t d;
        lv_area_t a;
        char glyph[8];
        uint32_t n = i - idx;

        if (cp == 0) {
            break;
        }
        if (n < sizeof(glyph)) {
            memcpy(glyph, text + idx, n);
            glyph[n] = '\0';
            lv_draw_label_dsc_init(&d);
            d.font = font;
            d.color = color;
            d.text = glyph;
            d.text_local = 1;
            d.rotation = (int32_t)lroundf((theta - 3.14159265f) * 1800.0f / 3.14159265f);
            a.x1 = (int32_t)lroundf(gx - gw / 2.0f);
            a.y1 = (int32_t)lroundf(gy - (float)lh / 2.0f);
            a.x2 = a.x1 + (int32_t)gw + 4;
            a.y2 = a.y1 + lh - 1;
            lv_draw_label(layer, &d, &a);
        }
        s += gw + (float)spacing;
        p = text + i;
    }
}

static void render_orbit_bg(clock_ui_context_t *ctx)
{
    const ui_skin_t *sk = ui_skin();
    lv_layer_t layer;
    int32_t cy = KIT_CENTER - O_GLOW_Y;

    render_glow(ctx->alarms.ring_bg_buf, KIT_SCREEN, KIT_SCREEN - O_GLOW_Y, O_GLOW_Y);
    lv_canvas_init_layer(ctx->alarms.ring_bg, &layer);
    /* The band: LVGL angles, 0 = three o'clock. */
    draw_arc(&layer, KIT_CENTER, cy, O_SLIDE_R, 76, 40, 140, lv_color_hex(0x1C1C1E), LV_OPA_COVER, true);
    draw_curved_text(&layer, alm_str(ctx, &alm_s_slide_to_stop), &or_medium_22, sk->text_2, KIT_CENTER, cy, O_SLIDE_R,
                     1);
    lv_canvas_finish_layer(ctx->alarms.ring_bg, &layer);
}

static void render_atelier_bg(clock_ui_context_t *ctx)
{
    const ui_skin_t *sk = ui_skin();
    lv_layer_t layer;
    lv_draw_line_dsc_t line;
    lv_draw_rect_dsc_t rect;
    int32_t ex;
    int32_t ey;
    lv_area_t a;

    render_sunray(ctx->alarms.ring_bg_buf);
    lv_canvas_init_layer(ctx->alarms.ring_bg, &layer);
    draw_arc(&layer, KIT_CENTER, KIT_CENTER, 355, 1, 0, 360, sk->accent_deep, 140, false);
    lv_draw_line_dsc_init(&line);
    for (int i = 0; i < 60; i++) {
        int32_t deg = i * 6;
        bool major = i % 5 == 0;
        int32_t x0;
        int32_t y0;
        int32_t x1;
        int32_t y1;

        if (deg > 126 && deg < 234) {
            continue; /* the groove takes the lower bezel */
        }
        alm_polar(KIT_CENTER, KIT_CENTER, major ? 331 : 339, deg * 10, &x0, &y0);
        alm_polar(KIT_CENTER, KIT_CENTER, 347, deg * 10, &x1, &y1);
        line.color = major ? sk->label : sk->line_strong;
        line.width = major ? 2 : 1;
        line.p1.x = x0;
        line.p1.y = y0;
        line.p2.x = x1;
        line.p2.y = y1;
        lv_draw_line(&layer, &line);
    }
    /* The groove: ink, a rule hairline, the recessed track. It spans exactly the knob's travel
     * (A_SLIDE_END..A_SLIDE_START, converted to LVGL angles with 0 at three o'clock), so the
     * jewel sits flush in the rounded ends at rest and when it reaches the stop mark. */
    draw_arc(&layer, KIT_CENTER, KIT_CENTER, A_SLIDE_R, 64, A_SLIDE_END / 10 - 90, A_SLIDE_START / 10 - 90, sk->bg,
             LV_OPA_COVER, true);
    draw_arc(&layer, KIT_CENTER, KIT_CENTER, A_SLIDE_R, 62, A_SLIDE_END / 10 - 90, A_SLIDE_START / 10 - 90,
             sk->line_strong, LV_OPA_COVER, true);
    draw_arc(&layer, KIT_CENTER, KIT_CENTER, A_SLIDE_R, 60, A_SLIDE_END / 10 - 90, A_SLIDE_START / 10 - 90,
             sk->surface, LV_OPA_COVER, true);
    /* The stop mark at the end of the groove. */
    alm_polar(KIT_CENTER, KIT_CENTER, A_SLIDE_R, 1340, &ex, &ey);
    draw_arc(&layer, ex, ey, 19, 2, 0, 360, sk->accent_deep, LV_OPA_COVER, false);
    lv_draw_rect_dsc_init(&rect);
    rect.bg_color = sk->accent_deep;
    rect.radius = 3;
    a.x1 = ex - 7;
    a.y1 = ey - 7;
    a.x2 = ex + 6;
    a.y2 = ey + 6;
    lv_draw_rect(&layer, &rect, &a);
    lv_canvas_finish_layer(ctx->alarms.ring_bg, &layer);
}

static void ensure_background(clock_ui_context_t *ctx)
{
    clock_ui_alarm_state_t *a = &ctx->alarms;

    if (ALM_IS_INSTRUMENT()) {
        return;
    }
    if (a->ring_bg != NULL) {
        if (ALM_IS_ORBIT() && a->ring_bg_language != ctx->settings->language + 1) {
            a->ring_bg_language = (uint8_t)(ctx->settings->language + 1);
            render_orbit_bg(ctx); /* "slide to stop" is set in the language */
            lv_obj_invalidate(a->ring_bg);
        }
        return;
    }
    a->ring_bg_language = (uint8_t)(ctx->settings->language + 1);
    if (ALM_IS_ORBIT()) {
        a->ring_bg = alm_canvas(a->ring_main, KIT_SCREEN, KIT_SCREEN - O_GLOW_Y, &a->ring_bg_buf);
        if (a->ring_bg == NULL) {
            return;
        }
        lv_obj_set_pos(a->ring_bg, 0, O_GLOW_Y);
        lv_obj_move_to_index(a->ring_bg, 0);
        render_orbit_bg(ctx);
    } else {
        a->ring_bg = alm_canvas(a->ring_main, KIT_SCREEN, KIT_SCREEN, &a->ring_bg_buf);
        if (a->ring_bg == NULL) {
            return;
        }
        lv_obj_set_pos(a->ring_bg, 0, 0);
        lv_obj_move_to_index(a->ring_bg, 0);
        render_atelier_bg(ctx);
    }
}

/* Instrument: the minute rim and the orange second mark. */
static void instrument_rim_cb(lv_event_t *e)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_context_t *ctx = lv_event_get_user_data(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_draw_line_dsc_t line;

    lv_draw_line_dsc_init(&line);
    for (int i = 0; i < 60; i++) {
        bool major = i % 5 == 0;
        int32_t x0;
        int32_t y0;
        int32_t x1;
        int32_t y1;

        alm_polar(KIT_CENTER, KIT_CENTER, major ? 322 : 332, i * 60, &x0, &y0);
        alm_polar(KIT_CENTER, KIT_CENTER, 342, i * 60, &x1, &y1);
        line.color = major ? sk->text_3 : sk->line_strong;
        line.width = major ? 3 : 2;
        line.p1.x = x0;
        line.p1.y = y0;
        line.p2.x = x1;
        line.p2.y = y1;
        lv_draw_line(layer, &line);
    }
    if (ctx->alarms.ring_sec_shown >= 0) {
        int32_t x0;
        int32_t y0;
        int32_t x1;
        int32_t y1;

        alm_polar(KIT_CENTER, KIT_CENTER, 316, ctx->alarms.ring_sec_shown * 60, &x0, &y0);
        alm_polar(KIT_CENTER, KIT_CENTER, 342, ctx->alarms.ring_sec_shown * 60, &x1, &y1);
        line.color = sk->accent;
        line.width = 4;
        line.p1.x = x0;
        line.p1.y = y0;
        line.p2.x = x1;
        line.p2.y = y1;
        lv_draw_line(layer, &line);
    }
}

static void invalidate_sec_mark(clock_ui_context_t *ctx, int sec)
{
    int32_t x0;
    int32_t y0;
    int32_t x1;
    int32_t y1;
    lv_area_t a;

    alm_polar(KIT_CENTER, KIT_CENTER, 314, sec * 60, &x0, &y0);
    alm_polar(KIT_CENTER, KIT_CENTER, 344, sec * 60, &x1, &y1);
    a.x1 = LV_MIN(x0, x1) - 4;
    a.y1 = LV_MIN(y0, y1) - 4;
    a.x2 = LV_MAX(x0, x1) + 4;
    a.y2 = LV_MAX(y0, y1) + 4;
    lv_obj_invalidate_area(ctx->alarms.ring_main, &a);
}

/* --- Stop controls --------------------------------------------------------------------------- */

static void hold_exec_cb(void *var, int32_t v)
{
    lv_arc_set_value(var, v);
}

static void hold_done_cb(lv_anim_t *anim)
{
    clock_ui_context_t *ctx = lv_anim_get_user_data(anim);

    if (ctx->alarms.ring_holding && lv_arc_get_value(ctx->alarms.ring_hold_arc) >= 1000) {
        ctx->alarms.ring_holding = false;
        ring_stop_requested(ctx);
    }
}

static void hold_animate(clock_ui_context_t *ctx, int32_t to, uint32_t ms)
{
    lv_obj_t *arc = ctx->alarms.ring_hold_arc;
    lv_anim_t a;

    lv_anim_delete(arc, hold_exec_cb);
    lv_anim_init(&a);
    lv_anim_set_var(&a, arc);
    lv_anim_set_exec_cb(&a, hold_exec_cb);
    lv_anim_set_values(&a, lv_arc_get_value(arc), to);
    lv_anim_set_duration(&a, ms);
    lv_anim_set_user_data(&a, ctx);
    if (to > 0) {
        lv_anim_set_completed_cb(&a, hold_done_cb);
    }
    lv_anim_start(&a);
}

static void hold_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    int32_t v = lv_arc_get_value(ctx->alarms.ring_hold_arc);

    if (code == LV_EVENT_PRESSED) {
        ctx->alarms.ring_holding = true;
        hold_animate(ctx, 1000, (uint32_t)(HOLD_MS * (1000 - v) / 1000));
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if (ctx->alarms.ring_holding) {
            ctx->alarms.ring_holding = false;
            hold_animate(ctx, 0, 220);
        }
    }
}

static void slide_place(clock_ui_context_t *ctx, int32_t angle)
{
    lv_obj_t *k = ctx->alarms.ring_knob;
    int32_t r = ALM_IS_ORBIT() ? O_SLIDE_R : A_SLIDE_R;
    int32_t x;
    int32_t y;

    ctx->alarms.ring_knob_angle = (int16_t)angle;
    alm_polar(KIT_CENTER, KIT_CENTER, r, angle, &x, &y);
    lv_obj_set_pos(k, x - lv_obj_get_width(k) / 2, y - lv_obj_get_height(k) / 2);
}

static void slide_exec_cb(void *var, int32_t v)
{
    clock_ui_context_t *ctx = var;

    slide_place(ctx, v);
}

static void slide_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    int32_t start = ALM_IS_ORBIT() ? O_SLIDE_START : A_SLIDE_START;
    int32_t end = ALM_IS_ORBIT() ? O_SLIDE_END : A_SLIDE_END;
    lv_indev_t *indev = lv_indev_active();
    lv_point_t p;
    int32_t angle;

    if (code == LV_EVENT_PRESSED) {
        lv_anim_delete(ctx, slide_exec_cb);
        ctx->alarms.ring_sliding = true;
        return;
    }
    if ((code == LV_EVENT_PRESSING || code == LV_EVENT_RELEASED) && ctx->alarms.ring_sliding && indev != NULL) {
        lv_indev_get_point(indev, &p);
        angle = alm_angle_of(KIT_CENTER, KIT_CENTER, p.x, p.y, NULL);
        if (angle < 900 || angle > 2700) {
            angle = p.x < KIT_CENTER ? start : end; /* finger above the centre line */
        }
        angle = LV_CLAMP(end, angle, start);
        if (angle != ctx->alarms.ring_knob_angle) {
            slide_place(ctx, angle);
        }
        if (code == LV_EVENT_PRESSING) {
            return;
        }
        /* RELEASED: the lift-off point counts too, then settle below. */
    }
    if ((code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) && ctx->alarms.ring_sliding) {
        lv_anim_t a;

        ctx->alarms.ring_sliding = false;
        if (ctx->alarms.ring_knob_angle <= end + 60) {
            slide_place(ctx, end);
            ring_stop_requested(ctx);
            return;
        }
        lv_anim_init(&a);
        lv_anim_set_var(&a, ctx);
        lv_anim_set_exec_cb(&a, slide_exec_cb);
        lv_anim_set_values(&a, ctx->alarms.ring_knob_angle, start);
        lv_anim_set_duration(&a, 260);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_start(&a);
    }
}

static void reset_stop_controls(clock_ui_context_t *ctx)
{
    clock_ui_alarm_state_t *a = &ctx->alarms;

    a->ring_holding = false;
    a->ring_sliding = false;
    if (a->ring_hold_arc != NULL) {
        lv_anim_delete(a->ring_hold_arc, hold_exec_cb);
        lv_arc_set_value(a->ring_hold_arc, 0);
    }
    if (a->ring_knob != NULL) {
        lv_anim_delete(ctx, slide_exec_cb);
        slide_place(ctx, ALM_IS_ORBIT() ? O_SLIDE_START : A_SLIDE_START);
    }
}

/* --- Math challenge ---------------------------------------------------------------------- */

static bool requires_math(const clock_ui_context_t *ctx)
{
    const alarm_config_t *alarm = alm_active_alarm(ctx);

    if (ctx->alarms.debug_ringing && !ctx->runtime->alarm_ringing) {
        return ctx->alarms.debug_math || (alarm != NULL && alarm->math_unlock_enabled);
    }
    return alarm != NULL && alarm->math_unlock_enabled;
}

static void math_show_answer(clock_ui_context_t *ctx)
{
    clock_ui_alarm_state_t *a = &ctx->alarms;

    alm_set_text(a->math_answer, a->math_answer_len > 0 ? a->math_answer_text : "");
}

static void math_set_error(clock_ui_context_t *ctx, bool error)
{
    alm_show(ctx->alarms.math_error, error);
    alm_show(ctx->alarms.math_head, !error);
}

static void math_reset_input(clock_ui_context_t *ctx)
{
    ctx->alarms.math_answer_text[0] = '\0';
    ctx->alarms.math_answer_len = 0;
    math_show_answer(ctx);
}

static void math_generate(clock_ui_context_t *ctx)
{
    clock_ui_alarm_state_t *a = &ctx->alarms;
    uint32_t seed = lv_tick_get() + (uint32_t)alm_now();
    char text[24];

    if ((seed & 1U) == 0U) {
        a->math_a = (uint8_t)(3U + (seed % 8U));
        a->math_b = (uint8_t)(2U + ((seed / 7U) % 14U));
        a->math_expected = (uint8_t)(a->math_a + a->math_b);
        a->math_plus = true;
    } else {
        a->math_b = (uint8_t)(2U + (seed % 7U));
        a->math_expected = (uint8_t)(4U + ((seed / 5U) % 12U));
        a->math_a = (uint8_t)(a->math_expected + a->math_b);
        a->math_plus = false;
    }
    /* Instrument sets the whole sum with its question mark; the others put the answer box
     * where the question mark would be. */
    snprintf(text, sizeof(text), "%u %s %u =%s", a->math_a, a->math_plus ? "+" : "\xE2\x88\x92", a->math_b,
             ALM_IS_INSTRUMENT() ? " ?" : "");
    alm_set_text(a->math_problem, text);
    math_reset_input(ctx);
    math_set_error(ctx, false);
}

static void math_submit(clock_ui_context_t *ctx)
{
    clock_ui_alarm_state_t *a = &ctx->alarms;

    if (a->math_answer_len == 0) {
        return;
    }
    if (strtoul(a->math_answer_text, NULL, 10) == a->math_expected) {
        if (a->debug_ringing && !ctx->runtime->alarm_ringing) {
            a->debug_ringing = false;
            a->debug_math = false;
            alm_ring_sync(ctx, alm_now());
        } else if (ctx->callbacks.on_alarm_stop_requested != NULL) {
            ctx->callbacks.on_alarm_stop_requested(ctx->user_ctx);
        }
        return;
    }
    math_reset_input(ctx);
    math_set_error(ctx, true);
}

static void math_key_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);
    clock_ui_alarm_state_t *a = &ctx->alarms;
    lv_obj_t *key = lv_event_get_target(e);
    int k = -1;

    for (int i = 0; i < 12; i++) {
        if (a->math_keys[i] == key) {
            k = i;
            break;
        }
    }
    if (k == KEY_OK) {
        math_submit(ctx);
    } else if (k == KEY_BKSP) {
        if (a->math_answer_len > 0) {
            a->math_answer_text[--a->math_answer_len] = '\0';
            math_show_answer(ctx);
        }
    } else if (k >= 0 && k <= 9 && a->math_answer_len < MATH_MAX_INPUT) {
        a->math_answer_text[a->math_answer_len++] = (char)('0' + k);
        a->math_answer_text[a->math_answer_len] = '\0';
        math_set_error(ctx, false);
        math_show_answer(ctx);
        if (a->math_answer_len >= (a->math_expected >= 10U ? 2U : 1U)) {
            math_submit(ctx); /* answer complete: no OK needed */
        }
    }
}

static void math_set_visible(clock_ui_context_t *ctx, bool visible)
{
    clock_ui_alarm_state_t *a = &ctx->alarms;

    a->math_visible = visible;
    alm_show(a->math, visible);
    alm_show(a->ring_main, !visible);
    if (!visible) {
        math_reset_input(ctx);
        math_set_error(ctx, false);
    }
}

void alm_ring_show_math(clock_ui_context_t *ctx)
{
    if (ctx->alarms.math == NULL) {
        return;
    }
    math_generate(ctx);
    math_set_visible(ctx, true);
}

static void ring_stop_requested(clock_ui_context_t *ctx)
{
    if (requires_math(ctx)) {
        alm_ring_show_math(ctx);
        reset_stop_controls(ctx);
        return;
    }
    if (ctx->alarms.debug_ringing && !ctx->runtime->alarm_ringing) {
        ctx->alarms.debug_ringing = false;
        alm_ring_sync(ctx, alm_now());
        return;
    }
    if (ctx->callbacks.on_alarm_stop_requested != NULL) {
        ctx->callbacks.on_alarm_stop_requested(ctx->user_ctx);
    }
}

static void snooze_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);

    if (ctx->alarms.debug_ringing && !ctx->runtime->alarm_ringing) {
        ctx->alarms.debug_ringing = false;
        ctx->alarms.debug_math = false;
        alm_ring_sync(ctx, alm_now());
        return;
    }
    if (ctx->callbacks.on_alarm_snooze_requested != NULL) {
        ctx->callbacks.on_alarm_snooze_requested(ctx->user_ctx);
    }
}

/* --- Building --------------------------------------------------------------------------------- */

static lv_obj_t *label_at(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, int32_t center_y,
                          int32_t width)
{
    lv_obj_t *l = alm_label(parent, font, color, "");

    lv_obj_set_width(l, width);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, center_y - lv_font_get_line_height(font) / 2);
    return l;
}

static lv_obj_t *hrow(lv_obj_t *parent, int32_t center_y, int32_t height, int32_t gap)
{
    lv_obj_t *r = alm_box(parent);

    lv_obj_set_size(r, LV_SIZE_CONTENT, height);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(r, gap, 0);
    lv_obj_align(r, LV_ALIGN_TOP_MID, 0, center_y - height / 2);
    return r;
}

static void build_instrument_main(clock_ui_context_t *ctx, lv_obj_t *m)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_alarm_state_t *a = &ctx->alarms;
    lv_obj_t *r;
    lv_obj_t *btn;
    lv_obj_t *arc;

    lv_obj_add_event_cb(m, instrument_rim_cb, LV_EVENT_DRAW_MAIN_END, ctx);
    r = hrow(m, 121, 40, 10);
    kit_icon(r, KIT_ICON_ALARM, sk->accent);
    a->ring_title = alm_label(r, &it_medium_28, sk->accent, alm_text(ctx, L10N_ALARM));
    a->ring_time = label_at(m, sk->font_display, sk->text, 222, 600);
    a->ring_sub = label_at(m, sk->font_label, sk->text_2, 317, 520);
    lv_obj_set_style_text_letter_space(a->ring_sub, sk->label_letter_space, 0);

    btn = kit_button(m, KIT_BUTTON_PRIMARY, KIT_ICON_NONE, alm_text(ctx, L10N_SNOOZE));
    lv_obj_set_size(btn, 384, 112);
    lv_obj_set_pos(btn, 168, 364);
    lv_obj_set_style_radius(btn, 20, 0);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(btn, 2, 0);
    lv_obj_set_style_text_font(lv_obj_get_child(btn, 0), &it_medium_34, 0);
    a->ring_snooze_min = kit_label(btn, KIT_TEXT_LABEL, "");
    lv_obj_set_style_text_color(a->ring_snooze_min, sk->accent_text, 0);
    lv_obj_add_event_cb(btn, snooze_cb, LV_EVENT_CLICKED, ctx);
    a->ring_snooze = btn;

    arc = lv_arc_create(m);
    lv_obj_remove_style_all(arc);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(arc, 146, 146);
    lv_obj_set_pos(arc, 360 - 73, 580 - 73);
    lv_arc_set_rotation(arc, 270);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_arc_set_range(arc, 0, 1000);
    lv_arc_set_value(arc, 0);
    lv_obj_set_style_arc_width(arc, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, sk->line, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, sk->accent, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    kit_arc_bake(arc);
    a->ring_hold_arc = arc;

    btn = kit_round_button(m, KIT_BUTTON_SECONDARY, KIT_ICON_NONE, 112);
    lv_obj_set_pos(btn, 304, 524);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(btn, 6, 0);
    kit_icon_small(btn, KIT_ICON_STOP, sk->text);
    r = kit_label(btn, KIT_TEXT_LABEL, alm_str(ctx, &alm_s_hold));
    lv_obj_set_style_text_color(r, sk->text, 0);
    lv_obj_set_style_text_letter_space(r, 2, 0);
    ui_l10n_bind_kit_str(ctx, r, &alm_s_hold);
    lv_obj_add_event_cb(btn, hold_cb, LV_EVENT_PRESSED, ctx);
    lv_obj_add_event_cb(btn, hold_cb, LV_EVENT_RELEASED, ctx);
    lv_obj_add_event_cb(btn, hold_cb, LV_EVENT_PRESS_LOST, ctx);
    a->ring_hold_btn = btn;
}

static void build_orbit_main(clock_ui_context_t *ctx, lv_obj_t *m)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_alarm_state_t *a = &ctx->alarms;
    lv_obj_t *r;
    lv_obj_t *btn;
    lv_obj_t *icon;

    r = hrow(m, 112, 34, 10);
    kit_icon(r, KIT_ICON_BELL, sk->tint_alarms);
    a->ring_title = alm_label(r, sk->font_heading, sk->tint_alarms, alm_text(ctx, L10N_ALARM));
    a->ring_time = label_at(m, sk->font_display, sk->text, 220, 640);
    a->ring_sub = label_at(m, sk->font_body, sk->text_2, 324, 520);

    btn = kit_round_button(m, KIT_BUTTON_PRIMARY, KIT_ICON_NONE, 184);
    kit_set_tint(btn, sk->tint_alarms);
    lv_obj_set_pos(btn, 268, 364);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    r = alm_label(btn, sk->font_title, lv_color_black(), alm_text(ctx, L10N_SNOOZE));
    a->ring_snooze_min = alm_label(btn, &or_medium_22, lv_color_mix(lv_color_black(), sk->tint_alarms, 184), "");
    lv_obj_add_event_cb(btn, snooze_cb, LV_EVENT_CLICKED, ctx);
    a->ring_snooze = btn;

    a->ring_knob = alm_box(m);
    lv_obj_set_size(a->ring_knob, 64, 64);
    lv_obj_set_style_radius(a->ring_knob, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(a->ring_knob, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(a->ring_knob, lv_color_white(), 0);
    lv_obj_set_style_shadow_width(a->ring_knob, 10, 0);
    lv_obj_set_style_shadow_offset_y(a->ring_knob, 4, 0);
    lv_obj_set_style_shadow_opa(a->ring_knob, 90, 0);
    lv_obj_set_style_shadow_color(a->ring_knob, lv_color_black(), 0);
    icon = lv_image_create(a->ring_knob);
    lv_image_set_src(icon, &ui_alarms_arrow_right_32);
    lv_obj_set_style_image_recolor(icon, lv_color_black(), 0);
    lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);
    lv_obj_center(icon);
    lv_obj_add_flag(a->ring_knob, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(a->ring_knob, 16);
}

static void build_atelier_main(clock_ui_context_t *ctx, lv_obj_t *m)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_alarm_state_t *a = &ctx->alarms;
    lv_obj_t *btn;
    lv_obj_t *l;
    lv_obj_t *icon;

    a->ring_title = kit_label(m, KIT_TEXT_LABEL, "");
    lv_obj_set_width(a->ring_title, 400);
    lv_obj_set_style_text_align(a->ring_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(a->ring_title, LV_ALIGN_TOP_MID, 0, 112 - 13);
    lv_obj_set_style_text_color(a->ring_title, sk->accent, 0);
    lv_obj_set_style_text_letter_space(a->ring_title, 5, 0);
    a->ring_time = label_at(m, sk->font_display, sk->text, 214, 640);
    a->ring_sub = label_at(m, sk->font_body, sk->text_2, 302, 600);

    btn = alm_box(m);
    lv_obj_set_size(btn, 320, 112);
    lv_obj_set_pos(btn, 200, 352);
    lv_obj_set_style_radius(btn, 56, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, sk->surface, 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_border_color(btn, lv_color_mix(sk->accent, sk->surface, 200), 0);
    lv_obj_set_style_outline_width(btn, 1, 0);
    lv_obj_set_style_outline_pad(btn, 6, 0);
    lv_obj_set_style_outline_color(btn, sk->line, 0);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(btn, 2, 0);
    kit_make_pressable(btn);
    l = alm_label(btn, sk->font_title, sk->text, alm_text(ctx, L10N_SNOOZE));
    lv_obj_set_style_max_width(l, 280, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_DOTS);
    a->ring_snooze_min = kit_label(btn, KIT_TEXT_LABEL, "");
    lv_obj_set_style_text_color(a->ring_snooze_min, sk->text_2, 0);
    lv_obj_add_event_cb(btn, snooze_cb, LV_EVENT_CLICKED, ctx);
    a->ring_snooze = btn;

    l = kit_label(m, KIT_TEXT_LABEL, alm_str(ctx, &alm_s_slide_to_stop));
    ui_l10n_bind_kit_str(ctx, l, &alm_s_slide_to_stop);
    lv_obj_set_style_text_color(l, sk->value, 0);
    lv_obj_set_width(l, 400);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 654 - 13);

    a->ring_knob = lv_image_create(m);
    lv_image_set_src(a->ring_knob, &ui_alarms_knob_54);
    icon = kit_icon_small(a->ring_knob, KIT_ICON_CHEVRON_RIGHT, sk->accent_text);
    lv_obj_center(icon);
    lv_obj_add_flag(a->ring_knob, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(a->ring_knob, 18);
}

static lv_obj_t *math_key(clock_ui_context_t *ctx, lv_obj_t *parent, int k, int32_t cx, int32_t cy)
{
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *key;
    char text[2] = {(char)('0' + k), '\0'};
    kit_button_kind_t kind = k == KEY_OK ? KIT_BUTTON_PRIMARY : KIT_BUTTON_SECONDARY;
    kit_icon_t icon = k == KEY_OK ? KIT_ICON_CHECK : k == KEY_BKSP ? KIT_ICON_BACKSPACE : KIT_ICON_NONE;

    if (ALM_IS_INSTRUMENT()) {
        key = kit_button(parent, kind, icon, k <= 9 ? text : "");
        lv_obj_set_size(key, 112, 72);
        lv_obj_set_style_pad_hor(key, 0, 0);
        if (k <= 9) {
            lv_obj_set_style_text_font(lv_obj_get_child(key, 0), &it_light_34, 0);
        } else {
            lv_obj_delete(lv_obj_get_child(key, 1)); /* icon only */
        }
        lv_obj_set_pos(key, cx - 56, cy - 36);
    } else {
        int32_t d = ALM_IS_ORBIT() ? 80 : 84;

        key = kit_round_button(parent, kind, icon, d);
        lv_obj_set_pos(key, cx - d / 2, cy - d / 2);
        if (ALM_IS_ATELIER()) {
            lv_obj_set_style_outline_width(key, 0, 0);
            if (k == KEY_BKSP) {
                lv_obj_set_style_bg_color(key, sk->raised, 0);
                lv_obj_set_style_border_color(key, sk->line_strong, 0);
            }
        }
        if (ALM_IS_ORBIT()) {
            if (k == KEY_OK) {
                kit_set_tint(key, sk->tint_alarms);
            } else if (k == KEY_BKSP) {
                lv_obj_set_style_image_recolor(lv_obj_get_child(key, 0), sk->text_2, 0);
            }
        }
        if (k <= 9) {
            lv_obj_t *l = alm_label(key, ALM_IS_ORBIT() ? sk->font_title : &at_serif_40, sk->text, text);

            lv_obj_center(l);
        }
    }
    lv_obj_add_event_cb(key, math_key_cb, LV_EVENT_CLICKED, ctx);
    ctx->alarms.math_keys[k] = key;
    return key;
}

static void math_rim_cb(lv_event_t *e)
{
    alm_draw_rim(lv_event_get_layer(e), 999, 999);
}

static void build_math(clock_ui_context_t *ctx, lv_obj_t *m)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_alarm_state_t *a = &ctx->alarms;
    static const int order[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, KEY_BKSP, 0, KEY_OK};
    lv_obj_t *r;
    lv_obj_t *box;
    lv_obj_t *btn;
    int32_t head_y = ALM_IS_INSTRUMENT() ? 59 : ALM_IS_ORBIT() ? 98 : 78;

    if (ALM_IS_ATELIER()) {
        lv_obj_add_event_cb(m, math_rim_cb, LV_EVENT_DRAW_MAIN_END, NULL);
    }

    /* Header, and the wrong-answer message in its place. */
    if (ALM_IS_ORBIT()) {
        r = hrow(m, head_y, 30, 8);
        kit_icon_small(r, KIT_ICON_BELL, sk->tint_alarms);
        a->math_head = alm_label(r, &or_medium_22, sk->text_2, "");
        lv_label_set_recolor(a->math_head, true);
        a->math_head = r;
        lv_obj_set_user_data(r, lv_obj_get_child(r, 1));
    } else {
        a->math_head = label_at(m, sk->font_label, ALM_IS_ATELIER() ? sk->accent : sk->text_3, head_y, 560);
        lv_obj_set_style_text_letter_space(a->math_head, sk->label_letter_space, 0);
        lv_label_set_recolor(a->math_head, true);
    }
    a->math_error = label_at(m, ALM_IS_ORBIT() ? &or_medium_22 : sk->font_body, sk->danger_text, head_y, 560);
    alm_set_text(a->math_error, alm_text(ctx, L10N_WRONG_ANSWER));
    lv_obj_add_flag(a->math_error, LV_OBJ_FLAG_HIDDEN);

    /* The sum and the answer. */
    if (ALM_IS_INSTRUMENT()) {
        r = hrow(m, 114, 56, 18);
        a->math_problem = alm_label(r, sk->font_time_m, sk->text, "");
        box = alm_box(m);
        lv_obj_set_size(box, 180, 68);
        lv_obj_set_pos(box, 270, 160);
        lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(box, sk->well, 0);
        lv_obj_set_style_border_width(box, 1, 0);
        lv_obj_set_style_border_color(box, sk->line, 0);
        lv_obj_set_style_radius(box, sk->radius_s, 0);
        lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(box, 4, 0);
        a->math_answer = alm_label(box, &it_medium_34, sk->text, "");
        a->math_caret = alm_box(box);
        lv_obj_set_size(a->math_caret, 2, 36);
    } else if (ALM_IS_ORBIT()) {
        r = hrow(m, 160, 72, 18);
        a->math_problem = alm_label(r, sk->font_time_m, sk->text, "");
        box = alm_box(r);
        lv_obj_set_size(box, 128, 72);
        lv_obj_set_style_radius(box, 36, 0);
        lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(box, lv_color_hex(0x2C2C2E), 0);
        lv_obj_set_style_border_width(box, 2, 0);
        lv_obj_set_style_border_color(box, sk->tint_alarms, 0);
        lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(box, 4, 0);
        a->math_answer = alm_label(box, sk->font_time_m, sk->text, "");
        a->math_caret = alm_box(box);
        lv_obj_set_size(a->math_caret, 3, 44);
    } else {
        r = hrow(m, 135, 62, 16);
        a->math_problem = alm_label(r, sk->font_time_m, sk->text, "");
        box = alm_box(r);
        lv_obj_set_size(box, LV_SIZE_CONTENT, 62);
        lv_obj_set_style_min_width(box, 84, 0);
        lv_obj_set_style_border_side(box, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_width(box, 2, 0);
        lv_obj_set_style_border_color(box, sk->accent, 0);
        lv_obj_set_style_pad_hor(box, 10, 0);
        lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(box, 4, 0);
        a->math_answer = alm_label(box, sk->font_time_m, sk->accent, "");
        a->math_caret = alm_box(box);
        lv_obj_set_size(a->math_caret, 2, 44);
    }
    lv_obj_set_style_bg_opa(a->math_caret, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(a->math_caret, ALM_IS_ORBIT() ? sk->tint_alarms : sk->accent, 0);

    /* Keypad, fitted to the circle. */
    for (int i = 0; i < 12; i++) {
        int c = i % 3;
        int row = i / 3;
        int32_t cx;
        int32_t cy;

        if (ALM_IS_INSTRUMENT()) {
            cx = 236 + c * 124;
            cy = 284 + row * 84;
        } else if (ALM_IS_ORBIT()) {
            cx = 260 + c * 100;
            cy = 256 + row * 92;
        } else {
            cx = 260 + c * 100;
            cy = 236 + row * 98;
        }
        math_key(ctx, m, order[i], cx, cy);
    }

    /* Snooze stays one tap away. */
    if (ALM_IS_INSTRUMENT()) {
        lv_obj_t *vr;

        btn = kit_button(m, KIT_BUTTON_SECONDARY, KIT_ICON_NONE, alm_text(ctx, L10N_SNOOZE));
        lv_obj_set_height(btn, 64);
        lv_obj_set_style_radius(btn, 32, 0);
        lv_obj_set_style_pad_hor(btn, 24, 0);
        vr = alm_box(btn);
        lv_obj_set_size(vr, 1, 22);
        lv_obj_set_style_bg_opa(vr, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(vr, sk->line_strong, 0);
        a->math_snooze_label = kit_label(btn, KIT_TEXT_LABEL, "");
        lv_obj_set_style_text_color(a->math_snooze_label, sk->text_2, 0);
        lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 592);
    } else if (ALM_IS_ORBIT()) {
        btn = kit_button(m, KIT_BUTTON_SECONDARY, KIT_ICON_NONE, "");
        lv_obj_set_height(btn, 60);
        lv_obj_set_style_pad_hor(btn, 26, 0);
        lv_obj_set_style_bg_color(btn, sk->surface, 0);
        a->math_snooze_label = lv_obj_get_child(btn, 0);
        lv_obj_set_style_text_font(a->math_snooze_label, &or_medium_22, 0);
        lv_obj_set_style_text_color(a->math_snooze_label, sk->tint_alarms, 0);
        lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 594);
    } else {
        btn = kit_button(m, KIT_BUTTON_SECONDARY, KIT_ICON_NONE, "");
        lv_obj_set_style_pad_hor(btn, 30, 0);
        a->math_snooze_label = lv_obj_get_child(btn, 0);
        lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 604);
    }
    lv_obj_add_event_cb(btn, snooze_cb, LV_EVENT_CLICKED, ctx);
    a->math_snooze = btn;
}

void alm_ring_create(clock_ui_context_t *ctx)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_alarm_state_t *a = &ctx->alarms;

    a->ring = alm_box(ui_chrome_root(ctx));
    lv_obj_set_size(a->ring, KIT_SCREEN, KIT_SCREEN);
    lv_obj_set_style_bg_opa(a->ring, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(a->ring, sk->bg, 0);
    lv_obj_add_flag(a->ring, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN);

    a->ring_main = alm_box(a->ring);
    lv_obj_set_size(a->ring_main, KIT_SCREEN, KIT_SCREEN);
    a->math = alm_box(a->ring);
    lv_obj_set_size(a->math, KIT_SCREEN, KIT_SCREEN);
    lv_obj_add_flag(a->math, LV_OBJ_FLAG_HIDDEN);

    if (ALM_IS_INSTRUMENT()) {
        build_instrument_main(ctx, a->ring_main);
    } else if (ALM_IS_ORBIT()) {
        build_orbit_main(ctx, a->ring_main);
    } else {
        build_atelier_main(ctx, a->ring_main);
    }
    if (a->ring_knob != NULL) {
        lv_obj_add_event_cb(a->ring_knob, slide_cb, LV_EVENT_PRESSED, ctx);
        lv_obj_add_event_cb(a->ring_knob, slide_cb, LV_EVENT_PRESSING, ctx);
        lv_obj_add_event_cb(a->ring_knob, slide_cb, LV_EVENT_RELEASED, ctx);
        lv_obj_add_event_cb(a->ring_knob, slide_cb, LV_EVENT_PRESS_LOST, ctx);
        lv_obj_remove_flag(a->ring_knob, LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_SCROLL_CHAIN_VER);
        ui_attach_click_feedback(a->ring_knob, LV_EVENT_PRESSED);
        slide_place(ctx, ALM_IS_ORBIT() ? O_SLIDE_START : A_SLIDE_START);
    }
    build_math(ctx, a->math);
    a->ring_sec_shown = -1;
}

static void sync_texts(clock_ui_context_t *ctx, time_t now)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_alarm_state_t *a = &ctx->alarms;
    const alarm_config_t *alarm = alm_active_alarm(ctx);
    char text[128];
    char date[64];
    char mins[24];
    struct tm tm;
    const char *sound = alm_str(ctx, alm_sound_name(alarm != NULL ? alarm->sound : ctx->settings->default_alarm_sound));

    localtime_r(&now, &tm);
    alm_format_hm(a->ring_time_text, sizeof(a->ring_time_text), tm.tm_hour, tm.tm_min);
    alm_set_text(a->ring_time, a->ring_time_text);
    alm_set_text(a->ring_title, alm_text(ctx, L10N_ALARM));
    alm_format_minutes(ctx, ctx->settings->snooze_minutes, mins, sizeof(mins));
    alm_set_text(a->ring_snooze_min, mins);
    if (ALM_IS_INSTRUMENT()) {
        kit_button_set_text(a->ring_snooze, alm_text(ctx, L10N_SNOOZE));
        alm_format_short_date(ctx, now, date, sizeof(date));
        if (alarm != NULL) {
            snprintf(text, sizeof(text), "%s \xC2\xB7 %s", date, alm_repeat_title(ctx, alarm));
        } else {
            snprintf(text, sizeof(text), "%s", date);
        }
        kit_text_upper(date, sizeof(date), text);
        alm_set_text(a->ring_sub, date);
    } else if (ALM_IS_ORBIT()) {
        char sum[48] = "";

        if (alarm != NULL) {
            alm_repeat_summary(ctx, alarm, sum, sizeof(sum));
            snprintf(text, sizeof(text), "%s \xC2\xB7 %s", sum, sound);
        } else {
            snprintf(text, sizeof(text), "%s", sound);
        }
        alm_set_text(a->ring_sub, text);
        {
            /* "Snooze" in Title 40 if it fits the 184 px circle, else in Heading 26. */
            lv_obj_t *l = lv_obj_get_child(a->ring_snooze, 0);
            const char *t = alm_text(ctx, L10N_SNOOZE);
            lv_point_t sz;
            const lv_font_t *f;

            lv_text_get_size(&sz, t, sk->font_title, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
            f = sz.x <= 160 ? sk->font_title : sk->font_heading;
            if (lv_obj_get_style_text_font(l, 0) != f) {
                lv_obj_set_style_text_font(l, f, 0);
            }
            alm_set_text(l, t);
        }
    } else {
        alm_format_long_date(ctx, now, date, sizeof(date));
        snprintf(text, sizeof(text), "%s \xC2\xB7 %s", date, sound);
        alm_set_text(a->ring_sub, text);
        alm_set_text(lv_obj_get_child(a->ring_snooze, 0), alm_text(ctx, L10N_SNOOZE));
    }

    /* Math header and snooze. */
    if (ALM_IS_INSTRUMENT()) {
        char head[96];

        kit_text_upper(head, sizeof(head), alm_str(ctx, &alm_s_solve));
        snprintf(text, sizeof(text), "#%06" PRIX32 " %s# \xC2\xB7 %s", lv_color_to_u32(sk->accent) & 0xFFFFFF,
                 a->ring_time_text, head);
        alm_set_text(a->math_head, text);
        kit_button_set_text(a->math_snooze, alm_text(ctx, L10N_SNOOZE));
        alm_set_text(a->math_snooze_label, mins);
    } else if (ALM_IS_ORBIT()) {
        snprintf(text, sizeof(text), "#%06" PRIX32 " %s# %s", lv_color_to_u32(sk->tint_alarms) & 0xFFFFFF,
                 a->ring_time_text, alm_str(ctx, &alm_s_solve_long));
        alm_set_text(lv_obj_get_user_data(a->math_head), text);
        snprintf(text, sizeof(text), "%s \xC2\xB7 %s", alm_text(ctx, L10N_SNOOZE), mins);
        alm_set_text(a->math_snooze_label, text);
    } else {
        char up[96];

        kit_text_upper(up, sizeof(up), alm_str(ctx, &alm_s_solve));
        alm_set_text(a->math_head, up);
        snprintf(text, sizeof(text), "%s \xC2\xB7 %s", alm_text(ctx, L10N_SNOOZE), mins);
        kit_label_set_text_if_changed(a->math_snooze_label, text);
    }
    alm_set_text(a->math_error, alm_text(ctx, L10N_WRONG_ANSWER));
}

void alm_ring_sync(clock_ui_context_t *ctx, time_t now)
{
    clock_ui_alarm_state_t *a = &ctx->alarms;
    bool ringing = alm_ringing(ctx);
    bool shown;

    if (a->ring == NULL) {
        return;
    }
    shown = !lv_obj_has_flag(a->ring, LV_OBJ_FLAG_HIDDEN);
    if (!ringing) {
        if (shown) {
            math_set_visible(ctx, false);
            reset_stop_controls(ctx);
            alm_show(a->ring, false);
        }
        return;
    }
    if (!shown) {
        ensure_background(ctx);
        reset_stop_controls(ctx);
        math_set_visible(ctx, false);
        a->ring_sec_shown = -1;
        alm_show(a->ring, true);
    }
    if (a->math_visible && !requires_math(ctx)) {
        math_set_visible(ctx, false);
    }
    sync_texts(ctx, now);
    if (lv_obj_get_index(a->ring) != (int32_t)lv_obj_get_child_count(lv_obj_get_parent(a->ring)) - 1) {
        lv_obj_move_foreground(a->ring); /* above any page opened while it rings */
    }
}

void alm_ring_tick(clock_ui_context_t *ctx, time_t now)
{
    clock_ui_alarm_state_t *a = &ctx->alarms;
    struct tm tm;

    if (a->ring == NULL || lv_obj_has_flag(a->ring, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }
    localtime_r(&now, &tm);
    if (ALM_IS_INSTRUMENT() && tm.tm_sec != a->ring_sec_shown) {
        if (a->ring_sec_shown >= 0) {
            invalidate_sec_mark(ctx, a->ring_sec_shown);
        }
        a->ring_sec_shown = (int16_t)tm.tm_sec;
        invalidate_sec_mark(ctx, a->ring_sec_shown);
    }
}

void alm_ring_free(clock_ui_context_t *ctx)
{
    clock_ui_alarm_state_t *a = &ctx->alarms;

    if (a->ring_hold_arc != NULL) {
        lv_anim_delete(a->ring_hold_arc, hold_exec_cb);
    }
    lv_anim_delete(ctx, slide_exec_cb);
    if (a->ring_bg != NULL) {
        lv_obj_delete(a->ring_bg);
        a->ring_bg = NULL;
    }
    if (a->ring_bg_buf != NULL) {
        heap_caps_free(a->ring_bg_buf);
        a->ring_bg_buf = NULL;
    }
}

/* --- Banner on the face ------------------------------------------------------------------------ */

/* The face controls carry an alarm chip that already shows the next alarm, and on some
 * faces it sits where the banner does. The banner steps out while the controls are up. */
static bool banner_held(const clock_ui_context_t *ctx)
{
    return ui_face_chrome_controls_visible(ctx);
}

static void banner_set_visible(clock_ui_context_t *ctx, bool visible)
{
    alm_show(ctx->alarms.banner, visible && !banner_held(ctx));
    if (ctx->alarms.banner_visible == visible) {
        return;
    }
    ctx->alarms.banner_visible = visible;
    request_set_temporary_brightness_floor(ctx, visible, brightness_ui_to_hw(BANNER_MIN_BRIGHTNESS_UI_PERCENT));
}

static void banner_click_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);
    time_t now = alm_now();

    if (ctx->alarms.banner_feedback_until > now && ctx->alarms.banner_feedback_revertible &&
        ctx->callbacks.on_next_alarm_cancel_undo_requested != NULL) {
        ctx->callbacks.on_next_alarm_cancel_undo_requested(ctx->user_ctx);
        alm_banner_feedback(ctx, alm_text(ctx, L10N_RESTORED), false, now);
        return;
    }
    if (ctx->callbacks.on_next_alarm_cancel_requested == NULL) {
        return;
    }
    ctx->callbacks.on_next_alarm_cancel_requested(ctx->user_ctx);
    alm_banner_feedback(ctx, alm_text(ctx, L10N_CANCELLED), true, now);
}

void alm_banner_feedback(clock_ui_context_t *ctx, const char *text, bool revertible, time_t now)
{
    ctx->alarms.banner_feedback_until = now + 2;
    ctx->alarms.banner_feedback_revertible = revertible;
    snprintf(ctx->alarms.banner_feedback_text, sizeof(ctx->alarms.banner_feedback_text), "%s", text);
    alm_banner_update(ctx, now);
}

void alm_banner_create(clock_ui_context_t *ctx)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_alarm_state_t *a = &ctx->alarms;
    lv_obj_t *b = alm_box(ui_chrome_root(ctx));
    int32_t h = ALM_IS_INSTRUMENT() ? 44 : 56;

    a->banner = b;
    lv_obj_set_size(b, LV_SIZE_CONTENT, h);
    lv_obj_set_style_max_width(b, 600, 0);
    lv_obj_set_style_radius(b, h / 2, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(b, LV_ALIGN_TOP_MID, 0, BANNER_CENTER_Y - h / 2);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_ext_click_area(b, (72 - h) / 2 + 2);
    kit_make_pressable(b);
    lv_obj_add_event_cb(b, banner_click_cb, LV_EVENT_CLICKED, ctx);

    if (ALM_IS_INSTRUMENT()) {
        lv_obj_set_style_bg_color(b, sk->bg, 0);
        lv_obj_set_style_bg_opa(b, 230, 0);
        lv_obj_set_style_border_color(b, sk->line_strong, 0);
        lv_obj_set_style_pad_hor(b, 18, 0);
        lv_obj_set_style_pad_column(b, 10, 0);
        a->banner_mark = alm_box(b);
        lv_obj_set_size(a->banner_mark, 8, 8);
        lv_obj_set_style_radius(a->banner_mark, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(a->banner_mark, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(a->banner_mark, sk->accent, 0);
        a->banner_time = alm_label(b, sk->font_caption, sk->text, "");
        a->banner_label = kit_label(b, KIT_TEXT_LABEL, "");
        lv_obj_set_style_text_color(a->banner_label, sk->text_2, 0);
    } else if (ALM_IS_ORBIT()) {
        lv_obj_set_style_bg_color(b, lv_color_hex(0x1C1C1E), 0);
        lv_obj_set_style_border_color(b, lv_color_hex(0x333336), 0);
        lv_obj_set_style_pad_left(b, 20, 0);
        lv_obj_set_style_pad_right(b, 24, 0);
        lv_obj_set_style_pad_column(b, 10, 0);
        a->banner_mark = kit_icon_small(b, KIT_ICON_BELL, sk->tint_alarms);
        a->banner_time = alm_label(b, sk->font_heading, sk->text, "");
        a->banner_label = alm_label(b, sk->font_body, sk->text_2, "");
    } else {
        lv_obj_set_style_bg_color(b, sk->surface, 0);
        lv_obj_set_style_border_color(b, sk->accent_deep, 0);
        lv_obj_set_style_pad_left(b, 20, 0);
        lv_obj_set_style_pad_right(b, 24, 0);
        lv_obj_set_style_pad_column(b, 12, 0);
        a->banner_mark = kit_icon_small(b, KIT_ICON_BELL, sk->accent);
        a->banner_time = alm_label(b, sk->font_value, sk->value, "");
        a->banner_label = alm_label(b, sk->font_body, sk->text_2, "");
    }
    alm_one_line(a->banner_label);
    lv_obj_set_width(a->banner_label, LV_SIZE_CONTENT);
    lv_obj_set_style_max_width(a->banner_label, ALM_IS_INSTRUMENT() ? 440 : 400, 0);
    a->banner_face = -1;
    a->banner_kind = BANNER_NONE;
}

static void banner_set(clock_ui_context_t *ctx, uint8_t kind, const char *time_text, const char *text)
{
    clock_ui_alarm_state_t *a = &ctx->alarms;

    if (a->banner_kind != kind) {
        a->banner_kind = kind;
        if (!ALM_IS_INSTRUMENT()) {
            kit_icon_set(a->banner_mark, kind == BANNER_SNOOZE ? KIT_ICON_SNOOZE : KIT_ICON_BELL);
        }
    }
    alm_show(a->banner_time, time_text != NULL && time_text[0] != '\0');
    alm_set_text(a->banner_time, time_text != NULL ? time_text : "");
    alm_set_text(a->banner_label, text);
}

/* Whether the banner has something to say: feedback, a snooze, or an alarm within half an
 * hour. Shared with the night overlay, which steps aside for it. */
bool alm_banner_wanted(const clock_ui_context_t *ctx, time_t now)
{
    const app_runtime_state_t *rt = ctx->runtime;

    return (ctx->alarms.banner_feedback_until > now && ctx->alarms.banner_feedback_text[0] != '\0') ||
           (rt->snooze_active && rt->snooze_deadline > now) ||
           (!rt->alarm_ringing && rt->next_alarm_epoch > now && (rt->next_alarm_epoch - now) <= 1800);
}

void alm_banner_update(clock_ui_context_t *ctx, time_t now)
{
    clock_ui_alarm_state_t *a = &ctx->alarms;
    const app_runtime_state_t *rt = ctx->runtime;
    char text[96];
    char t[8];

    if (a->banner == NULL) {
        return;
    }
    if (a->banner_feedback_until > now && a->banner_feedback_text[0] != '\0') {
        if (a->banner_feedback_revertible) {
            snprintf(text, sizeof(text), "%s \xC2\xB7 %s", a->banner_feedback_text, alm_str(ctx, &alm_s_tap_to_undo));
        } else {
            snprintf(text, sizeof(text), "%s", a->banner_feedback_text);
        }
        banner_set(ctx, BANNER_FEEDBACK, NULL, text);
        banner_set_visible(ctx, true);
        return;
    }
    a->banner_feedback_until = 0;
    a->banner_feedback_revertible = false;
    a->banner_feedback_text[0] = '\0';

    if (rt->snooze_active && rt->snooze_deadline > now) {
        int left = (int)((rt->snooze_deadline - now + 59) / 60);

        ui_format_clock(rt->snooze_deadline, t, sizeof(t));
        snprintf(text, sizeof(text), "%s %d %s", alm_text(ctx, L10N_WAKING_AGAIN_IN), left < 1 ? 1 : left,
                 alm_text(ctx, L10N_MINUTE_ABBR));
        banner_set(ctx, BANNER_SNOOZE, t, text);
        banner_set_visible(ctx, true);
        return;
    }
    if (!rt->alarm_ringing && rt->next_alarm_epoch > now && (rt->next_alarm_epoch - now) <= 1800) {
        int left = (int)((rt->next_alarm_epoch - now + 59) / 60);

        ui_format_clock(rt->next_alarm_epoch, t, sizeof(t));
        snprintf(text, sizeof(text), "%s %d %s", alm_text(ctx, L10N_WAKING_IN), left < 1 ? 1 : left,
                 alm_text(ctx, L10N_MINUTE_ABBR));
        banner_set(ctx, BANNER_WAKING, t, text);
        banner_set_visible(ctx, true);
        return;
    }
    banner_set_visible(ctx, false);
}

/* The banner is printed in the face's own inks on the face's ground, in the spot each face
 * keeps clear (the night alarm row's), so it reads as part of the face. Restyled only when
 * the face or its theme changes. */
void alm_banner_style(clock_ui_context_t *ctx, clock_face_id_t face)
{
    clock_ui_alarm_state_t *a = &ctx->alarms;
    ui_face_ink_t ink;
    int32_t h;

    if (a->banner == NULL) {
        return;
    }
    face = sanitize_enabled_face(face);
    ink = ui_face_ink(ctx, face);
    if (a->banner_face == (int8_t)face && lv_color_eq(a->banner_ink.text, ink.text) &&
        lv_color_eq(a->banner_ink.accent, ink.accent) && lv_color_eq(a->banner_ink.soft, ink.soft) &&
        lv_color_eq(a->banner_ink.paper, ink.paper)) {
        return;
    }
    a->banner_face = (int8_t)face;
    a->banner_ink = ink;
    h = lv_obj_get_style_height(a->banner, 0);
    lv_obj_align(a->banner, LV_ALIGN_TOP_MID, 0, ui_face_chrome_alarm_slot_y(face) + 18 - h / 2);
    lv_obj_set_style_bg_color(a->banner, ink.paper, 0);
    lv_obj_set_style_bg_opa(a->banner, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(a->banner, lv_color_mix(ink.soft, ink.paper, 120), 0);
    lv_obj_set_style_text_color(a->banner_time, ink.text, 0);
    lv_obj_set_style_text_color(a->banner_label, ink.soft, 0);
    if (ALM_IS_INSTRUMENT()) {
        lv_obj_set_style_bg_color(a->banner_mark, ink.accent, 0);
    } else {
        lv_obj_set_style_image_recolor(a->banner_mark, ink.accent, 0);
    }
}
