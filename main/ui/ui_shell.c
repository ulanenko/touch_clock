#include "ui/clock_ui_private.h"

void update_dots(clock_ui_context_t *ctx, clock_face_id_t active_face)
{
    for (int i = 0; i < CLOCK_FACE_COUNT; ++i) {
        if (ctx->page_dots[i] == NULL) {
            continue;
        }

        lv_obj_set_style_bg_color(ctx->page_dots[i],
                                  (i == active_face) ? lv_color_white() : lv_color_hex(0x555555),
                                  0);
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

static void face_swipe_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    lv_event_code_t code = lv_event_get_code(event);
    lv_indev_t *indev = lv_indev_active();
    lv_point_t point;
    lv_coord_t dx;
    lv_coord_t dy;
    clock_face_id_t current_face;
    clock_face_id_t target_face;

    if (settings_surface_is_open(ctx) || alarm_surface_is_open(ctx) || brightness_panel_is_open(ctx)) {
        ctx->faces.face_swipe_tracking = false;
        return;
    }

    if (indev == NULL) {
        ctx->faces.face_swipe_tracking = false;
        return;
    }

    if (code == LV_EVENT_PRESSED) {
        lv_indev_get_point(indev, &ctx->faces.face_swipe_start_point);
        ctx->faces.face_swipe_tracking = true;
        return;
    }

    if (code == LV_EVENT_PRESS_LOST) {
        ctx->faces.face_swipe_tracking = false;
        return;
    }

    if (code != LV_EVENT_RELEASED || !ctx->faces.face_swipe_tracking) {
        return;
    }

    ctx->faces.face_swipe_tracking = false;
    lv_indev_get_point(indev, &point);
    dx = point.x - ctx->faces.face_swipe_start_point.x;
    dy = point.y - ctx->faces.face_swipe_start_point.y;

    if (LV_ABS(dx) < 56 || LV_ABS(dx) <= LV_ABS(dy) + 20) {
        return;
    }

    current_face = ctx->runtime->in_night_mode ? ctx->settings->night_mode.face : ctx->settings->current_face;
    target_face = (dx < 0) ? clock_face_step_enabled(current_face, 1) : clock_face_step_enabled(current_face, -1);
    if (target_face == current_face) {
        return;
    }

    set_active_face(ctx, target_face, LV_ANIM_OFF);
    show_affordances_temporarily(ctx);

    if (ctx->runtime->in_night_mode) {
        if (ctx->settings->night_mode.face != target_face) {
            request_set_night_face(ctx, target_face);
        }
    } else if (ctx->settings->current_face != target_face) {
        request_set_current_face(ctx, target_face);
    }
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
    lv_obj_add_event_cb(ctx->faces.face_swipe_layer, face_swipe_event_cb, LV_EVENT_PRESSED, ctx);
    lv_obj_add_event_cb(ctx->faces.face_swipe_layer, face_swipe_event_cb, LV_EVENT_RELEASED, ctx);
    lv_obj_add_event_cb(ctx->faces.face_swipe_layer, face_swipe_event_cb, LV_EVENT_PRESS_LOST, ctx);
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

    ctx->suppress_events = true;
    apply_face_navigation_mode(ctx, face);
    sync_face_animation_state(ctx, face);
    lv_tileview_set_tile_by_index(ctx->tileview, visible_index, 0, anim);
    update_dots(ctx, face);
    sync_alarm_banner_style(ctx, face);
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
    sync_alarm_banner_style(ctx, face);
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
        show_affordances_temporarily(ctx);
        return;
    }

    if (code == LV_EVENT_SCROLL_END) {
        ctx->faces.tileview_scrolling = false;
        face = tile_to_face(ctx, lv_tileview_get_tile_active(ctx->tileview));
        sync_face_animation_state(ctx, face);
        update_face(ctx, face);
        update_dots(ctx, face);
        show_affordances_temporarily(ctx);
    }
}

static void create_dots(clock_ui_context_t *ctx)
{
    int visible_count = clock_face_visible_count();
    int start_x = -((visible_count - 1) * 18) / 2;

    for (int index = 0; index < visible_count; ++index) {
        clock_face_id_t face = clock_face_visible_index_to_id(index);

        ctx->page_dots[face] = lv_obj_create(ctx->screen);
        lv_obj_set_size(ctx->page_dots[face], 10, 10);
        lv_obj_set_style_radius(ctx->page_dots[face], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(ctx->page_dots[face], 0, 0);
        lv_obj_set_style_opa(ctx->page_dots[face], LV_OPA_TRANSP, 0);
        lv_obj_set_scrollbar_mode(ctx->page_dots[face], LV_SCROLLBAR_MODE_OFF);
        lv_obj_remove_flag(ctx->page_dots[face], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(ctx->page_dots[face], LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(ctx->page_dots[face], LV_ALIGN_BOTTOM_MID, start_x + index * 18, -38);
    }

    update_dots(ctx, sanitize_enabled_face(ctx->settings->current_face));
}

static void create_settings_button(clock_ui_context_t *ctx)
{
    lv_obj_t *label;

    ctx->settings_button = lv_button_create(ctx->screen);
    lv_obj_set_size(ctx->settings_button, 56, 56);
    lv_obj_align(ctx->settings_button, LV_ALIGN_TOP_RIGHT, -28, 28);
    lv_obj_set_style_radius(ctx->settings_button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(ctx->settings_button, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_bg_opa(ctx->settings_button, LV_OPA_70, 0);
    lv_obj_set_style_border_width(ctx->settings_button, 0, 0);
    lv_obj_add_event_cb(ctx->settings_button, settings_button_event_cb, LV_EVENT_CLICKED, ctx);

    label = lv_label_create(ctx->settings_button);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, LV_SYMBOL_SETTINGS);
    lv_obj_center(label);
}

void build_root_ui(clock_ui_context_t *ctx)
{
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

    create_face_swipe_layer(ctx);
    create_dots(ctx);
    create_brightness_pull_hint(ctx);
    create_brightness_edge_sensor(ctx);
    create_brightness_overlay(ctx);
    create_brightness_panel(ctx);
    create_settings_button(ctx);
    create_alarm_banner(ctx);
    create_alarm_management_overlay(ctx);
    create_alarm_settings_overlay(ctx);
    create_alarm_editor_overlay(ctx);
    create_alarm_overlay(ctx);
    create_settings_overlay(ctx);
    lv_obj_move_foreground(ctx->brightness.pull_hint);
    lv_obj_move_foreground(ctx->brightness.edge_sensor);
    set_active_face(ctx, ctx->settings->current_face, LV_ANIM_OFF);
}
