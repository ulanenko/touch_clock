/*
 * The sunrise before an alarm, on the screen: while the backlight rises out of the night (the
 * app drives it), a warm glow comes up from six o'clock, deep amber at first and softer gold
 * as the morning comes, and fades once the alarm rings. Halfway through, when the night hands
 * over to the day, the night face dissolves into the day face over a few seconds instead of
 * snapping to it.
 *
 * Both layers sit over the faces and under the chrome, and take no touches. The glow is a
 * narrow strip tiled across the screen, its alpha dithered: a plain gradient bands in RGB565
 * over the white faces.
 */
#include <math.h>

#include "domain/alarm_scheduler.h"
#include "ui/clock_ui_private.h"
#include "ui/ui_areas.h"

#define SUNRISE_GLOW_TOP 252         /* where the glow starts, px down the screen */
#define SUNRISE_GLOW_H (SCREEN_SIZE - SUNRISE_GLOW_TOP)
#define SUNRISE_GLOW_W 16            /* the strip, tiled: a multiple of the dither's 4 */
#define SUNRISE_GLOW_MAX 0.45f       /* its opacity at six o'clock, times its warmth */
#define SUNRISE_GLOW_OUT_MS 1500     /* its fade once the sunrise ends */
#define SUNRISE_CROSSFADE_MS 3000    /* the night face into the day face */

typedef struct {
    lv_obj_t *glow;
    lv_draw_buf_t *glow_buf; /* the strip, ARGB8888 */
    lv_obj_t *fade;          /* the night face as it was, dissolving */
    lv_draw_buf_t *fade_buf; /* its pixels, only while it fades */
    lv_opa_t glow_opa;       /* applied, to redraw only on a change */
    uint32_t glow_color;
    bool glow_shown;
    bool was_night;
} ui_sunrise_state_t;

static ui_sunrise_state_t s_sun;

static const uint8_t s_bayer[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

static float smooth(float x)
{
    x = LV_CLAMP(0.0f, x, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

static uint32_t mix_rgb(uint32_t a, uint32_t b, float t)
{
    uint32_t out = 0;

    for (int shift = 0; shift <= 16; shift += 8) {
        float ca = (float)((a >> shift) & 0xFF);
        float cb = (float)((b >> shift) & 0xFF);

        out |= (uint32_t)lroundf(ca + (cb - ca) * t) << shift;
    }
    return out;
}

static lv_obj_t *layer_create(clock_ui_context_t *ctx, lv_obj_t *obj)
{
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_IGNORE_LAYOUT);
    LV_UNUSED(ctx);
    return obj;
}

void ui_sunrise_create(clock_ui_context_t *ctx)
{
    s_sun = (ui_sunrise_state_t) {0};
    s_sun.fade = layer_create(ctx, lv_image_create(ctx->screen));
    s_sun.glow = layer_create(ctx, lv_image_create(ctx->screen));
    lv_obj_set_height(s_sun.glow, SUNRISE_GLOW_H);
    lv_obj_set_y(s_sun.glow, SUNRISE_GLOW_TOP);
    lv_image_set_inner_align(s_sun.glow, LV_IMAGE_ALIGN_TILE);
    s_sun.was_night = ctx->runtime->in_night_mode;
}

/* --- The glow ---------------------------------------------------------------------------- */

static void glow_out_exec(void *obj, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

static void glow_out_done(lv_anim_t *a)
{
    lv_obj_add_flag((lv_obj_t *)a->var, LV_OBJ_FLAG_HIDDEN);
}

static void glow_hide(void)
{
    lv_anim_t a;

    if (!s_sun.glow_shown) {
        return;
    }
    s_sun.glow_shown = false;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_sun.glow);
    lv_anim_set_exec_cb(&a, glow_out_exec);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_duration(&a, SUNRISE_GLOW_OUT_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_completed_cb(&a, glow_out_done);
    lv_anim_start(&a);
}

/* Clear at the top of the strip to opa at its foot, eased in. The alpha is ordered-dithered in
 * steps of 8 (about a level of the result in RGB565, whose blend drops a level at any alpha at
 * all): a fraction of the pixels takes each step, so the glow comes in without an edge. */
static void glow_render(uint32_t color, lv_opa_t opa)
{
    uint8_t r = (uint8_t)(color >> 16);
    uint8_t g = (uint8_t)(color >> 8);
    uint8_t b = (uint8_t)color;

    for (int32_t y = 0; y < SUNRISE_GLOW_H; y++) {
        uint8_t *row = s_sun.glow_buf->data + (size_t)y * s_sun.glow_buf->header.stride;
        float t = (float)y / (float)(SUNRISE_GLOW_H - 1);
        float a = (float)opa * t * t * (3.0f - 2.0f * t);

        for (int32_t x = 0; x < SUNRISE_GLOW_W; x++) {
            float d = ((float)s_bayer[y & 3][x & 3] + 0.5f) / 16.0f;
            int32_t ai = 8 * (int32_t)floorf(a / 8.0f + d);

            row[x * 4 + 0] = b;
            row[x * 4 + 1] = g;
            row[x * 4 + 2] = r;
            row[x * 4 + 3] = (uint8_t)LV_CLAMP(0, ai, 255);
        }
    }
}

/* Warmth 0.95 at dawn, 0.25 by the alarm; the colour from deep amber to soft gold. */
static void glow_set(uint32_t progress)
{
    float p = (float)progress / 1000.0f;
    float warm = 0.95f - 0.7f * smooth(p);
    lv_opa_t opa = (lv_opa_t)lroundf(SUNRISE_GLOW_MAX * warm * 255.0f);
    uint32_t color = mix_rgb(0xFF7828, 0xFFB45A, smooth(p));

    if (s_sun.glow_buf == NULL) {
        /* 30 KB, kept once a sunrise has been seen. */
        s_sun.glow_buf = lv_draw_buf_create(SUNRISE_GLOW_W, SUNRISE_GLOW_H, LV_COLOR_FORMAT_ARGB8888, LV_STRIDE_AUTO);
        if (s_sun.glow_buf == NULL) {
            return;
        }
    }
    if (!s_sun.glow_shown) {
        lv_anim_delete(s_sun.glow, glow_out_exec);
        lv_obj_set_style_opa(s_sun.glow, LV_OPA_COVER, 0);
        lv_obj_remove_flag(s_sun.glow, LV_OBJ_FLAG_HIDDEN);
        s_sun.glow_shown = true;
        s_sun.glow_opa = 0;
        s_sun.glow_color = 0;
    }
    /* Over twenty minutes these change a step every several seconds: redraw only then. */
    if (opa != s_sun.glow_opa || color != s_sun.glow_color) {
        s_sun.glow_opa = opa;
        s_sun.glow_color = color;
        glow_render(color, opa);
        lv_image_cache_drop(s_sun.glow_buf);
        lv_image_set_src(s_sun.glow, NULL);
        lv_image_set_src(s_sun.glow, s_sun.glow_buf);
    }
}

/* --- The night face into the day face ------------------------------------------------------ */

static void fade_free(void)
{
    lv_obj_add_flag(s_sun.fade, LV_OBJ_FLAG_HIDDEN);
    lv_image_set_src(s_sun.fade, NULL);
    if (s_sun.fade_buf != NULL) {
        lv_draw_buf_destroy(s_sun.fade_buf);
        s_sun.fade_buf = NULL;
    }
}

static void fade_exec(void *obj, int32_t v)
{
    lv_obj_set_style_image_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

static void fade_done(lv_anim_t *a)
{
    LV_UNUSED(a);
    fade_free();
}

void ui_sunrise_before_face_change(clock_ui_context_t *ctx)
{
    const app_runtime_state_t *rt = ctx->runtime;
    bool handover = s_sun.was_night && !rt->in_night_mode && rt->sunrise_from_night && rt->sunrise_active;
    lv_anim_t a;

    s_sun.was_night = rt->in_night_mode;
    if (!handover || s_sun.fade == NULL || s_sun.fade_buf != NULL) {
        return;
    }
    /* A megabyte for three seconds: without it the day face simply comes up. */
    s_sun.fade_buf = lv_draw_buf_create(SCREEN_SIZE, SCREEN_SIZE, LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
    if (s_sun.fade_buf == NULL ||
        lv_snapshot_take_to_draw_buf(ctx->tileview, LV_COLOR_FORMAT_RGB565, s_sun.fade_buf) != LV_RESULT_OK) {
        fade_free();
        return;
    }
    lv_image_set_src(s_sun.fade, s_sun.fade_buf);
    lv_obj_set_style_image_opa(s_sun.fade, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s_sun.fade, LV_OBJ_FLAG_HIDDEN);
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_sun.fade);
    lv_anim_set_exec_cb(&a, fade_exec);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_duration(&a, SUNRISE_CROSSFADE_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_completed_cb(&a, fade_done);
    lv_anim_start(&a);
}

void ui_sunrise_tick(clock_ui_context_t *ctx, time_t now)
{
    const app_runtime_state_t *rt = ctx->runtime;
    uint32_t progress = 0;

    if (s_sun.glow == NULL) {
        return;
    }
    s_sun.was_night = rt->in_night_mode;
    if (rt->sunrise_active && rt->sunrise_from_night && !rt->alarm_ringing && rt->next_alarm_epoch > now) {
        progress = alarm_scheduler_sunrise_progress(rt, ctx->settings, now);
    }
    if (progress > 0) {
        glow_set(progress);
    } else {
        glow_hide();
    }
}
