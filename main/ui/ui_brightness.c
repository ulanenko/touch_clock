#include "ui/clock_ui_private.h"

static void brightness_overlay_hide(clock_ui_context_t *ctx);

static void affordance_opa_anim_cb(void *obj, int32_t value)
{
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)value, 0);
}

static void affordance_hide_anim_ready_cb(lv_anim_t *anim)
{
    lv_obj_add_flag((lv_obj_t *)anim->var, LV_OBJ_FLAG_HIDDEN);
}

static void animate_affordance(lv_obj_t *obj, bool visible)
{
    lv_anim_t anim;
    lv_opa_t start_opa;
    lv_opa_t end_opa = visible ? LV_OPA_COVER : LV_OPA_TRANSP;

    if (obj == NULL) {
        return;
    }

    if (!visible && lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

    start_opa = lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN) ? LV_OPA_TRANSP : lv_obj_get_style_opa(obj, 0);
    lv_anim_delete(obj, affordance_opa_anim_cb);

    if (visible) {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }

    if (start_opa == end_opa) {
        lv_obj_set_style_opa(obj, end_opa, 0);
        if (!visible) {
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    lv_anim_init(&anim);
    lv_anim_set_var(&anim, obj);
    lv_anim_set_exec_cb(&anim, affordance_opa_anim_cb);
    lv_anim_set_values(&anim, start_opa, end_opa);
    lv_anim_set_time(&anim, visible ? AFFORDANCE_FADE_IN_MS : AFFORDANCE_FADE_OUT_MS);
    lv_anim_set_path_cb(&anim, visible ? lv_anim_path_ease_out : lv_anim_path_ease_in);
    if (!visible) {
        lv_anim_set_ready_cb(&anim, affordance_hide_anim_ready_cb);
    }
    lv_anim_start(&anim);
}

static void set_affordances_visible(clock_ui_context_t *ctx, bool visible)
{
    ctx->affordances_visible = visible;

    for (int i = 0; i < CLOCK_FACE_COUNT; ++i) {
        if (ctx->page_dots[i] == NULL) {
            continue;
        }

        animate_affordance(ctx->page_dots[i], visible);
    }

    if (ctx->brightness.pull_hint == NULL) {
        return;
    }

    animate_affordance(ctx->brightness.pull_hint, visible);
    animate_affordance(ctx->face_theme.pull_hint, visible);
}

void affordance_hide_timer_cb(lv_timer_t *timer)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_timer_get_user_data(timer);

    set_affordances_visible(ctx, false);
    if (ctx->affordance_hide_timer != NULL) {
        lv_timer_pause(ctx->affordance_hide_timer);
    }
}

void show_affordances_temporarily(clock_ui_context_t *ctx)
{
    if (settings_surface_is_open(ctx) || alarm_surface_is_open(ctx) || brightness_panel_is_open(ctx)) {
        return;
    }

    set_affordances_visible(ctx, true);
    if (ctx->affordance_hide_timer == NULL) {
        return;
    }

    lv_timer_set_period(ctx->affordance_hide_timer, AFFORDANCE_VISIBLE_MS);
    lv_timer_resume(ctx->affordance_hide_timer);
        lv_timer_reset(ctx->affordance_hide_timer);
}

static void sync_active_face_visual_state(clock_ui_context_t *ctx)
{
    if (ctx->tileview == NULL) {
        return;
    }

    sync_face_animation_state(ctx, tile_to_face(ctx, lv_tileview_get_tile_active(ctx->tileview)));
}

#define QUICK_ACTIONS_AUTO_CLOSE_MS 3000
#define BRIGHTNESS_PANEL_AUTO_CLOSE_MS 5000

static void quick_actions_auto_close_pause(clock_ui_context_t *ctx)
{
    if (ctx->brightness.overlay_auto_close_timer == NULL) {
        return;
    }

    lv_timer_pause(ctx->brightness.overlay_auto_close_timer);
}

static void quick_actions_auto_close_reset(clock_ui_context_t *ctx)
{
    if (ctx->brightness.overlay_auto_close_timer == NULL ||
        ctx->brightness.overlay == NULL ||
        lv_obj_has_flag(ctx->brightness.overlay, LV_OBJ_FLAG_HIDDEN) ||
        brightness_panel_is_open(ctx)) {
        return;
    }

    lv_timer_set_period(ctx->brightness.overlay_auto_close_timer, QUICK_ACTIONS_AUTO_CLOSE_MS);
    lv_timer_resume(ctx->brightness.overlay_auto_close_timer);
    lv_timer_reset(ctx->brightness.overlay_auto_close_timer);
}

static void quick_actions_auto_close_timer_cb(lv_timer_t *timer)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_timer_get_user_data(timer);

    if (ctx == NULL ||
        ctx->brightness.overlay == NULL ||
        lv_obj_has_flag(ctx->brightness.overlay, LV_OBJ_FLAG_HIDDEN) ||
        brightness_panel_is_open(ctx)) {
        lv_timer_pause(timer);
        return;
    }

    if (ctx->brightness.dragging || ctx->brightness.animating) {
        quick_actions_auto_close_reset(ctx);
        return;
    }

    brightness_overlay_hide(ctx);
}

static void brightness_panel_auto_close_pause(clock_ui_context_t *ctx)
{
    if (ctx->brightness.auto_close_timer == NULL) {
        return;
    }

    lv_timer_pause(ctx->brightness.auto_close_timer);
}

static void brightness_panel_auto_close_reset(clock_ui_context_t *ctx)
{
    if (ctx->brightness.auto_close_timer == NULL) {
        return;
    }

    lv_timer_set_period(ctx->brightness.auto_close_timer, BRIGHTNESS_PANEL_AUTO_CLOSE_MS);
    lv_timer_resume(ctx->brightness.auto_close_timer);
    lv_timer_reset(ctx->brightness.auto_close_timer);
}

static void brightness_panel_auto_close_timer_cb(lv_timer_t *timer)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_timer_get_user_data(timer);

    if (ctx == NULL || !brightness_panel_is_open(ctx)) {
        lv_timer_pause(timer);
        return;
    }

    if (ctx->brightness.dragging || ctx->brightness.animating) {
        brightness_panel_auto_close_reset(ctx);
        return;
    }

    brightness_panel_hide(ctx);
}

static uint8_t brightness_ui_get_target_hw(const clock_ui_context_t *ctx)
{
    if (ctx->runtime != NULL && ctx->runtime->in_night_mode) {
        if (ctx->runtime->night_brightness_override_active) {
            return ctx->runtime->night_brightness_override;
        }

        return ctx->settings->night_mode.brightness;
    }

    return ctx->settings->base_brightness;
}

void update_brightness_ui(clock_ui_context_t *ctx)
{
    char buffer[32];
    uint8_t target_brightness;

    if (ctx->brightness.value == NULL || ctx->brightness.slider == NULL) {
        return;
    }

    target_brightness = brightness_ui_get_target_hw(ctx);
    if (!ctx->brightness.ui_synced || ctx->brightness.last_ui_percent != target_brightness) {
        snprintf(buffer, sizeof(buffer), "%u%%", target_brightness);
        lv_label_set_text(ctx->brightness.value, buffer);

        ctx->suppress_events = true;
        lv_slider_set_value(ctx->brightness.slider, target_brightness, LV_ANIM_OFF);
        ctx->suppress_events = false;

        ctx->brightness.last_ui_percent = target_brightness;
        ctx->brightness.ui_synced = true;
    }
}

bool brightness_panel_is_open(const clock_ui_context_t *ctx)
{
    return ctx->brightness.panel_overlay != NULL &&
           !lv_obj_has_flag(ctx->brightness.panel_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void brightness_update_visual_state(clock_ui_context_t *ctx, int32_t sheet_y)
{
    int32_t clamped_y = LV_CLAMP(BRIGHTNESS_SHEET_OPEN_Y, sheet_y, BRIGHTNESS_SHEET_CLOSED_Y);
    int32_t travel = BRIGHTNESS_SHEET_CLOSED_Y - BRIGHTNESS_SHEET_OPEN_Y;
    int32_t progress = BRIGHTNESS_SHEET_CLOSED_Y - clamped_y;
    lv_opa_t opa = (lv_opa_t)((progress * BRIGHTNESS_SCRIM_OPA) / travel);

    lv_obj_set_y(ctx->brightness.sheet, clamped_y);
    lv_obj_set_style_bg_opa(ctx->brightness.overlay, opa, 0);
}

static void brightness_sheet_anim_cb(void *obj, int32_t value)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_obj_get_user_data((lv_obj_t *)obj);
    brightness_update_visual_state(ctx, value);
}

static void brightness_sheet_anim_ready_cb(lv_anim_t *anim)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_anim_get_user_data(anim);

    ctx->brightness.animating = false;

    if (!ctx->brightness.target_open) {
        lv_obj_add_flag(ctx->brightness.overlay, LV_OBJ_FLAG_HIDDEN);
        brightness_update_visual_state(ctx, BRIGHTNESS_SHEET_CLOSED_Y);
        quick_actions_auto_close_pause(ctx);
    } else {
        brightness_update_visual_state(ctx, BRIGHTNESS_SHEET_OPEN_Y);
        quick_actions_auto_close_reset(ctx);
    }

    sync_active_face_visual_state(ctx);
}

static uint32_t brightness_sheet_anim_duration(int32_t from_y, int32_t to_y)
{
    int32_t distance = LV_ABS(to_y - from_y);
    int32_t travel = BRIGHTNESS_SHEET_CLOSED_Y - BRIGHTNESS_SHEET_OPEN_Y;

    if (travel <= 0) {
        return BRIGHTNESS_SHEET_SHOW_MS;
    }

    return (uint32_t)LV_CLAMP(110, 110 + ((distance * 150) / travel), 260);
}

static void brightness_prepare_overlay_for_drag(clock_ui_context_t *ctx)
{
    if (ctx->brightness.overlay == NULL) {
        return;
    }

    lv_obj_clear_flag(ctx->brightness.overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ctx->brightness.overlay);
    sync_active_face_visual_state(ctx);
}

static void brightness_animate_sheet_to(clock_ui_context_t *ctx, int32_t target_y)
{
    lv_anim_t anim;
    int32_t current_y;

    if (ctx->brightness.overlay == NULL || ctx->brightness.sheet == NULL) {
        return;
    }

    target_y = LV_CLAMP(BRIGHTNESS_SHEET_OPEN_Y, target_y, BRIGHTNESS_SHEET_CLOSED_Y);
    current_y = lv_obj_get_y(ctx->brightness.sheet);

    lv_anim_delete(ctx->brightness.sheet, brightness_sheet_anim_cb);
    ctx->brightness.animating = false;

    if (current_y == target_y) {
        brightness_update_visual_state(ctx, target_y);
        ctx->brightness.target_open = (target_y <= BRIGHTNESS_SHEET_OPEN_Y);
        if (!ctx->brightness.target_open) {
            lv_obj_add_flag(ctx->brightness.overlay, LV_OBJ_FLAG_HIDDEN);
        } else {
            brightness_prepare_overlay_for_drag(ctx);
        }
        return;
    }

    brightness_prepare_overlay_for_drag(ctx);
    ctx->brightness.target_open = (target_y <= BRIGHTNESS_SHEET_OPEN_Y);

    ctx->brightness.animating = true;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, ctx->brightness.sheet);
    lv_anim_set_user_data(&anim, ctx);
    lv_anim_set_exec_cb(&anim, brightness_sheet_anim_cb);
    lv_anim_set_values(&anim, current_y, target_y);
    lv_anim_set_time(&anim, brightness_sheet_anim_duration(current_y, target_y));
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_set_ready_cb(&anim, brightness_sheet_anim_ready_cb);
    lv_anim_start(&anim);
}

static void brightness_overlay_set_visible(clock_ui_context_t *ctx, bool show)
{
    if (ctx->brightness.overlay == NULL) {
        return;
    }

    lv_anim_delete(ctx->brightness.sheet, brightness_sheet_anim_cb);
    ctx->brightness.animating = false;
    ctx->brightness.dragging = false;
    ctx->brightness.drag_from_edge = false;
    ctx->brightness.edge_swipe_triggered = false;

    if (show) {
        update_brightness_ui(ctx);
        brightness_prepare_overlay_for_drag(ctx);
        brightness_update_visual_state(ctx, BRIGHTNESS_SHEET_OPEN_Y);
        quick_actions_auto_close_reset(ctx);
    } else {
        quick_actions_auto_close_pause(ctx);
        brightness_update_visual_state(ctx, BRIGHTNESS_SHEET_CLOSED_Y);
        lv_obj_add_flag(ctx->brightness.overlay, LV_OBJ_FLAG_HIDDEN);
        sync_active_face_visual_state(ctx);
    }
}

void brightness_overlay_hide_immediately(clock_ui_context_t *ctx)
{
    brightness_overlay_set_visible(ctx, false);
}

static void brightness_overlay_hide(clock_ui_context_t *ctx)
{
    if (ctx->brightness.dragging || ctx->brightness.animating || lv_obj_has_flag(ctx->brightness.overlay, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

    brightness_animate_sheet_to(ctx, BRIGHTNESS_SHEET_CLOSED_Y);
}

void brightness_panel_hide(clock_ui_context_t *ctx)
{
    if (ctx->brightness.panel_overlay == NULL) {
        return;
    }

    brightness_panel_auto_close_pause(ctx);
    lv_obj_add_flag(ctx->brightness.panel_overlay, LV_OBJ_FLAG_HIDDEN);
    sync_active_face_visual_state(ctx);
}

static void brightness_slider_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    uint8_t target_brightness;

    if (ctx->suppress_events) {
        return;
    }

    brightness_panel_auto_close_reset(ctx);
    target_brightness = (uint8_t)lv_slider_get_value(lv_event_get_target(event));
    if (ctx->runtime != NULL && ctx->runtime->in_night_mode) {
        request_set_runtime_night_brightness(ctx, target_brightness);
    } else {
        request_set_base_brightness(ctx, target_brightness);
    }

    update_brightness_ui(ctx);
}

static void brightness_panel_show(clock_ui_context_t *ctx)
{
    if (ctx->brightness.panel_overlay == NULL || settings_surface_is_open(ctx) || alarm_surface_is_open(ctx)) {
        return;
    }

    update_brightness_ui(ctx);
    quick_actions_auto_close_pause(ctx);
    lv_obj_clear_flag(ctx->brightness.panel_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ctx->brightness.panel_overlay);
    brightness_panel_auto_close_reset(ctx);
    sync_active_face_visual_state(ctx);
}

static void brightness_panel_activity_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);

    if (ctx == NULL || ctx->suppress_events || !brightness_panel_is_open(ctx)) {
        return;
    }

    brightness_panel_auto_close_reset(ctx);
}

static void quick_actions_activity_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);

    if (ctx == NULL || ctx->suppress_events) {
        return;
    }

    quick_actions_auto_close_reset(ctx);
}

static void brightness_action_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);

    brightness_overlay_hide_immediately(ctx);
    brightness_panel_show(ctx);
}

static void alarms_action_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);

    alarm_management_open(ctx);
}

static void settings_action_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);

    open_settings_tab(ctx, SETTINGS_TAB_WIFI);
}

static void brightness_overlay_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);

    if (lv_event_get_target(event) == lv_event_get_current_target(event)) {
        brightness_overlay_hide(ctx);
    }
}

static void brightness_panel_overlay_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);

    if (lv_event_get_target(event) == lv_event_get_current_target(event)) {
        brightness_panel_hide(ctx);
    }
}

static void brightness_stop_event_bubble_cb(lv_event_t *event)
{
    lv_event_stop_bubbling(event);
}

static void brightness_drag_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    lv_event_code_t code = lv_event_get_code(event);
    lv_indev_t *indev = lv_event_get_indev(event);
    lv_point_t point;
    lv_obj_t *target = lv_event_get_target(event);
    bool from_edge = (target == ctx->brightness.edge_sensor);
    bool from_handle = (target == ctx->brightness.drag_handle);
    bool from_sheet = (target == ctx->brightness.sheet);
    int32_t dx;
    int32_t dy;
    int32_t target_y;
    int32_t travel = BRIGHTNESS_SHEET_CLOSED_Y - BRIGHTNESS_SHEET_OPEN_Y;
    int32_t open_threshold_y = BRIGHTNESS_SHEET_CLOSED_Y - ((travel * 45) / 100);

    if (indev == NULL || settings_surface_is_open(ctx) || alarm_surface_is_open(ctx) || brightness_panel_is_open(ctx)) {
        return;
    }

    lv_indev_get_point(indev, &point);

    if (!from_edge && !from_handle && !from_sheet) {
        return;
    }

    if (code == LV_EVENT_PRESSED) {
        if (ctx->brightness.animating) {
            lv_anim_delete(ctx->brightness.sheet, brightness_sheet_anim_cb);
            ctx->brightness.animating = false;
        }
        if (from_edge && !lv_obj_has_flag(ctx->brightness.overlay, LV_OBJ_FLAG_HIDDEN)) {
            return;
        }
        if ((from_handle || from_sheet) && lv_obj_has_flag(ctx->brightness.overlay, LV_OBJ_FLAG_HIDDEN)) {
            return;
        }

        if (from_edge) {
            ctx->brightness.drag_start_y = BRIGHTNESS_SHEET_CLOSED_Y;
        } else {
            ctx->brightness.drag_start_y = lv_obj_get_y(ctx->brightness.sheet);
        }

        ctx->brightness.dragging = true;
        ctx->brightness.drag_from_edge = from_edge;
        ctx->brightness.edge_swipe_triggered = false;
        ctx->brightness.drag_start_point = point;
        quick_actions_auto_close_reset(ctx);
        return;
    }

    if (!ctx->brightness.dragging) {
        return;
    }

    if (code == LV_EVENT_PRESS_LOST) {
        ctx->brightness.dragging = false;
        ctx->brightness.drag_from_edge = false;
        ctx->brightness.edge_swipe_triggered = false;
        return;
    }

    dx = point.x - ctx->brightness.drag_start_point.x;
    dy = point.y - ctx->brightness.drag_start_point.y;

    if (code == LV_EVENT_PRESSING) {
        if (ctx->brightness.drag_from_edge) {
            if (dy >= 0 || LV_ABS(dy) < LV_ABS(dx) + 12) {
                return;
            }

            update_brightness_ui(ctx);
            brightness_prepare_overlay_for_drag(ctx);
            target_y = ctx->brightness.drag_start_y + dy;
            brightness_update_visual_state(ctx, target_y);
            quick_actions_auto_close_reset(ctx);
            return;
        }

        target_y = ctx->brightness.drag_start_y + dy;
        brightness_update_visual_state(ctx, target_y);
        quick_actions_auto_close_reset(ctx);
        return;
    }

    if (code == LV_EVENT_RELEASED) {
        if (ctx->brightness.drag_from_edge) {
            ctx->brightness.dragging = false;
            ctx->brightness.drag_from_edge = false;
            ctx->brightness.edge_swipe_triggered = false;

            if (dy >= 0 || LV_ABS(dy) < LV_ABS(dx) + 12) {
                brightness_animate_sheet_to(ctx, BRIGHTNESS_SHEET_CLOSED_Y);
                return;
            }

            target_y = lv_obj_get_y(ctx->brightness.sheet);
            if (target_y <= open_threshold_y) {
                brightness_animate_sheet_to(ctx, BRIGHTNESS_SHEET_OPEN_Y);
            } else {
                brightness_animate_sheet_to(ctx, BRIGHTNESS_SHEET_CLOSED_Y);
            }
            return;
        }

        ctx->brightness.dragging = false;
        ctx->brightness.drag_from_edge = false;
        ctx->brightness.edge_swipe_triggered = false;

        target_y = lv_obj_get_y(ctx->brightness.sheet);
        if (target_y <= open_threshold_y) {
            brightness_animate_sheet_to(ctx, BRIGHTNESS_SHEET_OPEN_Y);
        } else {
            brightness_animate_sheet_to(ctx, BRIGHTNESS_SHEET_CLOSED_Y);
        }
    }
}

void create_brightness_pull_hint(clock_ui_context_t *ctx)
{
    ctx->brightness.pull_hint = lv_obj_create(ctx->screen);
    lv_obj_set_size(ctx->brightness.pull_hint, 88, 6);
    lv_obj_set_style_radius(ctx->brightness.pull_hint, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ctx->brightness.pull_hint, 0, 0);
    lv_obj_set_style_bg_color(ctx->brightness.pull_hint, lv_color_hex(0x808080), 0);
    lv_obj_set_style_bg_opa(ctx->brightness.pull_hint, LV_OPA_50, 0);
    lv_obj_set_style_opa(ctx->brightness.pull_hint, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(ctx->brightness.pull_hint, 0, 0);
    lv_obj_remove_flag(ctx->brightness.pull_hint, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ctx->brightness.pull_hint, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(ctx->brightness.pull_hint, LV_ALIGN_BOTTOM_MID, 0, -16);
}

void create_brightness_edge_sensor(clock_ui_context_t *ctx)
{
    ctx->brightness.edge_sensor = lv_obj_create(ctx->screen);
    lv_obj_set_size(ctx->brightness.edge_sensor, SCREEN_SIZE, BOTTOM_EDGE_ZONE);
    lv_obj_align(ctx->brightness.edge_sensor, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(ctx->brightness.edge_sensor, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ctx->brightness.edge_sensor, 0, 0);
    lv_obj_set_style_radius(ctx->brightness.edge_sensor, 0, 0);
    lv_obj_set_style_pad_all(ctx->brightness.edge_sensor, 0, 0);
    lv_obj_set_scrollbar_mode(ctx->brightness.edge_sensor, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(ctx->brightness.edge_sensor, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ctx->brightness.edge_sensor, brightness_drag_event_cb, LV_EVENT_PRESSED, ctx);
    lv_obj_add_event_cb(ctx->brightness.edge_sensor, brightness_drag_event_cb, LV_EVENT_PRESSING, ctx);
    lv_obj_add_event_cb(ctx->brightness.edge_sensor, brightness_drag_event_cb, LV_EVENT_RELEASED, ctx);
    lv_obj_add_event_cb(ctx->brightness.edge_sensor, brightness_drag_event_cb, LV_EVENT_PRESS_LOST, ctx);
}

static lv_obj_t *create_quick_action_button(lv_obj_t *parent,
                                            const char *symbol,
                                            lv_event_cb_t cb,
                                            clock_ui_context_t *ctx)
{
    lv_obj_t *button = create_icon_circle_button(parent, symbol, 128, NULL, NULL);

    lv_obj_add_event_cb(button, brightness_stop_event_bubble_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(button, brightness_stop_event_bubble_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(button, brightness_stop_event_bubble_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(button, brightness_stop_event_bubble_cb, LV_EVENT_PRESS_LOST, NULL);
    lv_obj_add_event_cb(button, brightness_stop_event_bubble_cb, LV_EVENT_CLICKED, NULL);

    if (cb != NULL) {
        lv_obj_add_event_cb(button, cb, LV_EVENT_PRESSED, ctx);
    }

    lv_obj_add_event_cb(button, quick_actions_activity_event_cb, LV_EVENT_PRESSED, ctx);
    lv_obj_add_event_cb(button, quick_actions_activity_event_cb, LV_EVENT_PRESSING, ctx);
    lv_obj_add_event_cb(button, quick_actions_activity_event_cb, LV_EVENT_RELEASED, ctx);

    return button;
}

void create_brightness_overlay(clock_ui_context_t *ctx)
{
    lv_obj_t *sheet_grabber;
    lv_obj_t *actions;

    ctx->brightness.overlay = lv_obj_create(ctx->screen);
    lv_obj_set_size(ctx->brightness.overlay, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_color(ctx->brightness.overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(ctx->brightness.overlay, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ctx->brightness.overlay, 0, 0);
    lv_obj_set_style_radius(ctx->brightness.overlay, 0, 0);
    lv_obj_set_style_pad_all(ctx->brightness.overlay, 0, 0);
    lv_obj_remove_flag(ctx->brightness.overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(ctx->brightness.overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(ctx->brightness.overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_user_data(ctx->brightness.overlay, ctx);
    lv_obj_add_event_cb(ctx->brightness.overlay, brightness_overlay_event_cb, LV_EVENT_CLICKED, ctx);

    ctx->brightness.sheet = lv_obj_create(ctx->brightness.overlay);
    lv_obj_set_size(ctx->brightness.sheet, BRIGHTNESS_SHEET_WIDTH, BRIGHTNESS_SHEET_HEIGHT);
    lv_obj_set_pos(ctx->brightness.sheet, BRIGHTNESS_SHEET_X, BRIGHTNESS_SHEET_CLOSED_Y);
    lv_obj_set_style_bg_color(ctx->brightness.sheet, lv_color_hex(0x161616), 0);
    lv_obj_set_style_bg_opa(ctx->brightness.sheet, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ctx->brightness.sheet, 0, 0);
    lv_obj_set_style_radius(ctx->brightness.sheet, 0, 0);
    lv_obj_set_style_pad_top(ctx->brightness.sheet, 28, 0);
    lv_obj_set_style_pad_bottom(ctx->brightness.sheet, 32, 0);
    lv_obj_set_style_pad_left(ctx->brightness.sheet, 24, 0);
    lv_obj_set_style_pad_right(ctx->brightness.sheet, 24, 0);
    lv_obj_set_style_shadow_width(ctx->brightness.sheet, 0, 0);
    lv_obj_remove_flag(ctx->brightness.sheet, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(ctx->brightness.sheet, ctx);
    lv_obj_add_event_cb(ctx->brightness.sheet, brightness_drag_event_cb, LV_EVENT_PRESSED, ctx);
    lv_obj_add_event_cb(ctx->brightness.sheet, brightness_drag_event_cb, LV_EVENT_PRESSING, ctx);
    lv_obj_add_event_cb(ctx->brightness.sheet, brightness_drag_event_cb, LV_EVENT_RELEASED, ctx);
    lv_obj_add_event_cb(ctx->brightness.sheet, brightness_drag_event_cb, LV_EVENT_PRESS_LOST, ctx);
    lv_obj_add_event_cb(ctx->brightness.sheet, quick_actions_activity_event_cb, LV_EVENT_PRESSED, ctx);
    lv_obj_add_event_cb(ctx->brightness.sheet, quick_actions_activity_event_cb, LV_EVENT_PRESSING, ctx);
    lv_obj_add_event_cb(ctx->brightness.sheet, quick_actions_activity_event_cb, LV_EVENT_RELEASED, ctx);
    sheet_grabber = lv_obj_create(ctx->brightness.sheet);
    lv_obj_set_size(sheet_grabber, 72, 6);
    lv_obj_set_style_radius(sheet_grabber, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(sheet_grabber, 0, 0);
    lv_obj_set_style_bg_color(sheet_grabber, lv_color_hex(0x9C9C9C), 0);
    lv_obj_set_style_bg_opa(sheet_grabber, LV_OPA_70, 0);
    lv_obj_set_style_shadow_width(sheet_grabber, 0, 0);
    lv_obj_remove_flag(sheet_grabber, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(sheet_grabber, LV_ALIGN_TOP_MID, 0, -10);

    ctx->brightness.drag_handle = lv_obj_create(ctx->brightness.sheet);
    lv_obj_set_size(ctx->brightness.drag_handle, lv_pct(100), 60);
    lv_obj_align(ctx->brightness.drag_handle, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(ctx->brightness.drag_handle, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ctx->brightness.drag_handle, 0, 0);
    lv_obj_set_style_radius(ctx->brightness.drag_handle, 0, 0);
    lv_obj_set_style_pad_all(ctx->brightness.drag_handle, 0, 0);
    lv_obj_set_scrollbar_mode(ctx->brightness.drag_handle, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(ctx->brightness.drag_handle, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ctx->brightness.drag_handle, LV_OBJ_FLAG_FLOATING);
    lv_obj_add_event_cb(ctx->brightness.drag_handle, brightness_drag_event_cb, LV_EVENT_PRESSED, ctx);
    lv_obj_add_event_cb(ctx->brightness.drag_handle, brightness_drag_event_cb, LV_EVENT_PRESSING, ctx);
    lv_obj_add_event_cb(ctx->brightness.drag_handle, brightness_drag_event_cb, LV_EVENT_RELEASED, ctx);
    lv_obj_add_event_cb(ctx->brightness.drag_handle, brightness_drag_event_cb, LV_EVENT_PRESS_LOST, ctx);
    lv_obj_add_event_cb(ctx->brightness.drag_handle, quick_actions_activity_event_cb, LV_EVENT_PRESSED, ctx);
    lv_obj_add_event_cb(ctx->brightness.drag_handle, quick_actions_activity_event_cb, LV_EVENT_PRESSING, ctx);
    lv_obj_add_event_cb(ctx->brightness.drag_handle, quick_actions_activity_event_cb, LV_EVENT_RELEASED, ctx);

    actions = lv_obj_create(ctx->brightness.sheet);
    lv_obj_set_size(actions, lv_pct(100), 144);
    lv_obj_align(actions, LV_ALIGN_TOP_MID, 0, 24);
    lv_obj_set_style_bg_opa(actions, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(actions, 0, 0);
    lv_obj_set_style_radius(actions, 0, 0);
    lv_obj_set_style_pad_all(actions, 0, 0);
    lv_obj_set_style_pad_column(actions, 28, 0);
    lv_obj_remove_flag(actions, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(actions, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(actions, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(actions, brightness_stop_event_bubble_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(actions, brightness_stop_event_bubble_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(actions, brightness_stop_event_bubble_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(actions, brightness_stop_event_bubble_cb, LV_EVENT_PRESS_LOST, NULL);
    lv_obj_add_event_cb(actions, quick_actions_activity_event_cb, LV_EVENT_PRESSED, ctx);
    lv_obj_add_event_cb(actions, quick_actions_activity_event_cb, LV_EVENT_PRESSING, ctx);
    lv_obj_add_event_cb(actions, quick_actions_activity_event_cb, LV_EVENT_RELEASED, ctx);

    create_quick_action_button(actions, QUICK_ACTION_SYMBOL_BRIGHTNESS, brightness_action_event_cb, ctx);
    create_quick_action_button(actions, LV_SYMBOL_BELL, alarms_action_event_cb, ctx);
    create_quick_action_button(actions, LV_SYMBOL_SETTINGS, settings_action_event_cb, ctx);
}

void create_brightness_panel(clock_ui_context_t *ctx)
{
    lv_obj_t *panel;
    lv_obj_t *title;

    ctx->brightness.panel_overlay = lv_obj_create(ctx->screen);
    lv_obj_set_size(ctx->brightness.panel_overlay, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_color(ctx->brightness.panel_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(ctx->brightness.panel_overlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(ctx->brightness.panel_overlay, 0, 0);
    lv_obj_set_style_radius(ctx->brightness.panel_overlay, 0, 0);
    lv_obj_set_style_pad_all(ctx->brightness.panel_overlay, 0, 0);
    lv_obj_add_flag(ctx->brightness.panel_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(ctx->brightness.panel_overlay, brightness_panel_overlay_event_cb, LV_EVENT_CLICKED, ctx);

    panel = lv_obj_create(ctx->brightness.panel_overlay);
    ctx->brightness.panel = panel;
    lv_obj_set_width(panel, 560);
    lv_obj_set_height(panel, LV_SIZE_CONTENT);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x151515), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 28, 0);
    lv_obj_set_style_pad_top(panel, 28, 0);
    lv_obj_set_style_pad_bottom(panel, 26, 0);
    lv_obj_set_style_pad_left(panel, 28, 0);
    lv_obj_set_style_pad_right(panel, 28, 0);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel, 18, 0);
    lv_obj_add_event_cb(panel, brightness_panel_activity_event_cb, LV_EVENT_PRESSED, ctx);
    lv_obj_add_event_cb(panel, brightness_panel_activity_event_cb, LV_EVENT_PRESSING, ctx);
    lv_obj_add_event_cb(panel, brightness_panel_activity_event_cb, LV_EVENT_RELEASED, ctx);

    title = lv_label_create(panel);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_label_set_text(title, "Brightness");

    ctx->brightness.slider = lv_slider_create(panel);
    lv_obj_set_width(ctx->brightness.slider, lv_pct(100));
    lv_slider_set_range(ctx->brightness.slider, 0, 100);
    style_slider(ctx->brightness.slider);
    lv_obj_add_event_cb(ctx->brightness.slider, brightness_panel_activity_event_cb, LV_EVENT_PRESSED, ctx);
    lv_obj_add_event_cb(ctx->brightness.slider, brightness_panel_activity_event_cb, LV_EVENT_PRESSING, ctx);
    lv_obj_add_event_cb(ctx->brightness.slider, brightness_panel_activity_event_cb, LV_EVENT_RELEASED, ctx);
    lv_obj_add_event_cb(ctx->brightness.slider, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, ctx);

    ctx->brightness.value = lv_label_create(panel);
    lv_obj_set_style_text_font(ctx->brightness.value, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(ctx->brightness.value, lv_color_white(), 0);
    lv_label_set_text(ctx->brightness.value, "50%");

    ctx->brightness.overlay_auto_close_timer = lv_timer_create(quick_actions_auto_close_timer_cb,
                                                               QUICK_ACTIONS_AUTO_CLOSE_MS,
                                                               ctx);
    lv_timer_pause(ctx->brightness.overlay_auto_close_timer);
    ctx->brightness.auto_close_timer = lv_timer_create(brightness_panel_auto_close_timer_cb,
                                                       BRIGHTNESS_PANEL_AUTO_CLOSE_MS,
                                                       ctx);
    lv_timer_pause(ctx->brightness.auto_close_timer);
    update_brightness_ui(ctx);
}
