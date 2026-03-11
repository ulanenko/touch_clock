static void style_centered_label(lv_obj_t *label, const lv_font_t *font, lv_color_t color)
{
    lv_obj_set_width(label, lv_pct(100));
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label, color, 0);
    if (font != NULL) {
        lv_obj_set_style_text_font(label, font, 0);
    }
}

static void close_wifi_dialog(void);
static void settings_set_label_text_if_changed(lv_obj_t *label, const char *text);
static const void *settings_get_face_preview_src(clock_face_id_t face);
static void sync_night_schedule_lock_state(void);
static void sync_night_face_preview(void);
static void sync_night_face_picker_previews(void);
static void sync_night_face_picker_selection(void);
static void close_night_face_picker(void);
static void settings_set_face_preview_src(lv_obj_t *image, clock_face_id_t face);
#define WIFI_UI_MAX_VISIBLE_NETWORKS 8
#define NIGHT_FACE_PREVIEW_SIZE 120

static void settings_set_roller_locked(lv_obj_t *roller, bool locked)
{
    if (roller == NULL) {
        return;
    }

    if (locked) {
        lv_obj_set_scroll_dir(roller, LV_DIR_NONE);
        lv_roller_set_visible_row_count(roller, 1);
        lv_obj_add_state(roller, LV_STATE_DISABLED);
    } else {
        lv_obj_set_scroll_dir(roller, LV_DIR_VER);
        lv_roller_set_visible_row_count(roller, 5);
        lv_obj_remove_state(roller, LV_STATE_DISABLED);
    }
}

static void invalidate_settings_ui_cache(void)
{
    s_ui.settings_ui.wifi_cache_valid = false;
    s_ui.settings_ui.night_cache_valid = false;
}

static void settings_set_label_text_if_changed(lv_obj_t *label, const char *text)
{
    const char *current_text;

    if (label == NULL || text == NULL) {
        return;
    }

    current_text = lv_label_get_text(label);
    if (current_text == NULL || strcmp(current_text, text) != 0) {
        lv_label_set_text(label, text);
    }
}

static void settings_set_roller_selected_if_changed(lv_obj_t *roller, uint16_t selected)
{
    if (roller == NULL) {
        return;
    }

    if (lv_roller_get_selected(roller) != selected) {
        lv_roller_set_selected(roller, selected, LV_ANIM_OFF);
    }
}

static void settings_format_timezone_label(char *buffer, size_t size, int tz)
{
    snprintf(buffer, size, "UTC%+d", tz);
}

static void settings_format_face_label(char *buffer, size_t size, clock_face_id_t face)
{
    snprintf(buffer, size, "%s", clock_face_name(face));
}

static void settings_set_switch_checked_if_changed(lv_obj_t *sw, bool checked)
{
    bool current_checked;

    if (sw == NULL) {
        return;
    }

    current_checked = lv_obj_has_state(sw, LV_STATE_CHECKED);
    if (current_checked == checked) {
        return;
    }

    if (checked) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(sw, LV_STATE_CHECKED);
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

    lv_obj_set_width(button, lv_pct(100));
    lv_obj_set_height(button, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(button, 22, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x232323), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x343434), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_pad_hor(button, 22, 0);
    lv_obj_set_style_pad_ver(button, 18, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_width(title, lv_pct(100));
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_label_set_text_fmt(title, "%s  %s", ssid, meta);
    lv_obj_center(title);

    lv_obj_add_event_cb(button, wifi_network_btn_event_cb, LV_EVENT_CLICKED, ctx);
    return button;
}

static void clear_wifi_list(void)
{
    if (s_ui.settings_ui.wifi_network_list == NULL) {
        return;
    }

    lv_obj_clean(s_ui.settings_ui.wifi_network_list);
}

static void sync_wifi_list(void)
{
    wifi_scan_result_t results[WIFI_TIME_MAX_SCAN_RESULTS];
    uint32_t generation = 0;
    size_t count;
    size_t visible_count;

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

    visible_count = LV_MIN(count, WIFI_UI_MAX_VISIBLE_NETWORKS);
    for (size_t i = 0; i < visible_count; ++i) {
        char meta[64];
        const char *ssid = results[i].ssid[0] ? results[i].ssid : "<hidden>";

        s_ui.settings_ui.network_ctx[i].network_index = i;
        snprintf(meta, sizeof(meta), "%ddBm", results[i].rssi);
        create_network_button(s_ui.settings_ui.wifi_network_list, ssid, meta, &s_ui.settings_ui.network_ctx[i]);
    }

    if (count > visible_count) {
        lv_obj_t *label = lv_label_create(s_ui.settings_ui.wifi_network_list);

        style_centered_label(label, NULL, lv_color_hex(0x8F8F8F));
        lv_label_set_text_fmt(label, "Showing %u of %u networks",
                              (unsigned)visible_count,
                              (unsigned)count);
    }
}

static void sync_night_controls(void)
{
    char face_label[64];
    char brightness_label[32];
    uint8_t brightness_ui;

    if (s_ui.settings_ui.night_enabled_sw == NULL) {
        return;
    }

    s_ui.suppress_events = true;
    settings_set_switch_checked_if_changed(s_ui.settings_ui.night_enabled_sw, s_ui.settings->night_mode.enabled);
    settings_set_roller_selected_if_changed(s_ui.settings_ui.night_start_hour_dd, s_ui.settings->night_mode.start_hour);
    settings_set_roller_selected_if_changed(s_ui.settings_ui.night_start_min_dd, s_ui.settings->night_mode.start_minute);
    settings_set_roller_selected_if_changed(s_ui.settings_ui.night_end_hour_dd, s_ui.settings->night_mode.end_hour);
    settings_set_roller_selected_if_changed(s_ui.settings_ui.night_end_min_dd, s_ui.settings->night_mode.end_minute);
    s_ui.suppress_events = false;

    sync_night_schedule_lock_state();

    s_ui.settings->night_mode.face = sanitize_enabled_face(s_ui.settings->night_mode.face);
    settings_format_face_label(face_label, sizeof(face_label), s_ui.settings->night_mode.face);
    settings_set_label_text_if_changed(s_ui.settings_ui.night_face_dd, face_label);
    sync_night_face_preview();
    sync_night_face_picker_selection();

    brightness_ui = brightness_hw_to_ui(s_ui.settings->night_mode.brightness);
    snprintf(brightness_label, sizeof(brightness_label), "%u%%", brightness_ui);
    settings_set_label_text_if_changed(s_ui.settings_ui.night_brightness_dd, brightness_label);
    if (s_ui.settings_ui.night_brightness_slider != NULL) {
        s_ui.suppress_events = true;
        lv_slider_set_value(s_ui.settings_ui.night_brightness_slider, brightness_ui, LV_ANIM_OFF);
        s_ui.suppress_events = false;
    }

    s_ui.settings_ui.cached_night_enabled = s_ui.settings->night_mode.enabled;
    s_ui.settings_ui.cached_in_night_mode = s_ui.runtime->in_night_mode;
    s_ui.settings_ui.cached_night_start_hour = s_ui.settings->night_mode.start_hour;
    s_ui.settings_ui.cached_night_start_minute = s_ui.settings->night_mode.start_minute;
    s_ui.settings_ui.cached_night_end_hour = s_ui.settings->night_mode.end_hour;
    s_ui.settings_ui.cached_night_end_minute = s_ui.settings->night_mode.end_minute;
    s_ui.settings_ui.cached_night_brightness = s_ui.settings->night_mode.brightness;
    s_ui.settings_ui.cached_night_face = s_ui.settings->night_mode.face;
    s_ui.settings_ui.night_cache_valid = true;
}

static void sync_wifi_controls(void)
{
    char saved[160];
    char timezone_label[24];

    if (s_ui.settings_ui.wifi_timezone_dd == NULL) {
        return;
    }

    settings_format_timezone_label(timezone_label, sizeof(timezone_label), s_ui.settings->wifi.timezone_offset_hours);
    settings_set_label_text_if_changed(s_ui.settings_ui.wifi_timezone_dd, timezone_label);

    settings_set_label_text_if_changed(s_ui.settings_ui.wifi_status_label,
                                       s_ui.runtime->wifi_status[0] ? s_ui.runtime->wifi_status : "Wi-Fi idle");

    if (s_ui.settings->wifi.ssid[0] != '\0') {
        snprintf(saved, sizeof(saved), "Saved: %s", s_ui.settings->wifi.ssid);
    } else {
        snprintf(saved, sizeof(saved), "Saved: none");
    }
    settings_set_label_text_if_changed(s_ui.settings_ui.wifi_saved_label, saved);

    sync_wifi_list();

    s_ui.settings_ui.cached_timezone_offset_hours = s_ui.settings->wifi.timezone_offset_hours;
    snprintf(s_ui.settings_ui.cached_wifi_status, sizeof(s_ui.settings_ui.cached_wifi_status), "%s",
             s_ui.runtime->wifi_status);
    snprintf(s_ui.settings_ui.cached_wifi_saved_ssid, sizeof(s_ui.settings_ui.cached_wifi_saved_ssid), "%s",
             s_ui.settings->wifi.ssid);
    s_ui.settings_ui.cached_wifi_scan_generation = wifi_time_get_scan_generation();
    s_ui.settings_ui.wifi_cache_valid = true;
}

static void refresh_settings_controls(void)
{
    sync_wifi_controls();
    sync_night_controls();
}

static bool wifi_controls_need_sync(void)
{
    if (!s_ui.settings_ui.wifi_cache_valid) {
        return true;
    }

    if (s_ui.settings_ui.cached_timezone_offset_hours != s_ui.settings->wifi.timezone_offset_hours) {
        return true;
    }

    if (strcmp(s_ui.settings_ui.cached_wifi_status, s_ui.runtime->wifi_status) != 0) {
        return true;
    }

    if (strcmp(s_ui.settings_ui.cached_wifi_saved_ssid, s_ui.settings->wifi.ssid) != 0) {
        return true;
    }

    if (s_ui.settings_ui.cached_wifi_scan_generation != wifi_time_get_scan_generation()) {
        return true;
    }

    return false;
}

static bool night_controls_need_sync(void)
{
    if (!s_ui.settings_ui.night_cache_valid) {
        return true;
    }

    if (s_ui.settings_ui.cached_night_enabled != s_ui.settings->night_mode.enabled ||
        s_ui.settings_ui.cached_in_night_mode != s_ui.runtime->in_night_mode ||
        s_ui.settings_ui.cached_night_start_hour != s_ui.settings->night_mode.start_hour ||
        s_ui.settings_ui.cached_night_start_minute != s_ui.settings->night_mode.start_minute ||
        s_ui.settings_ui.cached_night_end_hour != s_ui.settings->night_mode.end_hour ||
        s_ui.settings_ui.cached_night_end_minute != s_ui.settings->night_mode.end_minute ||
        s_ui.settings_ui.cached_night_brightness != s_ui.settings->night_mode.brightness ||
        s_ui.settings_ui.cached_night_face != sanitize_enabled_face(s_ui.settings->night_mode.face)) {
        return true;
    }

    return false;
}

static const void *settings_get_face_preview_src(clock_face_id_t face)
{
    switch (face) {
    case CLOCK_FACE_DIGITAL:
        refresh_digital_face_snapshot();
        return s_ui.faces.digital_snapshot_buf;
    case CLOCK_FACE_MATRIX:
        if (s_ui.faces.matrix_face_obj != NULL) {
            return lv_canvas_get_image(s_ui.faces.matrix_face_obj);
        }
        return NULL;
    case CLOCK_FACE_WHARTON:
        if (s_ui.faces.wharton_face_obj != NULL) {
            return lv_canvas_get_image(s_ui.faces.wharton_face_obj);
        }
        return NULL;
    case CLOCK_FACE_SLAVA:
        return &slava_face_img;
    case CLOCK_FACE_SLAVA_DARK:
        return &slava_dark_face_img;
    case CLOCK_FACE_STERNGLAS:
        return (s_ui.faces.sternglas_composite_buf != NULL) ? s_ui.faces.sternglas_composite_buf
                                                            : s_ui.faces.sternglas_snapshot_buf;
    case CLOCK_FACE_AVENIR:
        return (s_ui.faces.avenir_composite_buf != NULL) ? s_ui.faces.avenir_composite_buf
                                                         : s_ui.faces.avenir_snapshot_buf;
    case CLOCK_FACE_MODERN_SILVER:
        return (s_ui.faces.modern_silver_composite_buf != NULL) ? s_ui.faces.modern_silver_composite_buf
                                                                : s_ui.faces.modern_silver_snapshot_buf;
    default:
        return NULL;
    }
}

static void settings_set_face_preview_src(lv_obj_t *image, clock_face_id_t face)
{
    const void *src;

    if (image == NULL) {
        return;
    }

    src = settings_get_face_preview_src(face);

    if (src == NULL) {
        return;
    }

    if (lv_image_get_src(image) != src) {
        lv_image_set_src(image, src);
    }
}

static void sync_night_schedule_lock_state(void)
{
    settings_set_roller_locked(s_ui.settings_ui.night_start_hour_dd, s_ui.settings_ui.night_schedule_locked);
    settings_set_roller_locked(s_ui.settings_ui.night_start_min_dd, s_ui.settings_ui.night_schedule_locked);
    settings_set_roller_locked(s_ui.settings_ui.night_end_hour_dd, s_ui.settings_ui.night_schedule_locked);
    settings_set_roller_locked(s_ui.settings_ui.night_end_min_dd, s_ui.settings_ui.night_schedule_locked);

    if (s_ui.settings_ui.night_start_hour_cover != NULL) {
        if (s_ui.settings_ui.night_schedule_locked) {
            lv_obj_clear_flag(s_ui.settings_ui.night_start_hour_cover, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s_ui.settings_ui.night_start_min_cover, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s_ui.settings_ui.night_end_hour_cover, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s_ui.settings_ui.night_end_min_cover, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_ui.settings_ui.night_start_hour_cover, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_ui.settings_ui.night_start_min_cover, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_ui.settings_ui.night_end_hour_cover, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_ui.settings_ui.night_end_min_cover, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void night_schedule_lock_if_editing(void)
{
    if (!s_ui.settings_ui.night_schedule_locked) {
        s_ui.settings_ui.night_schedule_locked = true;
        sync_night_controls();
    }
}

static void sync_night_face_preview(void)
{
    if (s_ui.settings_ui.night_face_preview == NULL) {
        return;
    }

    settings_set_face_preview_src(s_ui.settings_ui.night_face_preview, s_ui.settings->night_mode.face);
}

static void sync_night_face_picker_previews(void)
{
    int visible_count = clock_face_visible_count();

    for (int index = 0; index < visible_count; ++index) {
        clock_face_id_t face = clock_face_visible_index_to_id(index);
        lv_obj_t *preview = s_ui.settings_ui.night_face_picker_preview[face];

        if (preview != NULL) {
            settings_set_face_preview_src(preview, face);
        }
    }
}

static void sync_night_face_picker_selection(void)
{
    int visible_count = clock_face_visible_count();

    for (int index = 0; index < visible_count; ++index) {
        clock_face_id_t face = clock_face_visible_index_to_id(index);
        lv_obj_t *card = s_ui.settings_ui.night_face_picker_card[face];
        if (card == NULL) {
            continue;
        }

        if (face == s_ui.settings->night_mode.face) {
            lv_obj_set_style_border_width(card, 3, 0);
            lv_obj_set_style_border_color(card, lv_color_hex(UI_ACCENT_COL), 0);
            lv_obj_set_style_bg_color(card, lv_color_hex(0x1F2428), 0);
        } else {
            lv_obj_set_style_border_width(card, 1, 0);
            lv_obj_set_style_border_color(card, lv_color_hex(0x2C2C2C), 0);
            lv_obj_set_style_bg_color(card, lv_color_hex(0x171717), 0);
        }
    }
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

    close_wifi_dialog();
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

static void close_wifi_dialog(void)
{
    s_ui.settings_ui.pending_ssid[0] = '\0';
    if (s_ui.settings_ui.wifi_keyboard != NULL) {
        lv_keyboard_set_textarea(s_ui.settings_ui.wifi_keyboard, NULL);
        lv_obj_add_flag(s_ui.settings_ui.wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_ui.settings_ui.wifi_dialog_overlay != NULL) {
        lv_obj_add_flag(s_ui.settings_ui.wifi_dialog_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

static lv_obj_t *create_settings_menu_entry(lv_obj_t *parent,
                                            const char *symbol,
                                            const char *title_text,
                                            const char *subtitle_text,
                                            lv_event_cb_t cb)
{
    lv_obj_t *entry = lv_obj_create(parent);
    lv_obj_t *row;
    lv_obj_t *left_cluster;
    lv_obj_t *icon;
    lv_obj_t *text_col;
    lv_obj_t *label;

    lv_obj_set_width(entry, 540);
    lv_obj_set_height(entry, LV_SIZE_CONTENT);
    lv_obj_add_flag(entry, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(entry, 30, 0);
    lv_obj_set_style_bg_color(entry, lv_color_hex(0x171717), 0);
    lv_obj_set_style_bg_color(entry, lv_color_hex(0x232323), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(entry, 0, 0);
    lv_obj_set_style_pad_all(entry, 22, 0);
    lv_obj_set_style_pad_row(entry, 8, 0);
    lv_obj_set_style_shadow_width(entry, 0, 0);
    lv_obj_set_layout(entry, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(entry, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(entry, LV_OBJ_FLAG_SCROLLABLE);
    if (cb != NULL) {
        lv_obj_add_event_cb(entry, cb, LV_EVENT_CLICKED, NULL);
    }

    row = create_row(entry);
    lv_obj_add_flag(row, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    left_cluster = create_row(row);
    lv_obj_add_flag(left_cluster, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_width(left_cluster, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(left_cluster, 14, 0);

    icon = lv_obj_create(left_cluster);
    lv_obj_add_flag(icon, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_size(icon, 58, 58);
    lv_obj_set_style_radius(icon, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(icon, lv_color_hex(UI_ACCENT_COL), 0);
    lv_obj_set_style_border_width(icon, 0, 0);
    lv_obj_set_style_pad_all(icon, 0, 0);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_SCROLLABLE);
    label = lv_label_create(icon);
    lv_obj_add_flag(label, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(UI_ACCENT_TEXT_COL), 0);
    lv_label_set_text(label, symbol);
    lv_obj_center(label);

    text_col = lv_obj_create(left_cluster);
    lv_obj_add_flag(text_col, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_width(text_col, LV_SIZE_CONTENT);
    lv_obj_set_height(text_col, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(text_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(text_col, 0, 0);
    lv_obj_set_style_pad_all(text_col, 0, 0);
    lv_obj_set_style_pad_row(text_col, 4, 0);
    lv_obj_set_layout(text_col, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(text_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(text_col, LV_OBJ_FLAG_SCROLLABLE);

    label = lv_label_create(text_col);
    lv_obj_add_flag(label, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, title_text);

    label = lv_label_create(text_col);
    lv_obj_add_flag(label, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_style_text_color(label, lv_color_hex(0xAFAFAF), 0);
    lv_label_set_text(label, subtitle_text);

    label = lv_label_create(row);
    lv_obj_add_flag(label, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x8F8F8F), 0);
    lv_label_set_text(label, LV_SYMBOL_RIGHT);

    return entry;
}

static void settings_show_root(void)
{
    s_ui.settings_ui.wifi_open = false;
    s_ui.settings_ui.night_open = false;
    s_ui.settings_ui.other_open = false;
    s_ui.settings_ui.night_face_picker_open = false;
    s_ui.settings_ui.close_swipe_consumed = false;
    s_ui.settings_ui.wifi_scrolling = false;
    s_ui.settings_ui.night_scrolling = false;
    invalidate_settings_ui_cache();
    close_wifi_dialog();
    close_night_face_picker();
    clear_wifi_list();
    if (s_ui.settings_ui.wifi_overlay != NULL) {
        lv_obj_add_flag(s_ui.settings_ui.wifi_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_ui.settings_ui.other_overlay != NULL) {
        lv_obj_add_flag(s_ui.settings_ui.other_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_ui.settings_ui.night_overlay != NULL) {
        lv_obj_add_flag(s_ui.settings_ui.night_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_ui.settings_ui.overlay != NULL) {
        lv_obj_clear_flag(s_ui.settings_ui.overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_ui.settings_ui.overlay);
    }
}

static void settings_open_wifi_detail(void)
{
    if (s_ui.settings_ui.wifi_overlay == NULL) {
        return;
    }

    s_ui.settings_ui.wifi_open = true;
    s_ui.settings_ui.night_open = false;
    s_ui.settings_ui.other_open = false;
    s_ui.settings_ui.close_swipe_consumed = false;
    s_ui.settings_ui.wifi_cache_valid = false;
    lv_obj_add_flag(s_ui.settings_ui.overlay, LV_OBJ_FLAG_HIDDEN);
    if (s_ui.settings_ui.other_overlay != NULL) {
        lv_obj_add_flag(s_ui.settings_ui.other_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_ui.settings_ui.night_overlay != NULL) {
        lv_obj_add_flag(s_ui.settings_ui.night_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    sync_wifi_controls();
    lv_obj_clear_flag(s_ui.settings_ui.wifi_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_ui.settings_ui.wifi_overlay);
}

static void settings_open_night_detail(void)
{
    if (s_ui.settings_ui.night_overlay == NULL) {
        return;
    }

    s_ui.settings_ui.night_open = true;
    s_ui.settings_ui.wifi_open = false;
    s_ui.settings_ui.other_open = false;
    s_ui.settings_ui.night_schedule_locked = true;
    s_ui.settings_ui.close_swipe_consumed = false;
    s_ui.settings_ui.night_cache_valid = false;
    lv_obj_add_flag(s_ui.settings_ui.overlay, LV_OBJ_FLAG_HIDDEN);
    if (s_ui.settings_ui.other_overlay != NULL) {
        lv_obj_add_flag(s_ui.settings_ui.other_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_ui.settings_ui.wifi_overlay != NULL) {
        lv_obj_add_flag(s_ui.settings_ui.wifi_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    close_night_face_picker();
    sync_night_controls();
    lv_obj_clear_flag(s_ui.settings_ui.night_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_ui.settings_ui.night_overlay);
}

static void settings_open_other_detail(void)
{
    if (s_ui.settings_ui.other_overlay == NULL) {
        return;
    }

    s_ui.settings_ui.other_open = true;
    s_ui.settings_ui.wifi_open = false;
    s_ui.settings_ui.night_open = false;
    s_ui.settings_ui.close_swipe_consumed = false;
    s_ui.settings_ui.wifi_cache_valid = false;
    lv_obj_add_flag(s_ui.settings_ui.overlay, LV_OBJ_FLAG_HIDDEN);
    if (s_ui.settings_ui.wifi_overlay != NULL) {
        lv_obj_add_flag(s_ui.settings_ui.wifi_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_ui.settings_ui.night_overlay != NULL) {
        lv_obj_add_flag(s_ui.settings_ui.night_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    sync_wifi_controls();
    lv_obj_clear_flag(s_ui.settings_ui.other_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_ui.settings_ui.other_overlay);
}

static void settings_wifi_entry_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    settings_open_wifi_detail();
}

static void settings_night_entry_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    settings_open_night_detail();
}

static void settings_other_entry_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    settings_open_other_detail();
}

static void settings_back_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    if (s_ui.settings_ui.night_face_picker_open) {
        close_night_face_picker();
        return;
    }
    settings_show_root();
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
    s_ui.settings_ui.close_dragging = false;
    s_ui.settings_ui.wifi_scrolling = false;
    s_ui.settings_ui.night_scrolling = false;
    set_root_ui_hidden(true);

    settings_show_root();
    if (tab_idx == SETTINGS_TAB_NIGHT) {
        settings_open_night_detail();
    }
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
    s_ui.settings_ui.wifi_open = false;
    s_ui.settings_ui.night_open = false;
    s_ui.settings_ui.other_open = false;
    s_ui.settings_ui.night_face_picker_open = false;
    s_ui.settings_ui.wifi_scrolling = false;
    s_ui.settings_ui.night_scrolling = false;
    s_ui.settings_ui.close_dragging = false;
    s_ui.settings_ui.close_swipe_consumed = false;
    invalidate_settings_ui_cache();
    close_wifi_dialog();
    close_night_face_picker();
    clear_wifi_list();
    if (s_ui.settings_ui.wifi_overlay != NULL) {
        lv_obj_add_flag(s_ui.settings_ui.wifi_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_ui.settings_ui.other_overlay != NULL) {
        lv_obj_add_flag(s_ui.settings_ui.other_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_ui.settings_ui.night_overlay != NULL) {
        lv_obj_add_flag(s_ui.settings_ui.night_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_flag(s_ui.settings_ui.overlay, LV_OBJ_FLAG_HIDDEN);
    set_root_ui_hidden(false);
    show_affordances_temporarily();
}

static void settings_overlay_event_cb(lv_event_t *event)
{
    if (lv_event_get_target(event) == lv_event_get_current_target(event)) {
        settings_close_event_cb(event);
    }
}

static void settings_detail_overlay_event_cb(lv_event_t *event)
{
    if (lv_event_get_target(event) == lv_event_get_current_target(event)) {
        settings_back_event_cb(event);
    }
}

static void settings_content_scroll_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t *target = lv_event_get_target(event);
    bool scrolling = (code == LV_EVENT_SCROLL_BEGIN || code == LV_EVENT_SCROLL);

    if (target == s_ui.settings_ui.wifi_content) {
        s_ui.settings_ui.wifi_scrolling = scrolling;
    } else if (target == s_ui.settings_ui.night_content) {
        s_ui.settings_ui.night_scrolling = scrolling;
    }

    if (code == LV_EVENT_SCROLL_END) {
        if (target == s_ui.settings_ui.wifi_content) {
            s_ui.settings_ui.wifi_scrolling = false;
        } else if (target == s_ui.settings_ui.night_content) {
            s_ui.settings_ui.night_scrolling = false;
        }
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

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        s_ui.settings_ui.close_dragging = false;
        s_ui.settings_ui.close_swipe_consumed = false;
        return;
    }

    if (s_ui.settings_ui.close_swipe_consumed) {
        return;
    }

    lv_indev_get_point(indev, &point);

    if (code == LV_EVENT_PRESSED) {
        s_ui.settings_ui.close_dragging = true;
        s_ui.settings_ui.close_drag_start_point = point;
        s_ui.settings_ui.close_swipe_consumed = false;
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
            s_ui.settings_ui.close_dragging = false;
            s_ui.settings_ui.close_swipe_consumed = true;
            if (s_ui.settings_ui.wifi_open || s_ui.settings_ui.night_open || s_ui.settings_ui.other_open) {
                settings_back_event_cb(event);
            } else {
                settings_close_event_cb(event);
            }
        }
        return;
    }
}

static void timezone_prev_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    if (s_ui.settings->wifi.timezone_offset_hours <= -12) {
        s_ui.settings->wifi.timezone_offset_hours = 14;
    } else {
        --s_ui.settings->wifi.timezone_offset_hours;
    }
    notify_settings_changed();
    sync_wifi_controls();
}

static void timezone_next_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    if (s_ui.settings->wifi.timezone_offset_hours >= 14) {
        s_ui.settings->wifi.timezone_offset_hours = -12;
    } else {
        ++s_ui.settings->wifi.timezone_offset_hours;
    }
    notify_settings_changed();
    sync_wifi_controls();
}

static void night_enabled_event_cb(lv_event_t *event)
{
    if (s_ui.suppress_events) {
        return;
    }

    night_schedule_lock_if_editing();
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
        s_ui.settings->night_mode.start_hour = lv_roller_get_selected(target);
    } else if (target == s_ui.settings_ui.night_start_min_dd) {
        s_ui.settings->night_mode.start_minute = lv_roller_get_selected(target);
    } else if (target == s_ui.settings_ui.night_end_hour_dd) {
        s_ui.settings->night_mode.end_hour = lv_roller_get_selected(target);
    } else if (target == s_ui.settings_ui.night_end_min_dd) {
        s_ui.settings->night_mode.end_minute = lv_roller_get_selected(target);
    }

    notify_settings_changed();
    sync_night_controls();
}

static void night_schedule_roller_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_CLICKED) {
        if (s_ui.settings_ui.night_schedule_locked) {
            s_ui.settings_ui.night_schedule_locked = false;
            sync_night_controls();
        }
        return;
    }

    if (code == LV_EVENT_SCROLL_END && !s_ui.settings_ui.night_schedule_locked) {
        s_ui.settings_ui.night_schedule_locked = true;
        sync_night_controls();
    }
}

static void night_schedule_activate_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);

    if (s_ui.settings_ui.night_schedule_locked) {
        s_ui.settings_ui.night_schedule_locked = false;
        sync_night_controls();
    }
}

static void night_schedule_background_event_cb(lv_event_t *event)
{
    if (lv_event_get_target(event) == lv_event_get_current_target(event) &&
        !s_ui.settings_ui.night_schedule_locked) {
        night_schedule_lock_if_editing();
    }
}

static void night_schedule_dismiss_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    night_schedule_lock_if_editing();
}

static void night_brightness_slider_event_cb(lv_event_t *event)
{
    char brightness_label[32];
    int ui_value;

    if (s_ui.suppress_events) {
        return;
    }

    ui_value = lv_slider_get_value(lv_event_get_target(event));
    s_ui.settings->night_mode.brightness = brightness_ui_to_hw(ui_value);
    if (s_ui.runtime != NULL &&
        s_ui.runtime->night_brightness_override_active &&
        s_ui.runtime->night_brightness_override == s_ui.settings->night_mode.brightness) {
        s_ui.runtime->night_brightness_override_active = false;
    }
    snprintf(brightness_label, sizeof(brightness_label), "%d%%", ui_value);
    settings_set_label_text_if_changed(s_ui.settings_ui.night_brightness_dd, brightness_label);
    s_ui.settings_ui.cached_night_brightness = s_ui.settings->night_mode.brightness;
    notify_settings_changed();
}

static void open_night_face_picker(void)
{
    if (s_ui.settings_ui.night_face_picker_overlay == NULL) {
        return;
    }

    sync_night_face_preview();
    sync_night_face_picker_previews();
    sync_night_face_picker_selection();
    s_ui.settings_ui.night_face_picker_open = true;
    lv_obj_clear_flag(s_ui.settings_ui.night_face_picker_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_ui.settings_ui.night_face_picker_overlay);
}

static void close_night_face_picker(void)
{
    s_ui.settings_ui.night_face_picker_open = false;
    if (s_ui.settings_ui.night_face_picker_overlay != NULL) {
        lv_obj_add_flag(s_ui.settings_ui.night_face_picker_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

static void night_face_button_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    night_schedule_lock_if_editing();
    open_night_face_picker();
}

static void night_face_picker_item_event_cb(lv_event_t *event)
{
    face_ctx_t *ctx = (face_ctx_t *)lv_event_get_user_data(event);

    if (ctx == NULL || !clock_face_is_enabled(ctx->face)) {
        return;
    }

    s_ui.settings->night_mode.face = ctx->face;
    notify_settings_changed();
    sync_night_controls();
    close_night_face_picker();
}

static void night_face_picker_overlay_event_cb(lv_event_t *event)
{
    if (lv_event_get_target(event) == lv_event_get_current_target(event)) {
        close_night_face_picker();
    }
}

static void night_face_picker_close_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    close_night_face_picker();
}

static void create_wifi_card(lv_obj_t *parent)
{
    lv_obj_t *row;

    s_ui.settings_ui.wifi_card = create_card(parent);
    center_card_children(s_ui.settings_ui.wifi_card);

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

static void create_timezone_card(lv_obj_t *parent)
{
    lv_obj_t *row;

    s_ui.settings_ui.timezone_card = create_card(parent);
    center_card_children(s_ui.settings_ui.timezone_card);
    create_centered_card_title(s_ui.settings_ui.timezone_card, "Time zone");

    row = create_step_selector(s_ui.settings_ui.timezone_card,
                               240,
                               78,
                               &s_ui.settings_ui.wifi_timezone_dd,
                               timezone_prev_event_cb,
                               NULL,
                               timezone_next_event_cb,
                               NULL);
    center_row(row);
}

static void create_networks_card(lv_obj_t *parent)
{
    s_ui.settings_ui.networks_card = create_card(parent);
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

static void create_night_card(lv_obj_t *parent)
{
    static const char *range_arrow = "->";
    lv_obj_t *row;
    lv_obj_t *label;
    lv_obj_t *card;
    lv_obj_t *preview_wrap;

    s_ui.settings_ui.night_card = create_card(parent);
    lv_obj_set_width(s_ui.settings_ui.night_card, 540);
    lv_obj_set_style_pad_all(s_ui.settings_ui.night_card, 24, 0);
    lv_obj_set_style_pad_row(s_ui.settings_ui.night_card, 14, 0);
    lv_obj_add_event_cb(s_ui.settings_ui.night_card, night_schedule_dismiss_event_cb, LV_EVENT_CLICKED, NULL);

    row = create_row(s_ui.settings_ui.night_card);
    center_row(row);
    label = lv_label_create(row);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, "Enabled");
    s_ui.settings_ui.night_enabled_sw = lv_switch_create(row);
    lv_obj_set_style_bg_color(s_ui.settings_ui.night_enabled_sw, lv_color_hex(0x313131), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_ui.settings_ui.night_enabled_sw, lv_color_hex(UI_ACCENT_COL), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_event_cb(s_ui.settings_ui.night_enabled_sw, night_enabled_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    card = create_card(parent);
    lv_obj_set_width(card, 540);
    lv_obj_set_style_pad_all(card, 24, 0);
    lv_obj_set_style_pad_row(card, 14, 0);
    lv_obj_add_event_cb(card, night_schedule_background_event_cb, LV_EVENT_CLICKED, NULL);

    label = lv_label_create(card);
    style_centered_label(label, &lv_font_montserrat_24, lv_color_white());
    lv_label_set_text(label, "Schedule");

    row = create_row(card);
    lv_obj_set_width(row, LV_SIZE_CONTENT);
    center_row(row);
    lv_obj_set_style_pad_column(row, 14, 0);
    label = lv_label_create(row);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, "Start");
    s_ui.settings_ui.night_start_hour_dd = create_time_roller(row, s_ui.hour_options, 164, night_hour_minute_event_cb, NULL);
    s_ui.settings_ui.night_start_min_dd = create_time_roller(row, s_ui.minute_options, 164, night_hour_minute_event_cb, NULL);
    lv_obj_add_event_cb(s_ui.settings_ui.night_start_hour_dd, night_schedule_roller_event_cb, LV_EVENT_SCROLL_END, NULL);
    lv_obj_add_event_cb(s_ui.settings_ui.night_start_min_dd, night_schedule_roller_event_cb, LV_EVENT_SCROLL_END, NULL);
    s_ui.settings_ui.night_start_hour_cover = lv_button_create(row);
    lv_obj_set_size(s_ui.settings_ui.night_start_hour_cover, 164, 236);
    lv_obj_add_flag(s_ui.settings_ui.night_start_hour_cover, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_style_bg_opa(s_ui.settings_ui.night_start_hour_cover, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ui.settings_ui.night_start_hour_cover, 0, 0);
    lv_obj_set_style_shadow_width(s_ui.settings_ui.night_start_hour_cover, 0, 0);
    lv_obj_set_style_pad_all(s_ui.settings_ui.night_start_hour_cover, 0, 0);
    lv_obj_align_to(s_ui.settings_ui.night_start_hour_cover, s_ui.settings_ui.night_start_hour_dd, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(s_ui.settings_ui.night_start_hour_cover, night_schedule_activate_event_cb, LV_EVENT_CLICKED, NULL);
    s_ui.settings_ui.night_start_min_cover = lv_button_create(row);
    lv_obj_set_size(s_ui.settings_ui.night_start_min_cover, 164, 236);
    lv_obj_add_flag(s_ui.settings_ui.night_start_min_cover, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_style_bg_opa(s_ui.settings_ui.night_start_min_cover, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ui.settings_ui.night_start_min_cover, 0, 0);
    lv_obj_set_style_shadow_width(s_ui.settings_ui.night_start_min_cover, 0, 0);
    lv_obj_set_style_pad_all(s_ui.settings_ui.night_start_min_cover, 0, 0);
    lv_obj_align_to(s_ui.settings_ui.night_start_min_cover, s_ui.settings_ui.night_start_min_dd, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(s_ui.settings_ui.night_start_min_cover, night_schedule_activate_event_cb, LV_EVENT_CLICKED, NULL);

    row = create_row(card);
    center_row(row);
    label = lv_label_create(row);
    lv_obj_set_style_text_color(label, lv_color_hex(0x8F8F8F), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_label_set_text(label, range_arrow);

    row = create_row(card);
    lv_obj_set_width(row, LV_SIZE_CONTENT);
    center_row(row);
    lv_obj_set_style_pad_column(row, 14, 0);
    label = lv_label_create(row);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, "End");
    s_ui.settings_ui.night_end_hour_dd = create_time_roller(row, s_ui.hour_options, 164, night_hour_minute_event_cb, NULL);
    s_ui.settings_ui.night_end_min_dd = create_time_roller(row, s_ui.minute_options, 164, night_hour_minute_event_cb, NULL);
    lv_obj_add_event_cb(s_ui.settings_ui.night_end_hour_dd, night_schedule_roller_event_cb, LV_EVENT_SCROLL_END, NULL);
    lv_obj_add_event_cb(s_ui.settings_ui.night_end_min_dd, night_schedule_roller_event_cb, LV_EVENT_SCROLL_END, NULL);
    s_ui.settings_ui.night_end_hour_cover = lv_button_create(row);
    lv_obj_set_size(s_ui.settings_ui.night_end_hour_cover, 164, 236);
    lv_obj_add_flag(s_ui.settings_ui.night_end_hour_cover, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_style_bg_opa(s_ui.settings_ui.night_end_hour_cover, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ui.settings_ui.night_end_hour_cover, 0, 0);
    lv_obj_set_style_shadow_width(s_ui.settings_ui.night_end_hour_cover, 0, 0);
    lv_obj_set_style_pad_all(s_ui.settings_ui.night_end_hour_cover, 0, 0);
    lv_obj_align_to(s_ui.settings_ui.night_end_hour_cover, s_ui.settings_ui.night_end_hour_dd, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(s_ui.settings_ui.night_end_hour_cover, night_schedule_activate_event_cb, LV_EVENT_CLICKED, NULL);
    s_ui.settings_ui.night_end_min_cover = lv_button_create(row);
    lv_obj_set_size(s_ui.settings_ui.night_end_min_cover, 164, 236);
    lv_obj_add_flag(s_ui.settings_ui.night_end_min_cover, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_style_bg_opa(s_ui.settings_ui.night_end_min_cover, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ui.settings_ui.night_end_min_cover, 0, 0);
    lv_obj_set_style_shadow_width(s_ui.settings_ui.night_end_min_cover, 0, 0);
    lv_obj_set_style_pad_all(s_ui.settings_ui.night_end_min_cover, 0, 0);
    lv_obj_align_to(s_ui.settings_ui.night_end_min_cover, s_ui.settings_ui.night_end_min_dd, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(s_ui.settings_ui.night_end_min_cover, night_schedule_activate_event_cb, LV_EVENT_CLICKED, NULL);

    card = create_card(parent);
    lv_obj_set_width(card, 540);
    lv_obj_set_style_pad_all(card, 24, 0);
    lv_obj_set_style_pad_row(card, 16, 0);
    lv_obj_add_event_cb(card, night_schedule_dismiss_event_cb, LV_EVENT_CLICKED, NULL);
    create_section_title(card, "Night face", "Tap to choose from previews");
    s_ui.settings_ui.night_face_button = lv_button_create(card);
    lv_obj_set_width(s_ui.settings_ui.night_face_button, lv_pct(100));
    lv_obj_set_height(s_ui.settings_ui.night_face_button, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(s_ui.settings_ui.night_face_button, 28, 0);
    lv_obj_set_style_bg_color(s_ui.settings_ui.night_face_button, lv_color_hex(0x232323), 0);
    lv_obj_set_style_bg_color(s_ui.settings_ui.night_face_button, lv_color_hex(0x2D2D2D), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(s_ui.settings_ui.night_face_button, 0, 0);
    lv_obj_set_style_pad_all(s_ui.settings_ui.night_face_button, 18, 0);
    lv_obj_set_style_shadow_width(s_ui.settings_ui.night_face_button, 0, 0);
    lv_obj_add_event_cb(s_ui.settings_ui.night_face_button, night_face_button_event_cb, LV_EVENT_CLICKED, NULL);

    preview_wrap = lv_obj_create(s_ui.settings_ui.night_face_button);
    lv_obj_add_flag(preview_wrap, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_width(preview_wrap, lv_pct(100));
    lv_obj_set_height(preview_wrap, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(preview_wrap, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(preview_wrap, 0, 0);
    lv_obj_set_style_pad_all(preview_wrap, 0, 0);
    lv_obj_set_style_pad_row(preview_wrap, 12, 0);
    lv_obj_set_layout(preview_wrap, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(preview_wrap, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(preview_wrap, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(preview_wrap, LV_OBJ_FLAG_SCROLLABLE);

    row = lv_obj_create(preview_wrap);
    lv_obj_add_flag(row, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_size(row, NIGHT_FACE_PREVIEW_SIZE, NIGHT_FACE_PREVIEW_SIZE);
    lv_obj_set_style_radius(row, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x0F0F0F), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, lv_color_hex(0x323232), 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_clip_corner(row, true, 0);
    lv_obj_set_style_shadow_width(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(row, LV_ALIGN_CENTER, 0, 0);

    s_ui.settings_ui.night_face_preview = lv_image_create(row);
    lv_obj_add_flag(s_ui.settings_ui.night_face_preview, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_size(s_ui.settings_ui.night_face_preview, NIGHT_FACE_PREVIEW_SIZE, NIGHT_FACE_PREVIEW_SIZE);
    lv_obj_set_style_bg_opa(s_ui.settings_ui.night_face_preview, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ui.settings_ui.night_face_preview, 0, 0);
    lv_obj_set_style_pad_all(s_ui.settings_ui.night_face_preview, 0, 0);
    lv_obj_clear_flag(s_ui.settings_ui.night_face_preview, LV_OBJ_FLAG_SCROLLABLE);
    lv_image_set_inner_align(s_ui.settings_ui.night_face_preview, LV_IMAGE_ALIGN_COVER);
    lv_obj_center(s_ui.settings_ui.night_face_preview);

    s_ui.settings_ui.night_face_dd = lv_label_create(preview_wrap);
    lv_obj_add_flag(s_ui.settings_ui.night_face_dd, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_width(s_ui.settings_ui.night_face_dd, lv_pct(100));
    lv_obj_set_style_text_font(s_ui.settings_ui.night_face_dd, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_ui.settings_ui.night_face_dd, lv_color_white(), 0);
    lv_obj_set_style_text_align(s_ui.settings_ui.night_face_dd, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_ui.settings_ui.night_face_dd, "");

    card = create_card(parent);
    lv_obj_set_width(card, 540);
    lv_obj_set_style_pad_all(card, 24, 0);
    lv_obj_set_style_pad_row(card, 14, 0);
    lv_obj_add_event_cb(card, night_schedule_dismiss_event_cb, LV_EVENT_CLICKED, NULL);
    create_section_title(card, "Brightness", "Matches the main slider interaction");

    row = create_row(card);
    center_row(row);
    s_ui.settings_ui.night_brightness_dd = lv_label_create(row);
    lv_obj_set_style_text_font(s_ui.settings_ui.night_brightness_dd, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(s_ui.settings_ui.night_brightness_dd, lv_color_white(), 0);
    lv_label_set_text(s_ui.settings_ui.night_brightness_dd, "0%");

    s_ui.settings_ui.night_brightness_slider = lv_slider_create(card);
    lv_slider_set_range(s_ui.settings_ui.night_brightness_slider, 0, 100);
    lv_obj_set_width(s_ui.settings_ui.night_brightness_slider, lv_pct(100));
    style_slider(s_ui.settings_ui.night_brightness_slider);
    lv_obj_set_height(s_ui.settings_ui.night_brightness_slider, 24);
    lv_obj_set_style_bg_color(s_ui.settings_ui.night_brightness_slider, lv_color_hex(0x2D2D2D), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_ui.settings_ui.night_brightness_slider, lv_color_hex(UI_ACCENT_COL), LV_PART_INDICATOR);
    lv_obj_set_style_shadow_width(s_ui.settings_ui.night_brightness_slider, 16, LV_PART_KNOB);
    lv_obj_set_style_shadow_color(s_ui.settings_ui.night_brightness_slider, lv_color_hex(UI_ACCENT_COL), LV_PART_KNOB);
    lv_obj_set_style_shadow_opa(s_ui.settings_ui.night_brightness_slider, LV_OPA_20, LV_PART_KNOB);
    lv_obj_add_event_cb(s_ui.settings_ui.night_brightness_slider, night_brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

static void create_night_face_picker_overlay(void)
{
    ui_surface_t surface;
    lv_obj_t *bottom_sensor;
    lv_obj_t *title;
    int visible_count = clock_face_visible_count();

    ui_surface_create_fullscreen(&surface,
                                 s_ui.screen,
                                 lv_color_black(),
                                 LV_OPA_70,
                                 lv_color_hex(0x0D0D0D),
                                 SETTINGS_HEADER_HEIGHT,
                                 "Choose night face",
                                 night_face_picker_overlay_event_cb,
                                 night_face_picker_close_event_cb);
    s_ui.settings_ui.night_face_picker_overlay = surface.overlay;
    s_ui.settings_ui.night_face_picker_content = surface.content;
    title = surface.title;
    lv_obj_set_width(title, lv_pct(100));
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    lv_obj_set_style_pad_top(surface.content, 8, 0);
    lv_obj_set_style_pad_bottom(surface.content, 24, 0);
    lv_obj_set_style_pad_row(surface.content, 16, 0);
    lv_obj_set_style_pad_column(surface.content, 12, 0);
    lv_obj_set_layout(surface.content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(surface.content, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(surface.content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(surface.content, LV_OBJ_FLAG_SCROLLABLE);

    for (int index = 0; index < visible_count; ++index) {
        clock_face_id_t face = clock_face_visible_index_to_id(index);
        lv_obj_t *card = lv_button_create(surface.content);
        lv_obj_t *preview;
        lv_obj_t *preview_shell;
        lv_obj_t *label;

        s_ui.settings_ui.face_ctx[face].face = face;
        s_ui.settings_ui.night_face_picker_card[face] = card;

        lv_obj_set_size(card, 156, 172);
        lv_obj_set_style_radius(card, 26, 0);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x171717), 0);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x22272B), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0x2C2C2C), 0);
        lv_obj_set_style_pad_all(card, 14, 0);
        lv_obj_set_style_pad_row(card, 10, 0);
        lv_obj_set_style_shadow_width(card, 0, 0);
        lv_obj_set_layout(card, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(card, night_face_picker_item_event_cb, LV_EVENT_CLICKED, &s_ui.settings_ui.face_ctx[face]);

        preview_shell = lv_obj_create(card);
        lv_obj_add_flag(preview_shell, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_size(preview_shell, 96, 96);
        lv_obj_set_style_radius(preview_shell, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(preview_shell, lv_color_hex(0x0F0F0F), 0);
        lv_obj_set_style_bg_opa(preview_shell, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(preview_shell, 1, 0);
        lv_obj_set_style_border_color(preview_shell, lv_color_hex(0x323232), 0);
        lv_obj_set_style_pad_all(preview_shell, 0, 0);
        lv_obj_set_style_clip_corner(preview_shell, true, 0);
        lv_obj_set_style_shadow_width(preview_shell, 0, 0);
        lv_obj_clear_flag(preview_shell, LV_OBJ_FLAG_SCROLLABLE);

        preview = lv_image_create(preview_shell);
        s_ui.settings_ui.night_face_picker_preview[face] = preview;
        lv_obj_add_flag(preview, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_size(preview, 96, 96);
        lv_obj_set_style_bg_opa(preview, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(preview, 0, 0);
        lv_obj_set_style_pad_all(preview, 0, 0);
        lv_obj_clear_flag(preview, LV_OBJ_FLAG_SCROLLABLE);
        lv_image_set_inner_align(preview, LV_IMAGE_ALIGN_COVER);
        lv_obj_center(preview);

        label = lv_label_create(card);
        s_ui.settings_ui.night_face_picker_label[face] = label;
        lv_obj_add_flag(label, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_width(label, lv_pct(100));
        lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_label_set_text(label, clock_face_name(face));
    }

    ui_surface_create_edge_sensor(s_ui.settings_ui.night_face_picker_overlay,
                                  &bottom_sensor,
                                  UI_SURFACE_EDGE_BOTTOM,
                                  ALARM_CLOSE_BOTTOM_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  (void *)UI_SURFACE_EDGE_BOTTOM);
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

static void create_wifi_overlay(void)
{
    ui_surface_t surface;
    lv_obj_t *title;

    ui_surface_create_fullscreen(&surface,
                                 s_ui.screen,
                                 lv_color_black(),
                                 LV_OPA_COVER,
                                 lv_color_hex(0x0D0D0D),
                                 SETTINGS_HEADER_HEIGHT,
                                 "Wi-Fi",
                                 settings_detail_overlay_event_cb,
                                 settings_back_event_cb);
    s_ui.settings_ui.wifi_overlay = surface.overlay;
    s_ui.settings_ui.wifi_content = surface.content;
    lv_obj_add_event_cb(surface.content, settings_content_scroll_event_cb, LV_EVENT_SCROLL_BEGIN, NULL);
    lv_obj_add_event_cb(surface.content, settings_content_scroll_event_cb, LV_EVENT_SCROLL, NULL);
    lv_obj_add_event_cb(surface.content, settings_content_scroll_event_cb, LV_EVENT_SCROLL_END, NULL);
    title = surface.title;
    lv_obj_set_width(title, lv_pct(100));
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    create_wifi_card(surface.content);
    create_networks_card(surface.content);

    ui_surface_create_edge_sensor(s_ui.settings_ui.wifi_overlay,
                                  &s_ui.settings_ui.wifi_top_sensor,
                                  UI_SURFACE_EDGE_TOP,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  (void *)UI_SURFACE_EDGE_TOP);
    ui_surface_create_edge_sensor(s_ui.settings_ui.wifi_overlay,
                                  &s_ui.settings_ui.wifi_bottom_sensor,
                                  UI_SURFACE_EDGE_BOTTOM,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  (void *)UI_SURFACE_EDGE_BOTTOM);
    ui_surface_create_edge_sensor(s_ui.settings_ui.wifi_overlay,
                                  &s_ui.settings_ui.wifi_left_sensor,
                                  UI_SURFACE_EDGE_LEFT,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  (void *)UI_SURFACE_EDGE_LEFT);
    ui_surface_create_edge_sensor(s_ui.settings_ui.wifi_overlay,
                                  &s_ui.settings_ui.wifi_right_sensor,
                                  UI_SURFACE_EDGE_RIGHT,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  (void *)UI_SURFACE_EDGE_RIGHT);
}

static void create_other_overlay(void)
{
    ui_surface_t surface;
    lv_obj_t *title;

    ui_surface_create_fullscreen(&surface,
                                 s_ui.screen,
                                 lv_color_black(),
                                 LV_OPA_COVER,
                                 lv_color_hex(0x0D0D0D),
                                 SETTINGS_HEADER_HEIGHT,
                                 "Other",
                                 settings_detail_overlay_event_cb,
                                 settings_back_event_cb);
    s_ui.settings_ui.other_overlay = surface.overlay;
    s_ui.settings_ui.other_content = surface.content;
    title = surface.title;
    lv_obj_set_width(title, lv_pct(100));
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);
    lv_obj_set_flex_align(surface.content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(surface.content, LV_OBJ_FLAG_SCROLLABLE);

    create_timezone_card(surface.content);

    ui_surface_create_edge_sensor(s_ui.settings_ui.other_overlay,
                                  &s_ui.settings_ui.other_top_sensor,
                                  UI_SURFACE_EDGE_TOP,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  (void *)UI_SURFACE_EDGE_TOP);
    ui_surface_create_edge_sensor(s_ui.settings_ui.other_overlay,
                                  &s_ui.settings_ui.other_bottom_sensor,
                                  UI_SURFACE_EDGE_BOTTOM,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  (void *)UI_SURFACE_EDGE_BOTTOM);
    ui_surface_create_edge_sensor(s_ui.settings_ui.other_overlay,
                                  &s_ui.settings_ui.other_left_sensor,
                                  UI_SURFACE_EDGE_LEFT,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  (void *)UI_SURFACE_EDGE_LEFT);
    ui_surface_create_edge_sensor(s_ui.settings_ui.other_overlay,
                                  &s_ui.settings_ui.other_right_sensor,
                                  UI_SURFACE_EDGE_RIGHT,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  (void *)UI_SURFACE_EDGE_RIGHT);
}

static void create_night_overlay(void)
{
    ui_surface_t surface;
    lv_obj_t *title;

    ui_surface_create_fullscreen(&surface,
                                 s_ui.screen,
                                 lv_color_black(),
                                 LV_OPA_COVER,
                                 lv_color_hex(0x0D0D0D),
                                 SETTINGS_HEADER_HEIGHT,
                                 "Night mode",
                                 settings_detail_overlay_event_cb,
                                 settings_back_event_cb);
    s_ui.settings_ui.night_overlay = surface.overlay;
    s_ui.settings_ui.night_content = surface.content;
    lv_obj_set_flex_align(surface.content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_bottom(surface.content, 72, 0);
    lv_obj_add_event_cb(surface.content, settings_content_scroll_event_cb, LV_EVENT_SCROLL_BEGIN, NULL);
    lv_obj_add_event_cb(surface.content, settings_content_scroll_event_cb, LV_EVENT_SCROLL, NULL);
    lv_obj_add_event_cb(surface.content, settings_content_scroll_event_cb, LV_EVENT_SCROLL_END, NULL);
    title = surface.title;
    lv_obj_set_width(title, lv_pct(100));
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    create_night_card(surface.content);

    ui_surface_create_edge_sensor(s_ui.settings_ui.night_overlay,
                                  &s_ui.settings_ui.night_top_sensor,
                                  UI_SURFACE_EDGE_TOP,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  (void *)UI_SURFACE_EDGE_TOP);
    ui_surface_create_edge_sensor(s_ui.settings_ui.night_overlay,
                                  &s_ui.settings_ui.night_bottom_sensor,
                                  UI_SURFACE_EDGE_BOTTOM,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  (void *)UI_SURFACE_EDGE_BOTTOM);
    ui_surface_create_edge_sensor(s_ui.settings_ui.night_overlay,
                                  &s_ui.settings_ui.night_left_sensor,
                                  UI_SURFACE_EDGE_LEFT,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  (void *)UI_SURFACE_EDGE_LEFT);
    ui_surface_create_edge_sensor(s_ui.settings_ui.night_overlay,
                                  &s_ui.settings_ui.night_right_sensor,
                                  UI_SURFACE_EDGE_RIGHT,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  (void *)UI_SURFACE_EDGE_RIGHT);
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
    lv_obj_set_flex_align(surface.content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(surface.content, LV_OBJ_FLAG_SCROLLABLE);

    create_settings_menu_entry(surface.content,
                               LV_SYMBOL_WIFI,
                               "Wi-Fi",
                               "Networks and clock sync",
                               settings_wifi_entry_event_cb);
    create_settings_menu_entry(surface.content,
                               LV_SYMBOL_SETTINGS,
                               "Night mode",
                               "Schedule and night face",
                               settings_night_entry_event_cb);
    create_settings_menu_entry(surface.content,
                               LV_SYMBOL_SETTINGS,
                               "Other",
                               "Time zone",
                               settings_other_entry_event_cb);

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

    create_wifi_overlay();
    create_other_overlay();
    create_night_overlay();
    create_night_face_picker_overlay();
    create_wifi_dialog();
}
