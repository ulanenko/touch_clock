#pragma once

/*
 * Alarms area state: banner, list, alarm settings, editor, ringing.
 *
 * Included by clock_ui_internal.h at the point where the shared helper types are defined;
 * do not include directly. Owned by the alarms area (ui_alarms*.c): change it freely, but
 * keep the struct name, because the UI core zeroes it when the chrome is rebuilt for a style
 * change. Every object pointer below lives under the chrome root; the three pixel buffers
 * are freed in ui_alarms_destroy().
 */

#include "ui/kit/ui_kit.h"

/* One alarm row in the list. */
typedef struct {
    lv_obj_t *wrap;        /* swipe holder: row + delete key */
    lv_obj_t *row;         /* the visible row, slides left to reveal delete */
    lv_obj_t *time;
    lv_obj_t *title;       /* repeat preset / summary */
    lv_obj_t *days_box;    /* day strip (Instrument, Atelier) */
    lv_obj_t *days[7];     /* in display order Mon..Sun */
    lv_obj_t *sub;         /* "Sat 1 Nov" / "OFF" under the title */
    lv_obj_t *sw;
    lv_obj_t *del;
    alarm_ctx_t actx;
} alm_row_t;

/* Instrument: drums, Orbit: 24 h ring with rollers, Atelier: hour and minute rings. */
typedef struct {
    lv_obj_t *hour_roller;
    lv_obj_t *minute_roller;
    lv_obj_t *arc;          /* Orbit: now -> alarm arc. Atelier: minute sweep */
    lv_obj_t *now_dot;      /* Orbit */
    lv_obj_t *knob;         /* Orbit alarm knob, Atelier minute jewel */
    lv_obj_t *hour_jewel;   /* Atelier */
    lv_obj_t *hour_jewel_label;
    lv_obj_t *minute_mark;  /* Atelier: gilt label over the 5-minute numeral */
    lv_obj_t *dial;         /* Atelier: pre-rendered dial (canvas) */
    lv_obj_t *touch;        /* Atelier: annulus that takes the drags */
    lv_obj_t *hh;           /* Atelier centre: hours, colon, minutes */
    lv_obj_t *mm;
    lv_obj_t *repeat;       /* Atelier centre eyebrow */
    uint8_t drag_ring;      /* 0 none, 1 hours, 2 minutes */
    int8_t shown_hour;
    int8_t shown_minute;
    int16_t shown_now_angle;
} alm_timeset_t;

typedef struct {
    /* --- navigation ------------------------------------------------------------------ */
    bool open;              /* list */
    bool settings_open;
    bool editor_open;
    bool editor_is_new;
    bool editor_from_list;
    bool close_dragging;
    bool close_swipe_consumed;
    bool management_scrolling;
    bool list_swipe_dragging;
    bool list_swipe_consumed;
    bool banner_visible;
    bool cache_valid;
    bool debug_ringing;     /* preview of the ringing screen without a real alarm */
    bool debug_math;
    int8_t editor_index;
    int8_t focus_alarm_index;
    int8_t swipe_open_index;
    int8_t swipe_drag_index;
    lv_point_t close_drag_start_point;
    lv_point_t list_swipe_start_point;
    ui_edge_ctx_t close_edge_ctx[4];
    lv_timer_t *auto_close_timer;
    alarm_config_t editor_draft;
    alarm_day_ctx_t day_ctx[7];
    long editor_minute;

    /* --- list ------------------------------------------------------------------------ */
    kit_page_t list_page;
    lv_obj_t *list_next;        /* next-up line or readout */
    lv_obj_t *list_next_led;
    lv_obj_t *list_group;
    lv_obj_t *list_add;         /* floating Add key (Instrument, Orbit) or header action */
    lv_obj_t *list_settings_row;
    lv_obj_t *list_settings_title;
    lv_obj_t *list_settings_sub;
    lv_obj_t *list_empty;
    alm_row_t rows[MAX_ALARMS];

    /* --- alarm settings -------------------------------------------------------------- */
    kit_page_t settings_page;
    lv_obj_t *snooze_value;
    lv_obj_t *snooze_unit;
    lv_obj_t *snooze_minus;
    lv_obj_t *snooze_plus;
    lv_obj_t *volume_slider;
    lv_obj_t *volume_value;
    lv_obj_t *settings_test_btn;
    lv_obj_t *settings_sound_sub;
    lv_obj_t *settings_sound_title;
    lv_obj_t *ascending_sw;

    /* --- editor ---------------------------------------------------------------------- */
    lv_obj_t *editor_root;
    lv_obj_t *editor_scroll;    /* Instrument page / Orbit, Atelier pager */
    lv_obj_t *editor_page2;     /* Orbit, Atelier: details page (scrolls inside the pager) */
    lv_obj_t *editor_eyebrow;
    lv_obj_t *editor_time_small; /* Atelier details: "06:30" */
    lv_obj_t *editor_countdown;
    lv_obj_t *editor_close_btn;
    lv_obj_t *editor_delete_btn;
    lv_obj_t *editor_delete_label;
    lv_obj_t *editor_save_btn;
    lv_obj_t *editor_save_label;
    lv_obj_t *editor_save2_btn;  /* Orbit page 1 / Atelier page 1 confirm */
    lv_obj_t *editor_seg;
    lv_obj_t *editor_day_btn[7]; /* by weekday (0 = Sunday) */
    lv_obj_t *editor_test_btn;
    lv_obj_t *editor_sound_sub;
    lv_obj_t *editor_sound_title;
    lv_obj_t *editor_math_sw;
    lv_obj_t *editor_enabled_row;
    lv_obj_t *editor_enabled_sw;
    lv_obj_t *editor_dots[2];    /* Orbit page dots */
    lv_obj_t *editor_bottom_scrim;
    bool editor_on_details;
    uint8_t editor_language;    /* language + 1 of the segment labels */
    alm_timeset_t ts;

    /* --- ringing --------------------------------------------------------------------- */
    lv_obj_t *ring;             /* full-screen overlay */
    lv_obj_t *ring_bg;          /* canvas with the static background (Orbit, Atelier) */
    lv_obj_t *ring_main;        /* time, snooze, stop */
    lv_obj_t *ring_title;
    lv_obj_t *ring_time;
    lv_obj_t *ring_sub;
    lv_obj_t *ring_snooze;
    lv_obj_t *ring_snooze_min;
    lv_obj_t *ring_sec_tick;    /* Instrument: orange second mark on the rim */
    lv_obj_t *ring_hold_btn;    /* Instrument */
    lv_obj_t *ring_hold_arc;
    lv_obj_t *ring_knob;        /* Orbit, Atelier slide */
    bool ring_holding;
    bool ring_sliding;
    bool ring_done;
    int16_t ring_knob_angle;    /* 0.1 degree */
    int16_t ring_sec_shown;
    lv_obj_t *math;             /* math challenge layer */
    lv_obj_t *math_head;
    lv_obj_t *math_error;
    lv_obj_t *math_problem;
    lv_obj_t *math_answer;
    lv_obj_t *math_caret;
    lv_obj_t *math_keys[12];    /* 0-9, backspace, OK */
    lv_obj_t *math_snooze;
    lv_obj_t *math_snooze_label;
    bool math_visible;
    char math_answer_text[4];
    uint8_t math_answer_len;
    uint8_t math_a;
    uint8_t math_b;
    uint8_t math_expected;
    bool math_plus;
    char ring_time_text[8];

    /* --- banner on the face ---------------------------------------------------------- */
    lv_obj_t *banner;
    lv_obj_t *banner_mark;      /* LED (Instrument) or icon */
    lv_obj_t *banner_time;
    lv_obj_t *banner_label;
    time_t banner_feedback_until;
    bool banner_feedback_revertible;
    char banner_feedback_text[48];
    int8_t banner_light_face;   /* -1 unknown */
    uint8_t banner_kind;        /* what the mark currently shows */

    /* --- caches for sync ------------------------------------------------------------- */
    alarm_config_t cached_alarms[MAX_ALARMS];
    bool cached_alarm_ringing;
    bool cached_alarm_test_active;
    bool cached_snooze_active;
    bool cached_ascending_alarm_enabled;
    uint8_t cached_default_sound;
    time_t cached_snooze_deadline;
    time_t cached_next_alarm_epoch;
    int8_t cached_next_alarm_index;
    int8_t cached_active_alarm_index;
    int8_t cached_skipped_alarm_index;
    time_t cached_skipped_alarm_epoch;
    uint8_t cached_alarm_volume;
    uint8_t cached_snooze_minutes;
    uint8_t cached_language;
    long cached_minute;         /* wall-clock minute the countdowns were drawn for */

    /* --- pixel buffers (PSRAM), freed in ui_alarms_destroy ----------------------------- */
    void *dial_buf;             /* Atelier editor dial */
    void *ring_bg_buf;          /* Orbit glow / Atelier sunray */
    uint8_t ring_bg_language;   /* language + 1 the curved Orbit label was set in */
} clock_ui_alarm_state_t;
