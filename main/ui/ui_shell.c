/*
 * The faces and the face chrome hooks.
 *
 * Faces: the tileview of faces, the face swipe (sideways drags the faces along as two discs,
 * down pulls the theme sheet, a tap shows the controls) and the snapshot backdrop the
 * pull sheets draw while they move. build_root_ui builds the faces and then the chrome.
 *
 * Face chrome: the ui_face_chrome_* hooks of ui_areas.h dispatch to the face chrome files
 * (see ui_face_chrome.h).
 */
#include "ui/clock_ui_private.h"
#include "ui/ui_areas.h"
#include "ui/ui_face_chrome.h"
#include "ui/kit/ui_glass.h"

#include <math.h>
#include <string.h>

#include "domain/face_catalog.h"

#define FACE_SLIDE_GAP 40      /* px of black between the two discs */
#define FACE_SLIDE_SPAN (SCREEN_SIZE + FACE_SLIDE_GAP)
#define FACE_SLIDE_START 14    /* px sideways before a press on the face becomes a slide */
#define FACE_SLIDE_COMMIT 140 /* px: released past this, the slide goes on to the next face */
#define FACE_SLIDE_FLICK 0.35f /* px/ms: a flick this fast goes on to the next face from anywhere */
#define FACE_SLIDE_OMEGA 0.022f /* 1/ms: the settling spring's stiffness (critically damped) */
#define FACE_SLIDE_PROGRAM_V 0.0f /* px/ms: a slide started by a call starts from rest */
#define FACE_SLIDE_FRESH_MS 5000 /* a snapshot of a face showing seconds is redone after this */
#define FACE_SLIDE_PREPARE_MS 250 /* how often the idle face checks its neighbours' snapshots */
#define FACE_PULL_START 18 /* px down before a press on the face becomes a theme sheet pull */

/* Blacks out a snapshot outside the screen's circle, with an antialiased edge, so the slide
 * can draw it as a plain image: masking it once here is far cheaper than clip_radius on every
 * frame. */
static void face_slide_mask_disc(lv_draw_buf_t *buf)
{
    const float r = SCREEN_SIZE / 2.0f;

    for (int32_t y = 0; y < SCREEN_SIZE; ++y) {
        uint16_t *row = (uint16_t *)(buf->data + (size_t)y * buf->header.stride);
        float dy = (float)y + 0.5f - r;
        float hw = dy * dy < r * r ? sqrtf(r * r - dy * dy) : 0.0f;
        int32_t solid = (int32_t)floorf(r - hw - 0.5f); /* pixels left of this are fully outside */

        solid = LV_CLAMP(0, solid, SCREEN_SIZE / 2);
        memset(row, 0, (size_t)solid * sizeof(uint16_t));
        memset(row + SCREEN_SIZE - solid, 0, (size_t)solid * sizeof(uint16_t));
        for (int32_t x = solid; x < LV_MIN(solid + 3, SCREEN_SIZE / 2); ++x) {
            float cover = LV_CLAMP(0.0f, hw - (r - ((float)x + 0.5f)) + 0.5f, 1.0f);
            uint32_t a = (uint32_t)(cover * 256.0f);

            for (int side = 0; side < 2; ++side) {
                uint16_t *px = &row[side == 0 ? x : SCREEN_SIZE - 1 - x];
                uint32_t v = *px;

                *px = (uint16_t)((((v >> 11) * a >> 8) << 11) | ((((v >> 5) & 0x3F) * a >> 8) << 5) |
                                 ((v & 0x1F) * a >> 8));
            }
        }
    }
}

static lv_draw_buf_t *screen_scratch(clock_ui_context_t *ctx);

/* The buffer of a slide slot. The face the slide starts on goes into the screen scratch
 * buffer, which otherwise holds the backdrop of a moving sheet (never at the same time); the
 * neighbours have buffers of their own. */
static lv_draw_buf_t *face_slide_buf(clock_ui_context_t *ctx, int slot)
{
    lv_draw_buf_t **buf = &ctx->faces.face_slide_bufs[slot];

    if (slot == 0) {
        *buf = screen_scratch(ctx);
    } else if (*buf == NULL) {
        *buf = lv_draw_buf_create(SCREEN_SIZE, SCREEN_SIZE, LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
    }
    return *buf;
}

/* Faces that show seconds go stale within a minute. */
static bool face_shows_seconds(clock_face_id_t face)
{
    return face == CLOCK_FACE_DIGITAL || face == CLOCK_FACE_WHARTON || face == CLOCK_FACE_AVENIR ||
           face == CLOCK_FACE_MODERN_SILVER;
}

static ui_face_slide_shot_t face_slide_shot_now(clock_ui_context_t *ctx, clock_face_id_t face)
{
    return (ui_face_slide_shot_t) {
        .face = face,
        .minute = (long)(face_now() / 60),
        .theme = ctx->settings->face_themes[face],
        .language = (uint8_t)ctx->settings->language,
        .taken_ms = lv_tick_get(),
    };
}

/* Whether a slot still shows the face as it is now. */
static bool face_slide_shot_fresh(clock_ui_context_t *ctx, int slot, clock_face_id_t face)
{
    const ui_face_slide_shot_t *shot = &ctx->faces.face_slide_shots[slot];
    ui_face_slide_shot_t now = face_slide_shot_now(ctx, face);

    return ctx->faces.face_slide_bufs[slot] != NULL && shot->face == face && shot->minute == now.minute &&
           shot->theme == now.theme && shot->language == now.language &&
           (!face_shows_seconds(face) || lv_tick_elaps(shot->taken_ms) < FACE_SLIDE_FRESH_MS);
}

/* Snapshots a face into a slide slot (RGB565, the full screen, masked to the disc). */
static bool face_slide_snapshot(clock_ui_context_t *ctx, clock_face_id_t face, int slot)
{
    lv_obj_t *render = ctx->faces.face_slide_render;
    const void *src = ui_face_preview_source(ctx, face);
    lv_draw_buf_t *buf;

    ctx->faces.face_slide_shots[slot].face = CLOCK_FACE_COUNT;
    if (src == NULL) {
        return false;
    }
    lv_obj_set_size(render, SCREEN_SIZE, SCREEN_SIZE);
    lv_image_set_src(render, src);
    lv_image_set_inner_align(render, LV_IMAGE_ALIGN_COVER);
    lv_obj_update_layout(render);
    buf = face_slide_buf(ctx, slot);
    if (buf == NULL || lv_snapshot_reshape_draw_buf(render, buf) != LV_RESULT_OK ||
        lv_snapshot_take_to_draw_buf(render, LV_COLOR_FORMAT_RGB565, buf) != LV_RESULT_OK) {
        return false;
    }
    face_slide_mask_disc(buf);
    ctx->faces.face_slide_shots[slot] = face_slide_shot_now(ctx, face);
    return true;
}

/* Which face a slot's snapshot shows, to the minute (the frost of it is no finer). */
static uint32_t face_slide_stamp(const ui_face_slide_shot_t *shot)
{
    return 1u + ((uint32_t)shot->face | (uint32_t)shot->theme << 8 | ((uint32_t)shot->minute & 0xFFFFu) << 16);
}

/* The slot of the neighbour on a side: 1 the next face, 2 the previous one. */
static int face_slide_side_slot(int8_t dir)
{
    return dir < 0 ? 2 : 1;
}

/* Draws the two faces (masked to discs when snapshotted) as plain images, the starting one at
 * the slide offset and its neighbour a disc width and the gap beside it, and black where
 * neither is. It runs before the layer's own drawing and stops it: the layer's opaque
 * background only tells LVGL to start each frame here, under it nothing is drawn. */
static void face_slide_draw_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    int32_t offset = ctx->faces.face_slide_offset;
    int32_t xs[2] = {offset, offset + ctx->faces.face_slide_dir * FACE_SLIDE_SPAN};
    int32_t covered_from = SCREEN_SIZE;
    int32_t covered_to = -1;
    lv_draw_rect_dsc_t black;
    lv_area_t gap;

    lv_event_stop_processing(event);
    if (ui_skin_is_glass() && ctx->faces.face_slide_controls_were_visible) {
        int side = face_slide_side_slot(ctx->faces.face_slide_dir);

        /* The controls stay over the slide: their glass shows what slides under them. */
        ui_glass_live_frame(ctx->faces.face_slide_bufs[0], xs[0],
                            ctx->faces.face_slide_dir != 0 ? ctx->faces.face_slide_bufs[side] : NULL, xs[1], side,
                            face_slide_stamp(&ctx->faces.face_slide_shots[side]));
    }
    lv_draw_rect_dsc_init(&black);
    black.bg_color = lv_color_black();
    black.bg_opa = LV_OPA_COVER;
    for (int i = 0; i < 2; ++i) {
        int slot = i == 0 ? 0 : face_slide_side_slot(ctx->faces.face_slide_dir);
        lv_draw_buf_t *buf = ctx->faces.face_slide_bufs[slot];
        lv_draw_image_dsc_t dsc;
        lv_area_t area;

        if (buf == NULL || buf->data == NULL || xs[i] <= -SCREEN_SIZE || xs[i] >= SCREEN_SIZE ||
            (i == 1 && ctx->faces.face_slide_dir == 0)) {
            continue;
        }
        lv_area_set(&area, xs[i], 0, xs[i] + SCREEN_SIZE - 1, SCREEN_SIZE - 1);
        lv_draw_image_dsc_init(&dsc);
        dsc.src = buf;
        dsc.image_area = area;
        lv_draw_image(layer, &dsc, &area);
        covered_from = LV_MIN(covered_from, LV_MAX(0, area.x1));
        covered_to = LV_MAX(covered_to, LV_MIN(SCREEN_SIZE - 1, area.x2));
    }
    /* Black left of, right of and between the discs. */
    if (covered_to < 0) {
        lv_area_set(&gap, 0, 0, SCREEN_SIZE - 1, SCREEN_SIZE - 1);
        lv_draw_rect(layer, &black, &gap);
        return;
    }
    if (covered_from > 0) {
        lv_area_set(&gap, 0, 0, covered_from - 1, SCREEN_SIZE - 1);
        lv_draw_rect(layer, &black, &gap);
    }
    if (covered_to < SCREEN_SIZE - 1) {
        lv_area_set(&gap, covered_to + 1, 0, SCREEN_SIZE - 1, SCREEN_SIZE - 1);
        lv_draw_rect(layer, &black, &gap);
    }
    if (ctx->faces.face_slide_dir != 0) {
        int32_t left = LV_MIN(xs[0], xs[1]);

        lv_area_set(&gap, LV_MAX(0, left + SCREEN_SIZE), 0, LV_MIN(SCREEN_SIZE - 1, left + FACE_SLIDE_SPAN - 1),
                    SCREEN_SIZE - 1);
        if (gap.x1 <= gap.x2) {
            lv_draw_rect(layer, &black, &gap);
        }
    }
}

static void face_slide_set_offset(clock_ui_context_t *ctx, int32_t offset)
{
    ctx->faces.face_slide_offset = LV_CLAMP(-FACE_SLIDE_SPAN, offset, FACE_SLIDE_SPAN);
    lv_obj_invalidate(ctx->faces.face_slide_shell);
}

static clock_face_id_t face_slide_current(clock_ui_context_t *ctx)
{
    return sanitize_enabled_face(ctx->runtime->in_night_mode ? ctx->settings->night_mode.face
                                                             : ctx->settings->current_face);
}

/* Puts the layer up over the face the slide starts on. */
static bool face_slide_begin(clock_ui_context_t *ctx)
{
    clock_face_id_t from = face_slide_current(ctx);

    if (ctx->faces.face_slide_shell == NULL || clock_face_step_enabled(from, 1) == from ||
        !face_slide_snapshot(ctx, from, 0)) {
        return false;
    }
    ctx->faces.face_slide_from = from;
    ctx->faces.face_slide_to = from;
    ctx->faces.face_slide_dir = 0;
    ctx->faces.face_slide_offset = 0;
    ctx->faces.face_slide_velocity = 0.0f;
    ctx->faces.face_slide_active = true;
    ctx->faces.face_slide_controls_were_visible = face_controls_visible(ctx);
    lv_obj_move_foreground(ctx->faces.face_slide_shell);
    if (ctx->faces.face_slide_controls_were_visible) {
        /* The controls stay where they are while the faces slide under them. */
        lv_obj_move_foreground(ctx->chrome_root);
        face_controls_hold(ctx);
    }
    lv_obj_remove_flag(ctx->faces.face_slide_shell, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(ctx->faces.face_slide_shell);
    return true;
}

/* Makes sure the neighbour on the side the starting face moves away from is snapshotted:
 * normally it already is (face_slide_prepare_cb). */
static void face_slide_prepare_side(clock_ui_context_t *ctx, int32_t offset)
{
    int8_t dir = offset < 0 ? 1 : offset > 0 ? -1 : 0;
    int slot = face_slide_side_slot(dir);
    clock_face_id_t to;

    if (dir == 0 || dir == ctx->faces.face_slide_dir) {
        return;
    }
    to = clock_face_step_enabled(ctx->faces.face_slide_from, dir);
    if (!face_slide_shot_fresh(ctx, slot, to) && !face_slide_snapshot(ctx, to, slot)) {
        ctx->faces.face_slide_dir = 0;
        return;
    }
    ctx->faces.face_slide_dir = dir;
    ctx->faces.face_slide_to = to;
}

/* Liquid Glass: snapshots the face and builds the glass backdrop for mode, unless it already
 * shows this face as it is now. */
/* Which face (minute, theme) the glass backdrop was built from. */
static clock_face_id_t s_face = CLOCK_FACE_COUNT;
static long s_minute = -1;
static uint8_t s_theme;

void face_glass_capture(clock_ui_context_t *ctx, int mode_in)
{
    ui_glass_mode_t mode = (ui_glass_mode_t)mode_in;
    clock_face_id_t face;
    long minute = (long)(face_now() / 60);
    bool same_face;

    if (!ui_skin_is_glass() || ctx->faces.face_slide_active) {
        return;
    }
    face = face_slide_current(ctx);
    same_face = ui_glass_backdrop_ready() && face == s_face && minute == s_minute &&
                ctx->settings->face_themes[face] == s_theme;
    if (same_face && ui_glass_backdrop_mode() == mode) {
        return;
    }
    /* The frost (the slow part) is only rebuilt for a new face or minute; going from the face
     * to a page only dims what the glass refracts. */
    if (same_face && mode == UI_GLASS_PAGE && ui_glass_backdrop_to_page(screen_scratch(ctx))) {
        ctx->faces.face_slide_shots[0].face = CLOCK_FACE_COUNT;
        return;
    }
    if (!face_slide_snapshot(ctx, face, 0)) {
        return;
    }
    ui_glass_backdrop_identity((uint32_t)face | (uint32_t)ctx->settings->face_themes[face] << 8);
    ui_glass_backdrop_build(ctx->faces.face_slide_bufs[0], mode, !same_face);
    s_face = face;
    s_minute = minute;
    s_theme = ctx->settings->face_themes[face];
}

/* While the face sits idle, snapshots its neighbours ahead of a slide, one per call: again
 * when a minute turns, and every few seconds for a face showing seconds. Snapshotting a face
 * can draw into buffers the shown face shares (the hand-drawn faces), so the shown face is
 * redrawn straight after, before LVGL renders. */
static void face_slide_prepare_cb(lv_timer_t *timer)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_timer_get_user_data(timer);
    clock_face_id_t current;

    if (ctx->faces.face_slide_active || ctx->faces.face_swipe_tracking || ui_settings_is_open(ctx) ||
        ui_alarms_is_open(ctx) || face_themes_visible(ctx) || face_quick_visible(ctx) ||
        (ctx->runtime != NULL && ctx->runtime->alarm_ringing) || lv_display_get_inactive_time(NULL) < 400) {
        return;
    }
    for (lv_indev_t *indev = lv_indev_get_next(NULL); indev != NULL; indev = lv_indev_get_next(indev)) {
        if (lv_indev_get_state(indev) == LV_INDEV_STATE_PRESSED) {
            return;
        }
    }
    current = face_slide_current(ctx);
    if (face_shell_active_face(ctx) != current) {
        return;
    }
    for (int8_t dir = 1; dir >= -1; dir -= 2) {
        clock_face_id_t face = clock_face_step_enabled(current, dir);
        int slot = face_slide_side_slot(dir);

        if (face == current || face_slide_shot_fresh(ctx, slot, face)) {
            continue;
        }
        face_slide_snapshot(ctx, face, slot);
        update_face(ctx, current);
        return;
    }
    /* Liquid Glass: the neighbours' frost too, for the controls' glass over a slide. */
    for (int8_t dir = 1; ui_skin_is_glass() && dir >= -1; dir -= 2) {
        int slot = face_slide_side_slot(dir);
        uint32_t stamp = face_slide_stamp(&ctx->faces.face_slide_shots[slot]);

        if (ctx->faces.face_slide_shots[slot].face != CLOCK_FACE_COUNT && !ui_glass_side_frost_ready(slot, stamp)) {
            ui_glass_side_frost(slot, ctx->faces.face_slide_bufs[slot], stamp);
            return;
        }
    }
    /* Liquid Glass: keep the face's glass ready, so the controls come up at once, and bake
     * what the chrome will show a little at a time (one tick does at most a few shapes). */
    if (!face_controls_visible(ctx)) {
        face_glass_capture(ctx, UI_GLASS_FACE);
        if (ui_glass_backdrop_ready()) {
            ui_glass_prewarm(ctx->chrome_root, 20);
        }
    }
}

/* Liquid Glass, the controls up over the slide: the face it ends on becomes the backdrop from
 * the frost built for it at idle, and the controls keep their live glass, so they are right
 * the moment the slide stops (building the backdrop and baking them again takes a while). */
static bool face_slide_glass_finish(clock_ui_context_t *ctx, bool changed, clock_face_id_t face)
{
    int8_t dir = ctx->faces.face_slide_dir;
    bool kept;

    if (!ui_skin_is_glass() || !ctx->faces.face_slide_controls_were_visible) {
        return false;
    }
    if (!changed) {
        return ui_glass_live_finish(ctx->faces.face_slide_bufs[0], 0, 0, 0, 0);
    }
    kept = ui_glass_live_finish(ctx->faces.face_slide_bufs[face_slide_side_slot(dir)], face_slide_side_slot(dir),
                                face_slide_stamp(&ctx->faces.face_slide_shots[face_slide_side_slot(dir)]),
                                face_slide_side_slot((int8_t)-dir), face_slide_stamp(&ctx->faces.face_slide_shots[0]));
    if (kept) {
        s_face = face;
        s_minute = ctx->faces.face_slide_shots[face_slide_side_slot(dir)].minute;
        s_theme = ctx->settings->face_themes[face];
        ui_glass_backdrop_identity((uint32_t)face | (uint32_t)ctx->settings->face_themes[face] << 8);
    }
    return kept;
}

static void face_slide_finish(lv_anim_t *anim)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_anim_get_user_data(anim);
    bool changed = LV_ABS(ctx->faces.face_slide_offset) >= FACE_SLIDE_SPAN && ctx->faces.face_slide_dir != 0;
    clock_face_id_t face = changed ? ctx->faces.face_slide_to : ctx->faces.face_slide_from;

    if (!face_slide_glass_finish(ctx, changed, face)) {
        ui_glass_live_end();
    }
    /* Snapshotting the neighbour may have drawn into buffers this face shares with it
     * (the hand-drawn faces): bring the face up to date before the layer goes. */
    update_face(ctx, face);
    if (changed) {
        /* The face the slide left is the new face's neighbour on the other side: keep its
         * snapshot there (into that side's buffer: slot 0 is the scratch buffer). */
        int back = face_slide_side_slot((int8_t)-ctx->faces.face_slide_dir);
        lv_draw_buf_t *dst = face_slide_buf(ctx, back);
        lv_draw_buf_t *src = ctx->faces.face_slide_bufs[0];

        ctx->faces.face_slide_shots[back].face = CLOCK_FACE_COUNT;
        if (dst != NULL && src != NULL && dst->data_size >= src->data_size) {
            lv_draw_buf_copy(dst, NULL, src, NULL);
            ctx->faces.face_slide_shots[back] = ctx->faces.face_slide_shots[0];
        }
        ctx->faces.face_slide_shots[face_slide_side_slot(ctx->faces.face_slide_dir)].face = CLOCK_FACE_COUNT;
    }
    ctx->faces.face_slide_active = false;
    ctx->faces.face_slide_dir = 0;
    lv_obj_add_flag(ctx->faces.face_slide_shell, LV_OBJ_FLAG_HIDDEN);
    set_active_face(ctx, face, LV_ANIM_OFF);
    if (changed) {
        if (ctx->runtime->in_night_mode) {
            if (ctx->settings->night_mode.face != face) {
                request_set_night_face(ctx, face);
            }
        } else if (ctx->settings->current_face != face) {
            request_set_current_face(ctx, face);
        }
    }
    /* The controls stayed up through the slide: now they show the new face (its glass, its
     * page dot) and their hide timer runs again. */
    if (ctx->faces.face_slide_controls_were_visible) {
        face_controls_show_still(ctx);
    }
}

/* The settle follows a critically damped spring, x(t) = T + (d + (v + w d) t) e^(-w t) with
 * d = x0 - T: it starts at the finger's speed and comes to rest without overshooting. */
static float face_slide_spring_x(const clock_ui_context_t *ctx, float t)
{
    float d = ctx->faces.face_slide_spring_x0 - (float)ctx->faces.face_slide_spring_target;
    float w = FACE_SLIDE_OMEGA;

    return (float)ctx->faces.face_slide_spring_target + (d + (ctx->faces.face_slide_spring_v0 + w * d) * t) * expf(-w * t);
}

static void face_slide_anim_cb(void *var, int32_t value)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)var;

    face_slide_set_offset(ctx, (int32_t)lroundf(face_slide_spring_x(ctx, (float)value)));
}

static void face_slide_settle(clock_ui_context_t *ctx, int32_t target, float v0)
{
    float d = (float)ctx->faces.face_slide_offset - (float)target;
    uint32_t ms = 0;
    lv_anim_t anim;

    /* Moving towards the target faster than w d would overshoot it: cap the speed there. */
    if (v0 * d < 0.0f && fabsf(v0) > FACE_SLIDE_OMEGA * fabsf(d)) {
        v0 = (v0 < 0.0f ? -1.0f : 1.0f) * FACE_SLIDE_OMEGA * fabsf(d);
    }
    ctx->faces.face_slide_spring_x0 = (float)ctx->faces.face_slide_offset;
    ctx->faces.face_slide_spring_v0 = v0;
    ctx->faces.face_slide_spring_target = target;
    /* Done once within half a pixel of the target. */
    while (ms < 1000 && fabsf(face_slide_spring_x(ctx, (float)ms) - (float)target) >= 0.5f) {
        ms += 8;
    }
    lv_anim_delete(ctx, face_slide_anim_cb);
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, ctx);
    lv_anim_set_user_data(&anim, ctx);
    lv_anim_set_exec_cb(&anim, face_slide_anim_cb);
    lv_anim_set_values(&anim, 0, (int32_t)LV_MAX(ms, 8));
    lv_anim_set_duration(&anim, LV_MAX(ms, 8));
    lv_anim_set_path_cb(&anim, lv_anim_path_linear);
    lv_anim_set_completed_cb(&anim, face_slide_finish);
    lv_anim_start(&anim);
}

bool face_shell_slide_drag(clock_ui_context_t *ctx, lv_point_t start, lv_point_t point)
{
    uint32_t now = lv_tick_get();
    int32_t offset;
    bool began = false;

    if (!ctx->faces.face_slide_dragging) {
        int32_t dx = point.x - start.x;
        int32_t dy = point.y - start.y;

        if (ctx->faces.face_slide_active) {
            /* Caught while settling: carry on from where the faces are. */
            lv_anim_delete(ctx, face_slide_anim_cb);
            ctx->faces.face_slide_anchor_x = point.x - ctx->faces.face_slide_offset;
        } else {
            if (LV_ABS(dx) < FACE_SLIDE_START || LV_ABS(dx) <= LV_ABS(dy) + 8 || !face_slide_begin(ctx)) {
                return false;
            }
            /* From where the finger came down, less the dead zone: touch is read only every
             * few frames, and a quick swipe has gone well past the dead zone by the first
             * read that starts the slide. */
            ctx->faces.face_slide_anchor_x = start.x + (dx < 0 ? -FACE_SLIDE_START : FACE_SLIDE_START);
        }
        ctx->faces.face_slide_dragging = true;
        began = true;
    }

    offset = point.x - ctx->faces.face_slide_anchor_x;
    face_slide_prepare_side(ctx, offset);
    /* Timed after the snapshots, which can hold up the UI thread: the time they take is not
     * the finger's. */
    now = lv_tick_get();
    if (began) {
        ctx->faces.face_slide_last_x = point.x;
        ctx->faces.face_slide_last_ms = now;
        ctx->faces.face_slide_start_ms = now;
        ctx->faces.face_slide_velocity = 0.0f;
        ctx->faces.face_slide_start_x = point.x;
    } else if (point.x != ctx->faces.face_slide_last_x && now != ctx->faces.face_slide_last_ms) {
        float v = (float)(point.x - ctx->faces.face_slide_last_x) / (float)(now - ctx->faces.face_slide_last_ms);

        ctx->faces.face_slide_velocity = 0.6f * v + 0.4f * ctx->faces.face_slide_velocity;
        ctx->faces.face_slide_last_x = point.x;
        ctx->faces.face_slide_last_ms = now;
    }
    ctx->faces.face_slide_read_ms = now;
    face_slide_set_offset(ctx, offset);
    return true;
}

bool face_shell_slide_dragging(const clock_ui_context_t *ctx)
{
    return ctx->faces.face_slide_dragging;
}

void face_shell_slide_release(clock_ui_context_t *ctx)
{
    int32_t offset = ctx->faces.face_slide_offset;
    float v = ctx->faces.face_slide_velocity;
    int32_t target = 0;

    if (!ctx->faces.face_slide_dragging) {
        return;
    }
    ctx->faces.face_slide_dragging = false;
    /* A quick, short swipe is a flick by its average speed, whatever its last samples say. */
    {
        uint32_t took = ctx->faces.face_slide_last_ms - ctx->faces.face_slide_start_ms;
        float average = took > 0 ? (float)(ctx->faces.face_slide_last_x - ctx->faces.face_slide_start_x) / (float)took
                                 : 0.0f;

        if (took < 350 && LV_ABS(average) > LV_ABS(v)) {
            v = average;
        }
    }
    /* A finger seen resting before the lift has no speed left. Only touch reads count: a
     * stalled UI thread reads nothing and must not turn a flick into a rest. */
    if (ctx->faces.face_slide_read_ms - ctx->faces.face_slide_last_ms > 80) {
        v = 0.0f;
    }
    if (ctx->faces.face_slide_dir != 0 && offset != 0) {
        bool flick = LV_ABS(v) >= FACE_SLIDE_FLICK && (v < 0) == (offset < 0);
        bool back_flick = LV_ABS(v) >= FACE_SLIDE_FLICK && (v < 0) != (offset < 0);

        if ((LV_ABS(offset) >= FACE_SLIDE_COMMIT && !back_flick) || flick) {
            target = offset < 0 ? -FACE_SLIDE_SPAN : FACE_SLIDE_SPAN;
        }
    }
    face_slide_settle(ctx, target, v);
}

clock_face_id_t face_shell_active_face(clock_ui_context_t *ctx)
{
    return tile_to_face(ctx, lv_tileview_get_tile_active(ctx->tileview));
}

void face_shell_sync_face_visuals(clock_ui_context_t *ctx)
{
    if (ctx == NULL || ctx->tileview == NULL) {
        return;
    }

    sync_face_animation_state(ctx, face_shell_active_face(ctx));
}

/*
 * While a sheet is dragged or animating, the face under it is shown as a snapshot drawn
 * with image opacity over the overlay's opaque black background, instead of the live face
 * under a translucent black scrim. It looks the same, but the opaque overlay lets LVGL start
 * each frame at the overlay, so the live face and the full-screen scrim blend are skipped.
 * The snapshot is taken when the movement starts, so the face holds still until it ends.
 */
lv_obj_t *ui_backdrop_create(lv_obj_t *overlay)
{
    lv_obj_t *backdrop = lv_image_create(overlay);

    lv_obj_move_to_index(backdrop, 0);
    lv_obj_add_flag(backdrop, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
    lv_obj_remove_flag(backdrop, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(backdrop, 0, 0);
    return backdrop;
}

/* A full-screen RGB565 buffer for passing uses: the backdrop of a moving sheet, the face a
 * slide starts on. */
static lv_draw_buf_t *screen_scratch(clock_ui_context_t *ctx)
{
    if (!ctx->backdrop_ready) {
        uint32_t stride = SCREEN_SIZE * 2U;
        uint32_t size = stride * SCREEN_SIZE;
        void *data = heap_caps_aligned_alloc(CONFIG_CACHE_L2_CACHE_LINE_SIZE, size, MALLOC_CAP_SPIRAM);

        if (data == NULL) {
            return NULL;
        }
        if (lv_draw_buf_init(&ctx->backdrop_buf, SCREEN_SIZE, SCREEN_SIZE, LV_COLOR_FORMAT_RGB565,
                             stride, data, size) != LV_RESULT_OK) {
            heap_caps_free(data);
            return NULL;
        }
        ctx->backdrop_ready = true;
    }
    return &ctx->backdrop_buf;
}

static bool backdrop_capture(clock_ui_context_t *ctx, lv_obj_t *overlay)
{
    lv_result_t res;

    if (screen_scratch(ctx) == NULL) {
        return false;
    }
    /* The slide's snapshot of its starting face shares this buffer. */
    ctx->faces.face_slide_shots[0].face = CLOCK_FACE_COUNT;
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    res = lv_snapshot_take_to_draw_buf(ctx->screen, LV_COLOR_FORMAT_RGB565, &ctx->backdrop_buf);
    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    return res == LV_RESULT_OK;
}

void ui_backdrop_set(clock_ui_context_t *ctx, lv_obj_t *overlay, lv_obj_t *backdrop, bool moving, lv_opa_t dim_opa)
{
    bool shown;

    if (ctx == NULL || overlay == NULL || backdrop == NULL) {
        return;
    }

    shown = !lv_obj_has_flag(backdrop, LV_OBJ_FLAG_HIDDEN);
    if (moving && !shown && backdrop_capture(ctx, overlay)) {
        lv_image_set_src(backdrop, &ctx->backdrop_buf);
        lv_obj_invalidate(backdrop);
        lv_obj_remove_flag(backdrop, LV_OBJ_FLAG_HIDDEN);
        shown = true;
    } else if (!moving && shown) {
        lv_obj_add_flag(backdrop, LV_OBJ_FLAG_HIDDEN);
        shown = false;
    }

    if (shown) {
        lv_opa_t image_opa = LV_OPA_COVER - dim_opa;

        if (lv_obj_get_style_bg_opa(overlay, LV_PART_MAIN) != LV_OPA_COVER) {
            lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
        }
        if (lv_obj_get_style_image_opa(backdrop, LV_PART_MAIN) != image_opa) {
            lv_obj_set_style_image_opa(backdrop, image_opa, 0);
        }
    } else if (lv_obj_get_style_bg_opa(overlay, LV_PART_MAIN) != dim_opa) {
        lv_obj_set_style_bg_opa(overlay, dim_opa, 0);
    }
}

void face_theme_preview_colors(clock_face_id_t face, uint8_t theme, uint32_t colors[3])
{
    if (colors == NULL || evening_face_preview_colors(face, theme, colors)) {
        return;
    }

    switch (face) {
    case CLOCK_FACE_DIGITAL:
        if (theme == 1U) {
            colors[0] = 0x02060B;
            colors[1] = 0x123248;
            colors[2] = 0x6FD8FF;
        } else if (theme == 2U) {
            colors[0] = 0x090401;
            colors[1] = 0x4A2B12;
            colors[2] = 0xFFB347;
        } else {
            colors[0] = 0x010401;
            colors[1] = 0x143C14;
            colors[2] = 0x5CFB5C;
        }
        break;
    case CLOCK_FACE_MATRIX:
        if (theme == 1U) {
            colors[0] = 0x070502;
            colors[1] = 0x1C1205;
            colors[2] = 0xFFB200;
        } else if (theme == 2U) {
            colors[0] = 0x03070C;
            colors[1] = 0x0C1A28;
            colors[2] = 0x5CC8FF;
        } else {
            colors[0] = 0x050505;
            colors[1] = 0x071107;
            colors[2] = 0x00CC44;
        }
        break;
    case CLOCK_FACE_WHARTON:
        if (theme == 1U) {
            colors[0] = 0x0A0507;
            colors[1] = 0x1F0A10;
            colors[2] = 0xFF5C7A;
        } else if (theme == 2U) {
            colors[0] = 0x04090A;
            colors[1] = 0x0D2124;
            colors[2] = 0x7FE7FF;
        } else {
            colors[0] = 0x0A0907;
            colors[1] = 0x1E1600;
            colors[2] = 0xD4A017;
        }
        break;
    case CLOCK_FACE_STERNGLAS:
        if (theme == 1U) {
            colors[0] = 0xF5F5F5;
            colors[1] = 0x9A5B2A;
            colors[2] = 0xC97B42;
        } else if (theme == 2U) {
            colors[0] = 0x14181D;
            colors[1] = 0x87B7FF;
            colors[2] = 0xCFE2FF;
        } else {
            colors[0] = 0xF5F5F5;
            colors[1] = 0x104F8C;
            colors[2] = 0x1862A8;
        }
        break;
    case CLOCK_FACE_AVENIR:
        if (theme == 1U) {
            colors[0] = 0xF5A65C;
            colors[1] = 0x173A5E;
            colors[2] = 0x295B86;
        } else if (theme == 2U) {
            colors[0] = 0x26493F;
            colors[1] = 0xA4C3B0;
            colors[2] = 0xF3E7D0;
        } else {
            colors[0] = 0xF5A65C;
            colors[1] = 0xC9782F;
            colors[2] = 0x2A2A2A;
        }
        break;
    case CLOCK_FACE_MODERN_SILVER:
        if (theme == 1U) {
            colors[0] = 0xE7EAED;
            colors[1] = 0x1B1B1B;
            colors[2] = 0xE04545;
        } else if (theme == 2U) {
            colors[0] = 0x0F1215;
            colors[1] = 0xF1F3F5;
            colors[2] = 0xFF9B42;
        } else {
            colors[0] = 0xE7EAED;
            colors[1] = 0x111111;
            colors[2] = 0x0095FF;
        }
        break;
    default:
        colors[0] = 0x1A1A1A;
        colors[1] = 0x3A3A3A;
        colors[2] = 0xD8DDE3;
        break;
    }
}

clock_face_id_t tile_to_face(clock_ui_context_t *ctx, lv_obj_t *tile)
{
    for (int i = 0; i < CLOCK_FACE_COUNT; ++i) {
        if (!clock_face_is_enabled((clock_face_id_t)i)) {
            continue;
        }
        if (ctx->tiles[i] == tile) {
            return (clock_face_id_t)i;
        }
    }

    return clock_face_first_enabled();
}

static void apply_face_navigation_mode(clock_ui_context_t *ctx, clock_face_id_t face)
{
    LV_UNUSED(face);
    lv_obj_set_scroll_dir(ctx->tileview, LV_DIR_NONE);
}

void update_dots(clock_ui_context_t *ctx, clock_face_id_t active_face)
{
    face_controls_set_face(ctx, active_face);
}

void face_shell_swipe_face(clock_ui_context_t *ctx, int32_t dx)
{
    if (ctx->faces.face_slide_active) {
        return;
    }
    if (!face_slide_begin(ctx)) {
        return;
    }
    face_slide_prepare_side(ctx, dx < 0 ? -1 : 1);
    if (ctx->faces.face_slide_dir == 0) {
        lv_anim_t anim;

        lv_anim_init(&anim);
        lv_anim_set_user_data(&anim, ctx);
        face_slide_finish(&anim);
        return;
    }
    face_slide_settle(ctx, dx < 0 ? -FACE_SLIDE_SPAN : FACE_SLIDE_SPAN, FACE_SLIDE_PROGRAM_V);
}

static bool theme_pull_active(const clock_ui_context_t *ctx)
{
    return ctx->face_theme.dragging && ctx->face_theme.drag_from_closed;
}

/* Presses on the face area (and on the themes tab of the controls, which routes here). */
static void face_swipe_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    lv_event_code_t code = lv_event_get_code(event);
    lv_indev_t *indev = lv_event_get_indev(event);
    lv_point_t point;
    lv_coord_t dx;
    lv_coord_t dy;

    if (indev == NULL || ui_settings_is_open(ctx) || ui_alarms_is_open(ctx) || face_quick_visible(ctx) ||
        (face_themes_visible(ctx) && !theme_pull_active(ctx))) {
        ctx->faces.face_swipe_tracking = false;
        return;
    }

    lv_indev_get_point(indev, &point);
    if (code == LV_EVENT_PRESSED) {
        ctx->faces.face_swipe_start_point = point;
        ctx->faces.face_swipe_tracking = true;
        return;
    }
    if (!ctx->faces.face_swipe_tracking) {
        return;
    }
    dx = point.x - ctx->faces.face_swipe_start_point.x;
    dy = point.y - ctx->faces.face_swipe_start_point.y;

    if (code == LV_EVENT_PRESSING) {
        if (!theme_pull_active(ctx)) {
            if (face_shell_slide_drag(ctx, ctx->faces.face_swipe_start_point, point)) {
                return;
            }
            if (dy < FACE_PULL_START || dy < LV_ABS(dx) + 12 || ctx->faces.face_slide_active) {
                return;
            }
            face_themes_prepare(ctx);
            face_sheet_drag_begin(&ctx->face_theme.sheet, ctx->faces.face_swipe_start_point, true);
        }
        face_sheet_drag_to(&ctx->face_theme.sheet, dy);
        return;
    }

    ctx->faces.face_swipe_tracking = false;
    if (theme_pull_active(ctx)) {
        face_sheet_drag_end(&ctx->face_theme.sheet, code == LV_EVENT_PRESS_LOST);
        return;
    }
    if (face_shell_slide_dragging(ctx)) {
        face_shell_slide_release(ctx);
        return;
    }
    if (code != LV_EVENT_RELEASED || ctx->faces.face_slide_active) {
        return;
    }

    if (LV_ABS(dx) < 24 && LV_ABS(dy) < 24) {
        if (lv_event_get_current_target(event) == ctx->face_chrome.themes_tab) {
            face_themes_open(ctx);
        } else {
            face_controls_toggle(ctx);
        }
        return;
    }
    /* A swipe so quick that it ended before the drag began (the UI thread was busy). */
    if (LV_ABS(dx) >= 56 && LV_ABS(dx) > LV_ABS(dy) + 20) {
        face_shell_swipe_face(ctx, dx);
    }
}

void face_shell_attach_swipe(clock_ui_context_t *ctx, lv_obj_t *obj)
{
    lv_obj_add_event_cb(obj, face_swipe_event_cb, LV_EVENT_PRESSED, ctx);
    lv_obj_add_event_cb(obj, face_swipe_event_cb, LV_EVENT_PRESSING, ctx);
    lv_obj_add_event_cb(obj, face_swipe_event_cb, LV_EVENT_RELEASED, ctx);
    lv_obj_add_event_cb(obj, face_swipe_event_cb, LV_EVENT_PRESS_LOST, ctx);
}

static void create_face_slide_layer(clock_ui_context_t *ctx)
{
    ctx->faces.face_slide_render = lv_image_create(ctx->screen);
    lv_obj_set_pos(ctx->faces.face_slide_render, -2000, -2000);
    lv_obj_clear_flag(ctx->faces.face_slide_render, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(ctx->faces.face_slide_render, LV_OBJ_FLAG_CLICKABLE);

    /* Not clickable: a press while the faces settle reaches the swipe layer under it and
     * catches them. */
    ctx->faces.face_slide_shell = lv_obj_create(ctx->screen);
    lv_obj_remove_style_all(ctx->faces.face_slide_shell);
    lv_obj_set_size(ctx->faces.face_slide_shell, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_color(ctx->faces.face_slide_shell, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(ctx->faces.face_slide_shell, LV_OPA_COVER, 0);
    lv_obj_remove_flag(ctx->faces.face_slide_shell, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ctx->faces.face_slide_shell, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
    lv_obj_add_event_cb(ctx->faces.face_slide_shell, face_slide_draw_cb,
                        (lv_event_code_t)(LV_EVENT_DRAW_MAIN | LV_EVENT_PREPROCESS), ctx);
    for (int slot = 0; slot < 3; ++slot) {
        ctx->faces.face_slide_shots[slot].face = CLOCK_FACE_COUNT;
    }
    ctx->faces.face_slide_prepare_timer = lv_timer_create(face_slide_prepare_cb, FACE_SLIDE_PREPARE_MS, ctx);
}

static void create_face_swipe_layer(clock_ui_context_t *ctx)
{
    ctx->faces.face_swipe_layer = lv_obj_create(ctx->screen);
    lv_obj_set_size(ctx->faces.face_swipe_layer, SCREEN_SIZE, SCREEN_SIZE - BOTTOM_EDGE_ZONE - 8);
    lv_obj_align(ctx->faces.face_swipe_layer, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(ctx->faces.face_swipe_layer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ctx->faces.face_swipe_layer, 0, 0);
    lv_obj_set_style_radius(ctx->faces.face_swipe_layer, 0, 0);
    lv_obj_set_style_pad_all(ctx->faces.face_swipe_layer, 0, 0);
    lv_obj_clear_flag(ctx->faces.face_swipe_layer, LV_OBJ_FLAG_SCROLLABLE);
    face_shell_attach_swipe(ctx, ctx->faces.face_swipe_layer);
}

void set_active_face(clock_ui_context_t *ctx, clock_face_id_t face, lv_anim_enable_t anim)
{
    int visible_index;

    if (!clock_face_is_valid(face)) {
        face = clock_face_first_enabled();
    }
    if (!clock_face_is_enabled(face)) {
        face = clock_face_first_enabled();
    }

    visible_index = clock_face_visible_id_to_index(face);
    if (visible_index < 0) {
        face = clock_face_first_enabled();
        visible_index = clock_face_visible_id_to_index(face);
    }

    /* A refresh re-applies the same face: only a real face change closes the theme sheet. */
    if (ctx->tileview != NULL && face != tile_to_face(ctx, lv_tileview_get_tile_active(ctx->tileview))) {
        face_themes_close(ctx, true);
    }
    ctx->suppress_events = true;
    apply_face_navigation_mode(ctx, face);
    sync_face_animation_state(ctx, face);
    lv_tileview_set_tile_by_index(ctx->tileview, visible_index, 0, anim);
    update_dots(ctx, face);
    ui_alarms_on_face_changed(ctx, face);
    face_night_sync(ctx, face_now(), false);
    ctx->suppress_events = false;
}

static void tileview_value_changed_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    lv_obj_t *active_tile;
    clock_face_id_t face;

    LV_UNUSED(event);

    if (ctx->suppress_events) {
        return;
    }

    active_tile = lv_tileview_get_tile_active(ctx->tileview);
    face = tile_to_face(ctx, active_tile);
    apply_face_navigation_mode(ctx, face);
    sync_face_animation_state(ctx, face);
    update_dots(ctx, face);
    ui_alarms_on_face_changed(ctx, face);
    show_affordances_temporarily(ctx);

    if (ctx->runtime->in_night_mode) {
        if (ctx->settings->night_mode.face != face) {
            request_set_night_face(ctx, face);
        }
    } else if (ctx->settings->current_face != face) {
        request_set_current_face(ctx, face);
    }
}

static void tileview_scroll_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    lv_event_code_t code = lv_event_get_code(event);
    clock_face_id_t face;

    LV_UNUSED(event);
    if (code == LV_EVENT_SCROLL_BEGIN) {
        ctx->faces.tileview_scrolling = true;
        face = tile_to_face(ctx, lv_tileview_get_tile_active(ctx->tileview));
        sync_face_animation_state(ctx, face);
        if (!ctx->suppress_events) {
            show_affordances_temporarily(ctx);
        }
        return;
    }

    if (code == LV_EVENT_SCROLL_END) {
        ctx->faces.tileview_scrolling = false;
        face = tile_to_face(ctx, lv_tileview_get_tile_active(ctx->tileview));
        sync_face_animation_state(ctx, face);
        update_face(ctx, face);
        update_dots(ctx, face);
        if (!ctx->suppress_events) {
            show_affordances_temporarily(ctx);
        }
    }
}

void build_root_ui(clock_ui_context_t *ctx)
{
    ui_install_theme_overrides(lv_display_get_default());
    ui_ppa_blend_install();
    ui_l10n_reset(ctx);
    ctx->screen = lv_screen_active();
    lv_obj_set_style_bg_color(ctx->screen, lv_color_black(), 0);
    lv_obj_set_style_pad_all(ctx->screen, 0, 0);
    lv_obj_remove_flag(ctx->screen, LV_OBJ_FLAG_SCROLLABLE);

    ctx->tileview = lv_tileview_create(ctx->screen);
    lv_obj_set_size(ctx->tileview, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_color(ctx->tileview, lv_color_black(), 0);
    lv_obj_set_style_pad_all(ctx->tileview, 0, 0);
    lv_obj_set_style_border_width(ctx->tileview, 0, 0);
    lv_obj_set_scrollbar_mode(ctx->tileview, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(ctx->tileview, LV_OBJ_FLAG_SCROLL_ONE);
    lv_obj_remove_flag(ctx->tileview, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_set_style_anim_duration(ctx->tileview, 0, 0);
    lv_obj_set_scroll_snap_x(ctx->tileview, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_scroll_snap_y(ctx->tileview, LV_SCROLL_SNAP_NONE);
    lv_obj_align(ctx->tileview, LV_ALIGN_CENTER, 0, 0);

    for (int index = 0; index < clock_face_visible_count(); ++index) {
        clock_face_id_t face = clock_face_visible_index_to_id(index);

        ctx->tiles[face] = lv_tileview_add_tile(ctx->tileview, index, 0, LV_DIR_HOR);
        lv_obj_set_style_pad_all(ctx->tiles[face], 0, 0);
        lv_obj_set_style_border_width(ctx->tiles[face], 0, 0);
    }

    create_digital_face(ctx, ctx->tiles[CLOCK_FACE_DIGITAL]);
    create_matrix_face(ctx, ctx->tiles[CLOCK_FACE_MATRIX]);
    create_wharton_face(ctx, ctx->tiles[CLOCK_FACE_WHARTON]);
    create_sternglas_face(ctx, ctx->tiles[CLOCK_FACE_STERNGLAS]);
    create_avenir_face(ctx, ctx->tiles[CLOCK_FACE_AVENIR]);
    create_modern_silver_face(ctx, ctx->tiles[CLOCK_FACE_MODERN_SILVER]);
    create_ember_face(ctx, ctx->tiles[CLOCK_FACE_EMBER]);
    create_nocturne_face(ctx, ctx->tiles[CLOCK_FACE_NOCTURNE]);
    create_filament_face(ctx, ctx->tiles[CLOCK_FACE_FILAMENT]);

    lv_obj_add_event_cb(ctx->tileview, tileview_value_changed_cb, LV_EVENT_VALUE_CHANGED, ctx);
    lv_obj_add_event_cb(ctx->tileview, tileview_scroll_event_cb, LV_EVENT_SCROLL_BEGIN, ctx);
    lv_obj_add_event_cb(ctx->tileview, tileview_scroll_event_cb, LV_EVENT_SCROLL, ctx);
    lv_obj_add_event_cb(ctx->tileview, tileview_scroll_event_cb, LV_EVENT_SCROLL_END, ctx);

    create_face_slide_layer(ctx);
    create_face_swipe_layer(ctx);
    ui_chrome_build(ctx);
    set_active_face(ctx, ctx->settings->current_face, LV_ANIM_OFF);
#if CONFIG_TOUCH_CLOCK_PERF_TOUR
    ui_perf_tour_start(ctx);
#endif
}

/* --- Debug calls (simulator, perf tour) ------------------------------------------------- */

static void debug_close_theme(clock_ui_context_t *ctx)
{
    face_themes_close(ctx, true);
}

static void debug_close_quick(clock_ui_context_t *ctx)
{
    face_quick_close(ctx, false);
}

static void debug_open_theme(clock_ui_context_t *ctx)
{
    face_themes_prepare(ctx);
    face_sheet_open(&ctx->face_theme.sheet, false);
}

static void debug_open_quick(clock_ui_context_t *ctx)
{
    face_quick_sync(ctx);
    face_sheet_open(&ctx->brightness.sheet, false);
}

static void debug_hide_controls(clock_ui_context_t *ctx)
{
    face_controls_hide(ctx);
}

/* Cycles the theme of the active face (renders the theme previews in the simulator). */
static void debug_next_theme(clock_ui_context_t *ctx)
{
    clock_face_id_t face = face_shell_active_face(ctx);
    uint8_t theme = ctx->settings->face_themes[face];

    request_set_face_theme(ctx, face, (uint8_t)((theme + 1U) % CLOCK_FACE_THEME_COUNT));
}
/* Glass timing (the simulator's "timecall"): the frost rebuilt for the face shown, the switch
 * to a page, and bakes of a page row and of a sheet-sized panel. */
static void debug_glass_backdrop(clock_ui_context_t *ctx)
{
    if (ui_skin_is_glass() && face_slide_snapshot(ctx, face_slide_current(ctx), 0)) {
        ui_glass_backdrop_build(ctx->faces.face_slide_bufs[0], UI_GLASS_FACE, true);
    }
}

static void debug_glass_to_page(clock_ui_context_t *ctx)
{
    ui_glass_backdrop_to_page(screen_scratch(ctx));
}

static void debug_glass_bake_row(clock_ui_context_t *ctx)
{
    LV_UNUSED(ctx);
    ui_glass_debug_bake(500, 88, 32, UI_GLASS_CLEAR);
}

static void debug_glass_bake_panel(clock_ui_context_t *ctx)
{
    LV_UNUSED(ctx);
    ui_glass_debug_bake(600, 572, 64, UI_GLASS_CLEAR);
}


/* --- Hooks (ui_areas.h) ----------------------------------------------------------------- */

void ui_face_chrome_create(clock_ui_context_t *ctx)
{
    face_night_create(ctx);
    face_quick_create_edge_sensor(ctx);
    face_controls_create(ctx);
    if (ctx->face_chrome.themes_tab != NULL) {
        face_shell_attach_swipe(ctx, ctx->face_chrome.themes_tab);
    }
    ui_debug_register("glass_backdrop", debug_glass_backdrop);
    ui_debug_register("glass_to_page", debug_glass_to_page);
    ui_debug_register("glass_bake_row", debug_glass_bake_row);
    ui_debug_register("glass_bake_panel", debug_glass_bake_panel);
    ui_debug_register("show_controls", ui_face_chrome_show_controls);
    ui_debug_register("boot_intro", ui_boot_replay);
    ui_debug_register("hide_controls", debug_hide_controls);
    ui_debug_register("open_theme", debug_open_theme);
    ui_debug_register("close_theme", debug_close_theme);
    ui_debug_register("open_quick", debug_open_quick);
    ui_debug_register("close_quick", debug_close_quick);
    ui_debug_register("next_theme", debug_next_theme);
    face_night_register_debug();
}

void ui_face_chrome_create_top(clock_ui_context_t *ctx)
{
    face_themes_create(ctx);
    face_quick_create(ctx);
    /* The bottom pull zone stays above the alarm banner, as before. */
    lv_obj_move_foreground(ctx->brightness.edge_sensor);
}

void ui_face_chrome_destroy(clock_ui_context_t *ctx)
{
    for (int i = 0; i < 2; i++) {
        if (ctx->face_chrome.rim_band[i] != NULL) {
            lv_draw_buf_destroy(ctx->face_chrome.rim_band[i]);
            ctx->face_chrome.rim_band[i] = NULL;
        }
    }
    face_themes_destroy(ctx);
    face_quick_destroy(ctx);
}

void ui_face_chrome_tick(clock_ui_context_t *ctx, time_t now, clock_face_id_t active_face, bool face_covered)
{
    if (!face_covered && !ctx->faces.tileview_scrolling) {
        face_controls_set_face(ctx, active_face);
    }
    if (face_controls_visible(ctx)) {
        if (ctx->runtime->alarm_ringing) {
            face_controls_hide(ctx);
        } else {
            face_controls_sync(ctx, now, false);
        }
    }
    face_night_sync(ctx, now, false);
    if (face_quick_visible(ctx)) {
        face_quick_sync(ctx);
    }
}

void ui_face_chrome_refresh(clock_ui_context_t *ctx)
{
    time_t now = face_now();

    face_quick_sync(ctx);
    face_themes_sync(ctx);
    face_controls_sync(ctx, now, true);
    face_night_sync(ctx, now, true);
}

void ui_face_chrome_on_face_changed(clock_ui_context_t *ctx, clock_face_id_t face)
{
    face_controls_set_face(ctx, face);
    face_night_sync(ctx, face_now(), false);
}

void ui_face_chrome_show_controls(clock_ui_context_t *ctx)
{
    face_controls_show(ctx);
}

void ui_face_chrome_set_hidden(clock_ui_context_t *ctx, bool hidden)
{
    face_controls_set_hidden(ctx, hidden);
    face_quick_set_hidden(ctx, hidden);
    if (hidden) {
        face_themes_close(ctx, false);
        face_quick_close(ctx, false);
    }
    face_night_sync(ctx, face_now(), false);
}

void ui_face_chrome_close_all(clock_ui_context_t *ctx)
{
    face_quick_close(ctx, false);
    face_themes_close(ctx, false);
    face_controls_hide(ctx);
    face_night_sync(ctx, face_now(), false);
}

bool ui_face_chrome_covers_face(const clock_ui_context_t *ctx)
{
    /* While a sheet moves, the face is drawn from the backdrop snapshot. */
    return face_sheet_is_moving(&ctx->face_theme.sheet) || face_sheet_is_moving(&ctx->brightness.sheet);
}

bool ui_face_chrome_dims_face(const clock_ui_context_t *ctx)
{
    return face_themes_visible(ctx) || face_quick_visible(ctx);
}

bool ui_face_chrome_controls_visible(const clock_ui_context_t *ctx)
{
    return face_controls_visible(ctx);
}
