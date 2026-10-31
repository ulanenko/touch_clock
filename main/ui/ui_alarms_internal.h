#pragma once

/*
 * Internal interface of the alarms area, shared by ui_alarms*.c only.
 *
 *   ui_alarms.c          hooks, navigation, sync, strings and formatting
 *   ui_alarms_list.c     alarm list and alarm settings pages
 *   ui_alarms_editor.c   alarm editor (repeat, days, sound, math, enabled, delete/save)
 *   ui_alarms_timeset.c  the editor's time setter, one per style
 *   ui_alarms_ring.c     ringing screen, math challenge and the banner on the face
 *
 * Logic is shared and drawing branches on ui_skin_style(): one callback changes a value,
 * each style only decides how it is shown.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "ui/clock_ui_private.h"
#include "ui/kit/ui_kit.h"
#include "ui/kit/ui_skin.h"
#include "ui/ui_areas.h"
#include "ui/ui_format.h"


/* --- Strings (ui_alarms.c) ---------------------------------------------------------- */

extern const ui_l10n_str_t alm_s_alarm_settings;
extern const ui_l10n_str_t alm_s_settings_title;
extern const ui_l10n_str_t alm_s_sound_name;
extern const ui_l10n_str_t alm_s_tap_to_change;
extern const ui_l10n_str_t alm_s_sound_tap;
extern const ui_l10n_str_t alm_s_sound;
extern const ui_l10n_str_t alm_s_repeat;
extern const ui_l10n_str_t alm_s_new_alarm;
extern const ui_l10n_str_t alm_s_test;
extern const ui_l10n_str_t alm_s_stop;
extern const ui_l10n_str_t alm_s_hold;
extern const ui_l10n_str_t alm_s_slide_to_stop;
extern const ui_l10n_str_t alm_s_solve;
extern const ui_l10n_str_t alm_s_solve_long;
extern const ui_l10n_str_t alm_s_tap_to_undo;
extern const ui_l10n_str_t alm_s_custom;

const char *alm_str(const clock_ui_context_t *ctx, const ui_l10n_str_t *str);
const char *alm_text(const clock_ui_context_t *ctx, l10n_text_id_t id);
/* Day key letter(s) for a weekday (0 = Sunday): "M", "Пн", "Wt". */
const char *alm_day_key(const clock_ui_context_t *ctx, int wday);
/* Weekday in display order (Monday first). */
int alm_display_day(int index);

/* --- Formatting (ui_alarms.c) ------------------------------------------------------- */

void alm_format_hm(char *out, size_t size, int hour, int minute);
/* Preset name ("Weekdays") or "Custom". */
const char *alm_repeat_title(const clock_ui_context_t *ctx, const alarm_config_t *alarm);
/* Preset name or the chosen days ("Mon Wed Fri"). */
void alm_repeat_summary(const clock_ui_context_t *ctx, const alarm_config_t *alarm, char *out, size_t size);
/* Next time hour:minute comes round after now. */
time_t alm_next_occurrence(time_t now, int hour, int minute);
/* "Sat 1 Nov" */
void alm_format_short_date(const clock_ui_context_t *ctx, time_t when, char *out, size_t size);
/* "Wednesday, 28 October" */
void alm_format_long_date(const clock_ui_context_t *ctx, time_t when, char *out, size_t size);
/* "3 alarms" */
void alm_format_count(const clock_ui_context_t *ctx, int count, char *out, size_t size);
/* "9 min" */
void alm_format_minutes(const clock_ui_context_t *ctx, int minutes, char *out, size_t size);
bool alm_slot_is_empty(const alarm_config_t *alarm);
int alm_find_new_slot(const clock_ui_context_t *ctx);
bool alm_ringing(const clock_ui_context_t *ctx);
const alarm_config_t *alm_active_alarm(const clock_ui_context_t *ctx);
time_t alm_now(void);

/* --- Navigation (ui_alarms.c) ------------------------------------------------------- */

void alm_list_open(clock_ui_context_t *ctx);
void alm_list_close(clock_ui_context_t *ctx);
void alm_settings_open(clock_ui_context_t *ctx);
void alm_settings_close(clock_ui_context_t *ctx);
void alm_editor_open(clock_ui_context_t *ctx, uint8_t index, bool is_new);
void alm_editor_close(clock_ui_context_t *ctx);
void alm_auto_close_reset(clock_ui_context_t *ctx);
void alm_auto_close_pause(clock_ui_context_t *ctx);
/* Edge swipes close the page (as before the redesign). */
void alm_add_edge_close(clock_ui_context_t *ctx, lv_obj_t *page_root);
/* Re-reads settings and runtime into every page that is built. */
void alm_sync_all(clock_ui_context_t *ctx);
void alm_test_toggle(clock_ui_context_t *ctx);
/* Plays the Test sound from the start (after the sound was changed). */
void alm_sound_preview(clock_ui_context_t *ctx);
/* Name of an alarm_sound_t. */
const ui_l10n_str_t *alm_sound_name(uint8_t sound);
void alm_test_stop(clock_ui_context_t *ctx);

/* --- List and settings pages (ui_alarms_list.c) ------------------------------------- */

void alm_list_create(clock_ui_context_t *ctx);
void alm_list_sync(clock_ui_context_t *ctx, time_t now);
void alm_list_tick(clock_ui_context_t *ctx, time_t now);
void alm_list_close_swipe(clock_ui_context_t *ctx, int8_t index, bool animate);
void alm_list_reveal_delete(clock_ui_context_t *ctx, int8_t index);
void alm_settings_create(clock_ui_context_t *ctx);
void alm_settings_sync(clock_ui_context_t *ctx);

/* --- Editor (ui_alarms_editor.c, ui_alarms_timeset.c) ------------------------------- */

void alm_editor_create(clock_ui_context_t *ctx);
void alm_editor_prepare(clock_ui_context_t *ctx); /* on open: first page, new/existing */
void alm_editor_sync(clock_ui_context_t *ctx);
void alm_editor_tick(clock_ui_context_t *ctx, time_t now);
void alm_editor_show_details(clock_ui_context_t *ctx, bool details, bool animate);
/* The time setter changed the draft. */
void alm_editor_time_changed(clock_ui_context_t *ctx);
void alm_editor_save(clock_ui_context_t *ctx);
void alm_editor_free(clock_ui_context_t *ctx);

void alm_timeset_create(clock_ui_context_t *ctx, lv_obj_t *parent);
void alm_timeset_sync(clock_ui_context_t *ctx);
/* Orbit: the arc starts at the current time. */
void alm_timeset_tick(clock_ui_context_t *ctx, time_t now);
void alm_timeset_free(clock_ui_context_t *ctx);

/* --- Ringing and banner (ui_alarms_ring.c) ------------------------------------------ */

void alm_ring_create(clock_ui_context_t *ctx);
void alm_ring_sync(clock_ui_context_t *ctx, time_t now);
void alm_ring_tick(clock_ui_context_t *ctx, time_t now);
void alm_ring_show_math(clock_ui_context_t *ctx);
void alm_ring_free(clock_ui_context_t *ctx);
void alm_banner_create(clock_ui_context_t *ctx);
void alm_banner_update(clock_ui_context_t *ctx, time_t now);
void alm_banner_style(clock_ui_context_t *ctx, clock_face_id_t face);
bool alm_banner_wanted(const clock_ui_context_t *ctx, time_t now);
void alm_banner_feedback(clock_ui_context_t *ctx, const char *text, bool revertible, time_t now);

/* --- Small drawing helpers (ui_alarms.c) -------------------------------------------- */

/* A plain container: no theme, not clickable, not scrollable. */
lv_obj_t *alm_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text);
/* Single line with dots: fixed to the font's line height. */
void alm_one_line(lv_obj_t *label);
void alm_set_color(lv_obj_t *obj, lv_color_t color, lv_part_t part);
/* Point on a circle around (cx, cy); angle in degrees, 0 = 12 o'clock, clockwise. */
void alm_polar(int32_t cx, int32_t cy, int32_t r, int32_t angle_x10, int32_t *x, int32_t *y);
/* Angle (0.1 degree, 0 = 12 o'clock, clockwise) and distance of a point from (cx, cy). */
int32_t alm_angle_of(int32_t cx, int32_t cy, int32_t x, int32_t y, int32_t *dist);
/* Allocates a w x h RGB565 canvas in PSRAM; *buf receives the pixels. */
lv_obj_t *alm_canvas(lv_obj_t *parent, int32_t w, int32_t h, void **buf);
