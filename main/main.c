#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"

static const char *TAG = "clock_app";

#define SCREEN_SIZE     720
#define CENTER          (SCREEN_SIZE / 2)
#define CLOCK_RADIUS    340
#define HOUR_HAND_LEN   195
#define MIN_HAND_LEN    270
#define SEC_HAND_LEN    310
#define TICK_INNER      315
#define TICK_OUTER      338
#define BOTTOM_EDGE_ZONE 96
#define BRIGHTNESS_SHEET_WIDTH 560
#define BRIGHTNESS_SHEET_HEIGHT 252
#define BRIGHTNESS_SHEET_X ((SCREEN_SIZE - BRIGHTNESS_SHEET_WIDTH) / 2)
#define BRIGHTNESS_SHEET_OPEN_Y (SCREEN_SIZE - BRIGHTNESS_SHEET_HEIGHT - 40)
#define BRIGHTNESS_SHEET_CLOSED_Y SCREEN_SIZE
#define BRIGHTNESS_SCRIM_OPA LV_OPA_60

/* ── UI handles ─────────────────────────────────────────── */
static lv_obj_t *tv;                 /* tileview root       */
static lv_obj_t *tile_digital;       /* tile 0 – digital    */
static lv_obj_t *tile_analog;        /* tile 1 – analog     */
static lv_obj_t *brightness_overlay;
static lv_obj_t *brightness_sheet;
static lv_obj_t *brightness_slider;
static lv_obj_t *brightness_value;
static lv_obj_t *brightness_edge_sensor;
static lv_obj_t *brightness_pull_hint;

/* Digital face */
static lv_obj_t *lbl_time;
static lv_obj_t *lbl_date;
static lv_obj_t *lbl_seconds;

/* Analog face */
static lv_point_precise_t hour_pts[2];
static lv_point_precise_t min_pts[2];
static lv_point_precise_t sec_pts[2];
static lv_obj_t *line_hour;
static lv_obj_t *line_min;
static lv_obj_t *line_sec;
static lv_obj_t *center_dot;

/* Dot indicator */
static lv_obj_t *dot_left;
static lv_obj_t *dot_right;
static int current_brightness = 50;
static bool brightness_animating = false;
static bool brightness_dragging = false;
static bool brightness_drag_from_edge = false;
static lv_point_t brightness_drag_start_point;
static int32_t brightness_drag_start_y = BRIGHTNESS_SHEET_CLOSED_Y;

/* ── Styles ─────────────────────────────────────────────── */
static lv_style_t style_hour;
static lv_style_t style_min;
static lv_style_t style_sec;

static void styles_init(void)
{
    lv_style_init(&style_hour);
    lv_style_set_line_width(&style_hour, 8);
    lv_style_set_line_color(&style_hour, lv_color_white());
    lv_style_set_line_rounded(&style_hour, true);

    lv_style_init(&style_min);
    lv_style_set_line_width(&style_min, 5);
    lv_style_set_line_color(&style_min, lv_color_white());
    lv_style_set_line_rounded(&style_min, true);

    lv_style_init(&style_sec);
    lv_style_set_line_width(&style_sec, 2);
    lv_style_set_line_color(&style_sec, lv_color_make(0xFF, 0x44, 0x44));
    lv_style_set_line_rounded(&style_sec, true);
}

/* ── Helpers ────────────────────────────────────────────── */

static void hand_endpoint(int cx, int cy, int length, float angle_deg,
                          lv_point_precise_t *p0, lv_point_precise_t *p1)
{
    float rad = (angle_deg - 90.0f) * (M_PI / 180.0f);
    p0->x = cx;
    p0->y = cy;
    p1->x = cx + (int)(length * cosf(rad));
    p1->y = cy + (int)(length * sinf(rad));
}

static struct tm get_local_time(void)
{
    time_t now;
    time(&now);
    struct tm ti;
    localtime_r(&now, &ti);
    return ti;
}

/* ── Digital face ───────────────────────────────────────── */

static void create_digital_face(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, lv_color_black(), 0);

    lbl_time = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_time, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl_time, lv_color_white(), 0);
    lv_label_set_text(lbl_time, "00:00");
    lv_obj_align(lbl_time, LV_ALIGN_CENTER, 0, -40);

    lbl_seconds = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_seconds, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(lbl_seconds, lv_color_make(0x88, 0x88, 0x88), 0);
    lv_label_set_text(lbl_seconds, "00");
    lv_obj_align(lbl_seconds, LV_ALIGN_CENTER, 0, 20);

    lbl_date = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_date, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_date, lv_color_make(0xAA, 0xAA, 0xAA), 0);
    lv_label_set_text(lbl_date, "Mon, Jan 01");
    lv_obj_align(lbl_date, LV_ALIGN_CENTER, 0, 70);
}

static void update_digital_face(void)
{
    struct tm ti = get_local_time();

    static char buf_time[16];
    snprintf(buf_time, sizeof(buf_time), "%02d:%02d", ti.tm_hour, ti.tm_min);
    lv_label_set_text(lbl_time, buf_time);

    static char buf_sec[8];
    snprintf(buf_sec, sizeof(buf_sec), "%02d", ti.tm_sec);
    lv_label_set_text(lbl_seconds, buf_sec);

    static const char *days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    static const char *months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                   "Jul","Aug","Sep","Oct","Nov","Dec"};
    static char buf_date[32];
    snprintf(buf_date, sizeof(buf_date), "%s, %s %02d",
             days[ti.tm_wday], months[ti.tm_mon], ti.tm_mday);
    lv_label_set_text(lbl_date, buf_date);
}

/* ── Analog face ────────────────────────────────────────── */

static void create_analog_face(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, lv_color_black(), 0);

    /*
     * Pre-render all static dial elements (ring, ticks, numerals) onto a
     * single canvas.  During swipe transitions LVGL only needs to blit this
     * one image instead of traversing ~70 individual objects.
     */
    lv_obj_t *canvas = lv_canvas_create(parent);
    static void *cbuf;
    cbuf = heap_caps_malloc(SCREEN_SIZE * SCREEN_SIZE * sizeof(lv_color16_t),
                            MALLOC_CAP_SPIRAM);
    lv_canvas_set_buffer(canvas, cbuf, SCREEN_SIZE, SCREEN_SIZE,
                         LV_COLOR_FORMAT_RGB565);
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_COVER);

    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    /* Outer ring */
    lv_draw_arc_dsc_t arc_dsc;
    lv_draw_arc_dsc_init(&arc_dsc);
    arc_dsc.center.x = CENTER;
    arc_dsc.center.y = CENTER;
    arc_dsc.radius   = CLOCK_RADIUS + 10;
    arc_dsc.start_angle = 0;
    arc_dsc.end_angle   = 360;
    arc_dsc.width  = 2;
    arc_dsc.color  = lv_color_make(0x55, 0x55, 0x55);
    arc_dsc.opa    = LV_OPA_COVER;
    lv_draw_arc(&layer, &arc_dsc);

    /* Hour tick marks */
    for (int i = 0; i < 12; i++) {
        float angle_deg = i * 30.0f;
        float rad = (angle_deg - 90.0f) * (M_PI / 180.0f);
        int inner_r = (i % 3 == 0) ? TICK_INNER - 10 : TICK_INNER;

        lv_draw_line_dsc_t ld;
        lv_draw_line_dsc_init(&ld);
        ld.p1.x = CENTER + (int)(inner_r * cosf(rad));
        ld.p1.y = CENTER + (int)(inner_r * sinf(rad));
        ld.p2.x = CENTER + (int)(TICK_OUTER * cosf(rad));
        ld.p2.y = CENTER + (int)(TICK_OUTER * sinf(rad));
        ld.color = lv_color_white();
        ld.width = (i % 3 == 0) ? 3 : 1;
        ld.opa   = LV_OPA_COVER;
        lv_draw_line(&layer, &ld);

        if (i % 3 == 0) {
            int num = (i == 0) ? 12 : i / 3 * 3;
            int lbl_r = TICK_INNER - 35;
            int lx = CENTER + (int)(lbl_r * cosf(rad));
            int ly = CENTER + (int)(lbl_r * sinf(rad));

            static char num_buf[12][4];
            snprintf(num_buf[i], sizeof(num_buf[i]), "%d", num);

            lv_draw_label_dsc_t lbl_dsc;
            lv_draw_label_dsc_init(&lbl_dsc);
            lbl_dsc.text  = num_buf[i];
            lbl_dsc.font  = &lv_font_montserrat_24;
            lbl_dsc.color = lv_color_white();
            lbl_dsc.opa   = LV_OPA_COVER;
            lbl_dsc.align = LV_TEXT_ALIGN_CENTER;

            lv_area_t coords;
            coords.x1 = lx - 18;
            coords.y1 = ly - 14;
            coords.x2 = lx + 18;
            coords.y2 = ly + 14;
            lv_draw_label(&layer, &lbl_dsc, &coords);
        }
    }

    /* Minute tick marks */
    for (int i = 0; i < 60; i++) {
        if (i % 5 == 0) continue;
        float angle_deg = i * 6.0f;
        float rad = (angle_deg - 90.0f) * (M_PI / 180.0f);

        lv_draw_line_dsc_t ld;
        lv_draw_line_dsc_init(&ld);
        ld.p1.x = CENTER + (int)((TICK_OUTER - 6) * cosf(rad));
        ld.p1.y = CENTER + (int)((TICK_OUTER - 6) * sinf(rad));
        ld.p2.x = CENTER + (int)(TICK_OUTER * cosf(rad));
        ld.p2.y = CENTER + (int)(TICK_OUTER * sinf(rad));
        ld.color = lv_color_make(0x88, 0x88, 0x88);
        ld.width = 1;
        ld.opa   = LV_OPA_COVER;
        lv_draw_line(&layer, &ld);
    }

    lv_canvas_finish_layer(canvas, &layer);

    /* Dynamic elements: 3 hands + center dot (only 4 LVGL objects) */
    line_hour = lv_line_create(parent);
    lv_obj_add_style(line_hour, &style_hour, 0);

    line_min = lv_line_create(parent);
    lv_obj_add_style(line_min, &style_min, 0);

    line_sec = lv_line_create(parent);
    lv_obj_add_style(line_sec, &style_sec, 0);

    center_dot = lv_obj_create(parent);
    lv_obj_set_size(center_dot, 14, 14);
    lv_obj_set_style_radius(center_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(center_dot, lv_color_white(), 0);
    lv_obj_set_style_border_width(center_dot, 0, 0);
    lv_obj_set_style_pad_all(center_dot, 0, 0);
    lv_obj_align(center_dot, LV_ALIGN_CENTER, 0, 0);
}

static void update_analog_face(void)
{
    struct tm ti = get_local_time();

    float hour_angle = ((ti.tm_hour % 12) + ti.tm_min / 60.0f) * 30.0f;
    float min_angle  = (ti.tm_min + ti.tm_sec / 60.0f) * 6.0f;
    float sec_angle  = ti.tm_sec * 6.0f;

    hand_endpoint(CENTER, CENTER, HOUR_HAND_LEN, hour_angle, &hour_pts[0], &hour_pts[1]);
    hand_endpoint(CENTER, CENTER, MIN_HAND_LEN,  min_angle,  &min_pts[0],  &min_pts[1]);
    hand_endpoint(CENTER, CENTER, SEC_HAND_LEN,  sec_angle,  &sec_pts[0],  &sec_pts[1]);

    lv_line_set_points(line_hour, hour_pts, 2);
    lv_line_set_points(line_min,  min_pts,  2);
    lv_line_set_points(line_sec,  sec_pts,  2);
}

/* ── Page indicator dots ────────────────────────────────── */

static void update_dots(int active_page)
{
    lv_color_t on  = lv_color_white();
    lv_color_t off = lv_color_make(0x55, 0x55, 0x55);

    lv_obj_set_style_bg_color(dot_left,  (active_page == 0) ? on : off, 0);
    lv_obj_set_style_bg_color(dot_right, (active_page == 1) ? on : off, 0);
}

static void tv_value_changed_cb(lv_event_t *e)
{
    lv_obj_t *active = lv_tileview_get_tile_active(tv);
    int page = (active == tile_analog) ? 1 : 0;
    update_dots(page);
}

static void create_dots(lv_obj_t *scr)
{
    dot_left = lv_obj_create(scr);
    lv_obj_set_size(dot_left, 10, 10);
    lv_obj_set_style_radius(dot_left, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot_left, 0, 0);
    lv_obj_set_scrollbar_mode(dot_left, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(dot_left, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(dot_left, LV_ALIGN_BOTTOM_MID, -10, -38);

    dot_right = lv_obj_create(scr);
    lv_obj_set_size(dot_right, 10, 10);
    lv_obj_set_style_radius(dot_right, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot_right, 0, 0);
    lv_obj_set_scrollbar_mode(dot_right, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(dot_right, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(dot_right, LV_ALIGN_BOTTOM_MID, 10, -38);

    update_dots(0);
}

static void create_brightness_pull_hint(lv_obj_t *scr)
{
    brightness_pull_hint = lv_obj_create(scr);
    lv_obj_set_size(brightness_pull_hint, 88, 6);
    lv_obj_set_style_radius(brightness_pull_hint, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(brightness_pull_hint, 0, 0);
    lv_obj_set_style_bg_color(brightness_pull_hint, lv_color_make(0x80, 0x80, 0x80), 0);
    lv_obj_set_style_bg_opa(brightness_pull_hint, LV_OPA_50, 0);
    lv_obj_set_style_shadow_width(brightness_pull_hint, 0, 0);
    lv_obj_set_scrollbar_mode(brightness_pull_hint, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(brightness_pull_hint, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(brightness_pull_hint, LV_ALIGN_BOTTOM_MID, 0, -16);
}

static void create_brightness_edge_sensor(lv_obj_t *scr)
{
    brightness_edge_sensor = lv_obj_create(scr);
    lv_obj_set_size(brightness_edge_sensor, SCREEN_SIZE, BOTTOM_EDGE_ZONE);
    lv_obj_align(brightness_edge_sensor, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(brightness_edge_sensor, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(brightness_edge_sensor, 0, 0);
    lv_obj_set_style_radius(brightness_edge_sensor, 0, 0);
    lv_obj_set_style_pad_all(brightness_edge_sensor, 0, 0);
    lv_obj_set_scrollbar_mode(brightness_edge_sensor, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(brightness_edge_sensor, LV_OBJ_FLAG_SCROLLABLE);
}

/* ── Brightness overlay ────────────────────────────────── */

static int clamp_brightness(int brightness)
{
    if (brightness < 0) {
        return 0;
    }

    if (brightness > 100) {
        return 100;
    }

    return brightness;
}

static void update_brightness_ui(void)
{
    static char buf[32];

    snprintf(buf, sizeof(buf), "%d%%", current_brightness);
    lv_label_set_text(brightness_value, buf);
    lv_slider_set_value(brightness_slider, current_brightness, LV_ANIM_OFF);
}

static void apply_brightness(int brightness)
{
    current_brightness = clamp_brightness(brightness);

    esp_err_t err = bsp_display_brightness_set(current_brightness);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set brightness to %d%%: %s",
                 current_brightness, esp_err_to_name(err));
    }

    update_brightness_ui();
}

static void brightness_update_visual_state(int32_t sheet_y)
{
    int32_t clamped_y = LV_CLAMP(BRIGHTNESS_SHEET_OPEN_Y, sheet_y, BRIGHTNESS_SHEET_CLOSED_Y);
    int32_t travel = BRIGHTNESS_SHEET_CLOSED_Y - BRIGHTNESS_SHEET_OPEN_Y;
    int32_t progress = BRIGHTNESS_SHEET_CLOSED_Y - clamped_y;
    lv_opa_t opa = (lv_opa_t)((progress * BRIGHTNESS_SCRIM_OPA) / travel);

    lv_obj_set_y(brightness_sheet, clamped_y);
    lv_obj_set_style_bg_opa(brightness_overlay, opa, 0);
}

static void brightness_scrim_anim_cb(void *obj, int32_t value)
{
    lv_obj_set_style_bg_opa((lv_obj_t *)obj, (lv_opa_t)value, 0);
}

static void brightness_sheet_y_anim_cb(void *obj, int32_t value)
{
    lv_obj_set_y((lv_obj_t *)obj, value);
}

static void brightness_show_anim_ready_cb(lv_anim_t *a)
{
    LV_UNUSED(a);
    brightness_animating = false;
    brightness_dragging = false;
}

static void brightness_hide_anim_ready_cb(lv_anim_t *a)
{
    LV_UNUSED(a);
    brightness_animating = false;
    brightness_dragging = false;
    lv_obj_add_flag(brightness_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void brightness_overlay_animate(bool show)
{
    lv_anim_t panel_anim;
    lv_anim_t scrim_anim;
    int32_t start_y = lv_obj_get_y(brightness_sheet);
    lv_opa_t start_opa = lv_obj_get_style_bg_opa(brightness_overlay, 0);

    brightness_animating = true;
    brightness_dragging = false;

    if (show) {
        update_brightness_ui();
        if (lv_obj_has_flag(brightness_overlay, LV_OBJ_FLAG_HIDDEN)) {
            brightness_update_visual_state(BRIGHTNESS_SHEET_CLOSED_Y);
            lv_obj_clear_flag(brightness_overlay, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(brightness_overlay);
            start_y = BRIGHTNESS_SHEET_CLOSED_Y;
            start_opa = LV_OPA_TRANSP;
        }
    }

    lv_anim_init(&panel_anim);
    lv_anim_set_var(&panel_anim, brightness_sheet);
    lv_anim_set_exec_cb(&panel_anim, brightness_sheet_y_anim_cb);
    lv_anim_set_time(&panel_anim, show ? 220 : 180);
    lv_anim_set_values(&panel_anim, start_y,
                       show ? BRIGHTNESS_SHEET_OPEN_Y : BRIGHTNESS_SHEET_CLOSED_Y);
    lv_anim_set_path_cb(&panel_anim, show ? lv_anim_path_ease_out : lv_anim_path_ease_in);
    lv_anim_set_ready_cb(&panel_anim, show ? brightness_show_anim_ready_cb : brightness_hide_anim_ready_cb);
    lv_anim_start(&panel_anim);

    lv_anim_init(&scrim_anim);
    lv_anim_set_var(&scrim_anim, brightness_overlay);
    lv_anim_set_exec_cb(&scrim_anim, brightness_scrim_anim_cb);
    lv_anim_set_time(&scrim_anim, show ? 220 : 180);
    lv_anim_set_values(&scrim_anim, start_opa,
                       show ? BRIGHTNESS_SCRIM_OPA : LV_OPA_TRANSP);
    lv_anim_set_path_cb(&scrim_anim, show ? lv_anim_path_ease_out : lv_anim_path_ease_in);
    lv_anim_start(&scrim_anim);
}

static void brightness_overlay_hide(void)
{
    if (brightness_animating || lv_obj_has_flag(brightness_overlay, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

    brightness_overlay_animate(false);
}

static void brightness_prepare_for_drag_from_edge(void)
{
    if (!lv_obj_has_flag(brightness_overlay, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

    update_brightness_ui();
    brightness_update_visual_state(BRIGHTNESS_SHEET_CLOSED_Y);
    lv_obj_clear_flag(brightness_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(brightness_overlay);
}

static void brightness_begin_drag(const lv_point_t *point, bool from_edge)
{
    lv_anim_delete(brightness_sheet, brightness_sheet_y_anim_cb);
    lv_anim_delete(brightness_overlay, brightness_scrim_anim_cb);

    brightness_animating = false;
    brightness_dragging = true;
    brightness_drag_from_edge = from_edge;
    brightness_drag_start_point = *point;

    if (from_edge) {
        brightness_prepare_for_drag_from_edge();
        brightness_drag_start_y = BRIGHTNESS_SHEET_CLOSED_Y;
    }
    else {
        brightness_drag_start_y = lv_obj_get_y(brightness_sheet);
    }
}

static void brightness_update_drag(const lv_point_t *point)
{
    int32_t dy;
    int32_t sheet_y;

    if (!brightness_dragging) {
        return;
    }

    dy = point->y - brightness_drag_start_point.y;
    sheet_y = brightness_drag_start_y + dy;
    brightness_update_visual_state(sheet_y);
}

static void brightness_finish_drag(void)
{
    int32_t midpoint = (BRIGHTNESS_SHEET_OPEN_Y + BRIGHTNESS_SHEET_CLOSED_Y) / 2;
    int32_t current_y = lv_obj_get_y(brightness_sheet);
    bool from_edge;

    if (!brightness_dragging) {
        return;
    }

    from_edge = brightness_drag_from_edge;
    brightness_dragging = false;
    brightness_drag_from_edge = false;

    if (from_edge) {
        if (current_y < midpoint) {
            brightness_overlay_animate(true);
        }
        else {
            brightness_overlay_animate(false);
        }
        return;
    }

    if (current_y > midpoint) {
        brightness_overlay_animate(false);
    }
    else {
        brightness_overlay_animate(true);
    }
}

static void brightness_slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    apply_brightness(lv_slider_get_value(slider));
}

static void brightness_overlay_event_cb(lv_event_t *e)
{
    if (lv_event_get_target(e) != lv_event_get_current_target(e)) {
        return;
    }

    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        brightness_overlay_hide();
    }
}

static void brightness_drag_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_event_get_indev(e);
    lv_point_t point;
    lv_obj_t *target = lv_event_get_target(e);
    bool from_edge = (target == brightness_edge_sensor);

    if (indev == NULL) {
        return;
    }

    lv_indev_get_point(indev, &point);

    if (code == LV_EVENT_PRESSED) {
        if (brightness_animating) {
            return;
        }

        if (from_edge && !lv_obj_has_flag(brightness_overlay, LV_OBJ_FLAG_HIDDEN)) {
            return;
        }

        if (!from_edge && lv_obj_has_flag(brightness_overlay, LV_OBJ_FLAG_HIDDEN)) {
            return;
        }

        brightness_begin_drag(&point, from_edge);
        return;
    }

    if (!brightness_dragging) {
        return;
    }

    if (code == LV_EVENT_PRESS_LOST) {
        brightness_dragging = false;
        brightness_drag_from_edge = false;
        if (!lv_obj_has_flag(brightness_overlay, LV_OBJ_FLAG_HIDDEN)) {
            if (lv_obj_get_y(brightness_sheet) > ((BRIGHTNESS_SHEET_OPEN_Y + BRIGHTNESS_SHEET_CLOSED_Y) / 2)) {
                brightness_overlay_animate(false);
            }
            else {
                brightness_overlay_animate(true);
            }
        }
        return;
    }

    if (code == LV_EVENT_PRESSING) {
        brightness_update_drag(&point);
        return;
    }

    if (code == LV_EVENT_RELEASED) {
        brightness_finish_drag();
        return;
    }
}

static void create_brightness_overlay(lv_obj_t *scr)
{
    lv_obj_t *sheet_grabber;

    brightness_overlay = lv_obj_create(scr);
    lv_obj_set_size(brightness_overlay, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_color(brightness_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(brightness_overlay, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(brightness_overlay, 0, 0);
    lv_obj_set_style_radius(brightness_overlay, 0, 0);
    lv_obj_set_style_pad_all(brightness_overlay, 0, 0);
    lv_obj_set_scrollbar_mode(brightness_overlay, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(brightness_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(brightness_overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(brightness_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(brightness_overlay, brightness_overlay_event_cb, LV_EVENT_CLICKED, NULL);

    brightness_sheet = lv_obj_create(brightness_overlay);
    lv_obj_set_size(brightness_sheet, BRIGHTNESS_SHEET_WIDTH, BRIGHTNESS_SHEET_HEIGHT);
    lv_obj_set_pos(brightness_sheet, BRIGHTNESS_SHEET_X, BRIGHTNESS_SHEET_CLOSED_Y);
    lv_obj_set_style_bg_color(brightness_sheet, lv_color_make(0x16, 0x16, 0x16), 0);
    lv_obj_set_style_bg_opa(brightness_sheet, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(brightness_sheet, 0, 0);
    lv_obj_set_style_radius(brightness_sheet, 32, 0);
    lv_obj_set_style_pad_top(brightness_sheet, 28, 0);
    lv_obj_set_style_pad_bottom(brightness_sheet, 24, 0);
    lv_obj_set_style_pad_left(brightness_sheet, 28, 0);
    lv_obj_set_style_pad_right(brightness_sheet, 28, 0);
    lv_obj_set_style_shadow_width(brightness_sheet, 24, 0);
    lv_obj_set_style_shadow_opa(brightness_sheet, LV_OPA_30, 0);
    lv_obj_set_style_shadow_color(brightness_sheet, lv_color_black(), 0);
    lv_obj_set_scrollbar_mode(brightness_sheet, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(brightness_sheet, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(brightness_sheet, brightness_drag_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(brightness_sheet, brightness_drag_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(brightness_sheet, brightness_drag_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(brightness_sheet, brightness_drag_event_cb, LV_EVENT_PRESS_LOST, NULL);

    sheet_grabber = lv_obj_create(brightness_sheet);
    lv_obj_set_size(sheet_grabber, 72, 6);
    lv_obj_set_style_radius(sheet_grabber, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(sheet_grabber, 0, 0);
    lv_obj_set_style_bg_color(sheet_grabber, lv_color_make(0x9C, 0x9C, 0x9C), 0);
    lv_obj_set_style_bg_opa(sheet_grabber, LV_OPA_70, 0);
    lv_obj_set_style_shadow_width(sheet_grabber, 0, 0);
    lv_obj_set_scrollbar_mode(sheet_grabber, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(sheet_grabber, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(sheet_grabber, LV_ALIGN_TOP_MID, 0, -10);
    lv_obj_add_event_cb(sheet_grabber, brightness_drag_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(sheet_grabber, brightness_drag_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(sheet_grabber, brightness_drag_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(sheet_grabber, brightness_drag_event_cb, LV_EVENT_PRESS_LOST, NULL);

    lv_obj_t *title = lv_label_create(brightness_sheet);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_label_set_text(title, "Brightness");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    brightness_value = lv_label_create(brightness_sheet);
    lv_obj_set_style_text_font(brightness_value, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(brightness_value, lv_color_white(), 0);
    lv_obj_align(brightness_value, LV_ALIGN_TOP_MID, 0, 124);

    brightness_slider = lv_slider_create(brightness_sheet);
    lv_obj_set_size(brightness_slider, 100, 16);
    lv_obj_set_width(brightness_slider, lv_pct(100));
    lv_obj_align(brightness_slider, LV_ALIGN_TOP_MID, 0, 88);
    lv_slider_set_range(brightness_slider, 0, 100);
    lv_obj_set_style_bg_color(brightness_slider, lv_color_make(0x36, 0x36, 0x36), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(brightness_slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(brightness_slider, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(brightness_slider, lv_color_white(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(brightness_slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(brightness_slider, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(brightness_slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_outline_width(brightness_slider, 0, LV_PART_KNOB);
    lv_obj_add_event_cb(brightness_slider, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    update_brightness_ui();
}

/* ── Timer callback (1 Hz) ──────────────────────────────── */

static void clock_timer_cb(lv_timer_t *timer)
{
    update_digital_face();
    update_analog_face();
}

/* ── Entry point ────────────────────────────────────────── */

void app_main(void)
{
    /* Set a reasonable default time so the clock isn't at epoch zero */
    struct timeval boot_time = {
        .tv_sec = 1741500000,  /* ~2025-03-09 */
        .tv_usec = 0,
    };
    settimeofday(&boot_time, NULL);

    bsp_display_cfg_t cfg = {
        .lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG(),
        .rotation = ESP_LV_ADAPTER_ROTATE_0,
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL,
        .touch_flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        }
    };
    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();
    bsp_display_brightness_set(50);

    bsp_display_lock(1000);

    styles_init();

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Tileview: horizontal swipe between two tiles */
    tv = lv_tileview_create(scr);
    lv_obj_set_size(tv, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_color(tv, lv_color_black(), 0);
    lv_obj_set_style_pad_all(tv, 0, 0);
    lv_obj_set_style_border_width(tv, 0, 0);
    lv_obj_set_scrollbar_mode(tv, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(tv, LV_OBJ_FLAG_SCROLL_ONE);
    lv_obj_align(tv, LV_ALIGN_CENTER, 0, 0);

    tile_digital = lv_tileview_add_tile(tv, 0, 0, LV_DIR_HOR);
    tile_analog  = lv_tileview_add_tile(tv, 1, 0, LV_DIR_HOR);

    lv_obj_set_style_pad_all(tile_digital, 0, 0);
    lv_obj_set_style_border_width(tile_digital, 0, 0);
    lv_obj_set_style_pad_all(tile_analog, 0, 0);
    lv_obj_set_style_border_width(tile_analog, 0, 0);

    create_digital_face(tile_digital);
    create_analog_face(tile_analog);

    /* Page dots overlay */
    create_dots(scr);
    create_brightness_pull_hint(scr);
    create_brightness_edge_sensor(scr);
    create_brightness_overlay(scr);
    lv_obj_add_event_cb(tv, tv_value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(brightness_edge_sensor, brightness_drag_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(brightness_edge_sensor, brightness_drag_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(brightness_edge_sensor, brightness_drag_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(brightness_edge_sensor, brightness_drag_event_cb, LV_EVENT_PRESS_LOST, NULL);

    /* 1-second update timer */
    lv_timer_create(clock_timer_cb, 1000, NULL);

    /* Initial render */
    update_digital_face();
    update_analog_face();

    bsp_display_unlock();

    ESP_LOGI(TAG, "Clock app started");
}
