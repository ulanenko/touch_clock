#pragma once
/*
 * Liquid Glass (ui_skin()->glass): chrome surfaces drawn as glass over the face.
 *
 * The face is snapshotted when a glass surface comes up and turned into the frost (what the
 * glass shows: the face blurred, lifted and a little more saturated) and, for pages, the dim
 * (the sharp face darkened: a page's ground). Every chrome object painted with one of the
 * glass skin's marker colours, and every coloured button or bead, then draws as glass: the
 * frost under its shape and a cached overlay with its tint, sheen and specular rim.
 */
#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

typedef enum {
    UI_GLASS_FACE, /* over the face itself (the face controls): clear glass */
    UI_GLASS_PAGE, /* pages and sheets: the face dimmed behind, smoky glass */
} ui_glass_mode_t;

typedef enum {
    UI_GLASS_CLEAR,  /* cards, rows, groups */
    UI_GLASS_RAISED, /* keys, pressed rows: a little brighter */
    UI_GLASS_WELL,   /* recesses: tracks, fields */
    UI_GLASS_SHEET,  /* sheets pulled over the face */
    UI_GLASS_TINT,   /* coloured glass: accent buttons, badges */
} ui_glass_kind_t;

/* Builds the backdrop from a full-screen RGB565 snapshot of the face: what the glass refracts
 * for mode (the face, or the dimmed face for pages) and, with frost_too, the frost (the slow
 * part: only when the face itself changed). */
void ui_glass_backdrop_build(const lv_draw_buf_t *face, ui_glass_mode_t mode, bool frost_too);
/* From the face to a page without a new snapshot: the sharp face is kept in keep_sharp (for
 * the page's entrance) and dimmed in place. False when the backdrop is not the face's. */
bool ui_glass_backdrop_to_page(lv_draw_buf_t *keep_sharp);
bool ui_glass_backdrop_ready(void);
/* Which face (and theme) the backdrop shows: page glass baked for it stays valid while it does. */
void ui_glass_backdrop_identity(uint32_t identity);
ui_glass_mode_t ui_glass_backdrop_mode(void);
/* Counts frost builds: images baked from the frost are stale when it changed. */
uint32_t ui_glass_backdrop_generation(void);
/* Frees the backdrop and the cached overlays (a style change). */
void ui_glass_release(void);
/* Drops the least recently drawn bakes until bytes more of PSRAM would still leave the cache's
 * floor free: called before making a large buffer of one's own (a sheet's snapshot). */
void ui_glass_make_room(size_t bytes);

/* Draws a glass shape over area (screen coordinates), baked for key (the object it is). */
void ui_glass_draw(lv_layer_t *layer, const void *key, const lv_area_t *area, int32_t radius, ui_glass_kind_t kind,
                   lv_color_t tint);
/* A sheet's body where area is: the frost as the sheet shows it, fixed to the screen (a moving
 * sheet's snapshot is drawn over it). */
void ui_glass_draw_sheet_ground(lv_layer_t *layer, const lv_area_t *area, int32_t radius);
/* While true, sheets draw only their rim (the snapshot of a sheet about to move). */
void ui_glass_set_hollow(bool hollow);
/* Clears the body inside a hollow snapshot's corners (key: the sheet; ext: its margin). */
void ui_glass_hollow_corners(const void *key, lv_draw_buf_t *snap, int32_t ext);
/* Draws a page's ground: the dimmed face (black before the first backdrop). */
void ui_glass_draw_ground(lv_layer_t *layer, const lv_area_t *area);

void ui_glass_image_free(lv_draw_buf_t *image);
/* The same band as smoked glass, the look of the face controls: nothing of the backdrop in it,
 * so it is made once and kept. */
lv_draw_buf_t *ui_glass_arc_shell(int32_t cx, int32_t cy, int32_t r_mid, int32_t width, float a0, float a1,
                                  ui_glass_kind_t kind, lv_color_t tint, lv_area_t *area);
/* The same band blended straight into an RGB565 buffer (a canvas: the ringing screen). */
void ui_glass_arc_blend(lv_draw_buf_t *dst, int32_t dst_x, int32_t dst_y, int32_t cx, int32_t cy, int32_t r_mid,
                        int32_t width, float a0, float a1, ui_glass_kind_t kind, lv_color_t tint);
/* The frost blended into an RGB565 buffer at an offset (a canvas's ground). */
void ui_glass_frost_blit(lv_draw_buf_t *dst, int32_t dst_x, int32_t dst_y);

/* A page coming up: its controls rise into place one after another, and (from_face) its
 * ground fades from the face to the dimmed face. targets: the objects that rise, in order. */
void ui_glass_page_enter(lv_obj_t *root, lv_obj_t *const *targets, int count, bool from_face);

/* Bakes, while the face is idle, the glass of everything under root that has none current yet
 * (hidden pages included), so opening them does not wait; returns true once all is baked. */
bool ui_glass_prewarm(lv_obj_t *root, uint32_t budget_ms);
/* Bakes ahead the glass of what under root is off the screen now but on it moved by dy (a
 * page's rows below the fold where they will be once it scrolls by -dy) and has no bake yet;
 * they are baked again where they stop. Returns true once all is baked. */
bool ui_glass_prewarm_at(lv_obj_t *root, int32_t dy, uint32_t budget_ms);
/* As ui_glass_prewarm, but making room as a draw would (the least recently drawn bakes go):
 * for what must come up at once, ahead of everything else. */
bool ui_glass_prewarm_first(lv_obj_t *root, uint32_t budget_ms);

/* For timing (debug calls): bakes a w x h shape of kind once, as a row on a page would be. */
void ui_glass_debug_bake(int32_t w, int32_t h, int32_t radius, ui_glass_kind_t kind);

/* Makes every chrome object created under root draw as glass where its colours say so. */
void ui_glass_attach(lv_obj_t *root);
