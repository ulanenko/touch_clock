/*
 * The faces and the face chrome hooks.
 *
 * Faces: the tileview of faces, the face swipe (left/right changes face with a circular
 * reveal, down pulls the theme sheet, a tap shows the controls) and the snapshot backdrop the
 * pull sheets draw while they move. build_root_ui builds the faces and then the chrome.
 *
 * Face chrome: the ui_face_chrome_* hooks of ui_areas.h dispatch to the face chrome files
 * (see ui_face_chrome.h).
 */
#include "ui/clock_ui_private.h"
#include "ui/ui_areas.h"
#include "ui/ui_face_chrome.h"

#include <string.h>

#include "domain/face_catalog.h"

#define FACE_REVEAL_START_SIZE 48
#define FACE_REVEAL_END_SIZE 1040
#define FACE_REVEAL_MS 220
#define FACE_PULL_START 18 /* px down before a press on the face becomes a theme sheet pull */

static void face_reveal_anim_cb(void *var, int32_t value);
static void face_reveal_anim_ready_cb(lv_anim_t *anim);
static bool face_reveal_prepare(clock_ui_context_t *ctx, clock_face_id_t face);
static bool face_reveal_start(clock_ui_context_t *ctx, clock_face_id_t face);

static void face_reveal_anim_cb(void *var, int32_t value)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)var;
    int32_t visible;

    if (ctx == NULL || ctx->faces.face_reveal_shell == NULL) {
        return;
    }

    /* Keep the view a multiple of 4 px so the cropped buffer stays 4-byte aligned. */
    visible = LV_CLAMP(4, value, SCREEN_SIZE) & ~3;
    ctx->faces.face_reveal_size = value;
    lv_obj_set_size(ctx->faces.face_reveal_shell, visible, visible);
    lv_obj_center(ctx->faces.face_reveal_shell);
    lv_obj_invalidate(ctx->faces.face_reveal_shell);
}

/* Draws the centre of the target face snapshot clipped to the growing circle. Cropping the
 * snapshot to the circle's box lets clip_radius do the masking without the full-screen ARGB
 * layers that clip_corner on a parent object would allocate every frame. */
static void face_reveal_draw_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    lv_draw_buf_t *src = ctx->faces.face_reveal_buf;
    lv_draw_buf_t *view = &ctx->faces.face_reveal_view;
    lv_draw_image_dsc_t dsc;
    lv_area_t area;
    int32_t visible;
    int32_t x0;
    int32_t y0;

    if (src == NULL || src->data == NULL) {
        return;
    }

    lv_obj_get_coords(ctx->faces.face_reveal_shell, &area);
    visible = lv_area_get_width(&area);
    x0 = ((int32_t)src->header.w - visible) / 2;
    y0 = ((int32_t)src->header.h - visible) / 2;
    if (x0 < 0 || y0 < 0) {
        return;
    }

    *view = *src;
    view->header.w = visible;
    view->header.h = visible;
    view->data = src->data + (size_t)y0 * src->header.stride + (size_t)x0 * 2;
    view->data_size = (uint32_t)src->header.stride * visible;

    lv_draw_image_dsc_init(&dsc);
    dsc.src = view;
    dsc.image_area = area;
    dsc.clip_radius = ctx->faces.face_reveal_size < SCREEN_SIZE ? visible / 2 : 0;
    lv_draw_image(lv_event_get_layer(event), &dsc, &area);
}

static void face_reveal_anim_ready_cb(lv_anim_t *anim)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_anim_get_user_data(anim);

    if (ctx == NULL || ctx->faces.face_reveal_shell == NULL) {
        return;
    }

    ctx->faces.face_reveal_animating = false;
    lv_obj_add_flag(ctx->faces.face_reveal_shell, LV_OBJ_FLAG_HIDDEN);
    set_active_face(ctx, ctx->faces.face_reveal_target, LV_ANIM_OFF);
    /* The page dots show which face this is; the controls stay hidden while the circle
     * grows so the reveal frames draw nothing else. */
    face_controls_show(ctx);
}

static bool face_reveal_prepare(clock_ui_context_t *ctx, clock_face_id_t face)
{
    const void *src;

    if (ctx == NULL ||
        ctx->faces.face_reveal_render == NULL ||
        ctx->faces.face_reveal_shell == NULL) {
        return false;
    }

    src = ui_face_preview_source(ctx, face);
    if (src == NULL) {
        return false;
    }

    lv_obj_set_size(ctx->faces.face_reveal_render, SCREEN_SIZE, SCREEN_SIZE);
    lv_image_set_src(ctx->faces.face_reveal_render, src);
    lv_image_set_inner_align(ctx->faces.face_reveal_render, LV_IMAGE_ALIGN_COVER);
    lv_obj_update_layout(ctx->faces.face_reveal_render);

    if (ctx->faces.face_reveal_buf == NULL ||
        lv_snapshot_reshape_draw_buf(ctx->faces.face_reveal_render, ctx->faces.face_reveal_buf) != LV_RESULT_OK) {
        if (ctx->faces.face_reveal_buf != NULL) {
            lv_draw_buf_destroy(ctx->faces.face_reveal_buf);
        }
        ctx->faces.face_reveal_buf =
            lv_snapshot_create_draw_buf(ctx->faces.face_reveal_render, LV_COLOR_FORMAT_RGB565);
        if (ctx->faces.face_reveal_buf == NULL) {
            return false;
        }
    }

    if (lv_snapshot_take_to_draw_buf(ctx->faces.face_reveal_render,
                                     LV_COLOR_FORMAT_RGB565,
                                     ctx->faces.face_reveal_buf) != LV_RESULT_OK) {
        return false;
    }

    ctx->faces.face_reveal_target = face;
    return true;
}

static bool face_reveal_start(clock_ui_context_t *ctx, clock_face_id_t face)
{
    lv_anim_t anim;

    if (ctx == NULL || ctx->faces.face_reveal_animating) {
        return false;
    }

    face = sanitize_enabled_face(face);
    if (!face_reveal_prepare(ctx, face)) {
        return false;
    }

    ctx->faces.face_reveal_animating = true;
    lv_anim_delete(ctx, face_reveal_anim_cb);
    lv_obj_move_foreground(ctx->faces.face_reveal_shell);
    lv_obj_clear_flag(ctx->faces.face_reveal_shell, LV_OBJ_FLAG_HIDDEN);
    face_reveal_anim_cb(ctx, FACE_REVEAL_START_SIZE);

    lv_anim_init(&anim);
    lv_anim_set_var(&anim, ctx);
    lv_anim_set_user_data(&anim, ctx);
    lv_anim_set_exec_cb(&anim, face_reveal_anim_cb);
    lv_anim_set_values(&anim, FACE_REVEAL_START_SIZE, FACE_REVEAL_END_SIZE);
    lv_anim_set_time(&anim, FACE_REVEAL_MS);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_set_ready_cb(&anim, face_reveal_anim_ready_cb);
    lv_anim_start(&anim);
    return true;
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

static bool backdrop_capture(clock_ui_context_t *ctx, lv_obj_t *overlay)
{
    lv_result_t res;

    if (!ctx->backdrop_ready) {
        uint32_t stride = SCREEN_SIZE * 2U;
        uint32_t size = stride * SCREEN_SIZE;
        void *data = heap_caps_aligned_alloc(CONFIG_CACHE_L2_CACHE_LINE_SIZE, size, MALLOC_CAP_SPIRAM);

        if (data == NULL) {
            return false;
        }
        if (lv_draw_buf_init(&ctx->backdrop_buf, SCREEN_SIZE, SCREEN_SIZE, LV_COLOR_FORMAT_RGB565,
                             stride, data, size) != LV_RESULT_OK) {
            heap_caps_free(data);
            return false;
        }
        ctx->backdrop_ready = true;
    }

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
    if (colors == NULL) {
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
    clock_face_id_t current_face;
    clock_face_id_t target_face;

    current_face = ctx->runtime->in_night_mode ? ctx->settings->night_mode.face : ctx->settings->current_face;
    target_face = (dx < 0) ? clock_face_step_enabled(current_face, 1) : clock_face_step_enabled(current_face, -1);
    if (target_face == current_face) {
        return;
    }

    face_controls_hide(ctx);
    if (!face_reveal_start(ctx, target_face)) {
        set_active_face(ctx, target_face, LV_ANIM_OFF);
        face_controls_show(ctx);
    }

    if (ctx->runtime->in_night_mode) {
        if (ctx->settings->night_mode.face != target_face) {
            request_set_night_face(ctx, target_face);
        }
    } else if (ctx->settings->current_face != target_face) {
        request_set_current_face(ctx, target_face);
    }
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
            if (dy < FACE_PULL_START || dy < LV_ABS(dx) + 12 || ctx->faces.face_reveal_animating) {
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
    if (code != LV_EVENT_RELEASED) {
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

    if (LV_ABS(dx) < 56 || LV_ABS(dx) <= LV_ABS(dy) + 20) {
        return;
    }
    face_shell_swipe_face(ctx, dx);
}

void face_shell_attach_swipe(clock_ui_context_t *ctx, lv_obj_t *obj)
{
    lv_obj_add_event_cb(obj, face_swipe_event_cb, LV_EVENT_PRESSED, ctx);
    lv_obj_add_event_cb(obj, face_swipe_event_cb, LV_EVENT_PRESSING, ctx);
    lv_obj_add_event_cb(obj, face_swipe_event_cb, LV_EVENT_RELEASED, ctx);
    lv_obj_add_event_cb(obj, face_swipe_event_cb, LV_EVENT_PRESS_LOST, ctx);
}

static void create_face_reveal_layer(clock_ui_context_t *ctx)
{
    ctx->faces.face_reveal_render = lv_image_create(ctx->screen);
    lv_obj_set_pos(ctx->faces.face_reveal_render, -2000, -2000);
    lv_obj_clear_flag(ctx->faces.face_reveal_render, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(ctx->faces.face_reveal_render, LV_OBJ_FLAG_CLICKABLE);

    ctx->faces.face_reveal_shell = lv_obj_create(ctx->screen);
    lv_obj_remove_style_all(ctx->faces.face_reveal_shell);
    lv_obj_set_size(ctx->faces.face_reveal_shell, FACE_REVEAL_START_SIZE, FACE_REVEAL_START_SIZE);
    lv_obj_center(ctx->faces.face_reveal_shell);
    lv_obj_clear_flag(ctx->faces.face_reveal_shell, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ctx->faces.face_reveal_shell, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
    lv_obj_add_event_cb(ctx->faces.face_reveal_shell, face_reveal_draw_cb, LV_EVENT_DRAW_MAIN, ctx);
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

    lv_obj_add_event_cb(ctx->tileview, tileview_value_changed_cb, LV_EVENT_VALUE_CHANGED, ctx);
    lv_obj_add_event_cb(ctx->tileview, tileview_scroll_event_cb, LV_EVENT_SCROLL_BEGIN, ctx);
    lv_obj_add_event_cb(ctx->tileview, tileview_scroll_event_cb, LV_EVENT_SCROLL, ctx);
    lv_obj_add_event_cb(ctx->tileview, tileview_scroll_event_cb, LV_EVENT_SCROLL_END, ctx);

    create_face_reveal_layer(ctx);
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

/* --- Hooks (ui_areas.h) ----------------------------------------------------------------- */

void ui_face_chrome_create(clock_ui_context_t *ctx)
{
    face_night_create(ctx);
    face_quick_create_edge_sensor(ctx);
    face_controls_create(ctx);
    if (ctx->face_chrome.themes_tab != NULL) {
        face_shell_attach_swipe(ctx, ctx->face_chrome.themes_tab);
    }
    ui_debug_register("show_controls", ui_face_chrome_show_controls);
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
