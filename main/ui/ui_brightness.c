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

static void set_affordances_visible(bool visible)
{
    s_ui.affordances_visible = visible;

    for (int i = 0; i < CLOCK_FACE_COUNT; ++i) {
        if (s_ui.page_dots[i] == NULL) {
            continue;
        }

        animate_affordance(s_ui.page_dots[i], visible);
    }

    if (s_ui.brightness.pull_hint == NULL) {
        return;
    }

    animate_affordance(s_ui.brightness.pull_hint, visible);
}

static void affordance_hide_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    set_affordances_visible(false);
    if (s_ui.affordance_hide_timer != NULL) {
        lv_timer_pause(s_ui.affordance_hide_timer);
    }
}

static void show_affordances_temporarily(void)
{
    if (s_ui.settings_ui.open || alarm_surface_is_open() || brightness_panel_is_open()) {
        return;
    }

    set_affordances_visible(true);
    if (s_ui.affordance_hide_timer == NULL) {
        return;
    }

    lv_timer_set_period(s_ui.affordance_hide_timer, AFFORDANCE_VISIBLE_MS);
    lv_timer_resume(s_ui.affordance_hide_timer);
    lv_timer_reset(s_ui.affordance_hide_timer);
}

static int clamp_brightness(int brightness)
{
    if (brightness < DISPLAY_BRIGHTNESS_MIN_PERCENT) {
        return DISPLAY_BRIGHTNESS_MIN_PERCENT;
    }
    if (brightness > DISPLAY_BRIGHTNESS_MAX_PERCENT) {
        return DISPLAY_BRIGHTNESS_MAX_PERCENT;
    }
    return brightness;
}

static int clamp_brightness_ui(int brightness)
{
    if (brightness < 0) {
        return 0;
    }
    if (brightness > 100) {
        return 100;
    }
    return brightness;
}

static uint8_t brightness_ui_to_hw(int ui_percent)
{
    int clamped = clamp_brightness_ui(ui_percent);
    int span = DISPLAY_BRIGHTNESS_MAX_PERCENT - DISPLAY_BRIGHTNESS_MIN_PERCENT;
    int hw = DISPLAY_BRIGHTNESS_MIN_PERCENT + ((clamped * span + 50) / 100);

    return (uint8_t)clamp_brightness(hw);
}

static uint8_t brightness_hw_to_ui(int hw_percent)
{
    int clamped = clamp_brightness(hw_percent);
    int span = DISPLAY_BRIGHTNESS_MAX_PERCENT - DISPLAY_BRIGHTNESS_MIN_PERCENT;

    if (span <= 0) {
        return 100;
    }

    return (uint8_t)clamp_brightness_ui(((clamped - DISPLAY_BRIGHTNESS_MIN_PERCENT) * 100 + (span / 2)) / span);
}

static void update_brightness_ui(void)
{
    char buffer[32];
    uint8_t ui_brightness;

    if (s_ui.brightness.value == NULL || s_ui.brightness.slider == NULL) {
        return;
    }

    ui_brightness = brightness_hw_to_ui(s_ui.settings->base_brightness);
    snprintf(buffer, sizeof(buffer), "%u%%", ui_brightness);
    lv_label_set_text(s_ui.brightness.value, buffer);
    lv_slider_set_value(s_ui.brightness.slider, ui_brightness, LV_ANIM_OFF);
}

static bool brightness_panel_is_open(void)
{
    return s_ui.brightness.panel_overlay != NULL &&
           !lv_obj_has_flag(s_ui.brightness.panel_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void brightness_update_visual_state(int32_t sheet_y)
{
    int32_t clamped_y = LV_CLAMP(BRIGHTNESS_SHEET_OPEN_Y, sheet_y, BRIGHTNESS_SHEET_CLOSED_Y);
    int32_t travel = BRIGHTNESS_SHEET_CLOSED_Y - BRIGHTNESS_SHEET_OPEN_Y;
    int32_t progress = BRIGHTNESS_SHEET_CLOSED_Y - clamped_y;
    lv_opa_t opa = (lv_opa_t)((progress * BRIGHTNESS_SCRIM_OPA) / travel);

    lv_obj_set_y(s_ui.brightness.sheet, clamped_y);
    lv_obj_set_style_bg_opa(s_ui.brightness.overlay, opa, 0);
}

static void brightness_sheet_anim_cb(void *obj, int32_t value)
{
    LV_UNUSED(obj);
    brightness_update_visual_state(value);
}

static void brightness_sheet_anim_ready_cb(lv_anim_t *anim)
{
    s_ui.brightness.animating = false;

    if (!s_ui.brightness.target_open) {
        lv_obj_add_flag(s_ui.brightness.overlay, LV_OBJ_FLAG_HIDDEN);
        brightness_update_visual_state(BRIGHTNESS_SHEET_CLOSED_Y);
    } else {
        brightness_update_visual_state(BRIGHTNESS_SHEET_OPEN_Y);
    }
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

static void brightness_prepare_overlay_for_drag(void)
{
    if (s_ui.brightness.overlay == NULL) {
        return;
    }

    lv_obj_clear_flag(s_ui.brightness.overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_ui.brightness.overlay);
}

static void brightness_animate_sheet_to(int32_t target_y)
{
    lv_anim_t anim;
    int32_t current_y;

    if (s_ui.brightness.overlay == NULL || s_ui.brightness.sheet == NULL) {
        return;
    }

    target_y = LV_CLAMP(BRIGHTNESS_SHEET_OPEN_Y, target_y, BRIGHTNESS_SHEET_CLOSED_Y);
    current_y = lv_obj_get_y(s_ui.brightness.sheet);

    lv_anim_delete(s_ui.brightness.sheet, brightness_sheet_anim_cb);
    s_ui.brightness.animating = false;

    if (current_y == target_y) {
        brightness_update_visual_state(target_y);
        s_ui.brightness.target_open = (target_y <= BRIGHTNESS_SHEET_OPEN_Y);
        if (!s_ui.brightness.target_open) {
            lv_obj_add_flag(s_ui.brightness.overlay, LV_OBJ_FLAG_HIDDEN);
        } else {
            brightness_prepare_overlay_for_drag();
        }
        return;
    }

    brightness_prepare_overlay_for_drag();
    s_ui.brightness.target_open = (target_y <= BRIGHTNESS_SHEET_OPEN_Y);

    s_ui.brightness.animating = true;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, s_ui.brightness.sheet);
    lv_anim_set_exec_cb(&anim, brightness_sheet_anim_cb);
    lv_anim_set_values(&anim, current_y, target_y);
    lv_anim_set_time(&anim, brightness_sheet_anim_duration(current_y, target_y));
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_set_ready_cb(&anim, brightness_sheet_anim_ready_cb);
    lv_anim_start(&anim);
}

static void brightness_overlay_set_visible(bool show)
{
    if (s_ui.brightness.overlay == NULL) {
        return;
    }

    lv_anim_delete(s_ui.brightness.sheet, brightness_sheet_anim_cb);
    s_ui.brightness.animating = false;
    s_ui.brightness.dragging = false;
    s_ui.brightness.drag_from_edge = false;

    if (show) {
        update_brightness_ui();
        brightness_prepare_overlay_for_drag();
        brightness_update_visual_state(BRIGHTNESS_SHEET_OPEN_Y);
    } else {
        brightness_update_visual_state(BRIGHTNESS_SHEET_CLOSED_Y);
        lv_obj_add_flag(s_ui.brightness.overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

static void brightness_overlay_hide_immediately(void)
{
    brightness_overlay_set_visible(false);
}

static void brightness_overlay_hide(void)
{
    if (s_ui.brightness.dragging || s_ui.brightness.animating || lv_obj_has_flag(s_ui.brightness.overlay, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

    brightness_animate_sheet_to(BRIGHTNESS_SHEET_CLOSED_Y);
}

static void brightness_panel_hide(void)
{
    if (s_ui.brightness.panel_overlay == NULL) {
        return;
    }

    lv_obj_add_flag(s_ui.brightness.panel_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void brightness_slider_event_cb(lv_event_t *event)
{
    if (s_ui.suppress_events) {
        return;
    }

    s_ui.settings->base_brightness = brightness_ui_to_hw(lv_slider_get_value(lv_event_get_target(event)));
    update_brightness_ui();
    notify_settings_changed();
}

static void brightness_panel_show(void)
{
    if (s_ui.brightness.panel_overlay == NULL || s_ui.settings_ui.open || alarm_surface_is_open()) {
        return;
    }

    update_brightness_ui();
    lv_obj_clear_flag(s_ui.brightness.panel_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_ui.brightness.panel_overlay);
}

static void brightness_action_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    brightness_overlay_hide_immediately();
    brightness_panel_show();
}

static void alarms_action_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    alarm_management_open();
}

static void settings_action_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    open_settings_tab(SETTINGS_TAB_WIFI);
}

static void brightness_overlay_event_cb(lv_event_t *event)
{
    if (lv_event_get_target(event) == lv_event_get_current_target(event)) {
        brightness_overlay_hide();
    }
}

static void brightness_panel_overlay_event_cb(lv_event_t *event)
{
    if (lv_event_get_target(event) == lv_event_get_current_target(event)) {
        brightness_panel_hide();
    }
}

static void brightness_stop_event_bubble_cb(lv_event_t *event)
{
    lv_event_stop_bubbling(event);
}

static void brightness_drag_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_indev_t *indev = lv_event_get_indev(event);
    lv_point_t point;
    lv_obj_t *target = lv_event_get_target(event);
    bool from_edge = (target == s_ui.brightness.edge_sensor);
    bool from_handle = (target == s_ui.brightness.drag_handle);
    bool from_sheet = (target == s_ui.brightness.sheet);
    int32_t dy;
    int32_t target_y;
    int32_t travel = BRIGHTNESS_SHEET_CLOSED_Y - BRIGHTNESS_SHEET_OPEN_Y;
    int32_t open_threshold_y = BRIGHTNESS_SHEET_CLOSED_Y - ((travel * 45) / 100);

    if (indev == NULL || s_ui.settings_ui.open || alarm_surface_is_open() || brightness_panel_is_open()) {
        return;
    }

    lv_indev_get_point(indev, &point);

    if (!from_edge && !from_handle && !from_sheet) {
        return;
    }

    if (code == LV_EVENT_PRESSED) {
        if (s_ui.brightness.animating) {
            lv_anim_delete(s_ui.brightness.sheet, brightness_sheet_anim_cb);
            s_ui.brightness.animating = false;
        }
        if (from_edge && !lv_obj_has_flag(s_ui.brightness.overlay, LV_OBJ_FLAG_HIDDEN)) {
            return;
        }
        if ((from_handle || from_sheet) && lv_obj_has_flag(s_ui.brightness.overlay, LV_OBJ_FLAG_HIDDEN)) {
            return;
        }

        if (from_edge) {
            update_brightness_ui();
            brightness_prepare_overlay_for_drag();
            brightness_update_visual_state(BRIGHTNESS_SHEET_CLOSED_Y);
            s_ui.brightness.drag_start_y = BRIGHTNESS_SHEET_CLOSED_Y;
        } else {
            s_ui.brightness.drag_start_y = lv_obj_get_y(s_ui.brightness.sheet);
        }

        s_ui.brightness.dragging = true;
        s_ui.brightness.drag_from_edge = from_edge;
        s_ui.brightness.drag_start_point = point;
        return;
    }

    if (!s_ui.brightness.dragging) {
        return;
    }

    if (code == LV_EVENT_PRESS_LOST) {
        s_ui.brightness.dragging = false;
        s_ui.brightness.drag_from_edge = false;
        return;
    }

    if (code == LV_EVENT_PRESSING) {
        dy = point.y - s_ui.brightness.drag_start_point.y;
        target_y = s_ui.brightness.drag_start_y + dy;
        brightness_update_visual_state(target_y);
        return;
    }

    if (code == LV_EVENT_RELEASED) {
        s_ui.brightness.dragging = false;
        s_ui.brightness.drag_from_edge = false;

        target_y = lv_obj_get_y(s_ui.brightness.sheet);
        if (target_y <= open_threshold_y) {
            brightness_animate_sheet_to(BRIGHTNESS_SHEET_OPEN_Y);
        } else {
            brightness_animate_sheet_to(BRIGHTNESS_SHEET_CLOSED_Y);
        }
    }
}

static void create_brightness_pull_hint(void)
{
    s_ui.brightness.pull_hint = lv_obj_create(s_ui.screen);
    lv_obj_set_size(s_ui.brightness.pull_hint, 88, 6);
    lv_obj_set_style_radius(s_ui.brightness.pull_hint, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(s_ui.brightness.pull_hint, 0, 0);
    lv_obj_set_style_bg_color(s_ui.brightness.pull_hint, lv_color_hex(0x808080), 0);
    lv_obj_set_style_bg_opa(s_ui.brightness.pull_hint, LV_OPA_50, 0);
    lv_obj_set_style_opa(s_ui.brightness.pull_hint, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(s_ui.brightness.pull_hint, 0, 0);
    lv_obj_remove_flag(s_ui.brightness.pull_hint, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_ui.brightness.pull_hint, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(s_ui.brightness.pull_hint, LV_ALIGN_BOTTOM_MID, 0, -16);
}

static void create_brightness_edge_sensor(void)
{
    s_ui.brightness.edge_sensor = lv_obj_create(s_ui.screen);
    lv_obj_set_size(s_ui.brightness.edge_sensor, SCREEN_SIZE, BOTTOM_EDGE_ZONE);
    lv_obj_align(s_ui.brightness.edge_sensor, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(s_ui.brightness.edge_sensor, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ui.brightness.edge_sensor, 0, 0);
    lv_obj_set_style_radius(s_ui.brightness.edge_sensor, 0, 0);
    lv_obj_set_style_pad_all(s_ui.brightness.edge_sensor, 0, 0);
    lv_obj_set_scrollbar_mode(s_ui.brightness.edge_sensor, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(s_ui.brightness.edge_sensor, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_ui.brightness.edge_sensor, brightness_drag_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_ui.brightness.edge_sensor, brightness_drag_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(s_ui.brightness.edge_sensor, brightness_drag_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(s_ui.brightness.edge_sensor, brightness_drag_event_cb, LV_EVENT_PRESS_LOST, NULL);
}

static lv_obj_t *create_quick_action_button(lv_obj_t *parent,
                                            const char *symbol,
                                            lv_event_cb_t cb)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_t *icon = lv_label_create(button);

    lv_obj_set_size(button, 128, 128);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x262626), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x3A3A3A), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_pad_all(button, 0, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_add_event_cb(button, brightness_stop_event_bubble_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(button, brightness_stop_event_bubble_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(button, brightness_stop_event_bubble_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(button, brightness_stop_event_bubble_cb, LV_EVENT_PRESS_LOST, NULL);
    lv_obj_add_event_cb(button, brightness_stop_event_bubble_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_set_style_text_font(icon, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(icon, lv_color_white(), 0);
    lv_label_set_text(icon, symbol);
    lv_obj_center(icon);

    if (cb != NULL) {
        lv_obj_add_event_cb(button, cb, LV_EVENT_PRESSED, NULL);
    }

    return button;
}

static void create_brightness_overlay(void)
{
    lv_obj_t *sheet_grabber;
    lv_obj_t *actions;

    s_ui.brightness.overlay = lv_obj_create(s_ui.screen);
    lv_obj_set_size(s_ui.brightness.overlay, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_color(s_ui.brightness.overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_ui.brightness.overlay, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ui.brightness.overlay, 0, 0);
    lv_obj_set_style_radius(s_ui.brightness.overlay, 0, 0);
    lv_obj_set_style_pad_all(s_ui.brightness.overlay, 0, 0);
    lv_obj_remove_flag(s_ui.brightness.overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(s_ui.brightness.overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(s_ui.brightness.overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_ui.brightness.overlay, brightness_overlay_event_cb, LV_EVENT_CLICKED, NULL);

    s_ui.brightness.sheet = lv_obj_create(s_ui.brightness.overlay);
    lv_obj_set_size(s_ui.brightness.sheet, BRIGHTNESS_SHEET_WIDTH, BRIGHTNESS_SHEET_HEIGHT);
    lv_obj_set_pos(s_ui.brightness.sheet, BRIGHTNESS_SHEET_X, BRIGHTNESS_SHEET_CLOSED_Y);
    lv_obj_set_style_bg_color(s_ui.brightness.sheet, lv_color_hex(0x161616), 0);
    lv_obj_set_style_bg_opa(s_ui.brightness.sheet, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_ui.brightness.sheet, 0, 0);
    lv_obj_set_style_radius(s_ui.brightness.sheet, 0, 0);
    lv_obj_set_style_pad_top(s_ui.brightness.sheet, 28, 0);
    lv_obj_set_style_pad_bottom(s_ui.brightness.sheet, 32, 0);
    lv_obj_set_style_pad_left(s_ui.brightness.sheet, 24, 0);
    lv_obj_set_style_pad_right(s_ui.brightness.sheet, 24, 0);
    lv_obj_set_style_shadow_width(s_ui.brightness.sheet, 0, 0);
    lv_obj_remove_flag(s_ui.brightness.sheet, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_ui.brightness.sheet, brightness_drag_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_ui.brightness.sheet, brightness_drag_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(s_ui.brightness.sheet, brightness_drag_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(s_ui.brightness.sheet, brightness_drag_event_cb, LV_EVENT_PRESS_LOST, NULL);
    sheet_grabber = lv_obj_create(s_ui.brightness.sheet);
    lv_obj_set_size(sheet_grabber, 72, 6);
    lv_obj_set_style_radius(sheet_grabber, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(sheet_grabber, 0, 0);
    lv_obj_set_style_bg_color(sheet_grabber, lv_color_hex(0x9C9C9C), 0);
    lv_obj_set_style_bg_opa(sheet_grabber, LV_OPA_70, 0);
    lv_obj_set_style_shadow_width(sheet_grabber, 0, 0);
    lv_obj_remove_flag(sheet_grabber, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(sheet_grabber, LV_ALIGN_TOP_MID, 0, -10);

    s_ui.brightness.drag_handle = lv_obj_create(s_ui.brightness.sheet);
    lv_obj_set_size(s_ui.brightness.drag_handle, lv_pct(100), 60);
    lv_obj_align(s_ui.brightness.drag_handle, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(s_ui.brightness.drag_handle, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ui.brightness.drag_handle, 0, 0);
    lv_obj_set_style_radius(s_ui.brightness.drag_handle, 0, 0);
    lv_obj_set_style_pad_all(s_ui.brightness.drag_handle, 0, 0);
    lv_obj_set_scrollbar_mode(s_ui.brightness.drag_handle, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(s_ui.brightness.drag_handle, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_ui.brightness.drag_handle, LV_OBJ_FLAG_FLOATING);
    lv_obj_add_event_cb(s_ui.brightness.drag_handle, brightness_drag_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_ui.brightness.drag_handle, brightness_drag_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(s_ui.brightness.drag_handle, brightness_drag_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(s_ui.brightness.drag_handle, brightness_drag_event_cb, LV_EVENT_PRESS_LOST, NULL);

    actions = lv_obj_create(s_ui.brightness.sheet);
    lv_obj_set_size(actions, lv_pct(100), 144);
    lv_obj_align(actions, LV_ALIGN_TOP_MID, 0, 34);
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

    create_quick_action_button(actions, QUICK_ACTION_SYMBOL_BRIGHTNESS, brightness_action_event_cb);
    create_quick_action_button(actions, LV_SYMBOL_BELL, alarms_action_event_cb);
    create_quick_action_button(actions, LV_SYMBOL_SETTINGS, settings_action_event_cb);
}

static void create_brightness_panel(void)
{
    lv_obj_t *panel;
    lv_obj_t *title;

    s_ui.brightness.panel_overlay = lv_obj_create(s_ui.screen);
    lv_obj_set_size(s_ui.brightness.panel_overlay, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_color(s_ui.brightness.panel_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_ui.brightness.panel_overlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(s_ui.brightness.panel_overlay, 0, 0);
    lv_obj_set_style_radius(s_ui.brightness.panel_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_ui.brightness.panel_overlay, 0, 0);
    lv_obj_add_flag(s_ui.brightness.panel_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_ui.brightness.panel_overlay, brightness_panel_overlay_event_cb, LV_EVENT_CLICKED, NULL);

    panel = lv_obj_create(s_ui.brightness.panel_overlay);
    s_ui.brightness.panel = panel;
    lv_obj_set_size(panel, 560, 280);
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

    title = lv_label_create(panel);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_label_set_text(title, "Brightness");

    s_ui.brightness.slider = lv_slider_create(panel);
    lv_obj_set_width(s_ui.brightness.slider, lv_pct(100));
    lv_slider_set_range(s_ui.brightness.slider, 0, 100);
    style_slider(s_ui.brightness.slider);
    lv_obj_add_event_cb(s_ui.brightness.slider, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_ui.brightness.value = lv_label_create(panel);
    lv_obj_set_style_text_font(s_ui.brightness.value, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(s_ui.brightness.value, lv_color_white(), 0);
    lv_label_set_text(s_ui.brightness.value, "50%");

    update_brightness_ui();
}
