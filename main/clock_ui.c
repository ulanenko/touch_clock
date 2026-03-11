#include "clock_ui.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "assets/slava_assets.h"
#include "assets/seven_segment_font.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"
#include "others/snapshot/lv_snapshot.h"
#include "ui/ui_controls.h"
#include "ui/ui_surface.h"
#include "wifi_time.h"

#define SCREEN_SIZE 720
#define CENTER (SCREEN_SIZE / 2)
#define CLOCK_RADIUS 340
#define HOUR_HAND_LEN 195
#define MIN_HAND_LEN 270
#define SEC_HAND_LEN 310
#define TICK_INNER 315
#define TICK_OUTER 338

#define BOTTOM_EDGE_ZONE 196
#define QUICK_ACTION_SWIPE_TRIGGER 28
#define BRIGHTNESS_SHEET_WIDTH SCREEN_SIZE
#define BRIGHTNESS_SHEET_HEIGHT 244
#define BRIGHTNESS_SHEET_X 0
#define BRIGHTNESS_SHEET_OPEN_Y (SCREEN_SIZE - BRIGHTNESS_SHEET_HEIGHT)
#define BRIGHTNESS_SHEET_CLOSED_Y SCREEN_SIZE
#define BRIGHTNESS_SCRIM_OPA LV_OPA_60
#define BRIGHTNESS_SHEET_SHOW_MS 110
#define BRIGHTNESS_SHEET_HIDE_MS 90
#define AFFORDANCE_VISIBLE_MS 1000
#define AFFORDANCE_FADE_IN_MS 140
#define AFFORDANCE_FADE_OUT_MS 220
#define SETTINGS_CLOSE_EDGE_ZONE 16
#define SETTINGS_CLOSE_SWIPE_TRIGGER 26
#define ALARM_CLOSE_BOTTOM_EDGE_ZONE 24

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

#define MTX_GRID_X 33
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

#define SEG_PANEL_W 648
#define SEG_PANEL_H 324
#define SEG_CANVAS_W 620
#define SEG_CANVAS_H 248
#define SEG_DIGIT_W 108
#define SEG_DIGIT_H 176
#define SEG_DIGIT_THICK 28
#define SEG_DIGIT_GAP 22
#define SEG_DIGIT_PAIR_GAP 34
#define SEG_COLON_GAP 34
#define SEG_COLON_SIZE 22
#define SEG_COL_ON 0x5FFFB2
#define SEG_COL_HIGHLIGHT 0xD8FFE8
#define SEG_COL_GLOW 0x29C978
#define SEG_COL_OFF 0x163523
#define SEG_PANEL_BG 0x08110D
#define SEG_CANVAS_BG 0x040B08

#define WIFI_DIALOG_WIDTH 620
#define WIFI_DIALOG_HEIGHT 440
#define QUICK_ACTION_SYMBOL_BRIGHTNESS LV_SYMBOL_TINT
#define UI_ACCENT_COL 0xD8DDE3
#define UI_ACCENT_COL_PRESSED 0xB8BEC6
#define UI_ACCENT_TEXT_COL 0x14171B
#define UI_ACCENT_BORDER_COL 0x8D959E
#define UI_ACCENT_GLOW_COL 0xEEF2F5
#define UI_ACCENT_MUTED_COL 0xB2B9C1

enum {
    SETTINGS_TAB_WIFI = 0,
    SETTINGS_TAB_NIGHT = 1,
};

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
    bool animating;
    bool dragging;
    bool drag_from_edge;
    bool edge_swipe_triggered;
    bool target_open;
    bool ui_synced;
    lv_point_t drag_start_point;
    int32_t drag_start_y;
    uint8_t last_ui_percent;
    lv_obj_t *overlay;
    lv_obj_t *sheet;
    lv_obj_t *panel_overlay;
    lv_obj_t *panel;
    lv_obj_t *slider;
    lv_obj_t *value;
    lv_obj_t *edge_sensor;
    lv_obj_t *drag_handle;
    lv_obj_t *pull_hint;
} clock_ui_brightness_state_t;

typedef struct {
    bool close_dragging;
    bool open;
    bool wifi_open;
    bool night_open;
    bool wifi_scrolling;
    bool night_scrolling;
    bool wifi_cache_valid;
    bool night_cache_valid;
    bool cached_night_enabled;
    bool cached_in_night_mode;
    int8_t cached_timezone_offset_hours;
    uint8_t cached_night_start_hour;
    uint8_t cached_night_start_minute;
    uint8_t cached_night_end_hour;
    uint8_t cached_night_end_minute;
    uint8_t cached_night_brightness;
    clock_face_id_t cached_night_face;
    uint32_t scan_generation;
    uint32_t cached_wifi_scan_generation;
    char cached_wifi_status[96];
    char cached_wifi_saved_ssid[33];
    char pending_ssid[33];
    lv_point_t close_drag_start_point;
    lv_obj_t *overlay;
    lv_obj_t *panel;
    lv_obj_t *content;
    lv_obj_t *top_sensor;
    lv_obj_t *bottom_sensor;
    lv_obj_t *left_sensor;
    lv_obj_t *right_sensor;
    lv_obj_t *wifi_overlay;
    lv_obj_t *wifi_content;
    lv_obj_t *wifi_top_sensor;
    lv_obj_t *wifi_bottom_sensor;
    lv_obj_t *wifi_left_sensor;
    lv_obj_t *wifi_right_sensor;
    lv_obj_t *night_overlay;
    lv_obj_t *night_content;
    lv_obj_t *night_top_sensor;
    lv_obj_t *night_bottom_sensor;
    lv_obj_t *night_left_sensor;
    lv_obj_t *night_right_sensor;
    lv_obj_t *wifi_card;
    lv_obj_t *timezone_card;
    lv_obj_t *networks_card;
    lv_obj_t *night_card;
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
    lv_obj_t *night_face_dd;
    lv_obj_t *night_brightness_dd;
    lv_obj_t *night_status_label;
    network_ctx_t network_ctx[WIFI_TIME_MAX_SCAN_RESULTS];
} clock_ui_settings_state_t;

typedef struct {
    bool close_dragging;
    bool open;
    bool editor_open;
    bool settings_open;
    bool management_scrolling;
    bool editor_is_new;
    bool list_swipe_dragging;
    bool list_swipe_consumed;
    lv_point_t close_drag_start_point;
    lv_point_t list_swipe_start_point;
    lv_obj_t *banner;
    lv_obj_t *banner_label;
    lv_obj_t *management_overlay;
    lv_obj_t *management_content;
    lv_obj_t *management_status;
    lv_obj_t *management_card[MAX_ALARMS + 2];
    lv_obj_t *management_list;
    lv_obj_t *quick_create_row;
    lv_obj_t *management_settings_btn;
    lv_obj_t *management_settings_summary;
    lv_obj_t *manage_snooze_btn;
    lv_obj_t *manage_volume_slider;
    lv_obj_t *manage_volume_label;
    lv_obj_t *manage_test_btn;
    lv_obj_t *settings_overlay;
    lv_obj_t *settings_top_sensor;
    lv_obj_t *settings_bottom_sensor;
    lv_obj_t *settings_left_sensor;
    lv_obj_t *settings_right_sensor;
    lv_obj_t *list_card[MAX_ALARMS];
    lv_obj_t *list_content[MAX_ALARMS];
    lv_obj_t *list_delete_btn[MAX_ALARMS];
    lv_obj_t *list_time_label[MAX_ALARMS];
    lv_obj_t *list_meta_label[MAX_ALARMS];
    lv_obj_t *list_badge[MAX_ALARMS];
    lv_obj_t *list_badge_label[MAX_ALARMS];
    lv_obj_t *list_toggle[MAX_ALARMS];
    lv_obj_t *editor_overlay;
    lv_obj_t *editor_time_label;
    lv_obj_t *editor_summary_label;
    lv_obj_t *editor_enabled_sw;
    lv_obj_t *editor_hour_roller;
    lv_obj_t *editor_minute_roller;
    lv_obj_t *editor_repeat_btn[4];
    lv_obj_t *editor_day_btn[7];
    lv_obj_t *editor_delete_btn;
    lv_obj_t *top_sensor;
    lv_obj_t *bottom_sensor;
    lv_obj_t *left_sensor;
    lv_obj_t *right_sensor;
    lv_obj_t *overlay;
    lv_obj_t *overlay_label;
    lv_obj_t *overlay_subtitle;
    lv_obj_t *snooze_btn;
    lv_obj_t *stop_btn;
    time_t banner_feedback_until;
    bool banner_feedback_revertible;
    char banner_feedback_text[48];
    alarm_ctx_t alarm_ctx[MAX_ALARMS];
    alarm_day_ctx_t alarm_day_ctx[MAX_ALARMS][7];
    alarm_config_t editor_draft;
    int8_t editor_index;
    int8_t focus_alarm_index;
    int8_t swipe_open_index;
    int8_t swipe_drag_index;
    uint8_t management_card_count;
} clock_ui_alarm_state_t;

typedef struct {
    lv_obj_t *digital_glow;
    lv_obj_t *digital_live_root;
    lv_obj_t *digital_snapshot_img;
    lv_draw_buf_t *digital_snapshot_buf;
    lv_coord_t digital_snapshot_ext_draw;
    bool tileview_scrolling;
    bool digital_cache_valid;
    uint8_t digital_last_hour12;
    uint8_t digital_last_minute;
    uint8_t digital_last_second;
    bool digital_last_pm;
    int16_t digital_last_year;
    int16_t digital_last_yday;
    int8_t digital_last_wday;
    lv_obj_t *digital_time_bg;
    lv_obj_t *digital_time_glow;
    lv_obj_t *digital_time_fg;
    lv_obj_t *digital_seconds_bg;
    lv_obj_t *digital_seconds_glow;
    lv_obj_t *digital_seconds_fg;
    lv_obj_t *digital_ampm_glow;
    lv_obj_t *digital_ampm_label;
    lv_obj_t *digital_day_glow[7];
    lv_obj_t *digital_day_label[7];
    lv_obj_t *digital_date_glow;
    lv_obj_t *digital_date_label;
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
    lv_point_precise_t sternglas_hour_pts[2];
    lv_point_precise_t sternglas_min_pts[2];
    lv_point_precise_t sternglas_hour_shadow_pts[2];
    lv_point_precise_t sternglas_min_shadow_pts[2];
    lv_obj_t *sternglas_line_hour;
    lv_obj_t *sternglas_line_min;
    lv_obj_t *sternglas_line_hour_shadow;
    lv_obj_t *sternglas_line_min_shadow;
    lv_obj_t *sternglas_center_dot;
    lv_obj_t *sternglas_center_inner_dot;
    lv_obj_t *sternglas_snapshot_img;
    lv_draw_buf_t *sternglas_snapshot_buf;
    lv_obj_t *sternglas_composite_img;
    lv_draw_buf_t *sternglas_composite_buf;
    lv_obj_t *sternglas_hands_canvas;
    void *sternglas_hands_buf;
    lv_obj_t *avenir_snapshot_img;
    lv_draw_buf_t *avenir_snapshot_buf;
    lv_obj_t *avenir_composite_img;
    lv_draw_buf_t *avenir_composite_buf;
    lv_obj_t *avenir_hands_canvas;
    void *avenir_hands_buf;
    lv_obj_t *modern_silver_snapshot_img;
    lv_draw_buf_t *modern_silver_snapshot_buf;
    lv_obj_t *modern_silver_composite_img;
    lv_draw_buf_t *modern_silver_composite_buf;
    lv_obj_t *modern_silver_hands_canvas;
    void *modern_silver_hands_buf;
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
    void *segment_face_buf;
    lv_obj_t *segment_panel;
    lv_obj_t *segment_face_obj;
    lv_obj_t *segment_date_label;
    lv_obj_t *face_swipe_layer;
    bool face_swipe_tracking;
    lv_point_t face_swipe_start_point;
} clock_ui_face_state_t;

typedef struct {
    app_settings_t *settings;
    app_runtime_state_t *runtime;
    clock_ui_callbacks_t callbacks;
    void *user_ctx;
    bool suppress_events;
    bool affordances_visible;
    char hour_options[96];
    char minute_options[192];
    char timezone_options[256];
    char face_options[96];
    lv_timer_t *affordance_hide_timer;
    lv_obj_t *screen;
    lv_obj_t *tileview;
    lv_obj_t *tiles[CLOCK_FACE_COUNT];
    lv_obj_t *page_dots[CLOCK_FACE_COUNT];
    lv_obj_t *settings_button;
    clock_ui_brightness_state_t brightness;
    clock_ui_settings_state_t settings_ui;
    clock_ui_alarm_state_t alarms;
    clock_ui_face_state_t faces;
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

static clock_face_id_t tile_to_face(lv_obj_t *tile);
static void set_active_face(clock_face_id_t face, lv_anim_enable_t anim);
static void show_affordances_temporarily(void);
static void refresh_digital_face_snapshot(void);

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

static const uint8_t s_segment_font[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66,
    0x6D, 0x7D, 0x07, 0x7F, 0x6F,
};

static const uint8_t s_matrix_digit_col[4] = {3, 9, 19, 25};
static const uint8_t s_matrix_colon_col = 16;
static const uint8_t s_matrix_digit_row0 = (MTX_GRID_Y - MTX_DIGIT_H) / 2;
static const char *s_day_short[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char *s_month_short[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

enum {
    ALARM_REPEAT_PRESET_ONCE = 0,
    ALARM_REPEAT_PRESET_EVERY_DAY = 1,
    ALARM_REPEAT_PRESET_WEEKDAYS = 2,
    ALARM_REPEAT_PRESET_WEEKENDS = 3,
};

static void wifi_network_btn_event_cb(lv_event_t *event);
static void update_matrix_face(void);
static void update_wharton_face(void);
static void update_slava_face(void);
static void update_slava_dark_face(void);
static void update_sternglas_face(void);
static void update_avenir_face(void);
static void update_modern_silver_face(void);
static void update_face(clock_face_id_t face);
static void sync_face_animation_state(clock_face_id_t face);
static bool brightness_panel_is_open(void);
static void open_settings_tab(uint32_t tab_idx);
static void sync_alarm_controls(void);
static void alarm_management_open(void);
static void alarm_management_close(void);
static void alarm_editor_close(void);
static void open_alarm_editor(uint8_t alarm_index, bool is_new);

static void notify_settings_changed(void)
{
    if (s_ui.callbacks.on_settings_changed != NULL) {
        s_ui.callbacks.on_settings_changed(s_ui.user_ctx);
    }
}

static void set_root_ui_hidden(bool hidden)
{
    if (s_ui.tileview != NULL) {
        if (hidden) {
            lv_obj_add_flag(s_ui.tileview, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(s_ui.tileview, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (s_ui.settings_button != NULL) {
        if (hidden) {
            lv_obj_add_flag(s_ui.settings_button, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(s_ui.settings_button, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (s_ui.faces.face_swipe_layer != NULL) {
        if (hidden) {
            lv_obj_add_flag(s_ui.faces.face_swipe_layer, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(s_ui.faces.face_swipe_layer, LV_OBJ_FLAG_HIDDEN);
        }
    }

    for (int face = 0; face < CLOCK_FACE_COUNT; ++face) {
        if (s_ui.page_dots[face] == NULL) {
            continue;
        }

        if (hidden) {
            lv_obj_add_flag(s_ui.page_dots[face], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(s_ui.page_dots[face], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static clock_face_id_t sanitize_enabled_face(clock_face_id_t face)
{
    if (!clock_face_is_enabled(face)) {
        return clock_face_first_enabled();
    }

    return face;
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
    int visible_count = clock_face_visible_count();

    buffer[0] = '\0';
    for (int index = 0; index < visible_count; ++index) {
        clock_face_id_t face = clock_face_visible_index_to_id(index);

        pos += snprintf(buffer + pos, size - pos, "%s%s",
                        clock_face_name(face),
                        (index == (visible_count - 1)) ? "" : "\n");
    }
}

static bool alarm_surface_is_open(void)
{
    return s_ui.alarms.open || s_ui.alarms.editor_open;
}

static void format_alarm_time(char *buffer, size_t size, uint8_t hour, uint8_t minute)
{
    int display_hour = hour % 12;

    if (display_hour == 0) {
        display_hour = 12;
    }

    snprintf(buffer, size, "%d:%02u %s", display_hour, minute, (hour < 12) ? "AM" : "PM");
}

static void format_alarm_repeat_summary(char *buffer, size_t size, const alarm_config_t *alarm)
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

static int count_enabled_alarms(void)
{
    int count = 0;

    for (int i = 0; i < MAX_ALARMS; ++i) {
        if (s_ui.settings->alarms[i].enabled) {
            ++count;
        }
    }

    return count;
}

static int find_alarm_slot_for_new_alarm(void)
{
    for (int i = 0; i < MAX_ALARMS; ++i) {
        if (!s_ui.settings->alarms[i].enabled) {
            return i;
        }
    }

    return MAX_ALARMS - 1;
}

static uint8_t alarm_repeat_preset_from_config(const alarm_config_t *alarm)
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

static void apply_repeat_preset_to_alarm(alarm_config_t *alarm, uint8_t preset)
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

#include "ui/ui_brightness.c"
#include "ui/ui_alarms.c"
#include "ui/ui_settings.c"
#include "ui/ui_faces.c"
#include "ui/ui_shell.c"

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
    bool faces_sanitized = false;

    refresh_settings_controls();
    update_brightness_ui();

    if (s_ui.settings->current_face != sanitize_enabled_face(s_ui.settings->current_face)) {
        s_ui.settings->current_face = sanitize_enabled_face(s_ui.settings->current_face);
        faces_sanitized = true;
    }
    if (s_ui.settings->night_mode.face != sanitize_enabled_face(s_ui.settings->night_mode.face)) {
        s_ui.settings->night_mode.face = sanitize_enabled_face(s_ui.settings->night_mode.face);
        faces_sanitized = true;
    }
    if (faces_sanitized) {
        notify_settings_changed();
    }

    set_active_face(s_ui.runtime->in_night_mode ? s_ui.settings->night_mode.face : s_ui.settings->current_face, LV_ANIM_OFF);
}

void clock_ui_tick(time_t now)
{
    s_ui.settings->current_face = sanitize_enabled_face(s_ui.settings->current_face);
    s_ui.settings->night_mode.face = sanitize_enabled_face(s_ui.settings->night_mode.face);

    clock_face_id_t desired_face = s_ui.runtime->in_night_mode ? s_ui.settings->night_mode.face : s_ui.settings->current_face;
    clock_face_id_t active_face;
    bool opaque_menu_open = s_ui.settings_ui.open ||
                            s_ui.alarms.open ||
                            s_ui.alarms.editor_open ||
                            s_ui.alarms.settings_open;

    if (tile_to_face(lv_tileview_get_tile_active(s_ui.tileview)) != desired_face) {
        set_active_face(desired_face, LV_ANIM_OFF);
    }
    active_face = tile_to_face(lv_tileview_get_tile_active(s_ui.tileview));
    if (!opaque_menu_open && !s_ui.faces.tileview_scrolling) {
        update_face(active_face);
        update_dots(active_face);
    }

    if (!opaque_menu_open) {
        update_alarm_banner(now);
    }
    sync_alarm_overlay(now);
    if ((s_ui.alarms.open || s_ui.alarms.settings_open) &&
        !s_ui.alarms.editor_open &&
        !s_ui.alarms.management_scrolling) {
        sync_alarm_controls();
    }
    if (s_ui.settings_ui.wifi_open && !s_ui.settings_ui.wifi_scrolling && wifi_controls_need_sync()) {
        sync_wifi_controls();
    }
    if (s_ui.settings_ui.night_open && !s_ui.settings_ui.night_scrolling && night_controls_need_sync()) {
        sync_night_controls();
    }
    if (s_ui.brightness.animating ||
        s_ui.brightness.dragging ||
        brightness_panel_is_open() ||
        (s_ui.brightness.overlay != NULL && !lv_obj_has_flag(s_ui.brightness.overlay, LV_OBJ_FLAG_HIDDEN))) {
        update_brightness_ui();
    }
}
