/*
 * Kit controls: switch, slider, segmented control, day keys, roller, keyboard and field.
 */
#include <string.h>

#include "assets/skin_fonts/skin_fonts.h"
#include "assets/ui_assets/kit_assets.h"
#include "src/draw/lv_draw_private.h"
#include "ui/kit/ui_kit_internal.h"
#include "ui/ui_controls.h"
#include "ui/ui_frame_free.h"

/* --- Switch ----------------------------------------------------------------------------- */

lv_obj_t *kit_switch(lv_obj_t *parent, lv_color_t tint)
{
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *sw = lv_switch_create(parent);
    int32_t w = sk->switch_w;
    int32_t h = sk->switch_h;

    lv_obj_remove_style_all(sw);
    lv_obj_set_size(sw, w, h);
    lv_obj_set_style_anim_duration(sw, 140, 0);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_KNOB);
    kit_ensure_tap(sw, w, h);
    ui_attach_click_feedback(sw, LV_EVENT_VALUE_CHANGED);

    if (kit_is_instrument()) {
        /* Slide switch: recessed window, keycap thumb with grip ridges, and a lit bar (on) or
         * an empty ring (off) in the free half of the window. The indicator part is that mark:
         * the main padding places it. */
        lv_obj_set_style_radius(sw, sk->radius_s, 0);
        lv_obj_set_style_bg_color(sw, sk->well, 0);
        lv_obj_set_style_border_width(sw, 1, 0);
        lv_obj_set_style_border_color(sw, sk->line, 0);
        lv_obj_set_style_pad_left(sw, w - 2 - 18 - 10, 0);
        lv_obj_set_style_pad_right(sw, 18, 0);
        lv_obj_set_style_pad_ver(sw, (h - 2 - 10) / 2, 0);
        lv_obj_set_style_pad_left(sw, 16, LV_STATE_CHECKED);
        lv_obj_set_style_pad_right(sw, w - 2 - 16 - 16, LV_STATE_CHECKED);
        lv_obj_set_style_pad_ver(sw, (h - 2 - 4) / 2, LV_STATE_CHECKED);

        lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
        lv_obj_set_style_border_width(sw, 2, LV_PART_INDICATOR);
        lv_obj_set_style_border_color(sw, sk->line_strong, LV_PART_INDICATOR);
        lv_obj_set_style_radius(sw, 2, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_border_width(sw, 0, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(sw, tint, LV_PART_INDICATOR | LV_STATE_CHECKED);

        lv_obj_set_style_pad_all(sw, -4, LV_PART_KNOB);
        lv_obj_set_style_radius(sw, 7, LV_PART_KNOB);
        lv_obj_set_style_bg_color(sw, kit_rgb(0x585450), LV_PART_KNOB);
        lv_obj_set_style_bg_color(sw, sk->text, LV_PART_KNOB | LV_STATE_CHECKED);
        lv_obj_set_style_border_side(sw, LV_BORDER_SIDE_TOP, LV_PART_KNOB);
        lv_obj_set_style_border_width(sw, 1, LV_PART_KNOB);
        lv_obj_set_style_border_color(sw, kit_rgb(0x686460), LV_PART_KNOB);
        lv_obj_set_style_border_color(sw, kit_rgb(0xF8F4E8), LV_PART_KNOB | LV_STATE_CHECKED);
        lv_obj_set_style_bg_image_src(sw, &ui_kit_instrument_grip, LV_PART_KNOB);
    } else if (kit_is_orbit()) {
        /* iOS-style capsule: area tint when on; Other's graphite turns silver. */
        bool silver = lv_color_eq(tint, sk->tint_other);

        lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(sw, sk->well, 0);
        lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(sw, silver ? kit_rgb(0xE0E4E8) : tint, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_pad_all(sw, -3, LV_PART_KNOB);
        lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_KNOB);
        lv_obj_set_style_bg_color(sw, kit_rgb(0xF8FCF8), LV_PART_KNOB);
        if (silver) {
            lv_obj_set_style_bg_color(sw, sk->surface, LV_PART_KNOB | LV_STATE_CHECKED);
        }
    } else {
        /* Jewelled switch: a steel bead slides to the right and becomes a gilt bead with a
         * ruby, the track takes a gilt rim. */
        lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(sw, sk->raised, 0);
        lv_obj_set_style_border_width(sw, 1, 0);
        lv_obj_set_style_border_color(sw, sk->line_strong, 0);
        lv_obj_set_style_bg_color(sw, sk->accent_soft, LV_STATE_CHECKED);
        lv_obj_set_style_border_color(sw, tint, LV_STATE_CHECKED);
        lv_obj_set_style_pad_all(sw, -5, LV_PART_KNOB);
        lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_KNOB);
        lv_obj_set_style_bg_opa(sw, LV_OPA_TRANSP, LV_PART_KNOB);
        lv_obj_set_style_bg_image_src(sw, kit_image_src(KIT_IMAGE_JEWEL_30_OFF), LV_PART_KNOB);
        lv_obj_set_style_bg_image_src(sw, kit_image_src(KIT_IMAGE_JEWEL_30), LV_PART_KNOB | LV_STATE_CHECKED);
    }
    return sw;
}

void kit_switch_set(lv_obj_t *sw, bool on)
{
    if (sw == NULL || lv_obj_has_state(sw, LV_STATE_CHECKED) == on) {
        return;
    }
    if (on) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(sw, LV_STATE_CHECKED);
    }
}

/* --- Slider ----------------------------------------------------------------------------- */

static void fill(lv_layer_t *layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2, lv_color_t c, int32_t radius,
                 lv_opa_t opa)
{
    lv_draw_rect_dsc_t d;
    lv_area_t a = {x1, y1, x2, y2};

    lv_draw_rect_dsc_init(&d);
    d.bg_color = c;
    d.bg_opa = opa;
    d.radius = radius;
    lv_draw_rect(layer, &d, &a);
}

/* Instrument ruler: the 51 ticks and the base line as two cached images (ticks lit and
 * unlit), drawn clipped either side of the knob. Drawing the ticks as 51 rects every frame
 * slowed the device's draw dispatcher for the whole frame. */
#define RULER_H 41
#define RULER_CACHE 4

typedef struct {
    int32_t w;
    bool lit;
    lv_draw_buf_t *buf;
} ruler_entry_t;

static ruler_entry_t s_rulers[RULER_CACHE];

static void ruler_px(lv_draw_buf_t *buf, int32_t x0, int32_t y0, int32_t x1, int32_t y1, lv_color_t c)
{
    for (int32_t y = LV_MAX(0, y0); y <= LV_MIN(RULER_H - 1, y1); y++) {
        lv_color32_t *row = (lv_color32_t *)(buf->data + (size_t)y * buf->header.stride);

        for (int32_t x = LV_MAX(0, x0); x <= LV_MIN((int32_t)buf->header.w - 1, x1); x++) {
            row[x] = (lv_color32_t) {.blue = c.blue, .green = c.green, .red = c.red, .alpha = 0xFF};
        }
    }
}

static const lv_draw_buf_t *ruler_image(int32_t w, bool lit)
{
    const ui_skin_t *sk = ui_skin();
    ruler_entry_t *slot = NULL;
    lv_color_t c = lit ? sk->text_2 : sk->line_strong;

    for (int i = 0; i < RULER_CACHE; i++) {
        if (s_rulers[i].buf != NULL && s_rulers[i].w == w && s_rulers[i].lit == lit) {
            return s_rulers[i].buf;
        }
        if (slot == NULL && s_rulers[i].buf == NULL) {
            slot = &s_rulers[i];
        }
    }
    if (slot == NULL) {
        /* Made while a frame is drawn: the one dropped may be in its draw tasks. */
        slot = &s_rulers[0];
        ui_frame_free(slot->buf);
    }
    slot->w = w;
    slot->lit = lit;
    slot->buf = lv_draw_buf_create((uint32_t)w, RULER_H, LV_COLOR_FORMAT_ARGB8888, 0);
    if (slot->buf == NULL) {
        return NULL;
    }
    lv_draw_buf_clear(slot->buf, NULL);
    ruler_px(slot->buf, 0, 40, w - 1, 40, sk->line);
    for (int i = 0; i <= 50; i++) {
        int32_t x = i * (w - 1) / 50;

        if (i % 5 == 0) {
            ruler_px(slot->buf, x - (i == 50 ? 1 : 0), 22, x + (i == 50 ? 0 : 1), 40, c);
        } else {
            ruler_px(slot->buf, x, 30, x, 40, c);
        }
    }
    return slot->buf;
}

void kit_slider_cache_flush(void)
{
    for (int i = 0; i < RULER_CACHE; i++) {
        if (s_rulers[i].buf != NULL) {
            lv_draw_buf_destroy(s_rulers[i].buf);
            s_rulers[i].buf = NULL;
        }
    }
}

static void draw_ruler(lv_layer_t *layer, const lv_area_t *a, int32_t kx, lv_opa_t opa)
{
    lv_area_t clip = layer->_clip_area;
    lv_area_t area = {a->x1, a->y1, a->x2, a->y1 + RULER_H - 1};
    lv_draw_image_dsc_t img;

    lv_draw_image_dsc_init(&img);
    img.opa = opa;
    for (int lit = 1; lit >= 0; lit--) {
        lv_area_t part = clip;

        img.src = ruler_image(lv_area_get_width(a), lit != 0);
        if (img.src == NULL) {
            continue;
        }
        if (lit) {
            part.x2 = LV_MIN(clip.x2, kx);
        } else {
            part.x1 = LV_MAX(clip.x1, kx + 1);
        }
        if (part.x1 <= part.x2) {
            layer->_clip_area = part;
            lv_draw_image(layer, &img, &area);
        }
    }
    layer->_clip_area = clip;
}

static void slider_draw_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_current_target(e);
    const ui_skin_t *sk = ui_skin();
    lv_layer_t *layer;
    lv_area_t a;
    int32_t w;
    int32_t min;
    int32_t max;
    int32_t kx;
    lv_color_t tint;
    lv_opa_t opa;

    if (lv_event_get_code(e) == LV_EVENT_REFR_EXT_DRAW_SIZE) {
        lv_event_set_ext_draw_size(e, kit_is_orbit() ? 18 : 16);
        return;
    }
    layer = lv_event_get_layer(e);
    lv_obj_get_coords(obj, &a);
    w = lv_area_get_width(&a);
    min = lv_slider_get_min_value(obj);
    max = lv_slider_get_max_value(obj);
    kx = a.x1 + (max > min ? (lv_slider_get_value(obj) - min) * (w - 1) / (max - min) : 0);
    tint = lv_obj_get_style_bg_color(obj, LV_PART_INDICATOR);
    opa = lv_obj_get_style_opa_recursive(obj, LV_PART_MAIN);

    if (kit_is_instrument()) {
        /* Ruler: a tick every 2 %, majors every 10 %, a fader cap with the signal index. */
        draw_ruler(layer, &a, kx, opa);
        fill(layer, kx - 10, a.y1 + 4, kx + 10, a.y1 + 45, sk->text, 4, opa);
        fill(layer, kx - 1, a.y1 + 10, kx, a.y1 + 39, tint, 0, opa);
    } else if (kit_is_orbit()) {
        /* Capsule: 12 px track, white fill, 32 px knob. */
        int32_t cy = a.y1 + lv_area_get_height(&a) / 2;

        fill(layer, a.x1, cy - 6, a.x2, cy + 5, sk->well, 6, opa);
        fill(layer, a.x1, cy - 6, LV_MAX(kx, a.x1 + 11), cy + 5, tint, 6, opa);
        fill(layer, kx - 16, cy - 16, kx + 15, cy + 15, kit_rgb(0xF8FCF8), LV_RADIUS_CIRCLE, opa);
    } else {
        /* Hairline with gilt fill, engraved ticks below, the jewel as knob. */
        int32_t ty = a.y1 + 19;
        lv_draw_image_dsc_t img;
        lv_area_t ia;

        fill(layer, a.x1, ty, a.x2, ty + 1, sk->line_strong, 0, opa);
        for (int i = 0; i <= 10; i++) {
            int32_t x = a.x1 + i * (w - 1) / 10;
            bool major = i % 5 == 0;

            fill(layer, x, ty + 8, x, ty + 8 + (major ? 8 : 5), major ? sk->accent_deep : sk->line_strong, 0, opa);
        }
        fill(layer, a.x1, ty, kx, ty + 1, tint, 0, opa);
        lv_draw_image_dsc_init(&img);
        img.src = kit_image_src(KIT_IMAGE_JEWEL_28);
        img.opa = opa;
        ia.x1 = kx - 14;
        ia.y1 = ty + 1 - 14;
        ia.x2 = ia.x1 + 27;
        ia.y2 = ia.y1 + 27;
        lv_draw_image(layer, &img, &ia);
    }
}

lv_obj_t *kit_slider(lv_obj_t *parent, int32_t min, int32_t max, lv_color_t tint)
{
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *slider = lv_slider_create(parent);
    int32_t h = kit_is_instrument() ? 48 : kit_is_orbit() ? 32 : 40;

    lv_obj_remove_style_all(slider);
    lv_slider_set_range(slider, min, max);
    lv_obj_set_size(slider, sk->content_w, h);
    /* The tint is kept on the (undrawn) indicator part for the draw callback. */
    lv_obj_set_style_bg_color(slider, kit_is_orbit() && lv_color_eq(tint, sk->accent) ? kit_rgb(0xF8FCF8) : tint,
                              LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_ext_click_area(slider, (sk->tap_min - h + 1) / 2);
    lv_obj_add_event_cb(slider, slider_draw_cb, LV_EVENT_DRAW_MAIN_END, NULL);
    lv_obj_add_event_cb(slider, slider_draw_cb, LV_EVENT_REFR_EXT_DRAW_SIZE, NULL);
    lv_obj_refresh_ext_draw_size(slider);
    return slider;
}

void kit_slider_set(lv_obj_t *slider, int32_t value)
{
    if (slider != NULL && lv_slider_get_value(slider) != value) {
        lv_slider_set_value(slider, value, LV_ANIM_OFF);
    }
}

/* --- Segmented control ------------------------------------------------------------------ */

#define KIT_SEG_MAX 6

typedef struct {
    lv_obj_t *seg[KIT_SEG_MAX];
    int count;
    int selected;
} seg_data_t;

static void seg_free_cb(lv_event_t *e)
{
    lv_free(lv_event_get_user_data(e));
}

static void seg_style_item(lv_obj_t *item, bool on)
{
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *label = lv_obj_get_child(item, 0);

    if (on) {
        lv_obj_add_state(item, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(item, LV_STATE_CHECKED);
    }
    if (kit_is_instrument()) {
        lv_obj_set_style_text_font(label, on ? &it_medium_22 : sk->font_body, 0);
        lv_obj_set_style_text_color(label, on ? sk->text : sk->text_3, 0);
    } else if (kit_is_orbit()) {
        lv_obj_set_style_text_color(label, on ? kit_on_color(lv_obj_get_style_bg_color(item, LV_STATE_CHECKED)) : sk->text,
                                    0);
    } else {
        lv_obj_set_style_text_color(label, on ? sk->text : sk->text_2, 0);
    }
}

/* Orbit and Atelier size segments to their labels and share the rest equally (CSS flex:auto). */
static void seg_layout(lv_obj_t *seg, seg_data_t *d)
{
    int32_t inner = lv_obj_get_content_width(seg);
    int32_t gap = lv_obj_get_style_pad_column(seg, 0);
    int32_t widths[KIT_SEG_MAX];
    int32_t sum = 0;
    int32_t extra;

    if (inner <= 0 || d->count == 0) {
        return;
    }
    for (int i = 0; i < d->count; i++) {
        lv_obj_t *label = lv_obj_get_child(d->seg[i], 0);
        lv_point_t size;

        lv_text_get_size(&size, lv_label_get_text(label), lv_obj_get_style_text_font(label, 0), 0, 0, LV_COORD_MAX,
                         LV_TEXT_FLAG_NONE);
        widths[i] = size.x + 2 * lv_obj_get_style_pad_left(d->seg[i], 0);
        sum += widths[i];
    }
    extra = (inner - sum - gap * (d->count - 1)) / d->count;
    for (int i = 0; i < d->count; i++) {
        lv_obj_set_width(d->seg[i], LV_MAX(widths[i] + extra, 40));
    }
}

static void seg_size_cb(lv_event_t *e)
{
    lv_obj_t *seg = lv_event_get_current_target(e);

    seg_layout(seg, lv_event_get_user_data(e));
}

static void seg_click_cb(lv_event_t *e)
{
    lv_obj_t *item = lv_event_get_current_target(e);
    lv_obj_t *seg = lv_obj_get_parent(item);
    seg_data_t *d = lv_event_get_user_data(e);
    int index = (int)lv_obj_get_index(item);

    if (index == d->selected) {
        return;
    }
    kit_segmented_set(seg, index);
    lv_obj_send_event(seg, LV_EVENT_VALUE_CHANGED, NULL);
}

/* Instrument: hairline separators between unselected segments. */
static void seg_sep_cb(lv_event_t *e)
{
    lv_obj_t *seg = lv_event_get_current_target(e);
    seg_data_t *d = lv_event_get_user_data(e);
    lv_layer_t *layer = lv_event_get_layer(e);

    for (int i = 1; i < d->count; i++) {
        lv_area_t a;
        lv_draw_rect_dsc_t r;

        if (i == d->selected || i - 1 == d->selected) {
            continue;
        }
        lv_obj_get_coords(d->seg[i], &a);
        a.x2 = a.x1;
        a.y1 += 18;
        a.y2 -= 18;
        lv_draw_rect_dsc_init(&r);
        r.bg_color = ui_skin()->line;
        lv_draw_rect(layer, &r, &a);
    }
    (void)seg;
}

/* Instrument: the signal index on the raised segment. */
static void seg_index_cb(lv_event_t *e)
{
    lv_obj_t *item = lv_event_get_current_target(e);
    lv_area_t a;
    lv_draw_rect_dsc_t r;
    int32_t cx;

    if (!lv_obj_has_state(item, LV_STATE_CHECKED)) {
        return;
    }
    lv_obj_get_coords(item, &a);
    cx = a.x1 + lv_area_get_width(&a) / 2;
    lv_draw_rect_dsc_init(&r);
    r.bg_color = ui_skin()->accent;
    r.radius = 2;
    a.x1 = cx - 9;
    a.x2 = cx + 8;
    a.y1 += 8;
    a.y2 = a.y1 + 2;
    lv_draw_rect(lv_event_get_layer(e), &r, &a);
}

lv_obj_t *kit_segmented(lv_obj_t *parent, const char *const *labels, int count, int selected)
{
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *seg = kit_plain(parent);
    seg_data_t *d = lv_malloc_zeroed(sizeof(seg_data_t));

    if (count > KIT_SEG_MAX) {
        count = KIT_SEG_MAX;
    }
    d->count = count;
    d->selected = -1;
    lv_obj_add_event_cb(seg, seg_free_cb, LV_EVENT_DELETE, d);
    lv_obj_set_size(seg, lv_pct(100), 72);
    lv_obj_set_flex_flow(seg, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(seg, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(seg, LV_OPA_COVER, 0);
    if (kit_is_instrument()) {
        lv_obj_set_style_bg_color(seg, sk->well, 0);
        lv_obj_set_style_border_width(seg, 1, 0);
        lv_obj_set_style_border_color(seg, sk->line, 0);
        lv_obj_set_style_radius(seg, sk->radius_s, 0);
        lv_obj_set_style_pad_all(seg, 4, 0);
        lv_obj_add_event_cb(seg, seg_sep_cb, LV_EVENT_DRAW_MAIN_END, d);
    } else if (kit_is_orbit()) {
        lv_obj_set_style_bg_color(seg, sk->surface, 0);
        lv_obj_set_style_radius(seg, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_pad_all(seg, 6, 0);
        lv_obj_set_style_pad_column(seg, 4, 0);
    } else {
        lv_obj_set_style_bg_color(seg, sk->surface, 0);
        lv_obj_set_style_border_width(seg, 1, 0);
        lv_obj_set_style_border_color(seg, sk->line_strong, 0);
        lv_obj_set_style_radius(seg, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_pad_all(seg, 5, 0);
        lv_obj_set_style_pad_column(seg, 2, 0);
    }

    for (int i = 0; i < count; i++) {
        lv_obj_t *item = kit_plain(seg);
        lv_obj_t *label;

        d->seg[i] = item;
        lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_height(item, lv_pct(100));
        lv_obj_set_style_radius(item, kit_is_instrument() ? 7 : LV_RADIUS_CIRCLE, 0);
        lv_obj_add_event_cb(item, seg_click_cb, LV_EVENT_CLICKED, d);
        ui_attach_click_feedback(item, LV_EVENT_CLICKED);
        if (kit_is_instrument()) {
            lv_obj_set_flex_grow(item, 1);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, LV_STATE_CHECKED);
            lv_obj_set_style_bg_color(item, sk->raised, LV_STATE_CHECKED);
            kit_add_key_deco(item, &kit_deco_segment);
            lv_obj_add_event_cb(item, seg_index_cb, LV_EVENT_DRAW_MAIN_END, NULL);
        } else if (kit_is_orbit()) {
            lv_obj_set_style_pad_hor(item, 12, 0);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, LV_STATE_CHECKED);
            lv_obj_set_style_bg_color(item, sk->accent, LV_STATE_CHECKED);
        } else {
            lv_obj_set_style_pad_hor(item, 14, 0);
            lv_obj_set_style_border_width(item, 1, LV_STATE_CHECKED);
            lv_obj_set_style_border_color(item, sk->accent, LV_STATE_CHECKED);
            lv_obj_set_style_bg_opa(item, LV_OPA_COVER, LV_STATE_CHECKED);
            lv_obj_set_style_bg_color(item, sk->raised, LV_STATE_CHECKED);
        }
        label = kit_label_raw(item, kit_is_orbit() ? KIT_TEXT_VALUE : KIT_TEXT_BODY, labels[i]);
        lv_obj_center(label);
        seg_style_item(item, false);
    }
    if (!kit_is_instrument()) {
        lv_obj_add_event_cb(seg, seg_size_cb, LV_EVENT_SIZE_CHANGED, d);
    }
    kit_segmented_set(seg, selected);
    return seg;
}

void kit_segmented_set(lv_obj_t *seg, int selected)
{
    seg_data_t *d;

    if (seg == NULL || (d = kit_event_data(seg, seg_free_cb)) == NULL) {
        return;
    }
    if (selected == d->selected) {
        return;
    }
    if (d->selected >= 0 && d->selected < d->count) {
        seg_style_item(d->seg[d->selected], false);
    }
    d->selected = selected;
    if (selected >= 0 && selected < d->count) {
        seg_style_item(d->seg[selected], true);
    }
    lv_obj_invalidate(seg);
}

int kit_segmented_get(lv_obj_t *seg)
{
    seg_data_t *d;

    if (seg == NULL || (d = kit_event_data(seg, seg_free_cb)) == NULL) {
        return -1;
    }
    return d->selected;
}

void kit_segmented_set_label(lv_obj_t *seg, int index, const char *label)
{
    seg_data_t *d;

    if (seg == NULL || (d = kit_event_data(seg, seg_free_cb)) == NULL || index < 0 || index >= d->count) {
        return;
    }
    kit_label_set_text_if_changed(lv_obj_get_child(d->seg[index], 0), label);
    if (!kit_is_instrument()) {
        seg_layout(seg, d);
    }
}

/* --- Day toggle ------------------------------------------------------------------------- */

static void day_state_cb(lv_event_t *e)
{
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *key = lv_event_get_current_target(e);
    lv_obj_t *label = lv_obj_get_child(key, 0);
    bool on = lv_obj_has_state(key, LV_STATE_CHECKED);
    lv_color_t c;

    if (kit_is_instrument()) {
        c = on ? sk->text : sk->text_3;
    } else if (kit_is_orbit()) {
        c = on ? kit_on_color(lv_obj_get_style_bg_color(key, LV_STATE_CHECKED)) : sk->text;
    } else {
        c = on ? sk->text : sk->text_3;
    }
    if (!lv_color_eq(lv_obj_get_style_text_color(label, 0), c)) {
        lv_obj_set_style_text_color(label, c, 0);
    }
}

/* Instrument: the signal LED under the letter of a day that is on. */
static void day_led_cb(lv_event_t *e)
{
    lv_obj_t *key = lv_event_get_current_target(e);
    lv_area_t a;
    lv_draw_rect_dsc_t r;
    int32_t cx;

    if (!lv_obj_has_state(key, LV_STATE_CHECKED)) {
        return;
    }
    lv_obj_get_coords(key, &a);
    cx = a.x1 + lv_area_get_width(&a) / 2;
    lv_draw_rect_dsc_init(&r);
    r.bg_color = ui_skin()->accent;
    r.radius = LV_RADIUS_CIRCLE;
    a.x1 = cx - 3;
    a.x2 = cx + 2;
    a.y1 = a.y2 - 17;
    a.y2 = a.y1 + 5;
    lv_draw_rect(lv_event_get_layer(e), &r, &a);
}

lv_obj_t *kit_day_toggle(lv_obj_t *parent, const char *letter, bool on)
{
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *key = kit_plain(parent);
    lv_obj_t *label;
    int32_t d = kit_is_instrument() ? 64 : kit_is_orbit() ? 66 : 56;

    lv_obj_add_flag(key, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_size(key, d, d);
    lv_obj_set_style_radius(key, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(key, LV_OPA_COVER, 0);
    kit_ensure_tap(key, d, d);
    kit_add_pressed_dim(key);
    ui_attach_click_feedback(key, LV_EVENT_CLICKED);
    if (kit_is_instrument()) {
        lv_obj_set_style_bg_color(key, sk->well, 0);
        lv_obj_set_style_border_width(key, 1, 0);
        lv_obj_set_style_border_color(key, sk->line, 0);
        lv_obj_set_style_bg_color(key, sk->raised, LV_STATE_CHECKED);
        kit_add_key_deco(key, &kit_deco_key_checked);
        lv_obj_add_event_cb(key, day_led_cb, LV_EVENT_DRAW_MAIN_END, NULL);
        label = kit_label_raw(key, KIT_TEXT_BUTTON, letter);
        lv_obj_align(label, LV_ALIGN_CENTER, 0, -3);
    } else if (kit_is_orbit()) {
        lv_obj_set_style_bg_color(key, sk->surface, 0);
        lv_obj_set_style_bg_color(key, sk->accent, LV_STATE_CHECKED);
        label = kit_label_raw(key, KIT_TEXT_HEADING, letter);
        lv_obj_center(label);
    } else {
        lv_obj_set_style_bg_opa(key, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(key, 1, 0);
        lv_obj_set_style_border_color(key, sk->line, 0);
        lv_obj_set_style_bg_opa(key, LV_OPA_COVER, LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(key, sk->raised, LV_STATE_CHECKED);
        lv_obj_set_style_border_color(key, sk->accent, LV_STATE_CHECKED);
        label = kit_label_raw(key, KIT_TEXT_BODY, letter);
        lv_obj_center(label);
    }
    lv_obj_add_event_cb(key, day_state_cb, LV_EVENT_STYLE_CHANGED, NULL);
    if (on) {
        lv_obj_add_state(key, LV_STATE_CHECKED);
    }
    lv_obj_send_event(key, LV_EVENT_STYLE_CHANGED, NULL);
    return key;
}

/* --- Roller ----------------------------------------------------------------------------- */

/* Instrument: two signal marks either side of the selection window. The user data holds
 * which sides are drawn (bit 0 left, bit 1 right). */
#define MARK_LEFT 1U
#define MARK_RIGHT 2U

static void roller_marks_cb(lv_event_t *e)
{
    lv_obj_t *roller = lv_event_get_current_target(e);
    uintptr_t sides = (uintptr_t)lv_event_get_user_data(e);
    lv_layer_t *layer;
    lv_area_t a;
    lv_draw_rect_dsc_t r;
    int32_t cy;

    if (lv_event_get_code(e) == LV_EVENT_REFR_EXT_DRAW_SIZE) {
        lv_event_set_ext_draw_size(e, 12);
        return;
    }
    layer = lv_event_get_layer(e);
    lv_obj_get_coords(roller, &a);
    cy = a.y1 + lv_area_get_height(&a) / 2;
    lv_draw_rect_dsc_init(&r);
    r.bg_color = ui_skin()->accent;
    r.radius = 2;
    lv_area_t left = {a.x1 - 10, cy - 2, a.x1 - 5, cy + 1};
    lv_area_t right = {a.x2 + 5, cy - 2, a.x2 + 10, cy + 1};
    if (sides & MARK_LEFT) {
        lv_draw_rect(layer, &r, &left);
    }
    if (sides & MARK_RIGHT) {
        lv_draw_rect(layer, &r, &right);
    }
}

void kit_roller_set_marks(lv_obj_t *roller, bool left, bool right)
{
    uintptr_t sides = (left ? MARK_LEFT : 0U) | (right ? MARK_RIGHT : 0U);

    if (roller == NULL || !kit_is_instrument()) {
        return;
    }
    kit_remove_event_cb(roller, roller_marks_cb);
    if (sides != 0U) {
        lv_obj_add_event_cb(roller, roller_marks_cb, LV_EVENT_DRAW_MAIN_END, (void *)sides);
        lv_obj_add_event_cb(roller, roller_marks_cb, LV_EVENT_REFR_EXT_DRAW_SIZE, (void *)sides);
    }
    lv_obj_refresh_ext_draw_size(roller);
    lv_obj_invalidate(roller);
}

lv_obj_t *kit_roller(lv_obj_t *parent, const char *options, uint32_t visible_rows, const lv_font_t *selected_font)
{
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *roller = lv_roller_create(parent);
    const lv_font_t *main_font;
    lv_color_t main_color;
    int32_t extra;

    if (selected_font == NULL) {
        selected_font = sk->font_time_l;
    }
    if (kit_is_instrument()) {
        main_font = &it_light_34;
        main_color = sk->text_3;
        extra = 14;
    } else if (kit_is_orbit()) {
        main_font = sk->font_title;
        main_color = kit_rgb(0x484848);
        extra = 30;
    } else {
        main_font = sk->font_title;
        main_color = sk->text_3;
        extra = 23;
    }
    lv_obj_remove_style_all(roller);
    lv_obj_remove_flag(roller, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_set_style_text_font(roller, main_font, 0);
    lv_obj_set_style_text_color(roller, main_color, 0);
    lv_obj_set_style_text_align(roller, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(roller,
                                     (lv_font_get_line_height(selected_font) - lv_font_get_line_height(main_font)) / 2 +
                                         extra,
                                     0);
    lv_obj_set_style_anim_duration(roller, 200, 0);
    lv_obj_set_style_text_font(roller, selected_font, LV_PART_SELECTED);
    lv_obj_set_style_text_color(roller, sk->text, LV_PART_SELECTED);
    if (kit_is_instrument()) {
        lv_obj_set_style_bg_opa(roller, LV_OPA_COVER, LV_PART_SELECTED);
        lv_obj_set_style_bg_color(roller, sk->well, LV_PART_SELECTED);
        lv_obj_set_style_border_width(roller, 1, LV_PART_SELECTED);
        lv_obj_set_style_border_color(roller, sk->line, LV_PART_SELECTED);
        lv_obj_set_style_radius(roller, sk->radius_s, LV_PART_SELECTED);
        kit_roller_set_marks(roller, true, true);
    } else if (kit_is_atelier()) {
        lv_obj_set_style_border_side(roller, LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM, LV_PART_SELECTED);
        lv_obj_set_style_border_width(roller, 1, LV_PART_SELECTED);
        lv_obj_set_style_border_color(roller, sk->line_strong, LV_PART_SELECTED);
    }
    lv_roller_set_options(roller, options != NULL ? options : "", LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(roller, visible_rows > 0 ? visible_rows : 3);
    ui_attach_click_feedback(roller, LV_EVENT_VALUE_CHANGED);
    return roller;
}

/* --- Keyboard --------------------------------------------------------------------------- */

/* Rows narrow towards the bottom of the circle: 10, 9, 7 + 2, then 4 keys centred with
 * hidden spacers. Special keys are text the kit fonts contain (arrows, not FontAwesome). */
#define KB_BKSP "\xE2\x86\x90"  /* ← */
#define KB_SHIFT "\xE2\x86\x91" /* ↑ */
#define KB_HIDDEN (LV_BUTTONMATRIX_CTRL_HIDDEN | LV_BUTTONMATRIX_CTRL_NO_REPEAT)
#define KB_FN (LV_BUTTONMATRIX_CTRL_CHECKED | LV_BUTTONMATRIX_CTRL_NO_REPEAT)

static const char *const kb_lower[] = {
    "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
    " ", "a", "s", "d", "f", "g", "h", "j", "k", "l", " ", "\n",
    " ", KB_SHIFT, "z", "x", "c", "v", "b", "n", "m", KB_BKSP, " ", "\n",
    " ", "?123", ",", " ", ".", " ", ""};
static const char *const kb_upper[] = {
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
    " ", "A", "S", "D", "F", "G", "H", "J", "K", "L", " ", "\n",
    " ", KB_SHIFT, "Z", "X", "C", "V", "B", "N", "M", KB_BKSP, " ", "\n",
    " ", "?123", ",", " ", ".", " ", ""};
static const char *const kb_special[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
    " ", "-", "/", ":", ";", "(", ")", "&", "@", "\"", " ", "\n",
    " ", "#", "+", "=", "%", "?", "!", "*", "'", KB_BKSP, " ", "\n",
    " ", "abc", "_", " ", "$", " ", ""};
static const lv_buttonmatrix_ctrl_t kb_ctrl[] = {
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    KB_HIDDEN | 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, KB_HIDDEN | 2,
    KB_HIDDEN | 1, KB_FN | 5, 4, 4, 4, 4, 4, 4, 4, KB_FN | 5, KB_HIDDEN | 1,
    KB_HIDDEN | 5, KB_FN | 6, 4, 15, 4, KB_HIDDEN | 5};
static const lv_buttonmatrix_ctrl_t kb_ctrl_special[] = {
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    KB_HIDDEN | 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, KB_HIDDEN | 2,
    KB_HIDDEN | 1, 4, 4, 4, 4, 4, 4, 4, 4, KB_FN | 5, KB_HIDDEN | 1,
    KB_HIDDEN | 5, KB_FN | 6, 4, 15, 4, KB_HIDDEN | 5};
static const char *const kb_number[] = {
    "1", "2", "3", "\n",
    "4", "5", "6", "\n",
    "7", "8", "9", "\n",
    KB_BKSP, "0", "OK", ""};
static const lv_buttonmatrix_ctrl_t kb_ctrl_number[] = {
    1, 1, 1,
    1, 1, 1,
    1, 1, 1,
    KB_FN | 1, 1, KB_FN | 1};

static void keyboard_event_cb(lv_event_t *e)
{
    lv_obj_t *kb = lv_event_get_current_target(e);
    lv_obj_t *ta = lv_keyboard_get_textarea(kb);
    uint32_t id = lv_buttonmatrix_get_selected_button(kb);
    const char *txt;
    lv_keyboard_mode_t mode = lv_keyboard_get_mode(kb);

    if (id == LV_BUTTONMATRIX_BUTTON_NONE) {
        return;
    }
    txt = lv_buttonmatrix_get_button_text(kb, id);
    if (txt == NULL || lv_buttonmatrix_has_button_ctrl(kb, id, LV_BUTTONMATRIX_CTRL_HIDDEN)) {
        return;
    }
    if (strcmp(txt, KB_SHIFT) == 0) {
        lv_keyboard_set_mode(kb, mode == LV_KEYBOARD_MODE_TEXT_UPPER ? LV_KEYBOARD_MODE_TEXT_LOWER
                                                                     : LV_KEYBOARD_MODE_TEXT_UPPER);
        return;
    }
    if (strcmp(txt, "?123") == 0) {
        lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_SPECIAL);
        return;
    }
    if (strcmp(txt, "abc") == 0) {
        lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
        return;
    }
    if (strcmp(txt, "OK") == 0) {
        lv_obj_send_event(kb, LV_EVENT_READY, NULL);
        if (ta != NULL) {
            lv_obj_send_event(ta, LV_EVENT_READY, NULL);
        }
        return;
    }
    if (ta == NULL) {
        return;
    }
    if (strcmp(txt, KB_BKSP) == 0) {
        lv_textarea_delete_char(ta);
    } else {
        lv_textarea_add_text(ta, txt);
    }
}

void kit_style_keyboard(lv_obj_t *keyboard)
{
    const ui_skin_t *sk = ui_skin();
    const lv_font_t *font = kit_is_instrument() ? sk->font_heading : kit_is_orbit() ? sk->font_heading : &at_sans_26;

    if (keyboard == NULL) {
        return;
    }
    lv_obj_remove_event_cb(keyboard, lv_keyboard_def_event_cb);
    lv_obj_add_event_cb(keyboard, keyboard_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    ui_attach_click_feedback(keyboard, LV_EVENT_VALUE_CHANGED);
    lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER, kb_lower, kb_ctrl);
    lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_TEXT_UPPER, kb_upper, kb_ctrl);
    lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_SPECIAL, kb_special, kb_ctrl_special);
    lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_NUMBER, kb_number, kb_ctrl_number);
    lv_keyboard_set_mode(keyboard, lv_keyboard_get_mode(keyboard));
    lv_keyboard_set_popovers(keyboard, false);

    lv_obj_remove_style_all(keyboard);
    lv_obj_set_style_bg_opa(keyboard, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(keyboard, 0, 0);
    lv_obj_set_style_pad_row(keyboard, kit_is_orbit() ? 8 : 6, 0);
    lv_obj_set_style_pad_column(keyboard, kit_is_orbit() ? 8 : 6, 0);
    lv_obj_set_style_text_font(keyboard, font, LV_PART_ITEMS);
    lv_obj_set_style_text_color(keyboard, sk->text, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(keyboard, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_opa(keyboard, LV_OPA_70, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_font(keyboard, sk->font_body, LV_PART_ITEMS | LV_STATE_CHECKED);
    if (kit_is_instrument()) {
        lv_obj_set_style_bg_color(keyboard, sk->raised, LV_PART_ITEMS);
        lv_obj_set_style_border_width(keyboard, 1, LV_PART_ITEMS);
        lv_obj_set_style_border_color(keyboard, sk->line, LV_PART_ITEMS);
        lv_obj_set_style_radius(keyboard, sk->radius_s, LV_PART_ITEMS);
        lv_obj_set_style_bg_color(keyboard, sk->well, LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_text_color(keyboard, sk->text_2, LV_PART_ITEMS | LV_STATE_CHECKED);
    } else if (kit_is_orbit()) {
        lv_obj_set_style_bg_color(keyboard, sk->raised, LV_PART_ITEMS);
        lv_obj_set_style_radius(keyboard, 14, LV_PART_ITEMS);
        lv_obj_set_style_bg_color(keyboard, sk->surface, LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_text_color(keyboard, sk->text_2, LV_PART_ITEMS | LV_STATE_CHECKED);
    } else {
        lv_obj_set_style_bg_color(keyboard, sk->surface, LV_PART_ITEMS);
        lv_obj_set_style_border_width(keyboard, 1, LV_PART_ITEMS);
        lv_obj_set_style_border_color(keyboard, sk->line, LV_PART_ITEMS);
        lv_obj_set_style_radius(keyboard, sk->radius_s, LV_PART_ITEMS);
        lv_obj_set_style_bg_color(keyboard, sk->raised, LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_text_color(keyboard, sk->value, LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_text_letter_space(keyboard, 2, LV_PART_ITEMS | LV_STATE_CHECKED);
    }
}

/* --- Field ------------------------------------------------------------------------------ */

void kit_style_field(lv_obj_t *textarea)
{
    const ui_skin_t *sk = ui_skin();
    const lv_font_t *font = sk->font_heading;
    int32_t h = kit_is_orbit() ? 80 : 72;
    int32_t lh = lv_font_get_line_height(font);

    if (textarea == NULL) {
        return;
    }
    lv_obj_remove_style_all(textarea);
    lv_obj_set_height(textarea, h);
    lv_obj_set_scrollbar_mode(textarea, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_opa(textarea, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(textarea, font, 0);
    lv_obj_set_style_text_color(textarea, sk->text, 0);
    lv_obj_set_style_pad_ver(textarea, (h - lh) / 2 - (kit_is_orbit() ? 2 : 1), 0);
    lv_obj_set_style_text_color(textarea, sk->text_3, LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_bg_opa(textarea, LV_OPA_TRANSP, LV_PART_CURSOR);
    lv_obj_set_style_border_side(textarea, LV_BORDER_SIDE_LEFT, LV_PART_CURSOR);
    lv_obj_set_style_border_width(textarea, kit_is_orbit() ? 3 : 2, LV_PART_CURSOR);
    lv_obj_set_style_border_color(textarea, sk->accent, LV_PART_CURSOR);
    lv_obj_set_style_anim_duration(textarea, 500, LV_PART_CURSOR);
    lv_obj_set_style_text_letter_space(textarea, 3, 0);
    if (kit_is_instrument()) {
        lv_obj_set_style_bg_color(textarea, sk->well, 0);
        lv_obj_set_style_border_width(textarea, 1, 0);
        lv_obj_set_style_border_color(textarea, sk->line, 0);
        lv_obj_set_style_radius(textarea, sk->radius_s, 0);
        lv_obj_set_style_pad_hor(textarea, 20, 0);
    } else if (kit_is_orbit()) {
        lv_obj_set_style_bg_color(textarea, sk->raised, 0);
        lv_obj_set_style_border_width(textarea, 2, 0);
        lv_obj_set_style_border_color(textarea, sk->accent, 0);
        lv_obj_set_style_radius(textarea, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_pad_left(textarea, 32, 0);
        lv_obj_set_style_pad_right(textarea, 24, 0);
    } else {
        lv_obj_set_style_bg_color(textarea, sk->surface, 0);
        lv_obj_set_style_border_width(textarea, 1, 0);
        lv_obj_set_style_border_color(textarea, sk->accent, 0);
        lv_obj_set_style_radius(textarea, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_pad_hor(textarea, 28, 0);
        lv_obj_set_style_text_color(textarea, sk->value, 0);
    }
    lv_textarea_set_password_bullet(textarea, "\xE2\x80\xA2");
}

/* --- Tint ------------------------------------------------------------------------------- */

void kit_set_tint(lv_obj_t *obj, lv_color_t tint)
{
    const ui_skin_t *sk = ui_skin();
    seg_data_t *d;

    if (obj == NULL) {
        return;
    }
    if (lv_obj_check_type(obj, &lv_button_class)) {
        kit_button_set_tint(obj, tint);
    } else if (lv_obj_check_type(obj, &lv_switch_class)) {
        if (kit_is_instrument()) {
            lv_obj_set_style_bg_color(obj, tint, LV_PART_INDICATOR | LV_STATE_CHECKED);
        } else if (kit_is_orbit()) {
            bool silver = lv_color_eq(tint, sk->tint_other);

            lv_obj_set_style_bg_color(obj, silver ? kit_rgb(0xE0E4E8) : tint, LV_PART_INDICATOR | LV_STATE_CHECKED);
            lv_obj_set_style_bg_color(obj, silver ? sk->surface : kit_rgb(0xF8FCF8), LV_PART_KNOB | LV_STATE_CHECKED);
        } else {
            lv_obj_set_style_border_color(obj, tint, LV_STATE_CHECKED);
        }
    } else if (lv_obj_check_type(obj, &lv_slider_class)) {
        lv_obj_set_style_bg_color(obj, tint, LV_PART_INDICATOR);
    } else if (!kit_is_orbit()) {
        return;
    } else if ((d = kit_event_data(obj, seg_free_cb)) != NULL) {
        for (int i = 0; i < d->count; i++) {
            lv_obj_set_style_bg_color(d->seg[i], tint, LV_STATE_CHECKED);
            seg_style_item(d->seg[i], i == d->selected);
        }
    } else if (lv_obj_has_flag(obj, LV_OBJ_FLAG_CHECKABLE)) {
        /* Day key: the label colour follows in day_state_cb. */
        lv_obj_set_style_bg_color(obj, tint, LV_STATE_CHECKED);
    }
}
