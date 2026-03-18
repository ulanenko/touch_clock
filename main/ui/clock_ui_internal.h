#pragma once

#include "clock_ui.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "assets/seven_segment_font.h"
#include "clock_model.h"
#include "domain/brightness_policy.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"
#include "others/snapshot/lv_snapshot.h"
#include "ui/ui_controls.h"
#include "ui/ui_surface.h"

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
#define BOOT_SPINNER_DOT_COUNT 12
#define FACE_THEME_SHEET_HEIGHT SCREEN_SIZE
#define FACE_THEME_SHEET_OPEN_Y 0
#define FACE_THEME_SHEET_CLOSED_Y (-FACE_THEME_SHEET_HEIGHT)
#define FACE_THEME_SCRIM_OPA LV_OPA_70
#define FACE_THEME_SHEET_SHOW_MS 140
#define FACE_THEME_SHEET_HIDE_MS 110

#define SETTINGS_PANEL_MARGIN 32
#define SETTINGS_HEADER_HEIGHT 76

#define KEYBOARD_WIDTH 620
#define KEYBOARD_HEIGHT 208
#define KEYBOARD_BOTTOM_INSET 18

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
    struct clock_ui_state_t *ui;
    uint8_t alarm_index;
} alarm_ctx_t;

typedef struct {
    struct clock_ui_state_t *ui;
    uint8_t alarm_index;
    uint8_t day_index;
} alarm_day_ctx_t;

typedef struct {
    struct clock_ui_state_t *ui;
    uint8_t network_index;
} network_ctx_t;

typedef struct {
    struct clock_ui_state_t *ui;
    clock_face_id_t face;
} face_ctx_t;

typedef struct {
    struct clock_ui_state_t *ui;
    clock_face_id_t face;
    uint8_t theme;
} face_theme_option_ctx_t;

typedef struct {
    struct clock_ui_state_t *ui;
    ui_surface_edge_t edge;
} ui_edge_ctx_t;

typedef struct {
    lv_obj_t *overlay;
    lv_obj_t *title;
    lv_obj_t *subtitle;
    lv_obj_t *dots[BOOT_SPINNER_DOT_COUNT];
    uint8_t phase;
} clock_ui_boot_state_t;

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
    lv_timer_t *overlay_auto_close_timer;
    lv_timer_t *auto_close_timer;
    lv_obj_t *edge_sensor;
    lv_obj_t *drag_handle;
    lv_obj_t *pull_hint;
} clock_ui_brightness_state_t;

typedef struct {
    bool close_dragging;
    bool open;
    bool wifi_open;
    bool night_open;
    bool other_open;
    bool night_face_picker_open;
    bool night_schedule_editor_open;
    bool close_swipe_consumed;
    bool wifi_scrolling;
    bool night_scrolling;
    bool night_control_dragging;
    bool night_control_interaction_suppressed;
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
    lv_point_t night_control_drag_start_point;
    lv_obj_t *night_control_drag_target;
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
    lv_obj_t *other_overlay;
    lv_obj_t *other_content;
    lv_obj_t *other_top_sensor;
    lv_obj_t *other_bottom_sensor;
    lv_obj_t *other_left_sensor;
    lv_obj_t *other_right_sensor;
    lv_obj_t *night_overlay;
    lv_obj_t *night_content;
    lv_obj_t *night_top_sensor;
    lv_obj_t *night_bottom_sensor;
    lv_obj_t *night_left_sensor;
    lv_obj_t *night_right_sensor;
    lv_obj_t *night_schedule_overlay;
    lv_obj_t *night_schedule_content;
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
    lv_obj_t *night_schedule_button;
    lv_obj_t *night_schedule_summary;
    lv_obj_t *night_start_hour_dd;
    lv_obj_t *night_start_min_dd;
    lv_obj_t *night_end_hour_dd;
    lv_obj_t *night_end_min_dd;
    lv_obj_t *night_face_button;
    lv_obj_t *night_face_preview_shell;
    lv_obj_t *night_face_preview;
    lv_draw_buf_t *night_face_preview_buf;
    lv_obj_t *night_face_preview_scroll;
    lv_draw_buf_t *night_face_preview_scroll_buf;
    lv_obj_t *night_face_render_canvas;
    lv_obj_t *night_face_dd;
    lv_obj_t *night_face_picker_overlay;
    lv_obj_t *night_face_picker_content;
    lv_obj_t *night_face_picker_card[CLOCK_FACE_COUNT];
    lv_obj_t *night_face_picker_preview[CLOCK_FACE_COUNT];
    lv_draw_buf_t *night_face_picker_preview_buf[CLOCK_FACE_COUNT];
    lv_obj_t *night_face_picker_label[CLOCK_FACE_COUNT];
    lv_obj_t *night_brightness_slider;
    lv_obj_t *night_brightness_dd;
    lv_obj_t *night_status_label;
    network_ctx_t network_ctx[CLOCK_WIFI_SCAN_RESULT_MAX];
    face_ctx_t face_ctx[CLOCK_FACE_COUNT];
    ui_edge_ctx_t close_edge_ctx[4];
} clock_ui_settings_state_t;

typedef struct {
    bool close_dragging;
    bool open;
    bool editor_open;
    bool settings_open;
    bool close_swipe_consumed;
    bool management_scrolling;
    bool editor_is_new;
    bool cache_valid;
    bool list_swipe_dragging;
    bool list_swipe_consumed;
    bool cached_alarm_ringing;
    bool cached_alarm_test_active;
    bool cached_snooze_active;
    lv_point_t close_drag_start_point;
    lv_point_t list_swipe_start_point;
    lv_obj_t *banner;
    lv_obj_t *banner_label;
    lv_obj_t *management_overlay;
    lv_obj_t *management_content;
    lv_obj_t *management_status;
    lv_timer_t *management_auto_close_timer;
    lv_obj_t *management_card[MAX_ALARMS + 2];
    lv_obj_t *management_list;
    lv_obj_t *quick_create_row;
    lv_obj_t *management_settings_btn;
    lv_obj_t *management_settings_summary;
    lv_obj_t *manage_snooze_btn;
    lv_obj_t *manage_volume_slider;
    lv_obj_t *manage_volume_label;
    lv_obj_t *manage_ascending_sw;
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
    lv_obj_t *editor_repeat_card;
    lv_obj_t *editor_time_card;
    lv_obj_t *editor_time_label;
    lv_obj_t *editor_summary_label;
    lv_obj_t *editor_hour_roller;
    lv_obj_t *editor_minute_roller;
    lv_obj_t *editor_repeat_btn[4];
    lv_obj_t *editor_day_btn[7];
    lv_obj_t *editor_delete_btn;
    lv_obj_t *editor_save_btn;
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
    time_t cached_snooze_deadline;
    time_t cached_next_alarm_epoch;
    time_t cached_skipped_alarm_epoch;
    bool banner_feedback_revertible;
    bool cached_ascending_alarm_enabled;
    char banner_feedback_text[48];
    alarm_ctx_t alarm_ctx[MAX_ALARMS];
    alarm_day_ctx_t alarm_day_ctx[MAX_ALARMS][7];
    ui_edge_ctx_t close_edge_ctx[4];
    alarm_config_t editor_draft;
    alarm_config_t cached_alarms[MAX_ALARMS];
    int8_t cached_next_alarm_index;
    int8_t cached_skipped_alarm_index;
    int8_t editor_index;
    int8_t focus_alarm_index;
    int8_t swipe_open_index;
    int8_t swipe_drag_index;
    uint8_t cached_alarm_volume;
    uint8_t cached_snooze_minutes;
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
    uint8_t sternglas_snapshot_theme;
    lv_obj_t *sternglas_composite_img;
    lv_draw_buf_t *sternglas_composite_buf;
    lv_obj_t *sternglas_hands_canvas;
    void *sternglas_hands_buf;
    lv_obj_t *avenir_snapshot_img;
    lv_draw_buf_t *avenir_snapshot_buf;
    uint8_t avenir_snapshot_theme;
    lv_obj_t *avenir_composite_img;
    lv_draw_buf_t *avenir_composite_buf;
    lv_obj_t *avenir_hands_canvas;
    void *avenir_hands_buf;
    lv_obj_t *modern_silver_snapshot_img;
    lv_draw_buf_t *modern_silver_snapshot_buf;
    uint8_t modern_silver_snapshot_theme;
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
    lv_obj_t *face_swipe_layer;
    bool face_swipe_tracking;
    lv_point_t face_swipe_start_point;
} clock_ui_face_state_t;

typedef struct {
    bool button_visible;
    bool overlay_open;
    bool animating;
    bool dragging;
    bool drag_from_face;
    bool snapshot_visible;
    bool snapshot_dirty;
    bool target_open;
    clock_face_id_t picker_face;
    lv_timer_t *auto_close_timer;
    lv_timer_t *button_hide_timer;
    lv_point_t drag_start_point;
    int32_t drag_start_y;
    lv_obj_t *button;
    lv_obj_t *overlay;
    lv_obj_t *panel;
    lv_obj_t *snapshot_img;
    lv_draw_buf_t *snapshot_buf;
    lv_obj_t *drag_handle;
    lv_obj_t *grabber;
    lv_obj_t *pull_hint;
    lv_obj_t *title;
    lv_obj_t *content;
    lv_obj_t *option_card[CLOCK_FACE_THEME_COUNT];
    lv_obj_t *option_title[CLOCK_FACE_THEME_COUNT];
    lv_obj_t *option_swatches[CLOCK_FACE_THEME_COUNT][3];
    face_theme_option_ctx_t option_ctx[CLOCK_FACE_THEME_COUNT];
} clock_ui_face_theme_state_t;

typedef struct clock_ui_state_t {
    const app_settings_t *settings;
    const app_runtime_state_t *runtime;
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
    clock_ui_boot_state_t boot;
    clock_ui_brightness_state_t brightness;
    clock_ui_settings_state_t settings_ui;
    clock_ui_alarm_state_t alarms;
    clock_ui_face_state_t faces;
    clock_ui_face_theme_state_t face_theme;
} clock_ui_state_t;

typedef clock_ui_state_t clock_ui_context_t;
