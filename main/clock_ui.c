#include "clock_ui.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"
#include "wifi_time.h"

extern const lv_img_dsc_t slava_face_img;
extern const lv_img_dsc_t slava_dark_face_img;

#define SCREEN_SIZE 720
#define CENTER (SCREEN_SIZE / 2)
#define CLOCK_RADIUS 340
#define HOUR_HAND_LEN 195
#define MIN_HAND_LEN 270
#define SEC_HAND_LEN 310
#define TICK_INNER 315
#define TICK_OUTER 338

#define BOTTOM_EDGE_ZONE 110
#define BRIGHTNESS_SHEET_WIDTH 560
#define BRIGHTNESS_SHEET_HEIGHT 252
#define BRIGHTNESS_SHEET_X ((SCREEN_SIZE - BRIGHTNESS_SHEET_WIDTH) / 2)
#define BRIGHTNESS_SHEET_OPEN_Y (SCREEN_SIZE - BRIGHTNESS_SHEET_HEIGHT - 40)
#define BRIGHTNESS_SHEET_CLOSED_Y SCREEN_SIZE
#define BRIGHTNESS_SCRIM_OPA LV_OPA_60
#define BRIGHTNESS_SHEET_SHOW_MS 160
#define BRIGHTNESS_SHEET_HIDE_MS 130
#define AFFORDANCE_VISIBLE_MS 1000
#define AFFORDANCE_FADE_IN_MS 140
#define AFFORDANCE_FADE_OUT_MS 220

#define SETTINGS_PANEL_MARGIN 32
#define SETTINGS_HEADER_HEIGHT 76

#define KEYBOARD_WIDTH 620
#define KEYBOARD_HEIGHT 208
#define KEYBOARD_BOTTOM_INSET 18

#define SLAVA_HOUR_HAND_LEN 128
#define SLAVA_MIN_HAND_LEN 180
#define SLAVA_SEC_HAND_LEN 204
#define SLAVA_SEC_TAIL_LEN 38
#define SLAVA_HAND_COL 0x1A1A1A
#define SLAVA_DARK_HAND_COL 0xD0D0D0
#define SLAVA_SEC_COL 0xCC1111

#define MTX_GRID_X 31
#define MTX_GRID_Y 33
#define MTX_DIGIT_H 7
#define MTX_DOT_SIZE 16
#define MTX_DOT_GAP 6
#define MTX_PITCH (MTX_DOT_SIZE + MTX_DOT_GAP)
#define MTX_DOT_RAD 5
#define MTX_COL_ON 0x00CC44
#define MTX_COL_OFF 0x071107

#define WH_RING_DOTS 60
#define WH_RING_R 324
#define WH_DOT_LIT_R 8
#define WH_DOT_DIM_R 5
#define WH_DOT_SZ 16
#define WH_DOT_GAP 6
#define WH_DOT_PITCH (WH_DOT_SZ + WH_DOT_GAP)
#define WH_DIGIT_COLS 5
#define WH_DIGIT_ROWS 7
#define WH_COL_ON 0xD4A017
#define WH_COL_OFF 0x1E1600

#define WIFI_DIALOG_WIDTH 620
#define WIFI_DIALOG_HEIGHT 440

typedef struct {
    uint8_t alarm_index;
} alarm_ctx_t;

typedef struct {
    uint8_t alarm_index;
    uint8_t day_index;
} alarm_day_ctx_t;

typedef struct {
    uint8_t network_index;
} network_ctx_t;

typedef struct {
    app_settings_t *settings;
    app_runtime_state_t *runtime;
    clock_ui_callbacks_t callbacks;
    void *user_ctx;
    bool suppress_events;
    bool brightness_animating;
    bool brightness_dragging;
    bool brightness_drag_from_edge;
    bool affordances_visible;
    bool settings_open;
    uint32_t scan_generation;
    char pending_ssid[33];
    char hour_options[96];
    char minute_options[192];
    char timezone_options[256];
    char face_options[96];
    lv_point_t brightness_drag_start_point;
    int32_t brightness_drag_start_y;
    lv_timer_t *affordance_hide_timer;

    lv_obj_t *screen;
    lv_obj_t *tileview;
    lv_obj_t *tiles[CLOCK_FACE_COUNT];
    lv_obj_t *page_dots[CLOCK_FACE_COUNT];
    lv_obj_t *brightness_overlay;
    lv_obj_t *brightness_sheet;
    lv_obj_t *brightness_panel_overlay;
    lv_obj_t *brightness_panel;
    lv_obj_t *brightness_slider;
    lv_obj_t *brightness_value;
    lv_obj_t *brightness_edge_sensor;
    lv_obj_t *brightness_pull_hint;
    lv_obj_t *settings_button;
    lv_obj_t *settings_overlay;
    lv_obj_t *settings_panel;
    lv_obj_t *settings_tabview;
    lv_obj_t *alarm_banner;
    lv_obj_t *alarm_banner_label;
    lv_obj_t *alarm_overlay;
    lv_obj_t *alarm_overlay_label;
    lv_obj_t *alarm_overlay_subtitle;
    lv_obj_t *wifi_dialog_overlay;
    lv_obj_t *wifi_dialog;
    lv_obj_t *wifi_dialog_title;
    lv_obj_t *wifi_password_ta;
    lv_obj_t *wifi_keyboard;

    lv_obj_t *wifi_status_label;
    lv_obj_t *wifi_saved_label;
    lv_obj_t *wifi_network_list;
    lv_obj_t *wifi_timezone_dd;

    lv_obj_t *night_enabled_sw;
    lv_obj_t *night_start_hour_dd;
    lv_obj_t *night_start_min_dd;
    lv_obj_t *night_end_hour_dd;
    lv_obj_t *night_end_min_dd;
    lv_obj_t *night_brightness_slider;
    lv_obj_t *night_brightness_label;
    lv_obj_t *night_face_dd;
    lv_obj_t *night_status_label;

    lv_obj_t *alarm_status_label;
    lv_obj_t *snooze_dd;
    lv_obj_t *alarm_enabled_sw[MAX_ALARMS];
    lv_obj_t *alarm_hour_dd[MAX_ALARMS];
    lv_obj_t *alarm_min_dd[MAX_ALARMS];
    lv_obj_t *alarm_day_btn[MAX_ALARMS][7];

    network_ctx_t network_ctx[WIFI_TIME_MAX_SCAN_RESULTS];
    alarm_ctx_t alarm_ctx[MAX_ALARMS];
    alarm_day_ctx_t alarm_day_ctx[MAX_ALARMS][7];

    lv_obj_t *lbl_time;
    lv_obj_t *lbl_date;

    lv_point_precise_t hour_pts[2];
    lv_point_precise_t min_pts[2];
    lv_point_precise_t sec_pts[2];
    lv_obj_t *line_hour;
    lv_obj_t *line_min;
    lv_obj_t *line_sec;
    lv_obj_t *center_dot;
    void *analog_face_buf;

    lv_point_precise_t slava_hour_pts[2];
    lv_point_precise_t slava_min_pts[2];
    lv_point_precise_t slava_sec_pts[2];
    lv_obj_t *slava_line_hour;
    lv_obj_t *slava_line_min;
    lv_obj_t *slava_line_sec;
    lv_obj_t *slava_center_dot;

    lv_point_precise_t slava_dark_hour_pts[2];
    lv_point_precise_t slava_dark_min_pts[2];
    lv_point_precise_t slava_dark_sec_pts[2];
    lv_obj_t *slava_dark_line_hour;
    lv_obj_t *slava_dark_line_min;
    lv_obj_t *slava_dark_line_sec;
    lv_obj_t *slava_dark_center_dot;

    bool matrix_on[MTX_GRID_X][MTX_GRID_Y];
    int matrix_x0;
    int matrix_y0;
    void *matrix_face_buf;
    lv_obj_t *matrix_face_obj;

    bool wharton_digit_dots[4][WH_DIGIT_COLS][WH_DIGIT_ROWS];
    bool wharton_colon_on;
    int wharton_second_count;
    void *wharton_face_buf;
    lv_obj_t *wharton_face_obj;
} clock_ui_state_t;

static clock_ui_state_t s_ui = {0};

static lv_style_t s_style_hour;
static lv_style_t s_style_min;
static lv_style_t s_style_sec;

static const uint8_t s_matrix_font[10][MTX_DIGIT_H] = {
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

static const uint8_t s_wharton_font[10][WH_DIGIT_ROWS] = {
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

static const uint8_t s_matrix_digit_col[4] = {2, 8, 18, 24};
static const uint8_t s_matrix_colon_col = 15;
static const uint8_t s_matrix_digit_row0 = (MTX_GRID_Y - MTX_DIGIT_H) / 2;
static const char *s_day_letters[7] = {"S", "M", "T", "W", "T", "F", "S"};

static void wifi_network_btn_event_cb(lv_event_t *event);
static void update_matrix_face(void);
static void update_wharton_face(void);
static void update_slava_face(void);
static void update_slava_dark_face(void);
static bool brightness_panel_is_open(void);

static void notify_settings_changed(void)
{
    if (s_ui.callbacks.on_settings_changed != NULL) {
        s_ui.callbacks.on_settings_changed(s_ui.user_ctx);
    }
}

static struct tm get_local_time_now(void)
{
    time_t now;
    struct tm local_tm;

    time(&now);
    localtime_r(&now, &local_tm);
    return local_tm;
}

static void hand_endpoint(int cx, int cy, int length, float angle_deg, lv_point_precise_t *p0, lv_point_precise_t *p1)
{
    float rad = (angle_deg - 90.0f) * (M_PI / 180.0f);

    p0->x = cx;
    p0->y = cy;
    p1->x = cx + (int)(length * cosf(rad));
    p1->y = cy + (int)(length * sinf(rad));
}

static void hand_line_endpoints(int cx, int cy, int tail_length, int head_length,
                                float angle_deg, lv_point_precise_t *p0, lv_point_precise_t *p1)
{
    float rad = (angle_deg - 90.0f) * (M_PI / 180.0f);

    p0->x = cx - (int)(tail_length * cosf(rad));
    p0->y = cy - (int)(tail_length * sinf(rad));
    p1->x = cx + (int)(head_length * cosf(rad));
    p1->y = cy + (int)(head_length * sinf(rad));
}

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

    buffer[0] = '\0';
    for (int face = 0; face < CLOCK_FACE_COUNT; ++face) {
        pos += snprintf(buffer + pos, size - pos, "%s%s",
                        clock_face_name((clock_face_id_t)face),
                        (face == (CLOCK_FACE_COUNT - 1)) ? "" : "\n");
    }
}

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

    if (s_ui.brightness_pull_hint == NULL) {
        return;
    }

    animate_affordance(s_ui.brightness_pull_hint, visible);
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
    if (s_ui.settings_open || brightness_panel_is_open()) {
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

static void update_dots(clock_face_id_t active_face)
{
    for (int i = 0; i < CLOCK_FACE_COUNT; ++i) {
        lv_obj_set_style_bg_color(s_ui.page_dots[i],
                                  (i == active_face) ? lv_color_white() : lv_color_hex(0x555555),
                                  0);
    }
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

    if (s_ui.brightness_value == NULL || s_ui.brightness_slider == NULL) {
        return;
    }

    ui_brightness = brightness_hw_to_ui(s_ui.settings->base_brightness);
    snprintf(buffer, sizeof(buffer), "%u%%", ui_brightness);
    lv_label_set_text(s_ui.brightness_value, buffer);
    lv_slider_set_value(s_ui.brightness_slider, ui_brightness, LV_ANIM_OFF);
}

static bool brightness_panel_is_open(void)
{
    return s_ui.brightness_panel_overlay != NULL &&
           !lv_obj_has_flag(s_ui.brightness_panel_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void brightness_update_visual_state(int32_t sheet_y)
{
    int32_t clamped_y = LV_CLAMP(BRIGHTNESS_SHEET_OPEN_Y, sheet_y, BRIGHTNESS_SHEET_CLOSED_Y);
    int32_t travel = BRIGHTNESS_SHEET_CLOSED_Y - BRIGHTNESS_SHEET_OPEN_Y;
    int32_t progress = BRIGHTNESS_SHEET_CLOSED_Y - clamped_y;
    lv_opa_t opa = (lv_opa_t)((progress * BRIGHTNESS_SCRIM_OPA) / travel);

    lv_obj_set_y(s_ui.brightness_sheet, clamped_y);
    lv_obj_set_style_bg_opa(s_ui.brightness_overlay, opa, 0);
}

static void brightness_scrim_anim_cb(void *obj, int32_t value)
{
    lv_obj_set_style_bg_opa((lv_obj_t *)obj, (lv_opa_t)value, 0);
}

static void brightness_sheet_y_anim_cb(void *obj, int32_t value)
{
    lv_obj_set_y((lv_obj_t *)obj, value);
}

static void brightness_show_anim_ready_cb(lv_anim_t *anim)
{
    LV_UNUSED(anim);
    s_ui.brightness_animating = false;
    s_ui.brightness_dragging = false;
}

static void brightness_overlay_hide_immediately(void)
{
    if (s_ui.brightness_overlay == NULL) {
        return;
    }

    lv_anim_delete(s_ui.brightness_sheet, brightness_sheet_y_anim_cb);
    lv_anim_delete(s_ui.brightness_overlay, brightness_scrim_anim_cb);
    s_ui.brightness_animating = false;
    s_ui.brightness_dragging = false;
    s_ui.brightness_drag_from_edge = false;
    brightness_update_visual_state(BRIGHTNESS_SHEET_CLOSED_Y);
    lv_obj_add_flag(s_ui.brightness_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void brightness_hide_anim_ready_cb(lv_anim_t *anim)
{
    LV_UNUSED(anim);
    s_ui.brightness_animating = false;
    s_ui.brightness_dragging = false;
    lv_obj_add_flag(s_ui.brightness_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void brightness_overlay_animate(bool show)
{
    lv_anim_t panel_anim;
    lv_anim_t scrim_anim;
    uint32_t duration = show ? BRIGHTNESS_SHEET_SHOW_MS : BRIGHTNESS_SHEET_HIDE_MS;
    int32_t start_y = lv_obj_get_y(s_ui.brightness_sheet);
    lv_opa_t start_opa = lv_obj_get_style_bg_opa(s_ui.brightness_overlay, 0);

    if (s_ui.settings_open) {
        return;
    }

    s_ui.brightness_animating = true;
    s_ui.brightness_dragging = false;
    set_affordances_visible(false);
    if (s_ui.affordance_hide_timer != NULL) {
        lv_timer_pause(s_ui.affordance_hide_timer);
    }

    if (show) {
        update_brightness_ui();
        if (lv_obj_has_flag(s_ui.brightness_overlay, LV_OBJ_FLAG_HIDDEN)) {
            brightness_update_visual_state(BRIGHTNESS_SHEET_CLOSED_Y);
            lv_obj_clear_flag(s_ui.brightness_overlay, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(s_ui.brightness_overlay);
            start_y = BRIGHTNESS_SHEET_CLOSED_Y;
            start_opa = LV_OPA_TRANSP;
        }
    }

    lv_anim_init(&panel_anim);
    lv_anim_set_var(&panel_anim, s_ui.brightness_sheet);
    lv_anim_set_exec_cb(&panel_anim, brightness_sheet_y_anim_cb);
    lv_anim_set_time(&panel_anim, duration);
    lv_anim_set_values(&panel_anim, start_y, show ? BRIGHTNESS_SHEET_OPEN_Y : BRIGHTNESS_SHEET_CLOSED_Y);
    lv_anim_set_path_cb(&panel_anim, show ? lv_anim_path_ease_out : lv_anim_path_ease_in);
    lv_anim_set_ready_cb(&panel_anim, show ? brightness_show_anim_ready_cb : brightness_hide_anim_ready_cb);
    lv_anim_start(&panel_anim);

    lv_anim_init(&scrim_anim);
    lv_anim_set_var(&scrim_anim, s_ui.brightness_overlay);
    lv_anim_set_exec_cb(&scrim_anim, brightness_scrim_anim_cb);
    lv_anim_set_time(&scrim_anim, duration);
    lv_anim_set_values(&scrim_anim, start_opa, show ? BRIGHTNESS_SCRIM_OPA : LV_OPA_TRANSP);
    lv_anim_set_path_cb(&scrim_anim, show ? lv_anim_path_ease_out : lv_anim_path_ease_in);
    lv_anim_start(&scrim_anim);
}

static void brightness_overlay_hide(void)
{
    if (s_ui.brightness_animating || lv_obj_has_flag(s_ui.brightness_overlay, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

    brightness_overlay_animate(false);
}

static void brightness_panel_show(void)
{
    if (s_ui.brightness_panel_overlay == NULL || s_ui.settings_open) {
        return;
    }

    update_brightness_ui();
    lv_obj_clear_flag(s_ui.brightness_panel_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_ui.brightness_panel_overlay);
}

static void brightness_panel_hide(void)
{
    if (s_ui.brightness_panel_overlay == NULL) {
        return;
    }

    lv_obj_add_flag(s_ui.brightness_panel_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void brightness_prepare_for_drag_from_edge(void)
{
    if (!lv_obj_has_flag(s_ui.brightness_overlay, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

    update_brightness_ui();
    brightness_update_visual_state(BRIGHTNESS_SHEET_CLOSED_Y);
    lv_obj_clear_flag(s_ui.brightness_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_ui.brightness_overlay);
}

static void brightness_begin_drag(const lv_point_t *point, bool from_edge)
{
    lv_anim_delete(s_ui.brightness_sheet, brightness_sheet_y_anim_cb);
    lv_anim_delete(s_ui.brightness_overlay, brightness_scrim_anim_cb);

    show_affordances_temporarily();
    s_ui.brightness_animating = false;
    s_ui.brightness_dragging = true;
    s_ui.brightness_drag_from_edge = from_edge;
    s_ui.brightness_drag_start_point = *point;
    s_ui.brightness_drag_start_y = from_edge ? BRIGHTNESS_SHEET_CLOSED_Y : lv_obj_get_y(s_ui.brightness_sheet);

    if (from_edge) {
        brightness_prepare_for_drag_from_edge();
    }
}

static void brightness_update_drag(const lv_point_t *point)
{
    int32_t dy;

    if (!s_ui.brightness_dragging) {
        return;
    }

    dy = point->y - s_ui.brightness_drag_start_point.y;
    brightness_update_visual_state(s_ui.brightness_drag_start_y + dy);
}

static void brightness_finish_drag(void)
{
    int32_t midpoint = (BRIGHTNESS_SHEET_OPEN_Y + BRIGHTNESS_SHEET_CLOSED_Y) / 2;
    int32_t current_y;
    bool from_edge;

    if (!s_ui.brightness_dragging) {
        return;
    }

    current_y = lv_obj_get_y(s_ui.brightness_sheet);
    from_edge = s_ui.brightness_drag_from_edge;
    s_ui.brightness_dragging = false;
    s_ui.brightness_drag_from_edge = false;

    if (from_edge) {
        brightness_overlay_animate(current_y < midpoint);
        return;
    }

    brightness_overlay_animate(current_y <= midpoint);
}

static clock_face_id_t tile_to_face(lv_obj_t *tile)
{
    for (int i = 0; i < CLOCK_FACE_COUNT; ++i) {
        if (s_ui.tiles[i] == tile) {
            return (clock_face_id_t)i;
        }
    }

    return CLOCK_FACE_DIGITAL;
}

static void set_active_face(clock_face_id_t face, lv_anim_enable_t anim)
{
    if (!clock_face_is_valid(face)) {
        face = CLOCK_FACE_DIGITAL;
    }

    s_ui.suppress_events = true;
    lv_tileview_set_tile_by_index(s_ui.tileview, face, 0, anim);
    update_dots(face);
    s_ui.suppress_events = false;
}

static void update_alarm_banner(time_t now)
{
    char text[64];

    if (s_ui.runtime->alarm_ringing) {
        lv_label_set_text(s_ui.alarm_banner_label, "RINGING");
        lv_obj_clear_flag(s_ui.alarm_banner, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (s_ui.runtime->snooze_active && s_ui.runtime->snooze_deadline > now) {
        struct tm snooze_tm;

        localtime_r(&s_ui.runtime->snooze_deadline, &snooze_tm);
        snprintf(text, sizeof(text), "SNOOZE %02d:%02d", snooze_tm.tm_hour, snooze_tm.tm_min);
        lv_label_set_text(s_ui.alarm_banner_label, text);
        lv_obj_clear_flag(s_ui.alarm_banner, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (s_ui.runtime->next_alarm_epoch > now) {
        struct tm next_tm;

        localtime_r(&s_ui.runtime->next_alarm_epoch, &next_tm);
        snprintf(text, sizeof(text), "ALARM %02d:%02d", next_tm.tm_hour, next_tm.tm_min);
        lv_label_set_text(s_ui.alarm_banner_label, text);
        lv_obj_clear_flag(s_ui.alarm_banner, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_add_flag(s_ui.alarm_banner, LV_OBJ_FLAG_HIDDEN);
}

static void sync_alarm_overlay(time_t now)
{
    char subtitle[96];

    if (!s_ui.runtime->alarm_ringing) {
        lv_obj_add_flag(s_ui.alarm_overlay, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_label_set_text(s_ui.alarm_overlay_label, "Alarm");
    if (s_ui.runtime->next_alarm_epoch > now) {
        struct tm next_tm;

        localtime_r(&s_ui.runtime->next_alarm_epoch, &next_tm);
        snprintf(subtitle, sizeof(subtitle), "Scheduled for %02d:%02d", next_tm.tm_hour, next_tm.tm_min);
    } else {
        snprintf(subtitle, sizeof(subtitle), "%u minute snooze", s_ui.settings->snooze_minutes);
    }

    lv_label_set_text(s_ui.alarm_overlay_subtitle, subtitle);
    lv_obj_clear_flag(s_ui.alarm_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_ui.alarm_overlay);
}

static void sync_wifi_list(void)
{
    wifi_scan_result_t results[WIFI_TIME_MAX_SCAN_RESULTS];
    uint32_t generation = 0;
    size_t count;

    count = wifi_time_get_scan_results(results, WIFI_TIME_MAX_SCAN_RESULTS, &generation);
    if (generation == s_ui.scan_generation && lv_obj_get_child_count(s_ui.wifi_network_list) != 0) {
        return;
    }

    s_ui.scan_generation = generation;
    lv_obj_clean(s_ui.wifi_network_list);

    if (count == 0) {
        lv_obj_t *label = lv_label_create(s_ui.wifi_network_list);
        lv_label_set_text(label, "No scan results yet");
        lv_obj_set_style_text_color(label, lv_color_hex(0xA8A8A8), 0);
        lv_obj_set_width(label, lv_pct(100));
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        char row[96];

        s_ui.network_ctx[i].network_index = i;
        snprintf(row, sizeof(row), "%s  %ddBm", results[i].ssid[0] ? results[i].ssid : "<hidden>", results[i].rssi);
        lv_obj_t *btn = lv_list_add_button(s_ui.wifi_network_list, LV_SYMBOL_WIFI, row);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x242424), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x343434), LV_STATE_PRESSED);
        lv_obj_set_style_text_color(btn, lv_color_white(), 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 16, 0);
        lv_obj_add_event_cb(btn, wifi_network_btn_event_cb, LV_EVENT_CLICKED, &s_ui.network_ctx[i]);
    }
}

static void create_section_title(lv_obj_t *parent, const char *title, const char *subtitle)
{
    lv_obj_t *heading = lv_label_create(parent);
    lv_obj_set_style_text_font(heading, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(heading, lv_color_white(), 0);
    lv_label_set_text(heading, title);

    if (subtitle != NULL) {
        lv_obj_t *desc = lv_label_create(parent);
        lv_obj_set_style_text_color(desc, lv_color_hex(0xA8A8A8), 0);
        lv_label_set_text(desc, subtitle);
    }
}

static lv_obj_t *create_card(lv_obj_t *parent)
{
    lv_obj_t *card = lv_obj_create(parent);

    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x171717), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 24, 0);
    lv_obj_set_style_text_color(card, lv_color_white(), 0);
    lv_obj_set_style_pad_all(card, 20, 0);
    lv_obj_set_style_pad_row(card, 16, 0);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    return card;
}

static lv_obj_t *create_row(lv_obj_t *parent)
{
    lv_obj_t *row = lv_obj_create(parent);

    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_set_style_text_color(row, lv_color_white(), 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_column(row, 12, 0);
    lv_obj_set_style_pad_row(row, 12, 0);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    return row;
}

static lv_obj_t *create_action_button(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_t *label = lv_label_create(button);

    lv_obj_set_height(button, 48);
    lv_obj_set_style_radius(button, 18, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x303030), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x454545), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(button, lv_color_white(), 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_pad_left(button, 18, 0);
    lv_obj_set_style_pad_right(button, 18, 0);
    lv_obj_set_style_pad_top(button, 10, 0);
    lv_obj_set_style_pad_bottom(button, 10, 0);
    lv_label_set_text(label, text);
    lv_obj_center(label);

    if (cb != NULL) {
        lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, user_data);
    }

    return button;
}

static lv_obj_t *create_dropdown(lv_obj_t *parent, const char *options, uint32_t width)
{
    lv_obj_t *dd = lv_dropdown_create(parent);

    lv_dropdown_set_options(dd, options);
    lv_obj_set_width(dd, width);
    lv_obj_set_height(dd, 48);
    lv_obj_set_style_bg_color(dd, lv_color_hex(0x232323), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dd, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_color(dd, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_color(dd, lv_color_white(), LV_PART_INDICATOR);
    lv_obj_set_style_border_width(dd, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(dd, 16, LV_PART_MAIN);
    return dd;
}

static void style_slider(lv_obj_t *slider)
{
    lv_obj_set_height(slider, 16);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x363636), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_white(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_outline_width(slider, 0, LV_PART_KNOB);
}

static void apply_alarm_day_state(uint8_t alarm_index, uint8_t day_index)
{
    bool enabled = (s_ui.settings->alarms[alarm_index].days_mask & (1U << day_index)) != 0;

    if (enabled) {
        lv_obj_add_state(s_ui.alarm_day_btn[alarm_index][day_index], LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(s_ui.alarm_day_btn[alarm_index][day_index], LV_STATE_CHECKED);
    }
}

static void sync_alarm_controls(void)
{
    char status[96];

    s_ui.suppress_events = true;
    lv_dropdown_set_selected(s_ui.snooze_dd,
                             (s_ui.settings->snooze_minutes == 5) ? 0 :
                             (s_ui.settings->snooze_minutes == 10) ? 1 :
                             (s_ui.settings->snooze_minutes == 15) ? 2 :
                             (s_ui.settings->snooze_minutes == 20) ? 3 : 4);

    for (int i = 0; i < MAX_ALARMS; ++i) {
        if (s_ui.settings->alarms[i].enabled) {
            lv_obj_add_state(s_ui.alarm_enabled_sw[i], LV_STATE_CHECKED);
        } else {
            lv_obj_remove_state(s_ui.alarm_enabled_sw[i], LV_STATE_CHECKED);
        }

        lv_dropdown_set_selected(s_ui.alarm_hour_dd[i], s_ui.settings->alarms[i].hour);
        lv_dropdown_set_selected(s_ui.alarm_min_dd[i], s_ui.settings->alarms[i].minute);

        for (int day = 0; day < 7; ++day) {
            apply_alarm_day_state(i, day);
        }
    }
    s_ui.suppress_events = false;

    if (s_ui.runtime->alarm_ringing) {
        snprintf(status, sizeof(status), "Alarm is ringing");
    } else if (s_ui.runtime->snooze_active) {
        struct tm snooze_tm;

        localtime_r(&s_ui.runtime->snooze_deadline, &snooze_tm);
        snprintf(status, sizeof(status), "Snoozed until %02d:%02d", snooze_tm.tm_hour, snooze_tm.tm_min);
    } else if (s_ui.runtime->next_alarm_epoch > 0) {
        struct tm next_tm;

        localtime_r(&s_ui.runtime->next_alarm_epoch, &next_tm);
        snprintf(status, sizeof(status), "Next alarm %s %02d:%02d",
                 s_day_letters[next_tm.tm_wday], next_tm.tm_hour, next_tm.tm_min);
    } else {
        snprintf(status, sizeof(status), "No alarms enabled");
    }

    lv_label_set_text(s_ui.alarm_status_label, status);
}

static void sync_night_controls(void)
{
    char brightness[32];
    char status[96];
    uint8_t ui_brightness;

    s_ui.suppress_events = true;
    if (s_ui.settings->night_mode.enabled) {
        lv_obj_add_state(s_ui.night_enabled_sw, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(s_ui.night_enabled_sw, LV_STATE_CHECKED);
    }

    lv_dropdown_set_selected(s_ui.night_start_hour_dd, s_ui.settings->night_mode.start_hour);
    lv_dropdown_set_selected(s_ui.night_start_min_dd, s_ui.settings->night_mode.start_minute);
    lv_dropdown_set_selected(s_ui.night_end_hour_dd, s_ui.settings->night_mode.end_hour);
    lv_dropdown_set_selected(s_ui.night_end_min_dd, s_ui.settings->night_mode.end_minute);
    lv_dropdown_set_selected(s_ui.night_face_dd, s_ui.settings->night_mode.face);
    ui_brightness = brightness_hw_to_ui(s_ui.settings->night_mode.brightness);
    lv_slider_set_value(s_ui.night_brightness_slider, ui_brightness, LV_ANIM_OFF);
    s_ui.suppress_events = false;

    snprintf(brightness, sizeof(brightness), "%u%%", ui_brightness);
    lv_label_set_text(s_ui.night_brightness_label, brightness);

    snprintf(status, sizeof(status), "Night mode %s%s",
             s_ui.settings->night_mode.enabled ? "enabled" : "disabled",
             s_ui.runtime->in_night_mode ? "  active now" : "");
    lv_label_set_text(s_ui.night_status_label, status);
}

static void sync_wifi_controls(void)
{
    char saved[160];

    s_ui.suppress_events = true;
    lv_dropdown_set_selected(s_ui.wifi_timezone_dd, s_ui.settings->wifi.timezone_offset_hours + 12);
    s_ui.suppress_events = false;

    lv_label_set_text(s_ui.wifi_status_label, s_ui.runtime->wifi_status[0] ? s_ui.runtime->wifi_status : "Wi-Fi idle");

    if (s_ui.settings->wifi.ssid[0] != '\0') {
        snprintf(saved, sizeof(saved), "Saved network: %s", s_ui.settings->wifi.ssid);
    } else {
        snprintf(saved, sizeof(saved), "Saved network: none");
    }
    lv_label_set_text(s_ui.wifi_saved_label, saved);

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

    snprintf(s_ui.pending_ssid, sizeof(s_ui.pending_ssid), "%s", results[ctx->network_index].ssid);
    lv_label_set_text_fmt(s_ui.wifi_dialog_title, "Join %s", s_ui.pending_ssid);
    lv_textarea_set_text(s_ui.wifi_password_ta, "");
    lv_keyboard_set_textarea(s_ui.wifi_keyboard, s_ui.wifi_password_ta);
    lv_obj_clear_flag(s_ui.wifi_dialog_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_ui.wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_ui.wifi_dialog_overlay);
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
    if (lv_event_get_current_target(event) == s_ui.wifi_dialog_overlay &&
        lv_event_get_target(event) != s_ui.wifi_dialog_overlay) {
        return;
    }

    lv_keyboard_set_textarea(s_ui.wifi_keyboard, NULL);
    lv_obj_add_flag(s_ui.wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_ui.wifi_dialog_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void wifi_dialog_connect_event_cb(lv_event_t *event)
{
    const char *password;

    LV_UNUSED(event);

    if (s_ui.pending_ssid[0] == '\0') {
        return;
    }

    password = lv_textarea_get_text(s_ui.wifi_password_ta);
    snprintf(s_ui.settings->wifi.ssid, sizeof(s_ui.settings->wifi.ssid), "%s", s_ui.pending_ssid);
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
        lv_keyboard_set_textarea(s_ui.wifi_keyboard, s_ui.wifi_password_ta);
        lv_obj_clear_flag(s_ui.wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

static void wifi_keyboard_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        wifi_dialog_close_event_cb(event);
    }
}

static void settings_button_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    brightness_overlay_hide_immediately();
    brightness_panel_hide();
    s_ui.settings_open = true;
    refresh_settings_controls();
    if (s_ui.scan_generation == 0 && s_ui.callbacks.on_wifi_scan_requested != NULL) {
        s_ui.callbacks.on_wifi_scan_requested(s_ui.user_ctx);
    }
    lv_obj_clear_flag(s_ui.settings_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_ui.settings_overlay);
}

static void settings_close_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    s_ui.settings_open = false;
    lv_obj_add_flag(s_ui.settings_overlay, LV_OBJ_FLAG_HIDDEN);
    show_affordances_temporarily();
}

static void settings_overlay_event_cb(lv_event_t *event)
{
    if (lv_event_get_target(event) == lv_event_get_current_target(event)) {
        settings_close_event_cb(event);
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

    if (target == s_ui.night_start_hour_dd) {
        s_ui.settings->night_mode.start_hour = lv_dropdown_get_selected(target);
    } else if (target == s_ui.night_start_min_dd) {
        s_ui.settings->night_mode.start_minute = lv_dropdown_get_selected(target);
    } else if (target == s_ui.night_end_hour_dd) {
        s_ui.settings->night_mode.end_hour = lv_dropdown_get_selected(target);
    } else if (target == s_ui.night_end_min_dd) {
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

static void snooze_event_cb(lv_event_t *event)
{
    static const uint8_t options[] = {5, 10, 15, 20, 30};

    if (s_ui.suppress_events) {
        return;
    }

    s_ui.settings->snooze_minutes = options[lv_dropdown_get_selected(lv_event_get_target(event))];
    notify_settings_changed();
}

static void alarm_enabled_event_cb(lv_event_t *event)
{
    alarm_ctx_t *ctx = (alarm_ctx_t *)lv_event_get_user_data(event);

    if (s_ui.suppress_events || ctx == NULL || ctx->alarm_index >= MAX_ALARMS) {
        return;
    }

    s_ui.settings->alarms[ctx->alarm_index].enabled = lv_obj_has_state(lv_event_get_target(event), LV_STATE_CHECKED);
    notify_settings_changed();
    sync_alarm_controls();
}

static void alarm_time_event_cb(lv_event_t *event)
{
    alarm_ctx_t *ctx = (alarm_ctx_t *)lv_event_get_user_data(event);
    lv_obj_t *target = lv_event_get_target(event);

    if (s_ui.suppress_events || ctx == NULL || ctx->alarm_index >= MAX_ALARMS) {
        return;
    }

    if (target == s_ui.alarm_hour_dd[ctx->alarm_index]) {
        s_ui.settings->alarms[ctx->alarm_index].hour = lv_dropdown_get_selected(target);
    } else if (target == s_ui.alarm_min_dd[ctx->alarm_index]) {
        s_ui.settings->alarms[ctx->alarm_index].minute = lv_dropdown_get_selected(target);
    }

    notify_settings_changed();
}

static void alarm_day_event_cb(lv_event_t *event)
{
    alarm_day_ctx_t *ctx = (alarm_day_ctx_t *)lv_event_get_user_data(event);
    uint8_t mask;

    if (s_ui.suppress_events || ctx == NULL || ctx->alarm_index >= MAX_ALARMS || ctx->day_index >= 7) {
        return;
    }

    mask = (uint8_t)(1U << ctx->day_index);
    if (lv_obj_has_state(lv_event_get_target(event), LV_STATE_CHECKED)) {
        s_ui.settings->alarms[ctx->alarm_index].days_mask |= mask;
    } else {
        s_ui.settings->alarms[ctx->alarm_index].days_mask &= (uint8_t)~mask;
        if (s_ui.settings->alarms[ctx->alarm_index].days_mask == 0) {
            s_ui.settings->alarms[ctx->alarm_index].days_mask = 0x7F;
        }
    }

    notify_settings_changed();
    sync_alarm_controls();
}

static void alarm_snooze_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    if (s_ui.callbacks.on_alarm_snooze_requested != NULL) {
        s_ui.callbacks.on_alarm_snooze_requested(s_ui.user_ctx);
    }
}

static void alarm_stop_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    if (s_ui.callbacks.on_alarm_stop_requested != NULL) {
        s_ui.callbacks.on_alarm_stop_requested(s_ui.user_ctx);
    }
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

static void brightness_action_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    brightness_overlay_hide_immediately();
    brightness_panel_show();
}

static void settings_action_event_cb(lv_event_t *event)
{
    settings_button_event_cb(event);
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

static void brightness_drag_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_indev_t *indev = lv_event_get_indev(event);
    lv_point_t point;
    lv_obj_t *target = lv_event_get_target(event);
    bool from_edge = (target == s_ui.brightness_edge_sensor);

    if (indev == NULL || s_ui.settings_open || brightness_panel_is_open()) {
        return;
    }

    lv_indev_get_point(indev, &point);

    if (code == LV_EVENT_PRESSED) {
        if (s_ui.brightness_animating) {
            return;
        }
        if (from_edge && !lv_obj_has_flag(s_ui.brightness_overlay, LV_OBJ_FLAG_HIDDEN)) {
            return;
        }
        if (!from_edge && lv_obj_has_flag(s_ui.brightness_overlay, LV_OBJ_FLAG_HIDDEN)) {
            return;
        }
        brightness_begin_drag(&point, from_edge);
        return;
    }

    if (!s_ui.brightness_dragging) {
        return;
    }

    if (code == LV_EVENT_PRESS_LOST) {
        s_ui.brightness_dragging = false;
        s_ui.brightness_drag_from_edge = false;
        brightness_overlay_animate(lv_obj_get_y(s_ui.brightness_sheet) <=
                                   ((BRIGHTNESS_SHEET_OPEN_Y + BRIGHTNESS_SHEET_CLOSED_Y) / 2));
        return;
    }

    if (code == LV_EVENT_PRESSING) {
        brightness_update_drag(&point);
        return;
    }

    if (code == LV_EVENT_RELEASED) {
        brightness_finish_drag();
    }
}

static void tileview_value_changed_cb(lv_event_t *event)
{
    lv_obj_t *active_tile;
    clock_face_id_t face;

    LV_UNUSED(event);

    if (s_ui.suppress_events) {
        return;
    }

    active_tile = lv_tileview_get_tile_active(s_ui.tileview);
    face = tile_to_face(active_tile);
    update_dots(face);
    show_affordances_temporarily();

    if (!s_ui.runtime->in_night_mode && s_ui.settings->current_face != face) {
        s_ui.settings->current_face = face;
        notify_settings_changed();
    }
}

static void tileview_scroll_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);

    LV_UNUSED(event);
    if (code == LV_EVENT_SCROLL_BEGIN || code == LV_EVENT_SCROLL || code == LV_EVENT_SCROLL_END) {
        show_affordances_temporarily();
    }
}

static void create_digital_face(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, lv_color_black(), 0);

    s_ui.lbl_time = lv_label_create(parent);
    lv_obj_set_width(s_ui.lbl_time, SCREEN_SIZE);
    lv_obj_set_style_text_font(s_ui.lbl_time, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_ui.lbl_time, lv_color_white(), 0);
    lv_obj_set_style_text_align(s_ui.lbl_time, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_ui.lbl_time, "00:00");
    lv_obj_set_style_transform_scale(s_ui.lbl_time, 384, 0);
    lv_obj_align(s_ui.lbl_time, LV_ALIGN_CENTER, 0, -56);

    s_ui.lbl_date = lv_label_create(parent);
    lv_obj_set_style_text_font(s_ui.lbl_date, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_ui.lbl_date, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(s_ui.lbl_date, "Mon, Jan 01");
    lv_obj_align(s_ui.lbl_date, LV_ALIGN_CENTER, 0, 122);
}

static void update_digital_face(void)
{
    static const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    struct tm ti = get_local_time_now();
    char buf_time[16];
    char buf_date[32];

    snprintf(buf_time, sizeof(buf_time), "%02d:%02d", ti.tm_hour, ti.tm_min);
    snprintf(buf_date, sizeof(buf_date), "%s, %s %02d", days[ti.tm_wday], months[ti.tm_mon], ti.tm_mday);

    lv_label_set_text(s_ui.lbl_time, buf_time);
    lv_label_set_text(s_ui.lbl_date, buf_date);
}

static void update_face(clock_face_id_t face)
{
    switch (face) {
    case CLOCK_FACE_DIGITAL:
        update_digital_face();
        break;
    case CLOCK_FACE_MATRIX:
        update_matrix_face();
        break;
    case CLOCK_FACE_WHARTON:
        update_wharton_face();
        break;
    case CLOCK_FACE_SLAVA:
        update_slava_face();
        break;
    case CLOCK_FACE_SLAVA_DARK:
        update_slava_dark_face();
        break;
    default:
        update_digital_face();
        break;
    }
}

static void build_matrix_state(void)
{
    struct tm ti = get_local_time_now();
    int digits[4] = {ti.tm_hour / 10, ti.tm_hour % 10, ti.tm_min / 10, ti.tm_min % 10};

    memset(s_ui.matrix_on, 0, sizeof(s_ui.matrix_on));

    for (int d = 0; d < 4; ++d) {
        const uint8_t *glyph = s_matrix_font[digits[d]];
        int col0 = s_matrix_digit_col[d];

        for (int r = 0; r < MTX_DIGIT_H; ++r) {
            for (int c = 0; c < 5; ++c) {
                if ((glyph[r] >> (4 - c)) & 1U) {
                    s_ui.matrix_on[col0 + c][s_matrix_digit_row0 + r] = true;
                }
            }
        }
    }

    if ((ti.tm_sec & 1) == 0) {
        s_ui.matrix_on[s_matrix_colon_col][s_matrix_digit_row0 + 2] = true;
        s_ui.matrix_on[s_matrix_colon_col][s_matrix_digit_row0 + 4] = true;
    }
}

static void create_matrix_face(lv_obj_t *parent)
{
    int total_w = (MTX_GRID_X - 1) * MTX_PITCH + MTX_DOT_SIZE;
    int total_h = (MTX_GRID_Y - 1) * MTX_PITCH + MTX_DOT_SIZE;

    lv_obj_set_style_bg_color(parent, lv_color_hex(0x050505), 0);
    s_ui.matrix_x0 = (SCREEN_SIZE - total_w) / 2;
    s_ui.matrix_y0 = (SCREEN_SIZE - total_h) / 2;

    if (s_ui.matrix_face_buf == NULL) {
        s_ui.matrix_face_buf = heap_caps_malloc(SCREEN_SIZE * SCREEN_SIZE * sizeof(lv_color16_t), MALLOC_CAP_SPIRAM);
    }

    s_ui.matrix_face_obj = lv_canvas_create(parent);
    lv_canvas_set_buffer(s_ui.matrix_face_obj, s_ui.matrix_face_buf, SCREEN_SIZE, SCREEN_SIZE, LV_COLOR_FORMAT_RGB565);
    lv_obj_clear_flag(s_ui.matrix_face_obj, LV_OBJ_FLAG_SCROLLABLE);
}

static void update_matrix_face(void)
{
    lv_layer_t layer;
    lv_draw_rect_dsc_t dsc;

    build_matrix_state();
    lv_canvas_fill_bg(s_ui.matrix_face_obj, lv_color_hex(0x050505), LV_OPA_COVER);
    lv_canvas_init_layer(s_ui.matrix_face_obj, &layer);
    lv_draw_rect_dsc_init(&dsc);
    dsc.radius = MTX_DOT_RAD;
    dsc.bg_opa = LV_OPA_COVER;
    dsc.border_width = 0;
    dsc.shadow_width = 0;
    dsc.outline_width = 0;

    for (int c = 0; c < MTX_GRID_X; ++c) {
        for (int r = 0; r < MTX_GRID_Y; ++r) {
            lv_area_t area;

            area.x1 = s_ui.matrix_x0 + c * MTX_PITCH;
            area.y1 = s_ui.matrix_y0 + r * MTX_PITCH;
            area.x2 = area.x1 + MTX_DOT_SIZE - 1;
            area.y2 = area.y1 + MTX_DOT_SIZE - 1;
            dsc.bg_color = lv_color_hex(s_ui.matrix_on[c][r] ? MTX_COL_ON : MTX_COL_OFF);
            lv_draw_rect(&layer, &dsc, &area);
        }
    }

    lv_canvas_finish_layer(s_ui.matrix_face_obj, &layer);
}

static void build_wharton_state(void)
{
    struct tm ti = get_local_time_now();
    int digits[4] = {ti.tm_hour / 10, ti.tm_hour % 10, ti.tm_min / 10, ti.tm_min % 10};

    for (int d = 0; d < 4; ++d) {
        const uint8_t *glyph = s_wharton_font[digits[d]];
        for (int r = 0; r < WH_DIGIT_ROWS; ++r) {
            for (int c = 0; c < WH_DIGIT_COLS; ++c) {
                s_ui.wharton_digit_dots[d][c][r] = ((glyph[r] >> (4 - c)) & 1U) != 0;
            }
        }
    }

    s_ui.wharton_colon_on = ((ti.tm_sec & 1) == 0);
    s_ui.wharton_second_count = ti.tm_sec;
}

static void create_wharton_face(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, lv_color_black(), 0);

    if (s_ui.wharton_face_buf == NULL) {
        s_ui.wharton_face_buf = heap_caps_malloc(SCREEN_SIZE * SCREEN_SIZE * sizeof(lv_color16_t), MALLOC_CAP_SPIRAM);
    }

    s_ui.wharton_face_obj = lv_canvas_create(parent);
    lv_canvas_set_buffer(s_ui.wharton_face_obj, s_ui.wharton_face_buf, SCREEN_SIZE, SCREEN_SIZE, LV_COLOR_FORMAT_RGB565);
    lv_obj_clear_flag(s_ui.wharton_face_obj, LV_OBJ_FLAG_SCROLLABLE);
}

static void update_wharton_face(void)
{
    lv_layer_t layer;
    lv_draw_rect_dsc_t dsc;
    int digit_w;
    int colon_w;
    int pair_gap;
    int colon_gap;
    int total_w;
    int digit_h;
    int base_x;
    int base_y;
    int digit_x[4];
    int colon_x;

    build_wharton_state();
    lv_canvas_fill_bg(s_ui.wharton_face_obj, lv_color_black(), LV_OPA_COVER);
    lv_canvas_init_layer(s_ui.wharton_face_obj, &layer);
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_opa = LV_OPA_COVER;
    dsc.border_width = 0;
    dsc.shadow_width = 0;
    dsc.outline_width = 0;

    for (int i = 0; i < WH_RING_DOTS; ++i) {
        float angle = (i * 6.0f - 90.0f) * (M_PI / 180.0f);
        int cx = CENTER + (int)(WH_RING_R * cosf(angle));
        int cy = CENTER + (int)(WH_RING_R * sinf(angle));
        int radius = (i < s_ui.wharton_second_count) ? WH_DOT_LIT_R : WH_DOT_DIM_R;
        lv_area_t area;

        area.x1 = cx - radius;
        area.y1 = cy - radius;
        area.x2 = cx + radius;
        area.y2 = cy + radius;
        dsc.radius = radius;
        dsc.bg_color = lv_color_hex((i < s_ui.wharton_second_count) ? WH_COL_ON : WH_COL_OFF);
        lv_draw_rect(&layer, &dsc, &area);
    }

    digit_w = WH_DIGIT_COLS * WH_DOT_PITCH - WH_DOT_GAP;
    colon_w = WH_DOT_SZ;
    pair_gap = WH_DOT_PITCH;
    colon_gap = WH_DOT_PITCH + 2;
    total_w = 4 * digit_w + 2 * pair_gap + colon_w + 2 * colon_gap;
    digit_h = WH_DIGIT_ROWS * WH_DOT_PITCH - WH_DOT_GAP;
    base_x = (SCREEN_SIZE - total_w) / 2;
    base_y = (SCREEN_SIZE - digit_h) / 2 - 8;
    digit_x[0] = base_x;
    digit_x[1] = digit_x[0] + digit_w + pair_gap;
    colon_x = digit_x[1] + digit_w + colon_gap;
    digit_x[2] = colon_x + colon_w + colon_gap;
    digit_x[3] = digit_x[2] + digit_w + pair_gap;

    dsc.radius = WH_DOT_SZ / 2;
    for (int d = 0; d < 4; ++d) {
        for (int c = 0; c < WH_DIGIT_COLS; ++c) {
            for (int r = 0; r < WH_DIGIT_ROWS; ++r) {
                lv_area_t area;
                area.x1 = digit_x[d] + c * WH_DOT_PITCH;
                area.y1 = base_y + r * WH_DOT_PITCH;
                area.x2 = area.x1 + WH_DOT_SZ - 1;
                area.y2 = area.y1 + WH_DOT_SZ - 1;
                dsc.bg_color = lv_color_hex(s_ui.wharton_digit_dots[d][c][r] ? WH_COL_ON : WH_COL_OFF);
                lv_draw_rect(&layer, &dsc, &area);
            }
        }
    }

    dsc.bg_color = lv_color_hex(s_ui.wharton_colon_on ? WH_COL_ON : WH_COL_OFF);
    for (int i = 0; i < 2; ++i) {
        lv_area_t area;
        int colon_y = base_y + ((i == 0) ? 2 : 4) * WH_DOT_PITCH;
        area.x1 = colon_x;
        area.y1 = colon_y;
        area.x2 = colon_x + WH_DOT_SZ - 1;
        area.y2 = colon_y + WH_DOT_SZ - 1;
        lv_draw_rect(&layer, &dsc, &area);
    }

    lv_canvas_finish_layer(s_ui.wharton_face_obj, &layer);
}

static void create_slava_face(lv_obj_t *parent)
{
    lv_obj_t *face = lv_image_create(parent);

    lv_obj_set_style_bg_color(parent, lv_color_black(), 0);
    lv_image_set_src(face, &slava_face_img);
    lv_obj_center(face);

    s_ui.slava_line_hour = lv_line_create(parent);
    lv_obj_set_style_line_width(s_ui.slava_line_hour, 7, 0);
    lv_obj_set_style_line_color(s_ui.slava_line_hour, lv_color_hex(SLAVA_HAND_COL), 0);
    lv_obj_set_style_line_rounded(s_ui.slava_line_hour, true, 0);

    s_ui.slava_line_min = lv_line_create(parent);
    lv_obj_set_style_line_width(s_ui.slava_line_min, 5, 0);
    lv_obj_set_style_line_color(s_ui.slava_line_min, lv_color_hex(SLAVA_HAND_COL), 0);
    lv_obj_set_style_line_rounded(s_ui.slava_line_min, true, 0);

    s_ui.slava_line_sec = lv_line_create(parent);
    lv_obj_set_style_line_width(s_ui.slava_line_sec, 3, 0);
    lv_obj_set_style_line_color(s_ui.slava_line_sec, lv_color_hex(SLAVA_SEC_COL), 0);
    lv_obj_set_style_line_rounded(s_ui.slava_line_sec, true, 0);

    s_ui.slava_center_dot = lv_obj_create(parent);
    lv_obj_set_size(s_ui.slava_center_dot, 16, 16);
    lv_obj_set_style_radius(s_ui.slava_center_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_ui.slava_center_dot, lv_color_hex(SLAVA_HAND_COL), 0);
    lv_obj_set_style_border_width(s_ui.slava_center_dot, 0, 0);
    lv_obj_set_style_pad_all(s_ui.slava_center_dot, 0, 0);
    lv_obj_align(s_ui.slava_center_dot, LV_ALIGN_CENTER, 0, 0);
}

static void update_slava_face(void)
{
    struct tm ti = get_local_time_now();
    float hour_angle = ((ti.tm_hour % 12) + ti.tm_min / 60.0f) * 30.0f;
    float min_angle = (ti.tm_min + ti.tm_sec / 60.0f) * 6.0f;
    float sec_angle = ti.tm_sec * 6.0f;

    hand_endpoint(CENTER, CENTER, SLAVA_HOUR_HAND_LEN, hour_angle, &s_ui.slava_hour_pts[0], &s_ui.slava_hour_pts[1]);
    hand_endpoint(CENTER, CENTER, SLAVA_MIN_HAND_LEN, min_angle, &s_ui.slava_min_pts[0], &s_ui.slava_min_pts[1]);
    hand_line_endpoints(CENTER, CENTER, SLAVA_SEC_TAIL_LEN, SLAVA_SEC_HAND_LEN, sec_angle,
                        &s_ui.slava_sec_pts[0], &s_ui.slava_sec_pts[1]);

    lv_line_set_points(s_ui.slava_line_hour, s_ui.slava_hour_pts, 2);
    lv_line_set_points(s_ui.slava_line_min, s_ui.slava_min_pts, 2);
    lv_line_set_points(s_ui.slava_line_sec, s_ui.slava_sec_pts, 2);
}

static void create_slava_dark_face(lv_obj_t *parent)
{
    lv_obj_t *face = lv_image_create(parent);

    lv_obj_set_style_bg_color(parent, lv_color_black(), 0);
    lv_image_set_src(face, &slava_dark_face_img);
    lv_obj_center(face);

    s_ui.slava_dark_line_hour = lv_line_create(parent);
    lv_obj_set_style_line_width(s_ui.slava_dark_line_hour, 7, 0);
    lv_obj_set_style_line_color(s_ui.slava_dark_line_hour, lv_color_hex(SLAVA_DARK_HAND_COL), 0);
    lv_obj_set_style_line_rounded(s_ui.slava_dark_line_hour, true, 0);

    s_ui.slava_dark_line_min = lv_line_create(parent);
    lv_obj_set_style_line_width(s_ui.slava_dark_line_min, 5, 0);
    lv_obj_set_style_line_color(s_ui.slava_dark_line_min, lv_color_hex(SLAVA_DARK_HAND_COL), 0);
    lv_obj_set_style_line_rounded(s_ui.slava_dark_line_min, true, 0);

    s_ui.slava_dark_line_sec = lv_line_create(parent);
    lv_obj_set_style_line_width(s_ui.slava_dark_line_sec, 3, 0);
    lv_obj_set_style_line_color(s_ui.slava_dark_line_sec, lv_color_hex(SLAVA_SEC_COL), 0);
    lv_obj_set_style_line_rounded(s_ui.slava_dark_line_sec, true, 0);

    s_ui.slava_dark_center_dot = lv_obj_create(parent);
    lv_obj_set_size(s_ui.slava_dark_center_dot, 16, 16);
    lv_obj_set_style_radius(s_ui.slava_dark_center_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_ui.slava_dark_center_dot, lv_color_hex(SLAVA_DARK_HAND_COL), 0);
    lv_obj_set_style_border_width(s_ui.slava_dark_center_dot, 0, 0);
    lv_obj_set_style_pad_all(s_ui.slava_dark_center_dot, 0, 0);
    lv_obj_align(s_ui.slava_dark_center_dot, LV_ALIGN_CENTER, 0, 0);
}

static void update_slava_dark_face(void)
{
    struct tm ti = get_local_time_now();
    float hour_angle = ((ti.tm_hour % 12) + ti.tm_min / 60.0f) * 30.0f;
    float min_angle = (ti.tm_min + ti.tm_sec / 60.0f) * 6.0f;
    float sec_angle = ti.tm_sec * 6.0f;

    hand_endpoint(CENTER, CENTER, SLAVA_HOUR_HAND_LEN, hour_angle, &s_ui.slava_dark_hour_pts[0], &s_ui.slava_dark_hour_pts[1]);
    hand_endpoint(CENTER, CENTER, SLAVA_MIN_HAND_LEN, min_angle, &s_ui.slava_dark_min_pts[0], &s_ui.slava_dark_min_pts[1]);
    hand_line_endpoints(CENTER, CENTER, SLAVA_SEC_TAIL_LEN, SLAVA_SEC_HAND_LEN, sec_angle,
                        &s_ui.slava_dark_sec_pts[0], &s_ui.slava_dark_sec_pts[1]);

    lv_line_set_points(s_ui.slava_dark_line_hour, s_ui.slava_dark_hour_pts, 2);
    lv_line_set_points(s_ui.slava_dark_line_min, s_ui.slava_dark_min_pts, 2);
    lv_line_set_points(s_ui.slava_dark_line_sec, s_ui.slava_dark_sec_pts, 2);
}

static void create_dots(void)
{
    int start_x = -((CLOCK_FACE_COUNT - 1) * 18) / 2;

    for (int i = 0; i < CLOCK_FACE_COUNT; ++i) {
        s_ui.page_dots[i] = lv_obj_create(s_ui.screen);
        lv_obj_set_size(s_ui.page_dots[i], 10, 10);
        lv_obj_set_style_radius(s_ui.page_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(s_ui.page_dots[i], 0, 0);
        lv_obj_set_style_opa(s_ui.page_dots[i], LV_OPA_TRANSP, 0);
        lv_obj_set_scrollbar_mode(s_ui.page_dots[i], LV_SCROLLBAR_MODE_OFF);
        lv_obj_remove_flag(s_ui.page_dots[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(s_ui.page_dots[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(s_ui.page_dots[i], LV_ALIGN_BOTTOM_MID, start_x + i * 18, -38);
    }

    update_dots(s_ui.settings->current_face);
}

static void create_brightness_pull_hint(void)
{
    s_ui.brightness_pull_hint = lv_obj_create(s_ui.screen);
    lv_obj_set_size(s_ui.brightness_pull_hint, 88, 6);
    lv_obj_set_style_radius(s_ui.brightness_pull_hint, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(s_ui.brightness_pull_hint, 0, 0);
    lv_obj_set_style_bg_color(s_ui.brightness_pull_hint, lv_color_hex(0x808080), 0);
    lv_obj_set_style_bg_opa(s_ui.brightness_pull_hint, LV_OPA_50, 0);
    lv_obj_set_style_opa(s_ui.brightness_pull_hint, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(s_ui.brightness_pull_hint, 0, 0);
    lv_obj_remove_flag(s_ui.brightness_pull_hint, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_ui.brightness_pull_hint, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(s_ui.brightness_pull_hint, LV_ALIGN_BOTTOM_MID, 0, -16);
}

static void create_brightness_edge_sensor(void)
{
    s_ui.brightness_edge_sensor = lv_obj_create(s_ui.screen);
    lv_obj_set_size(s_ui.brightness_edge_sensor, SCREEN_SIZE, BOTTOM_EDGE_ZONE);
    lv_obj_align(s_ui.brightness_edge_sensor, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(s_ui.brightness_edge_sensor, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ui.brightness_edge_sensor, 0, 0);
    lv_obj_set_style_radius(s_ui.brightness_edge_sensor, 0, 0);
    lv_obj_set_style_pad_all(s_ui.brightness_edge_sensor, 0, 0);
    lv_obj_set_scrollbar_mode(s_ui.brightness_edge_sensor, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(s_ui.brightness_edge_sensor, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_ui.brightness_edge_sensor, brightness_drag_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_ui.brightness_edge_sensor, brightness_drag_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(s_ui.brightness_edge_sensor, brightness_drag_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(s_ui.brightness_edge_sensor, brightness_drag_event_cb, LV_EVENT_PRESS_LOST, NULL);
}

static lv_obj_t *create_quick_action_button(lv_obj_t *parent,
                                            const char *symbol,
                                            const char *text,
                                            lv_event_cb_t cb)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_t *icon = lv_label_create(button);
    lv_obj_t *label = lv_label_create(button);

    lv_obj_set_size(button, 228, 132);
    lv_obj_set_style_radius(button, 24, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x252525), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x353535), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_layout(button, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(button, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(button, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(button, 14, 0);
    lv_obj_set_style_pad_row(button, 8, 0);

    lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(icon, lv_color_white(), 0);
    lv_label_set_text(icon, symbol);

    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, text);

    if (cb != NULL) {
        lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, NULL);
    }

    return button;
}

static void create_brightness_overlay(void)
{
    lv_obj_t *sheet_grabber;
    lv_obj_t *title;
    lv_obj_t *subtitle;
    lv_obj_t *actions;

    s_ui.brightness_overlay = lv_obj_create(s_ui.screen);
    lv_obj_set_size(s_ui.brightness_overlay, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_color(s_ui.brightness_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_ui.brightness_overlay, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ui.brightness_overlay, 0, 0);
    lv_obj_set_style_radius(s_ui.brightness_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_ui.brightness_overlay, 0, 0);
    lv_obj_remove_flag(s_ui.brightness_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(s_ui.brightness_overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(s_ui.brightness_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_ui.brightness_overlay, brightness_overlay_event_cb, LV_EVENT_CLICKED, NULL);

    s_ui.brightness_sheet = lv_obj_create(s_ui.brightness_overlay);
    lv_obj_set_size(s_ui.brightness_sheet, BRIGHTNESS_SHEET_WIDTH, BRIGHTNESS_SHEET_HEIGHT);
    lv_obj_set_pos(s_ui.brightness_sheet, BRIGHTNESS_SHEET_X, BRIGHTNESS_SHEET_CLOSED_Y);
    lv_obj_set_style_bg_color(s_ui.brightness_sheet, lv_color_hex(0x161616), 0);
    lv_obj_set_style_bg_opa(s_ui.brightness_sheet, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_ui.brightness_sheet, 0, 0);
    lv_obj_set_style_radius(s_ui.brightness_sheet, 32, 0);
    lv_obj_set_style_pad_top(s_ui.brightness_sheet, 28, 0);
    lv_obj_set_style_pad_bottom(s_ui.brightness_sheet, 24, 0);
    lv_obj_set_style_pad_left(s_ui.brightness_sheet, 28, 0);
    lv_obj_set_style_pad_right(s_ui.brightness_sheet, 28, 0);
    lv_obj_set_style_shadow_width(s_ui.brightness_sheet, 24, 0);
    lv_obj_set_style_shadow_opa(s_ui.brightness_sheet, LV_OPA_30, 0);
    lv_obj_set_style_shadow_color(s_ui.brightness_sheet, lv_color_black(), 0);
    lv_obj_remove_flag(s_ui.brightness_sheet, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_ui.brightness_sheet, brightness_drag_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_ui.brightness_sheet, brightness_drag_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(s_ui.brightness_sheet, brightness_drag_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(s_ui.brightness_sheet, brightness_drag_event_cb, LV_EVENT_PRESS_LOST, NULL);

    sheet_grabber = lv_obj_create(s_ui.brightness_sheet);
    lv_obj_set_size(sheet_grabber, 72, 6);
    lv_obj_set_style_radius(sheet_grabber, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(sheet_grabber, 0, 0);
    lv_obj_set_style_bg_color(sheet_grabber, lv_color_hex(0x9C9C9C), 0);
    lv_obj_set_style_bg_opa(sheet_grabber, LV_OPA_70, 0);
    lv_obj_set_style_shadow_width(sheet_grabber, 0, 0);
    lv_obj_remove_flag(sheet_grabber, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(sheet_grabber, LV_ALIGN_TOP_MID, 0, -10);

	    title = lv_label_create(s_ui.brightness_sheet);
	    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
	    lv_obj_set_style_text_color(title, lv_color_white(), 0);
	    lv_label_set_text(title, "Quick actions");
	    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    subtitle = lv_label_create(s_ui.brightness_sheet);
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0xA8A8A8), 0);
    lv_label_set_text(subtitle, "Open brightness or full settings.");
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 44);

    actions = lv_obj_create(s_ui.brightness_sheet);
    lv_obj_set_size(actions, lv_pct(100), 132);
    lv_obj_align(actions, LV_ALIGN_TOP_MID, 0, 92);
    lv_obj_set_style_bg_opa(actions, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(actions, 0, 0);
    lv_obj_set_style_radius(actions, 0, 0);
    lv_obj_set_style_pad_all(actions, 0, 0);
    lv_obj_set_style_pad_column(actions, 16, 0);
    lv_obj_remove_flag(actions, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(actions, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(actions, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    create_quick_action_button(actions, LV_SYMBOL_TINT, "Brightness", brightness_action_event_cb);
    create_quick_action_button(actions, LV_SYMBOL_SETTINGS, "Settings", settings_action_event_cb);
}

static void create_brightness_panel(void)
{
    lv_obj_t *panel;
    lv_obj_t *title;
    lv_obj_t *close_btn;
    lv_obj_t *close_label;

    s_ui.brightness_panel_overlay = lv_obj_create(s_ui.screen);
    lv_obj_set_size(s_ui.brightness_panel_overlay, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_color(s_ui.brightness_panel_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_ui.brightness_panel_overlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(s_ui.brightness_panel_overlay, 0, 0);
    lv_obj_set_style_radius(s_ui.brightness_panel_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_ui.brightness_panel_overlay, 0, 0);
    lv_obj_add_flag(s_ui.brightness_panel_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_ui.brightness_panel_overlay, brightness_panel_overlay_event_cb, LV_EVENT_CLICKED, NULL);

    panel = lv_obj_create(s_ui.brightness_panel_overlay);
    s_ui.brightness_panel = panel;
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

    close_btn = lv_button_create(panel);
    lv_obj_set_size(close_btn, 44, 44);
    lv_obj_align(close_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_radius(close_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(0x282828), 0);
    lv_obj_set_style_border_width(close_btn, 0, 0);
    lv_obj_add_event_cb(close_btn, brightness_panel_overlay_event_cb, LV_EVENT_CLICKED, NULL);

    close_label = lv_label_create(close_btn);
    lv_label_set_text(close_label, LV_SYMBOL_CLOSE);
    lv_obj_center(close_label);

    s_ui.brightness_slider = lv_slider_create(panel);
    lv_obj_set_width(s_ui.brightness_slider, lv_pct(100));
    lv_slider_set_range(s_ui.brightness_slider, 0, 100);
    style_slider(s_ui.brightness_slider);
    lv_obj_add_event_cb(s_ui.brightness_slider, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_ui.brightness_value = lv_label_create(panel);
    lv_obj_set_style_text_font(s_ui.brightness_value, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(s_ui.brightness_value, lv_color_white(), 0);
    lv_label_set_text(s_ui.brightness_value, "50%");

    update_brightness_ui();
}

static void create_settings_button(void)
{
    lv_obj_t *label;

    s_ui.settings_button = lv_button_create(s_ui.screen);
    lv_obj_set_size(s_ui.settings_button, 56, 56);
    lv_obj_align(s_ui.settings_button, LV_ALIGN_TOP_RIGHT, -28, 28);
    lv_obj_set_style_radius(s_ui.settings_button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_ui.settings_button, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_bg_opa(s_ui.settings_button, LV_OPA_70, 0);
    lv_obj_set_style_border_width(s_ui.settings_button, 0, 0);
    lv_obj_add_event_cb(s_ui.settings_button, settings_button_event_cb, LV_EVENT_CLICKED, NULL);

    label = lv_label_create(s_ui.settings_button);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, LV_SYMBOL_SETTINGS);
    lv_obj_center(label);
}

static void create_alarm_banner(void)
{
    s_ui.alarm_banner = lv_obj_create(s_ui.screen);
    lv_obj_set_style_bg_color(s_ui.alarm_banner, lv_color_hex(0x101010), 0);
    lv_obj_set_style_bg_opa(s_ui.alarm_banner, LV_OPA_80, 0);
    lv_obj_set_style_radius(s_ui.alarm_banner, 18, 0);
    lv_obj_set_style_border_width(s_ui.alarm_banner, 0, 0);
    lv_obj_set_style_pad_left(s_ui.alarm_banner, 14, 0);
    lv_obj_set_style_pad_right(s_ui.alarm_banner, 14, 0);
    lv_obj_set_style_pad_top(s_ui.alarm_banner, 8, 0);
    lv_obj_set_style_pad_bottom(s_ui.alarm_banner, 8, 0);
    lv_obj_align(s_ui.alarm_banner, LV_ALIGN_TOP_LEFT, 28, 28);
    lv_obj_remove_flag(s_ui.alarm_banner, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_ui.alarm_banner, LV_OBJ_FLAG_HIDDEN);

    s_ui.alarm_banner_label = lv_label_create(s_ui.alarm_banner);
    lv_obj_set_style_text_color(s_ui.alarm_banner_label, lv_color_hex(0xE3C26A), 0);
    lv_label_set_text(s_ui.alarm_banner_label, "");
}

static void create_alarm_overlay(void)
{
    lv_obj_t *panel;
    lv_obj_t *actions;

    s_ui.alarm_overlay = lv_obj_create(s_ui.screen);
    lv_obj_set_size(s_ui.alarm_overlay, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_color(s_ui.alarm_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_ui.alarm_overlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(s_ui.alarm_overlay, 0, 0);
    lv_obj_set_style_radius(s_ui.alarm_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_ui.alarm_overlay, 0, 0);
    lv_obj_add_flag(s_ui.alarm_overlay, LV_OBJ_FLAG_HIDDEN);

    panel = lv_obj_create(s_ui.alarm_overlay);
    lv_obj_set_size(panel, 420, 280);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x151515), 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 28, 0);
    lv_obj_set_style_pad_all(panel, 28, 0);
    lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(panel, 20, 0);

    s_ui.alarm_overlay_label = lv_label_create(panel);
    lv_obj_set_style_text_font(s_ui.alarm_overlay_label, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(s_ui.alarm_overlay_label, lv_color_white(), 0);
    lv_label_set_text(s_ui.alarm_overlay_label, "Alarm");

    s_ui.alarm_overlay_subtitle = lv_label_create(panel);
    lv_obj_set_style_text_color(s_ui.alarm_overlay_subtitle, lv_color_hex(0xBBBBBB), 0);
    lv_label_set_text(s_ui.alarm_overlay_subtitle, "");

    actions = create_row(panel);
    create_action_button(actions, "Snooze", alarm_snooze_event_cb, NULL);
    create_action_button(actions, "Stop", alarm_stop_event_cb, NULL);
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

    s_ui.wifi_status_label = lv_label_create(card);
    lv_obj_set_style_text_color(s_ui.wifi_status_label, lv_color_white(), 0);
    lv_label_set_text(s_ui.wifi_status_label, "Wi-Fi idle");

    s_ui.wifi_saved_label = lv_label_create(card);
    lv_obj_set_style_text_color(s_ui.wifi_saved_label, lv_color_hex(0xA8A8A8), 0);
    lv_label_set_text(s_ui.wifi_saved_label, "Saved network: none");

    row = create_row(card);
    create_action_button(row, "Scan", wifi_scan_event_cb, NULL);
    create_action_button(row, "Sync time", wifi_sync_event_cb, NULL);
    create_action_button(row, "Forget", wifi_forget_event_cb, NULL);

    title = lv_label_create(card);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_label_set_text(title, "Time zone");

    s_ui.wifi_timezone_dd = create_dropdown(card, s_ui.timezone_options, 220);
    lv_obj_add_event_cb(s_ui.wifi_timezone_dd, timezone_dd_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    subtitle = lv_label_create(card);
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0xA8A8A8), 0);
    lv_label_set_text(subtitle, "Tap a network below to enter the password.");

    s_ui.wifi_network_list = lv_list_create(card);
    lv_obj_set_width(s_ui.wifi_network_list, lv_pct(100));
    lv_obj_set_height(s_ui.wifi_network_list, 280);
    lv_obj_set_style_bg_color(s_ui.wifi_network_list, lv_color_hex(0x111111), 0);
    lv_obj_set_style_bg_opa(s_ui.wifi_network_list, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(s_ui.wifi_network_list, lv_color_white(), 0);
    lv_obj_set_style_border_width(s_ui.wifi_network_list, 0, 0);
    lv_obj_set_style_radius(s_ui.wifi_network_list, 18, 0);
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
    s_ui.night_enabled_sw = lv_switch_create(row);
    lv_obj_add_event_cb(s_ui.night_enabled_sw, night_enabled_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_ui.night_status_label = lv_label_create(card);
    lv_obj_set_style_text_color(s_ui.night_status_label, lv_color_hex(0xA8A8A8), 0);
    lv_label_set_text(s_ui.night_status_label, "Night mode disabled");

    label = lv_label_create(card);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, "Start");
    row = create_row(card);
    s_ui.night_start_hour_dd = create_dropdown(row, s_ui.hour_options, 120);
    s_ui.night_start_min_dd = create_dropdown(row, s_ui.minute_options, 120);
    lv_obj_add_event_cb(s_ui.night_start_hour_dd, night_hour_minute_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_ui.night_start_min_dd, night_hour_minute_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    label = lv_label_create(card);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, "End");
    row = create_row(card);
    s_ui.night_end_hour_dd = create_dropdown(row, s_ui.hour_options, 120);
    s_ui.night_end_min_dd = create_dropdown(row, s_ui.minute_options, 120);
    lv_obj_add_event_cb(s_ui.night_end_hour_dd, night_hour_minute_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_ui.night_end_min_dd, night_hour_minute_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    label = lv_label_create(card);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, "Night brightness");
    s_ui.night_brightness_slider = lv_slider_create(card);
    lv_slider_set_range(s_ui.night_brightness_slider, 0, 100);
    lv_obj_set_width(s_ui.night_brightness_slider, lv_pct(100));
    style_slider(s_ui.night_brightness_slider);
    lv_obj_add_event_cb(s_ui.night_brightness_slider, night_brightness_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    s_ui.night_brightness_label = lv_label_create(card);
    lv_obj_set_style_text_color(s_ui.night_brightness_label, lv_color_hex(0xA8A8A8), 0);

    label = lv_label_create(card);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, "Night face");
    row = create_row(card);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_margin_bottom(row, 12, 0);
    s_ui.night_face_dd = create_dropdown(row, s_ui.face_options, 240);
    lv_obj_add_event_cb(s_ui.night_face_dd, night_face_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

static void create_alarm_card(lv_obj_t *parent, uint8_t alarm_index)
{
    lv_obj_t *card = create_card(parent);
    lv_obj_t *row;
    lv_obj_t *title;

    s_ui.alarm_ctx[alarm_index].alarm_index = alarm_index;

    row = create_row(card);
    title = lv_label_create(row);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_label_set_text_fmt(title, "Alarm %u", (unsigned)(alarm_index + 1));
    s_ui.alarm_enabled_sw[alarm_index] = lv_switch_create(row);
    lv_obj_add_event_cb(s_ui.alarm_enabled_sw[alarm_index],
                        alarm_enabled_event_cb,
                        LV_EVENT_VALUE_CHANGED,
                        &s_ui.alarm_ctx[alarm_index]);

    row = create_row(card);
    s_ui.alarm_hour_dd[alarm_index] = create_dropdown(row, s_ui.hour_options, 120);
    s_ui.alarm_min_dd[alarm_index] = create_dropdown(row, s_ui.minute_options, 120);
    lv_obj_add_event_cb(s_ui.alarm_hour_dd[alarm_index],
                        alarm_time_event_cb,
                        LV_EVENT_VALUE_CHANGED,
                        &s_ui.alarm_ctx[alarm_index]);
    lv_obj_add_event_cb(s_ui.alarm_min_dd[alarm_index],
                        alarm_time_event_cb,
                        LV_EVENT_VALUE_CHANGED,
                        &s_ui.alarm_ctx[alarm_index]);

    row = create_row(card);
    for (int day = 0; day < 7; ++day) {
        lv_obj_t *btn = lv_button_create(row);
        lv_obj_t *label = lv_label_create(btn);

        s_ui.alarm_day_ctx[alarm_index][day].alarm_index = alarm_index;
        s_ui.alarm_day_ctx[alarm_index][day].day_index = day;

        lv_obj_set_size(btn, 44, 44);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
        lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x2A2A2A), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0xC4A24C), LV_STATE_CHECKED);
        lv_obj_set_style_text_color(btn, lv_color_white(), LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(btn, lv_color_black(), LV_STATE_CHECKED);
        lv_obj_add_event_cb(btn, alarm_day_event_cb, LV_EVENT_VALUE_CHANGED, &s_ui.alarm_day_ctx[alarm_index][day]);
        lv_label_set_text(label, s_day_letters[day]);
        lv_obj_center(label);

        s_ui.alarm_day_btn[alarm_index][day] = btn;
    }
}

static void create_alarm_tab(lv_obj_t *tab)
{
    lv_obj_t *card;
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
    create_section_title(card, "Alarms", "Weekly alarms with snooze and sunrise ramp.");

    s_ui.alarm_status_label = lv_label_create(card);
    lv_obj_set_style_text_color(s_ui.alarm_status_label, lv_color_hex(0xA8A8A8), 0);
    lv_label_set_text(s_ui.alarm_status_label, "No alarms enabled");

    label = lv_label_create(card);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, "Snooze");
    s_ui.snooze_dd = create_dropdown(card, "5 min\n10 min\n15 min\n20 min\n30 min", 160);
    lv_obj_add_event_cb(s_ui.snooze_dd, snooze_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    for (int i = 0; i < MAX_ALARMS; ++i) {
        create_alarm_card(tab, i);
    }
}

static void create_wifi_dialog(void)
{
    lv_obj_t *panel;
    lv_obj_t *row;

    s_ui.wifi_dialog_overlay = lv_obj_create(s_ui.screen);
    lv_obj_set_size(s_ui.wifi_dialog_overlay, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_color(s_ui.wifi_dialog_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_ui.wifi_dialog_overlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(s_ui.wifi_dialog_overlay, 0, 0);
    lv_obj_set_style_radius(s_ui.wifi_dialog_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_ui.wifi_dialog_overlay, 0, 0);
    lv_obj_add_flag(s_ui.wifi_dialog_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_ui.wifi_dialog_overlay, wifi_dialog_close_event_cb, LV_EVENT_CLICKED, NULL);

    panel = lv_obj_create(s_ui.wifi_dialog_overlay);
    s_ui.wifi_dialog = panel;
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

    s_ui.wifi_dialog_title = lv_label_create(panel);
    lv_obj_set_style_text_font(s_ui.wifi_dialog_title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_ui.wifi_dialog_title, lv_color_white(), 0);
    lv_label_set_text(s_ui.wifi_dialog_title, "Join network");

    s_ui.wifi_password_ta = lv_textarea_create(panel);
    lv_obj_set_width(s_ui.wifi_password_ta, lv_pct(100));
    lv_obj_set_style_bg_color(s_ui.wifi_password_ta, lv_color_hex(0x232323), 0);
    lv_obj_set_style_bg_opa(s_ui.wifi_password_ta, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(s_ui.wifi_password_ta, lv_color_white(), 0);
    lv_obj_set_style_text_color(s_ui.wifi_password_ta, lv_color_hex(0x8E8E8E), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_border_width(s_ui.wifi_password_ta, 0, 0);
    lv_obj_set_style_radius(s_ui.wifi_password_ta, 16, 0);
    lv_textarea_set_placeholder_text(s_ui.wifi_password_ta, "Password");
    lv_textarea_set_password_mode(s_ui.wifi_password_ta, true);
    lv_textarea_set_one_line(s_ui.wifi_password_ta, true);
    lv_obj_add_event_cb(s_ui.wifi_password_ta, wifi_ta_focus_event_cb, LV_EVENT_FOCUSED, NULL);

    row = create_row(panel);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    create_action_button(row, "Cancel", wifi_dialog_close_event_cb, NULL);
    create_action_button(row, "Connect", wifi_dialog_connect_event_cb, NULL);

    s_ui.wifi_keyboard = lv_keyboard_create(s_ui.wifi_dialog_overlay);
    lv_obj_set_size(s_ui.wifi_keyboard, KEYBOARD_WIDTH, KEYBOARD_HEIGHT);
    lv_obj_align(s_ui.wifi_keyboard, LV_ALIGN_BOTTOM_MID, 0, -KEYBOARD_BOTTOM_INSET);
    lv_obj_set_style_radius(s_ui.wifi_keyboard, 28, 0);
    lv_obj_set_style_bg_color(s_ui.wifi_keyboard, lv_color_hex(0x151515), 0);
    lv_obj_set_style_bg_opa(s_ui.wifi_keyboard, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_ui.wifi_keyboard, 0, 0);
    lv_obj_add_flag(s_ui.wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_ui.wifi_keyboard, wifi_keyboard_event_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(s_ui.wifi_keyboard, wifi_keyboard_event_cb, LV_EVENT_CANCEL, NULL);
}

static void create_settings_overlay(void)
{
    lv_obj_t *header;
    lv_obj_t *title;
    lv_obj_t *close_btn;
    lv_obj_t *close_label;
    lv_obj_t *tab_wifi;
    lv_obj_t *tab_alarm;
    lv_obj_t *tab_night;

    s_ui.settings_overlay = lv_obj_create(s_ui.screen);
    lv_obj_set_size(s_ui.settings_overlay, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_color(s_ui.settings_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_ui.settings_overlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(s_ui.settings_overlay, 0, 0);
    lv_obj_set_style_radius(s_ui.settings_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_ui.settings_overlay, 0, 0);
    lv_obj_add_flag(s_ui.settings_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_ui.settings_overlay, settings_overlay_event_cb, LV_EVENT_CLICKED, NULL);

    s_ui.settings_panel = lv_obj_create(s_ui.settings_overlay);
    lv_obj_set_size(s_ui.settings_panel,
                    SCREEN_SIZE - SETTINGS_PANEL_MARGIN * 2,
                    SCREEN_SIZE - SETTINGS_PANEL_MARGIN * 2);
    lv_obj_align(s_ui.settings_panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(s_ui.settings_panel, lv_color_hex(0x0D0D0D), 0);
    lv_obj_set_style_bg_opa(s_ui.settings_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_ui.settings_panel, 0, 0);
    lv_obj_set_style_radius(s_ui.settings_panel, 28, 0);
    lv_obj_set_style_pad_all(s_ui.settings_panel, 0, 0);
    lv_obj_set_layout(s_ui.settings_panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(s_ui.settings_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(s_ui.settings_panel, LV_OBJ_FLAG_SCROLLABLE);

    header = lv_obj_create(s_ui.settings_panel);
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

    s_ui.settings_tabview = lv_tabview_create(s_ui.settings_panel);
    lv_obj_set_width(s_ui.settings_tabview, lv_pct(100));
    lv_obj_set_flex_grow(s_ui.settings_tabview, 1);
    lv_obj_set_style_bg_color(s_ui.settings_tabview, lv_color_hex(0x0D0D0D), 0);
    lv_obj_set_style_bg_opa(s_ui.settings_tabview, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_ui.settings_tabview, 0, 0);
    lv_tabview_set_tab_bar_size(s_ui.settings_tabview, 54);
    lv_obj_t *tab_bar = lv_tabview_get_tab_bar(s_ui.settings_tabview);
    lv_obj_set_style_bg_color(tab_bar, lv_color_hex(0x131313), 0);
    lv_obj_set_style_bg_opa(tab_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tab_bar, 0, 0);
    lv_obj_set_style_text_color(tab_bar, lv_color_hex(0xA8A8A8), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(tab_bar, lv_color_hex(0x232323), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(tab_bar, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_radius(tab_bar, 0, LV_PART_ITEMS);
    lv_obj_set_style_border_width(tab_bar, 0, LV_PART_ITEMS);
    lv_obj_set_style_text_color(tab_bar, lv_color_black(), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(tab_bar, lv_color_hex(0xC4A24C), LV_PART_ITEMS | LV_STATE_CHECKED);

    tab_wifi = lv_tabview_add_tab(s_ui.settings_tabview, "Wi-Fi");
    tab_alarm = lv_tabview_add_tab(s_ui.settings_tabview, "Alarms");
    tab_night = lv_tabview_add_tab(s_ui.settings_tabview, "Night");

    create_wifi_tab(tab_wifi);
    create_alarm_tab(tab_alarm);
    create_night_tab(tab_night);
    create_wifi_dialog();
}

static void build_root_ui(void)
{
    s_ui.screen = lv_screen_active();
    lv_obj_set_style_bg_color(s_ui.screen, lv_color_black(), 0);
    lv_obj_set_style_pad_all(s_ui.screen, 0, 0);
    lv_obj_remove_flag(s_ui.screen, LV_OBJ_FLAG_SCROLLABLE);

    s_ui.tileview = lv_tileview_create(s_ui.screen);
    lv_obj_set_size(s_ui.tileview, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_color(s_ui.tileview, lv_color_black(), 0);
    lv_obj_set_style_pad_all(s_ui.tileview, 0, 0);
    lv_obj_set_style_border_width(s_ui.tileview, 0, 0);
    lv_obj_set_scrollbar_mode(s_ui.tileview, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(s_ui.tileview, LV_OBJ_FLAG_SCROLL_ONE);
    lv_obj_align(s_ui.tileview, LV_ALIGN_CENTER, 0, 0);

    for (int face = 0; face < CLOCK_FACE_COUNT; ++face) {
        s_ui.tiles[face] = lv_tileview_add_tile(s_ui.tileview, face, 0, LV_DIR_HOR);
        lv_obj_set_style_pad_all(s_ui.tiles[face], 0, 0);
        lv_obj_set_style_border_width(s_ui.tiles[face], 0, 0);
    }

    create_digital_face(s_ui.tiles[CLOCK_FACE_DIGITAL]);
    create_matrix_face(s_ui.tiles[CLOCK_FACE_MATRIX]);
    create_wharton_face(s_ui.tiles[CLOCK_FACE_WHARTON]);
    create_slava_face(s_ui.tiles[CLOCK_FACE_SLAVA]);
    create_slava_dark_face(s_ui.tiles[CLOCK_FACE_SLAVA_DARK]);

    lv_obj_add_event_cb(s_ui.tileview, tileview_value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_ui.tileview, tileview_scroll_event_cb, LV_EVENT_SCROLL_BEGIN, NULL);
    lv_obj_add_event_cb(s_ui.tileview, tileview_scroll_event_cb, LV_EVENT_SCROLL, NULL);
    lv_obj_add_event_cb(s_ui.tileview, tileview_scroll_event_cb, LV_EVENT_SCROLL_END, NULL);

    create_dots();
    create_brightness_pull_hint();
    create_brightness_edge_sensor();
    create_brightness_overlay();
    create_brightness_panel();
    create_settings_button();
    create_alarm_banner();
    create_alarm_overlay();
    create_settings_overlay();
    set_active_face(s_ui.settings->current_face, LV_ANIM_OFF);
}

esp_err_t clock_ui_init(app_settings_t *settings,
                        app_runtime_state_t *runtime,
                        const clock_ui_callbacks_t *callbacks,
                        void *user_ctx)
{
    memset(&s_ui, 0, sizeof(s_ui));
    s_ui.settings = settings;
    s_ui.runtime = runtime;
    s_ui.user_ctx = user_ctx;
    if (callbacks != NULL) {
        s_ui.callbacks = *callbacks;
    }

    build_hour_options(s_ui.hour_options, sizeof(s_ui.hour_options));
    build_minute_options(s_ui.minute_options, sizeof(s_ui.minute_options));
    build_timezone_options(s_ui.timezone_options, sizeof(s_ui.timezone_options));
    build_face_options(s_ui.face_options, sizeof(s_ui.face_options));
    styles_init();
    build_root_ui();
    s_ui.affordance_hide_timer = lv_timer_create(affordance_hide_timer_cb, AFFORDANCE_VISIBLE_MS, NULL);
    lv_timer_pause(s_ui.affordance_hide_timer);
    refresh_settings_controls();
    update_brightness_ui();
    show_affordances_temporarily();
    return ESP_OK;
}

void clock_ui_refresh(void)
{
    refresh_settings_controls();
    update_brightness_ui();
    set_active_face(s_ui.runtime->in_night_mode ? s_ui.settings->night_mode.face : s_ui.settings->current_face, LV_ANIM_OFF);
}

void clock_ui_tick(time_t now)
{
    clock_face_id_t desired_face = s_ui.runtime->in_night_mode ? s_ui.settings->night_mode.face : s_ui.settings->current_face;
    clock_face_id_t active_face;

    if (tile_to_face(lv_tileview_get_tile_active(s_ui.tileview)) != desired_face) {
        set_active_face(desired_face, LV_ANIM_OFF);
    }
    active_face = tile_to_face(lv_tileview_get_tile_active(s_ui.tileview));
    update_face(active_face);
    update_dots(active_face);

    update_alarm_banner(now);
    sync_alarm_overlay(now);

    if (s_ui.settings_open) {
        sync_wifi_controls();
        sync_night_controls();
        sync_alarm_controls();
    }
}
