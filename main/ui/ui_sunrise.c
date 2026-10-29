/*
 * The sunrise before an alarm, on the screen, while the backlight rises out of the night (the
 * app drives it).
 *
 * With a wake-up face set, the night face dissolves into Daybreak as the sunrise starts: the
 * sun climbs from behind its hill with the light (ui_faces_evening.c draws it). It stays
 * through the alarm and dissolves into the day face once the alarm stops; a tap puts it away
 * early. The night hands over to the day halfway through, unseen under it.
 *
 * Without one (or once it is put away), a warm glow comes up from six o'clock over the face,
 * deep amber at first and softer gold as the morning comes, and the night face dissolves into
 * the day face when the night hands over.
 *
 * The layers sit over the faces and under the chrome; only Daybreak takes touches. The glow is
 * a narrow strip tiled across the screen, its alpha dithered: a plain gradient bands in RGB565
 * over the white faces.
 *
 * The preview (Settings > Night > Sunrise) plays it all in under a minute over whatever face is
 * up, the backlight rising with it (the app drives it) and the clock's time running fast to the
 * alarm; a tap ends it, and it hands back to the page it came from.
 */
#include <math.h>

#include "domain/alarm_scheduler.h"
#include "ui/clock_ui_private.h"
#include "ui/ui_areas.h"
#include "ui/ui_face_chrome.h"

#define SUNRISE_GLOW_TOP 252         /* where the glow starts, px down the screen */
#define SUNRISE_GLOW_H (SCREEN_SIZE - SUNRISE_GLOW_TOP)
#define SUNRISE_GLOW_W 16            /* the strip, tiled: a multiple of the dither's 4 */
#define SUNRISE_GLOW_MAX 0.45f       /* its opacity at six o'clock, times its warmth */
#define SUNRISE_GLOW_OUT_MS 1500     /* its fade once the sunrise ends */
#define SUNRISE_CROSSFADE_MS 3000    /* the night face into the day face */
#define DAYBREAK_DISSOLVE_MS 3000    /* Daybreak in over the night face, out to the day face */
#define DAYBREAK_STEP 5              /* progress (of 1000) between redraws: the sun moves under a pixel */
#define PREVIEW_RISE_MS 45000        /* the preview's sunrise */
#define PREVIEW_RING_MS 7000         /* then its alarm ("Good morning", full brightness) */
#define PREVIEW_STEP 20              /* Daybreak's redraws in it: about one a second */
#define PREVIEW_PERIOD_MS 40         /* the backlight's steps in it */

typedef struct {
    lv_obj_t *glow;
    lv_draw_buf_t *glow_buf; /* the strip, ARGB8888 */
    lv_obj_t *fade;          /* the night face as it was, dissolving */
    lv_draw_buf_t *fade_buf; /* its pixels, only while it fades */
    lv_opa_t glow_opa;       /* applied, to redraw only on a change */
    uint32_t glow_color;
    bool glow_shown;
    bool was_night;
    lv_obj_t *day;           /* Daybreak */
    lv_draw_buf_t *day_buf;  /* its pixels, a megabyte, only while it shows */
    bool day_up;             /* shown, or dissolving in */
    bool day_leaving;        /* dissolving out */
    time_t day_alarm;        /* the alarm it wakes for */
    time_t day_dismissed;    /* the alarm whose Daybreak a tap put away */
    uint32_t day_key;        /* what its pixels show */
    lv_obj_t *catcher;       /* over the preview: a tap ends it */
    lv_timer_t *preview;     /* runs the preview */
    uint32_t preview_start;  /* its lv_tick */
    time_t preview_now;      /* the time it started */
    void (*preview_done)(clock_ui_context_t *ctx);
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

/* --- Daybreak ------------------------------------------------------------------------------- */

static void day_opa_exec(void *obj, int32_t v)
{
    lv_obj_set_style_image_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

static void day_gone(lv_anim_t *a)
{
    LV_UNUSED(a);
    lv_obj_add_flag(s_sun.day, LV_OBJ_FLAG_HIDDEN);
    lv_image_set_src(s_sun.day, NULL);
    if (s_sun.day_buf != NULL) {
        lv_image_cache_drop(s_sun.day_buf);
        lv_draw_buf_destroy(s_sun.day_buf);
        s_sun.day_buf = NULL;
    }
    s_sun.day_up = false;
    s_sun.day_leaving = false;
}

static void day_dissolve(int32_t from, int32_t to, lv_anim_completed_cb_t done)
{
    lv_anim_t a;

    lv_anim_delete(s_sun.day, day_opa_exec);
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_sun.day);
    lv_anim_set_exec_cb(&a, day_opa_exec);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, (uint32_t)(DAYBREAK_DISSOLVE_MS * LV_ABS(to - from) / 255));
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_completed_cb(&a, done);
    lv_anim_start(&a);
}

static void day_hide(void)
{
    if (!s_sun.day_up || s_sun.day_leaving) {
        return;
    }
    s_sun.day_leaving = true;
    day_dissolve(lv_obj_get_style_image_opa(s_sun.day, 0), LV_OPA_TRANSP, day_gone);
}

static void preview_end(clock_ui_context_t *ctx, bool hand_back);
static void preview_tap_cb(lv_event_t *e);

static void day_tap_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    s_sun.day_dismissed = s_sun.day_alarm;
    day_hide();
}

/* Draws Daybreak when what it shows moved on: the sun by a step, the minute, the alarm. */
static void day_draw(clock_ui_context_t *ctx, uint32_t progress, time_t now, bool ringing)
{
    struct tm ti;
    uint32_t step = s_sun.preview != NULL ? PREVIEW_STEP : DAYBREAK_STEP;
    uint8_t theme = (uint8_t)(ctx->settings->night_mode.sunrise_face - 1);
    uint32_t key;

    localtime_r(&now, &ti);
    key = progress / step | (uint32_t)(ti.tm_hour * 60 + ti.tm_min) << 8 | (uint32_t)ringing << 19 |
          (uint32_t)theme << 20 | (uint32_t)ctx->settings->language << 22;
    ui_daybreak_view_t view = {
        .theme = theme,
        .progress = (uint16_t)progress,
        .now = now,
        .alarm = s_sun.day_alarm,
        .ringing = ringing,
    };

    if (key == s_sun.day_key) {
        return;
    }
    s_sun.day_key = key;
    ui_daybreak_render(ctx, s_sun.day_buf, &view);
    lv_image_cache_drop(s_sun.day_buf);
    lv_image_set_src(s_sun.day, NULL);
    lv_image_set_src(s_sun.day, s_sun.day_buf);
}

/* Shows Daybreak, dissolving in over the face; false without the megabyte it needs. */
static bool day_show(clock_ui_context_t *ctx, uint32_t progress, time_t now, bool ringing)
{
    if (s_sun.day_buf == NULL) {
        s_sun.day_buf = lv_draw_buf_create(SCREEN_SIZE, SCREEN_SIZE, LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
        if (s_sun.day_buf == NULL) {
            return false;
        }
    }
    s_sun.day_key = UINT32_MAX;
    day_draw(ctx, progress, now, ringing);
    lv_obj_set_style_image_opa(s_sun.day, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(s_sun.day, LV_OBJ_FLAG_HIDDEN);
    s_sun.day_up = true;
    s_sun.day_leaving = false;
    day_dissolve(LV_OPA_TRANSP, LV_OPA_COVER, NULL);
    return true;
}

/* Whether Daybreak should show, and how far through the sunrise. It comes up with a sunrise
 * from the night, and stays through the alarm (a few seconds' grace for the ring to start). */
static bool day_wanted(clock_ui_context_t *ctx, time_t now, uint32_t *progress)
{
    const app_runtime_state_t *rt = ctx->runtime;

    if (ctx->settings->night_mode.sunrise_face == 0) {
        return false;
    }
    if (rt->sunrise_active && rt->sunrise_from_night && rt->next_alarm_epoch > now &&
        rt->next_alarm_epoch != s_sun.day_dismissed) {
        *progress = alarm_scheduler_sunrise_progress(rt, ctx->settings, now);
        if (*progress > 0) {
            s_sun.day_alarm = rt->next_alarm_epoch;
            return true;
        }
    }
    if (s_sun.day_up && !s_sun.day_leaving &&
        (rt->alarm_ringing || (now >= s_sun.day_alarm - 2 && now <= s_sun.day_alarm + 3))) {
        *progress = 1000;
        return true;
    }
    return false;
}

bool ui_sunrise_covers_face(void)
{
    return s_sun.day_up && !s_sun.day_leaving;
}

/* Debug call (simulator timecall): one full Daybreak drawing, halfway through. */
static void debug_daybreak_render(clock_ui_context_t *ctx)
{
    lv_draw_buf_t *buf = lv_draw_buf_create(SCREEN_SIZE, SCREEN_SIZE, LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
    ui_daybreak_view_t view = {.theme = 0, .progress = 500, .now = face_now(), .alarm = face_now() + 600};

    if (buf != NULL) {
        ui_daybreak_render(ctx, buf, &view);
        lv_draw_buf_destroy(buf);
    }
}

static void debug_sunrise_preview(clock_ui_context_t *ctx)
{
    ui_sunrise_preview(ctx, NULL);
}

void ui_sunrise_create(clock_ui_context_t *ctx)
{
    s_sun = (ui_sunrise_state_t) {0};
    s_sun.fade = layer_create(ctx, lv_image_create(ctx->screen));
    s_sun.glow = layer_create(ctx, lv_image_create(ctx->screen));
    lv_obj_set_height(s_sun.glow, SUNRISE_GLOW_H);
    lv_obj_set_y(s_sun.glow, SUNRISE_GLOW_TOP);
    lv_image_set_inner_align(s_sun.glow, LV_IMAGE_ALIGN_TILE);
    s_sun.day = layer_create(ctx, lv_image_create(ctx->screen));
    lv_obj_add_flag(s_sun.day, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_sun.day, day_tap_cb, LV_EVENT_CLICKED, NULL);
    s_sun.catcher = layer_create(ctx, lv_obj_create(ctx->screen));
    lv_obj_add_flag(s_sun.catcher, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_sun.catcher, preview_tap_cb, LV_EVENT_CLICKED, ctx);
    s_sun.was_night = ctx->runtime->in_night_mode;
    ui_debug_register("daybreak_render", debug_daybreak_render);
    ui_debug_register("sunrise_preview", debug_sunrise_preview);
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
    if (!handover || ui_sunrise_covers_face() || s_sun.fade == NULL || s_sun.fade_buf != NULL) {
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
    if (s_sun.preview != NULL) {
        return; /* the preview's timer has it */
    }
    if (day_wanted(ctx, now, &progress)) {
        if (s_sun.day_up && !s_sun.day_leaving) {
            day_draw(ctx, progress, now, rt->alarm_ringing);
        } else if (!s_sun.day_up) {
            day_show(ctx, progress, now, rt->alarm_ringing);
        }
    } else {
        day_hide();
    }
    if (ui_sunrise_covers_face()) {
        glow_hide();
        return;
    }
    progress = 0;
    if (rt->sunrise_active && rt->sunrise_from_night && !rt->alarm_ringing && rt->next_alarm_epoch > now) {
        progress = alarm_scheduler_sunrise_progress(rt, ctx->settings, now);
    }
    if (progress > 0) {
        glow_set(progress);
    } else {
        glow_hide();
    }
}

/* --- The preview ---------------------------------------------------------------------------- */

static void preview_tick_cb(lv_timer_t *timer)
{
    clock_ui_context_t *ctx = lv_timer_get_user_data(timer);
    const app_runtime_state_t *rt = ctx->runtime;
    uint32_t elapsed = lv_tick_elaps(s_sun.preview_start);
    uint32_t window = alarm_scheduler_sunrise_seconds(ctx->settings);
    uint32_t progress = elapsed >= PREVIEW_RISE_MS ? 1000 : elapsed * 1000U / PREVIEW_RISE_MS;
    bool ringing = elapsed >= PREVIEW_RISE_MS;
    time_t now = s_sun.preview_now + (time_t)(window * progress / 1000U);

    /* A real sunrise or alarm, or a page opened over it, ends it. */
    if (rt->alarm_ringing || rt->sunrise_active || ui_settings_is_open(ctx) || ui_alarms_is_open(ctx)) {
        preview_end(ctx, false);
        return;
    }
    if (elapsed >= PREVIEW_RISE_MS + PREVIEW_RING_MS) {
        preview_end(ctx, true);
        return;
    }
    request_sunrise_preview(ctx, (int16_t)(ringing ? 1001 : progress));
    if (ctx->settings->night_mode.sunrise_face != 0 && (s_sun.day_up || day_show(ctx, progress, now, ringing))) {
        if (!s_sun.day_leaving) {
            day_draw(ctx, progress, now, ringing);
        }
        return;
    }
    glow_set(LV_MAX(progress, 1U));
}

static void preview_tap_cb(lv_event_t *e)
{
    preview_end(lv_event_get_user_data(e), true);
}

/* Ends the preview; hand_back returns to the page it was started from, at once. */
static void preview_end(clock_ui_context_t *ctx, bool hand_back)
{
    void (*done)(clock_ui_context_t *ctx) = s_sun.preview_done;

    if (s_sun.preview == NULL) {
        return;
    }
    lv_timer_delete(s_sun.preview);
    s_sun.preview = NULL;
    s_sun.preview_done = NULL;
    lv_obj_add_flag(s_sun.catcher, LV_OBJ_FLAG_HIDDEN);
    request_sunrise_preview(ctx, -1);
    if (hand_back && done != NULL) {
        /* The page covers it all: no dissolve under it. */
        lv_anim_delete(s_sun.day, day_opa_exec);
        if (s_sun.day_up) {
            day_gone(NULL);
        }
        lv_anim_delete(s_sun.glow, glow_out_exec);
        lv_obj_add_flag(s_sun.glow, LV_OBJ_FLAG_HIDDEN);
        s_sun.glow_shown = false;
        done(ctx);
        return;
    }
    day_hide();
    glow_hide();
}

bool ui_sunrise_preview(clock_ui_context_t *ctx, void (*done)(clock_ui_context_t *ctx))
{
    const app_runtime_state_t *rt = ctx->runtime;

    if (s_sun.catcher == NULL || s_sun.preview != NULL || s_sun.day_up || rt->alarm_ringing || rt->sunrise_active) {
        return false;
    }
    s_sun.preview = lv_timer_create(preview_tick_cb, PREVIEW_PERIOD_MS, ctx);
    if (s_sun.preview == NULL) {
        return false;
    }
    ui_settings_close_all(ctx);
    ui_alarms_close_all(ctx);
    ui_face_chrome_close_all(ctx);
    s_sun.preview_start = lv_tick_get();
    s_sun.preview_now = face_now();
    s_sun.preview_done = done;
    s_sun.day_alarm = s_sun.preview_now + (time_t)alarm_scheduler_sunrise_seconds(ctx->settings);
    lv_obj_remove_flag(s_sun.catcher, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_sun.catcher);
    preview_tick_cb(s_sun.preview);
    return true;
}
