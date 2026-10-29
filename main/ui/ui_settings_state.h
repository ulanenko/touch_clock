#pragma once

/*
 * Settings area state.
 *
 * Included by clock_ui_internal.h at the point where the shared helper types are defined;
 * do not include directly. Owned by the settings area (ui_settings*.c): change it freely, but
 * keep the struct name, because the UI core zeroes it when the chrome is rebuilt for a style
 * change. Every pointer here points into the chrome tree or to buffers freed in
 * ui_settings_destroy().
 */

#include "ui/kit/ui_kit.h"

/* Pages of the settings area. HOME..OTHER are the pages the core knows (ui_settings_page_t);
 * the others are sub pages that belong to one of them. */
typedef enum {
    UI_ST_HOME = 0,
    UI_ST_WIFI,
    UI_ST_NIGHT,
    UI_ST_OTHER,
    UI_ST_TIMEZONE,     /* Wi-Fi */
    UI_ST_MANUAL_TIME,  /* Wi-Fi */
    UI_ST_SCHEDULE,     /* Night */
    UI_ST_FACE,         /* Night */
    UI_ST_NIGHT_BRIGHT, /* Night (Orbit brightness tile) */
    UI_ST_SUNRISE,      /* Night */
    UI_ST_LANGUAGE,     /* Other */
    UI_ST_STYLE,        /* Other */
    UI_ST_PAGE_COUNT,
} ui_st_page_t;

#define UI_ST_NET_MAX 10
#define UI_ST_TZ_MAX 24
#define UI_ST_DIM_MAX 24

/* User data of list rows (networks, time zones, faces, languages, styles). */
typedef struct {
    struct clock_ui_state_t *ui;
    uint8_t index;
} ui_st_item_t;

/* A label or icon that turns text_3 while Night mode is off. */
typedef struct {
    lv_obj_t *obj;
    lv_color_t color;
    bool image;
} ui_st_dim_t;

typedef struct {
    /* Navigation. */
    bool open;
    uint8_t page; /* ui_st_page_t on screen */
    bool built[UI_ST_PAGE_COUNT];
    kit_page_t pages[UI_ST_PAGE_COUNT];
    lv_obj_t *legend[UI_ST_PAGE_COUNT]; /* collapsed title at the top (Instrument, Atelier) */
    bool scrolling;
    bool wifi_scrolling; /* read by the perf tour */
    bool night_scrolling;
    bool other_scrolling;
    int32_t last_minute;
    clock_language_t applied_language;

    /* Home. */
    lv_obj_t *home_status;   /* status readout / line: network part */
    lv_obj_t *home_status_2; /* status: sync part */
    lv_obj_t *home_led;
    lv_obj_t *home_footer;   /* Instrument: version */
    lv_obj_t *home_footer_2; /* Instrument: update ready */
    lv_obj_t *home_update;   /* Instrument LED / Atelier ruby chip on Other */
    lv_obj_t *home_update_label;

    /* Wi-Fi. */
    lv_obj_t *wifi_hero;
    lv_obj_t *wifi_hero_title;
    lv_obj_t *wifi_hero_sub;
    lv_obj_t *wifi_hero_signal;
    lv_obj_t *wifi_hero_mark;
    lv_obj_t *wifi_other_label; /* "Other networks" section */
    lv_obj_t *wifi_net_group;
    lv_obj_t *wifi_net_count;
    lv_obj_t *wifi_scan_btn;
    lv_obj_t *wifi_scan_label;
    lv_obj_t *wifi_tz_title;
    lv_obj_t *wifi_tz_value;
    lv_obj_t *wifi_tz_sub;
    lv_obj_t *wifi_auto_sw;
    lv_obj_t *wifi_auto_sub;
    lv_obj_t *wifi_sync_sub;
    lv_obj_t *wifi_manual_row;
    lv_obj_t *wifi_forget_btn;
    uint32_t wifi_list_gen;
    bool wifi_list_valid;
    bool wifi_list_connected;
    bool wifi_list_scanning;
    char wifi_list_ssid[33];
    ui_st_item_t net_item[UI_ST_NET_MAX];

    /* Join network. */
    lv_obj_t *join_root;
    lv_obj_t *join_title;
    lv_obj_t *join_meta;
    lv_obj_t *join_field;
    lv_obj_t *join_eye;
    lv_obj_t *join_keyboard;
    bool join_open;
    bool join_show_password;
    char join_ssid[33];

    /* Time zone and manual time. */
    lv_obj_t *tz_check[UI_ST_TZ_MAX];
    lv_obj_t *tz_row[UI_ST_TZ_MAX];
    ui_st_item_t tz_item[UI_ST_TZ_MAX];
    lv_obj_t *man_day;
    lv_obj_t *man_month;
    lv_obj_t *man_year;
    lv_obj_t *man_hour;
    lv_obj_t *man_minute;

    /* Night mode. */
    lv_obj_t *night_sw;
    lv_obj_t *night_hint;
    lv_obj_t *night_sched_value;
    lv_obj_t *night_sched_sub;
    lv_obj_t *night_dial;
    lv_obj_t *night_start_value;
    lv_obj_t *night_end_value;
    lv_obj_t *night_face_img;
    lv_obj_t *night_face_sub;
    lv_obj_t *night_bright_slider;
    lv_obj_t *night_bright_value;
    lv_obj_t *night_gauge;
    lv_obj_t *night_sunrise_sw;
    lv_obj_t *night_sunrise_value; /* "20 min · 60%": opens the sunrise's options */
    ui_st_item_t sunrise_len_item[3];
    lv_obj_t *sunrise_len_title[3]; /* "20 minutes before the alarm": composed, so synced */
    lv_obj_t *sunrise_len_check[3];
    ui_st_item_t sunrise_peak_item[3];
    lv_obj_t *sunrise_peak_check[3];
    lv_obj_t *sunrise_chime_sw;
    ui_st_item_t sunrise_face_item[4]; /* Daybreak's three themes, then none */
    lv_obj_t *sunrise_face_title[4];
    lv_obj_t *sunrise_face_check[4];
    lv_obj_t *night_click[6];
    ui_st_dim_t night_dim[UI_ST_DIM_MAX];
    uint8_t night_dim_count;
    lv_obj_t *night_ring;
    lv_obj_t *night_jewel[2];
    int8_t night_drag;        /* jewel being dragged, -1 none */
    uint16_t night_drag_min[2]; /* schedule ends during a drag, minutes of the day */
    bool night_valid;
    bool night_enabled_shown;
    uint32_t night_sched_shown;
    int16_t night_bright_shown;
    clock_face_id_t night_face_shown;
    uint32_t night_face_key;
    lv_draw_buf_t *night_face_buf;

    /* Schedule editor. */
    lv_obj_t *sched_seg;
    lv_obj_t *sched_hour;
    lv_obj_t *sched_minute;
    uint8_t sched_which; /* 0 start, 1 end */

    /* Night face picker. */
    lv_obj_t *face_img[CLOCK_FACE_COUNT];
    lv_obj_t *face_ring[CLOCK_FACE_COUNT];
    lv_obj_t *face_check[CLOCK_FACE_COUNT];
    lv_obj_t *face_name[CLOCK_FACE_COUNT];
    lv_draw_buf_t *face_buf[CLOCK_FACE_COUNT];
    ui_st_item_t face_item[CLOCK_FACE_COUNT];
    uint32_t face_key[CLOCK_FACE_COUNT]; /* what each thumbnail shows (st_face_key) */
    uint32_t face_want[CLOCK_FACE_COUNT]; /* what it should show: redrawn one a tick */
    lv_timer_t *face_timer;
    lv_obj_t *thumb_shell; /* offscreen renderer for round face thumbnails */
    lv_obj_t *thumb_image;

    /* Night brightness page (Orbit). */
    lv_obj_t *nb_slider;
    lv_obj_t *nb_value;

    /* Other. */
    lv_obj_t *other_lang_value;
    lv_obj_t *other_style_value;
    lv_obj_t *other_sound_sw;
    lv_obj_t *other_vol_slider;
    lv_obj_t *other_vol_value;
    lv_obj_t *other_vol_icons[2];
    lv_obj_t *upd_head;
    lv_obj_t *upd_old;
    lv_obj_t *upd_arrow;
    lv_obj_t *upd_sub;
    lv_obj_t *upd_btn;
    lv_obj_t *upd_bar;
    lv_obj_t *upd_bar_fill;
    lv_obj_t *about_anchor;
    lv_obj_t *lang_check[CLOCK_LANGUAGE_COUNT];
    ui_st_item_t lang_item[CLOCK_LANGUAGE_COUNT];
    lv_obj_t *style_check[CLOCK_UI_STYLE_COUNT];
    ui_st_item_t style_item[CLOCK_UI_STYLE_COUNT];
} clock_ui_settings_state_t;
