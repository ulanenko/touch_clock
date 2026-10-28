/*
 * Shared chrome components in the three UI styles: geometry, icons, pages, groups, rows,
 * buttons, chips, cards and dialogs. Controls (switch, slider, segmented, day keys, roller,
 * keyboard, field) are in ui_kit_controls.c, text in ui_kit_text.c.
 *
 * Everything is built from solid fills, borders, outlines, small A8/RGB565A8 images and a
 * few custom draws (Instrument key skirts, the Atelier rim) that only draw lines and rects.
 */
#include <math.h>
#include <string.h>

#include "assets/ui_assets/kit_assets.h"
#include "ui/kit/ui_kit_internal.h"
#include "ui/ui_controls.h"

/* --- Geometry ------------------------------------------------------------------------- */

int32_t kit_circle_half_width(int32_t y, int32_t margin)
{
    int32_t r = KIT_CENTER - margin;
    int32_t dy = y - KIT_CENTER;

    if (r <= 0 || dy <= -r || dy >= r) {
        return 0;
    }
    return (int32_t)sqrtf((float)(r * r - dy * dy));
}

/* --- Small helpers ---------------------------------------------------------------------- */

lv_color_t kit_rgb(uint32_t hex)
{
    return lv_color_hex(hex);
}

lv_obj_t *kit_plain(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);

    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE);
    return obj;
}

void kit_add_pressed_dim(lv_obj_t *obj)
{
    static lv_style_t pressed;
    static bool ready;

    if (!ready) {
        lv_style_init(&pressed);
        lv_style_set_opa(&pressed, LV_OPA_70);
        lv_style_set_recolor_opa(&pressed, LV_OPA_TRANSP);
        ready = true;
    }
    lv_obj_add_style(obj, &pressed, LV_STATE_PRESSED);
}

void kit_ensure_tap(lv_obj_t *obj, int32_t w, int32_t h)
{
    int32_t small = LV_MIN(w, h);
    int32_t min = ui_skin()->tap_min;

    if (small > 0 && small < min) {
        lv_obj_set_ext_click_area(obj, (min - small + 1) / 2);
    }
}

lv_color_t kit_on_color(lv_color_t fill)
{
    uint32_t luma = (uint32_t)fill.red * 299 + (uint32_t)fill.green * 587 + (uint32_t)fill.blue * 114;

    return luma > 165 * 1000 ? lv_color_black() : lv_color_hex(0xF8FCF8);
}

lv_color_t kit_wash(lv_color_t tint)
{
    /* Orbit washes (#183350 for #0A84FF, #4E391A for #FF9F0A) are the tint at about 30 %. */
    return lv_color_mix(tint, lv_color_black(), 78);
}

bool kit_has_event_cb(lv_obj_t *obj, lv_event_cb_t cb)
{
    uint32_t n = lv_obj_get_event_count(obj);

    for (uint32_t i = 0; i < n; i++) {
        lv_event_dsc_t *dsc = lv_obj_get_event_dsc(obj, i);

        if (dsc != NULL && lv_event_dsc_get_cb(dsc) == cb) {
            return true;
        }
    }
    return false;
}

void kit_remove_event_cb(lv_obj_t *obj, lv_event_cb_t cb)
{
    /* Backwards: lv_obj_remove_event_cb skips the second of two adjacent matches. */
    for (int32_t i = (int32_t)lv_obj_get_event_count(obj) - 1; i >= 0; i--) {
        lv_event_dsc_t *dsc = lv_obj_get_event_dsc(obj, (uint32_t)i);

        if (dsc != NULL && lv_event_dsc_get_cb(dsc) == cb) {
            lv_obj_remove_event(obj, (uint32_t)i);
        }
    }
}

void *kit_event_data(lv_obj_t *obj, lv_event_cb_t cb)
{
    uint32_t n = lv_obj_get_event_count(obj);

    for (uint32_t i = 0; i < n; i++) {
        lv_event_dsc_t *dsc = lv_obj_get_event_dsc(obj, i);

        if (dsc != NULL && lv_event_dsc_get_cb(dsc) == cb) {
            return lv_event_dsc_get_user_data(dsc);
        }
    }
    return NULL;
}

void kit_make_pressable(lv_obj_t *obj)
{
    if (obj == NULL) {
        return;
    }
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_PRESS_LOCK);
    kit_add_pressed_dim(obj);
    ui_attach_click_feedback(obj, LV_EVENT_CLICKED);
}

/* --- Instrument key decoration ---------------------------------------------------------- */

const kit_key_deco_t kit_deco_key = {
    .skirt = LV_COLOR_MAKE(0x08, 0x08, 0x08),
    .edge = LV_COLOR_MAKE(0x48, 0x44, 0x40),
    .depth = 3,
};
const kit_key_deco_t kit_deco_key_signal = {
    .skirt = LV_COLOR_MAKE(0x78, 0x2C, 0x08),
    .edge = LV_COLOR_MAKE(0xF8, 0x90, 0x60),
    .depth = 3,
};
const kit_key_deco_t kit_deco_key_checked = {
    .skirt = LV_COLOR_MAKE(0x08, 0x08, 0x08),
    .edge = LV_COLOR_MAKE(0x48, 0x44, 0x40),
    .depth = 3,
    .checked_only = true,
};
const kit_key_deco_t kit_deco_segment = {
    .skirt = LV_COLOR_MAKE(0x08, 0x08, 0x08),
    .edge = LV_COLOR_MAKE(0x48, 0x44, 0x40),
    .depth = 2,
    .checked_only = true,
};

static void key_deco_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_current_target(e);
    const kit_key_deco_t *deco = lv_event_get_user_data(e);
    lv_layer_t *layer;
    lv_area_t a;
    int32_t radius;
    lv_opa_t opa;

    if (code == LV_EVENT_REFR_EXT_DRAW_SIZE) {
        lv_event_set_ext_draw_size(e, deco->depth);
        return;
    }
    if (deco->checked_only && !lv_obj_has_state(obj, LV_STATE_CHECKED)) {
        return;
    }
    layer = lv_event_get_layer(e);
    lv_obj_get_coords(obj, &a);
    radius = lv_obj_get_style_radius(obj, LV_PART_MAIN);
    radius = LV_MIN(radius, LV_MIN(lv_area_get_width(&a), lv_area_get_height(&a)) / 2);
    opa = lv_obj_get_style_opa_recursive(obj, LV_PART_MAIN);

    if (code == LV_EVENT_DRAW_MAIN_BEGIN) {
        /* The skirt: the key's shape again, a few px lower, under the key. */
        lv_draw_rect_dsc_t d;

        lv_draw_rect_dsc_init(&d);
        d.bg_color = deco->skirt;
        d.bg_opa = opa;
        d.radius = radius;
        lv_area_move(&a, 0, deco->depth);
        lv_draw_rect(layer, &d, &a);
    } else if (code == LV_EVENT_DRAW_MAIN_END) {
        /* The lit top edge of the keycap. */
        if (radius * 2 >= lv_area_get_height(&a)) {
            /* A cached ring: one per key size, shared by every key of that size. */
            kit_draw_ring(layer, a.x1 + lv_area_get_width(&a) / 2, a.y1 + lv_area_get_height(&a) / 2, radius - 1, 1, 215,
                          325, deco->edge, opa);
        } else {
            lv_draw_rect_dsc_t d;
            lv_area_t edge = {a.x1 + radius / 2, a.y1, a.x2 - radius / 2, a.y1};

            lv_draw_rect_dsc_init(&d);
            d.bg_color = deco->edge;
            d.bg_opa = opa;
            lv_draw_rect(layer, &d, &edge);
        }
    }
}

void kit_add_key_deco(lv_obj_t *obj, const kit_key_deco_t *deco)
{
    lv_obj_add_event_cb(obj, key_deco_cb, LV_EVENT_REFR_EXT_DRAW_SIZE, (void *)deco);
    lv_obj_add_event_cb(obj, key_deco_cb, LV_EVENT_DRAW_MAIN_BEGIN, (void *)deco);
    lv_obj_add_event_cb(obj, key_deco_cb, LV_EVENT_DRAW_MAIN_END, (void *)deco);
    lv_obj_refresh_ext_draw_size(obj);
}

/* --- Icons ------------------------------------------------------------------------------ */

#define KIT_ICON_LIST(X)                                                                        \
    X(BACK, back) X(CLOSE, close) X(CHEVRON_RIGHT, chevron_right) X(CHEVRON_DOWN, chevron_down)  \
    X(CHEVRON_UP, chevron_up) X(CHECK, check) X(PLUS, plus) X(MINUS, minus) X(WIFI, wifi)         \
    X(WIFI_2, wifi_2) X(WIFI_1, wifi_1) X(LOCK, lock) X(MOON, moon) X(SUN, sun)                   \
    X(SUNRISE, sunrise) X(ALARM, alarm) X(BELL, bell) X(SETTINGS, settings) X(GEAR, gear)          \
    X(SOUND, sound) X(SOUND_OFF, sound_off) X(THEMES, themes) X(BRIGHTNESS, brightness)          \
    X(GLOBE, globe) X(DOWNLOAD, download) X(INFO, info) X(CLOCK, clock) X(SYNC, sync)            \
    X(TRASH, trash) X(PLAY, play) X(STOP, stop) X(EYE, eye) X(EYE_OFF, eye_off)                  \
    X(BACKSPACE, backspace) X(REPEAT, repeat) X(SNOOZE, snooze) X(MATH, math)                    \
    X(RESTART, restart) X(STYLE, style)

#define ICON_I(e, n) [KIT_ICON_##e] = &ui_kit_instrument_##n,
#define ICON_O(e, n) [KIT_ICON_##e] = &ui_kit_orbit_##n,
#define ICON_A(e, n) [KIT_ICON_##e] = &ui_kit_atelier_##n,
#define ICON_IS(e, n) [KIT_ICON_##e] = &ui_kit_instrument_sm_##n,
#define ICON_OS(e, n) [KIT_ICON_##e] = &ui_kit_orbit_sm_##n,
#define ICON_AS(e, n) [KIT_ICON_##e] = &ui_kit_atelier_sm_##n,
#define ICON_OL(e, n) [KIT_ICON_##e] = &ui_kit_orbit_lg_##n,

static const lv_image_dsc_t *const s_icons[3][KIT_ICON_COUNT] = {
    [CLOCK_UI_STYLE_INSTRUMENT] = {KIT_ICON_LIST(ICON_I)},
    [CLOCK_UI_STYLE_ORBIT] = {KIT_ICON_LIST(ICON_O)},
    [CLOCK_UI_STYLE_ATELIER] = {KIT_ICON_LIST(ICON_A)},
};
static const lv_image_dsc_t *const s_icons_small[3][KIT_ICON_COUNT] = {
    [CLOCK_UI_STYLE_INSTRUMENT] = {KIT_ICON_LIST(ICON_IS)},
    [CLOCK_UI_STYLE_ORBIT] = {KIT_ICON_LIST(ICON_OS)},
    [CLOCK_UI_STYLE_ATELIER] = {KIT_ICON_LIST(ICON_AS)},
};

/* Orbit scales the glyph with its large round keys. */
static const lv_image_dsc_t *const s_icons_orbit_large[KIT_ICON_COUNT] = {KIT_ICON_LIST(ICON_OL)};

static unsigned style_index(void)
{
    unsigned s = (unsigned)ui_skin()->style;

    return s < 3 ? s : (unsigned)CLOCK_UI_STYLE_ORBIT;
}

const lv_image_dsc_t *kit_icon_src(kit_icon_t icon)
{
    if (icon <= KIT_ICON_NONE || icon >= KIT_ICON_COUNT) {
        return NULL;
    }
    return s_icons[style_index()][icon];
}

const lv_image_dsc_t *kit_icon_small_src(kit_icon_t icon)
{
    if (icon <= KIT_ICON_NONE || icon >= KIT_ICON_COUNT) {
        return NULL;
    }
    return s_icons_small[style_index()][icon];
}

static lv_obj_t *icon_image(lv_obj_t *parent, const lv_image_dsc_t *src, lv_color_t color)
{
    lv_obj_t *img = lv_image_create(parent);

    if (src != NULL) {
        lv_image_set_src(img, src);
    }
    lv_obj_set_style_image_recolor(img, color, 0);
    lv_obj_set_style_image_recolor_opa(img, LV_OPA_COVER, 0);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(img, LV_OBJ_FLAG_EVENT_BUBBLE);
    return img;
}

const lv_image_dsc_t *kit_image_src(kit_image_t image)
{
    switch (image) {
    case KIT_IMAGE_JEWEL_28:
        return &ui_kit_atelier_jewel_28;
    case KIT_IMAGE_JEWEL_30:
        return &ui_kit_atelier_jewel_on_30;
    case KIT_IMAGE_JEWEL_30_OFF:
        return &ui_kit_atelier_jewel_off_30;
    case KIT_IMAGE_JEWEL_40:
        return &ui_kit_atelier_jewel_40;
    case KIT_IMAGE_LOZENGE:
        return &ui_kit_atelier_lozenge;
    default:
        return NULL;
    }
}

lv_obj_t *kit_icon(lv_obj_t *parent, kit_icon_t icon, lv_color_t color)
{
    return icon_image(parent, kit_icon_src(icon), color);
}

lv_obj_t *kit_icon_small(lv_obj_t *parent, kit_icon_t icon, lv_color_t color)
{
    return icon_image(parent, kit_icon_small_src(icon), color);
}

void kit_icon_set(lv_obj_t *image, kit_icon_t icon)
{
    const lv_image_dsc_t *src;
    const lv_image_dsc_t *cur;

    if (image == NULL) {
        return;
    }
    cur = lv_image_get_src(image);
    /* Keep the size the image was created with. */
    if (cur != NULL && cur->header.w > 28 && kit_is_orbit() && icon > KIT_ICON_NONE && icon < KIT_ICON_COUNT) {
        src = s_icons_orbit_large[icon];
    } else {
        src = (cur != NULL && cur->header.w < 28) ? kit_icon_small_src(icon) : kit_icon_src(icon);
    }
    if (src != cur) {
        lv_image_set_src(image, src);
    }
}

/* --- Buttons ---------------------------------------------------------------------------- */

static void set_kind(lv_obj_t *obj, kit_button_kind_t kind)
{
    if (kind & 1) {
        lv_obj_add_flag(obj, KIT_FLAG_KIND_A);
    }
    if (kind & 2) {
        lv_obj_add_flag(obj, KIT_FLAG_KIND_B);
    }
}

static kit_button_kind_t get_kind(lv_obj_t *obj)
{
    return (kit_button_kind_t)((lv_obj_has_flag(obj, KIT_FLAG_KIND_A) ? 1 : 0) |
                               (lv_obj_has_flag(obj, KIT_FLAG_KIND_B) ? 2 : 0));
}

static lv_obj_t *button_base(lv_obj_t *parent, kit_button_kind_t kind)
{
    lv_obj_t *btn = lv_button_create(parent);

    lv_obj_remove_style_all(btn);
    /* No press lock: sliding off a button cancels it, so a flick that starts on one (an Add
     * or Save key) scrolls or does nothing instead of clicking it on release. */
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ON_FOCUS | LV_OBJ_FLAG_PRESS_LOCK);
    set_kind(btn, kind);
    kit_add_pressed_dim(btn);
    ui_attach_click_feedback(btn, LV_EVENT_CLICKED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    return btn;
}

/* Foreground (text and icon) colour of a button kind. */
static lv_color_t button_fg(kit_button_kind_t kind)
{
    const ui_skin_t *sk = ui_skin();

    switch (kind) {
    case KIT_BUTTON_PRIMARY:
        return sk->accent_text;
    case KIT_BUTTON_DESTRUCTIVE:
        return sk->danger_text;
    case KIT_BUTTON_QUIET:
        if (kit_is_orbit()) {
            return sk->accent;
        }
        return kit_is_atelier() ? sk->value : sk->text;
    case KIT_BUTTON_SECONDARY:
    default:
        return kit_is_atelier() ? sk->value : sk->text;
    }
}

static void style_button_surface(lv_obj_t *btn, kit_button_kind_t kind, bool round)
{
    const ui_skin_t *sk = ui_skin();

    if (kit_is_instrument()) {
        lv_obj_set_style_radius(btn, round ? LV_RADIUS_CIRCLE : sk->radius_s, 0);
        if (kind == KIT_BUTTON_QUIET && round) {
            lv_obj_set_style_bg_color(btn, sk->well, 0);
            lv_obj_set_style_border_width(btn, 1, 0);
            lv_obj_set_style_border_color(btn, sk->line, 0);
            return;
        }
        lv_obj_set_style_border_width(btn, 1, 0);
        if (kind == KIT_BUTTON_PRIMARY) {
            lv_obj_set_style_bg_color(btn, sk->accent, 0);
            lv_obj_set_style_border_color(btn, kit_rgb(0xF87830), 0);
            kit_add_key_deco(btn, &kit_deco_key_signal);
        } else {
            lv_obj_set_style_bg_color(btn, sk->raised, 0);
            lv_obj_set_style_border_color(btn, sk->line, 0);
            kit_add_key_deco(btn, &kit_deco_key);
        }
    } else if (kit_is_orbit()) {
        lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
        switch (kind) {
        case KIT_BUTTON_PRIMARY:
            lv_obj_set_style_bg_color(btn, sk->accent, 0);
            break;
        case KIT_BUTTON_QUIET:
            lv_obj_set_style_bg_color(btn, round ? sk->surface : sk->accent_soft, 0);
            break;
        case KIT_BUTTON_DESTRUCTIVE:
            lv_obj_set_style_bg_color(btn, round ? sk->raised : sk->danger_soft, 0);
            break;
        case KIT_BUTTON_SECONDARY:
        default:
            lv_obj_set_style_bg_color(btn, sk->raised, 0);
            break;
        }
    } else {
        /* Atelier: framed in brass; round keys are pushers with a second ring. */
        lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        switch (kind) {
        case KIT_BUTTON_PRIMARY:
            if (round) {
                lv_obj_set_style_bg_color(btn, sk->accent, 0);
            } else {
                lv_obj_set_style_bg_color(btn, kit_rgb(0xE0C890), 0);
                lv_obj_set_style_bg_grad_color(btn, kit_rgb(0xC8A470), 0);
                lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_VER, 0);
            }
            lv_obj_set_style_border_color(btn, kit_rgb(round ? 0xE8D8B8 : 0xF0E0B8), 0);
            break;
        case KIT_BUTTON_QUIET:
            if (round) {
                lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
                lv_obj_set_style_border_color(btn, sk->line_strong, 0);
            } else {
                lv_obj_set_style_bg_color(btn, sk->surface, 0);
                lv_obj_set_style_border_color(btn, sk->accent_deep, 0);
            }
            break;
        case KIT_BUTTON_DESTRUCTIVE:
            lv_obj_set_style_bg_color(btn, sk->surface, 0);
            lv_obj_set_style_border_color(btn, sk->danger, 0);
            break;
        case KIT_BUTTON_SECONDARY:
        default:
            lv_obj_set_style_bg_color(btn, sk->surface, 0);
            lv_obj_set_style_border_color(btn, sk->accent_deep, 0);
            break;
        }
        if (round && kind != KIT_BUTTON_QUIET) {
            lv_obj_set_style_outline_width(btn, 1, 0);
            lv_obj_set_style_outline_pad(btn, 4, 0);
            lv_obj_set_style_outline_color(btn, sk->line, 0);
        }
        if (round) {
            kit_ring_border(btn);
        }
    }
}

lv_obj_t *kit_button(lv_obj_t *parent, kit_button_kind_t kind, kit_icon_t icon, const char *text)
{
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *btn = button_base(parent, kind);
    lv_color_t fg = button_fg(kind);
    int32_t h = sk->button_h;
    int32_t pad = 24;
    lv_obj_t *label;

    if (kit_is_orbit()) {
        pad = 30;
        if (kind == KIT_BUTTON_QUIET || kind == KIT_BUTTON_DESTRUCTIVE) {
            h = 56; /* wash pills */
            pad = 22;
        }
    } else if (kit_is_atelier()) {
        pad = 28;
        if (kind == KIT_BUTTON_QUIET) {
            h = 52; /* the small framed button (Sync now, Test) */
            pad = 22;
        }
    } else if (kind == KIT_BUTTON_QUIET) {
        h = 56; /* the small tool key with a printed legend (Sync, Test) */
        pad = 18;
    }
    style_button_surface(btn, kind, false);
    lv_obj_set_size(btn, LV_SIZE_CONTENT, h);
    lv_obj_set_style_pad_hor(btn, pad, 0);
    lv_obj_set_style_pad_column(btn, kit_is_orbit() ? 10 : 12, 0);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    kit_ensure_tap(btn, 200, h);
    if (icon != KIT_ICON_NONE) {
        kit_icon_small(btn, icon, fg);
        if (kind == KIT_BUTTON_QUIET && kit_is_instrument()) {
            lv_obj_set_style_pad_left(btn, 14, 0);
        }
    }
    if (kind == KIT_BUTTON_QUIET && !kit_is_orbit()) {
        label = kit_label_raw(btn, KIT_TEXT_LABEL, text);
        if (kit_is_atelier()) {
            lv_obj_set_style_text_letter_space(label, 2, 0);
        }
    } else {
        label = kit_label_raw(btn, KIT_TEXT_BUTTON, text);
    }
    lv_obj_set_style_text_color(label, fg, 0);
    if (kit_is_atelier() && kind == KIT_BUTTON_PRIMARY) {
        lv_obj_set_style_text_color(label, sk->accent_text, 0);
    }
    return btn;
}

void kit_button_set_text(lv_obj_t *button, const char *text)
{
    uint32_t n;

    if (button == NULL) {
        return;
    }
    n = lv_obj_get_child_count(button);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *child = lv_obj_get_child(button, (int32_t)i);

        if (lv_obj_check_type(child, &lv_label_class)) {
            kit_label_set_text_if_changed(child, text);
            return;
        }
    }
}

lv_obj_t *kit_round_button(lv_obj_t *parent, kit_button_kind_t kind, kit_icon_t icon, int32_t size)
{
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *btn = button_base(parent, kind);
    lv_color_t fg = button_fg(kind);
    lv_obj_t *img;

    if (size <= 0) {
        size = sk->round_button;
    }
    lv_obj_add_flag(btn, KIT_FLAG_ROUND);
    style_button_surface(btn, kind, true);
    lv_obj_set_size(btn, size, size);
    kit_ensure_tap(btn, size, size);
    if (kit_is_instrument()) {
        fg = kind == KIT_BUTTON_PRIMARY ? sk->accent_text
             : kind == KIT_BUTTON_QUIET ? sk->text_2
             : kind == KIT_BUTTON_DESTRUCTIVE ? sk->danger_text
                                            : sk->text;
    } else if (kit_is_orbit()) {
        fg = kind == KIT_BUTTON_PRIMARY ? sk->accent_text
             : kind == KIT_BUTTON_QUIET ? sk->accent
             : kind == KIT_BUTTON_DESTRUCTIVE ? sk->danger
                                            : sk->text;
    } else {
        fg = kind == KIT_BUTTON_PRIMARY ? sk->accent_text
             : kind == KIT_BUTTON_DESTRUCTIVE ? sk->danger_text
                                            : sk->value;
    }
    if (icon != KIT_ICON_NONE) {
        if (kit_is_orbit() && size >= 80) {
            img = icon_image(btn, s_icons_orbit_large[icon], fg);
        } else {
            img = kit_icon(btn, icon, fg);
        }
        lv_obj_center(img);
    }
    return btn;
}

void kit_button_set_tint(lv_obj_t *button, lv_color_t tint)
{
    kit_button_kind_t kind;
    bool round;
    lv_color_t fg;
    bool set_fg = true;
    uint32_t n;

    if (button == NULL || !kit_is_orbit()) {
        return;
    }
    kind = get_kind(button);
    round = lv_obj_has_flag(button, KIT_FLAG_ROUND);
    switch (kind) {
    case KIT_BUTTON_PRIMARY:
        lv_obj_set_style_bg_color(button, tint, 0);
        fg = kit_on_color(tint);
        break;
    case KIT_BUTTON_QUIET:
        if (!round) {
            lv_obj_set_style_bg_color(button, kit_wash(tint), 0);
        }
        fg = tint;
        break;
    case KIT_BUTTON_SECONDARY:
        fg = tint;
        set_fg = round;
        break;
    default:
        fg = tint;
        set_fg = false;
        break;
    }
    if (!set_fg) {
        return;
    }
    n = lv_obj_get_child_count(button);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *child = lv_obj_get_child(button, (int32_t)i);

        if (lv_obj_check_type(child, &lv_image_class)) {
            lv_obj_set_style_image_recolor(child, fg, 0);
        } else if (lv_obj_check_type(child, &lv_label_class)) {
            lv_obj_set_style_text_color(child, fg, 0);
        }
    }
}

/* --- Chips ------------------------------------------------------------------------------ */

static void chip_apply(lv_obj_t *chip, bool active)
{
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *mark = lv_obj_get_child(chip, 0);
    lv_obj_t *label = lv_obj_get_child(chip, 1);
    bool is_icon = mark != NULL && lv_obj_check_type(mark, &lv_image_class);

    if (kit_is_instrument()) {
        if (is_icon) {
            lv_obj_set_style_image_recolor(mark, active ? sk->accent : sk->text_2, 0);
        } else if (mark != NULL) {
            lv_obj_set_style_bg_color(mark, active ? sk->accent : sk->well, 0);
            lv_obj_set_style_border_width(mark, active ? 0 : 1, 0);
        }
        lv_obj_set_style_text_color(label, active ? sk->text : sk->text_2, 0);
    } else if (kit_is_orbit()) {
        lv_obj_set_style_bg_color(chip, active ? sk->accent : kit_rgb(0x181C20), 0);
        lv_obj_set_style_border_color(chip, active ? sk->accent : kit_rgb(0x303030), 0);
        if (is_icon) {
            lv_obj_set_style_image_recolor(mark, active ? sk->accent_text : sk->accent, 0);
        }
        lv_obj_set_style_text_color(label, active ? sk->accent_text : sk->text, 0);
    } else {
        if (is_icon) {
            lv_obj_set_style_image_recolor(mark, active ? sk->accent : sk->value, 0);
        } else if (mark != NULL) {
            if (active) {
                lv_obj_remove_flag(mark, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(mark, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

lv_obj_t *kit_chip(lv_obj_t *parent, kit_icon_t icon, const char *text, bool active)
{
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *chip = kit_plain(parent);
    lv_obj_t *mark;
    lv_obj_t *label;

    lv_obj_set_size(chip, LV_SIZE_CONTENT, kit_is_orbit() ? 56 : 44);
    lv_obj_set_style_radius(chip, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(chip, 1, 0);
    lv_obj_set_flex_flow(chip, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(chip, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    if (kit_is_instrument()) {
        lv_obj_set_style_bg_color(chip, sk->well, 0);
        lv_obj_set_style_border_color(chip, sk->line, 0);
        lv_obj_set_style_pad_hor(chip, 18, 0);
        lv_obj_set_style_pad_column(chip, 12, 0);
    } else if (kit_is_orbit()) {
        lv_obj_set_style_pad_left(chip, 18, 0);
        lv_obj_set_style_pad_right(chip, 22, 0);
        lv_obj_set_style_pad_column(chip, 10, 0);
    } else {
        lv_obj_set_style_bg_color(chip, sk->surface, 0);
        lv_obj_set_style_border_color(chip, sk->accent_deep, 0);
        lv_obj_set_style_pad_hor(chip, 18, 0);
        lv_obj_set_style_pad_column(chip, 10, 0);
    }

    if (icon != KIT_ICON_NONE) {
        mark = kit_icon_small(chip, icon, sk->text_2);
    } else if (kit_is_orbit()) {
        mark = kit_plain(chip); /* no mark in Orbit: an empty slot keeps the child order */
        lv_obj_set_size(mark, 0, 0);
        lv_obj_set_style_margin_right(mark, -10, 0);
    } else {
        int32_t d = kit_is_instrument() ? 8 : 10;

        mark = kit_plain(chip);
        lv_obj_set_size(mark, d, d);
        lv_obj_set_style_radius(mark, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(mark, LV_OPA_COVER, 0);
        if (kit_is_instrument()) {
            lv_obj_set_style_border_color(mark, sk->line_strong, 0);
        } else {
            lv_obj_set_style_bg_color(mark, sk->danger, 0);
            lv_obj_set_style_border_width(mark, 2, 0);
            lv_obj_set_style_border_color(mark, sk->danger_soft, 0);
        }
    }
    if (kit_is_instrument()) {
        label = kit_label_raw(chip, KIT_TEXT_CAPTION, text);
    } else if (kit_is_orbit()) {
        label = kit_label_raw(chip, KIT_TEXT_VALUE, text);
    } else {
        label = kit_label_raw(chip, KIT_TEXT_LABEL, text);
        lv_obj_set_style_text_color(label, sk->value, 0);
    }
    chip_apply(chip, active);
    return chip;
}

void kit_chip_set(lv_obj_t *chip, const char *text, bool active)
{
    if (chip == NULL) {
        return;
    }
    kit_label_set_text_if_changed(lv_obj_get_child(chip, 1), text);
    chip_apply(chip, active);
}

/* --- Groups, cards, dividers ------------------------------------------------------------- */

static void column(lv_obj_t *obj)
{
    lv_obj_set_width(obj, lv_pct(100));
    lv_obj_set_height(obj, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_COLUMN);
}

lv_obj_t *kit_group(lv_obj_t *parent)
{
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *group = kit_plain(parent);

    column(group);
    if (kit_is_instrument()) {
        /* Rows draw their top rule, the group closes with the bottom rule. */
        lv_obj_set_style_border_side(group, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_width(group, 1, 0);
        lv_obj_set_style_border_color(group, sk->line, 0);
    } else if (kit_is_orbit()) {
        lv_obj_set_style_pad_row(group, 8, 0);
        lv_obj_set_style_pad_bottom(group, 16, 0);
    }
    return group;
}

lv_obj_t *kit_group_joined(lv_obj_t *parent)
{
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *wrap;
    lv_obj_t *group;

    if (!kit_is_orbit()) {
        return kit_group(parent);
    }
    /* The wrap keeps the 16 px gap below the platter that Orbit groups have. */
    wrap = kit_plain(parent);
    column(wrap);
    lv_obj_set_style_pad_bottom(wrap, 16, 0);
    group = kit_plain(wrap);
    column(group);
    lv_obj_set_style_bg_opa(group, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(group, sk->surface, 0);
    lv_obj_set_style_radius(group, sk->radius_m, 0);
    lv_obj_add_flag(group, LV_OBJ_FLAG_USER_1); /* marks a joined group for its rows */
    return group;
}

lv_obj_t *kit_card(lv_obj_t *parent)
{
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *card = kit_plain(parent);

    column(card);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(card, 24, 0);
    lv_obj_set_style_pad_row(card, sk->gap_xs, 0);
    if (kit_is_instrument()) {
        lv_obj_set_style_bg_color(card, sk->well, 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, sk->line, 0);
        lv_obj_set_style_radius(card, sk->radius_m, 0);
    } else if (kit_is_orbit()) {
        lv_obj_set_style_bg_color(card, sk->surface, 0);
        lv_obj_set_style_radius(card, sk->radius_m, 0);
        lv_obj_set_style_pad_left(card, 18, 0);
    } else {
        lv_obj_set_style_bg_color(card, sk->surface, 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, sk->accent_deep, 0);
        lv_obj_set_style_radius(card, sk->radius_m, 0);
        lv_obj_set_style_pad_left(card, 28, 0);
    }
    return card;
}

/* Atelier engraved rule: a dark 1 px line over a light 1 px line. */
static lv_obj_t *engraved_rule(lv_obj_t *parent, int32_t width)
{
    lv_obj_t *rule = kit_plain(parent);

    lv_obj_set_size(rule, width, 2);
    lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(rule, kit_rgb(0x000008), 0);
    lv_obj_set_style_border_side(rule, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(rule, 1, 0);
    lv_obj_set_style_border_color(rule, kit_rgb(0x282830), 0);
    return rule;
}

lv_obj_t *kit_divider(lv_obj_t *parent)
{
    lv_obj_t *div = kit_plain(parent);

    lv_obj_set_width(div, lv_pct(100));
    if (kit_is_atelier()) {
        lv_obj_t *rule;

        lv_obj_set_height(div, 2);
        rule = engraved_rule(div, 120);
        lv_obj_align(rule, LV_ALIGN_CENTER, 0, 0);
    } else {
        lv_obj_set_height(div, 1);
        lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(div, ui_skin()->line, 0);
    }
    return div;
}

void kit_label_one_line(lv_obj_t *label)
{
    if (label == NULL) {
        return;
    }
    /* A fixed one-line height: with LV_SIZE_CONTENT a dotted label still wraps. */
    lv_obj_set_height(label, lv_font_get_line_height(lv_obj_get_style_text_font(label, LV_PART_MAIN)));
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
}

lv_obj_t *kit_spacer(lv_obj_t *parent, int32_t height)
{
    lv_obj_t *sp = kit_plain(parent);

    lv_obj_set_size(sp, lv_pct(100), height);
    return sp;
}

lv_obj_t *kit_section_label(lv_obj_t *parent, const char *text)
{
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *sec = kit_plain(parent);
    lv_obj_t *label;

    lv_obj_set_width(sec, lv_pct(100));
    lv_obj_set_flex_flow(sec, LV_FLEX_FLOW_ROW);
    if (kit_is_instrument()) {
        /* Legend on the left, sitting on the rule of the first row. */
        lv_obj_set_height(sec, 52);
        lv_obj_set_style_pad_bottom(sec, 10, 0);
        lv_obj_set_flex_align(sec, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
        label = kit_label_raw(sec, KIT_TEXT_LABEL, text);
    } else if (kit_is_orbit()) {
        lv_obj_set_height(sec, 32);
        lv_obj_set_flex_align(sec, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        label = kit_label_raw(sec, KIT_TEXT_LABEL, text);
    } else {
        /* Atelier: brass hairlines either side of a centred legend. */
        lv_obj_t *l;

        lv_obj_set_height(sec, 56);
        lv_obj_set_style_pad_top(sec, 14, 0);
        lv_obj_set_style_pad_column(sec, sk->gap_s, 0);
        lv_obj_set_flex_align(sec, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        l = kit_plain(sec);
        lv_obj_set_size(l, 36, 1);
        lv_obj_set_style_bg_opa(l, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(l, sk->accent_deep, 0);
        label = kit_label_raw(sec, KIT_TEXT_LABEL, text);
        l = kit_plain(sec);
        lv_obj_set_size(l, 36, 1);
        lv_obj_set_style_bg_opa(l, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(l, sk->accent_deep, 0);
    }
    return label;
}

/* --- Rows ------------------------------------------------------------------------------- */

/* Orbit joined groups: an inset hairline above every row but the first. */
static void inset_rule_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_current_target(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_draw_rect_dsc_t d;
    lv_area_t a;

    lv_obj_get_coords(obj, &a);
    a.x1 += 28;
    a.x2 -= 28;
    a.y2 = a.y1;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = ui_skin()->line;
    lv_draw_rect(layer, &d, &a);
}

static lv_obj_t *row_leading(lv_obj_t *row, const kit_row_opts_t *opts)
{
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *holder = kit_plain(row);
    lv_obj_t *icon;

    lv_obj_set_style_radius(holder, kit_is_instrument() ? sk->radius_s : LV_RADIUS_CIRCLE, 0);
    if (kit_is_instrument()) {
        /* A recessed tile. */
        lv_obj_set_size(holder, 56, 56);
        lv_obj_set_style_bg_opa(holder, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(holder, sk->well, 0);
        lv_obj_set_style_border_width(holder, 1, 0);
        lv_obj_set_style_border_color(holder, sk->line, 0);
        icon = kit_icon(holder, opts->icon, sk->text_2);
    } else if (kit_is_orbit()) {
        /* A badge in the area tint. */
        lv_color_t tint = opts->tint;

        if (lv_color_eq(tint, lv_color_black())) {
            tint = sk->accent;
        }
        lv_obj_set_size(holder, 56, 56);
        lv_obj_set_style_bg_opa(holder, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(holder, tint, 0);
        icon = kit_icon(holder, opts->icon, kit_on_color(tint));
    } else {
        /* A sub-dial: brass ring with an inner hairline ring. The 42 px hairline is the
         * border and the 52 px brass ring its outline, drawn as one cached dial; the margin
         * keeps the 52 px footprint. */
        lv_obj_set_size(holder, 42, 42);
        lv_obj_set_style_margin_all(holder, 5, 0);
        lv_obj_set_style_border_width(holder, 1, 0);
        lv_obj_set_style_border_color(holder, sk->line, 0);
        lv_obj_set_style_outline_width(holder, 1, 0);
        lv_obj_set_style_outline_pad(holder, 4, 0);
        lv_obj_set_style_outline_color(holder, sk->accent_deep, 0);
        kit_ring_border(holder);
        icon = kit_icon_small(holder, opts->icon, sk->value);
    }
    lv_obj_center(icon);
    return holder;
}

void kit_row_create(kit_row_t *row, lv_obj_t *group, const kit_row_opts_t *opts)
{
    const ui_skin_t *sk = ui_skin();
    bool joined = kit_is_orbit() && lv_obj_has_flag(group, LV_OBJ_FLAG_USER_1);
    bool first = lv_obj_get_child_count(group) == 0;
    bool two = opts->subtitle != NULL;
    lv_obj_t *r;
    lv_obj_t *col;
    int32_t min_h = two ? sk->row_h_2 : sk->row_h;

    memset(row, 0, sizeof(*row));
    r = kit_plain(group);
    row->row = r;
    lv_obj_set_width(r, lv_pct(100));
    lv_obj_set_height(r, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_ver(r, 10, 0);

    if (kit_is_instrument()) {
        if (opts->icon != KIT_ICON_NONE) {
            min_h += 16;
        }
        lv_obj_set_style_pad_column(r, sk->gap_s, 0);
        lv_obj_set_style_border_side(r, LV_BORDER_SIDE_TOP, 0);
        lv_obj_set_style_border_width(r, 1, 0);
        lv_obj_set_style_border_color(r, sk->line, 0);
        lv_obj_set_style_bg_color(r, sk->surface, LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_STATE_PRESSED);
    } else if (kit_is_orbit()) {
        lv_obj_set_style_pad_column(r, 18, 0);
        lv_obj_set_style_pad_left(r, opts->icon != KIT_ICON_NONE ? 18 : 28, 0);
        lv_obj_set_style_pad_right(r, 24, 0);
        if (joined) {
            if (!first) {
                lv_obj_add_event_cb(r, inset_rule_cb, LV_EVENT_DRAW_MAIN_END, NULL);
            }
        } else {
            lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(r, sk->surface, 0);
            lv_obj_set_style_radius(r, 32, 0);
            lv_obj_set_style_bg_color(r, sk->raised, LV_STATE_PRESSED);
        }
    } else {
        lv_obj_set_style_pad_column(r, sk->gap_s, 0);
        if (!first) {
            lv_obj_set_style_border_side(r, LV_BORDER_SIDE_TOP, 0);
            lv_obj_set_style_border_width(r, 1, 0);
            lv_obj_set_style_border_color(r, sk->line, 0);
        }
        lv_obj_set_style_bg_color(r, sk->surface, LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_STATE_PRESSED);
    }

    if (opts->on_click != NULL) {
        lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(r, opts->on_click, LV_EVENT_CLICKED, opts->user_data);
        ui_attach_click_feedback(r, LV_EVENT_CLICKED);
        if (joined) {
            kit_add_pressed_dim(r);
        }
    }

    if (opts->icon != KIT_ICON_NONE) {
        row->icon = row_leading(r, opts);
    }

    /* The text column carries the row's minimum height and centres its lines: LVGL's flex
     * does not centre items against a parent's min_height. */
    col = kit_plain(r);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(col, min_h - 2 * lv_obj_get_style_pad_top(r, 0) - (kit_is_orbit() ? 0 : 1), 0);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(col, kit_is_instrument() ? 2 : kit_is_orbit() ? -1 : -5, 0);
    row->title = kit_label_raw(col, KIT_TEXT_HEADING, opts->title);
    lv_obj_set_width(row->title, lv_pct(100));
    kit_label_one_line(row->title);
    if (two) {
        row->subtitle = kit_label_raw(col, KIT_TEXT_BODY, opts->subtitle);
        lv_obj_set_width(row->subtitle, lv_pct(100));
    }

    switch (opts->trail) {
    case KIT_TRAIL_VALUE:
        row->value = kit_label_raw(r, KIT_TEXT_BODY, opts->value);
        lv_obj_set_style_text_color(row->value, kit_is_orbit() ? sk->text_2 : sk->value, 0);
        if (!opts->value_chevron) {
            break;
        }
        /* fall through */
    case KIT_TRAIL_CHEVRON:
        if (kit_is_instrument()) {
            kit_icon(r, KIT_ICON_CHEVRON_RIGHT, sk->text_3);
        } else {
            kit_icon_small(r, KIT_ICON_CHEVRON_RIGHT, kit_is_orbit() ? sk->text_3 : sk->accent_deep);
        }
        if (opts->trail == KIT_TRAIL_VALUE && kit_is_instrument()) {
            lv_obj_set_style_margin_left(lv_obj_get_child(r, -1), -4, 0);
        }
        break;
    case KIT_TRAIL_SWITCH: {
        lv_color_t tint = opts->tint;

        if (lv_color_eq(tint, lv_color_black())) {
            tint = sk->accent;
        }
        row->control = kit_switch(r, tint);
        break;
    }
    case KIT_TRAIL_CUSTOM:
        row->trail = kit_plain(r);
        lv_obj_set_size(row->trail, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row->trail, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row->trail, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(row->trail, 12, 0);
        break;
    case KIT_TRAIL_NONE:
    default:
        break;
    }
}

/* --- Pages ------------------------------------------------------------------------------ */

/* Atelier: the chapter ring around every page (hairline circle and minute track). */
static void minute_rim_spec(kit_dial_t *dial, int32_t skip_from_deg, int32_t skip_to_deg)
{
    const ui_skin_t *sk = ui_skin();

    memset(dial, 0, sizeof(*dial));
    dial->cx = KIT_CENTER;
    dial->cy = KIT_CENTER;
    dial->rings[0] = (kit_dial_ring_t) {.r = 356, .width = 1, .end_deg = 360, .color = sk->accent_deep, .opa = 140};
    dial->ticks[0] = (kit_dial_ticks_t) {
        .count = 60,
        .offset_deg = -90.0f,
        .r_in = 339,
        .r_out = 347,
        .major_every = 5,
        .r_in_major = 331,
        .r_out_major = 347,
        .width = 1,
        .width_major = 2,
        .color = sk->line_strong,
        .color_major = sk->accent_deep,
        .opa = LV_OPA_COVER,
    };
    for (int i = 0; i < 60; i++) {
        if (skip_from_deg >= 0 && i * 6 >= skip_from_deg && i * 6 <= skip_to_deg) {
            dial->ticks[0].skip |= 1ull << i;
        }
    }
}

void kit_draw_minute_rim(lv_layer_t *layer, int32_t skip_from_deg, int32_t skip_to_deg)
{
    kit_draw_minute_rim_at(layer, KIT_CENTER, KIT_CENTER, skip_from_deg, skip_to_deg);
}

void kit_draw_minute_rim_at(lv_layer_t *layer, int32_t cx, int32_t cy, int32_t skip_from_deg, int32_t skip_to_deg)
{
    kit_dial_t dial;

    minute_rim_spec(&dial, skip_from_deg, skip_to_deg);
    dial.cx = cx;
    dial.cy = cy;
    kit_dial_draw(layer, &dial);
}

void kit_minute_rim_prewarm(int32_t skip_from_deg, int32_t skip_to_deg)
{
    kit_dial_t dial;

    minute_rim_spec(&dial, skip_from_deg, skip_to_deg);
    kit_dial_prewarm(&dial);
}

void kit_prewarm(void)
{
    kit_dial_t dial;

    if (kit_is_atelier()) {
        minute_rim_spec(&dial, -1, -1);
        kit_dial_prewarm(&dial);
    }
}

static void atelier_rim_cb(lv_event_t *e)
{
    kit_draw_minute_rim(lv_event_get_layer(e), -1, -1);
}

/* The header (eyebrow, title, Atelier rule) scrolls away with the content, as on a watch;
 * the nav and action keys stay. A fade at the top makes rows vanish under the keys instead
 * of passing over them.
 *   Orbit: the band above the title, always shown (the title slides under the back cap).
 *   Instrument, Atelier: the keys sit beside the title, so the band reaches below them; it
 *   is shown once the title has scrolled into its solid part (one flag change, not per
 *   frame). */
static void scrim_band(int32_t *solid, int32_t *height, int32_t *threshold)
{
    if (kit_is_orbit()) {
        *solid = 60;
        *height = 96;
        *threshold = 0;
    } else if (kit_is_instrument()) {
        *solid = 126;
        *height = 160;
        *threshold = 12;
    } else {
        *solid = 130;
        *height = 164;
        *threshold = 14;
    }
}

static void scrim_scroll_cb(lv_event_t *e)
{
    lv_obj_t *scroll = lv_event_get_current_target(e);
    lv_obj_t *scrim = lv_event_get_user_data(e);
    int32_t solid;
    int32_t band;
    int32_t threshold;
    bool hide;

    scrim_band(&solid, &band, &threshold);
    hide = lv_obj_get_scroll_y(scroll) < threshold;
    if (hide != lv_obj_has_flag(scrim, LV_OBJ_FLAG_HIDDEN)) {
        if (hide) {
            lv_obj_add_flag(scrim, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(scrim, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static int32_t content_top(const kit_page_t *page)
{
    const ui_skin_t *sk = ui_skin();

    return sk->content_top + ((kit_is_orbit() && page->eyebrow != NULL) ? 28 : 0);
}

static void place_content(kit_page_t *page)
{
    lv_obj_set_pos(page->content, ui_skin()->content_x, content_top(page));
}

static lv_obj_t *header_key(lv_obj_t *root, kit_icon_t icon, bool action)
{
    const ui_skin_t *sk = ui_skin();
    kit_button_kind_t kind = kit_is_orbit() && !action ? KIT_BUTTON_QUIET : KIT_BUTTON_SECONDARY;
    lv_obj_t *key = kit_round_button(root, kind, icon, sk->round_button);

    if (!action) {
        lv_obj_set_pos(key, sk->nav_x, sk->page_top);
    } else if (kit_is_orbit()) {
        lv_obj_set_pos(key, 520, 92); /* on the rim to the right of the title */
    } else {
        lv_obj_set_pos(key, KIT_SCREEN - sk->nav_x - sk->round_button, sk->page_top);
    }
    return key;
}

void kit_page_create(kit_page_t *page,
                     lv_obj_t *parent,
                     const char *eyebrow,
                     const char *title,
                     kit_nav_t nav,
                     lv_event_cb_t nav_cb,
                     void *user_data)
{
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *root;
    lv_obj_t *scroll;
    lv_obj_t *content;
    int32_t half;
    int32_t safe_y;
    int32_t solid;
    int32_t band;
    int32_t threshold;

    memset(page, 0, sizeof(*page));
    root = kit_plain(parent);
    page->root = root;
    lv_obj_set_size(root, KIT_SCREEN, KIT_SCREEN);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(root, sk->bg, 0);
    lv_obj_add_flag(root, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN); /* blocks the face below */
    if (kit_is_atelier()) {
        lv_obj_add_event_cb(root, atelier_rim_cb, LV_EVENT_DRAW_POST, NULL);
    }

    /* The scrolling layer: the header and the content column. Its bottom padding lets the
     * last item scroll up to where the column still fits inside the circle (16 px clear of
     * the rim). */
    scroll = kit_plain(root);
    page->scroll = scroll;
    lv_obj_set_size(scroll, KIT_SCREEN, KIT_SCREEN);
    /* Clickable, so a drag that starts between rows or on any non-interactive part lands on
     * the scrolling layer (not on the page root) and scrolls the page. */
    lv_obj_add_flag(scroll, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_MOMENTUM |
                                LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_set_scroll_dir(scroll, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scroll, LV_SCROLLBAR_MODE_OFF);
    half = sk->content_w / 2;
    safe_y = KIT_CENTER + (int32_t)sqrtf((float)((KIT_CENTER - 16) * (KIT_CENTER - 16) - half * half));
    lv_obj_set_style_pad_bottom(scroll, KIT_SCREEN - safe_y, 0);

    content = kit_plain(scroll);
    page->content = content;
    lv_obj_set_size(content, sk->content_w, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);

    page->title = kit_label_raw(scroll, KIT_TEXT_TITLE, title);
    lv_obj_set_width(page->title, kit_is_orbit() ? 400 : KIT_SCREEN - 2 * (sk->nav_x + sk->round_button + 12));
    kit_label_one_line(page->title);
    lv_obj_set_style_text_align(page->title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(page->title, LV_ALIGN_TOP_MID, 0, sk->title_y);

    if (eyebrow != NULL) {
        kit_page_set_eyebrow(page, eyebrow);
    }
    place_content(page);

    if (kit_is_atelier()) {
        lv_obj_t *rule = engraved_rule(scroll, 240);
        lv_obj_t *loz = lv_image_create(scroll);

        lv_obj_align(rule, LV_ALIGN_TOP_MID, 0, 168);
        lv_image_set_src(loz, kit_image_src(KIT_IMAGE_LOZENGE));
        lv_obj_align(loz, LV_ALIGN_TOP_MID, 0, 168 + 1 - 11);
    }

    scrim_band(&solid, &band, &threshold);
    page->scrim = kit_plain(root);
    lv_obj_set_size(page->scrim, KIT_SCREEN, band);
    lv_obj_set_style_bg_opa(page->scrim, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(page->scrim, sk->bg, 0);
    lv_obj_set_style_bg_grad_color(page->scrim, sk->bg, 0);
    lv_obj_set_style_bg_main_opa(page->scrim, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_opa(page->scrim, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_main_stop(page->scrim, (int32_t)(255 * solid / band), 0);
    lv_obj_set_style_bg_grad_dir(page->scrim, LV_GRAD_DIR_VER, 0);
    if (threshold > 0) {
        lv_obj_add_flag(page->scrim, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_event_cb(scroll, scrim_scroll_cb, LV_EVENT_SCROLL, page->scrim);
    }

    if (nav != KIT_NAV_NONE) {
        page->nav = header_key(root, nav == KIT_NAV_BACK ? KIT_ICON_BACK : KIT_ICON_CLOSE, false);
        if (nav_cb != NULL) {
            lv_obj_add_event_cb(page->nav, nav_cb, LV_EVENT_CLICKED, user_data);
        }
    }
}

void kit_page_set_rim(kit_page_t *page, bool on)
{
    if (page == NULL || page->root == NULL || !kit_is_atelier()) {
        return;
    }
    if (on == kit_has_event_cb(page->root, atelier_rim_cb)) {
        return;
    }
    if (on) {
        lv_obj_add_event_cb(page->root, atelier_rim_cb, LV_EVENT_DRAW_POST, NULL);
    } else {
        kit_remove_event_cb(page->root, atelier_rim_cb);
    }
    lv_obj_invalidate(page->root);
}

lv_obj_t *kit_page_set_action(kit_page_t *page, kit_icon_t icon, lv_event_cb_t cb, void *user_data)
{
    if (page == NULL || page->root == NULL) {
        return NULL;
    }
    if (page->action == NULL) {
        page->action = header_key(page->root, icon, true);
        if (cb != NULL) {
            lv_obj_add_event_cb(page->action, cb, LV_EVENT_CLICKED, user_data);
        }
    } else {
        lv_obj_t *img = lv_obj_get_child(page->action, 0);

        kit_icon_set(img, icon);
    }
    return page->action;
}

void kit_page_set_title(kit_page_t *page, const char *title)
{
    if (page != NULL) {
        kit_label_set_text_if_changed(page->title, title);
    }
}

void kit_page_set_eyebrow(kit_page_t *page, const char *eyebrow)
{
    const ui_skin_t *sk = ui_skin();

    if (page == NULL || page->scroll == NULL) {
        return;
    }
    if (page->eyebrow == NULL) {
        if (eyebrow == NULL) {
            return;
        }
        page->eyebrow = kit_label_raw(page->scroll, KIT_TEXT_LABEL, eyebrow);
        lv_obj_set_width(page->eyebrow, kit_is_orbit() ? 440 : 400);
        kit_label_one_line(page->eyebrow);
        lv_obj_set_style_text_align(page->eyebrow, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(page->eyebrow, LV_ALIGN_TOP_MID, 0, sk->eyebrow_y);
        if (page->content != NULL) {
            place_content(page);
        }
        return;
    }
    kit_label_set_text_if_changed(page->eyebrow, eyebrow != NULL ? eyebrow : "");
}

/* --- Dialog ----------------------------------------------------------------------------- */

lv_obj_t *kit_dialog(lv_obj_t *parent, int32_t width)
{
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *dim = kit_plain(parent);
    lv_obj_t *card;

    lv_obj_set_size(dim, KIT_SCREEN, KIT_SCREEN);
    lv_obj_set_pos(dim, 0, 0);
    lv_obj_add_flag(dim, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(dim, 184, 0);
    lv_obj_set_style_bg_color(dim, kit_is_atelier() ? kit_rgb(0x080810) : lv_color_black(), 0);

    card = kit_plain(dim);
    lv_obj_set_width(card, width > 0 ? width : sk->content_w);
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_center(card);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(card, 32, 0);
    lv_obj_set_style_pad_row(card, sk->gap_s, 0);
    /* The screen's one soft shadow. */
    lv_obj_set_style_shadow_color(card, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(card, 140, 0);
    if (kit_is_instrument()) {
        lv_obj_set_style_bg_color(card, sk->surface, 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, sk->line_strong, 0);
        lv_obj_set_style_radius(card, sk->radius_m, 0);
        lv_obj_set_style_pad_hor(card, 40, 0);
        lv_obj_set_style_shadow_width(card, 48, 0);
        lv_obj_set_style_shadow_offset_y(card, 24, 0);
    } else if (kit_is_orbit()) {
        lv_obj_set_style_bg_color(card, sk->surface, 0);
        lv_obj_set_style_radius(card, sk->radius_m, 0);
        lv_obj_set_style_shadow_width(card, 56, 0);
        lv_obj_set_style_shadow_offset_y(card, 14, 0);
        lv_obj_set_style_shadow_opa(card, 153, 0);
    } else {
        lv_obj_set_style_bg_color(card, sk->surface, 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, sk->accent_deep, 0);
        lv_obj_set_style_radius(card, sk->radius_m, 0);
        lv_obj_set_style_shadow_width(card, 28, 0);
        lv_obj_set_style_shadow_offset_y(card, 10, 0);
    }
    return card;
}
