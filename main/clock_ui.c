#include "ui/clock_ui_private.h"

static clock_ui_context_t s_ctx = {0};

lv_style_t s_style_hour;
lv_style_t s_style_min;
lv_style_t s_style_sec;

const uint8_t s_matrix_font[10][MTX_DIGIT_H] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F},
    {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E},
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E},
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},
};

const uint8_t s_wharton_font[10][WH_DIGIT_ROWS] = {
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F},
    {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E},
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E},
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},
};

const uint8_t s_matrix_digit_col[4] = {3, 9, 19, 25};
const uint8_t s_matrix_colon_col = 16;
const uint8_t s_matrix_digit_row0 = (MTX_GRID_Y - MTX_DIGIT_H) / 2;
const char *s_day_short[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char *s_month_short[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

static void styles_init(void)
{
    lv_style_init(&s_style_hour);
    lv_style_set_line_width(&s_style_hour, 8);
    lv_style_set_line_color(&s_style_hour, lv_color_white());
    lv_style_set_line_rounded(&s_style_hour, true);

    lv_style_init(&s_style_min);
    lv_style_set_line_width(&s_style_min, 5);
    lv_style_set_line_color(&s_style_min, lv_color_white());
    lv_style_set_line_rounded(&s_style_min, true);

    lv_style_init(&s_style_sec);
    lv_style_set_line_width(&s_style_sec, 2);
    lv_style_set_line_color(&s_style_sec, lv_color_hex(0xFF4444));
    lv_style_set_line_rounded(&s_style_sec, true);
}

static void build_hour_options(char *buffer, size_t size)
{
    size_t pos = 0;

    buffer[0] = '\0';
    for (int hour = 0; hour < 24; ++hour) {
        pos += snprintf(buffer + pos, size - pos, "%02d%s", hour, (hour == 23) ? "" : "\n");
    }
}

static void build_minute_options(char *buffer, size_t size)
{
    size_t pos = 0;

    buffer[0] = '\0';
    for (int minute = 0; minute < 60; ++minute) {
        pos += snprintf(buffer + pos, size - pos, "%02d%s", minute, (minute == 59) ? "" : "\n");
    }
}

static void build_timezone_options(char *buffer, size_t size)
{
    size_t pos = 0;

    buffer[0] = '\0';
    for (int tz = -12; tz <= 14; ++tz) {
        pos += snprintf(buffer + pos, size - pos, "UTC%+d%s", tz, (tz == 14) ? "" : "\n");
    }
}

static void build_face_options(char *buffer, size_t size)
{
    size_t pos = 0;
    int visible_count = clock_face_visible_count();

    buffer[0] = '\0';
    for (int index = 0; index < visible_count; ++index) {
        clock_face_id_t face = clock_face_visible_index_to_id(index);

        pos += snprintf(buffer + pos, size - pos, "%s%s",
                        clock_face_name(face),
                        (index == (visible_count - 1)) ? "" : "\n");
    }
}

static void prewarm_face_previews(clock_ui_context_t *ctx)
{
    clock_face_id_t active_face;
    int visible_count;

    if (ctx->screen == NULL) {
        return;
    }

    lv_obj_update_layout(ctx->screen);

    visible_count = clock_face_visible_count();
    for (int index = 0; index < visible_count; ++index) {
        clock_face_id_t face = clock_face_visible_index_to_id(index);

        update_face(ctx, face);
    }

    refresh_digital_face_snapshot(ctx);

    active_face = ctx->runtime->in_night_mode ? ctx->settings->night_mode.face : ctx->settings->current_face;
    set_active_face(ctx, active_face, LV_ANIM_OFF);
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

void request_set_base_brightness(clock_ui_context_t *ctx, uint8_t hw_percent)
{
    if (ctx->callbacks.on_set_base_brightness != NULL) {
        ctx->callbacks.on_set_base_brightness(ctx->user_ctx, hw_percent);
    }
}

void request_set_runtime_night_brightness(clock_ui_context_t *ctx, uint8_t hw_percent)
{
    if (ctx->callbacks.on_set_runtime_night_brightness != NULL) {
        ctx->callbacks.on_set_runtime_night_brightness(ctx->user_ctx, hw_percent);
    }
}

void request_set_current_face(clock_ui_context_t *ctx, clock_face_id_t face)
{
    if (ctx->callbacks.on_set_current_face != NULL) {
        ctx->callbacks.on_set_current_face(ctx->user_ctx, face);
    }
}

void request_set_night_face(clock_ui_context_t *ctx, clock_face_id_t face)
{
    if (ctx->callbacks.on_set_night_face != NULL) {
        ctx->callbacks.on_set_night_face(ctx->user_ctx, face);
    }
}

void request_set_timezone(clock_ui_context_t *ctx, int8_t utc_offset_hours)
{
    if (ctx->callbacks.on_set_timezone != NULL) {
        ctx->callbacks.on_set_timezone(ctx->user_ctx, utc_offset_hours);
    }
}

void request_save_wifi_credentials(clock_ui_context_t *ctx, const char *ssid, const char *password)
{
    if (ctx->callbacks.on_save_wifi_credentials != NULL) {
        ctx->callbacks.on_save_wifi_credentials(ctx->user_ctx, ssid, password);
    }
}

void request_set_night_mode_enabled(clock_ui_context_t *ctx, bool enabled)
{
    if (ctx->callbacks.on_set_night_mode_enabled != NULL) {
        ctx->callbacks.on_set_night_mode_enabled(ctx->user_ctx, enabled);
    }
}

void request_set_night_schedule(clock_ui_context_t *ctx,
                                uint8_t start_hour,
                                uint8_t start_minute,
                                uint8_t end_hour,
                                uint8_t end_minute)
{
    if (ctx->callbacks.on_set_night_schedule != NULL) {
        ctx->callbacks.on_set_night_schedule(ctx->user_ctx, start_hour, start_minute, end_hour, end_minute);
    }
}

void request_set_night_brightness(clock_ui_context_t *ctx, uint8_t hw_percent)
{
    if (ctx->callbacks.on_set_night_brightness != NULL) {
        ctx->callbacks.on_set_night_brightness(ctx->user_ctx, hw_percent);
    }
}

void request_set_alarm_volume(clock_ui_context_t *ctx, uint8_t volume)
{
    if (ctx->callbacks.on_set_alarm_volume != NULL) {
        ctx->callbacks.on_set_alarm_volume(ctx->user_ctx, volume);
    }
}

void request_set_snooze_minutes(clock_ui_context_t *ctx, uint8_t minutes)
{
    if (ctx->callbacks.on_set_snooze_minutes != NULL) {
        ctx->callbacks.on_set_snooze_minutes(ctx->user_ctx, minutes);
    }
}

void request_set_alarm_enabled(clock_ui_context_t *ctx, uint8_t alarm_index, bool enabled)
{
    if (ctx->callbacks.on_set_alarm_enabled != NULL) {
        ctx->callbacks.on_set_alarm_enabled(ctx->user_ctx, alarm_index, enabled);
    }
}

void request_save_alarm(clock_ui_context_t *ctx, uint8_t alarm_index, const alarm_config_t *alarm)
{
    if (ctx->callbacks.on_save_alarm != NULL) {
        ctx->callbacks.on_save_alarm(ctx->user_ctx, alarm_index, alarm);
    }
}

void request_delete_alarm(clock_ui_context_t *ctx, uint8_t alarm_index)
{
    if (ctx->callbacks.on_delete_alarm != NULL) {
        ctx->callbacks.on_delete_alarm(ctx->user_ctx, alarm_index);
    }
}

void set_root_ui_hidden(clock_ui_context_t *ctx, bool hidden)
{
    if (ctx->tileview != NULL) {
        if (hidden) {
            lv_obj_add_flag(ctx->tileview, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(ctx->tileview, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (ctx->settings_button != NULL) {
        if (hidden) {
            lv_obj_add_flag(ctx->settings_button, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(ctx->settings_button, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (ctx->faces.face_swipe_layer != NULL) {
        if (hidden) {
            lv_obj_add_flag(ctx->faces.face_swipe_layer, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(ctx->faces.face_swipe_layer, LV_OBJ_FLAG_HIDDEN);
        }
    }

    for (int face = 0; face < CLOCK_FACE_COUNT; ++face) {
        if (ctx->page_dots[face] == NULL) {
            continue;
        }

        if (hidden) {
            lv_obj_add_flag(ctx->page_dots[face], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(ctx->page_dots[face], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

clock_face_id_t sanitize_enabled_face(clock_face_id_t face)
{
    if (!clock_face_is_enabled(face)) {
        return clock_face_first_enabled();
    }

    return face;
}

struct tm get_local_time_now(void)
{
    time_t now;
    struct tm local_tm;

    time(&now);
    localtime_r(&now, &local_tm);
    return local_tm;
}

void hand_endpoint(int cx, int cy, int length, float angle_deg, lv_point_precise_t *p0, lv_point_precise_t *p1)
{
    float rad = (angle_deg - 90.0f) * (M_PI / 180.0f);

    p0->x = cx;
    p0->y = cy;
    p1->x = cx + (int)(length * cosf(rad));
    p1->y = cy + (int)(length * sinf(rad));
}

void hand_line_endpoints(int cx, int cy, int tail_length, int head_length,
                         float angle_deg, lv_point_precise_t *p0, lv_point_precise_t *p1)
{
    float rad = (angle_deg - 90.0f) * (M_PI / 180.0f);

    p0->x = cx - (int)(tail_length * cosf(rad));
    p0->y = cy - (int)(tail_length * sinf(rad));
    p1->x = cx + (int)(head_length * cosf(rad));
    p1->y = cy + (int)(head_length * sinf(rad));
}

uint8_t brightness_ui_to_hw(int ui_percent)
{
    return brightness_policy_ui_to_hw((uint8_t)clamp_brightness_ui(ui_percent));
}

uint8_t brightness_hw_to_ui(int hw_percent)
{
    return brightness_policy_hw_to_ui((uint8_t)clamp_brightness(hw_percent));
}

bool alarm_surface_is_open(const clock_ui_context_t *ctx)
{
    return ctx->alarms.open || ctx->alarms.editor_open || ctx->alarms.settings_open;
}

void format_alarm_time(char *buffer, size_t size, uint8_t hour, uint8_t minute)
{
    snprintf(buffer, size, "%02u:%02u", hour, minute);
}

void format_alarm_repeat_summary(char *buffer, size_t size, const alarm_config_t *alarm)
{
    static const uint8_t s_day_display_order[7] = {1, 2, 3, 4, 5, 6, 0};
    size_t pos = 0;

    if (alarm->repeat_mode == ALARM_REPEAT_ONCE) {
        snprintf(buffer, size, "One time");
        return;
    }

    if (alarm->days_mask == 0x7F) {
        snprintf(buffer, size, "Every day");
        return;
    }

    if (alarm->days_mask == 0x3E) {
        snprintf(buffer, size, "Weekdays");
        return;
    }

    if (alarm->days_mask == 0x41) {
        snprintf(buffer, size, "Weekends");
        return;
    }

    buffer[0] = '\0';
    for (int idx = 0; idx < 7; ++idx) {
        int day = s_day_display_order[idx];

        if ((alarm->days_mask & (1U << day)) == 0) {
            continue;
        }

        if (buffer[0] != '\0' && pos < size) {
            pos += snprintf(buffer + pos, size - pos, " ");
        }
        if (pos < size) {
            pos += snprintf(buffer + pos, size - pos, "%s", s_day_short[day]);
        }
    }
}

int count_enabled_alarms(const clock_ui_context_t *ctx)
{
    int count = 0;

    for (int i = 0; i < MAX_ALARMS; ++i) {
        if (ctx->settings->alarms[i].enabled) {
            ++count;
        }
    }

    return count;
}

int find_alarm_slot_for_new_alarm(const clock_ui_context_t *ctx)
{
    for (int i = 0; i < MAX_ALARMS; ++i) {
        if (!ctx->settings->alarms[i].enabled) {
            return i;
        }
    }

    return -1;
}

uint8_t alarm_repeat_preset_from_config(const alarm_config_t *alarm)
{
    if (alarm->repeat_mode == ALARM_REPEAT_ONCE) {
        return ALARM_REPEAT_PRESET_ONCE;
    }
    if (alarm->days_mask == 0x7F) {
        return ALARM_REPEAT_PRESET_EVERY_DAY;
    }
    if (alarm->days_mask == 0x3E) {
        return ALARM_REPEAT_PRESET_WEEKDAYS;
    }
    if (alarm->days_mask == 0x41) {
        return ALARM_REPEAT_PRESET_WEEKENDS;
    }

    return 0xFF;
}

void apply_repeat_preset_to_alarm(alarm_config_t *alarm, uint8_t preset)
{
    switch (preset) {
    case ALARM_REPEAT_PRESET_ONCE:
        alarm->repeat_mode = ALARM_REPEAT_ONCE;
        alarm->days_mask = 0x7F;
        break;
    case ALARM_REPEAT_PRESET_EVERY_DAY:
        alarm->repeat_mode = ALARM_REPEAT_WEEKLY;
        alarm->days_mask = 0x7F;
        break;
    case ALARM_REPEAT_PRESET_WEEKDAYS:
        alarm->repeat_mode = ALARM_REPEAT_WEEKLY;
        alarm->days_mask = 0x3E;
        break;
    case ALARM_REPEAT_PRESET_WEEKENDS:
        alarm->repeat_mode = ALARM_REPEAT_WEEKLY;
        alarm->days_mask = 0x41;
        break;
    default:
        break;
    }
}

esp_err_t clock_ui_init(const app_settings_t *settings,
                        const app_runtime_state_t *runtime,
                        const clock_ui_callbacks_t *callbacks,
                        void *user_ctx)
{
    clock_ui_context_t *ctx = &s_ctx;

    memset(&s_ctx, 0, sizeof(s_ctx));
    ctx->settings = settings;
    ctx->runtime = runtime;
    ctx->user_ctx = user_ctx;
    if (callbacks != NULL) {
        ctx->callbacks = *callbacks;
    }

    build_hour_options(ctx->hour_options, sizeof(ctx->hour_options));
    build_minute_options(ctx->minute_options, sizeof(ctx->minute_options));
    build_timezone_options(ctx->timezone_options, sizeof(ctx->timezone_options));
    build_face_options(ctx->face_options, sizeof(ctx->face_options));
    styles_init();
    build_root_ui(ctx);
    prewarm_face_previews(ctx);
    ctx->affordance_hide_timer = lv_timer_create(affordance_hide_timer_cb, AFFORDANCE_VISIBLE_MS, ctx);
    lv_timer_pause(ctx->affordance_hide_timer);
    refresh_settings_controls(ctx);
    update_brightness_ui(ctx);
    show_affordances_temporarily(ctx);
    return ESP_OK;
}

void clock_ui_refresh(void)
{
    clock_ui_context_t *ctx = &s_ctx;
    clock_face_id_t current_face;
    clock_face_id_t night_face;

    refresh_settings_controls(ctx);
    update_brightness_ui(ctx);

    current_face = sanitize_enabled_face(ctx->settings->current_face);
    night_face = sanitize_enabled_face(ctx->settings->night_mode.face);
    set_active_face(ctx, ctx->runtime->in_night_mode ? night_face : current_face, LV_ANIM_OFF);
}

void clock_ui_tick(time_t now)
{
    clock_ui_context_t *ctx = &s_ctx;
    clock_face_id_t desired_face =
        ctx->runtime->in_night_mode ? sanitize_enabled_face(ctx->settings->night_mode.face)
                                    : sanitize_enabled_face(ctx->settings->current_face);
    clock_face_id_t active_face;
    bool opaque_menu_open = ctx->settings_ui.open ||
                            ctx->alarms.open ||
                            ctx->alarms.editor_open ||
                            ctx->alarms.settings_open;

    if (tile_to_face(ctx, lv_tileview_get_tile_active(ctx->tileview)) != desired_face) {
        set_active_face(ctx, desired_face, LV_ANIM_OFF);
    }
    active_face = tile_to_face(ctx, lv_tileview_get_tile_active(ctx->tileview));
    if (!opaque_menu_open && !ctx->faces.tileview_scrolling) {
        update_face(ctx, active_face);
        update_dots(ctx, active_face);
    }

    if (!opaque_menu_open) {
        update_alarm_banner(ctx, now);
    }
    sync_alarm_overlay(ctx, now);
    if ((ctx->alarms.open || ctx->alarms.settings_open) &&
        !ctx->alarms.editor_open &&
        !ctx->alarms.management_scrolling &&
        alarm_controls_need_sync(ctx)) {
        sync_alarm_controls(ctx);
    }
    if ((ctx->settings_ui.wifi_open || ctx->settings_ui.other_open) &&
        !ctx->settings_ui.wifi_scrolling &&
        wifi_controls_need_sync(ctx)) {
        sync_wifi_controls(ctx);
    }
    if (ctx->settings_ui.night_open && !ctx->settings_ui.night_scrolling && night_controls_need_sync(ctx)) {
        sync_night_controls(ctx);
    }
    if (ctx->brightness.animating ||
        ctx->brightness.dragging ||
        brightness_panel_is_open(ctx) ||
        (ctx->brightness.overlay != NULL && !lv_obj_has_flag(ctx->brightness.overlay, LV_OBJ_FLAG_HIDDEN))) {
        update_brightness_ui(ctx);
    }
}
