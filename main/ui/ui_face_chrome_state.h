#pragma once

/*
 * Face chrome state: controls over the face, theme sheet, quick actions sheet, night overlay.
 *
 * Included by clock_ui_internal.h at the point where the shared helper types are defined;
 * do not include directly. Owned by the face chrome area (ui_shell.c, ui_brightness.c,
 * ui_face_*.c): change it freely, but keep the struct names, because the UI core zeroes them
 * when the chrome is rebuilt for a style change.
 *
 * ui_faces.c reads overlay / animating / dragging of both sheets (it keeps the digital face
 * on a snapshot while a sheet is over it), so those names stay at the top level of the
 * sheet structs. The fields every pull sheet shares are declared once (FACE_SHEET_FIELDS)
 * and reachable both directly and as one face_sheet_t through an anonymous union, so the
 * shared sheet code in ui_face_sheet.c works on either sheet.
 */

#include "ui/kit/ui_kit.h"

#define FACE_SHEET_FIELDS                                                                       \
    struct clock_ui_state_t *ui;                                                               \
    lv_obj_t *overlay;      /* full screen: dim colour, tap outside the panel closes */       \
    lv_obj_t *backdrop_img; /* dimmed face snapshot while the sheet moves */                  \
    lv_obj_t *panel;        /* the sheet itself */                                             \
    lv_obj_t *snapshot_img; /* panel snapshot drawn instead of the panel while it moves */    \
    lv_draw_buf_t *snapshot_buf;                                                               \
    lv_timer_t *auto_close_timer;                                                              \
    void (*on_state)(struct clock_ui_state_t *ui); /* opened, closed or started to move */    \
    uint32_t auto_close_ms;                                                                    \
    int32_t open_y;                                                                            \
    int32_t closed_y;                                                                          \
    int32_t drag_start_y;                                                                      \
    lv_point_t drag_start_point;                                                               \
    lv_opa_t dim_opa;                                                                          \
    bool from_top;                                                                             \
    bool overlay_open;                                                                         \
    bool animating;                                                                            \
    bool dragging;                                                                             \
    bool drag_moved;      /* the current press moved the sheet */                              \
    bool click_blocked;   /* swallow the click that ends a sheet drag */                       \
    bool drag_from_closed;                                                                     \
    bool target_open;                                                                          \
    bool snapshot_visible;                                                                     \
    bool snapshot_dirty;                                                                       \
    uint32_t snapshot_glass; /* Liquid Glass: the backdrop generation the snapshot shows */  \
    bool snapshot_kept;        /* taken ahead at idle: closing keeps the buffer */             \
    uint32_t snapshot_failed;  /* 1 + the generation an idle snapshot found no memory for */  \
    bool freeze_face;          /* at rest too the face behind is the backdrop snapshot */      \
    lv_timer_t *refresh_timer; /* keeps that snapshot up with the face */

typedef struct {
    FACE_SHEET_FIELDS
} face_sheet_t;

typedef enum {
    QUICK_KEY_ALARMS = 0,
    QUICK_KEY_NIGHT,
    QUICK_KEY_SOUND,
    QUICK_KEY_SETTINGS,
    QUICK_KEY_COUNT,
} quick_key_t;

/* Quick actions sheet (pulled up from the bottom): keys and the brightness rim arc. */
typedef struct {
    union {
        face_sheet_t sheet;
        struct {
            FACE_SHEET_FIELDS
        };
    };
    lv_obj_t *edge_sensor;
    lv_obj_t *arc;
    lv_obj_t *value;      /* "40" (with "%" in Orbit) */
    lv_obj_t *percent;    /* "%" in Instrument and Atelier */
    lv_obj_t *keys[QUICK_KEY_COUNT];
    lv_obj_t *key_icons[QUICK_KEY_COUNT];
    lv_obj_t *key_labels[QUICK_KEY_COUNT];
    lv_obj_t *key_leds[QUICK_KEY_COUNT];
    lv_point_t edge_start_point;
    bool edge_tracking;
    bool edge_pulling;
    bool ui_synced;
    bool night_on;
    bool sound_on;
    bool keys_synced;
    uint8_t last_language;
    uint8_t last_ui_percent;
} clock_ui_brightness_state_t;

/* Theme sheet (pulled down from the top). */
typedef struct {
    union {
        face_sheet_t sheet;
        struct {
            FACE_SHEET_FIELDS
        };
    };
    uint32_t synced_key;
    clock_face_id_t picker_face;
    lv_obj_t *eyebrow;
    lv_obj_t *title;
    lv_obj_t *option_card[CLOCK_FACE_THEME_COUNT];
    lv_obj_t *option_thumb[CLOCK_FACE_THEME_COUNT];
    lv_obj_t *option_ring[CLOCK_FACE_THEME_COUNT];
    lv_obj_t *option_key[CLOCK_FACE_THEME_COUNT];  /* "Theme 1" (Instrument, Atelier) */
    lv_obj_t *option_name[CLOCK_FACE_THEME_COUNT];
    lv_obj_t *option_swatches[CLOCK_FACE_THEME_COUNT][3];
    lv_obj_t *check;                               /* check badge on the selected theme */
    face_theme_option_ctx_t option_ctx[CLOCK_FACE_THEME_COUNT];
} clock_ui_face_theme_state_t;

/* Controls shown over the face after a tap, and the night overlay. */
typedef struct {
    lv_obj_t *layer;         /* full screen: the dim, rim decorations, parent of the controls */
    lv_obj_t *themes_tab;    /* pull-down hint at 12 (taps open the theme sheet) */
    lv_obj_t *actions_tab;   /* pull-up hint at 6 */
    lv_obj_t *settings_key;
    lv_obj_t *alarms_key;
    lv_obj_t *badge;         /* update dot on the settings key (Orbit, Atelier) */
    lv_obj_t *dots_plate;    /* Instrument: plate holding the page dots */
    lv_obj_t *dots[CLOCK_FACE_COUNT];
    lv_obj_t *status;        /* Instrument readout plate / Orbit status card */
    lv_obj_t *status_alarm_row;
    lv_obj_t *status_alarm_time;
    lv_obj_t *status_alarm_note;
    lv_obj_t *status_update_row;
    lv_obj_t *status_update_text;
    lv_obj_t *update_chip;   /* Atelier */
    lv_obj_t *alarm_chip;    /* Atelier */
    lv_obj_t *alarm_chip_time;
    lv_obj_t *alarm_chip_note;
    lv_timer_t *hide_timer;
    lv_point_t press_point;  /* where the current press on a control started */
    clock_face_id_t active_face;
    uint8_t dot_count;
    int8_t dot_active;       /* visible index of the active face, -1 before the first sync */
    bool visible;
    int16_t reveal_ms;       /* time into the controls' entrance or exit */
    uint8_t reveal_mode;     /* 0 at rest, 1 coming in, 2 leaving */
    bool hidden;             /* set_hidden(true): settings is open */
    bool update_shown;
    bool alarm_shown;
    long synced_minute;      /* minute the texts were last built for */
    uint8_t synced_language;

    /* Night overlay. */
    lv_obj_t *night;
    lv_obj_t *night_moon;
    lv_obj_t *night_alarm_row;
    lv_obj_t *night_alarm_icon;
    lv_obj_t *night_time;
    lv_obj_t *night_countdown;
    long night_minute;       /* minute the night texts were built for, -1 to force */
    time_t night_alarm;
    clock_face_id_t night_face;
    uint8_t night_language;
    ui_face_ink_t night_ink; /* the face's inks the texts were coloured with */
    bool night_inked;
    bool night_visible;
    lv_draw_buf_t *rim_band[2]; /* Instrument tabs / Orbit bands at 12 and 6, built on first show */
    lv_area_t rim_band_area[2];
} clock_ui_face_chrome_state_t;
