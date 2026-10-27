#pragma once

/*
 * Shared helpers of the settings area (ui_settings*.c). Not for other areas.
 *
 *   ui_settings.c        lifecycle, navigation, home page, shared widgets, debug calls
 *   ui_settings_wifi.c   Wi-Fi page, join network, time zone and manual time pages
 *   ui_settings_night.c  Night mode page, schedule editor, night face picker
 *   ui_settings_other.c  Other page, language and interface style pickers
 */

#include "ui/clock_ui_private.h"
#include "ui/kit/ui_kit.h"
#include "ui/kit/ui_skin.h"
#include "ui/ui_areas.h"
#include "ui/ui_format.h"

#define ST(ctx) (&(ctx)->settings_ui)

static inline bool st_instrument(void)
{
    return ui_skin_style() == CLOCK_UI_STYLE_INSTRUMENT;
}

static inline bool st_orbit(void)
{
    return ui_skin_style() == CLOCK_UI_STYLE_ORBIT;
}

static inline bool st_atelier(void)
{
    return ui_skin_style() == CLOCK_UI_STYLE_ATELIER;
}

/* A text that follows the language: a localization id or an area string. */
typedef struct {
    l10n_text_id_t id;
    const ui_l10n_str_t *str;
} st_text_t;

#define ST_ID(x) ((st_text_t) {.id = (x), .str = NULL})
#define ST_STR(p) ((st_text_t) {.id = L10N_COUNT, .str = (p)})
#define ST_NONE ((st_text_t) {.id = L10N_COUNT, .str = NULL})

const char *st_text(const clock_ui_context_t *ctx, st_text_t t);
bool st_text_set(st_text_t t);
/* A kit label bound to a text (relabelled on a language change). */
lv_obj_t *st_label(clock_ui_context_t *ctx, lv_obj_t *parent, kit_text_role_t role, st_text_t t);
void st_bind(clock_ui_context_t *ctx, lv_obj_t *label, st_text_t t);
/* A kit row whose title and subtitle are bound texts. */
void st_row(clock_ui_context_t *ctx, kit_row_t *row, lv_obj_t *group, const kit_row_opts_t *opts, st_text_t title,
            st_text_t subtitle);
/* A plain flex row or column (transparent, not clickable). */
lv_obj_t *st_hbox(lv_obj_t *parent, int32_t gap);
lv_obj_t *st_vbox(lv_obj_t *parent, int32_t gap);
/* A small round dot (Instrument LED). */
lv_obj_t *st_dot(lv_obj_t *parent, int32_t size, lv_color_t color);
/* Wi-Fi signal meter (bars, or arcs in Atelier), 0..4. */
lv_obj_t *st_signal(lv_obj_t *parent, int32_t size);
void st_signal_set(lv_obj_t *meter, int level, bool on_accent);
int st_signal_level(int rssi);
/* An A8 settings asset tinted with color. */
lv_obj_t *st_image(lv_obj_t *parent, const lv_image_dsc_t *src, lv_color_t color);
/* Hides or shows an object (only touches it when the state changes). */
void st_show(lv_obj_t *obj, bool show);
void st_set_enabled(lv_obj_t *obj, bool enabled);
void st_set_clickable(lv_obj_t *obj, bool clickable);
/* Removes the gesture bubble so drags on the control do not navigate back. */
void st_keep_gestures(lv_obj_t *obj);
/* A single-line label of the given width that ends in dots when the text is longer. */
void st_one_line(lv_obj_t *label, int32_t width);
/* Style setters that only touch the object when the value changes (cheap every tick). */
void st_text_color(lv_obj_t *obj, lv_color_t color);
void st_bg_color(lv_obj_t *obj, lv_color_t color);
void st_recolor(lv_obj_t *obj, lv_color_t color);

/* Pages. */
kit_page_t *st_page(clock_ui_context_t *ctx, ui_st_page_t id);
/* Creates the page shell: header, nav key, gestures, scroll tracking, Instrument and
 * Atelier legend and scroll register. */
kit_page_t *st_page_begin(clock_ui_context_t *ctx, ui_st_page_t id, st_text_t eyebrow, st_text_t title,
                          kit_nav_t nav);
void st_page_set_eyebrow_text(clock_ui_context_t *ctx, ui_st_page_t id, const char *text);
void st_show_page(clock_ui_context_t *ctx, ui_st_page_t id);
void st_back(clock_ui_context_t *ctx);
void st_back_event_cb(lv_event_t *e);
ui_st_page_t st_parent(ui_st_page_t id);
/* Round face thumbnail: renders the face into an ARGB8888 buffer once (no live clip). */
void st_render_face_thumb(clock_ui_context_t *ctx, lv_obj_t *image, lv_draw_buf_t **buf, int32_t size,
                          clock_face_id_t face);

/* Formatting. */
void st_format_synced(clock_ui_context_t *ctx, char *out, size_t size, const ui_l10n_str_t *fmt);
void st_format_tz(uint8_t timezone_id, char *name, size_t name_size, char *offset, size_t offset_size);

/* Wi-Fi (ui_settings_wifi.c). */
void st_wifi_build(clock_ui_context_t *ctx);
void st_wifi_sync(clock_ui_context_t *ctx, bool force);
void st_join_open(clock_ui_context_t *ctx, const char *ssid, int rssi, int authmode);
void st_join_close(clock_ui_context_t *ctx);
void st_timezone_build(clock_ui_context_t *ctx);
void st_timezone_sync(clock_ui_context_t *ctx);
void st_timezone_open(clock_ui_context_t *ctx);
void st_manual_build(clock_ui_context_t *ctx);
void st_manual_sync(clock_ui_context_t *ctx, bool force);

/* Night (ui_settings_night.c). */
void st_night_build(clock_ui_context_t *ctx);
void st_night_sync(clock_ui_context_t *ctx, bool force);
void st_schedule_build(clock_ui_context_t *ctx);
void st_schedule_sync(clock_ui_context_t *ctx, bool force);
void st_schedule_open(clock_ui_context_t *ctx, uint8_t which);
void st_face_build(clock_ui_context_t *ctx);
void st_face_sync(clock_ui_context_t *ctx, bool force);
void st_night_bright_build(clock_ui_context_t *ctx);
void st_night_bright_sync(clock_ui_context_t *ctx);
void st_night_destroy(clock_ui_context_t *ctx);

/* Other (ui_settings_other.c). */
void st_other_build(clock_ui_context_t *ctx);
void st_other_sync(clock_ui_context_t *ctx, bool force);
void st_language_build(clock_ui_context_t *ctx);
void st_language_sync(clock_ui_context_t *ctx);
void st_style_build(clock_ui_context_t *ctx);
void st_style_sync(clock_ui_context_t *ctx);
lv_obj_t *st_other_about_anchor(clock_ui_context_t *ctx);
