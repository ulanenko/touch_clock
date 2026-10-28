#include "ui/clock_ui_private.h"
#include "ui/ui_areas.h"
#include "domain/timezone_rules.h"

#if CONFIG_TOUCH_CLOCK_PERF_TOUR
#include "esp_timer.h"
#endif

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

static void build_day_options(char *buffer, size_t size)
{
    size_t pos = 0;

    buffer[0] = '\0';
    for (int day = 1; day <= 31; ++day) {
        pos += snprintf(buffer + pos, size - pos, "%02d%s", day, (day == 31) ? "" : "\n");
    }
}

static void build_year_options(char *buffer, size_t size)
{
    size_t pos = 0;

    buffer[0] = '\0';
    for (int year = 2024; year <= 2035; ++year) {
        pos += snprintf(buffer + pos, size - pos, "%d%s", year, (year == 2035) ? "" : "\n");
    }
}

static void build_timezone_options(char *buffer, size_t size)
{
    size_t pos = 0;
    char label[32];
    uint8_t count = timezone_picker_count();

    buffer[0] = '\0';
    for (uint8_t index = 0; index < count; ++index) {
        timezone_format_picker_label(label, sizeof(label), index);
        pos += snprintf(buffer + pos, size - pos, "%s%s", label, (index + 1U == count) ? "" : "\n");
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
        update_face(ctx, clock_face_visible_index_to_id(index));
        ui_boot_step(ctx);
    }

    refresh_digital_face_snapshot(ctx);
    ui_boot_step(ctx);

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

void request_set_temporary_brightness_floor(clock_ui_context_t *ctx, bool enabled, uint8_t hw_percent)
{
    if (ctx->callbacks.on_set_temporary_brightness_floor != NULL) {
        ctx->callbacks.on_set_temporary_brightness_floor(ctx->user_ctx, enabled, hw_percent);
    }
}

void request_set_current_face(clock_ui_context_t *ctx, clock_face_id_t face)
{
    if (ctx->callbacks.on_set_current_face != NULL) {
        ctx->callbacks.on_set_current_face(ctx->user_ctx, face);
    }
}

void request_set_face_theme(clock_ui_context_t *ctx, clock_face_id_t face, uint8_t theme)
{
    if (ctx->callbacks.on_set_face_theme != NULL) {
        ctx->callbacks.on_set_face_theme(ctx->user_ctx, face, theme);
    }
}

void request_set_night_face(clock_ui_context_t *ctx, clock_face_id_t face)
{
    if (ctx->callbacks.on_set_night_face != NULL) {
        ctx->callbacks.on_set_night_face(ctx->user_ctx, face);
    }
}

void request_set_timezone(clock_ui_context_t *ctx, uint8_t timezone_id)
{
    if (ctx->callbacks.on_set_timezone != NULL) {
        ctx->callbacks.on_set_timezone(ctx->user_ctx, timezone_id);
    }
}

void request_set_time_sync_mode(clock_ui_context_t *ctx, time_sync_mode_t mode)
{
    if (ctx->callbacks.on_set_time_sync_mode != NULL) {
        ctx->callbacks.on_set_time_sync_mode(ctx->user_ctx, mode);
    }
}

void request_set_manual_time(clock_ui_context_t *ctx,
                             uint16_t year,
                             uint8_t month,
                             uint8_t day,
                             uint8_t hour,
                             uint8_t minute)
{
    if (ctx->callbacks.on_set_manual_time != NULL) {
        ctx->callbacks.on_set_manual_time(ctx->user_ctx, year, month, day, hour, minute);
    }
}

void request_save_wifi_credentials(clock_ui_context_t *ctx, const char *ssid, const char *password)
{
    if (ctx->callbacks.on_save_wifi_credentials != NULL) {
        ctx->callbacks.on_save_wifi_credentials(ctx->user_ctx, ssid, password);
    }
}

void request_ota_check(clock_ui_context_t *ctx)
{
    if (ctx->callbacks.on_ota_check_requested != NULL) {
        ctx->callbacks.on_ota_check_requested(ctx->user_ctx);
    }
}

void request_ota_install(clock_ui_context_t *ctx)
{
    if (ctx->callbacks.on_ota_install_requested != NULL) {
        ctx->callbacks.on_ota_install_requested(ctx->user_ctx);
    }
}

void request_ui_click_feedback(clock_ui_context_t *ctx)
{
    if (ctx->callbacks.on_ui_click_feedback != NULL) {
        ctx->callbacks.on_ui_click_feedback(ctx->user_ctx);
    }
}

void request_set_ui_click_sound_enabled(clock_ui_context_t *ctx, bool enabled)
{
    if (ctx->callbacks.on_set_ui_click_sound_enabled != NULL) {
        ctx->callbacks.on_set_ui_click_sound_enabled(ctx->user_ctx, enabled);
    }
}

void request_set_ui_click_volume(clock_ui_context_t *ctx, uint8_t volume)
{
    if (ctx->callbacks.on_set_ui_click_volume != NULL) {
        ctx->callbacks.on_set_ui_click_volume(ctx->user_ctx, volume);
    }
}

void request_set_language(clock_ui_context_t *ctx, clock_language_t language)
{
    if (ctx->callbacks.on_set_language != NULL) {
        ctx->callbacks.on_set_language(ctx->user_ctx, language);
    }
}

void request_set_default_alarm_sound(clock_ui_context_t *ctx, uint8_t sound)
{
    if (ctx->callbacks.on_set_default_alarm_sound != NULL) {
        ctx->callbacks.on_set_default_alarm_sound(ctx->user_ctx, sound);
    }
}

void request_set_ui_style(clock_ui_context_t *ctx, clock_ui_style_t style)
{
    if (ctx->callbacks.on_set_ui_style != NULL) {
        ctx->callbacks.on_set_ui_style(ctx->user_ctx, style);
    }
}

void request_set_night_mode_enabled(clock_ui_context_t *ctx, bool enabled)
{
    if (ctx->callbacks.on_set_night_mode_enabled != NULL) {
        ctx->callbacks.on_set_night_mode_enabled(ctx->user_ctx, enabled);
    }
}

void request_set_night_now(clock_ui_context_t *ctx, bool night)
{
    if (ctx->callbacks.on_set_night_now != NULL) {
        ctx->callbacks.on_set_night_now(ctx->user_ctx, night);
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

void request_set_night_sunrise_brightness_enabled(clock_ui_context_t *ctx, bool enabled)
{
    if (ctx->callbacks.on_set_night_sunrise_brightness_enabled != NULL) {
        ctx->callbacks.on_set_night_sunrise_brightness_enabled(ctx->user_ctx, enabled);
    }
}

void request_set_alarm_volume(clock_ui_context_t *ctx, uint8_t volume)
{
    if (ctx->callbacks.on_set_alarm_volume != NULL) {
        ctx->callbacks.on_set_alarm_volume(ctx->user_ctx, volume);
    }
}

void request_set_ascending_alarm_enabled(clock_ui_context_t *ctx, bool enabled)
{
    if (ctx->callbacks.on_set_ascending_alarm_enabled != NULL) {
        ctx->callbacks.on_set_ascending_alarm_enabled(ctx->user_ctx, enabled);
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

void ui_play_click_feedback(void)
{
    static uint32_t s_last_click_tick = 0;
    uint32_t now = lv_tick_get();

    if ((now - s_last_click_tick) < 35U) {
        return;
    }

    s_last_click_tick = now;
    request_ui_click_feedback(&s_ctx);
}

void set_root_ui_hidden(clock_ui_context_t *ctx, bool hidden)
{
    lv_obj_t *objs[] = {ctx->tileview, ctx->faces.face_swipe_layer};

    for (size_t i = 0; i < sizeof(objs) / sizeof(objs[0]); i++) {
        if (objs[i] == NULL) {
            continue;
        }
        if (hidden) {
            lv_obj_add_flag(objs[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(objs[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    ui_face_chrome_set_hidden(ctx, hidden);
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

void format_alarm_time(char *buffer, size_t size, uint8_t hour, uint8_t minute)
{
    snprintf(buffer, size, "%02u:%02u", hour, minute);
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

esp_err_t clock_ui_begin_boot(const app_settings_t *settings,
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
    build_day_options(ctx->day_options, sizeof(ctx->day_options));
    build_year_options(ctx->year_options, sizeof(ctx->year_options));
    build_timezone_options(ctx->timezone_options, sizeof(ctx->timezone_options));
    build_face_options(ctx->face_options, sizeof(ctx->face_options));
    styles_init();
    /* Steps: the interface, each face, the digital snapshot, the chrome. */
    ui_boot_create(ctx, (uint8_t)(clock_face_visible_count() + 3));
    return ESP_OK;
}

esp_err_t clock_ui_finish_boot(void)
{
    clock_ui_context_t *ctx = &s_ctx;

    build_root_ui(ctx);
    ui_boot_step(ctx);
    prewarm_face_previews(ctx);
    ui_chrome_refresh(ctx);
    ui_boot_step(ctx);
    /* The intro runs from here on the UI thread; the controls come up when it ends. */
    ui_boot_finish(ctx);
    return ESP_OK;
}

clock_ui_context_t *clock_ui_get_context(void)
{
    return &s_ctx;
}

void clock_ui_refresh(void)
{
    clock_ui_context_t *ctx = &s_ctx;
    clock_face_id_t current_face;
    clock_face_id_t night_face;

    if (ctx->chrome_root != NULL && ctx->settings->ui_style != ctx->applied_style) {
        ui_chrome_rebuild_async(ctx);
    }
    ui_settings_refresh(ctx);
    ui_face_chrome_refresh(ctx);
    ui_alarms_refresh(ctx);

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
    bool face_covered = ui_settings_is_open(ctx) || ui_alarms_is_open(ctx) || ui_face_chrome_covers_face(ctx);

    if (!ctx->faces.face_slide_active &&
        tile_to_face(ctx, lv_tileview_get_tile_active(ctx->tileview)) != desired_face) {
        set_active_face(ctx, desired_face, LV_ANIM_OFF);
    }
    active_face = tile_to_face(ctx, lv_tileview_get_tile_active(ctx->tileview));
    if (!face_covered && !ctx->faces.tileview_scrolling) {
#if CONFIG_TOUCH_CLOCK_PERF_TOUR
        int64_t started_us = esp_timer_get_time();

        update_face(ctx, active_face);
        if (esp_timer_get_time() - started_us > 5000) {
            /* The per-second face work runs on the UI thread: report the ticks that cost a frame. */
            printf("PERF face_update face=%d us=%lld\n", (int)active_face, esp_timer_get_time() - started_us);
        }
#else
        update_face(ctx, active_face);
#endif
    }
    ui_face_chrome_tick(ctx, now, active_face, face_covered);
    ui_alarms_tick(ctx, now, face_covered);
    ui_settings_tick(ctx, now);
}
