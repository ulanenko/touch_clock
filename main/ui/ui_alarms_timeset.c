/*
 * The alarm editor's time setter, one per style. All three change the same draft
 * (ctx->alarms.editor_draft) and call alm_editor_time_changed(); labels and marks are only
 * touched when the hour or minute actually changes.
 *
 *   Instrument: counter drums, two rollers in one recessed window.
 *   Orbit:      two rollers inside a 24 h ring; the orange arc runs from now to the alarm.
 *   Atelier:    set like a watch: drag the hour jewel round the inner 24 h ring and the
 *               minute jewel round the outer minute track. The static dial (guilloche,
 *               numerals, ticks) is drawn once into a PSRAM canvas.
 */
#include "ui/ui_alarms_internal.h"

#include "assets/skin_fonts/skin_fonts.h"
#include "assets/ui_assets/alarms_assets.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "lvgl_private.h" /* lv_hit_test_info_t */

/* Orbit ring (design/ui_concepts/orbit.html, screen 10). */
#define O_CX 360
#define O_CY 330
#define O_R 208
#define O_W 12

/* Atelier rings (design/ui_concepts/atelier.html, screen 10a). */
#define A_HOUR_R 244
#define A_MIN_LABEL_R 292
#define A_MIN_JEWEL_R 326
#define A_GUILLOCHE_R 214
#define A_RING_SPLIT_R 268

static void roller_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);
    lv_obj_t *roller = lv_event_get_target(e);
    alarm_config_t *d = &ctx->alarms.editor_draft;
    uint32_t v = lv_roller_get_selected(roller);

    if (ctx->suppress_events) {
        return;
    }
    if (roller == ctx->alarms.ts.hour_roller) {
        d->hour = (uint8_t)(v % 24);
    } else {
        d->minute = (uint8_t)(v % 60);
    }
    alm_timeset_sync(ctx);
    alm_editor_time_changed(ctx);
}

static lv_obj_t *drum(clock_ui_context_t *ctx, lv_obj_t *parent, const char *options, int32_t x, int32_t w,
                      int32_t center_y)
{
    lv_obj_t *r = kit_roller(parent, options, 3, ui_skin()->font_time_l);

    lv_roller_set_options(r, options, LV_ROLLER_MODE_INFINITE);
    lv_obj_set_width(r, w);
    lv_obj_update_layout(r);
    lv_obj_set_pos(r, x, center_y - lv_obj_get_height(r) / 2);
    lv_obj_add_event_cb(r, roller_cb, LV_EVENT_VALUE_CHANGED, ctx);
    return r;
}

/* --- Instrument: counter drums ------------------------------------------------------------ */

static void create_drums(clock_ui_context_t *ctx, lv_obj_t *parent)
{
    const ui_skin_t *sk = ui_skin();
    alm_timeset_t *ts = &ctx->alarms.ts;
    lv_obj_t *well = kit_plain(parent);
    lv_obj_t *colon;
    lv_obj_t *l;

    lv_obj_set_pos(well, 216, 120);
    lv_obj_set_size(well, 288, 96);
    lv_obj_set_style_bg_opa(well, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(well, sk->well, 0);
    lv_obj_set_style_border_width(well, 1, 0);
    lv_obj_set_style_border_color(well, sk->line, 0);
    lv_obj_set_style_radius(well, sk->radius_s, 0);

    /* The figures sit optically centred in the window (the font's box is taller above). */
    ts->hour_roller = drum(ctx, parent, ctx->hour_options, 216, 128, 172);
    ts->minute_roller = drum(ctx, parent, ctx->minute_options, 376, 128, 172);
    for (int i = 0; i < 2; i++) {
        lv_obj_t *r = i == 0 ? ts->hour_roller : ts->minute_roller;

        /* One shared window: the rollers' own windows are cleared, and only its outer
         * edges carry the signal marks. */
        lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, LV_PART_SELECTED);
        lv_obj_set_style_border_width(r, 0, LV_PART_SELECTED);
        kit_roller_set_marks(r, i == 0, i == 1);
    }
    /* The colon, centred in the window between the drums. */
    colon = kit_plain(parent);
    lv_obj_set_pos(colon, 344, 121);
    lv_obj_set_size(colon, 32, 94);
    lv_obj_set_style_bg_opa(colon, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(colon, sk->well, 0);
    l = alm_label(colon, sk->font_time_l, sk->text_3, ":");
    lv_obj_align(l, LV_ALIGN_CENTER, 0, -1);
}

/* --- Orbit: 24 h ring with rollers ------------------------------------------------------- */

static int32_t day_angle_x10(int hour, int minute)
{
    return (hour * 60 + minute) * 3600 / 1440; /* 0.1 degree, midnight at twelve */
}

static lv_obj_t *dot(lv_obj_t *parent, int32_t d, lv_color_t fill, int32_t border, lv_color_t border_color)
{
    lv_obj_t *o = kit_plain(parent);

    lv_obj_set_size(o, d, d);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(o, fill, 0);
    if (border > 0) {
        lv_obj_set_style_border_width(o, border, 0);
        lv_obj_set_style_border_color(o, border_color, 0);
    }
    return o;
}

static void place_on_ring(lv_obj_t *o, int32_t cx, int32_t cy, int32_t r, int32_t angle_x10)
{
    int32_t x;
    int32_t y;

    alm_polar(cx, cy, r, angle_x10, &x, &y);
    lv_obj_set_pos(o, x - lv_obj_get_width(o) / 2, y - lv_obj_get_height(o) / 2);
}

static void create_orbit(clock_ui_context_t *ctx, lv_obj_t *parent)
{
    const ui_skin_t *sk = ui_skin();
    alm_timeset_t *ts = &ctx->alarms.ts;
    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_t *colon;
    static const int32_t quarter[4][2] = {{360, 140}, {550, 330}, {360, 520}, {170, 330}};

    lv_obj_remove_style_all(arc);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(arc, 2 * (O_R + O_W / 2), 2 * (O_R + O_W / 2));
    lv_obj_set_pos(arc, O_CX - O_R - O_W / 2, O_CY - O_R - O_W / 2);
    lv_arc_set_rotation(arc, 270);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_obj_set_style_arc_width(arc, O_W, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, sk->surface, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, O_W, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, sk->tint_alarms, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    kit_arc_bake(arc);
    ts->arc = arc;
    for (int i = 0; i < 4; i++) {
        lv_obj_t *d = dot(parent, 6, lv_color_hex(0x484848), 0, sk->bg);

        lv_obj_set_pos(d, quarter[i][0] - 3, quarter[i][1] - 3);
    }

    ts->hour_roller = drum(ctx, parent, ctx->hour_options, 192, 150, O_CY);
    ts->minute_roller = drum(ctx, parent, ctx->minute_options, 378, 150, O_CY);
    colon = alm_label(parent, sk->font_time_l, sk->text, ":");
    lv_obj_set_width(colon, 60);
    lv_obj_set_style_text_align(colon, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(colon, 330, O_CY - lv_font_get_line_height(sk->font_time_l) / 2 - 6);

    ts->now_dot = dot(parent, 20, lv_color_white(), 3, lv_color_black());
    ts->knob = dot(parent, 42, sk->tint_alarms, 4, lv_color_black());

    ctx->alarms.editor_countdown = alm_label(parent, &or_medium_22, sk->tint_alarms, "");
    lv_obj_set_width(ctx->alarms.editor_countdown, 400);
    lv_obj_set_style_text_align(ctx->alarms.editor_countdown, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(ctx->alarms.editor_countdown, LV_ALIGN_TOP_MID, 0, 474);
}

static void orbit_arc(clock_ui_context_t *ctx, time_t now)
{
    alm_timeset_t *ts = &ctx->alarms.ts;
    const alarm_config_t *d = &ctx->alarms.editor_draft;
    struct tm tm;
    int32_t a_now;
    int32_t a_alarm = day_angle_x10(d->hour, d->minute);

    localtime_r(&now, &tm);
    a_now = day_angle_x10(tm.tm_hour, tm.tm_min);
    if (a_now != ts->shown_now_angle) {
        ts->shown_now_angle = (int16_t)a_now;
        place_on_ring(ts->now_dot, O_CX, O_CY, O_R, a_now);
    }
    place_on_ring(ts->knob, O_CX, O_CY, O_R, a_alarm);
    /* lv_arc wraps through 360 when the end is before the start. */
    lv_arc_set_angles(ts->arc, (lv_value_precise_t)(a_now / 10), (lv_value_precise_t)(a_alarm / 10));
}

/* --- Atelier: watch-setting rings ---------------------------------------------------------- */

static void draw_text_at(lv_layer_t *layer, const lv_font_t *font, lv_color_t color, int32_t cx, int32_t cy,
                         const char *text)
{
    lv_draw_label_dsc_t d;
    lv_point_t size;
    lv_area_t a;

    lv_draw_label_dsc_init(&d);
    d.font = font;
    d.color = color;
    d.text = text;
    d.text_local = 1;
    d.align = LV_TEXT_ALIGN_CENTER;
    lv_text_get_size(&size, text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    a.x1 = cx - size.x / 2 - 2;
    a.x2 = cx + size.x / 2 + 2;
    a.y1 = cy - size.y / 2;
    a.y2 = a.y1 + size.y - 1;
    lv_draw_label(layer, &d, &a);
}

/* Blends colour c at coverage a (0..1) into an RGB565 pixel. */
static void dial_px(uint16_t *px, lv_color_t c, float a)
{
    uint32_t m = (uint32_t)lroundf(a * 255.0f);
    uint32_t inv = 255 - m;
    uint32_t v = *px;
    uint32_t r = ((c.red >> 3) * m + (v >> 11) * inv + 127) / 255;
    uint32_t g = ((c.green >> 2) * m + ((v >> 5) & 0x3F) * inv + 127) / 255;
    uint32_t b = ((c.blue >> 3) * m + (v & 0x1F) * inv + 127) / 255;

    *px = (uint16_t)((r << 11) | (g << 5) | b);
}

/* Coverage of a pixel at distance d from the centre by a circle line of the given width
 * whose outer edge has radius r_outer (as an LVGL arc draws it). LVGL's antialiased hairlines
 * come out lighter than the exact coverage; `weight` matches them (measured on the dial: the
 * fine grooves about 0.6, single rings about 0.85). */
static float dial_cover(float d, float r_outer, float width, float weight)
{
    return weight * LV_CLAMP(0.0f, width / 2.0f + 0.5f - fabsf(d - (r_outer - width / 2.0f)), 1.0f);
}

/* The dial's concentric lines straight into its RGB565 canvas: the guilloche grooves (every
 * 4 px from 6 to 210, one RGB565 step above the ink), its 214 px rim, the 344 px minute
 * track and the 356 px bezel hairline. As 55 LVGL arcs they made the first open of the
 * editor hitch by 50-100 ms; here only pixels near a line are visited. */
static void render_dial_lines(lv_obj_t *canvas)
{
    const ui_skin_t *sk = ui_skin();
    lv_draw_buf_t *buf = lv_canvas_get_draw_buf(canvas);
    const lv_color_t groove = lv_color_hex(0x1C1824);
    const float disc = (float)A_GUILLOCHE_R + 1.5f;
    const float band_in = 341.0f;
    const float band_out = 357.5f;

    if (buf == NULL || buf->header.cf != LV_COLOR_FORMAT_RGB565) {
        return;
    }
    for (int32_t y = 0; y < (int32_t)buf->header.h; y++) {
        uint16_t *row = (uint16_t *)(buf->data + (size_t)y * buf->header.stride);
        float dy = (float)y + 0.5f - (float)KIT_CENTER;
        float dy2 = dy * dy;

        if (dy2 < disc * disc) {
            int32_t half = (int32_t)ceilf(sqrtf(disc * disc - dy2));

            for (int32_t x = LV_MAX(0, KIT_CENTER - half); x < LV_MIN(KIT_SCREEN, KIT_CENTER + half); x++) {
                float dx = (float)x + 0.5f - (float)KIT_CENTER;
                float d = sqrtf(dx * dx + dy2);
                /* The nearest groove: outer radius 6 + 4k, k = 0..51. */
                int32_t k = LV_CLAMP(0, (int32_t)lroundf((d + 0.5f - 6.0f) / 4.0f), 51);
                float a = dial_cover(d, (float)(6 + 4 * k), 1.0f, 0.6f);

                if (a > 0.0f) {
                    dial_px(&row[x], groove, a);
                }
                a = dial_cover(d, (float)A_GUILLOCHE_R, 1.0f, 0.85f);
                if (a > 0.0f) {
                    dial_px(&row[x], sk->line, a);
                }
            }
        }
        if (dy2 < band_out * band_out) {
            float x_out = sqrtf(band_out * band_out - dy2);
            float x_in = dy2 < band_in * band_in ? sqrtf(band_in * band_in - dy2) : 0.0f;

            for (int side = -1; side <= 1; side += 2) {
                int32_t x0 = KIT_CENTER + (side < 0 ? (int32_t)floorf(-x_out) : (int32_t)floorf(x_in)) - 1;
                int32_t x1 = KIT_CENTER + (side < 0 ? (int32_t)ceilf(-x_in) : (int32_t)ceilf(x_out)) + 1;

                for (int32_t x = LV_MAX(0, x0); x < LV_MIN(KIT_SCREEN, x1); x++) {
                    float dx = (float)x + 0.5f - (float)KIT_CENTER;
                    float d = sqrtf(dx * dx + dy2);
                    float a = dial_cover(d, 356.0f, 1.0f, 0.85f) * 140.0f / 255.0f;

                    if (a > 0.0f) {
                        dial_px(&row[x], sk->accent_deep, a);
                    }
                    a = dial_cover(d, 344.0f, 2.0f, 0.85f);
                    if (a > 0.0f) {
                        dial_px(&row[x], sk->line, a);
                    }
                }
            }
        }
    }
}

/* The static dial, drawn once: guilloche disc, minute track, numerals. */
static void render_dial(lv_obj_t *canvas)
{
    const ui_skin_t *sk = ui_skin();
    lv_layer_t layer;
    lv_draw_line_dsc_t line;
    char text[4];

    lv_canvas_fill_bg(canvas, sk->bg, LV_OPA_COVER);
    render_dial_lines(canvas);
    lv_canvas_init_layer(canvas, &layer);

    lv_draw_line_dsc_init(&line);
    for (int i = 0; i < 60; i++) {
        bool major = i % 5 == 0;
        int32_t x0;
        int32_t y0;
        int32_t x1;
        int32_t y1;

        alm_polar(KIT_CENTER, KIT_CENTER, major ? 316 : 322, i * 60, &x0, &y0);
        alm_polar(KIT_CENTER, KIT_CENTER, 334, i * 60, &x1, &y1);
        line.color = major ? sk->accent_deep : sk->line_strong;
        line.width = major ? 2 : 1;
        line.p1.x = x0;
        line.p1.y = y0;
        line.p2.x = x1;
        line.p2.y = y1;
        lv_draw_line(&layer, &line);
    }
    for (int m = 0; m < 60; m += 5) {
        int32_t x;
        int32_t y;

        alm_polar(KIT_CENTER, KIT_CENTER, A_MIN_LABEL_R, m * 60, &x, &y);
        snprintf(text, sizeof(text), "%02d", m);
        draw_text_at(&layer, &at_sans_medium_18, sk->text_2, x, y, text);
    }
    for (int h = 0; h < 24; h++) {
        int32_t x;
        int32_t y;

        alm_polar(KIT_CENTER, KIT_CENTER, A_HOUR_R, h * 150, &x, &y);
        snprintf(text, sizeof(text), "%02d", h);
        draw_text_at(&layer, &at_serif_30, h % 6 == 0 ? sk->value : sk->text_3, x, y, text);
    }
    lv_canvas_finish_layer(canvas, &layer);
}

static void ring_set(clock_ui_context_t *ctx, int32_t angle, uint8_t ring)
{
    alarm_config_t *d = &ctx->alarms.editor_draft;

    if (ring == 1) {
        uint8_t h = (uint8_t)(((angle + 75) / 150) % 24);

        if (h == d->hour) {
            return;
        }
        d->hour = h;
    } else {
        uint8_t m = (uint8_t)(((angle + 30) / 60) % 60);

        if (m == d->minute) {
            return;
        }
        d->minute = m;
    }
    alm_timeset_sync(ctx);
    alm_editor_time_changed(ctx);
}

static void ring_touch_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);
    alm_timeset_t *ts = &ctx->alarms.ts;
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_active();
    lv_point_t p;
    int32_t dist;
    int32_t angle;

    if (code == LV_EVENT_HIT_TEST) {
        lv_hit_test_info_t *info = lv_event_get_param(e);
        int32_t r;

        alm_angle_of(KIT_CENTER, KIT_CENTER, info->point->x, info->point->y, &r);
        info->res = r >= A_GUILLOCHE_R - 4 && r <= KIT_CENTER + 4;
        return;
    }
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        ts->drag_ring = 0;
        return;
    }
    if (indev == NULL) {
        return;
    }
    lv_indev_get_point(indev, &p);
    angle = alm_angle_of(KIT_CENTER, KIT_CENTER, p.x, p.y, &dist);
    if (code == LV_EVENT_PRESSED) {
        ts->drag_ring = dist < A_RING_SPLIT_R ? 1 : 2;
    }
    if (ts->drag_ring != 0 && dist > 40) {
        ring_set(ctx, angle, ts->drag_ring);
    }
}

static void create_atelier(clock_ui_context_t *ctx, lv_obj_t *parent)
{
    const ui_skin_t *sk = ui_skin();
    alm_timeset_t *ts = &ctx->alarms.ts;
    lv_obj_t *arc;
    lv_obj_t *row;
    lv_obj_t *colon;

    /* The dial canvas is created on first open (alm_timeset_sync) to keep boot light. */
    arc = lv_arc_create(parent);
    lv_obj_remove_style_all(arc);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(arc, 2 * 345, 2 * 345);
    lv_obj_set_pos(arc, KIT_CENTER - 345, KIT_CENTER - 345);
    lv_arc_set_rotation(arc, 270);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_obj_set_style_arc_width(arc, 0, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 2, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, sk->accent, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_INDICATOR);
    kit_arc_bake(arc);
    ts->arc = arc;

    ts->minute_mark = alm_label(parent, &at_sans_medium_18, sk->accent, "00");
    ts->hour_jewel = dot(parent, 54, sk->accent, 0, sk->bg);
    lv_obj_set_style_outline_width(ts->hour_jewel, 4, 0);
    lv_obj_set_style_outline_color(ts->hour_jewel, sk->bg, 0);
    ts->hour_jewel_label = alm_label(ts->hour_jewel, &at_serif_30, sk->accent_text, "00");
    lv_obj_align(ts->hour_jewel_label, LV_ALIGN_CENTER, 0, 1);
    ts->knob = lv_image_create(parent);
    lv_image_set_src(ts->knob, &ui_alarms_jewel_36);
    lv_obj_remove_flag(ts->knob, LV_OBJ_FLAG_CLICKABLE);

    /* Centre: repeat, time with brass colon and gilt minutes, countdown. */
    ts->repeat = kit_label(parent, KIT_TEXT_LABEL, "");
    lv_obj_set_width(ts->repeat, 300);
    lv_obj_set_style_text_align(ts->repeat, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(ts->repeat, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_align(ts->repeat, LV_ALIGN_TOP_MID, 0, 262 - 13);
    row = kit_plain(parent);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 338 - lv_font_get_line_height(sk->font_time_l) / 2);
    ts->hh = alm_label(row, sk->font_time_l, sk->text, "00");
    colon = alm_label(row, sk->font_time_l, sk->label, ":");
    lv_obj_set_style_margin_hor(colon, 2, 0);
    ts->mm = alm_label(row, sk->font_time_l, sk->accent, "00");
    ctx->alarms.editor_countdown = alm_label(parent, sk->font_body, sk->text_2, "");
    lv_obj_set_width(ctx->alarms.editor_countdown, 320);
    lv_obj_set_style_text_align(ctx->alarms.editor_countdown, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(ctx->alarms.editor_countdown, LV_ALIGN_TOP_MID, 0, 408 - 15);

    /* The rings take the drags; the centre (pushers, page scroll) stays free. */
    ts->touch = kit_plain(parent);
    lv_obj_set_size(ts->touch, KIT_SCREEN, KIT_SCREEN);
    lv_obj_add_flag(ts->touch, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_remove_flag(ts->touch, LV_OBJ_FLAG_SCROLL_CHAIN_HOR | LV_OBJ_FLAG_SCROLL_CHAIN_VER | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(ts->touch, ring_touch_cb, LV_EVENT_HIT_TEST, ctx);
    lv_obj_add_event_cb(ts->touch, ring_touch_cb, LV_EVENT_PRESSED, ctx);
    lv_obj_add_event_cb(ts->touch, ring_touch_cb, LV_EVENT_PRESSING, ctx);
    lv_obj_add_event_cb(ts->touch, ring_touch_cb, LV_EVENT_RELEASED, ctx);
    lv_obj_add_event_cb(ts->touch, ring_touch_cb, LV_EVENT_PRESS_LOST, ctx);
    ui_attach_click_feedback(ts->touch, LV_EVENT_PRESSED);
}

static void atelier_ensure_dial(clock_ui_context_t *ctx)
{
    alm_timeset_t *ts = &ctx->alarms.ts;
    lv_obj_t *parent;

    if (ts->dial != NULL || ts->arc == NULL) {
        return;
    }
    parent = lv_obj_get_parent(ts->arc);
    ts->dial = alm_canvas(parent, KIT_SCREEN, KIT_SCREEN, &ctx->alarms.dial_buf);
    if (ts->dial == NULL) {
        return;
    }
    lv_obj_set_pos(ts->dial, 0, 0);
    lv_obj_move_to_index(ts->dial, 0);
#if CONFIG_TOUCH_CLOCK_PERF_TOUR
    int64_t started_us = esp_timer_get_time();
#endif
    render_dial(ts->dial);
#if CONFIG_TOUCH_CLOCK_PERF_TOUR
    printf("PERF dial_build us=%lld t_ms=%lld\n", esp_timer_get_time() - started_us, esp_timer_get_time() / 1000);
#endif
}

static void atelier_sync(clock_ui_context_t *ctx, bool hour_changed, bool minute_changed)
{
    alm_timeset_t *ts = &ctx->alarms.ts;
    const alarm_config_t *d = &ctx->alarms.editor_draft;
    char text[8];

    if (hour_changed) {
        snprintf(text, sizeof(text), "%02u", d->hour);
        kit_label_set_text_if_changed(ts->hour_jewel_label, text);
        kit_label_set_text_if_changed(ts->hh, text);
        place_on_ring(ts->hour_jewel, KIT_CENTER, KIT_CENTER, A_HOUR_R, d->hour * 150);
    }
    if (minute_changed) {
        snprintf(text, sizeof(text), "%02u", d->minute);
        kit_label_set_text_if_changed(ts->mm, text);
        place_on_ring(ts->knob, KIT_CENTER, KIT_CENTER, A_MIN_JEWEL_R, d->minute * 60);
        lv_arc_set_angles(ts->arc, 0, (lv_value_precise_t)(d->minute * 6));
        kit_show(ts->minute_mark, d->minute % 5 == 0);
        if (d->minute % 5 == 0) {
            kit_label_set_text_if_changed(ts->minute_mark, text);
            lv_obj_update_layout(ts->minute_mark);
            place_on_ring(ts->minute_mark, KIT_CENTER, KIT_CENTER, A_MIN_LABEL_R, d->minute * 60);
        }
    }
}

/* Builds the Atelier dial a few seconds after the chrome, so the first open of the editor
 * does not wait for it. The timer runs once and stays paused (the chrome deletes it). */
static void dial_prebuild_cb(lv_timer_t *timer)
{
    lv_timer_pause(timer);
    atelier_ensure_dial(lv_timer_get_user_data(timer));
}

/* --- Common --------------------------------------------------------------------------------- */

void alm_timeset_create(clock_ui_context_t *ctx, lv_obj_t *parent)
{
    ctx->alarms.ts.shown_hour = -1;
    ctx->alarms.ts.shown_minute = -1;
    ctx->alarms.ts.shown_now_angle = -1;
    if (ui_skin_is_instrument()) {
        create_drums(ctx, parent);
    } else if (ui_skin_is_orbit()) {
        create_orbit(ctx, parent);
    } else {
        create_atelier(ctx, parent);
        ui_chrome_timer_create(ctx, dial_prebuild_cb, 3000, ctx);
    }
}

void alm_timeset_sync(clock_ui_context_t *ctx)
{
    alm_timeset_t *ts = &ctx->alarms.ts;
    const alarm_config_t *d = &ctx->alarms.editor_draft;
    bool hour_changed = ts->shown_hour != (int8_t)d->hour;
    bool minute_changed = ts->shown_minute != (int8_t)d->minute;

    if (ui_skin_is_atelier()) {
        atelier_ensure_dial(ctx);
        kit_label_set_text_if_changed(ts->repeat, alm_repeat_title(ctx, d));
    }
    if (!hour_changed && !minute_changed) {
        return;
    }
    ts->shown_hour = (int8_t)d->hour;
    ts->shown_minute = (int8_t)d->minute;
    if (ts->hour_roller != NULL) {
        ctx->suppress_events = true;
        if ((int)lv_roller_get_selected(ts->hour_roller) != d->hour) {
            lv_roller_set_selected(ts->hour_roller, d->hour, LV_ANIM_OFF);
        }
        if ((int)lv_roller_get_selected(ts->minute_roller) != d->minute) {
            lv_roller_set_selected(ts->minute_roller, d->minute, LV_ANIM_OFF);
        }
        ctx->suppress_events = false;
    }
    if (ui_skin_is_orbit()) {
        orbit_arc(ctx, alm_now());
    } else if (ui_skin_is_atelier()) {
        atelier_sync(ctx, hour_changed, minute_changed);
    }
}

void alm_timeset_tick(clock_ui_context_t *ctx, time_t now)
{
    if (ui_skin_is_orbit() && ctx->alarms.ts.arc != NULL) {
        orbit_arc(ctx, now);
    }
}

void alm_timeset_free(clock_ui_context_t *ctx)
{
    alm_timeset_t *ts = &ctx->alarms.ts;

    if (ts->dial != NULL) {
        lv_obj_delete(ts->dial);
        ts->dial = NULL;
    }
    if (ctx->alarms.dial_buf != NULL) {
        heap_caps_free(ctx->alarms.dial_buf);
        ctx->alarms.dial_buf = NULL;
    }
}
