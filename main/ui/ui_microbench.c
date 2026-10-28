/*
 * Synthetic scenes for calibrating the simulator's cost model (sim/cost_fit.py).
 *
 * Each scene isolates one kind of drawing (images of each format, fills, text, arcs, layers)
 * on a full-screen black ground. The perf tour (CONFIG_TOUCH_CLOCK_PERF_TOUR_MICROBENCH)
 * redraws each one continuously on the clock and records the frame times; the simulator
 * counts the same scenes' draw tasks and pixels ("microbench" command). Fitting one against
 * the other gives the device's cost per task and per pixel of each kind, which the simulator
 * then uses to estimate any screen ("estimate" command).
 *
 * Scene "cpu_loop" is not drawn: it times a per-pixel loop over PSRAM like the glass bake, to
 * scale host CPU time to the device.
 */
#include "ui/ui_microbench.h"

#include <stdio.h>
#include <string.h>

#include "ui/kit/ui_kit.h"

#define MB_SCREEN 720

static lv_obj_t *s_root;
static lv_draw_buf_t *s_full565;
static lv_draw_buf_t *s_part565;
static lv_draw_buf_t *s_fullargb;
static lv_timer_t *s_redraw;

static const char *const s_names[] = {
    "base",        "img565_full", "img565_part", "img565_round", "img565_opa",  "imgargb_full",
    "argb_sparse", "fill_plain",  "fill_round",  "fill_opa",     "fill_grad",   "border",
    "shadow",      "label_small", "label_big",   "arc",          "line",        "layer_opa",
    "transform",   "img565_part8", "fill_plain_big", "label_many", "cpu_loop",
};

int ui_microbench_count(void)
{
    return (int)(sizeof(s_names) / sizeof(s_names[0]));
}

const char *ui_microbench_name(int index)
{
    return index >= 0 && index < ui_microbench_count() ? s_names[index] : "?";
}

static lv_draw_buf_t *make_buf(int32_t w, int32_t h, lv_color_format_t cf)
{
    lv_draw_buf_t *b = lv_draw_buf_create((uint32_t)w, (uint32_t)h, cf, LV_STRIDE_AUTO);

    if (b == NULL) {
        return NULL;
    }
    for (int32_t y = 0; y < h; y++) {
        uint8_t *row = b->data + (size_t)y * b->header.stride;

        for (int32_t x = 0; x < w; x++) {
            uint8_t r = (uint8_t)(x * 255 / w);
            uint8_t g = (uint8_t)(y * 255 / h);
            uint8_t bl = (uint8_t)((x + y) * 127 / (w + h));

            if (cf == LV_COLOR_FORMAT_RGB565) {
                ((uint16_t *)row)[x] = (uint16_t)((r >> 3) << 11 | (g >> 2) << 5 | (bl >> 3));
            } else {
                /* Dense alpha everywhere: a worst-case ARGB image. */
                ((uint32_t *)row)[x] = (uint32_t)(64 + (x * 191 / w)) << 24 | (uint32_t)r << 16 | (uint32_t)g << 8 | bl;
            }
        }
    }
    return b;
}

static void ensure_bufs(void)
{
    if (s_full565 == NULL) {
        s_full565 = make_buf(MB_SCREEN, MB_SCREEN, LV_COLOR_FORMAT_RGB565);
    }
    if (s_part565 == NULL) {
        s_part565 = make_buf(300, 200, LV_COLOR_FORMAT_RGB565);
    }
    if (s_fullargb == NULL) {
        s_fullargb = make_buf(MB_SCREEN, MB_SCREEN, LV_COLOR_FORMAT_ARGB8888);
    }
}

static lv_obj_t *plain(lv_obj_t *parent, int32_t x, int32_t y, int32_t w, int32_t h)
{
    lv_obj_t *o = lv_obj_create(parent);

    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    return o;
}

static lv_obj_t *image(lv_obj_t *parent, const lv_draw_buf_t *src, int32_t x, int32_t y)
{
    lv_obj_t *img = lv_image_create(parent);

    lv_image_set_src(img, src);
    lv_obj_set_pos(img, x, y);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    return img;
}

/* Twelve boxes in a 3 x 4 grid, 160 x 100. */
static void boxes(lv_obj_t *parent, void (*style)(lv_obj_t *))
{
    for (int i = 0; i < 12; i++) {
        lv_obj_t *o = plain(parent, 100 + (i % 3) * 180, 140 + (i / 3) * 115, 160, 100);

        lv_obj_set_style_bg_color(o, lv_color_hex(0x3080F0), 0);
        lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
        style(o);
    }
}

static void st_plain(lv_obj_t *o)
{
    LV_UNUSED(o);
}

static void st_round(lv_obj_t *o)
{
    lv_obj_set_style_radius(o, 30, 0);
}

static void st_opa(lv_obj_t *o)
{
    lv_obj_set_style_bg_opa(o, LV_OPA_50, 0);
}

static void st_grad(lv_obj_t *o)
{
    lv_obj_set_style_bg_grad_color(o, lv_color_hex(0xF03080), 0);
    lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_VER, 0);
}

static void st_border(lv_obj_t *o)
{
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(o, 20, 0);
    lv_obj_set_style_border_width(o, 2, 0);
    lv_obj_set_style_border_color(o, lv_color_white(), 0);
}

static void ring_cb(lv_event_t *e)
{
    kit_draw_ring(lv_event_get_layer(e), 360, 360, 320, 40, 0, 360, lv_color_hex(0xC8A470), LV_OPA_COVER);
}

static void arcs_cb(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);

    for (int i = 0; i < 6; i++) {
        lv_draw_arc_dsc_t d;

        lv_draw_arc_dsc_init(&d);
        d.center.x = 360;
        d.center.y = 360;
        d.radius = (uint16_t)(320 - i * 40);
        d.width = 10;
        d.start_angle = (lv_value_precise_t)(i * 30);
        d.end_angle = (lv_value_precise_t)(i * 30 + 240);
        d.color = lv_color_hex(0x40C0F0);
        d.rounded = 1;
        lv_draw_arc(layer, &d);
    }
}

static void lines_cb(lv_event_t *e)
{
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_draw_line_dsc_t d;

    lv_draw_line_dsc_init(&d);
    d.color = lv_color_white();
    for (int i = 0; i < 60; i++) {
        float a = (float)i * 6.0f * 3.14159265f / 180.0f;

        d.width = i % 5 == 0 ? 4 : 2;
        d.p1.x = (lv_value_precise_t)(360 + 300 * lv_trigo_cos((int16_t)(i * 6)) / 32768);
        d.p1.y = (lv_value_precise_t)(360 + 300 * lv_trigo_sin((int16_t)(i * 6)) / 32768);
        d.p2.x = (lv_value_precise_t)(360 + 340 * lv_trigo_cos((int16_t)(i * 6)) / 32768);
        d.p2.y = (lv_value_precise_t)(360 + 340 * lv_trigo_sin((int16_t)(i * 6)) / 32768);
        LV_UNUSED(a);
        lv_draw_line(layer, &d);
    }
}

static void labels(lv_obj_t *parent, int count, const lv_font_t *font, const char *text)
{
    for (int i = 0; i < count; i++) {
        lv_obj_t *l = lv_label_create(parent);

        lv_obj_set_style_text_font(l, font, 0);
        lv_obj_set_style_text_color(l, lv_color_white(), 0);
        lv_label_set_text(l, text);
        lv_obj_set_pos(l, 110 + (i % 2) * 260, 120 + (i / 2) * (lv_font_get_line_height(font) + 4));
    }
}

static void redraw_cb(lv_timer_t *t)
{
    LV_UNUSED(t);
    if (s_root != NULL) {
        lv_obj_invalidate(s_root);
    }
}

/* The CPU loop: per pixel, two PSRAM reads, some integer work and a write, as the glass bake
 * does. Returns the time in microseconds. */
static int64_t cpu_loop(int64_t (*now_us)(void))
{
    static volatile uint32_t sink;
    int64_t t0;
    uint32_t acc = 0;

    ensure_bufs();
    if (s_full565 == NULL || s_part565 == NULL) {
        return 0;
    }
    t0 = now_us();
    for (int32_t y = 0; y < MB_SCREEN; y += 2) {
        const uint16_t *a = (const uint16_t *)(s_full565->data + (size_t)y * s_full565->header.stride);
        uint16_t *out = (uint16_t *)(s_full565->data + (size_t)(y + 1) * s_full565->header.stride);

        for (int32_t x = 0; x < MB_SCREEN; x++) {
            uint32_t v = a[x];
            uint32_t r = (v >> 11) * 8;
            uint32_t g = ((v >> 5) & 63) * 4;
            uint32_t b = (v & 31) * 8;
            uint32_t lum = (r * 54 + g * 183 + b * 19) >> 8;

            r = lum + (((r - lum) * 300) >> 8);
            acc += r;
            out[x] = (uint16_t)(((r >> 3) & 31) << 11 | ((g >> 2) & 63) << 5 | ((b >> 3) & 31));
        }
    }
    sink = acc;
    (void)sink;
    return now_us() - t0;
}

int64_t ui_microbench_cpu_loop(int64_t (*now_us)(void))
{
    return cpu_loop(now_us);
}

void ui_microbench_hide(void)
{
    if (s_redraw != NULL) {
        lv_timer_delete(s_redraw);
        s_redraw = NULL;
    }
    if (s_root != NULL) {
        lv_obj_delete(s_root);
        s_root = NULL;
    }
}

lv_obj_t *ui_microbench_show(int index, bool redraw_every_frame)
{
    const char *name = ui_microbench_name(index);
    lv_obj_t *r;

    ui_microbench_hide();
    ensure_bufs();
    /* On the screen, over everything: opaque, so nothing under it is drawn. */
    r = plain(lv_screen_active(), 0, 0, MB_SCREEN, MB_SCREEN);
    lv_obj_move_foreground(r);
    s_root = r;
    lv_obj_set_style_bg_color(r, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);

    if (strcmp(name, "img565_full") == 0) {
        image(r, s_full565, 0, 0);
    } else if (strcmp(name, "img565_part") == 0 || strcmp(name, "img565_round") == 0) {
        for (int i = 0; i < 4; i++) {
            lv_obj_t *img = image(r, s_part565, 50 + (i % 2) * 320, 140 + (i / 2) * 230);

            if (name[7] == 'r') {
                lv_obj_set_style_radius(img, 40, 0);
                lv_obj_set_style_clip_corner(img, true, 0);
            }
        }
    } else if (strcmp(name, "img565_part8") == 0) {
        for (int i = 0; i < 8; i++) {
            image(r, s_part565, 30 + (i % 2) * 360, 60 + (i / 2) * 150);
        }
    } else if (strcmp(name, "img565_opa") == 0) {
        lv_obj_set_style_image_opa(image(r, s_full565, 0, 0), LV_OPA_50, 0);
    } else if (strcmp(name, "imgargb_full") == 0) {
        image(r, s_fullargb, 0, 0);
    } else if (strcmp(name, "argb_sparse") == 0) {
        lv_obj_add_event_cb(r, ring_cb, LV_EVENT_DRAW_MAIN_END, NULL);
    } else if (strcmp(name, "fill_plain") == 0) {
        boxes(r, st_plain);
    } else if (strcmp(name, "fill_plain_big") == 0) {
        lv_obj_t *o = plain(r, 60, 60, 600, 600);

        lv_obj_set_style_bg_color(o, lv_color_hex(0x3080F0), 0);
        lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    } else if (strcmp(name, "fill_round") == 0) {
        boxes(r, st_round);
    } else if (strcmp(name, "fill_opa") == 0) {
        boxes(r, st_opa);
    } else if (strcmp(name, "fill_grad") == 0) {
        boxes(r, st_grad);
    } else if (strcmp(name, "border") == 0) {
        boxes(r, st_border);
    } else if (strcmp(name, "shadow") == 0) {
        for (int i = 0; i < 4; i++) {
            lv_obj_t *o = plain(r, 120 + (i % 2) * 260, 160 + (i / 2) * 220, 200, 140);

            lv_obj_set_style_bg_color(o, lv_color_hex(0x303840), 0);
            lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(o, 24, 0);
            lv_obj_set_style_shadow_width(o, 30, 0);
            lv_obj_set_style_shadow_color(o, lv_color_black(), 0);
            lv_obj_set_style_shadow_opa(o, LV_OPA_70, 0);
        }
    } else if (strcmp(name, "label_small") == 0) {
        labels(r, 16, ui_skin()->font_body, "Networks, clock, time");
    } else if (strcmp(name, "label_many") == 0) {
        labels(r, 40, ui_skin()->font_label, "Schedule 22:30");
    } else if (strcmp(name, "label_big") == 0) {
        lv_obj_t *l = lv_label_create(r);

        lv_obj_set_style_text_font(l, ui_skin()->font_display, 0);
        lv_obj_set_style_text_color(l, lv_color_white(), 0);
        lv_label_set_text(l, "12:34");
        lv_obj_center(l);
    } else if (strcmp(name, "arc") == 0) {
        lv_obj_add_event_cb(r, arcs_cb, LV_EVENT_DRAW_MAIN_END, NULL);
    } else if (strcmp(name, "line") == 0) {
        lv_obj_add_event_cb(r, lines_cb, LV_EVENT_DRAW_MAIN_END, NULL);
    } else if (strcmp(name, "layer_opa") == 0) {
        for (int i = 0; i < 6; i++) {
            lv_obj_t *o = plain(r, 80 + (i % 3) * 190, 180 + (i / 3) * 200, 180, 150);
            lv_obj_t *c = plain(o, 20, 20, 140, 110);

            lv_obj_set_style_bg_color(o, lv_color_hex(0x206040), 0);
            lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(c, lv_color_hex(0x80F0A0), 0);
            lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(c, 20, 0);
            lv_obj_set_style_opa(o, LV_OPA_60, 0);
        }
    } else if (strcmp(name, "transform") == 0) {
        lv_obj_t *img = image(r, s_part565, 210, 260);

        lv_obj_set_style_transform_scale(img, 320, 0);
        lv_obj_set_style_transform_pivot_x(img, 150, 0);
        lv_obj_set_style_transform_pivot_y(img, 100, 0);
    }
    if (redraw_every_frame) {
        s_redraw = lv_timer_create(redraw_cb, 1, NULL);
    }
    return r;
}
