/*
 * Pull sheets over the face: the theme sheet (from the top) and the quick actions sheet (from
 * the bottom) share this code.
 *
 * Performance (see docs/performance_audit.md):
 * - While a sheet moves, the face under it is a dimmed snapshot (ui_backdrop_set) and the
 *   panel is a snapshot image, so a pull frame draws two images and nothing live.
 * - The dim follows the sheet position continuously.
 * - At rest the panel is live over the live face under a translucent overlay, so the face
 *   keeps ticking (and a theme picked in the sheet shows at once below it).
 */
#include "ui/kit/ui_glass.h"
#include "ui/ui_face_chrome.h"
#include "ui/ui_areas.h"

#define SHEET_DRAG_SLOP 6
#define SHEET_OPEN_FROM_CLOSED_PCT 45 /* a pull from the face opens past 45 % of the travel */
#define SHEET_CLOSE_FROM_OPEN_PCT 80  /* a push on the open sheet closes below 80 % */

static void sheet_anim_cb(void *var, int32_t value);

static int32_t sheet_travel(const face_sheet_t *s)
{
    return LV_ABS(s->open_y - s->closed_y);
}

static int32_t sheet_clamp(const face_sheet_t *s, int32_t y)
{
    return LV_CLAMP(LV_MIN(s->open_y, s->closed_y), y, LV_MAX(s->open_y, s->closed_y));
}

static int32_t sheet_ext(const face_sheet_t *s)
{
    int32_t panel_w = lv_obj_get_width(s->panel);

    if (s->snapshot_buf != NULL && (int32_t)s->snapshot_buf->header.w > panel_w) {
        return ((int32_t)s->snapshot_buf->header.w - panel_w) / 2;
    }
    return 0;
}

static int32_t sheet_current_y(const face_sheet_t *s)
{
    if (s->snapshot_visible) {
        return lv_obj_get_y(s->snapshot_img) + sheet_ext(s);
    }
    return lv_obj_get_y(s->panel);
}

/* 0..256 of the way from closed to open. */
static int32_t sheet_progress(const face_sheet_t *s, int32_t y)
{
    int32_t travel = sheet_travel(s);

    if (travel <= 0) {
        return 256;
    }
    return (LV_ABS(sheet_clamp(s, y) - s->closed_y) * 256) / travel;
}

static void sheet_apply(face_sheet_t *s, int32_t y)
{
    lv_opa_t opa;

    y = sheet_clamp(s, y);
    opa = (lv_opa_t)((sheet_progress(s, y) * s->dim_opa) / 256);
    ui_backdrop_set(s->ui, s->overlay, s->backdrop_img, s->snapshot_visible, opa);
    if (s->snapshot_visible) {
        int32_t ext = sheet_ext(s);

        lv_obj_set_pos(s->snapshot_img, lv_obj_get_x(s->panel) - ext, y - ext);
    } else if (lv_obj_get_y(s->panel) != y) {
        lv_obj_set_y(s->panel, y);
    }
}

/* Only one sheet moves at a time, so the sheets share one panel snapshot buffer: the sheet
 * that last took it owns it, and another sheet taking it over leaves the owner to snapshot
 * its panel again when it next moves. */
static lv_draw_buf_t *s_snapshot_pool;
static face_sheet_t *s_snapshot_owner;

static bool sheet_take_snapshot(face_sheet_t *s)
{
    lv_draw_buf_t **buf = &s_snapshot_pool;
    int32_t y_was = LV_COORD_MAX;
    bool ok;

    if (s_snapshot_owner != s && s_snapshot_owner != NULL) {
        if (s_snapshot_owner->snapshot_visible) {
            return false; /* the other sheet is showing it: this one moves live */
        }
        s_snapshot_owner->snapshot_buf = NULL;
        s_snapshot_owner->snapshot_dirty = true;
    }
    s_snapshot_owner = s;
    s->snapshot_buf = NULL;
    /* Liquid Glass: the glass refracts what is behind it where the sheet comes to rest, so the
     * snapshot is taken there (a closed sheet sits off the screen, with nothing behind it). */
    if (ui_skin_is_glass()) {
        y_was = lv_obj_get_y(s->panel);
        lv_obj_set_y(s->panel, s->open_y);
    }
    lv_obj_update_layout(s->panel);
    ok = true;
    if (*buf == NULL || lv_snapshot_reshape_draw_buf(s->panel, *buf) != LV_RESULT_OK) {
        if (*buf != NULL) {
            lv_draw_buf_destroy(*buf);
        }
        *buf = lv_snapshot_create_draw_buf(s->panel, LV_COLOR_FORMAT_RGB565);
        ok = *buf != NULL;
    }
    if (ok && lv_snapshot_take_to_draw_buf(s->panel, LV_COLOR_FORMAT_RGB565, *buf) != LV_RESULT_OK) {
        ok = false;
    }
    if (y_was != LV_COORD_MAX) {
        lv_obj_set_y(s->panel, y_was);
        lv_obj_update_layout(s->panel);
    }
    if (!ok) {
        return false;
    }
    s->snapshot_glass = ui_glass_backdrop_generation();
    s->snapshot_buf = *buf;
    lv_image_set_src(s->snapshot_img, *buf);
    lv_obj_invalidate(s->snapshot_img);
    return true;
}

static void sheet_set_snapshot_visible(face_sheet_t *s, bool visible)
{
    int32_t y;

    if (s->snapshot_visible == visible) {
        return;
    }
    y = sheet_current_y(s);
    if (visible) {
        if (s->snapshot_dirty || s->snapshot_buf == NULL ||
            (ui_skin_is_glass() && s->snapshot_glass != ui_glass_backdrop_generation())) {
            if (!sheet_take_snapshot(s)) {
                return; /* no memory: the live panel moves instead */
            }
            s->snapshot_dirty = false;
        }
        s->snapshot_visible = true;
        lv_obj_remove_flag(s->snapshot_img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s->panel, LV_OBJ_FLAG_HIDDEN);
    } else {
        s->snapshot_visible = false;
        lv_obj_set_y(s->panel, y);
        lv_obj_add_flag(s->snapshot_img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s->panel, LV_OBJ_FLAG_HIDDEN);
    }
    sheet_apply(s, y);
}

static void sheet_notify(face_sheet_t *s)
{
    if (s->on_state != NULL) {
        s->on_state(s->ui);
    }
}

static void sheet_auto_close_pause(face_sheet_t *s)
{
    if (s->auto_close_timer != NULL) {
        lv_timer_pause(s->auto_close_timer);
    }
}

void face_sheet_activity(face_sheet_t *s)
{
    if (s == NULL || s->auto_close_timer == NULL || !face_sheet_is_visible(s)) {
        return;
    }
    lv_timer_set_period(s->auto_close_timer, s->auto_close_ms);
    lv_timer_reset(s->auto_close_timer);
    lv_timer_resume(s->auto_close_timer);
}

/* Shows the overlay above everything else in the chrome. */
static void sheet_show_overlay(face_sheet_t *s)
{
    bool was_hidden = lv_obj_has_flag(s->overlay, LV_OBJ_FLAG_HIDDEN);

    s->overlay_open = true;
    if (was_hidden) {
        lv_obj_remove_flag(s->overlay, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_move_foreground(s->overlay);
    if (was_hidden) {
        /* Hides the face controls and switches the digital face to its snapshot before the
         * backdrop is captured. */
        sheet_notify(s);
    }
}

static void sheet_finish(face_sheet_t *s)
{
    s->animating = false;
    s->dragging = false;
    sheet_set_snapshot_visible(s, false);
    if (s->target_open) {
        sheet_apply(s, s->open_y);
        face_sheet_activity(s);
    } else {
        sheet_apply(s, s->closed_y);
        s->overlay_open = false;
        lv_obj_add_flag(s->overlay, LV_OBJ_FLAG_HIDDEN);
        sheet_auto_close_pause(s);
    }
    sheet_notify(s);
}

static void sheet_anim_ready_cb(lv_anim_t *anim)
{
    face_sheet_t *s = (face_sheet_t *)lv_anim_get_user_data(anim);

    if (s != NULL && s->panel != NULL) {
        sheet_finish(s);
    }
}

static void sheet_anim_cb(void *var, int32_t value)
{
    face_sheet_t *s = (face_sheet_t *)lv_obj_get_user_data((lv_obj_t *)var);

    if (s != NULL) {
        sheet_apply(s, value);
    }
}

static void sheet_stop_anim(face_sheet_t *s)
{
    if (s->animating) {
        lv_anim_delete(s->panel, sheet_anim_cb);
        s->animating = false;
    }
}

static uint32_t sheet_anim_ms(const face_sheet_t *s, int32_t from, int32_t to)
{
    int32_t travel = sheet_travel(s);

    if (travel <= 0) {
        return 110;
    }
    return (uint32_t)LV_CLAMP(110, 110 + (LV_ABS(to - from) * 150) / travel, 260);
}

static void sheet_animate_to(face_sheet_t *s, bool open)
{
    int32_t target = open ? s->open_y : s->closed_y;
    int32_t from;
    lv_anim_t anim;

    sheet_stop_anim(s);
    s->dragging = false;
    s->target_open = open;
    if (open) {
        sheet_show_overlay(s);
    }
    from = sheet_current_y(s);
    if (from == target || lv_obj_has_flag(s->overlay, LV_OBJ_FLAG_HIDDEN)) {
        sheet_finish(s);
        return;
    }

    sheet_set_snapshot_visible(s, true);
    s->animating = true;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, s->panel);
    lv_anim_set_user_data(&anim, s);
    lv_anim_set_exec_cb(&anim, sheet_anim_cb);
    lv_anim_set_values(&anim, from, target);
    lv_anim_set_duration(&anim, sheet_anim_ms(s, from, target));
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_set_ready_cb(&anim, sheet_anim_ready_cb);
    lv_anim_start(&anim);
}

bool face_sheet_is_visible(const face_sheet_t *s)
{
    return s != NULL && s->overlay != NULL && !lv_obj_has_flag(s->overlay, LV_OBJ_FLAG_HIDDEN);
}

bool face_sheet_is_open(const face_sheet_t *s)
{
    return face_sheet_is_visible(s) && s->target_open && !s->animating && !s->dragging;
}

bool face_sheet_is_moving(const face_sheet_t *s)
{
    return face_sheet_is_visible(s) && s->snapshot_visible;
}

void face_sheet_open(face_sheet_t *s, bool animate)
{
    if (s == NULL || s->overlay == NULL) {
        return;
    }
    if (!face_sheet_is_visible(s)) {
        sheet_set_snapshot_visible(s, false);
        lv_obj_set_y(s->panel, s->closed_y);
    }
    if (animate) {
        sheet_animate_to(s, true);
        return;
    }
    sheet_stop_anim(s);
    s->target_open = true;
    sheet_show_overlay(s);
    sheet_finish(s);
}

void face_sheet_close(face_sheet_t *s, bool animate)
{
    if (s == NULL || s->overlay == NULL) {
        return;
    }
    sheet_auto_close_pause(s);
    if (!face_sheet_is_visible(s)) {
        s->overlay_open = false;
        s->dragging = false;
        return;
    }
    if (animate) {
        sheet_animate_to(s, false);
        return;
    }
    sheet_stop_anim(s);
    s->target_open = false;
    sheet_finish(s);
}

void face_sheet_drag_begin(face_sheet_t *s, lv_point_t start, bool from_closed)
{
    sheet_stop_anim(s);
    s->dragging = true;
    s->drag_moved = false;
    s->drag_from_closed = from_closed;
    s->drag_start_point = start;
    if (from_closed) {
        if (!face_sheet_is_visible(s)) {
            sheet_set_snapshot_visible(s, false);
            lv_obj_set_y(s->panel, s->closed_y);
        }
        s->target_open = false;
        sheet_show_overlay(s);
        sheet_set_snapshot_visible(s, true);
        s->drag_start_y = s->closed_y;
        sheet_apply(s, s->closed_y);
    } else {
        s->drag_start_y = sheet_current_y(s);
    }
    face_sheet_activity(s);
}

void face_sheet_drag_to(face_sheet_t *s, int32_t dy)
{
    if (s == NULL || !s->dragging) {
        return;
    }
    if (!s->drag_moved) {
        if (LV_ABS(dy) < SHEET_DRAG_SLOP) {
            return;
        }
        s->drag_moved = true;
        s->click_blocked = true;
        sheet_set_snapshot_visible(s, true);
    }
    sheet_apply(s, s->drag_start_y + dy);
    face_sheet_activity(s);
}

void face_sheet_drag_end(face_sheet_t *s, bool cancel)
{
    int32_t progress;
    bool open;

    if (s == NULL || !s->dragging) {
        return;
    }
    s->dragging = false;
    if (!s->drag_moved) {
        if (s->drag_from_closed) {
            sheet_animate_to(s, false);
        }
        return;
    }
    progress = sheet_progress(s, sheet_current_y(s));
    if (cancel) {
        open = !s->drag_from_closed;
    } else if (s->drag_from_closed) {
        open = progress >= (256 * SHEET_OPEN_FROM_CLOSED_PCT) / 100;
    } else {
        open = progress > (256 * SHEET_CLOSE_FROM_OPEN_PCT) / 100;
    }
    sheet_animate_to(s, open);
}

bool face_sheet_click_blocked(const face_sheet_t *s)
{
    return s != NULL && s->click_blocked;
}

void face_sheet_mark_dirty(face_sheet_t *s)
{
    if (s != NULL) {
        s->snapshot_dirty = true;
    }
}

static void sheet_drag_event_cb(lv_event_t *e)
{
    face_sheet_t *s = (face_sheet_t *)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_event_get_indev(e);
    lv_point_t point = {0, 0};

    if (s == NULL || indev == NULL) {
        return;
    }
    lv_indev_get_point(indev, &point);
    switch (code) {
    case LV_EVENT_PRESSED:
        s->click_blocked = false;
        if (face_sheet_is_visible(s) && (s->target_open || s->animating)) {
            face_sheet_drag_begin(s, point, false);
        }
        break;
    case LV_EVENT_PRESSING:
        face_sheet_drag_to(s, point.y - s->drag_start_point.y);
        break;
    case LV_EVENT_RELEASED:
        face_sheet_drag_end(s, false);
        break;
    case LV_EVENT_PRESS_LOST:
        face_sheet_drag_end(s, true);
        break;
    default:
        break;
    }
}

void face_sheet_attach_drag(face_sheet_t *s, lv_obj_t *obj)
{
    lv_obj_add_event_cb(obj, sheet_drag_event_cb, LV_EVENT_PRESSED, s);
    lv_obj_add_event_cb(obj, sheet_drag_event_cb, LV_EVENT_PRESSING, s);
    lv_obj_add_event_cb(obj, sheet_drag_event_cb, LV_EVENT_RELEASED, s);
    lv_obj_add_event_cb(obj, sheet_drag_event_cb, LV_EVENT_PRESS_LOST, s);
}

static void sheet_overlay_click_cb(lv_event_t *e)
{
    face_sheet_t *s = (face_sheet_t *)lv_event_get_user_data(e);

    if (lv_event_get_target(e) == lv_event_get_current_target(e) && !s->dragging) {
        face_sheet_close(s, true);
    }
}

static void sheet_auto_close_cb(lv_timer_t *timer)
{
    face_sheet_t *s = (face_sheet_t *)lv_timer_get_user_data(timer);

    if (!face_sheet_is_visible(s)) {
        lv_timer_pause(timer);
        return;
    }
    if (s->dragging || s->animating) {
        face_sheet_activity(s);
        return;
    }
    face_sheet_close(s, true);
}

void face_sheet_create(face_sheet_t *s, clock_ui_context_t *ctx, const face_sheet_opts_t *o)
{
    s->ui = ctx;
    s->from_top = o->from_top;
    s->open_y = o->open_y;
    s->closed_y = o->from_top ? -o->height : SCREEN_SIZE;
    s->dim_opa = ui_skin()->sheet_dim_opa;
    s->auto_close_ms = o->auto_close_ms;
    s->on_state = o->on_state;
    s->snapshot_dirty = true;

    s->overlay = lv_obj_create(ui_chrome_root(ctx));
    lv_obj_remove_style_all(s->overlay);
    lv_obj_set_size(s->overlay, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_color(s->overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s->overlay, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(s->overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s->overlay, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s->overlay, sheet_overlay_click_cb, LV_EVENT_CLICKED, s);

    s->backdrop_img = ui_backdrop_create(s->overlay);

    s->panel = lv_obj_create(s->overlay);
    lv_obj_remove_style_all(s->panel);
    lv_obj_set_size(s->panel, o->width, o->height);
    lv_obj_set_pos(s->panel, o->x, s->closed_y);
    lv_obj_set_style_bg_color(s->panel, o->color, 0);
    lv_obj_set_style_bg_opa(s->panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s->panel, o->radius, 0);
    if (o->border_side != LV_BORDER_SIDE_NONE) {
        lv_obj_set_style_border_side(s->panel, o->border_side, 0);
        lv_obj_set_style_border_width(s->panel, 1, 0);
        lv_obj_set_style_border_color(s->panel, o->border, 0);
    }
    lv_obj_remove_flag(s->panel, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_flag(s->panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_user_data(s->panel, s);
    face_sheet_attach_drag(s, s->panel);

    s->snapshot_img = lv_image_create(s->overlay);
    lv_obj_remove_flag(s->snapshot_img, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s->snapshot_img, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
    /* The panel's rounded corners (Orbit) are clipped from the RGB565 snapshot when drawn. */
    lv_obj_set_style_radius(s->snapshot_img, o->radius, 0);

    s->auto_close_timer = ui_chrome_timer_create(ctx, sheet_auto_close_cb, o->auto_close_ms, s);
    if (s->auto_close_timer != NULL) {
        lv_timer_pause(s->auto_close_timer);
    }
}

void face_sheet_destroy(face_sheet_t *s)
{
    if (s_snapshot_owner == s) {
        if (s_snapshot_pool != NULL) {
            lv_draw_buf_destroy(s_snapshot_pool);
            s_snapshot_pool = NULL;
        }
        s_snapshot_owner = NULL;
    }
    s->snapshot_buf = NULL;
}
