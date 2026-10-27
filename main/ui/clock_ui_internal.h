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
#include "localization.h"
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
#define UI_L10N_BINDING_MAX 400
#define UI_CHROME_TIMER_MAX 24

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

/* A UI string in every language, in clock_language_t order (en, pl, uk, ru, nl). Areas
 * declare their own strings inline with UI_L10N_STR instead of growing localization.c. */
typedef struct {
    const char *text[CLOCK_LANGUAGE_COUNT];
} ui_l10n_str_t;

#define UI_L10N_STR(en, pl, uk, ru, nl) {{(en), (pl), (uk), (ru), (nl)}}

typedef struct {
    lv_obj_t *label;
    l10n_text_id_t text_id;
    const ui_l10n_str_t *str; /* used instead of text_id when set */
    bool kit;                 /* kit label: keep its font and capitalisation */
} ui_l10n_binding_t;

typedef struct {
    lv_obj_t *overlay;
    lv_obj_t *title;
    lv_obj_t *subtitle;
    lv_obj_t *dots[BOOT_SPINNER_DOT_COUNT];
    uint8_t phase;
} clock_ui_boot_state_t;


#include "ui/ui_settings_state.h"

#include "ui/ui_alarms_state.h"

typedef struct {
    lv_obj_t *digital_glow;
    lv_obj_t *digital_live_root;
    lv_obj_t *digital_snapshot_img;
    lv_draw_buf_t *digital_snapshot_buf;
    lv_coord_t digital_snapshot_ext_draw;
    bool tileview_scrolling;
    bool digital_cache_valid;
    uint32_t digital_last_colors[3]; /* bg, dim, primary the labels were styled with */
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
    /* What the canvas shows, so a tick redraws only the dots that changed. */
    bool wharton_drawn;
    uint32_t wharton_drawn_colors[3]; /* bg, off, on */
    bool wharton_drawn_digit_dots[4][WH_DIGIT_COLS][WH_DIGIT_ROWS];
    bool wharton_drawn_colon_on;
    int wharton_drawn_second_count;
    void *wharton_face_buf;
    lv_obj_t *wharton_face_obj;
    lv_obj_t *face_reveal_render;
    lv_obj_t *face_reveal_shell;
    lv_draw_buf_t *face_reveal_buf;
    lv_draw_buf_t face_reveal_view;
    int32_t face_reveal_size;
    lv_obj_t *face_swipe_layer;
    clock_face_id_t face_reveal_target;
    bool face_reveal_animating;
    bool face_swipe_tracking;
    lv_point_t face_swipe_start_point;
} clock_ui_face_state_t;

/* Inks that belong to a face in its current theme, for details drawn over it (the night
 * alarm): text in the face's own ink, the accent its glow or hand colour, soft a quieter
 * version of the text that still reads on the face. */
typedef struct {
    lv_color_t text;
    lv_color_t accent;
    lv_color_t soft;
} ui_face_ink_t;

#include "ui/ui_face_chrome_state.h"

typedef struct clock_ui_state_t {
    const app_settings_t *settings;
    const app_runtime_state_t *runtime;
    clock_ui_callbacks_t callbacks;
    void *user_ctx;
    bool suppress_events;
    bool affordances_visible;
    char hour_options[96];
    char minute_options[192];
    char day_options[96];
    char year_options[64];
    char timezone_options[1024];
    char face_options[96];
    ui_l10n_binding_t l10n_bindings[UI_L10N_BINDING_MAX];
    uint16_t l10n_binding_count;
    clock_language_t cached_language;
    lv_timer_t *affordance_hide_timer;
    lv_obj_t *screen;
    lv_obj_t *tileview;
    lv_obj_t *tiles[CLOCK_FACE_COUNT];
    lv_obj_t *page_dots[CLOCK_FACE_COUNT];
    lv_obj_t *settings_button;
    lv_obj_t *update_button;
    lv_obj_t *update_button_label;
    clock_ui_boot_state_t boot;
    clock_ui_brightness_state_t brightness;
    clock_ui_settings_state_t settings_ui;
    clock_ui_alarm_state_t alarms;
    clock_ui_face_state_t faces;
    clock_ui_face_theme_state_t face_theme;
    clock_ui_face_chrome_state_t face_chrome;
    lv_draw_buf_t backdrop_buf;
    bool backdrop_ready;
    /* Chrome: everything skinned around the faces, rebuilt when the UI style changes. */
    lv_obj_t *chrome_root;
    lv_timer_t *chrome_timers[UI_CHROME_TIMER_MAX];
    uint8_t chrome_timer_count;
    uint16_t l10n_chrome_start;
    uint8_t applied_style;
    bool chrome_rebuild_pending;
} clock_ui_state_t;

typedef clock_ui_state_t clock_ui_context_t;
