static void sync_wifi_list(void)
{
    wifi_scan_result_t results[WIFI_TIME_MAX_SCAN_RESULTS];
    uint32_t generation = 0;
    size_t count;

    count = wifi_time_get_scan_results(results, WIFI_TIME_MAX_SCAN_RESULTS, &generation);
    if (generation == s_ui.settings_ui.scan_generation && lv_obj_get_child_count(s_ui.settings_ui.wifi_network_list) != 0) {
        return;
    }

    s_ui.settings_ui.scan_generation = generation;
    lv_obj_clean(s_ui.settings_ui.wifi_network_list);

    if (count == 0) {
        lv_obj_t *label = lv_label_create(s_ui.settings_ui.wifi_network_list);
        lv_label_set_text(label, "No scan results yet");
        lv_obj_set_style_text_color(label, lv_color_hex(0xA8A8A8), 0);
        lv_obj_set_width(label, lv_pct(100));
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        char row[96];

        s_ui.settings_ui.network_ctx[i].network_index = i;
        snprintf(row, sizeof(row), "%s  %ddBm", results[i].ssid[0] ? results[i].ssid : "<hidden>", results[i].rssi);
        lv_obj_t *btn = lv_list_add_button(s_ui.settings_ui.wifi_network_list, LV_SYMBOL_WIFI, row);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x242424), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x343434), LV_STATE_PRESSED);
        lv_obj_set_style_text_color(btn, lv_color_white(), 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 16, 0);
        lv_obj_add_event_cb(btn, wifi_network_btn_event_cb, LV_EVENT_CLICKED, &s_ui.settings_ui.network_ctx[i]);
    }
}

static void sync_night_controls(void)
{
    char brightness[32];
    char status[96];
    uint8_t ui_brightness;

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
    ui_brightness = brightness_hw_to_ui(s_ui.settings->night_mode.brightness);
    lv_slider_set_value(s_ui.settings_ui.night_brightness_slider, ui_brightness, LV_ANIM_OFF);
    s_ui.suppress_events = false;

    snprintf(brightness, sizeof(brightness), "%u%%", ui_brightness);
    lv_label_set_text(s_ui.settings_ui.night_brightness_label, brightness);

    snprintf(status, sizeof(status), "Night mode %s%s",
             s_ui.settings->night_mode.enabled ? "enabled" : "disabled",
             s_ui.runtime->in_night_mode ? "  active now" : "");
    lv_label_set_text(s_ui.settings_ui.night_status_label, status);
}

static void sync_wifi_controls(void)
{
    char saved[160];

    s_ui.suppress_events = true;
    lv_dropdown_set_selected(s_ui.settings_ui.wifi_timezone_dd, s_ui.settings->wifi.timezone_offset_hours + 12);
    s_ui.suppress_events = false;

    lv_label_set_text(s_ui.settings_ui.wifi_status_label, s_ui.runtime->wifi_status[0] ? s_ui.runtime->wifi_status : "Wi-Fi idle");

    if (s_ui.settings->wifi.ssid[0] != '\0') {
        snprintf(saved, sizeof(saved), "Saved network: %s", s_ui.settings->wifi.ssid);
    } else {
        snprintf(saved, sizeof(saved), "Saved network: none");
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
    lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_FOCUSED) {
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
    if (s_ui.settings_ui.tabview != NULL) {
        lv_tabview_set_active(s_ui.settings_ui.tabview, tab_idx, LV_ANIM_OFF);
    }
    lv_obj_clear_flag(s_ui.settings_ui.overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_ui.settings_ui.overlay);
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
    lv_obj_t *target = lv_event_get_target(event);
    lv_point_t point;
    int32_t dy;

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
        dy = point.y - s_ui.settings_ui.close_drag_start_point.y;
        if ((target == s_ui.settings_ui.top_sensor && dy >= SETTINGS_CLOSE_SWIPE_TRIGGER) ||
            (target == s_ui.settings_ui.bottom_sensor && dy <= -SETTINGS_CLOSE_SWIPE_TRIGGER)) {
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

static void night_brightness_event_cb(lv_event_t *event)
{
    if (s_ui.suppress_events) {
        return;
    }

    s_ui.settings->night_mode.brightness = brightness_ui_to_hw(lv_slider_get_value(lv_event_get_target(event)));
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

static void create_wifi_tab(lv_obj_t *tab)
{
    lv_obj_t *card;
    lv_obj_t *row;
    lv_obj_t *title;
    lv_obj_t *subtitle;

    lv_obj_set_style_bg_color(tab, lv_color_hex(0x0D0D0D), 0);
    lv_obj_set_style_bg_opa(tab, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(tab, 20, 0);
    lv_obj_set_style_pad_bottom(tab, 32, 0);
    lv_obj_set_style_pad_row(tab, 18, 0);
    lv_obj_set_layout(tab, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(tab, LV_DIR_VER);

    card = create_card(tab);
    create_section_title(card, "Wi-Fi & time", "Scan networks, save credentials, and sync from NTP.");

    s_ui.settings_ui.wifi_status_label = lv_label_create(card);
    lv_obj_set_style_text_color(s_ui.settings_ui.wifi_status_label, lv_color_white(), 0);
    lv_label_set_text(s_ui.settings_ui.wifi_status_label, "Wi-Fi idle");

    s_ui.settings_ui.wifi_saved_label = lv_label_create(card);
    lv_obj_set_style_text_color(s_ui.settings_ui.wifi_saved_label, lv_color_hex(0xA8A8A8), 0);
    lv_label_set_text(s_ui.settings_ui.wifi_saved_label, "Saved network: none");

    row = create_row(card);
    create_action_button(row, "Scan", wifi_scan_event_cb, NULL);
    create_action_button(row, "Sync time", wifi_sync_event_cb, NULL);
    create_action_button(row, "Forget", wifi_forget_event_cb, NULL);

    title = lv_label_create(card);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_label_set_text(title, "Time zone");

    s_ui.settings_ui.wifi_timezone_dd = create_dropdown(card, s_ui.timezone_options, 220);
    lv_obj_add_event_cb(s_ui.settings_ui.wifi_timezone_dd, timezone_dd_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    subtitle = lv_label_create(card);
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0xA8A8A8), 0);
    lv_label_set_text(subtitle, "Tap a network below to enter the password.");

    s_ui.settings_ui.wifi_network_list = lv_list_create(card);
    lv_obj_set_width(s_ui.settings_ui.wifi_network_list, lv_pct(100));
    lv_obj_set_height(s_ui.settings_ui.wifi_network_list, 280);
    lv_obj_set_style_bg_color(s_ui.settings_ui.wifi_network_list, lv_color_hex(0x111111), 0);
    lv_obj_set_style_bg_opa(s_ui.settings_ui.wifi_network_list, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(s_ui.settings_ui.wifi_network_list, lv_color_white(), 0);
    lv_obj_set_style_border_width(s_ui.settings_ui.wifi_network_list, 0, 0);
    lv_obj_set_style_radius(s_ui.settings_ui.wifi_network_list, 18, 0);
}

static void create_night_tab(lv_obj_t *tab)
{
    lv_obj_t *card;
    lv_obj_t *row;
    lv_obj_t *label;

    lv_obj_set_style_bg_color(tab, lv_color_hex(0x0D0D0D), 0);
    lv_obj_set_style_bg_opa(tab, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(tab, 20, 0);
    lv_obj_set_style_pad_bottom(tab, 32, 0);
    lv_obj_set_style_pad_row(tab, 18, 0);
    lv_obj_set_layout(tab, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(tab, LV_DIR_VER);

    card = create_card(tab);
    create_section_title(card, "Night mode", "Switch face and dim the display in the configured window.");

    row = create_row(card);
    label = lv_label_create(row);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, "Enable");
    s_ui.settings_ui.night_enabled_sw = lv_switch_create(row);
    lv_obj_add_event_cb(s_ui.settings_ui.night_enabled_sw, night_enabled_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_ui.settings_ui.night_status_label = lv_label_create(card);
    lv_obj_set_style_text_color(s_ui.settings_ui.night_status_label, lv_color_hex(0xA8A8A8), 0);
    lv_label_set_text(s_ui.settings_ui.night_status_label, "Night mode disabled");

    label = lv_label_create(card);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, "Start");
    row = create_row(card);
    s_ui.settings_ui.night_start_hour_dd = create_dropdown(row, s_ui.hour_options, 120);
    s_ui.settings_ui.night_start_min_dd = create_dropdown(row, s_ui.minute_options, 120);
    lv_obj_add_event_cb(s_ui.settings_ui.night_start_hour_dd, night_hour_minute_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_ui.settings_ui.night_start_min_dd, night_hour_minute_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    label = lv_label_create(card);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, "End");
    row = create_row(card);
    s_ui.settings_ui.night_end_hour_dd = create_dropdown(row, s_ui.hour_options, 120);
    s_ui.settings_ui.night_end_min_dd = create_dropdown(row, s_ui.minute_options, 120);
    lv_obj_add_event_cb(s_ui.settings_ui.night_end_hour_dd, night_hour_minute_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_ui.settings_ui.night_end_min_dd, night_hour_minute_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    label = lv_label_create(card);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, "Night brightness");
    s_ui.settings_ui.night_brightness_slider = lv_slider_create(card);
    lv_slider_set_range(s_ui.settings_ui.night_brightness_slider, 0, 100);
    lv_obj_set_width(s_ui.settings_ui.night_brightness_slider, lv_pct(100));
    style_slider(s_ui.settings_ui.night_brightness_slider);
    lv_obj_add_event_cb(s_ui.settings_ui.night_brightness_slider, night_brightness_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    s_ui.settings_ui.night_brightness_label = lv_label_create(card);
    lv_obj_set_style_text_color(s_ui.settings_ui.night_brightness_label, lv_color_hex(0xA8A8A8), 0);

    label = lv_label_create(card);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, "Night face");
    row = create_row(card);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_margin_bottom(row, 12, 0);
    s_ui.settings_ui.night_face_dd = create_dropdown(row, s_ui.face_options, 240);
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
    lv_obj_set_style_text_font(s_ui.settings_ui.wifi_dialog_title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_ui.settings_ui.wifi_dialog_title, lv_color_white(), 0);
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
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
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
    lv_obj_t *header;
    lv_obj_t *title;
    lv_obj_t *close_btn;
    lv_obj_t *close_label;
    lv_obj_t *tab_wifi;
    lv_obj_t *tab_night;

    s_ui.settings_ui.overlay = lv_obj_create(s_ui.screen);
    lv_obj_set_size(s_ui.settings_ui.overlay, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_color(s_ui.settings_ui.overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_ui.settings_ui.overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_ui.settings_ui.overlay, 0, 0);
    lv_obj_set_style_radius(s_ui.settings_ui.overlay, 0, 0);
    lv_obj_set_style_pad_all(s_ui.settings_ui.overlay, 0, 0);
    lv_obj_add_flag(s_ui.settings_ui.overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_ui.settings_ui.overlay, settings_overlay_event_cb, LV_EVENT_CLICKED, NULL);

    s_ui.settings_ui.panel = lv_obj_create(s_ui.settings_ui.overlay);
    lv_obj_set_size(s_ui.settings_ui.panel, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_align(s_ui.settings_ui.panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(s_ui.settings_ui.panel, lv_color_hex(0x0D0D0D), 0);
    lv_obj_set_style_bg_opa(s_ui.settings_ui.panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_ui.settings_ui.panel, 0, 0);
    lv_obj_set_style_radius(s_ui.settings_ui.panel, 0, 0);
    lv_obj_set_style_pad_all(s_ui.settings_ui.panel, 0, 0);
    lv_obj_set_layout(s_ui.settings_ui.panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(s_ui.settings_ui.panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(s_ui.settings_ui.panel, LV_OBJ_FLAG_SCROLLABLE);

    header = lv_obj_create(s_ui.settings_ui.panel);
    lv_obj_set_width(header, lv_pct(100));
    lv_obj_set_height(header, SETTINGS_HEADER_HEIGHT);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_pad_left(header, 18, 0);
    lv_obj_set_style_pad_right(header, 18, 0);
    lv_obj_set_style_pad_top(header, 18, 0);
    lv_obj_set_style_pad_bottom(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    title = lv_label_create(header);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_width(title, lv_pct(100));
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(title, "Clock settings");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    close_btn = lv_button_create(header);
    lv_obj_set_size(close_btn, 44, 44);
    lv_obj_set_style_radius(close_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(0x262626), 0);
    lv_obj_set_style_border_width(close_btn, 0, 0);
    lv_obj_align(close_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_add_event_cb(close_btn, settings_close_event_cb, LV_EVENT_CLICKED, NULL);
    close_label = lv_label_create(close_btn);
    lv_obj_set_style_text_color(close_label, lv_color_white(), 0);
    lv_label_set_text(close_label, LV_SYMBOL_CLOSE);
    lv_obj_center(close_label);

    s_ui.settings_ui.tabview = lv_tabview_create(s_ui.settings_ui.panel);
    lv_obj_set_width(s_ui.settings_ui.tabview, lv_pct(100));
    lv_obj_set_flex_grow(s_ui.settings_ui.tabview, 1);
    lv_obj_set_style_bg_color(s_ui.settings_ui.tabview, lv_color_hex(0x0D0D0D), 0);
    lv_obj_set_style_bg_opa(s_ui.settings_ui.tabview, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_ui.settings_ui.tabview, 0, 0);
    lv_tabview_set_tab_bar_size(s_ui.settings_ui.tabview, 54);
    lv_obj_t *tab_bar = lv_tabview_get_tab_bar(s_ui.settings_ui.tabview);
    lv_obj_set_style_bg_color(tab_bar, lv_color_hex(0x131313), 0);
    lv_obj_set_style_bg_opa(tab_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tab_bar, 0, 0);
    lv_obj_set_style_pad_left(tab_bar, 24, 0);
    lv_obj_set_style_pad_right(tab_bar, 24, 0);
    lv_obj_set_style_text_color(tab_bar, lv_color_hex(0xA8A8A8), LV_PART_ITEMS);
    lv_obj_set_style_text_color(tab_bar, lv_color_hex(0xA8A8A8), LV_PART_MAIN);
    lv_obj_set_style_bg_color(tab_bar, lv_color_hex(0x232323), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(tab_bar, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_radius(tab_bar, 0, LV_PART_ITEMS);
    lv_obj_set_style_border_width(tab_bar, 0, LV_PART_ITEMS);
    lv_obj_set_style_border_width(tab_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_text_font(tab_bar, &lv_font_montserrat_20, LV_PART_ITEMS);
    lv_obj_set_style_text_color(tab_bar, lv_color_black(), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(tab_bar, lv_color_hex(0xC4A24C), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(tab_bar, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_CHECKED);

    tab_wifi = lv_tabview_add_tab(s_ui.settings_ui.tabview, "Wi-Fi");
    tab_night = lv_tabview_add_tab(s_ui.settings_ui.tabview, "Night");

    create_wifi_tab(tab_wifi);
    create_night_tab(tab_night);

    s_ui.settings_ui.top_sensor = lv_obj_create(s_ui.settings_ui.overlay);
    lv_obj_set_size(s_ui.settings_ui.top_sensor, SCREEN_SIZE, SETTINGS_CLOSE_EDGE_ZONE);
    lv_obj_align(s_ui.settings_ui.top_sensor, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(s_ui.settings_ui.top_sensor, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ui.settings_ui.top_sensor, 0, 0);
    lv_obj_set_style_radius(s_ui.settings_ui.top_sensor, 0, 0);
    lv_obj_set_style_pad_all(s_ui.settings_ui.top_sensor, 0, 0);
    lv_obj_set_scrollbar_mode(s_ui.settings_ui.top_sensor, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(s_ui.settings_ui.top_sensor, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_ui.settings_ui.top_sensor, settings_close_swipe_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_ui.settings_ui.top_sensor, settings_close_swipe_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(s_ui.settings_ui.top_sensor, settings_close_swipe_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(s_ui.settings_ui.top_sensor, settings_close_swipe_event_cb, LV_EVENT_PRESS_LOST, NULL);

    s_ui.settings_ui.bottom_sensor = lv_obj_create(s_ui.settings_ui.overlay);
    lv_obj_set_size(s_ui.settings_ui.bottom_sensor, SCREEN_SIZE, SETTINGS_CLOSE_EDGE_ZONE);
    lv_obj_align(s_ui.settings_ui.bottom_sensor, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(s_ui.settings_ui.bottom_sensor, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ui.settings_ui.bottom_sensor, 0, 0);
    lv_obj_set_style_radius(s_ui.settings_ui.bottom_sensor, 0, 0);
    lv_obj_set_style_pad_all(s_ui.settings_ui.bottom_sensor, 0, 0);
    lv_obj_set_scrollbar_mode(s_ui.settings_ui.bottom_sensor, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(s_ui.settings_ui.bottom_sensor, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_ui.settings_ui.bottom_sensor, settings_close_swipe_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_ui.settings_ui.bottom_sensor, settings_close_swipe_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(s_ui.settings_ui.bottom_sensor, settings_close_swipe_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(s_ui.settings_ui.bottom_sensor, settings_close_swipe_event_cb, LV_EVENT_PRESS_LOST, NULL);

    create_wifi_dialog();
}

