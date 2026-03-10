static void style_centered_label(lv_obj_t *label, const lv_font_t *font, lv_color_t color)
{
    lv_obj_set_width(label, lv_pct(100));
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label, color, 0);
    if (font != NULL) {
        lv_obj_set_style_text_font(label, font, 0);
    }
}

static void create_centered_card_title(lv_obj_t *parent, const char *title)
{
    lv_obj_t *label = lv_label_create(parent);

    style_centered_label(label, &lv_font_montserrat_24, lv_color_white());
    lv_label_set_text(label, title);
}

static void center_card_children(lv_obj_t *card)
{
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
}

static lv_obj_t *create_network_button(lv_obj_t *parent, const char *ssid, const char *meta, network_ctx_t *ctx)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_t *title = lv_label_create(button);
    lv_obj_t *subtitle = lv_label_create(button);

    lv_obj_set_width(button, lv_pct(100));
    lv_obj_set_height(button, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(button, 22, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x232323), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x343434), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_pad_hor(button, 22, 0);
    lv_obj_set_style_pad_ver(button, 18, 0);
    lv_obj_set_style_pad_row(button, 8, 0);
    lv_obj_set_style_shadow_width(button, 16, 0);
    lv_obj_set_style_shadow_opa(button, LV_OPA_10, 0);
    lv_obj_set_style_shadow_color(button, lv_color_black(), 0);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(button, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(button, LV_FLEX_FLOW_COLUMN);

    lv_obj_set_width(title, lv_pct(100));
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_label_set_text(title, ssid);

    lv_obj_set_width(subtitle, lv_pct(100));
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0xA8A8A8), 0);
    lv_label_set_text(subtitle, meta);

    lv_obj_add_event_cb(button, wifi_network_btn_event_cb, LV_EVENT_CLICKED, ctx);
    return button;
}

static void sync_wifi_list(void)
{
    wifi_scan_result_t results[WIFI_TIME_MAX_SCAN_RESULTS];
    uint32_t generation = 0;
    size_t count;

    if (s_ui.settings_ui.wifi_network_list == NULL) {
        return;
    }

    count = wifi_time_get_scan_results(results, WIFI_TIME_MAX_SCAN_RESULTS, &generation);
    if (generation == s_ui.settings_ui.scan_generation &&
        lv_obj_get_child_count(s_ui.settings_ui.wifi_network_list) != 0) {
        return;
    }

    s_ui.settings_ui.scan_generation = generation;
    lv_obj_clean(s_ui.settings_ui.wifi_network_list);

    if (count == 0) {
        lv_obj_t *label = lv_label_create(s_ui.settings_ui.wifi_network_list);

        style_centered_label(label, NULL, lv_color_hex(0xA8A8A8));
        lv_label_set_text(label, "No networks yet");
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        char meta[64];
        const char *ssid = results[i].ssid[0] ? results[i].ssid : "<hidden>";

        s_ui.settings_ui.network_ctx[i].network_index = i;
        snprintf(meta, sizeof(meta), "%ddBm", results[i].rssi);
        create_network_button(s_ui.settings_ui.wifi_network_list, ssid, meta, &s_ui.settings_ui.network_ctx[i]);
    }
}

static void sync_night_controls(void)
{
    char status[96];

    if (s_ui.settings_ui.night_enabled_sw == NULL) {
        return;
    }

    s_ui.suppress_events = true;
    if (s_ui.settings->night_mode.enabled) {
        lv_obj_add_state(s_ui.settings_ui.night_enabled_sw, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(s_ui.settings_ui.night_enabled_sw, LV_STATE_CHECKED);
    }

    lv_dropdown_set_selected(s_ui.settings_ui.night_start_hour_dd, s_ui.settings->night_mode.start_hour);
    lv_dropdown_set_selected(s_ui.settings_ui.night_start_min_dd, s_ui.settings->night_mode.start_minute);
    lv_dropdown_set_selected(s_ui.settings_ui.night_end_hour_dd, s_ui.settings->night_mode.end_hour);
    lv_dropdown_set_selected(s_ui.settings_ui.night_end_min_dd, s_ui.settings->night_mode.end_minute);
    lv_dropdown_set_selected(s_ui.settings_ui.night_face_dd, s_ui.settings->night_mode.face);
    s_ui.suppress_events = false;

    snprintf(status, sizeof(status), "Night mode %s%s",
             s_ui.settings->night_mode.enabled ? "enabled" : "disabled",
             s_ui.runtime->in_night_mode ? "  active now" : "");
    lv_label_set_text(s_ui.settings_ui.night_status_label, status);
}

static void sync_wifi_controls(void)
{
    char saved[160];

    if (s_ui.settings_ui.wifi_timezone_dd == NULL) {
        return;
    }

    s_ui.suppress_events = true;
    lv_dropdown_set_selected(s_ui.settings_ui.wifi_timezone_dd, s_ui.settings->wifi.timezone_offset_hours + 12);
    s_ui.suppress_events = false;

    lv_label_set_text(s_ui.settings_ui.wifi_status_label,
                      s_ui.runtime->wifi_status[0] ? s_ui.runtime->wifi_status : "Wi-Fi idle");

    if (s_ui.settings->wifi.ssid[0] != '\0') {
        snprintf(saved, sizeof(saved), "Saved: %s", s_ui.settings->wifi.ssid);
    } else {
        snprintf(saved, sizeof(saved), "Saved: none");
    }
    lv_label_set_text(s_ui.settings_ui.wifi_saved_label, saved);

    sync_wifi_list();
}

static void refresh_settings_controls(void)
{
    sync_wifi_controls();
    sync_night_controls();
    sync_alarm_controls();
}

static void wifi_network_btn_event_cb(lv_event_t *event)
{
    wifi_scan_result_t results[WIFI_TIME_MAX_SCAN_RESULTS];
    network_ctx_t *ctx = (network_ctx_t *)lv_event_get_user_data(event);
    uint32_t generation = 0;
    size_t count;

    count = wifi_time_get_scan_results(results, WIFI_TIME_MAX_SCAN_RESULTS, &generation);
    if (ctx == NULL || ctx->network_index >= count) {
        return;
    }

    snprintf(s_ui.settings_ui.pending_ssid, sizeof(s_ui.settings_ui.pending_ssid), "%s", results[ctx->network_index].ssid);
    lv_label_set_text_fmt(s_ui.settings_ui.wifi_dialog_title, "Join %s", s_ui.settings_ui.pending_ssid);
    lv_textarea_set_text(s_ui.settings_ui.wifi_password_ta, "");
    lv_keyboard_set_textarea(s_ui.settings_ui.wifi_keyboard, s_ui.settings_ui.wifi_password_ta);
    lv_obj_clear_flag(s_ui.settings_ui.wifi_dialog_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_ui.settings_ui.wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_ui.settings_ui.wifi_dialog_overlay);
}

static void wifi_scan_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    if (s_ui.callbacks.on_wifi_scan_requested != NULL) {
        s_ui.callbacks.on_wifi_scan_requested(s_ui.user_ctx);
    }
}

static void wifi_forget_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    s_ui.settings->wifi.ssid[0] = '\0';
    s_ui.settings->wifi.password[0] = '\0';
    notify_settings_changed();

    if (s_ui.callbacks.on_wifi_forget_requested != NULL) {
        s_ui.callbacks.on_wifi_forget_requested(s_ui.user_ctx);
    }
}

static void wifi_sync_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    if (s_ui.callbacks.on_wifi_sync_requested != NULL) {
        s_ui.callbacks.on_wifi_sync_requested(s_ui.user_ctx);
    }
}

static void wifi_dialog_close_event_cb(lv_event_t *event)
{
    if (lv_event_get_current_target(event) == s_ui.settings_ui.wifi_dialog_overlay &&
        lv_event_get_target(event) != s_ui.settings_ui.wifi_dialog_overlay) {
        return;
    }

    s_ui.settings_ui.pending_ssid[0] = '\0';
    lv_keyboard_set_textarea(s_ui.settings_ui.wifi_keyboard, NULL);
    lv_obj_add_flag(s_ui.settings_ui.wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_ui.settings_ui.wifi_dialog_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void wifi_dialog_connect_event_cb(lv_event_t *event)
{
    const char *password;

    LV_UNUSED(event);

    if (s_ui.settings_ui.pending_ssid[0] == '\0') {
        return;
    }

    password = lv_textarea_get_text(s_ui.settings_ui.wifi_password_ta);
    snprintf(s_ui.settings->wifi.ssid, sizeof(s_ui.settings->wifi.ssid), "%s", s_ui.settings_ui.pending_ssid);
    snprintf(s_ui.settings->wifi.password, sizeof(s_ui.settings->wifi.password), "%s", password ? password : "");
    notify_settings_changed();

    if (s_ui.callbacks.on_wifi_connect_requested != NULL) {
        s_ui.callbacks.on_wifi_connect_requested(s_ui.user_ctx, s_ui.settings->wifi.ssid, s_ui.settings->wifi.password);
    }

    wifi_dialog_close_event_cb(event);
}

static void wifi_ta_focus_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_FOCUSED) {
        lv_keyboard_set_textarea(s_ui.settings_ui.wifi_keyboard, s_ui.settings_ui.wifi_password_ta);
        lv_obj_clear_flag(s_ui.settings_ui.wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

static void wifi_keyboard_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        wifi_dialog_close_event_cb(event);
    }
}

static void settings_scroll_to_section(uint32_t tab_idx)
{
    if (s_ui.settings_ui.content == NULL) {
        return;
    }

    if (tab_idx == SETTINGS_TAB_NIGHT && s_ui.settings_ui.night_card != NULL) {
        lv_obj_scroll_to_view(s_ui.settings_ui.night_card, LV_ANIM_OFF);
        return;
    }

    if (s_ui.settings_ui.wifi_card != NULL) {
        lv_obj_scroll_to_view(s_ui.settings_ui.wifi_card, LV_ANIM_OFF);
    } else {
        lv_obj_scroll_to_y(s_ui.settings_ui.content, 0, LV_ANIM_OFF);
    }
}

static void open_settings_tab(uint32_t tab_idx)
{
    brightness_overlay_hide_immediately();
    brightness_panel_hide();
    if (s_ui.alarms.editor_open) {
        alarm_editor_close();
    }
    if (s_ui.alarms.open) {
        alarm_management_close();
    }

    s_ui.settings_ui.open = true;
    refresh_settings_controls();
    if (s_ui.settings_ui.scan_generation == 0 && s_ui.callbacks.on_wifi_scan_requested != NULL) {
        s_ui.callbacks.on_wifi_scan_requested(s_ui.user_ctx);
    }

    lv_obj_clear_flag(s_ui.settings_ui.overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_ui.settings_ui.overlay);
    settings_scroll_to_section(tab_idx);
}

static void settings_button_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    open_settings_tab(SETTINGS_TAB_WIFI);
}

static void settings_close_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    s_ui.settings_ui.open = false;
    s_ui.settings_ui.close_dragging = false;
    if (s_ui.settings_ui.wifi_dialog_overlay != NULL) {
        lv_obj_add_flag(s_ui.settings_ui.wifi_dialog_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_ui.settings_ui.wifi_keyboard != NULL) {
        lv_keyboard_set_textarea(s_ui.settings_ui.wifi_keyboard, NULL);
        lv_obj_add_flag(s_ui.settings_ui.wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_flag(s_ui.settings_ui.overlay, LV_OBJ_FLAG_HIDDEN);
    show_affordances_temporarily();
}

static void settings_overlay_event_cb(lv_event_t *event)
{
    if (lv_event_get_target(event) == lv_event_get_current_target(event)) {
        settings_close_event_cb(event);
    }
}

static void settings_close_swipe_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_indev_t *indev = lv_event_get_indev(event);
    ui_surface_edge_t edge = (ui_surface_edge_t)(uintptr_t)lv_event_get_user_data(event);
    lv_point_t point;

    if (!s_ui.settings_ui.open || indev == NULL) {
        return;
    }

    lv_indev_get_point(indev, &point);

    if (code == LV_EVENT_PRESSED) {
        s_ui.settings_ui.close_dragging = true;
        s_ui.settings_ui.close_drag_start_point = point;
        return;
    }

    if (!s_ui.settings_ui.close_dragging) {
        return;
    }

    if (code == LV_EVENT_PRESSING) {
        if (ui_surface_edge_swipe_trigger(edge,
                                          &s_ui.settings_ui.close_drag_start_point,
                                          &point,
                                          SETTINGS_CLOSE_SWIPE_TRIGGER)) {
            settings_close_event_cb(event);
        }
        return;
    }

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        s_ui.settings_ui.close_dragging = false;
    }
}

static void timezone_dd_event_cb(lv_event_t *event)
{
    if (s_ui.suppress_events) {
        return;
    }

    s_ui.settings->wifi.timezone_offset_hours = (int8_t)lv_dropdown_get_selected(lv_event_get_target(event)) - 12;
    notify_settings_changed();
}

static void night_enabled_event_cb(lv_event_t *event)
{
    if (s_ui.suppress_events) {
        return;
    }

    s_ui.settings->night_mode.enabled = lv_obj_has_state(lv_event_get_target(event), LV_STATE_CHECKED);
    notify_settings_changed();
    sync_night_controls();
}

static void night_hour_minute_event_cb(lv_event_t *event)
{
    lv_obj_t *target = lv_event_get_target(event);

    if (s_ui.suppress_events) {
        return;
    }

    if (target == s_ui.settings_ui.night_start_hour_dd) {
        s_ui.settings->night_mode.start_hour = lv_dropdown_get_selected(target);
    } else if (target == s_ui.settings_ui.night_start_min_dd) {
        s_ui.settings->night_mode.start_minute = lv_dropdown_get_selected(target);
    } else if (target == s_ui.settings_ui.night_end_hour_dd) {
        s_ui.settings->night_mode.end_hour = lv_dropdown_get_selected(target);
    } else if (target == s_ui.settings_ui.night_end_min_dd) {
        s_ui.settings->night_mode.end_minute = lv_dropdown_get_selected(target);
    }

    notify_settings_changed();
    sync_night_controls();
}

static void night_face_event_cb(lv_event_t *event)
{
    if (s_ui.suppress_events) {
        return;
    }

    s_ui.settings->night_mode.face = (clock_face_id_t)lv_dropdown_get_selected(lv_event_get_target(event));
    notify_settings_changed();
}

static void create_wifi_card(void)
{
    lv_obj_t *row;

    s_ui.settings_ui.wifi_card = create_card(s_ui.settings_ui.content);
    center_card_children(s_ui.settings_ui.wifi_card);
    create_centered_card_title(s_ui.settings_ui.wifi_card, "Wi-Fi");

    s_ui.settings_ui.wifi_status_label = lv_label_create(s_ui.settings_ui.wifi_card);
    style_centered_label(s_ui.settings_ui.wifi_status_label, &lv_font_montserrat_20, lv_color_white());
    lv_label_set_text(s_ui.settings_ui.wifi_status_label, "Wi-Fi idle");

    s_ui.settings_ui.wifi_saved_label = lv_label_create(s_ui.settings_ui.wifi_card);
    style_centered_label(s_ui.settings_ui.wifi_saved_label, NULL, lv_color_hex(0xA8A8A8));
    lv_label_set_text(s_ui.settings_ui.wifi_saved_label, "Saved: none");

    row = create_row(s_ui.settings_ui.wifi_card);
    center_row(row);
    create_action_button(row, "Scan", wifi_scan_event_cb, NULL);
    create_action_button(row, "Sync time", wifi_sync_event_cb, NULL);
    create_action_button(row, "Forget", wifi_forget_event_cb, NULL);
}

static void create_timezone_card(void)
{
    lv_obj_t *row;

    s_ui.settings_ui.timezone_card = create_card(s_ui.settings_ui.content);
    center_card_children(s_ui.settings_ui.timezone_card);
    create_centered_card_title(s_ui.settings_ui.timezone_card, "Time zone");

    row = create_row(s_ui.settings_ui.timezone_card);
    center_row(row);
    s_ui.settings_ui.wifi_timezone_dd = create_dropdown(row, s_ui.timezone_options, 240);
    lv_obj_add_event_cb(s_ui.settings_ui.wifi_timezone_dd, timezone_dd_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

static void create_networks_card(void)
{
    s_ui.settings_ui.networks_card = create_card(s_ui.settings_ui.content);
    create_centered_card_title(s_ui.settings_ui.networks_card, "Networks");

    s_ui.settings_ui.wifi_network_list = lv_obj_create(s_ui.settings_ui.networks_card);
    lv_obj_set_width(s_ui.settings_ui.wifi_network_list, lv_pct(100));
    lv_obj_set_height(s_ui.settings_ui.wifi_network_list, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(s_ui.settings_ui.wifi_network_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ui.settings_ui.wifi_network_list, 0, 0);
    lv_obj_set_style_radius(s_ui.settings_ui.wifi_network_list, 0, 0);
    lv_obj_set_style_pad_all(s_ui.settings_ui.wifi_network_list, 0, 0);
    lv_obj_set_style_pad_row(s_ui.settings_ui.wifi_network_list, 14, 0);
    lv_obj_set_layout(s_ui.settings_ui.wifi_network_list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(s_ui.settings_ui.wifi_network_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(s_ui.settings_ui.wifi_network_list, LV_OBJ_FLAG_SCROLLABLE);
}

static void create_night_card(void)
{
    lv_obj_t *row;
    lv_obj_t *label;

    s_ui.settings_ui.night_card = create_card(s_ui.settings_ui.content);
    center_card_children(s_ui.settings_ui.night_card);
    create_centered_card_title(s_ui.settings_ui.night_card, "Night mode");

    row = create_row(s_ui.settings_ui.night_card);
    center_row(row);
    label = lv_label_create(row);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, "Enabled");
    s_ui.settings_ui.night_enabled_sw = lv_switch_create(row);
    lv_obj_add_event_cb(s_ui.settings_ui.night_enabled_sw, night_enabled_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_ui.settings_ui.night_status_label = lv_label_create(s_ui.settings_ui.night_card);
    style_centered_label(s_ui.settings_ui.night_status_label, NULL, lv_color_hex(0xA8A8A8));
    lv_label_set_text(s_ui.settings_ui.night_status_label, "Night mode disabled");

    label = lv_label_create(s_ui.settings_ui.night_card);
    style_centered_label(label, NULL, lv_color_white());
    lv_label_set_text(label, "Start");
    row = create_row(s_ui.settings_ui.night_card);
    center_row(row);
    s_ui.settings_ui.night_start_hour_dd = create_dropdown(row, s_ui.hour_options, 120);
    s_ui.settings_ui.night_start_min_dd = create_dropdown(row, s_ui.minute_options, 120);
    lv_obj_add_event_cb(s_ui.settings_ui.night_start_hour_dd, night_hour_minute_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_ui.settings_ui.night_start_min_dd, night_hour_minute_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    label = lv_label_create(s_ui.settings_ui.night_card);
    style_centered_label(label, NULL, lv_color_white());
    lv_label_set_text(label, "End");
    row = create_row(s_ui.settings_ui.night_card);
    center_row(row);
    s_ui.settings_ui.night_end_hour_dd = create_dropdown(row, s_ui.hour_options, 120);
    s_ui.settings_ui.night_end_min_dd = create_dropdown(row, s_ui.minute_options, 120);
    lv_obj_add_event_cb(s_ui.settings_ui.night_end_hour_dd, night_hour_minute_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_ui.settings_ui.night_end_min_dd, night_hour_minute_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    label = lv_label_create(s_ui.settings_ui.night_card);
    style_centered_label(label, NULL, lv_color_white());
    lv_label_set_text(label, "Night face");
    row = create_row(s_ui.settings_ui.night_card);
    center_row(row);
    s_ui.settings_ui.night_face_dd = create_dropdown(row, s_ui.face_options, 260);
    lv_obj_add_event_cb(s_ui.settings_ui.night_face_dd, night_face_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

static void create_wifi_dialog(void)
{
    lv_obj_t *panel;
    lv_obj_t *row;

    s_ui.settings_ui.wifi_dialog_overlay = lv_obj_create(s_ui.screen);
    lv_obj_set_size(s_ui.settings_ui.wifi_dialog_overlay, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_color(s_ui.settings_ui.wifi_dialog_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_ui.settings_ui.wifi_dialog_overlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(s_ui.settings_ui.wifi_dialog_overlay, 0, 0);
    lv_obj_set_style_radius(s_ui.settings_ui.wifi_dialog_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_ui.settings_ui.wifi_dialog_overlay, 0, 0);
    lv_obj_add_flag(s_ui.settings_ui.wifi_dialog_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_ui.settings_ui.wifi_dialog_overlay, wifi_dialog_close_event_cb, LV_EVENT_CLICKED, NULL);

    panel = lv_obj_create(s_ui.settings_ui.wifi_dialog_overlay);
    s_ui.settings_ui.wifi_dialog = panel;
    lv_obj_set_size(panel, WIFI_DIALOG_WIDTH, WIFI_DIALOG_HEIGHT);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 72);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x151515), 0);
    lv_obj_set_style_radius(panel, 28, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_text_color(panel, lv_color_white(), 0);
    lv_obj_set_style_pad_all(panel, 24, 0);
    lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel, 18, 0);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);

    s_ui.settings_ui.wifi_dialog_title = lv_label_create(panel);
    style_centered_label(s_ui.settings_ui.wifi_dialog_title, &lv_font_montserrat_24, lv_color_white());
    lv_label_set_text(s_ui.settings_ui.wifi_dialog_title, "Join network");

    s_ui.settings_ui.wifi_password_ta = lv_textarea_create(panel);
    lv_obj_set_width(s_ui.settings_ui.wifi_password_ta, lv_pct(100));
    lv_obj_set_style_bg_color(s_ui.settings_ui.wifi_password_ta, lv_color_hex(0x232323), 0);
    lv_obj_set_style_bg_opa(s_ui.settings_ui.wifi_password_ta, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(s_ui.settings_ui.wifi_password_ta, lv_color_white(), 0);
    lv_obj_set_style_text_color(s_ui.settings_ui.wifi_password_ta, lv_color_hex(0x8E8E8E), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_border_width(s_ui.settings_ui.wifi_password_ta, 0, 0);
    lv_obj_set_style_radius(s_ui.settings_ui.wifi_password_ta, 16, 0);
    lv_textarea_set_placeholder_text(s_ui.settings_ui.wifi_password_ta, "Password");
    lv_textarea_set_password_mode(s_ui.settings_ui.wifi_password_ta, true);
    lv_textarea_set_one_line(s_ui.settings_ui.wifi_password_ta, true);
    lv_obj_add_event_cb(s_ui.settings_ui.wifi_password_ta, wifi_ta_focus_event_cb, LV_EVENT_FOCUSED, NULL);

    row = create_row(panel);
    center_row(row);
    create_action_button(row, "Cancel", wifi_dialog_close_event_cb, NULL);
    create_action_button(row, "Connect", wifi_dialog_connect_event_cb, NULL);

    s_ui.settings_ui.wifi_keyboard = lv_keyboard_create(s_ui.settings_ui.wifi_dialog_overlay);
    lv_obj_set_size(s_ui.settings_ui.wifi_keyboard, KEYBOARD_WIDTH, KEYBOARD_HEIGHT);
    lv_obj_align(s_ui.settings_ui.wifi_keyboard, LV_ALIGN_BOTTOM_MID, 0, -KEYBOARD_BOTTOM_INSET);
    lv_obj_set_style_radius(s_ui.settings_ui.wifi_keyboard, 28, 0);
    lv_obj_set_style_bg_color(s_ui.settings_ui.wifi_keyboard, lv_color_hex(0x151515), 0);
    lv_obj_set_style_bg_opa(s_ui.settings_ui.wifi_keyboard, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_ui.settings_ui.wifi_keyboard, 0, 0);
    lv_obj_add_flag(s_ui.settings_ui.wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_ui.settings_ui.wifi_keyboard, wifi_keyboard_event_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(s_ui.settings_ui.wifi_keyboard, wifi_keyboard_event_cb, LV_EVENT_CANCEL, NULL);
}

static void create_settings_overlay(void)
{
    ui_surface_t surface;
    lv_obj_t *title;

    ui_surface_create_fullscreen(&surface,
                                 s_ui.screen,
                                 lv_color_black(),
                                 LV_OPA_COVER,
                                 lv_color_hex(0x0D0D0D),
                                 SETTINGS_HEADER_HEIGHT,
                                 "Settings",
                                 settings_overlay_event_cb,
                                 settings_close_event_cb);
    s_ui.settings_ui.overlay = surface.overlay;
    s_ui.settings_ui.panel = surface.panel;
    s_ui.settings_ui.content = surface.content;
    title = surface.title;
    lv_obj_set_width(title, lv_pct(100));
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    create_wifi_card();
    create_timezone_card();
    create_networks_card();
    create_night_card();

    ui_surface_create_edge_sensor(s_ui.settings_ui.overlay,
                                  &s_ui.settings_ui.top_sensor,
                                  UI_SURFACE_EDGE_TOP,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  (void *)UI_SURFACE_EDGE_TOP);
    ui_surface_create_edge_sensor(s_ui.settings_ui.overlay,
                                  &s_ui.settings_ui.bottom_sensor,
                                  UI_SURFACE_EDGE_BOTTOM,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  (void *)UI_SURFACE_EDGE_BOTTOM);
    ui_surface_create_edge_sensor(s_ui.settings_ui.overlay,
                                  &s_ui.settings_ui.left_sensor,
                                  UI_SURFACE_EDGE_LEFT,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  (void *)UI_SURFACE_EDGE_LEFT);
    ui_surface_create_edge_sensor(s_ui.settings_ui.overlay,
                                  &s_ui.settings_ui.right_sensor,
                                  UI_SURFACE_EDGE_RIGHT,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  (void *)UI_SURFACE_EDGE_RIGHT);

    create_wifi_dialog();
}
