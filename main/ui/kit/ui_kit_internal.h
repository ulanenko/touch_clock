#pragma once

/* Shared helpers of the kit implementation (ui_kit*.c). Not for area code. */

#include "ui/kit/ui_kit.h"

void kit_slider_cache_flush(void);

/* Object flags the kit uses to remember things about its own objects. */
#define KIT_FLAG_CAPS LV_OBJ_FLAG_USER_1    /* label text is set in capitals */
#define KIT_FLAG_KIND_A LV_OBJ_FLAG_USER_2  /* button kind, bit 0 */
#define KIT_FLAG_KIND_B LV_OBJ_FLAG_USER_3  /* button kind, bit 1 */
#define KIT_FLAG_ROUND LV_OBJ_FLAG_USER_4   /* kit_round_button */

static inline bool kit_is_instrument(void)
{
    return ui_skin()->style == CLOCK_UI_STYLE_INSTRUMENT;
}

static inline bool kit_is_orbit(void)
{
    return ui_skin()->style == CLOCK_UI_STYLE_ORBIT;
}

static inline bool kit_is_atelier(void)
{
    return ui_skin()->style == CLOCK_UI_STYLE_ATELIER;
}

/* A plain object without theme styles: not clickable, not scrollable, transparent. */
lv_obj_t *kit_plain(lv_obj_t *parent);
/* Dims the object to 70 % while pressed (no recolor, no layer). */
void kit_add_pressed_dim(lv_obj_t *obj);
/* Extends the touch area so the target is at least the skin's tap_min. */
void kit_ensure_tap(lv_obj_t *obj, int32_t w, int32_t h);
/* Text colour with enough contrast on a fill colour (black on bright tints). */
lv_color_t kit_on_color(lv_color_t fill);
/* Flat wash of a tint over the black ground (Orbit tinted buttons). */
lv_color_t kit_wash(lv_color_t tint);
lv_color_t kit_rgb(uint32_t hex);

/* Instrument hardware key: a skirt below the key and a lighter top edge, drawn around the
 * object. checked_only draws it only in LV_STATE_CHECKED (raised segments, day keys). */
typedef struct {
    lv_color_t skirt;
    lv_color_t edge;
    uint8_t depth;
    bool checked_only;
} kit_key_deco_t;

extern const kit_key_deco_t kit_deco_key;
extern const kit_key_deco_t kit_deco_key_signal;
extern const kit_key_deco_t kit_deco_key_checked;
extern const kit_key_deco_t kit_deco_segment;
void kit_add_key_deco(lv_obj_t *obj, const kit_key_deco_t *deco);

/* Label with a role style (no long mode, no width). */
lv_obj_t *kit_label_raw(lv_obj_t *parent, kit_text_role_t role, const char *text);
lv_color_t kit_role_color(kit_text_role_t role);

bool kit_has_event_cb(lv_obj_t *obj, lv_event_cb_t cb);
void kit_remove_event_cb(lv_obj_t *obj, lv_event_cb_t cb);
/* Finds the user data of the first event callback cb registered on obj. */
void *kit_event_data(lv_obj_t *obj, lv_event_cb_t cb);
