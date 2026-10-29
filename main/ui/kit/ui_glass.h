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
/* FACE mode: the face is mostly light (Sternglas, Modern Silver), so the glass is milky and
 * its content dark. */
bool ui_glass_backdrop_light(void);
/* Frees the backdrop and the cached overlays (a style change). */
void ui_glass_release(void);

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

/* A glass band along an arc (rim tabs, the ringing slide), refracting the frost, as an
 * ARGB8888 image. Angles in degrees, 0 = three o'clock, clockwise, a0 < a1. NULL without
 * memory or a backdrop; free with ui_glass_image_free. */
lv_draw_buf_t *ui_glass_arc_image(int32_t cx, int32_t cy, int32_t r_mid, int32_t width, float a0, float a1,
                                  ui_glass_kind_t kind, lv_color_t tint, lv_area_t *area);
void ui_glass_image_free(lv_draw_buf_t *image);
/* The same band blended straight into an RGB565 buffer (a canvas: the ringing screen). */
void ui_glass_arc_blend(lv_draw_buf_t *dst, int32_t dst_x, int32_t dst_y, int32_t cx, int32_t cy, int32_t r_mid,
                        int32_t width, float a0, float a1, ui_glass_kind_t kind, lv_color_t tint);
/* The frost blended into an RGB565 buffer at an offset (a canvas's ground). */
void ui_glass_frost_blit(lv_draw_buf_t *dst, int32_t dst_x, int32_t dst_y);

/* A page coming up: its controls rise into place one after another, and (from_face) its
 * ground fades from the face to the dimmed face. targets: the objects that rise, in order. */
void ui_glass_page_enter(lv_obj_t *root, lv_obj_t *const *targets, int count, bool from_face);

/* A face slide under the face controls: their glass follows what slides under it. The
 * neighbours' frost (slide slots 1 and 2) is built ahead, at idle, for a stamp that says
 * which face it shows; each frame names the two faces and where they are. */
void ui_glass_side_frost(int slot, const lv_draw_buf_t *face, uint32_t stamp);
bool ui_glass_side_frost_ready(int slot, uint32_t stamp);
void ui_glass_live_frame(const lv_draw_buf_t *a, int32_t ax, const lv_draw_buf_t *b, int32_t bx, int b_slot,
                         uint32_t b_stamp);
void ui_glass_live_end(void);
bool ui_glass_live_active(void);
/* During such a slide: an arc image from ui_glass_arc_image (at box, with the same shape)
 * drawn again over what is under it now. */
void ui_glass_arc_live(lv_draw_buf_t *img, const lv_area_t *box, int32_t cx, int32_t cy, int32_t r_mid, int32_t width,
                       float a0, float a1, ui_glass_kind_t kind, lv_color_t tint);

/* Bakes, while the face is idle, the glass of everything under root that has none current yet
 * (hidden pages included), so opening them does not wait; returns true once all is baked. */
bool ui_glass_prewarm(lv_obj_t *root, uint32_t budget_ms);

/* For timing (debug calls): bakes a w x h shape of kind once, as a row on a page would be. */
void ui_glass_debug_bake(int32_t w, int32_t h, int32_t radius, ui_glass_kind_t kind);

/* Makes every chrome object created under root draw as glass where its colours say so. */
void ui_glass_attach(lv_obj_t *root);
