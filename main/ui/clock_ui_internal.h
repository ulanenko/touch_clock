#pragma once

#include "clock_ui.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "assets/seven_segment_font.h"
#include "domain/face_catalog.h"
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

#define BOTTOM_EDGE_ZONE 196
#define SETTINGS_CLOSE_EDGE_ZONE 16
#define SETTINGS_CLOSE_SWIPE_TRIGGER 26
#define ALARM_CLOSE_BOTTOM_EDGE_ZONE 24
#define UI_L10N_BINDING_MAX 400
#define UI_CHROME_TIMER_MAX 24

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
    lv_obj_t *overlay; /* the boot screen (ui_boot.c) */
    lv_obj_t *title;
    lv_obj_t *bar;     /* the line under the wordmark, and its filled part */
    lv_obj_t *fill;
    uint8_t step;      /* build steps done, of steps */
    uint8_t steps;
    bool revealing;    /* the wordmark fading, the face to come up */
} clock_ui_boot_state_t;

/* Inks that belong to a face in its current theme, for details drawn over it (the night
 * alarm): text in the face's own ink, the accent its glow or hand colour, soft a quieter
 * version of the text that still reads on the face. */
typedef struct {
    lv_color_t text;
    lv_color_t accent;
    lv_color_t soft;
    lv_color_t paper; /* the face's ground under the alarm details */
} ui_face_ink_t;

/* An analog face as shown: the dial snapshot with the hands drawn in (RGB565), and where
 * the hands were drawn, so the next tick repaints only there. */
typedef struct {
    lv_obj_t *canvas;
    lv_draw_buf_t *buf;
    lv_area_t drawn;
    bool valid;
} ui_face_hands_t;

/* An evening face: its static layer (drawn once per theme and day) and what it shows. */
/* A face slide snapshot: which face, and what it showed. */
typedef struct {
    clock_face_id_t face; /* CLOCK_FACE_COUNT: none */
    long minute;
    uint8_t theme;
    uint8_t language;
    uint32_t taken_ms;
} ui_face_slide_shot_t;

typedef struct {
    ui_face_hands_t view;
    lv_draw_buf_t *base;
    uint32_t base_key; /* theme and day the base was drawn for; UINT32_MAX for none */
    int16_t shown_min; /* minute of the day drawn; -1 for none */
    int8_t shown_sec;
} ui_evening_face_t;

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
    lv_obj_t *sternglas_snapshot_img;
    lv_draw_buf_t *sternglas_snapshot_buf;
    uint8_t sternglas_snapshot_theme;
    ui_face_hands_t sternglas_hands;
    lv_obj_t *avenir_snapshot_img;
    lv_draw_buf_t *avenir_snapshot_buf;
    uint8_t avenir_snapshot_theme;
    ui_face_hands_t avenir_hands;
    lv_obj_t *modern_silver_snapshot_img;
    lv_draw_buf_t *modern_silver_snapshot_buf;
    uint8_t modern_silver_snapshot_theme;
    ui_face_hands_t modern_silver_hands;
    bool matrix_on[MTX_GRID_X][MTX_GRID_Y];
    /* What the canvas shows, so a tick redraws only the dots that changed. */
    bool matrix_drawn;
    uint32_t matrix_drawn_colors[3]; /* bg, off, on */
    bool matrix_drawn_on[MTX_GRID_X][MTX_GRID_Y];
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
    /* The face slide: while a face is dragged sideways (and while it settles), a layer over
     * everything draws snapshots of the face and of its neighbour as two discs. The
     * neighbours are snapshotted ahead, while the face is idle. */
    lv_obj_t *face_slide_render;   /* off-screen image the snapshots are taken from */
    lv_obj_t *face_slide_shell;    /* the full-screen layer drawing the discs */
    lv_draw_buf_t *face_slide_bufs[3]; /* [0] the face the slide started on, [1] next, [2] previous */
    ui_face_slide_shot_t face_slide_shots[3]; /* what each buffer holds */
    lv_timer_t *face_slide_prepare_timer;
    float face_slide_spring_x0;    /* the settle: a critically damped spring from x0 at v0 */
    float face_slide_spring_v0;
    int32_t face_slide_spring_target;
    clock_face_id_t face_slide_from;
    clock_face_id_t face_slide_to;
    int8_t face_slide_dir;         /* side the neighbour snapshot is of: 1 next, -1 previous, 0 none */
    int32_t face_slide_offset;     /* x of the starting face's disc */
    int32_t face_slide_anchor_x;   /* finger x at offset 0 */
    int32_t face_slide_last_x;
    uint32_t face_slide_last_ms;   /* when the finger last moved */
    uint32_t face_slide_read_ms;   /* when the finger was last seen */
    uint32_t face_slide_start_ms;  /* when the drag began */
    int32_t face_slide_start_x;    /* and where the finger was then */
    float face_slide_velocity;     /* px/ms, smoothed */
    bool face_slide_active;        /* the layer is up: dragging or settling */
    bool face_slide_dragging;
    bool face_slide_controls_were_visible;
    lv_obj_t *face_swipe_layer;
    bool face_swipe_tracking;
    lv_point_t face_swipe_start_point;
    ui_evening_face_t ember;
    ui_evening_face_t nocturne;
    ui_evening_face_t filament;
    /* The evening faces share one base buffer (only one face shows at a time; the view
     * buffer is shared by every face_hands face); the owner is the face whose base it holds. */
    lv_draw_buf_t *evening_base_buf;
    clock_face_id_t evening_owner;
    uint8_t *filament_ghosts; /* the ten cathode digits' outlines, one tube, A8 */
} clock_ui_face_state_t;

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
