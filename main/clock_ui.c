#include "clock_ui.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "assets/slava_assets.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"
#include "wifi_time.h"

#define SCREEN_SIZE 720
#define CENTER (SCREEN_SIZE / 2)
#define CLOCK_RADIUS 340
#define HOUR_HAND_LEN 195
#define MIN_HAND_LEN 270
#define SEC_HAND_LEN 310
#define TICK_INNER 315
#define TICK_OUTER 338

#define BOTTOM_EDGE_ZONE 140
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
#define SETTINGS_CLOSE_EDGE_ZONE 80
#define SETTINGS_CLOSE_SWIPE_TRIGGER 26

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

#define WIFI_DIALOG_WIDTH 620
#define WIFI_DIALOG_HEIGHT 440
#define QUICK_ACTION_SYMBOL_BRIGHTNESS LV_SYMBOL_TINT

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
    uint8_t kind;
} alarm_action_ctx_t;

typedef struct {
    bool animating;
    bool dragging;
    bool drag_from_edge;
    lv_point_t drag_start_point;
    int32_t drag_start_y;
    lv_obj_t *overlay;
    lv_obj_t *sheet;
    lv_obj_t *panel_overlay;
    lv_obj_t *panel;
    lv_obj_t *slider;
    lv_obj_t *value;
    lv_obj_t *edge_sensor;
    lv_obj_t *pull_hint;
} clock_ui_brightness_state_t;

typedef struct {
    bool close_dragging;
    bool open;
    uint32_t scan_generation;
    char pending_ssid[33];
    lv_point_t close_drag_start_point;
    lv_obj_t *overlay;
    lv_obj_t *panel;
    lv_obj_t *tabview;
    lv_obj_t *top_sensor;
    lv_obj_t *bottom_sensor;
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
    network_ctx_t network_ctx[WIFI_TIME_MAX_SCAN_RESULTS];
} clock_ui_settings_state_t;

typedef struct {
    bool open;
    bool editor_open;
    bool editor_is_new;
    lv_obj_t *banner;
    lv_obj_t *banner_label;
    lv_obj_t *management_overlay;
    lv_obj_t *management_status;
    lv_obj_t *management_list;
    lv_obj_t *quick_create_row;
    lv_obj_t *manage_snooze_btn;
    lv_obj_t *manage_volume_slider;
    lv_obj_t *manage_volume_label;
    lv_obj_t *manage_test_btn;
    lv_obj_t *list_card[MAX_ALARMS];
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
    lv_obj_t *overlay;
    lv_obj_t *overlay_label;
    lv_obj_t *overlay_subtitle;
    lv_obj_t *snooze_btn;
    lv_obj_t *stop_btn;
    alarm_ctx_t alarm_ctx[MAX_ALARMS];
    alarm_day_ctx_t alarm_day_ctx[MAX_ALARMS][7];
    alarm_action_ctx_t quick_action_ctx[4];
    alarm_config_t editor_draft;
    int8_t editor_index;
} clock_ui_alarm_state_t;

typedef struct {
    lv_obj_t *digital_time_label;
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

static const uint8_t s_matrix_digit_col[4] = {3, 9, 19, 25};
static const uint8_t s_matrix_colon_col = 16;
static const uint8_t s_matrix_digit_row0 = (MTX_GRID_Y - MTX_DIGIT_H) / 2;
static const char *s_day_short[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

enum {
    ALARM_QUICK_CREATE_IN_10 = 0,
    ALARM_QUICK_CREATE_IN_30 = 1,
    ALARM_QUICK_CREATE_TOMORROW_7 = 2,
    ALARM_QUICK_CREATE_WEEKDAYS_7 = 3,
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
static bool brightness_panel_is_open(void);
static void open_settings_tab(uint32_t tab_idx);
static void set_action_button_text(lv_obj_t *button, const char *text);
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
    for (int day = 0; day < 7; ++day) {
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

static time_t compute_alarm_occurrence_for_ui(const alarm_config_t *alarm, time_t now)
{
    struct tm now_tm;

    localtime_r(&now, &now_tm);

    if (!alarm->enabled) {
        return 0;
    }

    if (alarm->repeat_mode == ALARM_REPEAT_ONCE) {
        for (int day_offset = 0; day_offset < 2; ++day_offset) {
            struct tm candidate_tm = now_tm;
            time_t candidate;

            candidate_tm.tm_mday += day_offset;
            candidate_tm.tm_hour = alarm->hour;
            candidate_tm.tm_min = alarm->minute;
            candidate_tm.tm_sec = 0;
            candidate = mktime(&candidate_tm);
            if (candidate > now) {
                return candidate;
            }
        }

        return 0;
    }

    for (int day_offset = 0; day_offset < 8; ++day_offset) {
        struct tm candidate_tm = now_tm;
        struct tm normalized;
        time_t candidate;

        candidate_tm.tm_mday += day_offset;
        candidate_tm.tm_hour = alarm->hour;
        candidate_tm.tm_min = alarm->minute;
        candidate_tm.tm_sec = 0;
        candidate = mktime(&candidate_tm);
        if (candidate <= now) {
            continue;
        }

        localtime_r(&candidate, &normalized);
        if ((alarm->days_mask & (1U << normalized.tm_wday)) == 0) {
            continue;
        }

        return candidate;
    }

    return 0;
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

static lv_obj_t *create_big_action_button(lv_obj_t *parent,
                                          const char *text,
                                          lv_color_t color,
                                          lv_event_cb_t cb,
                                          void *user_data)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_t *label = lv_label_create(button);

    lv_obj_set_size(button, 250, 170);
    lv_obj_set_style_radius(button, 42, 0);
    lv_obj_set_style_bg_color(button, color, 0);
    lv_obj_set_style_bg_color(button, lv_color_darken(color, 25), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(button, lv_color_white(), 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_pad_all(button, 18, 0);
    lv_obj_set_style_shadow_width(button, 16, 0);
    lv_obj_set_style_shadow_opa(button, LV_OPA_20, 0);
    lv_obj_set_style_shadow_color(button, lv_color_black(), 0);

    lv_obj_set_width(label, lv_pct(100));
    lv_obj_set_style_text_font(label, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
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

static void set_action_button_text(lv_obj_t *button, const char *text)
{
    lv_obj_t *label = lv_obj_get_child(button, 0);

    if (label != NULL) {
        lv_label_set_text(label, text);
    }
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
    sync_alarm_controls();
    sync_wifi_controls();
    sync_night_controls();
    update_brightness_ui();
}
