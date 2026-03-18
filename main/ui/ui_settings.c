#include "ui/clock_ui_private.h"

static void style_centered_label(lv_obj_t *label, const lv_font_t *font, lv_color_t color)
{
    lv_obj_set_width(label, lv_pct(100));
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label, color, 0);
    if (font != NULL) {
        lv_obj_set_style_text_font(label, font, 0);
    }
}

static void close_wifi_dialog(clock_ui_context_t *ctx);
static void settings_set_label_text_if_changed(lv_obj_t *label, const char *text);
static void wifi_network_btn_event_cb(lv_event_t *event);
static void sync_night_face_preview(clock_ui_context_t *ctx);
static void sync_night_face_picker_previews(clock_ui_context_t *ctx);
static void sync_night_face_picker_selection(clock_ui_context_t *ctx);
static void settings_navigate_back(clock_ui_context_t *ctx);
static void close_night_face_picker(clock_ui_context_t *ctx);
static void close_night_schedule_editor(clock_ui_context_t *ctx);
static void settings_render_face_preview(clock_ui_context_t *ctx,
                                         lv_obj_t *canvas,
                                         lv_draw_buf_t **draw_buf,
                                         uint16_t size,
                                         clock_face_id_t face);
static void style_settings_switch(lv_obj_t *sw);
static ui_edge_ctx_t *settings_edge_ctx(clock_ui_context_t *ctx, ui_surface_edge_t edge);
#define WIFI_UI_MAX_VISIBLE_NETWORKS 8
#define NIGHT_FACE_PREVIEW_SIZE 120
#define NIGHT_CONTROL_SCROLL_CANCEL_TRIGGER 12

static size_t settings_get_scan_results(const clock_ui_context_t *ctx,
                                        const clock_wifi_scan_result_t **results,
                                        uint32_t *generation)
{
    if (results != NULL) {
        *results = ctx->runtime->wifi_scan_results;
    }
    if (generation != NULL) {
        *generation = ctx->runtime->wifi_scan_generation;
    }

    if (ctx->runtime->wifi_scan_count > CLOCK_WIFI_SCAN_RESULT_MAX) {
        return CLOCK_WIFI_SCAN_RESULT_MAX;
    }

    return ctx->runtime->wifi_scan_count;
}

static void invalidate_settings_ui_cache(clock_ui_context_t *ctx)
{
    ctx->settings_ui.wifi_cache_valid = false;
    ctx->settings_ui.night_cache_valid = false;
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

static void settings_set_enabled_state(lv_obj_t *obj, bool enabled)
{
    if (obj == NULL) {
        return;
    }

    if (enabled) {
        lv_obj_remove_state(obj, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(obj, LV_STATE_DISABLED);
    }
}

static void settings_set_clickable_enabled(lv_obj_t *obj, bool enabled)
{
    if (obj == NULL) {
        return;
    }

    if (enabled) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
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

static void settings_format_night_schedule_label(const clock_ui_context_t *ctx, char *buffer, size_t size)
{
    snprintf(buffer, size, "%02u:%02u  " LV_SYMBOL_RIGHT "  %02u:%02u",
             ctx->settings->night_mode.start_hour,
             ctx->settings->night_mode.start_minute,
             ctx->settings->night_mode.end_hour,
             ctx->settings->night_mode.end_minute);
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

static void settings_set_slider_value_if_changed(lv_obj_t *slider, int32_t value)
{
    if (slider == NULL) {
        return;
    }

    if (lv_slider_get_value(slider) != value) {
        lv_slider_set_value(slider, value, LV_ANIM_OFF);
    }
}

static void night_control_scroll_passthrough_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t *target = lv_event_get_target(event);
    lv_indev_t *indev = lv_event_get_indev(event);
    lv_point_t point;

    if (!ctx->settings_ui.night_open || indev == NULL || target == NULL) {
        return;
    }

    lv_indev_get_point(indev, &point);

    if (code == LV_EVENT_PRESSED) {
        ctx->settings_ui.night_control_dragging = true;
        ctx->settings_ui.night_control_interaction_suppressed = false;
        ctx->settings_ui.night_control_drag_target = target;
        ctx->settings_ui.night_control_drag_start_point = point;
        return;
    }

    if (code == LV_EVENT_PRESSING &&
        ctx->settings_ui.night_control_dragging &&
        ctx->settings_ui.night_control_drag_target == target) {
        int32_t dx = point.x - ctx->settings_ui.night_control_drag_start_point.x;
        int32_t dy = point.y - ctx->settings_ui.night_control_drag_start_point.y;

        if (LV_ABS(dy) > LV_ABS(dx) && LV_ABS(dy) >= NIGHT_CONTROL_SCROLL_CANCEL_TRIGGER) {
            ctx->settings_ui.night_control_dragging = false;
            ctx->settings_ui.night_control_interaction_suppressed = true;
            ctx->settings_ui.night_control_drag_target = NULL;
            lv_indev_reset(indev, target);
            lv_indev_stop_processing(indev);
            return;
        }
        return;
    }

    if ((code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) &&
        ctx->settings_ui.night_control_drag_target == target) {
        ctx->settings_ui.night_control_dragging = false;
        ctx->settings_ui.night_control_drag_target = NULL;
    }
}

static void style_settings_switch(lv_obj_t *sw)
{
    if (sw == NULL) {
        return;
    }

    lv_obj_set_size(sw, 96, 56);
    lv_obj_set_style_pad_all(sw, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0x2E2E2E), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(sw, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0x7D8894), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0xF4F6F8), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_shadow_width(sw, 0, LV_PART_KNOB);
    lv_obj_set_style_shadow_opa(sw, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_bg_color(sw, lv_color_white(), LV_PART_KNOB | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0x252525), LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0x59636D), LV_PART_INDICATOR | LV_STATE_CHECKED | LV_STATE_DISABLED);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_CHECKED | LV_STATE_DISABLED);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0xC5CBD1), LV_PART_KNOB | LV_STATE_DISABLED);
    lv_obj_set_style_bg_opa(sw, LV_OPA_70, LV_PART_KNOB | LV_STATE_DISABLED);
}

static void style_night_detail_card(lv_obj_t *card)
{
    if (card == NULL) {
        return;
    }
}

static void style_night_detail_button(lv_obj_t *button)
{
    if (button == NULL) {
        return;
    }
}

static void settings_apply_night_card_visual(lv_obj_t *card, bool enabled)
{
    if (card == NULL) {
        return;
    }

    lv_obj_set_style_bg_color(card, enabled ? lv_color_hex(0x171717) : lv_color_hex(0x141414), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, enabled ? 0 : 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x262626), 0);
    lv_obj_set_style_border_opa(card, enabled ? LV_OPA_TRANSP : LV_OPA_80, 0);
}

static void settings_apply_night_button_visual(lv_obj_t *button, bool enabled)
{
    if (button == NULL) {
        return;
    }

    lv_obj_set_style_bg_color(button, enabled ? lv_color_hex(0x232323) : lv_color_hex(0x1C1C1C), 0);
    lv_obj_set_style_bg_opa(button, enabled ? LV_OPA_COVER : LV_OPA_90, 0);
}

static void settings_apply_night_preview_visual(clock_ui_context_t *ctx, bool enabled)
{
    if (ctx->settings_ui.night_face_preview_shell != NULL) {
        lv_obj_set_style_bg_color(ctx->settings_ui.night_face_preview_shell,
                                  enabled ? lv_color_hex(0x0F0F0F) : lv_color_hex(0x121212),
                                  0);
        lv_obj_set_style_border_color(ctx->settings_ui.night_face_preview_shell,
                                      enabled ? lv_color_hex(0x323232) : lv_color_hex(0x262626),
                                      0);
    }
    if (ctx->settings_ui.night_face_preview != NULL) {
        lv_obj_set_style_image_opa(ctx->settings_ui.night_face_preview,
                                   enabled ? LV_OPA_COVER : LV_OPA_60,
                                   0);
    }
    if (ctx->settings_ui.night_face_preview_scroll != NULL) {
        lv_obj_set_style_image_opa(ctx->settings_ui.night_face_preview_scroll,
                                   enabled ? LV_OPA_COVER : LV_OPA_60,
                                   0);
    }
}

static ui_edge_ctx_t *settings_edge_ctx(clock_ui_context_t *ctx, ui_surface_edge_t edge)
{
    return &ctx->settings_ui.close_edge_ctx[edge];
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
    ui_attach_click_feedback(button, LV_EVENT_CLICKED);
    return button;
}

static void sync_wifi_list(clock_ui_context_t *ctx)
{
    const clock_wifi_scan_result_t *results = NULL;
    uint32_t generation = 0;
    size_t count;
    size_t visible_count;

    if (ctx->settings_ui.wifi_network_list == NULL) {
        return;
    }

    count = settings_get_scan_results(ctx, &results, &generation);
    if (generation == ctx->settings_ui.scan_generation &&
        lv_obj_get_child_count(ctx->settings_ui.wifi_network_list) != 0) {
        return;
    }

    ctx->settings_ui.scan_generation = generation;
    lv_obj_clean(ctx->settings_ui.wifi_network_list);

    if (count == 0) {
        lv_obj_t *label = lv_label_create(ctx->settings_ui.wifi_network_list);

        style_centered_label(label, NULL, lv_color_hex(0xA8A8A8));
        lv_label_set_text(label, "No networks yet");
        return;
    }

    visible_count = LV_MIN(count, WIFI_UI_MAX_VISIBLE_NETWORKS);
    for (size_t i = 0; i < visible_count; ++i) {
        char meta[64];
        const char *ssid = results[i].ssid[0] ? results[i].ssid : "<hidden>";

        ctx->settings_ui.network_ctx[i].network_index = i;
        snprintf(meta, sizeof(meta), "%ddBm", results[i].rssi);
        create_network_button(ctx->settings_ui.wifi_network_list, ssid, meta, &ctx->settings_ui.network_ctx[i]);
    }

    if (count > visible_count) {
        lv_obj_t *label = lv_label_create(ctx->settings_ui.wifi_network_list);

        style_centered_label(label, NULL, lv_color_hex(0x8F8F8F));
        lv_label_set_text_fmt(label, "Showing %u of %u networks",
                              (unsigned)visible_count,
                              (unsigned)count);
    }
}

void sync_night_controls(clock_ui_context_t *ctx)
{
    char face_label[64];
    char brightness_label[32];
    char schedule_label[32];
    uint8_t brightness_ui;
    bool night_enabled;
    bool face_changed;

    if (ctx->settings_ui.night_enabled_sw == NULL) {
        return;
    }

    ctx->suppress_events = true;
    settings_set_switch_checked_if_changed(ctx->settings_ui.night_enabled_sw, ctx->settings->night_mode.enabled);
    settings_set_switch_checked_if_changed(ctx->settings_ui.night_sunrise_sw,
                                           ctx->settings->night_mode.sunrise_brightness_enabled);
    settings_set_roller_selected_if_changed(ctx->settings_ui.night_start_hour_dd, ctx->settings->night_mode.start_hour);
    settings_set_roller_selected_if_changed(ctx->settings_ui.night_start_min_dd, ctx->settings->night_mode.start_minute);
    settings_set_roller_selected_if_changed(ctx->settings_ui.night_end_hour_dd, ctx->settings->night_mode.end_hour);
    settings_set_roller_selected_if_changed(ctx->settings_ui.night_end_min_dd, ctx->settings->night_mode.end_minute);
    ctx->suppress_events = false;
    settings_format_night_schedule_label(ctx, schedule_label, sizeof(schedule_label));
    settings_set_label_text_if_changed(ctx->settings_ui.night_schedule_summary, schedule_label);

    clock_face_id_t night_face = sanitize_enabled_face(ctx->settings->night_mode.face);

    face_changed = !ctx->settings_ui.night_cache_valid ||
                   ctx->settings_ui.cached_night_face != night_face;
    settings_format_face_label(face_label, sizeof(face_label), night_face);
    settings_set_label_text_if_changed(ctx->settings_ui.night_face_dd, face_label);
    if (face_changed) {
        sync_night_face_preview(ctx);
        if (ctx->settings_ui.night_face_picker_open) {
            sync_night_face_picker_previews(ctx);
            sync_night_face_picker_selection(ctx);
        }
    }

    brightness_ui = ctx->settings->night_mode.brightness;
    snprintf(brightness_label, sizeof(brightness_label), "%u%%", brightness_ui);
    settings_set_label_text_if_changed(ctx->settings_ui.night_brightness_dd, brightness_label);
    if (ctx->settings_ui.night_brightness_slider != NULL) {
        ctx->suppress_events = true;
        settings_set_slider_value_if_changed(ctx->settings_ui.night_brightness_slider, brightness_ui);
        ctx->suppress_events = false;
    }

    night_enabled = ctx->settings->night_mode.enabled;
    settings_apply_night_card_visual(ctx->settings_ui.night_schedule_card, night_enabled);
    settings_apply_night_card_visual(ctx->settings_ui.night_face_card, night_enabled);
    settings_apply_night_card_visual(ctx->settings_ui.night_brightness_card, night_enabled);
    settings_apply_night_card_visual(ctx->settings_ui.night_sunrise_card, night_enabled);
    settings_apply_night_button_visual(ctx->settings_ui.night_schedule_button, night_enabled);
    settings_apply_night_button_visual(ctx->settings_ui.night_face_button, night_enabled);
    settings_apply_night_preview_visual(ctx, night_enabled);
    settings_set_clickable_enabled(ctx->settings_ui.night_schedule_button, night_enabled);
    settings_set_clickable_enabled(ctx->settings_ui.night_face_button, night_enabled);
    settings_set_enabled_state(ctx->settings_ui.night_brightness_slider, night_enabled);
    settings_set_enabled_state(ctx->settings_ui.night_sunrise_sw, night_enabled);
    if (ctx->settings_ui.night_status_label != NULL) {
        settings_set_label_text_if_changed(ctx->settings_ui.night_status_label,
                                           "Turn on Night mode to edit schedule, face, brightness, and sunrise ramp.");
        if (night_enabled) {
            lv_obj_add_flag(ctx->settings_ui.night_status_label, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(ctx->settings_ui.night_status_label, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (ctx->settings_ui.night_schedule_summary != NULL) {
        lv_obj_set_style_text_color(ctx->settings_ui.night_schedule_summary,
                                    night_enabled ? lv_color_white() : lv_color_hex(0xC4C4C4),
                                    0);
    }
    if (ctx->settings_ui.night_brightness_dd != NULL) {
        lv_obj_set_style_text_color(ctx->settings_ui.night_brightness_dd,
                                    night_enabled ? lv_color_white() : lv_color_hex(0xC4C4C4),
                                    0);
    }

    ctx->settings_ui.cached_night_enabled = ctx->settings->night_mode.enabled;
    ctx->settings_ui.cached_night_sunrise_brightness_enabled =
        ctx->settings->night_mode.sunrise_brightness_enabled;
    ctx->settings_ui.cached_in_night_mode = ctx->runtime->in_night_mode;
    ctx->settings_ui.cached_night_start_hour = ctx->settings->night_mode.start_hour;
    ctx->settings_ui.cached_night_start_minute = ctx->settings->night_mode.start_minute;
    ctx->settings_ui.cached_night_end_hour = ctx->settings->night_mode.end_hour;
    ctx->settings_ui.cached_night_end_minute = ctx->settings->night_mode.end_minute;
    ctx->settings_ui.cached_night_brightness = ctx->settings->night_mode.brightness;
    ctx->settings_ui.cached_night_face = night_face;
    ctx->settings_ui.night_cache_valid = true;
}

void sync_wifi_controls(clock_ui_context_t *ctx)
{
    char saved[160];
    char timezone_label[24];

    if (ctx->settings_ui.wifi_timezone_dd == NULL) {
        return;
    }

    settings_format_timezone_label(timezone_label, sizeof(timezone_label), ctx->settings->wifi.timezone_offset_hours);
    settings_set_label_text_if_changed(ctx->settings_ui.wifi_timezone_dd, timezone_label);

    settings_set_label_text_if_changed(ctx->settings_ui.wifi_status_label,
                                       ctx->runtime->wifi_status[0] ? ctx->runtime->wifi_status : "Wi-Fi idle");

    if (ctx->settings->wifi.ssid[0] != '\0') {
        snprintf(saved, sizeof(saved), "Saved: %s", ctx->settings->wifi.ssid);
    } else {
        snprintf(saved, sizeof(saved), "Saved: none");
    }
    settings_set_label_text_if_changed(ctx->settings_ui.wifi_saved_label, saved);

    sync_wifi_list(ctx);

    ctx->settings_ui.cached_timezone_offset_hours = ctx->settings->wifi.timezone_offset_hours;
    snprintf(ctx->settings_ui.cached_wifi_status, sizeof(ctx->settings_ui.cached_wifi_status), "%s",
             ctx->runtime->wifi_status);
    snprintf(ctx->settings_ui.cached_wifi_saved_ssid, sizeof(ctx->settings_ui.cached_wifi_saved_ssid), "%s",
             ctx->settings->wifi.ssid);
    ctx->settings_ui.cached_wifi_scan_generation = ctx->runtime->wifi_scan_generation;
    ctx->settings_ui.wifi_cache_valid = true;
}

void refresh_settings_controls(clock_ui_context_t *ctx)
{
    sync_wifi_controls(ctx);
    sync_night_controls(ctx);
}

bool wifi_controls_need_sync(const clock_ui_context_t *ctx)
{
    if (!ctx->settings_ui.wifi_cache_valid) {
        return true;
    }

    if (ctx->settings_ui.cached_timezone_offset_hours != ctx->settings->wifi.timezone_offset_hours) {
        return true;
    }

    if (strcmp(ctx->settings_ui.cached_wifi_status, ctx->runtime->wifi_status) != 0) {
        return true;
    }

    if (strcmp(ctx->settings_ui.cached_wifi_saved_ssid, ctx->settings->wifi.ssid) != 0) {
        return true;
    }

    if (ctx->settings_ui.cached_wifi_scan_generation != ctx->runtime->wifi_scan_generation) {
        return true;
    }

    return false;
}

bool night_controls_need_sync(const clock_ui_context_t *ctx)
{
    if (!ctx->settings_ui.night_cache_valid) {
        return true;
    }

    if (ctx->settings_ui.cached_night_enabled != ctx->settings->night_mode.enabled ||
        ctx->settings_ui.cached_night_sunrise_brightness_enabled !=
            ctx->settings->night_mode.sunrise_brightness_enabled ||
        ctx->settings_ui.cached_in_night_mode != ctx->runtime->in_night_mode ||
        ctx->settings_ui.cached_night_start_hour != ctx->settings->night_mode.start_hour ||
        ctx->settings_ui.cached_night_start_minute != ctx->settings->night_mode.start_minute ||
        ctx->settings_ui.cached_night_end_hour != ctx->settings->night_mode.end_hour ||
        ctx->settings_ui.cached_night_end_minute != ctx->settings->night_mode.end_minute ||
        ctx->settings_ui.cached_night_brightness != ctx->settings->night_mode.brightness ||
        ctx->settings_ui.cached_night_face != sanitize_enabled_face(ctx->settings->night_mode.face)) {
        return true;
    }

    return false;
}

static void settings_render_face_preview(clock_ui_context_t *ctx,
                                         lv_obj_t *image,
                                         lv_draw_buf_t **draw_buf,
                                         uint16_t size,
                                         clock_face_id_t face)
{
    const void *src;

    if (image == NULL || draw_buf == NULL || ctx->settings_ui.night_face_render_canvas == NULL) {
        return;
    }

    src = ui_face_preview_source(ctx, face);
    if (src == NULL) {
        return;
    }

    lv_obj_set_size(ctx->settings_ui.night_face_render_canvas, size, size);
    lv_image_set_src(ctx->settings_ui.night_face_render_canvas, src);
    lv_image_set_inner_align(ctx->settings_ui.night_face_render_canvas, LV_IMAGE_ALIGN_COVER);
    lv_obj_update_layout(ctx->settings_ui.night_face_render_canvas);

    if (*draw_buf == NULL || lv_snapshot_reshape_draw_buf(ctx->settings_ui.night_face_render_canvas, *draw_buf) != LV_RESULT_OK) {
        if (*draw_buf != NULL) {
            lv_draw_buf_destroy(*draw_buf);
        }
        *draw_buf = lv_snapshot_create_draw_buf(ctx->settings_ui.night_face_render_canvas, LV_COLOR_FORMAT_RGB565);
        if (*draw_buf == NULL) {
            return;
        }
    }

    if (lv_snapshot_take_to_draw_buf(ctx->settings_ui.night_face_render_canvas,
                                     LV_COLOR_FORMAT_RGB565,
                                     *draw_buf) != LV_RESULT_OK) {
        return;
    }

    if (lv_image_get_src(image) != *draw_buf) {
        lv_image_set_src(image, *draw_buf);
    } else {
        lv_obj_invalidate(image);
    }
}

static void sync_night_face_preview(clock_ui_context_t *ctx)
{
    if (ctx->settings_ui.night_face_preview == NULL) {
        return;
    }

    settings_render_face_preview(ctx,
                                 ctx->settings_ui.night_face_preview,
                                 &ctx->settings_ui.night_face_preview_buf,
                                 NIGHT_FACE_PREVIEW_SIZE,
                                 ctx->settings->night_mode.face);

    if (ctx->settings_ui.night_face_preview_shell != NULL &&
        ctx->settings_ui.night_face_preview_scroll != NULL) {
        if (ctx->settings_ui.night_face_preview_scroll_buf == NULL ||
            lv_snapshot_reshape_draw_buf(ctx->settings_ui.night_face_preview_shell,
                                         ctx->settings_ui.night_face_preview_scroll_buf) != LV_RESULT_OK) {
            if (ctx->settings_ui.night_face_preview_scroll_buf != NULL) {
                lv_draw_buf_destroy(ctx->settings_ui.night_face_preview_scroll_buf);
            }
            ctx->settings_ui.night_face_preview_scroll_buf =
                lv_snapshot_create_draw_buf(ctx->settings_ui.night_face_preview_shell, LV_COLOR_FORMAT_ARGB8888);
        }

        if (ctx->settings_ui.night_face_preview_scroll_buf != NULL &&
            lv_snapshot_take_to_draw_buf(ctx->settings_ui.night_face_preview_shell,
                                         LV_COLOR_FORMAT_ARGB8888,
                                         ctx->settings_ui.night_face_preview_scroll_buf) == LV_RESULT_OK) {
            if (lv_image_get_src(ctx->settings_ui.night_face_preview_scroll) != ctx->settings_ui.night_face_preview_scroll_buf) {
                lv_image_set_src(ctx->settings_ui.night_face_preview_scroll,
                                 ctx->settings_ui.night_face_preview_scroll_buf);
            } else {
                lv_obj_invalidate(ctx->settings_ui.night_face_preview_scroll);
            }
        }
    }
}

static void sync_night_face_picker_previews(clock_ui_context_t *ctx)
{
    int visible_count = clock_face_visible_count();

    for (int index = 0; index < visible_count; ++index) {
        clock_face_id_t face = clock_face_visible_index_to_id(index);
        lv_obj_t *preview = ctx->settings_ui.night_face_picker_preview[face];

        if (preview != NULL) {
            settings_render_face_preview(ctx,
                                         preview,
                                         &ctx->settings_ui.night_face_picker_preview_buf[face],
                                         96,
                                         face);
        }
    }
}

static void sync_night_face_picker_selection(clock_ui_context_t *ctx)
{
    int visible_count = clock_face_visible_count();

    for (int index = 0; index < visible_count; ++index) {
        clock_face_id_t face = clock_face_visible_index_to_id(index);
        lv_obj_t *card = ctx->settings_ui.night_face_picker_card[face];
        if (card == NULL) {
            continue;
        }

        if (face == ctx->settings->night_mode.face) {
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
    network_ctx_t *network_ctx = (network_ctx_t *)lv_event_get_user_data(event);
    const clock_wifi_scan_result_t *results = NULL;
    clock_ui_context_t *ctx;
    size_t count;

    if (network_ctx == NULL || network_ctx->ui == NULL) {
        return;
    }

    ctx = network_ctx->ui;
    count = settings_get_scan_results(ctx, &results, NULL);
    if (network_ctx->network_index >= count) {
        return;
    }

    snprintf(ctx->settings_ui.pending_ssid,
             sizeof(ctx->settings_ui.pending_ssid),
             "%s",
             results[network_ctx->network_index].ssid);
    lv_label_set_text_fmt(ctx->settings_ui.wifi_dialog_title, "Join %s", ctx->settings_ui.pending_ssid);
    lv_textarea_set_text(ctx->settings_ui.wifi_password_ta, "");
    lv_keyboard_set_textarea(ctx->settings_ui.wifi_keyboard, ctx->settings_ui.wifi_password_ta);
    lv_obj_clear_flag(ctx->settings_ui.wifi_dialog_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ctx->settings_ui.wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ctx->settings_ui.wifi_dialog_overlay);
}

static void wifi_scan_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    if (ctx->callbacks.on_wifi_scan_requested != NULL) {
        ctx->callbacks.on_wifi_scan_requested(ctx->user_ctx);
    }
}

static void wifi_forget_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    if (ctx->callbacks.on_wifi_forget_requested != NULL) {
        ctx->callbacks.on_wifi_forget_requested(ctx->user_ctx);
    }
}

static void wifi_sync_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    if (ctx->callbacks.on_wifi_sync_requested != NULL) {
        ctx->callbacks.on_wifi_sync_requested(ctx->user_ctx);
    }
}

static void wifi_dialog_close_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);

    if (lv_event_get_current_target(event) == ctx->settings_ui.wifi_dialog_overlay &&
        lv_event_get_target(event) != ctx->settings_ui.wifi_dialog_overlay) {
        return;
    }

    close_wifi_dialog(ctx);
}

static void wifi_dialog_connect_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    const char *password;

    if (ctx->settings_ui.pending_ssid[0] == '\0') {
        return;
    }

    password = lv_textarea_get_text(ctx->settings_ui.wifi_password_ta);
    request_save_wifi_credentials(ctx, ctx->settings_ui.pending_ssid, password ? password : "");

    wifi_dialog_close_event_cb(event);
}

static void wifi_ta_focus_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);

    if (lv_event_get_code(event) == LV_EVENT_FOCUSED) {
        lv_keyboard_set_textarea(ctx->settings_ui.wifi_keyboard, ctx->settings_ui.wifi_password_ta);
        lv_obj_clear_flag(ctx->settings_ui.wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

static void wifi_keyboard_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        wifi_dialog_close_event_cb(event);
    }
}

static void close_wifi_dialog(clock_ui_context_t *ctx)
{
    ctx->settings_ui.pending_ssid[0] = '\0';
    if (ctx->settings_ui.wifi_keyboard != NULL) {
        lv_keyboard_set_textarea(ctx->settings_ui.wifi_keyboard, NULL);
        lv_obj_add_flag(ctx->settings_ui.wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
    if (ctx->settings_ui.wifi_dialog_overlay != NULL) {
        lv_obj_add_flag(ctx->settings_ui.wifi_dialog_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

static lv_obj_t *create_settings_menu_entry(lv_obj_t *parent,
                                            const char *symbol,
                                            const char *title_text,
                                            const char *subtitle_text,
                                            lv_event_cb_t cb,
                                            clock_ui_context_t *ctx)
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
        lv_obj_add_event_cb(entry, cb, LV_EVENT_CLICKED, ctx);
    }
    ui_attach_click_feedback(entry, LV_EVENT_CLICKED);

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

static void settings_show_root(clock_ui_context_t *ctx)
{
    ctx->settings_ui.wifi_open = false;
    ctx->settings_ui.night_open = false;
    ctx->settings_ui.other_open = false;
    ctx->settings_ui.night_face_picker_open = false;
    ctx->settings_ui.night_schedule_editor_open = false;
    ctx->settings_ui.close_swipe_consumed = false;
    ctx->settings_ui.wifi_scrolling = false;
    ctx->settings_ui.night_scrolling = false;
    invalidate_settings_ui_cache(ctx);
    close_wifi_dialog(ctx);
    close_night_face_picker(ctx);
    close_night_schedule_editor(ctx);
    if (ctx->settings_ui.wifi_overlay != NULL) {
        lv_obj_add_flag(ctx->settings_ui.wifi_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    if (ctx->settings_ui.other_overlay != NULL) {
        lv_obj_add_flag(ctx->settings_ui.other_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    if (ctx->settings_ui.night_overlay != NULL) {
        lv_obj_add_flag(ctx->settings_ui.night_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    if (ctx->settings_ui.overlay != NULL) {
        lv_obj_clear_flag(ctx->settings_ui.overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(ctx->settings_ui.overlay);
    }
}

static void settings_open_wifi_detail(clock_ui_context_t *ctx)
{
    if (ctx->settings_ui.wifi_overlay == NULL) {
        return;
    }

    ctx->settings_ui.wifi_open = true;
    ctx->settings_ui.night_open = false;
    ctx->settings_ui.other_open = false;
    ctx->settings_ui.close_swipe_consumed = false;
    ctx->settings_ui.wifi_cache_valid = false;
    lv_obj_add_flag(ctx->settings_ui.overlay, LV_OBJ_FLAG_HIDDEN);
    if (ctx->settings_ui.other_overlay != NULL) {
        lv_obj_add_flag(ctx->settings_ui.other_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    if (ctx->settings_ui.night_overlay != NULL) {
        lv_obj_add_flag(ctx->settings_ui.night_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    sync_wifi_controls(ctx);
    lv_obj_clear_flag(ctx->settings_ui.wifi_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ctx->settings_ui.wifi_overlay);
}

static void settings_open_night_detail(clock_ui_context_t *ctx)
{
    if (ctx->settings_ui.night_overlay == NULL) {
        return;
    }

    ctx->settings_ui.night_open = true;
    ctx->settings_ui.wifi_open = false;
    ctx->settings_ui.other_open = false;
    ctx->settings_ui.close_swipe_consumed = false;
    ctx->settings_ui.night_cache_valid = false;
    lv_obj_add_flag(ctx->settings_ui.overlay, LV_OBJ_FLAG_HIDDEN);
    if (ctx->settings_ui.other_overlay != NULL) {
        lv_obj_add_flag(ctx->settings_ui.other_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    if (ctx->settings_ui.wifi_overlay != NULL) {
        lv_obj_add_flag(ctx->settings_ui.wifi_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    close_night_face_picker(ctx);
    close_night_schedule_editor(ctx);
    sync_night_controls(ctx);
    lv_obj_clear_flag(ctx->settings_ui.night_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ctx->settings_ui.night_overlay);
}

static void settings_open_other_detail(clock_ui_context_t *ctx)
{
    if (ctx->settings_ui.other_overlay == NULL) {
        return;
    }

    ctx->settings_ui.other_open = true;
    ctx->settings_ui.wifi_open = false;
    ctx->settings_ui.night_open = false;
    ctx->settings_ui.close_swipe_consumed = false;
    ctx->settings_ui.wifi_cache_valid = false;
    lv_obj_add_flag(ctx->settings_ui.overlay, LV_OBJ_FLAG_HIDDEN);
    if (ctx->settings_ui.wifi_overlay != NULL) {
        lv_obj_add_flag(ctx->settings_ui.wifi_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    if (ctx->settings_ui.night_overlay != NULL) {
        lv_obj_add_flag(ctx->settings_ui.night_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    sync_wifi_controls(ctx);
    lv_obj_clear_flag(ctx->settings_ui.other_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ctx->settings_ui.other_overlay);
}

static void settings_wifi_entry_event_cb(lv_event_t *event)
{
    settings_open_wifi_detail((clock_ui_context_t *)lv_event_get_user_data(event));
}

static void settings_night_entry_event_cb(lv_event_t *event)
{
    settings_open_night_detail((clock_ui_context_t *)lv_event_get_user_data(event));
}

static void settings_other_entry_event_cb(lv_event_t *event)
{
    settings_open_other_detail((clock_ui_context_t *)lv_event_get_user_data(event));
}

static void settings_back_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    settings_navigate_back(ctx);
}

void open_settings_tab(clock_ui_context_t *ctx, uint32_t tab_idx)
{
    brightness_overlay_hide_immediately(ctx);
    brightness_panel_hide(ctx);
    if (ctx->alarms.editor_open) {
        alarm_editor_close(ctx);
    }
    if (ctx->alarms.open) {
        alarm_management_close(ctx);
    }

    ctx->settings_ui.open = true;
    ctx->settings_ui.close_dragging = false;
    ctx->settings_ui.wifi_scrolling = false;
    ctx->settings_ui.night_scrolling = false;
    set_root_ui_hidden(ctx, true);

    settings_show_root(ctx);
    if (tab_idx == SETTINGS_TAB_NIGHT) {
        settings_open_night_detail(ctx);
    }
}

void settings_button_event_cb(lv_event_t *event)
{
    open_settings_tab((clock_ui_context_t *)lv_event_get_user_data(event), SETTINGS_TAB_WIFI);
}

static void close_settings_surface(clock_ui_context_t *ctx)
{
    ctx->settings_ui.open = false;
    ctx->settings_ui.wifi_open = false;
    ctx->settings_ui.night_open = false;
    ctx->settings_ui.other_open = false;
    ctx->settings_ui.night_face_picker_open = false;
    ctx->settings_ui.night_schedule_editor_open = false;
    ctx->settings_ui.wifi_scrolling = false;
    ctx->settings_ui.night_scrolling = false;
    ctx->settings_ui.close_dragging = false;
    ctx->settings_ui.close_swipe_consumed = false;
    invalidate_settings_ui_cache(ctx);
    close_wifi_dialog(ctx);
    close_night_face_picker(ctx);
    close_night_schedule_editor(ctx);
    if (ctx->settings_ui.wifi_overlay != NULL) {
        lv_obj_add_flag(ctx->settings_ui.wifi_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    if (ctx->settings_ui.other_overlay != NULL) {
        lv_obj_add_flag(ctx->settings_ui.other_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    if (ctx->settings_ui.night_overlay != NULL) {
        lv_obj_add_flag(ctx->settings_ui.night_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_flag(ctx->settings_ui.overlay, LV_OBJ_FLAG_HIDDEN);
    set_root_ui_hidden(ctx, false);
    show_affordances_temporarily(ctx);
}

static void settings_navigate_back(clock_ui_context_t *ctx)
{
    if (ctx->settings_ui.night_face_picker_open) {
        close_night_face_picker(ctx);
        return;
    }
    if (ctx->settings_ui.night_schedule_editor_open) {
        close_night_schedule_editor(ctx);
        return;
    }
    settings_show_root(ctx);
}

bool settings_surface_is_open(const clock_ui_context_t *ctx)
{
    return ctx->settings_ui.open;
}

void settings_surface_close(clock_ui_context_t *ctx)
{
    close_settings_surface(ctx);
}

static void settings_close_event_cb(lv_event_t *event)
{
    close_settings_surface((clock_ui_context_t *)lv_event_get_user_data(event));
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
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t *target = lv_event_get_target(event);
    bool scrolling = (code == LV_EVENT_SCROLL_BEGIN || code == LV_EVENT_SCROLL);

    if (target == ctx->settings_ui.wifi_content) {
        ctx->settings_ui.wifi_scrolling = scrolling;
    } else if (target == ctx->settings_ui.night_content) {
        ctx->settings_ui.night_scrolling = scrolling;
        if (ctx->settings_ui.night_face_preview_shell != NULL) {
            if (scrolling) {
                if (!lv_obj_has_flag(ctx->settings_ui.night_face_preview_shell, LV_OBJ_FLAG_HIDDEN)) {
                    lv_obj_add_flag(ctx->settings_ui.night_face_preview_shell, LV_OBJ_FLAG_HIDDEN);
                }
                if (ctx->settings_ui.night_face_preview_scroll != NULL &&
                    lv_obj_has_flag(ctx->settings_ui.night_face_preview_scroll, LV_OBJ_FLAG_HIDDEN)) {
                    lv_obj_clear_flag(ctx->settings_ui.night_face_preview_scroll, LV_OBJ_FLAG_HIDDEN);
                }
            } else if (lv_obj_has_flag(ctx->settings_ui.night_face_preview_shell, LV_OBJ_FLAG_HIDDEN)) {
                lv_obj_clear_flag(ctx->settings_ui.night_face_preview_shell, LV_OBJ_FLAG_HIDDEN);
                if (ctx->settings_ui.night_face_preview_scroll != NULL &&
                    !lv_obj_has_flag(ctx->settings_ui.night_face_preview_scroll, LV_OBJ_FLAG_HIDDEN)) {
                    lv_obj_add_flag(ctx->settings_ui.night_face_preview_scroll, LV_OBJ_FLAG_HIDDEN);
                }
                sync_night_face_preview(ctx);
            }
        }
    }

    if (code == LV_EVENT_SCROLL_END) {
        if (target == ctx->settings_ui.wifi_content) {
            ctx->settings_ui.wifi_scrolling = false;
        } else if (target == ctx->settings_ui.night_content) {
            ctx->settings_ui.night_scrolling = false;
        }
    }
}

static void settings_close_swipe_event_cb(lv_event_t *event)
{
    ui_edge_ctx_t *edge_ctx = (ui_edge_ctx_t *)lv_event_get_user_data(event);
    lv_event_code_t code = lv_event_get_code(event);
    lv_indev_t *indev = lv_event_get_indev(event);
    clock_ui_context_t *ctx;
    lv_point_t point;

    if (edge_ctx == NULL || edge_ctx->ui == NULL) {
        return;
    }

    ctx = edge_ctx->ui;
    if (!ctx->settings_ui.open || indev == NULL) {
        return;
    }

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        ctx->settings_ui.close_dragging = false;
        ctx->settings_ui.close_swipe_consumed = false;
        return;
    }

    if (ctx->settings_ui.close_swipe_consumed) {
        return;
    }

    lv_indev_get_point(indev, &point);

    if (code == LV_EVENT_PRESSED) {
        ctx->settings_ui.close_dragging = true;
        ctx->settings_ui.close_drag_start_point = point;
        ctx->settings_ui.close_swipe_consumed = false;
        return;
    }

    if (!ctx->settings_ui.close_dragging) {
        return;
    }

    if (code == LV_EVENT_PRESSING) {
        if (ui_surface_edge_swipe_trigger(edge_ctx->edge,
                                          &ctx->settings_ui.close_drag_start_point,
                                          &point,
                                          SETTINGS_CLOSE_SWIPE_TRIGGER)) {
            ctx->settings_ui.close_dragging = false;
            ctx->settings_ui.close_swipe_consumed = true;
            if (ctx->settings_ui.wifi_open || ctx->settings_ui.night_open || ctx->settings_ui.other_open) {
                settings_navigate_back(ctx);
            } else {
                close_settings_surface(ctx);
            }
        }
        return;
    }
}

static void timezone_prev_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    if (ctx->settings->wifi.timezone_offset_hours <= -12) {
        request_set_timezone(ctx, 14);
    } else {
        request_set_timezone(ctx, (int8_t)(ctx->settings->wifi.timezone_offset_hours - 1));
    }
    sync_wifi_controls(ctx);
}

static void timezone_next_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    if (ctx->settings->wifi.timezone_offset_hours >= 14) {
        request_set_timezone(ctx, -12);
    } else {
        request_set_timezone(ctx, (int8_t)(ctx->settings->wifi.timezone_offset_hours + 1));
    }
    sync_wifi_controls(ctx);
}

static void night_enabled_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    if (ctx->suppress_events || ctx->settings_ui.night_control_interaction_suppressed) {
        ctx->settings_ui.night_control_interaction_suppressed = false;
        return;
    }

    request_set_night_mode_enabled(ctx, lv_obj_has_state(lv_event_get_target(event), LV_STATE_CHECKED));
    sync_night_controls(ctx);
}

static void night_hour_minute_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    lv_obj_t *target = lv_event_get_target(event);

    if (ctx->suppress_events) {
        return;
    }

    request_set_night_schedule(
        ctx,
        (target == ctx->settings_ui.night_start_hour_dd) ? (uint8_t)lv_roller_get_selected(target) : ctx->settings->night_mode.start_hour,
        (target == ctx->settings_ui.night_start_min_dd) ? (uint8_t)lv_roller_get_selected(target) : ctx->settings->night_mode.start_minute,
        (target == ctx->settings_ui.night_end_hour_dd) ? (uint8_t)lv_roller_get_selected(target) : ctx->settings->night_mode.end_hour,
        (target == ctx->settings_ui.night_end_min_dd) ? (uint8_t)lv_roller_get_selected(target) : ctx->settings->night_mode.end_minute);
    sync_night_controls(ctx);
}

static void open_night_schedule_editor(clock_ui_context_t *ctx)
{
    if (ctx->settings_ui.night_schedule_overlay == NULL) {
        return;
    }

    ctx->settings_ui.night_schedule_editor_open = true;
    sync_night_controls(ctx);
    lv_obj_clear_flag(ctx->settings_ui.night_schedule_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ctx->settings_ui.night_schedule_overlay);
}

static void close_night_schedule_editor(clock_ui_context_t *ctx)
{
    ctx->settings_ui.night_schedule_editor_open = false;
    if (ctx->settings_ui.night_schedule_overlay != NULL) {
        lv_obj_add_flag(ctx->settings_ui.night_schedule_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

static void night_schedule_button_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    if (ctx->settings_ui.night_control_interaction_suppressed) {
        ctx->settings_ui.night_control_interaction_suppressed = false;
        return;
    }

    open_night_schedule_editor(ctx);
}

static void night_schedule_editor_overlay_event_cb(lv_event_t *event)
{
    if (lv_event_get_target(event) == lv_event_get_current_target(event)) {
        close_night_schedule_editor((clock_ui_context_t *)lv_event_get_user_data(event));
    }
}

static void night_schedule_editor_close_event_cb(lv_event_t *event)
{
    close_night_schedule_editor((clock_ui_context_t *)lv_event_get_user_data(event));
}

static void night_brightness_slider_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    char brightness_label[32];
    int ui_value;

    if (ctx->suppress_events || ctx->settings_ui.night_control_interaction_suppressed) {
        ctx->settings_ui.night_control_interaction_suppressed = false;
        return;
    }

    ui_value = lv_slider_get_value(lv_event_get_target(event));
    snprintf(brightness_label, sizeof(brightness_label), "%d%%", ui_value);
    settings_set_label_text_if_changed(ctx->settings_ui.night_brightness_dd, brightness_label);
    request_set_night_brightness(ctx, (uint8_t)ui_value);
}

static void night_sunrise_enabled_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);

    if (ctx->suppress_events || ctx->settings_ui.night_control_interaction_suppressed) {
        ctx->settings_ui.night_control_interaction_suppressed = false;
        return;
    }

    request_set_night_sunrise_brightness_enabled(ctx,
                                                 lv_obj_has_state(lv_event_get_target(event), LV_STATE_CHECKED));
    sync_night_controls(ctx);
}

static void open_night_face_picker(clock_ui_context_t *ctx)
{
    if (ctx->settings_ui.night_face_picker_overlay == NULL) {
        return;
    }

    sync_night_face_preview(ctx);
    sync_night_face_picker_previews(ctx);
    sync_night_face_picker_selection(ctx);
    ctx->settings_ui.night_face_picker_open = true;
    lv_obj_clear_flag(ctx->settings_ui.night_face_picker_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ctx->settings_ui.night_face_picker_overlay);
}

static void close_night_face_picker(clock_ui_context_t *ctx)
{
    ctx->settings_ui.night_face_picker_open = false;
    if (ctx->settings_ui.night_face_picker_overlay != NULL) {
        lv_obj_add_flag(ctx->settings_ui.night_face_picker_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

static void night_face_button_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    if (ctx->settings_ui.night_control_interaction_suppressed) {
        ctx->settings_ui.night_control_interaction_suppressed = false;
        return;
    }

    open_night_face_picker(ctx);
}

static void night_face_picker_item_event_cb(lv_event_t *event)
{
    face_ctx_t *face_ctx = (face_ctx_t *)lv_event_get_user_data(event);
    clock_ui_context_t *ctx;

    if (face_ctx == NULL || face_ctx->ui == NULL || !clock_face_is_enabled(face_ctx->face)) {
        return;
    }

    ctx = face_ctx->ui;
    request_set_night_face(ctx, face_ctx->face);
    sync_night_controls(ctx);
    close_night_face_picker(ctx);
}

static void night_face_picker_overlay_event_cb(lv_event_t *event)
{
    if (lv_event_get_target(event) == lv_event_get_current_target(event)) {
        close_night_face_picker((clock_ui_context_t *)lv_event_get_user_data(event));
    }
}

static void night_face_picker_close_event_cb(lv_event_t *event)
{
    close_night_face_picker((clock_ui_context_t *)lv_event_get_user_data(event));
}

static void create_wifi_card(clock_ui_context_t *ctx, lv_obj_t *parent)
{
    lv_obj_t *row;

    ctx->settings_ui.wifi_card = create_card(parent);
    center_card_children(ctx->settings_ui.wifi_card);

    ctx->settings_ui.wifi_status_label = lv_label_create(ctx->settings_ui.wifi_card);
    style_centered_label(ctx->settings_ui.wifi_status_label, &lv_font_montserrat_20, lv_color_white());
    lv_label_set_text(ctx->settings_ui.wifi_status_label, "Wi-Fi idle");

    ctx->settings_ui.wifi_saved_label = lv_label_create(ctx->settings_ui.wifi_card);
    style_centered_label(ctx->settings_ui.wifi_saved_label, NULL, lv_color_hex(0xA8A8A8));
    lv_label_set_text(ctx->settings_ui.wifi_saved_label, "Saved: none");

    row = create_row(ctx->settings_ui.wifi_card);
    center_row(row);
    create_action_button(row, "Scan", wifi_scan_event_cb, ctx);
    create_action_button(row, "Sync time", wifi_sync_event_cb, ctx);
    create_action_button(row, "Forget", wifi_forget_event_cb, ctx);
}

static void create_timezone_card(clock_ui_context_t *ctx, lv_obj_t *parent)
{
    lv_obj_t *row;

    ctx->settings_ui.timezone_card = create_card(parent);
    center_card_children(ctx->settings_ui.timezone_card);
    create_centered_card_title(ctx->settings_ui.timezone_card, "Time zone");

    row = create_step_selector(ctx->settings_ui.timezone_card,
                               240,
                               78,
                               &ctx->settings_ui.wifi_timezone_dd,
                               timezone_prev_event_cb,
                               ctx,
                               timezone_next_event_cb,
                               ctx);
    center_row(row);
}

static void create_networks_card(clock_ui_context_t *ctx, lv_obj_t *parent)
{
    ctx->settings_ui.networks_card = create_card(parent);
    create_centered_card_title(ctx->settings_ui.networks_card, "Networks");

    ctx->settings_ui.wifi_network_list = lv_obj_create(ctx->settings_ui.networks_card);
    lv_obj_set_width(ctx->settings_ui.wifi_network_list, lv_pct(100));
    lv_obj_set_height(ctx->settings_ui.wifi_network_list, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(ctx->settings_ui.wifi_network_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ctx->settings_ui.wifi_network_list, 0, 0);
    lv_obj_set_style_radius(ctx->settings_ui.wifi_network_list, 0, 0);
    lv_obj_set_style_pad_all(ctx->settings_ui.wifi_network_list, 0, 0);
    lv_obj_set_style_pad_row(ctx->settings_ui.wifi_network_list, 14, 0);
    lv_obj_set_layout(ctx->settings_ui.wifi_network_list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(ctx->settings_ui.wifi_network_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(ctx->settings_ui.wifi_network_list, LV_OBJ_FLAG_SCROLLABLE);
}

static void create_night_card(clock_ui_context_t *ctx, lv_obj_t *parent)
{
    lv_obj_t *row;
    lv_obj_t *label;
    lv_obj_t *card;
    lv_obj_t *preview_wrap;

    ctx->settings_ui.night_card = create_card(parent);
    lv_obj_set_width(ctx->settings_ui.night_card, 540);
    lv_obj_set_style_pad_all(ctx->settings_ui.night_card, 24, 0);
    lv_obj_set_style_pad_row(ctx->settings_ui.night_card, 14, 0);

    row = create_row(ctx->settings_ui.night_card);
    center_row(row);
    label = lv_label_create(row);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, "Enabled");
    ctx->settings_ui.night_enabled_sw = lv_switch_create(row);
    style_settings_switch(ctx->settings_ui.night_enabled_sw);
    lv_obj_add_flag(ctx->settings_ui.night_enabled_sw, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(ctx->settings_ui.night_enabled_sw, night_control_scroll_passthrough_event_cb, LV_EVENT_PRESSED, ctx);
    lv_obj_add_event_cb(ctx->settings_ui.night_enabled_sw, night_control_scroll_passthrough_event_cb, LV_EVENT_PRESSING, ctx);
    lv_obj_add_event_cb(ctx->settings_ui.night_enabled_sw, night_control_scroll_passthrough_event_cb, LV_EVENT_RELEASED, ctx);
    lv_obj_add_event_cb(ctx->settings_ui.night_enabled_sw, night_control_scroll_passthrough_event_cb, LV_EVENT_PRESS_LOST, ctx);
    lv_obj_add_event_cb(ctx->settings_ui.night_enabled_sw, night_enabled_event_cb, LV_EVENT_VALUE_CHANGED, ctx);
    ui_attach_click_feedback(ctx->settings_ui.night_enabled_sw, LV_EVENT_VALUE_CHANGED);

    ctx->settings_ui.night_status_label = lv_label_create(ctx->settings_ui.night_card);
    lv_obj_set_width(ctx->settings_ui.night_status_label, lv_pct(100));
    lv_obj_set_style_text_font(ctx->settings_ui.night_status_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(ctx->settings_ui.night_status_label, lv_color_hex(0xA8A8A8), 0);
    lv_obj_set_style_text_align(ctx->settings_ui.night_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(ctx->settings_ui.night_status_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(ctx->settings_ui.night_status_label, "");
    lv_obj_add_flag(ctx->settings_ui.night_status_label, LV_OBJ_FLAG_HIDDEN);

    card = create_card(parent);
    ctx->settings_ui.night_schedule_card = card;
    lv_obj_set_width(card, 540);
    lv_obj_set_style_pad_all(card, 24, 0);
    lv_obj_set_style_pad_row(card, 10, 0);
    style_night_detail_card(card);
    create_section_title(card, "Schedule", "Tap to edit");
    ctx->settings_ui.night_schedule_button = lv_button_create(card);
    lv_obj_set_width(ctx->settings_ui.night_schedule_button, lv_pct(100));
    lv_obj_set_height(ctx->settings_ui.night_schedule_button, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(ctx->settings_ui.night_schedule_button, 28, 0);
    lv_obj_set_style_bg_color(ctx->settings_ui.night_schedule_button, lv_color_hex(0x232323), 0);
    lv_obj_set_style_bg_color(ctx->settings_ui.night_schedule_button, lv_color_hex(0x2D2D2D), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(ctx->settings_ui.night_schedule_button, 0, 0);
    lv_obj_set_style_pad_all(ctx->settings_ui.night_schedule_button, 20, 0);
    lv_obj_set_style_shadow_width(ctx->settings_ui.night_schedule_button, 0, 0);
    style_night_detail_button(ctx->settings_ui.night_schedule_button);
    lv_obj_add_flag(ctx->settings_ui.night_schedule_button, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(ctx->settings_ui.night_schedule_button, night_control_scroll_passthrough_event_cb, LV_EVENT_PRESSED, ctx);
    lv_obj_add_event_cb(ctx->settings_ui.night_schedule_button, night_control_scroll_passthrough_event_cb, LV_EVENT_PRESSING, ctx);
    lv_obj_add_event_cb(ctx->settings_ui.night_schedule_button, night_control_scroll_passthrough_event_cb, LV_EVENT_RELEASED, ctx);
    lv_obj_add_event_cb(ctx->settings_ui.night_schedule_button, night_control_scroll_passthrough_event_cb, LV_EVENT_PRESS_LOST, ctx);
    lv_obj_add_event_cb(ctx->settings_ui.night_schedule_button, night_schedule_button_event_cb, LV_EVENT_CLICKED, ctx);
    ui_attach_click_feedback(ctx->settings_ui.night_schedule_button, LV_EVENT_CLICKED);

    ctx->settings_ui.night_schedule_summary = lv_label_create(ctx->settings_ui.night_schedule_button);
    lv_obj_set_width(ctx->settings_ui.night_schedule_summary, lv_pct(100));
    lv_obj_set_style_text_font(ctx->settings_ui.night_schedule_summary, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(ctx->settings_ui.night_schedule_summary, lv_color_white(), 0);
    lv_obj_set_style_text_align(ctx->settings_ui.night_schedule_summary, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(ctx->settings_ui.night_schedule_summary, "");
    lv_obj_center(ctx->settings_ui.night_schedule_summary);

    card = create_card(parent);
    ctx->settings_ui.night_face_card = card;
    lv_obj_set_width(card, 540);
    lv_obj_set_style_pad_all(card, 24, 0);
    lv_obj_set_style_pad_row(card, 16, 0);
    style_night_detail_card(card);
    create_section_title(card, "Night face", "Tap to choose from previews");
    ctx->settings_ui.night_face_button = lv_button_create(card);
    lv_obj_set_width(ctx->settings_ui.night_face_button, lv_pct(100));
    lv_obj_set_height(ctx->settings_ui.night_face_button, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(ctx->settings_ui.night_face_button, 28, 0);
    lv_obj_set_style_bg_color(ctx->settings_ui.night_face_button, lv_color_hex(0x232323), 0);
    lv_obj_set_style_bg_color(ctx->settings_ui.night_face_button, lv_color_hex(0x2D2D2D), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(ctx->settings_ui.night_face_button, 0, 0);
    lv_obj_set_style_pad_all(ctx->settings_ui.night_face_button, 18, 0);
    lv_obj_set_style_shadow_width(ctx->settings_ui.night_face_button, 0, 0);
    style_night_detail_button(ctx->settings_ui.night_face_button);
    lv_obj_add_flag(ctx->settings_ui.night_face_button, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(ctx->settings_ui.night_face_button, night_control_scroll_passthrough_event_cb, LV_EVENT_PRESSED, ctx);
    lv_obj_add_event_cb(ctx->settings_ui.night_face_button, night_control_scroll_passthrough_event_cb, LV_EVENT_PRESSING, ctx);
    lv_obj_add_event_cb(ctx->settings_ui.night_face_button, night_control_scroll_passthrough_event_cb, LV_EVENT_RELEASED, ctx);
    lv_obj_add_event_cb(ctx->settings_ui.night_face_button, night_control_scroll_passthrough_event_cb, LV_EVENT_PRESS_LOST, ctx);
    lv_obj_add_event_cb(ctx->settings_ui.night_face_button, night_face_button_event_cb, LV_EVENT_CLICKED, ctx);
    ui_attach_click_feedback(ctx->settings_ui.night_face_button, LV_EVENT_CLICKED);

    preview_wrap = lv_obj_create(ctx->settings_ui.night_face_button);
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
    ctx->settings_ui.night_face_preview_shell = row;
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

    ctx->settings_ui.night_face_preview = lv_image_create(row);
    lv_obj_add_flag(ctx->settings_ui.night_face_preview, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_size(ctx->settings_ui.night_face_preview, NIGHT_FACE_PREVIEW_SIZE, NIGHT_FACE_PREVIEW_SIZE);
    lv_obj_set_style_bg_opa(ctx->settings_ui.night_face_preview, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ctx->settings_ui.night_face_preview, 0, 0);
    lv_obj_set_style_pad_all(ctx->settings_ui.night_face_preview, 0, 0);
    lv_obj_clear_flag(ctx->settings_ui.night_face_preview, LV_OBJ_FLAG_SCROLLABLE);
    lv_image_set_inner_align(ctx->settings_ui.night_face_preview, LV_IMAGE_ALIGN_CENTER);
    lv_obj_center(ctx->settings_ui.night_face_preview);

    ctx->settings_ui.night_face_preview_scroll = lv_image_create(ctx->settings_ui.night_face_button);
    lv_obj_add_flag(ctx->settings_ui.night_face_preview_scroll, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(ctx->settings_ui.night_face_preview_scroll, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(ctx->settings_ui.night_face_preview_scroll,
                    NIGHT_FACE_PREVIEW_SIZE,
                    NIGHT_FACE_PREVIEW_SIZE);
    lv_obj_set_style_bg_opa(ctx->settings_ui.night_face_preview_scroll, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ctx->settings_ui.night_face_preview_scroll, 0, 0);
    lv_obj_set_style_pad_all(ctx->settings_ui.night_face_preview_scroll, 0, 0);
    lv_obj_clear_flag(ctx->settings_ui.night_face_preview_scroll, LV_OBJ_FLAG_SCROLLABLE);
    lv_image_set_inner_align(ctx->settings_ui.night_face_preview_scroll, LV_IMAGE_ALIGN_CENTER);
    lv_obj_align_to(ctx->settings_ui.night_face_preview_scroll, row, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(ctx->settings_ui.night_face_preview_scroll, LV_OBJ_FLAG_HIDDEN);

    ctx->settings_ui.night_face_dd = NULL;

    card = create_card(parent);
    ctx->settings_ui.night_brightness_card = card;
    lv_obj_set_width(card, 540);
    lv_obj_set_style_pad_all(card, 24, 0);
    lv_obj_set_style_pad_row(card, 14, 0);
    style_night_detail_card(card);
    create_section_title(card, "Brightness", "Matches the main slider interaction");

    row = create_row(card);
    center_row(row);
    ctx->settings_ui.night_brightness_dd = lv_label_create(row);
    lv_obj_set_style_text_font(ctx->settings_ui.night_brightness_dd, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(ctx->settings_ui.night_brightness_dd, lv_color_white(), 0);
    lv_label_set_text(ctx->settings_ui.night_brightness_dd, "0%");

    ctx->settings_ui.night_brightness_slider = lv_slider_create(card);
    lv_slider_set_range(ctx->settings_ui.night_brightness_slider, 0, 100);
    lv_obj_set_width(ctx->settings_ui.night_brightness_slider, lv_pct(100));
    style_slider(ctx->settings_ui.night_brightness_slider);
    lv_obj_set_height(ctx->settings_ui.night_brightness_slider, 24);
    lv_obj_add_flag(ctx->settings_ui.night_brightness_slider, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(ctx->settings_ui.night_brightness_slider, night_control_scroll_passthrough_event_cb, LV_EVENT_PRESSED, ctx);
    lv_obj_add_event_cb(ctx->settings_ui.night_brightness_slider, night_control_scroll_passthrough_event_cb, LV_EVENT_PRESSING, ctx);
    lv_obj_add_event_cb(ctx->settings_ui.night_brightness_slider, night_control_scroll_passthrough_event_cb, LV_EVENT_RELEASED, ctx);
    lv_obj_add_event_cb(ctx->settings_ui.night_brightness_slider, night_control_scroll_passthrough_event_cb, LV_EVENT_PRESS_LOST, ctx);
    lv_obj_set_style_bg_color(ctx->settings_ui.night_brightness_slider, lv_color_hex(0x2D2D2D), LV_PART_MAIN);
    lv_obj_set_style_bg_color(ctx->settings_ui.night_brightness_slider, lv_color_hex(UI_ACCENT_COL), LV_PART_INDICATOR);
    lv_obj_set_style_shadow_width(ctx->settings_ui.night_brightness_slider, 0, LV_PART_KNOB);
    lv_obj_set_style_shadow_opa(ctx->settings_ui.night_brightness_slider, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_bg_color(ctx->settings_ui.night_brightness_slider,
                              lv_color_hex(0x222222),
                              LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_style_bg_color(ctx->settings_ui.night_brightness_slider,
                              lv_color_hex(0x5A5A5A),
                              LV_PART_INDICATOR | LV_STATE_DISABLED);
    lv_obj_set_style_bg_color(ctx->settings_ui.night_brightness_slider,
                              lv_color_hex(0xA8A8A8),
                              LV_PART_KNOB | LV_STATE_DISABLED);
    lv_obj_add_event_cb(ctx->settings_ui.night_brightness_slider, night_brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, ctx);
    ui_attach_click_feedback(ctx->settings_ui.night_brightness_slider, LV_EVENT_VALUE_CHANGED);

    card = create_card(parent);
    ctx->settings_ui.night_sunrise_card = card;
    lv_obj_set_width(card, 540);
    lv_obj_set_style_pad_all(card, 24, 0);
    lv_obj_set_style_pad_row(card, 14, 0);
    style_night_detail_card(card);
    row = create_labeled_trailing_control_row(card,
                                              "Sunrise ramp",
                                              "Brighten during the pre-alarm sunrise",
                                              360,
                                              NULL);

    ctx->settings_ui.night_sunrise_sw = lv_switch_create(row);
    style_settings_switch(ctx->settings_ui.night_sunrise_sw);
    lv_obj_add_flag(ctx->settings_ui.night_sunrise_sw, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(ctx->settings_ui.night_sunrise_sw,
                        night_control_scroll_passthrough_event_cb,
                        LV_EVENT_PRESSED,
                        ctx);
    lv_obj_add_event_cb(ctx->settings_ui.night_sunrise_sw,
                        night_control_scroll_passthrough_event_cb,
                        LV_EVENT_PRESSING,
                        ctx);
    lv_obj_add_event_cb(ctx->settings_ui.night_sunrise_sw,
                        night_control_scroll_passthrough_event_cb,
                        LV_EVENT_RELEASED,
                        ctx);
    lv_obj_add_event_cb(ctx->settings_ui.night_sunrise_sw,
                        night_control_scroll_passthrough_event_cb,
                        LV_EVENT_PRESS_LOST,
                        ctx);
    lv_obj_add_event_cb(ctx->settings_ui.night_sunrise_sw,
                        night_sunrise_enabled_event_cb,
                        LV_EVENT_VALUE_CHANGED,
                        ctx);
    ui_attach_click_feedback(ctx->settings_ui.night_sunrise_sw, LV_EVENT_VALUE_CHANGED);
}

static void create_night_face_picker_overlay(clock_ui_context_t *ctx)
{
    ui_surface_t surface;
    lv_obj_t *bottom_sensor;
    lv_obj_t *title;
    int visible_count = clock_face_visible_count();

    ui_surface_create_fullscreen(&surface,
                                 ctx->screen,
                                 lv_color_black(),
                                 LV_OPA_70,
                                 lv_color_hex(0x0D0D0D),
                                 SETTINGS_HEADER_HEIGHT,
                                 "Choose night face",
                                 night_face_picker_overlay_event_cb,
                                 night_face_picker_close_event_cb,
                                 ctx);
    ctx->settings_ui.night_face_picker_overlay = surface.overlay;
    ctx->settings_ui.night_face_picker_content = surface.content;
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

        ctx->settings_ui.face_ctx[face].ui = ctx;
        ctx->settings_ui.face_ctx[face].face = face;
        ctx->settings_ui.night_face_picker_card[face] = card;

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
        lv_obj_add_event_cb(card, night_face_picker_item_event_cb, LV_EVENT_CLICKED, &ctx->settings_ui.face_ctx[face]);
        ui_attach_click_feedback(card, LV_EVENT_CLICKED);

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
        ctx->settings_ui.night_face_picker_preview[face] = preview;
        lv_obj_add_flag(preview, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_size(preview, 96, 96);
        lv_obj_set_style_bg_opa(preview, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(preview, 0, 0);
        lv_obj_set_style_pad_all(preview, 0, 0);
        lv_obj_clear_flag(preview, LV_OBJ_FLAG_SCROLLABLE);
        lv_image_set_inner_align(preview, LV_IMAGE_ALIGN_CENTER);
        lv_obj_center(preview);

        label = lv_label_create(card);
        ctx->settings_ui.night_face_picker_label[face] = label;
        lv_obj_add_flag(label, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_width(label, lv_pct(100));
        lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_label_set_text(label, clock_face_name(face));
    }

    ui_surface_create_edge_sensor(ctx->settings_ui.night_face_picker_overlay,
                                  &bottom_sensor,
                                  UI_SURFACE_EDGE_BOTTOM,
                                  ALARM_CLOSE_BOTTOM_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  settings_edge_ctx(ctx, UI_SURFACE_EDGE_BOTTOM));

    ctx->settings_ui.night_face_render_canvas = lv_image_create(ctx->screen);
    lv_obj_set_pos(ctx->settings_ui.night_face_render_canvas, -2000, -2000);
    lv_obj_clear_flag(ctx->settings_ui.night_face_render_canvas, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(ctx->settings_ui.night_face_render_canvas, LV_OBJ_FLAG_CLICKABLE);
}

static void create_night_schedule_overlay(clock_ui_context_t *ctx)
{
    ui_surface_t surface;
    lv_obj_t *sensor;
    lv_obj_t *title;
    lv_obj_t *card;
    lv_obj_t *row;
    lv_obj_t *label;

    ui_surface_create_fullscreen(&surface,
                                 ctx->screen,
                                 lv_color_black(),
                                 LV_OPA_70,
                                 lv_color_hex(0x0D0D0D),
                                 SETTINGS_HEADER_HEIGHT,
                                 "Night schedule",
                                 night_schedule_editor_overlay_event_cb,
                                 night_schedule_editor_close_event_cb,
                                 ctx);
    ctx->settings_ui.night_schedule_overlay = surface.overlay;
    ctx->settings_ui.night_schedule_content = surface.content;
    title = surface.title;
    lv_obj_set_width(title, lv_pct(100));
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);
    lv_obj_set_flex_align(surface.content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(surface.content, LV_OBJ_FLAG_SCROLLABLE);

    card = create_card(surface.content);
    lv_obj_set_width(card, 540);
    lv_obj_set_style_pad_all(card, 24, 0);
    lv_obj_set_style_pad_row(card, 16, 0);

    label = lv_label_create(card);
    style_centered_label(label, &lv_font_montserrat_24, lv_color_white());
    lv_label_set_text(label, "Start");

    row = create_row(card);
    lv_obj_set_width(row, LV_SIZE_CONTENT);
    center_row(row);
    lv_obj_set_style_pad_column(row, 14, 0);
    ctx->settings_ui.night_start_hour_dd = create_time_roller(row, ctx->hour_options, 164, night_hour_minute_event_cb, ctx);
    ctx->settings_ui.night_start_min_dd = create_time_roller(row, ctx->minute_options, 164, night_hour_minute_event_cb, ctx);

    label = lv_label_create(card);
    style_centered_label(label, &lv_font_montserrat_24, lv_color_white());
    lv_label_set_text(label, "End");

    row = create_row(card);
    lv_obj_set_width(row, LV_SIZE_CONTENT);
    center_row(row);
    lv_obj_set_style_pad_column(row, 14, 0);
    ctx->settings_ui.night_end_hour_dd = create_time_roller(row, ctx->hour_options, 164, night_hour_minute_event_cb, ctx);
    ctx->settings_ui.night_end_min_dd = create_time_roller(row, ctx->minute_options, 164, night_hour_minute_event_cb, ctx);

    row = create_row(card);
    center_row(row);
    create_action_button(row, "Done", night_schedule_editor_close_event_cb, ctx);

    ui_surface_create_edge_sensor(ctx->settings_ui.night_schedule_overlay,
                                  &sensor,
                                  UI_SURFACE_EDGE_TOP,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  settings_edge_ctx(ctx, UI_SURFACE_EDGE_TOP));
    ui_surface_create_edge_sensor(ctx->settings_ui.night_schedule_overlay,
                                  &sensor,
                                  UI_SURFACE_EDGE_BOTTOM,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  settings_edge_ctx(ctx, UI_SURFACE_EDGE_BOTTOM));
    ui_surface_create_edge_sensor(ctx->settings_ui.night_schedule_overlay,
                                  &sensor,
                                  UI_SURFACE_EDGE_LEFT,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  settings_edge_ctx(ctx, UI_SURFACE_EDGE_LEFT));
    ui_surface_create_edge_sensor(ctx->settings_ui.night_schedule_overlay,
                                  &sensor,
                                  UI_SURFACE_EDGE_RIGHT,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  settings_edge_ctx(ctx, UI_SURFACE_EDGE_RIGHT));
}

static void create_wifi_dialog(clock_ui_context_t *ctx)
{
    lv_obj_t *panel;
    lv_obj_t *row;

    ctx->settings_ui.wifi_dialog_overlay = lv_obj_create(ctx->screen);
    lv_obj_set_size(ctx->settings_ui.wifi_dialog_overlay, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_color(ctx->settings_ui.wifi_dialog_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(ctx->settings_ui.wifi_dialog_overlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(ctx->settings_ui.wifi_dialog_overlay, 0, 0);
    lv_obj_set_style_radius(ctx->settings_ui.wifi_dialog_overlay, 0, 0);
    lv_obj_set_style_pad_all(ctx->settings_ui.wifi_dialog_overlay, 0, 0);
    lv_obj_add_flag(ctx->settings_ui.wifi_dialog_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(ctx->settings_ui.wifi_dialog_overlay, wifi_dialog_close_event_cb, LV_EVENT_CLICKED, ctx);

    panel = lv_obj_create(ctx->settings_ui.wifi_dialog_overlay);
    ctx->settings_ui.wifi_dialog = panel;
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

    ctx->settings_ui.wifi_dialog_title = lv_label_create(panel);
    style_centered_label(ctx->settings_ui.wifi_dialog_title, &lv_font_montserrat_24, lv_color_white());
    lv_label_set_text(ctx->settings_ui.wifi_dialog_title, "Join network");

    ctx->settings_ui.wifi_password_ta = lv_textarea_create(panel);
    lv_obj_set_width(ctx->settings_ui.wifi_password_ta, lv_pct(100));
    lv_obj_set_style_bg_color(ctx->settings_ui.wifi_password_ta, lv_color_hex(0x232323), 0);
    lv_obj_set_style_bg_opa(ctx->settings_ui.wifi_password_ta, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(ctx->settings_ui.wifi_password_ta, lv_color_white(), 0);
    lv_obj_set_style_text_color(ctx->settings_ui.wifi_password_ta, lv_color_hex(0x8E8E8E), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_border_width(ctx->settings_ui.wifi_password_ta, 0, 0);
    lv_obj_set_style_radius(ctx->settings_ui.wifi_password_ta, 16, 0);
    lv_textarea_set_placeholder_text(ctx->settings_ui.wifi_password_ta, "Password");
    lv_textarea_set_password_mode(ctx->settings_ui.wifi_password_ta, true);
    lv_textarea_set_one_line(ctx->settings_ui.wifi_password_ta, true);
    lv_obj_add_event_cb(ctx->settings_ui.wifi_password_ta, wifi_ta_focus_event_cb, LV_EVENT_FOCUSED, ctx);

    row = create_row(panel);
    center_row(row);
    create_action_button(row, "Cancel", wifi_dialog_close_event_cb, ctx);
    create_action_button(row, "Connect", wifi_dialog_connect_event_cb, ctx);

    ctx->settings_ui.wifi_keyboard = lv_keyboard_create(ctx->settings_ui.wifi_dialog_overlay);
    lv_obj_set_size(ctx->settings_ui.wifi_keyboard, KEYBOARD_WIDTH, KEYBOARD_HEIGHT);
    lv_obj_align(ctx->settings_ui.wifi_keyboard, LV_ALIGN_BOTTOM_MID, 0, -KEYBOARD_BOTTOM_INSET);
    lv_obj_set_style_radius(ctx->settings_ui.wifi_keyboard, 28, 0);
    lv_obj_set_style_bg_color(ctx->settings_ui.wifi_keyboard, lv_color_hex(0x151515), 0);
    lv_obj_set_style_bg_opa(ctx->settings_ui.wifi_keyboard, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ctx->settings_ui.wifi_keyboard, 0, 0);
    lv_obj_add_flag(ctx->settings_ui.wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(ctx->settings_ui.wifi_keyboard, wifi_keyboard_event_cb, LV_EVENT_READY, ctx);
    lv_obj_add_event_cb(ctx->settings_ui.wifi_keyboard, wifi_keyboard_event_cb, LV_EVENT_CANCEL, ctx);
}

static void create_wifi_overlay(clock_ui_context_t *ctx)
{
    ui_surface_t surface;
    lv_obj_t *title;

    ui_surface_create_fullscreen(&surface,
                                 ctx->screen,
                                 lv_color_black(),
                                 LV_OPA_COVER,
                                 lv_color_hex(0x0D0D0D),
                                 SETTINGS_HEADER_HEIGHT,
                                 "Wi-Fi",
                                 settings_detail_overlay_event_cb,
                                 settings_back_event_cb,
                                 ctx);
    ctx->settings_ui.wifi_overlay = surface.overlay;
    ctx->settings_ui.wifi_content = surface.content;
    lv_obj_add_event_cb(surface.content, settings_content_scroll_event_cb, LV_EVENT_SCROLL_BEGIN, ctx);
    lv_obj_add_event_cb(surface.content, settings_content_scroll_event_cb, LV_EVENT_SCROLL, ctx);
    lv_obj_add_event_cb(surface.content, settings_content_scroll_event_cb, LV_EVENT_SCROLL_END, ctx);
    title = surface.title;
    lv_obj_set_width(title, lv_pct(100));
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    create_wifi_card(ctx, surface.content);
    create_networks_card(ctx, surface.content);

    ui_surface_create_edge_sensor(ctx->settings_ui.wifi_overlay,
                                  &ctx->settings_ui.wifi_top_sensor,
                                  UI_SURFACE_EDGE_TOP,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  settings_edge_ctx(ctx, UI_SURFACE_EDGE_TOP));
    ui_surface_create_edge_sensor(ctx->settings_ui.wifi_overlay,
                                  &ctx->settings_ui.wifi_bottom_sensor,
                                  UI_SURFACE_EDGE_BOTTOM,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  settings_edge_ctx(ctx, UI_SURFACE_EDGE_BOTTOM));
    ui_surface_create_edge_sensor(ctx->settings_ui.wifi_overlay,
                                  &ctx->settings_ui.wifi_left_sensor,
                                  UI_SURFACE_EDGE_LEFT,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  settings_edge_ctx(ctx, UI_SURFACE_EDGE_LEFT));
    ui_surface_create_edge_sensor(ctx->settings_ui.wifi_overlay,
                                  &ctx->settings_ui.wifi_right_sensor,
                                  UI_SURFACE_EDGE_RIGHT,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  settings_edge_ctx(ctx, UI_SURFACE_EDGE_RIGHT));
}

static void create_other_overlay(clock_ui_context_t *ctx)
{
    ui_surface_t surface;
    lv_obj_t *title;

    ui_surface_create_fullscreen(&surface,
                                 ctx->screen,
                                 lv_color_black(),
                                 LV_OPA_COVER,
                                 lv_color_hex(0x0D0D0D),
                                 SETTINGS_HEADER_HEIGHT,
                                 "Other",
                                 settings_detail_overlay_event_cb,
                                 settings_back_event_cb,
                                 ctx);
    ctx->settings_ui.other_overlay = surface.overlay;
    ctx->settings_ui.other_content = surface.content;
    title = surface.title;
    lv_obj_set_width(title, lv_pct(100));
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);
    lv_obj_set_flex_align(surface.content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(surface.content, LV_OBJ_FLAG_SCROLLABLE);

    create_timezone_card(ctx, surface.content);

    ui_surface_create_edge_sensor(ctx->settings_ui.other_overlay,
                                  &ctx->settings_ui.other_top_sensor,
                                  UI_SURFACE_EDGE_TOP,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  settings_edge_ctx(ctx, UI_SURFACE_EDGE_TOP));
    ui_surface_create_edge_sensor(ctx->settings_ui.other_overlay,
                                  &ctx->settings_ui.other_bottom_sensor,
                                  UI_SURFACE_EDGE_BOTTOM,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  settings_edge_ctx(ctx, UI_SURFACE_EDGE_BOTTOM));
    ui_surface_create_edge_sensor(ctx->settings_ui.other_overlay,
                                  &ctx->settings_ui.other_left_sensor,
                                  UI_SURFACE_EDGE_LEFT,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  settings_edge_ctx(ctx, UI_SURFACE_EDGE_LEFT));
    ui_surface_create_edge_sensor(ctx->settings_ui.other_overlay,
                                  &ctx->settings_ui.other_right_sensor,
                                  UI_SURFACE_EDGE_RIGHT,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  settings_edge_ctx(ctx, UI_SURFACE_EDGE_RIGHT));
}

static void create_night_overlay(clock_ui_context_t *ctx)
{
    ui_surface_t surface;
    lv_obj_t *title;

    ui_surface_create_fullscreen(&surface,
                                 ctx->screen,
                                 lv_color_black(),
                                 LV_OPA_COVER,
                                 lv_color_hex(0x0D0D0D),
                                 SETTINGS_HEADER_HEIGHT,
                                 "Night mode",
                                 settings_detail_overlay_event_cb,
                                 settings_back_event_cb,
                                 ctx);
    ctx->settings_ui.night_overlay = surface.overlay;
    ctx->settings_ui.night_content = surface.content;
    lv_obj_set_flex_align(surface.content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_bottom(surface.content, 72, 0);
    lv_obj_add_flag(surface.content, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_add_event_cb(surface.content, settings_content_scroll_event_cb, LV_EVENT_SCROLL_BEGIN, ctx);
    lv_obj_add_event_cb(surface.content, settings_content_scroll_event_cb, LV_EVENT_SCROLL, ctx);
    lv_obj_add_event_cb(surface.content, settings_content_scroll_event_cb, LV_EVENT_SCROLL_END, ctx);
    title = surface.title;
    lv_obj_set_width(title, lv_pct(100));
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    create_night_card(ctx, surface.content);

    ui_surface_create_edge_sensor(ctx->settings_ui.night_overlay,
                                  &ctx->settings_ui.night_top_sensor,
                                  UI_SURFACE_EDGE_TOP,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  settings_edge_ctx(ctx, UI_SURFACE_EDGE_TOP));
    ui_surface_create_edge_sensor(ctx->settings_ui.night_overlay,
                                  &ctx->settings_ui.night_bottom_sensor,
                                  UI_SURFACE_EDGE_BOTTOM,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  settings_edge_ctx(ctx, UI_SURFACE_EDGE_BOTTOM));
    ui_surface_create_edge_sensor(ctx->settings_ui.night_overlay,
                                  &ctx->settings_ui.night_left_sensor,
                                  UI_SURFACE_EDGE_LEFT,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  settings_edge_ctx(ctx, UI_SURFACE_EDGE_LEFT));
    ui_surface_create_edge_sensor(ctx->settings_ui.night_overlay,
                                  &ctx->settings_ui.night_right_sensor,
                                  UI_SURFACE_EDGE_RIGHT,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  settings_edge_ctx(ctx, UI_SURFACE_EDGE_RIGHT));
}

void create_settings_overlay(clock_ui_context_t *ctx)
{
    ui_surface_t surface;
    lv_obj_t *title;
    static const ui_surface_edge_t s_edges[] = {
        UI_SURFACE_EDGE_TOP,
        UI_SURFACE_EDGE_BOTTOM,
        UI_SURFACE_EDGE_LEFT,
        UI_SURFACE_EDGE_RIGHT,
    };

    for (size_t i = 0; i < (sizeof(s_edges) / sizeof(s_edges[0])); ++i) {
        ctx->settings_ui.close_edge_ctx[s_edges[i]].ui = ctx;
        ctx->settings_ui.close_edge_ctx[s_edges[i]].edge = s_edges[i];
    }

    ui_surface_create_fullscreen(&surface,
                                 ctx->screen,
                                 lv_color_black(),
                                 LV_OPA_COVER,
                                 lv_color_hex(0x0D0D0D),
                                 SETTINGS_HEADER_HEIGHT,
                                 "Settings",
                                 settings_overlay_event_cb,
                                 settings_close_event_cb,
                                 ctx);
    ctx->settings_ui.overlay = surface.overlay;
    ctx->settings_ui.panel = surface.panel;
    ctx->settings_ui.content = surface.content;
    title = surface.title;
    lv_obj_set_width(title, lv_pct(100));
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);
    lv_obj_set_flex_align(surface.content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(surface.content, LV_OBJ_FLAG_SCROLLABLE);

    create_settings_menu_entry(surface.content,
                               LV_SYMBOL_WIFI,
                               "Wi-Fi",
                               "Networks and clock sync",
                               settings_wifi_entry_event_cb,
                               ctx);
    create_settings_menu_entry(surface.content,
                               LV_SYMBOL_SETTINGS,
                               "Night mode",
                               "Schedule and night face",
                               settings_night_entry_event_cb,
                               ctx);
    create_settings_menu_entry(surface.content,
                               LV_SYMBOL_SETTINGS,
                               "Other",
                               "Time zone",
                               settings_other_entry_event_cb,
                               ctx);

    ui_surface_create_edge_sensor(ctx->settings_ui.overlay,
                                  &ctx->settings_ui.top_sensor,
                                  UI_SURFACE_EDGE_TOP,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  settings_edge_ctx(ctx, UI_SURFACE_EDGE_TOP));
    ui_surface_create_edge_sensor(ctx->settings_ui.overlay,
                                  &ctx->settings_ui.bottom_sensor,
                                  UI_SURFACE_EDGE_BOTTOM,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  settings_edge_ctx(ctx, UI_SURFACE_EDGE_BOTTOM));
    ui_surface_create_edge_sensor(ctx->settings_ui.overlay,
                                  &ctx->settings_ui.left_sensor,
                                  UI_SURFACE_EDGE_LEFT,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  settings_edge_ctx(ctx, UI_SURFACE_EDGE_LEFT));
    ui_surface_create_edge_sensor(ctx->settings_ui.overlay,
                                  &ctx->settings_ui.right_sensor,
                                  UI_SURFACE_EDGE_RIGHT,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  settings_close_swipe_event_cb,
                                  settings_edge_ctx(ctx, UI_SURFACE_EDGE_RIGHT));

    create_wifi_overlay(ctx);
    create_other_overlay(ctx);
    create_night_overlay(ctx);
    create_night_schedule_overlay(ctx);
    create_night_face_picker_overlay(ctx);
    create_wifi_dialog(ctx);
}
