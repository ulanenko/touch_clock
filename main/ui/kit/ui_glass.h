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

/* Makes every chrome object created under root draw as glass where its colours say so. */
void ui_glass_attach(lv_obj_t *root);
