#pragma once

/*
 * Face chrome state: controls over the face, theme sheet, quick actions sheet, night overlay.
 *
 * Included by clock_ui_internal.h at the point where the shared helper types are defined;
 * do not include directly. Owned by the face chrome area (ui_shell.c, ui_brightness.c): change it freely, but keep the struct name,
 * because the UI core zeroes it when the chrome is rebuilt for a style change.
 */

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
    lv_obj_t *title;
    lv_timer_t *overlay_auto_close_timer;
    lv_timer_t *auto_close_timer;
    lv_obj_t *edge_sensor;
    lv_obj_t *drag_handle;
    lv_obj_t *pull_hint;
    lv_obj_t *backdrop_img;
} clock_ui_brightness_state_t;

typedef struct {
    bool button_visible;
    bool overlay_open;
    bool animating;
    bool dragging;
    bool drag_from_face;
    bool snapshot_visible;
    bool snapshot_dirty;
    bool target_open;
    uint32_t synced_key;
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
    lv_obj_t *backdrop_img;
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

typedef struct {
    bool reserved;
} clock_ui_face_chrome_state_t;
