#pragma once

/*
 * Face chrome internals, shared by the face chrome files only:
 *
 *   ui_shell.c          faces, face swipe and reveal, backdrop, the ui_face_chrome_* hooks
 *   ui_face_sheet.c     the pull-sheet mechanics shared by the theme and quick actions sheets
 *   ui_face_controls.c  controls over the face (tabs, keys, page dots, status, dim)
 *   ui_face_themes.c    the theme sheet
 *   ui_brightness.c     the quick actions sheet with the brightness rim arc
 *   ui_face_night.c     the night overlay
 *
 * Everything is drawn in the current UI style (ui_skin()); a style change deletes the tree,
 * zeroes the state and calls the create functions again.
 */

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "ui/clock_ui_private.h"
#include "ui/kit/ui_kit.h"
#include "ui/kit/ui_skin.h"

/* --- Small shared helpers (ui_face_controls.c) ----------------------------------------- */

/* A plain object without theme styles: not clickable, not scrollable, transparent. */
lv_obj_t *face_plain(lv_obj_t *parent);
/* A text label in an explicit font and colour (one-off sizes the kit roles do not cover). */
lv_obj_t *face_text(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text);
/* Remembers where presses on obj start, for face_click_is_tap(). */
void face_track_press(clock_ui_context_t *ctx, lv_obj_t *obj);
/* A click whose press stayed in place: a press dragged off a control does not trigger it. */
bool face_click_is_tap(const clock_ui_context_t *ctx, lv_event_t *e);
/* Tightens the letter spacing of a one-line label (down to 0) until it fits max_w. */
void face_fit_label(lv_obj_t *label, int32_t max_w, int32_t letter_space);
/* Upper-cases text into buf when the style sets labels in capitals. */
const char *face_caps(char *buf, size_t size, const char *text);
/* Stops PRESSED/PRESSING/RELEASED from bubbling to the sheet behind a control. */
void face_stop_bubble(lv_obj_t *obj);
/* The screen centre, the centre of every rim decoration. Objects that move (a sheet panel)
 * pass the centre in their own coordinates instead, so snapshots draw them right. */
#define FACE_CENTER ((lv_point_t) {KIT_CENTER, KIT_CENTER})
/* Draws an arc around c, radius to its outer edge (angles in LVGL degrees: 0 at 3 o'clock,
 * clockwise; end - start >= 360 draws the full circle). */
void face_draw_rim_arc(lv_layer_t *layer, lv_point_t c, int32_t radius, int32_t width, int32_t start, int32_t end,
                       lv_color_t color, lv_opa_t opa, bool rounded);
/* A short radial tick at angle (LVGL degrees) from r0 to r1 around c. */
void face_draw_tick(lv_layer_t *layer, lv_point_t c, int32_t r0, int32_t r1, float angle_deg, int32_t width,
                    lv_color_t color);
/* A filled circle centred at (x, y) (absolute screen coordinates). */
void face_draw_dot(lv_layer_t *layer, int32_t x, int32_t y, int32_t r, lv_color_t color, lv_opa_t opa);
/* Atelier engraved rule with the lozenge, width w, centred on parent at y. */
lv_obj_t *face_engraved_rule(lv_obj_t *parent, int32_t y, int32_t w);
/* Next alarm, if any: fills the clock ("06:30") and returns true. */
bool face_next_alarm(const clock_ui_context_t *ctx, time_t now, time_t *when);
/* "8h12m" (localised): the compact countdown of the Instrument readout. */
void face_format_countdown_compact(const clock_ui_context_t *ctx, time_t now, time_t when, char *out, size_t size);
/* Current wall clock time (the simulator overrides time()). */
time_t face_now(void);

/* --- Pull sheets (ui_face_sheet.c) ----------------------------------------------------- */

typedef struct {
    bool from_top;
    int32_t x;
    int32_t width;
    int32_t height;
    int32_t open_y;
    int32_t radius;
    lv_color_t color;
    lv_color_t border;
    lv_border_side_t border_side;
    uint32_t auto_close_ms;
    void (*on_state)(clock_ui_context_t *ctx);
    /* At rest, show the face as a snapshot drawn only around the panel (recaptured when the
     * face changes) instead of live under it: for a sheet whose controls redraw every frame
     * while dragged. */
    bool freeze_face;
} face_sheet_opts_t;

void face_sheet_create(face_sheet_t *sheet, clock_ui_context_t *ctx, const face_sheet_opts_t *opts);
void face_sheet_destroy(face_sheet_t *sheet);
/* The overlay is shown (open, opening, closing or being dragged). */
bool face_sheet_is_visible(const face_sheet_t *sheet);
/* Fully open and still. */
bool face_sheet_is_open(const face_sheet_t *sheet);
bool face_sheet_is_moving(const face_sheet_t *sheet);
void face_sheet_open(face_sheet_t *sheet, bool animate);
void face_sheet_close(face_sheet_t *sheet, bool animate);
/* Dragging the sheet: from a pull on the face (from_closed) or from the open sheet. */
void face_sheet_drag_begin(face_sheet_t *sheet, lv_point_t start, bool from_closed);
void face_sheet_drag_to(face_sheet_t *sheet, int32_t dy);
void face_sheet_drag_end(face_sheet_t *sheet, bool cancel);
/* Lets the user drag the open sheet by obj (the panel, or children that bubble). */
void face_sheet_attach_drag(face_sheet_t *sheet, lv_obj_t *obj);
/* Liquid Glass: bakes a closed sheet's glass where it opens, within budget_ms; false if
 * there is more to do. */
bool face_sheet_prewarm(face_sheet_t *sheet, uint32_t budget_ms);
/* Liquid Glass: takes a closed sheet's snapshot ahead of its pull, when it is out of date (the
 * minute turned, the panel changed); the sheet then keeps it while closed. Whether it took one. */
bool face_sheet_presnapshot(face_sheet_t *sheet);
/* Restarts the auto-close timer (user activity). */
void face_sheet_activity(face_sheet_t *sheet);
/* The panel content changed: take a new snapshot before it moves again. */
void face_sheet_mark_dirty(face_sheet_t *sheet);
/* True once after a press that dragged the sheet: the click that ends it is swallowed. */
bool face_sheet_click_blocked(const face_sheet_t *sheet);

/* --- Controls over the face (ui_face_controls.c) --------------------------------------- */

void face_controls_create(clock_ui_context_t *ctx);
void face_controls_show(clock_ui_context_t *ctx);
/* The same without the entrance, refreshed for the face now shown (after a face slide). */
void face_controls_show_still(clock_ui_context_t *ctx);
/* Keeps the controls up (their hide timer paused) while the faces slide under them. */
void face_controls_hold(clock_ui_context_t *ctx);
void face_controls_hide(clock_ui_context_t *ctx);
void face_controls_toggle(clock_ui_context_t *ctx);
bool face_controls_visible(const clock_ui_context_t *ctx);
void face_controls_set_face(clock_ui_context_t *ctx, clock_face_id_t face);
void face_controls_sync(clock_ui_context_t *ctx, time_t now, bool force);
void face_controls_set_hidden(clock_ui_context_t *ctx, bool hidden);

/* --- Night overlay (ui_face_night.c) --------------------------------------------------- */

void face_night_create(clock_ui_context_t *ctx);
void face_night_sync(clock_ui_context_t *ctx, time_t now, bool force);
void face_night_register_debug(void);

/* --- Theme sheet (ui_face_themes.c) ---------------------------------------------------- */

void face_themes_create(clock_ui_context_t *ctx);
void face_themes_destroy(clock_ui_context_t *ctx);
/* Prepares the sheet for the active face (before a pull or an open). */
void face_themes_prepare(clock_ui_context_t *ctx);
void face_themes_open(clock_ui_context_t *ctx);
void face_themes_close(clock_ui_context_t *ctx, bool animate);
void face_themes_sync(clock_ui_context_t *ctx);
bool face_themes_visible(const clock_ui_context_t *ctx);

/* --- Quick actions (ui_brightness.c) --------------------------------------------------- */

void face_quick_create(clock_ui_context_t *ctx);
void face_quick_create_edge_sensor(clock_ui_context_t *ctx);
void face_quick_destroy(clock_ui_context_t *ctx);
void face_quick_open(clock_ui_context_t *ctx);
void face_quick_close(clock_ui_context_t *ctx, bool animate);
void face_quick_sync(clock_ui_context_t *ctx);
bool face_quick_visible(const clock_ui_context_t *ctx);
void face_quick_set_hidden(clock_ui_context_t *ctx, bool hidden);

/* --- Shell (ui_shell.c) ---------------------------------------------------------------- */

/* Routes presses on obj to the face gestures (tap, pull down, swipe left/right). */
void face_shell_attach_swipe(clock_ui_context_t *ctx, lv_obj_t *obj);
/* Changes face with the slide, without a finger: to the next face for dx < 0. */
void face_shell_swipe_face(clock_ui_context_t *ctx, int32_t dx);
/* Moves the faces with a press that started at `start`: returns true once it drags them
 * sideways (or caught them settling), false while the press is still undecided. */
bool face_shell_slide_drag(clock_ui_context_t *ctx, lv_point_t start, lv_point_t point);
bool face_shell_slide_dragging(const clock_ui_context_t *ctx);
/* The finger lifted from a slide: settles on the nearer face, or the next one after a flick. */
void face_shell_slide_release(clock_ui_context_t *ctx);
/* The face sheets moved or changed state: re-evaluate the live/snapshot digital face. */
void face_shell_sync_face_visuals(clock_ui_context_t *ctx);
clock_face_id_t face_shell_active_face(clock_ui_context_t *ctx);
/* Theme preview colours (dial, marks, hands) of a face theme. */
void face_theme_preview_colors(clock_face_id_t face, uint8_t theme, uint32_t colors[3]);
