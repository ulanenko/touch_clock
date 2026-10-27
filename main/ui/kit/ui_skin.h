#pragma once

/*
 * Design tokens for the three UI styles (Instrument, Orbit, Atelier).
 *
 * Every chrome screen takes its colours, fonts and metrics from ui_skin(), never from
 * literals, so a style change only needs the chrome to be rebuilt. The values come from the
 * design presentations in design/ui_concepts/ (instrument.html, orbit.html, atelier.html),
 * which also show how each token is used. All colours are exact in RGB565.
 */

#include <stdbool.h>
#include <stdint.h>

#include "domain/clock_types.h"
#include "lvgl.h"

typedef struct {
    clock_ui_style_t style;

    /* Surfaces, from the deepest to the most raised. */
    lv_color_t bg;          /* screen ground of full pages */
    lv_color_t well;        /* recesses: slider tracks, fields, switch tracks */
    lv_color_t surface;     /* sheets, dialogs, cards */
    lv_color_t raised;      /* keys, platters, pressed rows */
    lv_color_t line;        /* 1 px hairlines and row separators */
    lv_color_t line_strong; /* rules, key edges, inactive ticks */

    /* Text. */
    lv_color_t text;   /* primary */
    lv_color_t text_2; /* secondary: subtitles, values */
    lv_color_t text_3; /* tertiary: hints, inactive marks */

    /* Accent: Instrument signal orange, Orbit system blue, Atelier gilt. */
    lv_color_t accent;
    lv_color_t accent_text;  /* text and icons on an accent fill */
    lv_color_t accent_soft;  /* flat tinted wash behind accent content */
    lv_color_t accent_deep;  /* darker shade: key skirt, gradient end, pressed */
    lv_color_t danger;
    lv_color_t danger_text;  /* destructive text on the surface */
    lv_color_t night;        /* night-mode overlay text and marks */

    /* Per-area tints (Orbit). Instrument and Atelier map them to the accent. */
    lv_color_t tint_wifi;
    lv_color_t tint_night;
    lv_color_t tint_alarms;
    lv_color_t tint_other;
    lv_color_t tint_themes;
    lv_color_t tint_brightness;
    lv_color_t tint_sound;

    lv_opa_t sheet_dim_opa; /* face dim level behind sheets */

    /* Fonts by role. */
    const lv_font_t *font_display; /* digits only: the ringing time */
    const lv_font_t *font_time_l;  /* digits only: the alarm editor time */
    const lv_font_t *font_time_m;  /* digits only: alarm list times, big values (40 %) */
    const lv_font_t *font_title;   /* page and sheet titles */
    const lv_font_t *font_value;   /* emphasised values (Atelier serif values) */
    const lv_font_t *font_heading; /* row titles */
    const lv_font_t *font_body;    /* body text, subtitles */
    const lv_font_t *font_button;  /* button labels */
    const lv_font_t *font_label;   /* small labels and eyebrows (caps where label_caps) */
    const lv_font_t *font_caption; /* small readouts and captions */
    bool label_caps;               /* labels and eyebrows are set in capitals */
    bool button_caps;              /* button labels are set in capitals */
    int8_t label_letter_space;     /* tracking for caps labels, px */

    /* Metrics (px, device coordinates). */
    int16_t content_x;       /* left edge of the page content column */
    int16_t content_w;       /* width of the page content column */
    int16_t page_top;        /* y of the back/close button */
    int16_t title_y;         /* y of the page title */
    int16_t content_top;     /* y where page content starts */
    int16_t row_h;           /* single-line row height */
    int16_t row_h_2;         /* two-line row height */
    int16_t radius_s;        /* keys, fields, chips */
    int16_t radius_m;        /* cards, panels, dialogs */
    int16_t radius_l;        /* sheets */
    int16_t gap_xs, gap_s, gap_m, gap_l, gap_xl; /* spacing scale */
    int16_t button_h;        /* standard button height */
    int16_t round_button;    /* round key / back button diameter */
    int16_t switch_w, switch_h;
    int16_t icon;            /* standard icon size */
    int16_t tap_min;         /* minimum touch target (ext_click_area tops up the rest) */
} ui_skin_t;

const ui_skin_t *ui_skin(void);
clock_ui_style_t ui_skin_style(void);
void ui_skin_set_style(clock_ui_style_t style);

/* Localised display name of a style, for the style picker. */
const char *ui_skin_style_name(clock_ui_style_t style);
