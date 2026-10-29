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

/* --- Boot ------------------------------------------------------------------------------- */

/* The boot screen over everything, its line in steps parts (the build holds the UI thread:
 * each step draws at once); finish fades it and brings the face up out of the dark. */
void ui_boot_create(clock_ui_context_t *ctx, uint8_t steps);
void ui_boot_step(clock_ui_context_t *ctx);
void ui_boot_finish(clock_ui_context_t *ctx);
void ui_boot_replay(clock_ui_context_t *ctx); /* debug call: the boot screen and the intro again */

/* The sunrise before an alarm (ui_sunrise.c): its glow, and the night face dissolving into the
 * day face when the night hands over. */
void ui_sunrise_create(clock_ui_context_t *ctx);
void ui_sunrise_tick(clock_ui_context_t *ctx, time_t now);
void ui_sunrise_before_face_change(clock_ui_context_t *ctx);
/* True while Daybreak covers the faces: the night overlay and the alarm banner stand aside. */
bool ui_sunrise_covers_face(void);
/* Plays the sunrise in under a minute (Settings > Night > Sunrise), done called when it is over
 * (or tapped away); false while a real one or an alarm is on. */
bool ui_sunrise_preview(clock_ui_context_t *ctx, void (*done)(clock_ui_context_t *ctx));

/* Daybreak, the wake-up face (ui_faces_evening.c), drawn whole into a 720x720 RGB565 buffer. */
typedef struct {
    uint8_t theme;     /* 0 Dawn, 1 Rose, 2 Mist */
    uint16_t progress; /* through the sunrise, 0..1000 */
    time_t now;
    time_t alarm;      /* the alarm it wakes for */
    bool ringing;
} ui_daybreak_view_t;
void ui_daybreak_render(clock_ui_context_t *ctx, lv_draw_buf_t *buf, const ui_daybreak_view_t *view);

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
bool ui_face_chrome_controls_visible(const clock_ui_context_t *ctx); /* tap controls are showing */
/* Top of the spot on a face (in the current style) where alarm details sit clear of the
 * face's own numerals and lettering: the night alarm row, the snooze banner. */
int32_t ui_face_chrome_alarm_slot_y(clock_face_id_t face);

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
/* The snooze / waking-soon banner is on the face (it carries the next alarm then). */
bool ui_alarms_banner_shown(const clock_ui_context_t *ctx, time_t now);
void ui_alarms_close_all(clock_ui_context_t *ctx);
