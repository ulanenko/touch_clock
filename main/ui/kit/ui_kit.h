#pragma once

/*
 * Shared components for the chrome, drawn in the current UI style (ui_skin()).
 *
 * Areas build every screen from these parts so the three styles stay consistent across
 * settings, alarms and the sheets. Each function picks geometry, fonts and colours from the
 * skin; areas only choose content and layout. Components are created in the current style
 * and are not restyled later: a style change rebuilds the chrome.
 *
 * The reference for how each part looks in each style is the "System" specimen and the
 * screens in design/ui_concepts/{instrument,orbit,atelier}.html.
 *
 * Performance rules (see docs/performance_audit.md): no layers (no style opa on
 * containers, no clip_corner, no transforms per frame), at most one shadow per screen,
 * images in RGB565/A8, and no style changes per frame unless the value changed.
 */

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"
#include "ui/kit/ui_skin.h"

/* --- Geometry ------------------------------------------------------------------------- */

#define KIT_SCREEN 720
#define KIT_CENTER 360

/* Half width of the visible circle at row y, minus margin (0 outside the circle). */
int32_t kit_circle_half_width(int32_t y, int32_t margin);

/* --- Dials ----------------------------------------------------------------------------
 * Static ring decorations (hairline rings, bezels, minute tracks) are baked once into cached
 * ARGB tiles and drawn as at most 16 images. Never draw them live: lv_draw_arc masks the
 * whole bounding box row by row, and many small draws (60 tick lines) slow the device's draw
 * dispatcher for the whole frame. For arcs that change while dragging, use kit_draw_arc.
 * Angles in degrees, 0 = three o'clock, clockwise. */
#define KIT_DIAL_RINGS 3
#define KIT_DIAL_TICK_SETS 2

typedef struct {
    int32_t r;         /* centre radius of the line or band */
    int32_t width;     /* 0 = unused */
    int32_t start_deg; /* start < end; a span of 360 or more is a full ring */
    int32_t end_deg;
    lv_color_t color;
    lv_opa_t opa;
} kit_dial_ring_t;

typedef struct {
    int32_t count;       /* ticks evenly around the full circle, 0 = unused */
    float offset_deg;    /* angle of tick 0 (-90 = twelve o'clock) */
    int32_t r_in, r_out;
    int32_t major_every; /* 0 = no majors */
    int32_t r_in_major, r_out_major;
    int32_t width, width_major;
    lv_color_t color, color_major;
    lv_opa_t opa;
    uint64_t skip;       /* bit i set: leave tick i out */
} kit_dial_ticks_t;

/* Zero the whole struct before filling it in: it is the cache key. */
typedef struct {
    int32_t cx, cy;
    kit_dial_ring_t rings[KIT_DIAL_RINGS];      /* drawn in order, later on top */
    kit_dial_ticks_t ticks[KIT_DIAL_TICK_SETS]; /* drawn over the rings */
} kit_dial_t;

void kit_dial_draw(lv_layer_t *layer, const kit_dial_t *dial);
/* Bakes a dial into the cache now (at chrome build) so its first frame does not pay for it. */
void kit_dial_prewarm(const kit_dial_t *dial);
/* Bakes the dials the kit itself draws for the current style. */
void kit_prewarm(void);
/* Frees every cached dial; the core calls it when the chrome is rebuilt. */
void kit_dial_cache_flush(void);
/* A single static ring or arc as a cached dial. */
/* Draws a styled lv_arc (track and indicator, as set before this call) with cached rings
 * instead of live arcs. Ring ends are always round. */
void kit_arc_bake(lv_obj_t *arc);
/* lv_draw_arc, but the draw task covers only the arc's bounds instead of its whole circle.
 * Use it for every live arc. */
void kit_draw_arc(lv_layer_t *layer, const lv_draw_arc_dsc_t *dsc);
void kit_draw_ring(lv_layer_t *layer, int32_t cx, int32_t cy, int32_t radius, int32_t width, int32_t start_deg,
                   int32_t end_deg, lv_color_t color, lv_opa_t opa);
/* Atelier chapter ring: rim hairline and minute track; ticks between skip_from_deg and
 * skip_to_deg (degrees from twelve o'clock, inclusive) are left out (-1 for none). */
void kit_draw_minute_rim(lv_layer_t *layer, int32_t skip_from_deg, int32_t skip_to_deg);

/* A band along an arc with round caps and an optional 1 px edge, pre-rendered into one ARGB8888
 * image (rim tabs, rim bands). Angles in degrees, 0 = three o'clock, clockwise, a0 < a1.
 * Returns NULL if out of memory; free with lv_draw_buf_destroy. */
lv_draw_buf_t *kit_arc_band_image(int32_t cx, int32_t cy, int32_t r_mid, int32_t width, float a0, float a1,
                                  lv_color_t fill, lv_opa_t fill_opa, lv_color_t edge, lv_opa_t edge_opa,
                                  lv_area_t *area);

/* --- Text ----------------------------------------------------------------------------- */

typedef enum {
    KIT_TEXT_TITLE,   /* page and sheet titles */
    KIT_TEXT_VALUE,   /* emphasised values */
    KIT_TEXT_HEADING, /* row titles */
    KIT_TEXT_BODY,    /* body text and subtitles (text_2) */
    KIT_TEXT_BUTTON,  /* button labels */
    KIT_TEXT_LABEL,   /* small labels and eyebrows: caps and tracked where the skin says so */
    KIT_TEXT_CAPTION, /* small readouts (text_3) */
    KIT_TEXT_TIME_M,  /* digits: list times, big values */
    KIT_TEXT_TIME_L,  /* digits: editor time */
    KIT_TEXT_DISPLAY, /* digits: ringing time */
} kit_text_role_t;

/* A label in the role's font and colour. Text is copied (and capitalised for LABEL). */
lv_obj_t *kit_label(lv_obj_t *parent, kit_text_role_t role, const char *text);
/* Sets the text of a kit label again, applying its role's capitalisation. */
void kit_label_set_text(lv_obj_t *label, const char *text);
/* Only touches the label when the text differs (cheap to call every tick). */
void kit_label_set_text_if_changed(lv_obj_t *label, const char *text);
const lv_font_t *kit_font(kit_text_role_t role);
/* One line, ending in "…" when too long: a fixed line height and LV_LABEL_LONG_MODE_DOTS.
 * Set the width first. Page titles, eyebrows and row titles already are. */
void kit_label_one_line(lv_obj_t *label);
/* UTF-8 upper case for Latin, Latin Extended-A and Cyrillic. */
void kit_text_upper(char *dst, size_t dst_size, const char *src);

/* --- Icons ---------------------------------------------------------------------------- */

typedef enum {
    KIT_ICON_NONE = 0,
    KIT_ICON_BACK,          /* chevron left */
    KIT_ICON_CLOSE,         /* cross */
    KIT_ICON_CHEVRON_RIGHT,
    KIT_ICON_CHEVRON_DOWN,
    KIT_ICON_CHEVRON_UP,
    KIT_ICON_CHECK,
    KIT_ICON_PLUS,
    KIT_ICON_MINUS,
    KIT_ICON_WIFI,          /* full signal */
    KIT_ICON_WIFI_2,        /* medium signal */
    KIT_ICON_WIFI_1,        /* weak signal */
    KIT_ICON_LOCK,
    KIT_ICON_MOON,
    KIT_ICON_SUN,
    KIT_ICON_SUNRISE,
    KIT_ICON_ALARM,         /* alarm clock */
    KIT_ICON_BELL,
    KIT_ICON_SETTINGS,      /* sliders */
    KIT_ICON_GEAR,
    KIT_ICON_SOUND,
    KIT_ICON_SOUND_OFF,
    KIT_ICON_THEMES,        /* palette / swatches */
    KIT_ICON_BRIGHTNESS,
    KIT_ICON_GLOBE,         /* language, time zone */
    KIT_ICON_DOWNLOAD,      /* software update */
    KIT_ICON_INFO,
    KIT_ICON_CLOCK,
    KIT_ICON_SYNC,          /* refresh / scan */
    KIT_ICON_TRASH,
    KIT_ICON_PLAY,
    KIT_ICON_STOP,
    KIT_ICON_EYE,
    KIT_ICON_EYE_OFF,
    KIT_ICON_BACKSPACE,
    KIT_ICON_REPEAT,
    KIT_ICON_SNOOZE,        /* zZ */
    KIT_ICON_MATH,          /* + − × ÷ */
    KIT_ICON_RESTART,
    KIT_ICON_STYLE,         /* interface style */
    KIT_ICON_COUNT,
} kit_icon_t;

/* An icon image in the current style, tinted with color (A8 image + image_recolor). */
lv_obj_t *kit_icon(lv_obj_t *parent, kit_icon_t icon, lv_color_t color);
void kit_icon_set(lv_obj_t *image, kit_icon_t icon);
/* Image source for drawing an icon yourself (lv_image_set_src / lv_draw_image). */
const lv_image_dsc_t *kit_icon_src(kit_icon_t icon);
/* The same icons at the style's small size (Instrument and Orbit 24 px, Atelier 22 px):
 * row chevrons, locks and checks in rows, icons inside buttons and chips. */
lv_obj_t *kit_icon_small(lv_obj_t *parent, kit_icon_t icon, lv_color_t color);
const lv_image_dsc_t *kit_icon_small_src(kit_icon_t icon);

/* --- Decorations ---------------------------------------------------------------------- */

typedef enum {
    KIT_IMAGE_JEWEL_28,     /* Atelier gilt bead with ruby: slider knob */
    KIT_IMAGE_JEWEL_30,     /* switch knob, on */
    KIT_IMAGE_JEWEL_30_OFF, /* steel bead: switch knob, off */
    KIT_IMAGE_JEWEL_40,     /* rim and arc knobs */
    KIT_IMAGE_LOZENGE,      /* Atelier lozenge ornament on engraved rules (23 px, ink moat) */
} kit_image_t;

/* RGB565A8 images of the kit's decorations (Atelier assets, usable in any style). */
const lv_image_dsc_t *kit_image_src(kit_image_t image);

/* --- Pages ---------------------------------------------------------------------------- */

typedef enum {
    KIT_NAV_NONE,
    KIT_NAV_BACK,  /* back key, top left of the header */
    KIT_NAV_CLOSE, /* close key */
} kit_nav_t;

typedef struct {
    lv_obj_t *root;    /* full-screen page, background skin bg; created hidden */
    lv_obj_t *scroll;  /* the scrolling layer: header and content scroll together (scroll this) */
    lv_obj_t *nav;     /* back/close key or NULL */
    lv_obj_t *action;  /* optional header action key (kit_page_set_action) or NULL */
    lv_obj_t *eyebrow; /* small label above the title, or NULL */
    lv_obj_t *title;
    lv_obj_t *content; /* flex column on the skin's content column, inside scroll */
    lv_obj_t *scrim;   /* fade at the top rim (kit internal) */
} kit_page_t;

/* A full-screen page: header (nav key, eyebrow, title) and a content column. Header and
 * content scroll together in page->scroll (the nav and action keys stay), which ends with
 * enough padding for the last item to scroll clear of the round edge.
 * nav_cb gets user_data on click. */
void kit_page_create(kit_page_t *page,
                     lv_obj_t *parent,
                     const char *eyebrow,
                     const char *title,
                     kit_nav_t nav,
                     lv_event_cb_t nav_cb,
                     void *user_data);
/* Adds a round header action key opposite the nav key (Scan, Add, ...). */
lv_obj_t *kit_page_set_action(kit_page_t *page, kit_icon_t icon, lv_event_cb_t cb, void *user_data);
void kit_page_set_title(kit_page_t *page, const char *title);
/* Atelier draws its chapter ring (minute track) around every page; off for pages that draw
 * their own rim (the night page's 24 h ring). No effect in the other styles. */
void kit_page_set_rim(kit_page_t *page, bool on);
void kit_page_set_eyebrow(kit_page_t *page, const char *eyebrow);

/* --- Groups and rows ------------------------------------------------------------------ */

/* A section label in the content column ("NETWORKS", engraved divider in Atelier). */
lv_obj_t *kit_section_label(lv_obj_t *parent, const char *text);
/* A group that holds rows: platter in Orbit, hairline list in Instrument and Atelier.
 * Width is the parent's content width; height follows its rows. */
lv_obj_t *kit_group(lv_obj_t *parent);
/* A group whose rows share one surface: one Orbit platter (radius 44) with inset hairlines
 * between the rows (About this device, Touch sound + volume). Same as kit_group elsewhere. */
lv_obj_t *kit_group_joined(lv_obj_t *parent);
/* A free-form card (dialogs, readout panels): full content width, height from content,
 * padded flex column. Instrument recessed well, Orbit platter, Atelier framed card. */
lv_obj_t *kit_card(lv_obj_t *parent);
/* A full-width hairline (Atelier: a short engraved rule, centred). */
lv_obj_t *kit_divider(lv_obj_t *parent);
/* Fixed vertical space in a content column. */
lv_obj_t *kit_spacer(lv_obj_t *parent, int32_t height);

typedef enum {
    KIT_TRAIL_NONE,
    KIT_TRAIL_CHEVRON, /* navigates */
    KIT_TRAIL_VALUE,   /* value text, optionally followed by a chevron (see value_chevron) */
    KIT_TRAIL_SWITCH,  /* lv_switch, in row.control */
    KIT_TRAIL_CUSTOM,  /* row.trail is an empty container for the caller */
} kit_trail_t;

typedef struct {
    kit_icon_t icon;     /* leading icon, or KIT_ICON_NONE */
    lv_color_t tint;     /* icon badge tint (Orbit) / icon colour; use ui_skin()->tint_* */
    const char *title;
    const char *subtitle; /* NULL for a single-line row */
    kit_trail_t trail;
    const char *value;    /* for KIT_TRAIL_VALUE */
    bool value_chevron;
    lv_event_cb_t on_click; /* row click (whole row is the target), or NULL */
    void *user_data;
} kit_row_opts_t;

typedef struct {
    lv_obj_t *row;
    lv_obj_t *icon;
    lv_obj_t *title;
    lv_obj_t *subtitle;
    lv_obj_t *value;
    lv_obj_t *control; /* the switch for KIT_TRAIL_SWITCH */
    lv_obj_t *trail;   /* the custom trailing container for KIT_TRAIL_CUSTOM */
} kit_row_t;

void kit_row_create(kit_row_t *row, lv_obj_t *group, const kit_row_opts_t *opts);

/* --- Controls ------------------------------------------------------------------------- */

/* lv_switch in the style; LV_EVENT_VALUE_CHANGED as usual. tint is the "on" colour. */
lv_obj_t *kit_switch(lv_obj_t *parent, lv_color_t tint);
void kit_switch_set(lv_obj_t *sw, bool on); /* without emitting events, only if changed */

/* lv_slider in the style (Instrument ruler, Orbit capsule, Atelier jewel on a hairline).
 * Width is set by the caller; the touch area is extended to the skin's tap_min. */
lv_obj_t *kit_slider(lv_obj_t *parent, int32_t min, int32_t max, lv_color_t tint);
void kit_slider_set(lv_obj_t *slider, int32_t value); /* only if changed, no events */

typedef enum {
    KIT_BUTTON_PRIMARY,     /* the one main action on a screen */
    KIT_BUTTON_SECONDARY,   /* neutral */
    KIT_BUTTON_QUIET,       /* text-like, low emphasis */
    KIT_BUTTON_DESTRUCTIVE,
} kit_button_kind_t;

/* A labelled button (icon optional), height skin button_h, width from content unless set. */
lv_obj_t *kit_button(lv_obj_t *parent, kit_button_kind_t kind, kit_icon_t icon, const char *text);
void kit_button_set_text(lv_obj_t *button, const char *text);
/* A round icon key (back keys, header actions, keypad keys). size 0 uses skin round_button. */
lv_obj_t *kit_round_button(lv_obj_t *parent, kit_button_kind_t kind, kit_icon_t icon, int32_t size);
/* Area tint for a kit_button / kit_round_button (Orbit: Alarms orange, Night indigo, ...).
 * PRIMARY fills with the tint (text black or white for contrast), QUIET becomes a flat wash
 * of the tint, SECONDARY and DESTRUCTIVE round keys tint their icon. Instrument and Atelier
 * keep their single accent and ignore it. */
void kit_button_set_tint(lv_obj_t *button, lv_color_t tint);
/* Area tint for any tintable kit part: buttons (as above), switches and sliders (all styles),
 * and in Orbit the selected segment and day keys (text turns black on bright tints). */
void kit_set_tint(lv_obj_t *obj, lv_color_t tint);
/* A small pill (status chips, tags). active uses the accent. */
lv_obj_t *kit_chip(lv_obj_t *parent, kit_icon_t icon, const char *text, bool active);
void kit_chip_set(lv_obj_t *chip, const char *text, bool active);

/* Segmented control. Emits LV_EVENT_VALUE_CHANGED on the returned object when the user
 * picks a segment; read it with kit_segmented_get. */
lv_obj_t *kit_segmented(lv_obj_t *parent, const char *const *labels, int count, int selected);
void kit_segmented_set(lv_obj_t *seg, int selected);
int kit_segmented_get(lv_obj_t *seg);
void kit_segmented_set_label(lv_obj_t *seg, int index, const char *label);

/* Toggleable round day key (M T W ...). Uses LV_STATE_CHECKED. */
lv_obj_t *kit_day_toggle(lv_obj_t *parent, const char *letter, bool on);

/* lv_roller styled for the skin: selected row in the time font, others dimmed. */
lv_obj_t *kit_roller(lv_obj_t *parent, const char *options, uint32_t visible_rows, const lv_font_t *selected_font);
/* Instrument: the two signal marks either side of the selection window (on by default).
 * Two rollers that share one window turn off their inner marks. No effect elsewhere. */
void kit_roller_set_marks(lv_obj_t *roller, bool left, bool right);

/* Styles an lv_keyboard for the skin (keys, special keys, pressed state). */
void kit_style_keyboard(lv_obj_t *keyboard);
/* Styles an lv_textarea as an input field. */
void kit_style_field(lv_obj_t *textarea);

/* A dialog card centred on a full-screen dim layer over the current screen. Returns the
 * card; the dim layer is its parent (delete or hide that to dismiss). */
lv_obj_t *kit_dialog(lv_obj_t *parent, int32_t width);

/* --- Feedback ------------------------------------------------------------------------- */

/* Pressed look and click sound for custom clickable objects (kit parts already have it). */
void kit_make_pressable(lv_obj_t *obj);

/* --- Specimen ------------------------------------------------------------------------- */

struct clock_ui_state_t;
/* Shows every kit part on scrollable pages for review in the simulator ("call kit_specimen"). */
void ui_kit_specimen_show(struct clock_ui_state_t *ctx);
