#pragma once

/*
 * The UI is split into the core (faces, face navigation, the 1 s tick, l10n, the chrome
 * lifecycle in ui_chrome.c) and three chrome areas, each drawn in the current UI style:
 *
 *   face chrome  ui_shell.c, ui_brightness.c  controls over the face, theme sheet,
 *                                              quick actions sheet, night overlay
 *   settings     ui_settings.c                 settings home, Wi-Fi, join, night, other
 *   alarms       ui_alarms.c                   banner, list, alarm settings, editor, ringing
 *
 * The core and the areas talk to each other only through the functions below. Every chrome
 * object must be created under ui_chrome_root(), every chrome timer with
 * ui_chrome_timer_create(), and animations must use a chrome object as their var, so the
 * whole chrome can be deleted and rebuilt when the style changes.
 */

#include <stdbool.h>
#include <time.h>

#include "ui/clock_ui_internal.h"

/* --- Core: chrome lifecycle (ui_chrome.c) ------------------------------------------------ */

lv_obj_t *ui_chrome_root(const clock_ui_context_t *ctx);
lv_timer_t *ui_chrome_timer_create(clock_ui_context_t *ctx, lv_timer_cb_t cb, uint32_t period_ms, void *user_data);
void ui_chrome_build(clock_ui_context_t *ctx);
/* Rebuilds the chrome in the current settings style after the running event finishes. */
void ui_chrome_rebuild_async(clock_ui_context_t *ctx);
/* Refreshes every area from settings and runtime state. */
void ui_chrome_refresh(clock_ui_context_t *ctx);

typedef void (*ui_debug_call_t)(clock_ui_context_t *ctx);
/* Named entry points (open a screen, set a state) for the simulator and the perf tour. */
void ui_debug_register(const char *name, ui_debug_call_t fn);
bool ui_debug_call(clock_ui_context_t *ctx, const char *name);

/* --- Face chrome ------------------------------------------------------------------------- */

void ui_face_chrome_create(clock_ui_context_t *ctx);     /* created below the other areas */
void ui_face_chrome_create_top(clock_ui_context_t *ctx); /* created above the other areas */
void ui_face_chrome_destroy(clock_ui_context_t *ctx);    /* free non-object resources */
void ui_face_chrome_tick(clock_ui_context_t *ctx, time_t now, clock_face_id_t active_face, bool face_covered);
void ui_face_chrome_refresh(clock_ui_context_t *ctx);
void ui_face_chrome_on_face_changed(clock_ui_context_t *ctx, clock_face_id_t face);
void ui_face_chrome_show_controls(clock_ui_context_t *ctx);
void ui_face_chrome_set_hidden(clock_ui_context_t *ctx, bool hidden);
void ui_face_chrome_close_all(clock_ui_context_t *ctx);
bool ui_face_chrome_covers_face(const clock_ui_context_t *ctx); /* an opaque surface is open */
bool ui_face_chrome_dims_face(const clock_ui_context_t *ctx);   /* a sheet over the face is open */

/* --- Settings ---------------------------------------------------------------------------- */

typedef enum {
    UI_SETTINGS_PAGE_NONE = 0, /* settings closed */
    UI_SETTINGS_PAGE_HOME,
    UI_SETTINGS_PAGE_WIFI,
    UI_SETTINGS_PAGE_NIGHT,
    UI_SETTINGS_PAGE_OTHER,
} ui_settings_page_t;

void ui_settings_create(clock_ui_context_t *ctx);
void ui_settings_destroy(clock_ui_context_t *ctx);
void ui_settings_tick(clock_ui_context_t *ctx, time_t now);
void ui_settings_refresh(clock_ui_context_t *ctx);
void ui_settings_open(clock_ui_context_t *ctx);
void ui_settings_open_page(clock_ui_context_t *ctx, ui_settings_page_t page);
ui_settings_page_t ui_settings_current_page(const clock_ui_context_t *ctx);
bool ui_settings_is_open(const clock_ui_context_t *ctx);
void ui_settings_close_all(clock_ui_context_t *ctx);

/* --- Alarms ------------------------------------------------------------------------------ */

void ui_alarms_create(clock_ui_context_t *ctx);
void ui_alarms_destroy(clock_ui_context_t *ctx);
void ui_alarms_tick(clock_ui_context_t *ctx, time_t now, bool face_covered);
void ui_alarms_refresh(clock_ui_context_t *ctx);
void ui_alarms_on_face_changed(clock_ui_context_t *ctx, clock_face_id_t face);
void ui_alarms_open(clock_ui_context_t *ctx);
bool ui_alarms_is_open(const clock_ui_context_t *ctx);
void ui_alarms_close_all(clock_ui_context_t *ctx);
