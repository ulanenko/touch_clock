/*
 * Settings area: lifecycle, navigation between its pages, the settings home page and the
 * widgets the settings pages share. The pages themselves are in ui_settings_wifi.c,
 * ui_settings_night.c and ui_settings_other.c.
 *
 * Every page is a kit page (ui_kit.h) under the chrome root, built the first time it is shown
 * and kept until the chrome is rebuilt. Only the page on screen is synced, once a second and
 * on refresh, and only labels and controls whose value changed are touched.
 */
#include "ui/ui_settings_internal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#if CONFIG_TOUCH_CLOCK_PERF_TOUR
#include "esp_timer.h"
#endif

#include "app_version.h"
#include "domain/timezone_rules.h"
#include "ui/kit/ui_glass.h"

static const ui_l10n_str_t s_settings_n = UI_L10N_STR("%s · %02d", "%s · %02d", "%s · %02d", "%s · %02d", "%s · %02d");
static const ui_l10n_str_t s_months = UI_L10N_STR("Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec",
                                                  "sty lut mar kwi maj cze lip sie wrz paź lis gru",
                                                  "січ лют бер кві тра чер лип сер вер жов лис гру",
                                                  "янв фев мар апр мая июн июл авг сен окт ноя дек",
                                                  "jan feb mrt apr mei jun jul aug sep okt nov dec");
static const ui_l10n_str_t s_synced_at = UI_L10N_STR("synced %s", "synchr. %s", "синхр. %s", "синхр. %s",
                                                     "gesynchr. %s");
static const ui_l10n_str_t s_not_synced = UI_L10N_STR("not synced", "bez synchr.", "без синхр.", "без синхр.",
                                                      "niet gesynchr.");
static const ui_l10n_str_t s_not_connected = UI_L10N_STR("Not connected", "Brak połączenia", "Не підключено",
                                                         "Не подключено", "Niet verbonden");
static const ui_l10n_str_t s_update_ready = UI_L10N_STR("%s ready", "%s gotowa", "%s готова", "%s готова",
                                                        "%s klaar");

/* --- Texts ----------------------------------------------------------------------------- */

bool st_text_set(st_text_t t)
{
    return t.str != NULL || t.id < L10N_COUNT;
}

const char *st_text(const clock_ui_context_t *ctx, st_text_t t)
{
    if (t.str != NULL) {
        return ui_l10n_str(ctx, t.str);
    }
    if (t.id < L10N_COUNT) {
        return ui_l10n_text(ctx, t.id);
    }
    return "";
}

void st_bind(clock_ui_context_t *ctx, lv_obj_t *label, st_text_t t)
{
    if (label == NULL || !st_text_set(t)) {
        return;
    }
    if (t.str != NULL) {
        ui_l10n_bind_kit_str(ctx, label, t.str);
    } else {
        ui_l10n_bind_kit(ctx, label, t.id);
    }
}

lv_obj_t *st_label(clock_ui_context_t *ctx, lv_obj_t *parent, kit_text_role_t role, st_text_t t)
{
    lv_obj_t *label = kit_label(parent, role, st_text(ctx, t));

    st_bind(ctx, label, t);
    return label;
}

void st_row(clock_ui_context_t *ctx, kit_row_t *row, lv_obj_t *group, const kit_row_opts_t *opts, st_text_t title,
            st_text_t subtitle)
{
    kit_row_opts_t o = *opts;

    o.title = st_text(ctx, title);
    o.subtitle = st_text_set(subtitle) ? st_text(ctx, subtitle) : opts->subtitle;
    kit_row_create(row, group, &o);
    st_bind(ctx, row->title, title);
    st_bind(ctx, row->subtitle, subtitle);
}

/* --- Small widgets --------------------------------------------------------------------- */

static lv_obj_t *plain(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);

    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE);
    return obj;
}

lv_obj_t *st_hbox(lv_obj_t *parent, int32_t gap)
{
    lv_obj_t *box = plain(parent);

    lv_obj_set_size(box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(box, gap, 0);
    lv_obj_add_flag(box, LV_OBJ_FLAG_EVENT_BUBBLE);
    return box;
}

lv_obj_t *st_vbox(lv_obj_t *parent, int32_t gap)
{
    lv_obj_t *box = plain(parent);

    lv_obj_set_size(box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(box, gap, 0);
    lv_obj_add_flag(box, LV_OBJ_FLAG_EVENT_BUBBLE);
    return box;
}

lv_obj_t *st_dot(lv_obj_t *parent, int32_t size, lv_color_t color)
{
    lv_obj_t *dot = plain(parent);

    lv_obj_set_size(dot, size, size);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(dot, color, 0);
    lv_obj_add_flag(dot, LV_OBJ_FLAG_EVENT_BUBBLE);
    return dot;
}

lv_obj_t *st_image(lv_obj_t *parent, const lv_image_dsc_t *src, lv_color_t color)
{
    lv_obj_t *img = lv_image_create(parent);

    lv_image_set_src(img, src);
    lv_obj_set_style_image_recolor(img, color, 0);
    lv_obj_set_style_image_recolor_opa(img, LV_OPA_COVER, 0);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(img, LV_OBJ_FLAG_EVENT_BUBBLE);
    return img;
}

void st_show(lv_obj_t *obj, bool show)
{
    if (obj == NULL || lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN) == !show) {
        return;
    }
    if (show) {
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

void st_set_enabled(lv_obj_t *obj, bool enabled)
{
    if (obj == NULL || lv_obj_has_state(obj, LV_STATE_DISABLED) == !enabled) {
        return;
    }
    if (enabled) {
        lv_obj_remove_state(obj, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(obj, LV_STATE_DISABLED);
    }
}

void st_set_clickable(lv_obj_t *obj, bool clickable)
{
    if (obj == NULL || lv_obj_has_flag(obj, LV_OBJ_FLAG_CLICKABLE) == clickable) {
        return;
    }
    if (clickable) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    }
}

void st_keep_gestures(lv_obj_t *obj)
{
    if (obj != NULL) {
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_GESTURE_BUBBLE);
    }
}

void st_one_line(lv_obj_t *label, int32_t width)
{
    lv_obj_set_width(label, width);
    lv_obj_set_height(label, lv_font_get_line_height(lv_obj_get_style_text_font(label, 0)));
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
}

void st_text_color(lv_obj_t *obj, lv_color_t color)
{
    if (obj != NULL && !lv_color_eq(lv_obj_get_style_text_color(obj, 0), color)) {
        lv_obj_set_style_text_color(obj, color, 0);
    }
}

void st_bg_color(lv_obj_t *obj, lv_color_t color)
{
    if (obj != NULL && !lv_color_eq(lv_obj_get_style_bg_color(obj, 0), color)) {
        lv_obj_set_style_bg_color(obj, color, 0);
    }
}

void st_recolor(lv_obj_t *obj, lv_color_t color)
{
    if (obj != NULL && !lv_color_eq(lv_obj_get_style_image_recolor(obj, 0), color)) {
        lv_obj_set_style_image_recolor(obj, color, 0);
    }
}

/* --- Signal meter ---------------------------------------------------------------------- */

int st_signal_level(int rssi)
{
    if (rssi == 0) {
        return 0;
    }
    if (rssi >= -55) {
        return 4;
    }
    if (rssi >= -65) {
        return 3;
    }
    if (rssi >= -75) {
        return 2;
    }
    return 1;
}

static void fill_rect(lv_layer_t *layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2, lv_color_t c, int32_t r)
{
    lv_draw_rect_dsc_t d;
    lv_area_t a = {x1, y1, x2, y2};

    lv_draw_rect_dsc_init(&d);
    d.bg_color = c;
    d.radius = r;
    lv_draw_rect(layer, &d, &a);
}

static void signal_draw_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_current_target(e);
    const ui_skin_t *sk = ui_skin();
    lv_layer_t *layer = lv_event_get_layer(e);
    int level = (int)(intptr_t)lv_obj_get_user_data(obj);
    lv_area_t a;
    int32_t h;

    lv_obj_get_coords(obj, &a);
    h = lv_area_get_height(&a);
    if (st_atelier()) {
        /* Three arcs and a dot, gilt up to the level (the Atelier wifi glyph). */
        static const float radii[3] = {4.6f, 9.0f, 13.5f};
        float k = (float)h / 24.0f;
        lv_draw_arc_dsc_t arc;
        lv_draw_rect_dsc_t dot;
        lv_area_t da;
        int32_t cx = a.x1 + h / 2;
        int32_t cy = a.y1 + (int32_t)(19.2f * k);
        int32_t w = LV_MAX(2, (int32_t)(1.9f * k + 0.5f));

        for (int i = 0; i < 3; i++) {
            lv_draw_arc_dsc_init(&arc);
            arc.color = level >= i + 2 ? sk->accent : sk->line_strong;
            arc.width = w;
            arc.rounded = 1;
            arc.center.x = cx;
            arc.center.y = cy;
            arc.radius = (uint16_t)(radii[i] * k + w / 2.0f);
            arc.start_angle = 227;
            arc.end_angle = 313;
            kit_draw_arc(layer, &arc);
        }
        lv_draw_rect_dsc_init(&dot);
        dot.bg_color = level >= 1 ? sk->accent : sk->line_strong;
        dot.radius = LV_RADIUS_CIRCLE;
        da.x1 = cx - w;
        da.x2 = cx + w - 1;
        da.y1 = cy - w;
        da.y2 = cy + w - 1;
        lv_draw_rect(layer, &dot, &da);
        return;
    }

    {
        /* Four bars rising to the right. */
        int32_t bw;
        int32_t gap;
        lv_color_t on = sk->text;
        lv_color_t off = st_orbit() ? lv_color_mix(sk->text, sk->bg, 72) : sk->line_strong;

        if (st_orbit()) {
            bw = LV_MAX(4, h / 4);
            gap = LV_MAX(3, h / 6);
        } else {
            bw = (h * 22 + 50) / 100;
            gap = (h * 14 + 50) / 100;
        }
        for (int i = 0; i < 4; i++) {
            int32_t bh = st_orbit() ? h * (6 + 4 * i) / 18 : (h * (300 + 233 * i) + 500) / 1000;
            int32_t x = a.x1 + i * (bw + gap);

            fill_rect(layer, x, a.y2 - bh + 1, x + bw - 1, a.y2, i < level ? on : off, st_orbit() ? 2 : 1);
        }
    }
}

lv_obj_t *st_signal(lv_obj_t *parent, int32_t size)
{
    lv_obj_t *meter = plain(parent);
    int32_t w;

    if (st_atelier()) {
        w = size;
    } else if (st_orbit()) {
        w = 4 * LV_MAX(4, size / 4) + 3 * LV_MAX(3, size / 6);
    } else {
        w = 4 * ((size * 22 + 50) / 100) + 3 * ((size * 14 + 50) / 100);
    }
    lv_obj_set_size(meter, w, size);
    lv_obj_add_flag(meter, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(meter, signal_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
    return meter;
}

void st_signal_set(lv_obj_t *meter, int level, bool on_accent)
{
    LV_UNUSED(on_accent);
    if (meter != NULL && (int)(intptr_t)lv_obj_get_user_data(meter) != level) {
        lv_obj_set_user_data(meter, (void *)(intptr_t)level);
        lv_obj_invalidate(meter);
    }
}

/* --- Formatting ------------------------------------------------------------------------ */

void st_format_synced(clock_ui_context_t *ctx, char *out, size_t size, const ui_l10n_str_t *fmt)
{
    char clock[8];

    if (ctx->settings->last_synced_epoch <= 0) {
        snprintf(out, size, "%s", ui_l10n_str(ctx, &s_not_synced));
        return;
    }
    ui_format_clock(ctx->settings->last_synced_epoch, clock, sizeof(clock));
    snprintf(out, size, ui_l10n_str(ctx, fmt != NULL ? fmt : &s_synced_at), clock);
}

void st_format_tz(uint8_t timezone_id, char *name, size_t name_size, char *offset, size_t offset_size)
{
    char picker[40];
    const char *open;
    const char *close;
    char *slash;

    timezone_format_label(name, name_size, timezone_id);
    /* "Warsaw / Berlin" reads better as "Warsaw, Berlin" in a row. */
    while ((slash = strstr(name, " / ")) != NULL) {
        slash[0] = ',';
        memmove(slash + 1, slash + 2, strlen(slash + 2) + 1);
    }
    offset[0] = '\0';
    timezone_format_picker_label(picker, sizeof(picker), timezone_picker_index_from_id(timezone_id));
    open = strchr(picker, '(');
    close = open != NULL ? strchr(open, ')') : NULL;
    if (open != NULL && close != NULL && (size_t)(close - open) < offset_size) {
        snprintf(offset, offset_size, "%.*s", (int)(close - open - 1), open + 1);
    }
}

/* --- Pages ----------------------------------------------------------------------------- */

kit_page_t *st_page(clock_ui_context_t *ctx, ui_st_page_t id)
{
    return &ST(ctx)->pages[id];
}

ui_st_page_t st_parent(ui_st_page_t id)
{
    switch (id) {
    case UI_ST_TIMEZONE:
    case UI_ST_MANUAL_TIME:
        return UI_ST_WIFI;
    case UI_ST_SCHEDULE:
    case UI_ST_FACE:
    case UI_ST_NIGHT_BRIGHT:
    case UI_ST_SUNRISE:
        return UI_ST_NIGHT;
    case UI_ST_LANGUAGE:
    case UI_ST_STYLE:
        return UI_ST_OTHER;
    default:
        return UI_ST_HOME;
    }
}

static void gesture_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);
    lv_indev_t *indev = lv_indev_active();

    if (indev == NULL || lv_indev_get_gesture_dir(indev) != LV_DIR_RIGHT) {
        return;
    }
    if (ST(ctx)->join_open || ST(ctx)->night_drag >= 0) {
        return;
    }
    lv_indev_wait_release(indev);
    st_back(ctx);
}

static void set_scrolling(clock_ui_context_t *ctx, lv_obj_t *scroll, bool scrolling)
{
    ST(ctx)->scrolling = scrolling;
    ST(ctx)->wifi_scrolling = scrolling && scroll == ST(ctx)->pages[UI_ST_WIFI].scroll;
    ST(ctx)->night_scrolling = scrolling && scroll == ST(ctx)->pages[UI_ST_NIGHT].scroll;
    ST(ctx)->other_scrolling = scrolling && scroll == ST(ctx)->pages[UI_ST_OTHER].scroll;
}

static void scroll_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);
    lv_obj_t *scroll = lv_event_get_current_target(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_SCROLL_BEGIN) {
        set_scrolling(ctx, scroll, true);
    } else if (code == LV_EVENT_SCROLL_END) {
        set_scrolling(ctx, scroll, false);
    }
}

/* Instrument and Atelier: the title collapses to a legend at the top once it has scrolled
 * away. One flag change when the threshold is crossed. */
static void legend_scroll_cb(lv_event_t *e)
{
    lv_obj_t *scroll = lv_event_get_current_target(e);
    lv_obj_t *legend = lv_event_get_user_data(e);

    st_show(legend, lv_obj_get_scroll_y(scroll) > 96);
}

/* Atelier's minute rim leaves out the ticks under the register (clock degrees from 12). */
#define REGISTER_RIM_SKIP_FROM 54
#define REGISTER_RIM_SKIP_TO 126

/* Scroll register on the right rim, drawn after the page (Instrument and Atelier). It reads
 * the scroll position when the screen is drawn, so it costs nothing extra while scrolling.
 * Drawn as cached rings: a live arc costs the device several ms per arc, every frame. On
 * Atelier pages it draws the minute rim too, with a gap for the register while the page
 * scrolls: painting the ticks out again cost about 1 ms a frame. */
static void register_draw_cb(lv_event_t *e)
{
    kit_page_t *page = lv_event_get_user_data(e);
    const ui_skin_t *sk = ui_skin();
    lv_layer_t *layer = lv_event_get_layer(e);
    int32_t y = lv_obj_get_scroll_y(page->scroll);
    int32_t rest = lv_obj_get_scroll_bottom(page->scroll);
    int32_t total = y + rest;
    int32_t a0;
    int32_t a1;
    int32_t span;
    int32_t len;
    int32_t pos;
    int32_t radius;

    if (st_atelier()) {
        kit_draw_minute_rim(layer, total > 8 ? REGISTER_RIM_SKIP_FROM : -1, REGISTER_RIM_SKIP_TO);
    }
    if (total <= 8) {
        return;
    }
    y = LV_CLAMP(0, y, total);
    if (st_atelier()) {
        a0 = -30;
        a1 = 30;
        radius = 342;
    } else {
        a0 = -22;
        a1 = 22;
        radius = 346;
    }
    span = a1 - a0;
    len = LV_MAX(8, span * KIT_SCREEN / (KIT_SCREEN + total));
    pos = a0 + (span - len) * y / total;

    kit_draw_ring(layer, KIT_CENTER, KIT_CENTER, radius, 4, a0, a1, sk->line, LV_OPA_COVER);
    /* The thumb moves with every scroll step: a live arc, as the ring cache would build (and
     * keep) a new image for each position. kit_draw_arc bounds it to its own area; a plain
     * lv_draw_arc covers the whole circle and holds up the rest of the frame. */
    {
        lv_draw_arc_dsc_t arc;

        lv_draw_arc_dsc_init(&arc);
        arc.center = (lv_point_t) {KIT_CENTER, KIT_CENTER};
        arc.radius = (uint16_t)(radius + 2); /* the ring's radius is its centre line */
        arc.width = 4;
        arc.start_angle = (lv_value_precise_t)((pos % 360 + 360) % 360);
        arc.end_angle = (lv_value_precise_t)(((pos + len) % 360 + 360) % 360);
        arc.rounded = 1;
        arc.color = st_atelier() ? sk->accent : sk->text_3;
        arc.opa = LV_OPA_COVER;
        kit_draw_arc(layer, &arc);
    }
}

void st_back_event_cb(lv_event_t *e)
{
    st_back((clock_ui_context_t *)lv_event_get_user_data(e));
}

kit_page_t *st_page_begin(clock_ui_context_t *ctx, ui_st_page_t id, st_text_t eyebrow, st_text_t title, kit_nav_t nav)
{
    kit_page_t *p = st_page(ctx, id);

    kit_page_create(p, ui_chrome_root(ctx), st_text_set(eyebrow) ? st_text(ctx, eyebrow) : NULL, st_text(ctx, title),
                    nav, st_back_event_cb, ctx);
    st_bind(ctx, p->title, title);
    st_bind(ctx, p->eyebrow, eyebrow);
    ST(ctx)->built[id] = true;

    /* A swipe to the right goes back (it stops at the page root). */
    lv_obj_remove_flag(p->root, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(p->root, gesture_cb, LV_EVENT_GESTURE, ctx);
    lv_obj_add_event_cb(p->scroll, scroll_cb, LV_EVENT_SCROLL_BEGIN, ctx);
    lv_obj_add_event_cb(p->scroll, scroll_cb, LV_EVENT_SCROLL_END, ctx);

    if (!st_orbit() && id != UI_ST_HOME) {
        lv_obj_t *legend = kit_label(p->root, KIT_TEXT_LABEL, st_text(ctx, title));

        st_bind(ctx, legend, title);
        lv_obj_set_width(legend, 300);
        lv_label_set_long_mode(legend, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_style_text_align(legend, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(legend, LV_ALIGN_TOP_MID, 0, st_atelier() ? 40 : 52);
        if (st_atelier()) {
            lv_obj_set_style_text_color(legend, ui_skin()->value, 0);
        }
        lv_obj_add_flag(legend, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_event_cb(p->scroll, legend_scroll_cb, LV_EVENT_SCROLL, legend);
        ST(ctx)->legend[id] = legend;
        if (!(st_atelier() && id == UI_ST_NIGHT)) {
            lv_obj_add_event_cb(p->root, register_draw_cb, LV_EVENT_DRAW_POST, p);
            kit_page_set_rim(p, false); /* the register draws it */
        }
    }
    return p;
}

void st_page_set_eyebrow_text(clock_ui_context_t *ctx, ui_st_page_t id, const char *text)
{
    kit_page_t *p = st_page(ctx, id);

    if (ST(ctx)->built[id] && p->eyebrow != NULL) {
        kit_label_set_text_if_changed(p->eyebrow, text);
    }
}

/* "Settings · 02" eyebrows of the Instrument pages. */
static void sync_eyebrows(clock_ui_context_t *ctx)
{
    static const ui_st_page_t pages[3] = {UI_ST_WIFI, UI_ST_NIGHT, UI_ST_OTHER};
    char text[48];

    if (!st_instrument()) {
        return;
    }
    for (int i = 0; i < 3; i++) {
        snprintf(text, sizeof(text), ui_l10n_str(ctx, &s_settings_n), ui_l10n_text(ctx, L10N_SETTINGS), i + 1);
        st_page_set_eyebrow_text(ctx, pages[i], text);
    }
}

/* --- Face thumbnails ------------------------------------------------------------------- */

void st_render_face_thumb(clock_ui_context_t *ctx, lv_obj_t *image, lv_draw_buf_t **buf, int32_t size,
                          clock_face_id_t face)
{
    clock_ui_settings_state_t *st = ST(ctx);
    const void *src;
#if CONFIG_TOUCH_CLOCK_PERF_TOUR
    int64_t perf_t0 = esp_timer_get_time();
#endif

    if (image == NULL || buf == NULL) {
        return;
    }
    if (st->thumb_shell == NULL) {
        /* A round clipping shell far off screen: it is only drawn into snapshots, so its
         * clip_corner layer is paid once per thumbnail, not per frame. */
        st->thumb_shell = plain(ui_chrome_root(ctx));
        lv_obj_set_pos(st->thumb_shell, -2000, -2000);
        lv_obj_set_style_radius(st->thumb_shell, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_clip_corner(st->thumb_shell, true, 0);
        lv_obj_set_style_bg_opa(st->thumb_shell, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(st->thumb_shell, lv_color_black(), 0);
        st->thumb_image = lv_image_create(st->thumb_shell);
        lv_obj_remove_flag(st->thumb_image, LV_OBJ_FLAG_CLICKABLE);
    }
    src = ui_face_preview_source(ctx, face);
    if (src == NULL) {
        return;
    }
    lv_obj_set_size(st->thumb_shell, size, size);
    lv_obj_set_size(st->thumb_image, size, size);
    lv_image_set_src(st->thumb_image, src);
    lv_image_set_inner_align(st->thumb_image, LV_IMAGE_ALIGN_COVER);
    lv_obj_update_layout(st->thumb_shell);

    if (*buf == NULL || lv_snapshot_reshape_draw_buf(st->thumb_shell, *buf) != LV_RESULT_OK) {
        if (*buf != NULL) {
            lv_draw_buf_destroy(*buf);
        }
        *buf = lv_snapshot_create_draw_buf(st->thumb_shell, LV_COLOR_FORMAT_ARGB8888);
        if (*buf == NULL) {
            return;
        }
    }
    if (lv_snapshot_take_to_draw_buf(st->thumb_shell, LV_COLOR_FORMAT_ARGB8888, *buf) != LV_RESULT_OK) {
        return;
    }
    if (lv_image_get_src(image) != *buf) {
        lv_image_set_src(image, *buf);
    } else {
        lv_obj_invalidate(image);
    }
#if CONFIG_TOUCH_CLOCK_PERF_TOUR
    printf("PERF face_thumb face=%d us=%lld\n", (int)face, esp_timer_get_time() - perf_t0);
#endif
}

/* --- Home ------------------------------------------------------------------------------ */

static void home_wifi_cb(lv_event_t *e)
{
    st_show_page(lv_event_get_user_data(e), UI_ST_WIFI);
}

static void home_night_cb(lv_event_t *e)
{
    st_show_page(lv_event_get_user_data(e), UI_ST_NIGHT);
}

static void home_other_cb(lv_event_t *e)
{
    st_show_page(lv_event_get_user_data(e), UI_ST_OTHER);
}

static void home_alarms_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);

    ui_settings_close_all(ctx);
    ui_alarms_open(ctx);
}

/* Atelier: the ruby update chip that sits after the Other title. */
static lv_obj_t *ruby_chip(lv_obj_t *parent, lv_obj_t **label)
{
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *chip = plain(parent);

    lv_obj_set_size(chip, LV_SIZE_CONTENT, 30);
    lv_obj_set_style_radius(chip, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(chip, 1, 0);
    lv_obj_set_style_border_color(chip, sk->danger, 0);
    lv_obj_set_style_pad_hor(chip, 12, 0);
    lv_obj_set_flex_flow(chip, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(chip, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(chip, 8, 0);
    lv_obj_add_flag(chip, LV_OBJ_FLAG_EVENT_BUBBLE);
    st_dot(chip, 8, sk->danger);
    *label = kit_label(chip, KIT_TEXT_LABEL, "");
    lv_obj_set_style_text_color(*label, sk->value, 0);
    lv_obj_set_style_text_letter_space(*label, 2, 0);
    return chip;
}

static void build_home(clock_ui_context_t *ctx)
{
    static const struct {
        kit_icon_t icon;
        l10n_text_id_t title;
        l10n_text_id_t subtitle;
        lv_event_cb_t cb;
    } entries[3] = {
        {KIT_ICON_WIFI, L10N_WIFI, L10N_WIFI_SUBTITLE, home_wifi_cb},
        {KIT_ICON_MOON, L10N_NIGHT_MODE, L10N_NIGHT_SUBTITLE, home_night_cb},
        {KIT_ICON_SETTINGS, L10N_OTHER, L10N_OTHER_SUBTITLE, home_other_cb},
    };
    const ui_skin_t *sk = ui_skin();
    const lv_color_t tints[3] = {sk->tint_wifi, sk->tint_night, sk->tint_other};
    clock_ui_settings_state_t *st = ST(ctx);
    kit_page_t *p;
    lv_obj_t *group;
    lv_obj_t *box;
    kit_row_t row;

    p = st_page_begin(ctx, UI_ST_HOME, st_atelier() ? ST_NONE : ST_NONE, ST_ID(L10N_SETTINGS),
                      st_atelier() ? KIT_NAV_BACK : KIT_NAV_CLOSE);
    if (st_instrument()) {
        kit_page_set_eyebrow(p, " ");
    } else if (st_atelier()) {
        kit_page_set_eyebrow(p, "Touch Clock");
        kit_page_set_action(p, KIT_ICON_BELL, home_alarms_cb, ctx);
    }

    if (st_instrument()) {
        /* Status readout in a recessed window. */
        box = plain(p->content);
        lv_obj_set_size(box, 432, 44);
        lv_obj_set_style_align(box, LV_ALIGN_TOP_MID, 0);
        lv_obj_set_style_radius(box, 22, 0);
        lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(box, sk->well, 0);
        lv_obj_set_style_border_width(box, 1, 0);
        lv_obj_set_style_border_color(box, sk->line, 0);
        lv_obj_set_style_pad_hor(box, 20, 0);
        lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(box, 12, 0);
        lv_obj_set_style_margin_bottom(box, 24, 0);
        lv_obj_set_style_margin_left(box, (sk->content_w - 432) / 2, 0);
        st->home_led = st_dot(box, 8, sk->accent);
        st->home_status = kit_label(box, KIT_TEXT_CAPTION, "WI-FI");
        lv_obj_set_style_text_color(st->home_status, sk->text_3, 0);
        st->home_status_2 = kit_label(box, KIT_TEXT_CAPTION, "");
        lv_obj_set_style_text_color(st->home_status_2, sk->text_2, 0);
        lv_label_set_long_mode(st->home_status_2, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_style_max_width(st->home_status_2, 300, 0);
    }

    if (st_orbit()) {
        /* Three hero platters fitted to the circle: the middle one is the widest. */
        group = kit_group(p->scroll);
        lv_obj_set_y(group, 180);
        lv_obj_set_flex_align(group, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(group, 12, 0);
    } else {
        if (st_atelier()) {
            kit_spacer(p->content, 20);
        }
        group = kit_group(p->content);
    }
    for (int i = 0; i < 3; i++) {
        kit_row_opts_t o = {0};

        o.icon = entries[i].icon;
        o.tint = tints[i];
        o.trail = KIT_TRAIL_CHEVRON;
        o.on_click = entries[i].cb;
        o.user_data = ctx;
        st_row(ctx, &row, group, &o, ST_ID(entries[i].title), ST_ID(entries[i].subtitle));
        if (st_orbit()) {
            lv_obj_set_width(row.row, i == 1 ? 600 : 550);
            lv_obj_set_style_radius(row.row, sk->radius_m, 0);
            lv_obj_set_style_pad_ver(row.row, 14, 0);
            lv_obj_set_style_pad_left(row.row, 22, 0);
            lv_obj_set_style_pad_column(row.row, 20, 0);
        }
        if (st_atelier()) {
            lv_obj_set_style_pad_ver(row.row, 12, 0);
        }
        if (i == 2 && st_instrument()) {
            st->home_update = st_dot(row.row, 8, sk->accent);
            lv_obj_move_to_index(st->home_update, (int32_t)lv_obj_get_child_count(row.row) - 2);
            lv_obj_set_style_margin_right(st->home_update, 4, 0);
        } else if (i == 2 && st_atelier()) {
            lv_obj_t *col = lv_obj_get_parent(row.title);
            lv_obj_t *line = st_hbox(col, 12);

            lv_obj_move_to_index(line, 0);
            lv_obj_set_width(line, lv_pct(100));
            lv_obj_set_parent(row.title, line);
            lv_obj_set_width(row.title, LV_SIZE_CONTENT);
            st->home_update = ruby_chip(line, &st->home_update_label);
        }
    }

    if (st_instrument()) {
        box = st_hbox(p->content, 0);
        lv_obj_set_width(box, lv_pct(100));
        lv_obj_set_style_pad_top(box, 26, 0);
        lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        st->home_footer = kit_label(box, KIT_TEXT_LABEL, "");
        st->home_footer_2 = kit_label(box, KIT_TEXT_LABEL, "");
        lv_obj_set_style_text_color(st->home_footer_2, sk->accent, 0);
    } else {
        lv_obj_t *parent = p->content;

        if (st_orbit()) {
            parent = plain(p->scroll);
            lv_obj_set_size(parent, KIT_SCREEN, LV_SIZE_CONTENT);
            lv_obj_set_y(parent, 578);
            lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
            lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        } else {
            kit_spacer(parent, 28);
            kit_divider(parent);
        }
        box = st_hbox(parent, 10);
        if (st_atelier()) {
            lv_obj_set_width(box, lv_pct(100));
            lv_obj_set_style_pad_top(box, 18, 0);
            lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            st->home_led = st_signal(box, 22);
        } else {
            st->home_led = kit_icon_small(box, KIT_ICON_WIFI, sk->accent);
        }
        st->home_status = kit_label(box, st_orbit() ? KIT_TEXT_VALUE : KIT_TEXT_BODY, "");
        if (st_orbit()) {
            lv_obj_set_style_text_color(st->home_status, sk->text, 0);
        }
        st->home_status_2 = kit_label(box, KIT_TEXT_BODY, "");
        lv_label_set_long_mode(st->home_status, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_style_max_width(st->home_status, 260, 0);
    }
    /* Home does not scroll. */
    lv_obj_remove_flag(p->scroll, LV_OBJ_FLAG_SCROLLABLE);
}

static void sync_home(clock_ui_context_t *ctx)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_settings_state_t *st = ST(ctx);
    const app_runtime_state_t *rt = ctx->runtime;
    bool connected = rt->wifi_connected && ctx->settings->wifi.ssid[0] != '\0';
    bool update = rt->ota_update_available && rt->ota_available_version[0] != '\0';
    char net[64];
    char sync[64];
    char text[96];

    if (!st->built[UI_ST_HOME]) {
        return;
    }
    if (connected) {
        snprintf(net, sizeof(net), "%s", ctx->settings->wifi.ssid);
        if (rt->time_synced || ctx->settings->last_synced_epoch > 0) {
            st_format_synced(ctx, sync, sizeof(sync), &s_synced_at);
        } else {
            snprintf(sync, sizeof(sync), "%s", ui_l10n_str(ctx, &s_not_synced));
        }
    } else if (ctx->settings->wifi.ssid[0] != '\0') {
        snprintf(net, sizeof(net), "%s", ctx->settings->wifi.ssid);
        if (rt->wifi_status[0] != '\0') {
            l10n_runtime_status(ctx->settings->language, rt->wifi_status, sync, sizeof(sync));
        } else {
            snprintf(sync, sizeof(sync), "%s", ui_l10n_str(ctx, &s_not_connected));
        }
    } else {
        net[0] = '\0';
        snprintf(sync, sizeof(sync), "%s", ui_l10n_str(ctx, &s_not_connected));
    }

    if (st_instrument()) {
        struct tm tm;
        time_t now = time(NULL);
        char month[8] = "";
        const char *months = ui_l10n_str(ctx, &s_months);
        int index = 0;

        snprintf(text, sizeof(text), "%s%s%s", net, net[0] != '\0' ? " · " : "", sync);
        kit_label_set_text_if_changed(st->home_status_2, text);
        st_bg_color(st->home_led, connected ? sk->accent : sk->line_strong);
        localtime_r(&now, &tm);
        for (const char *m = months; *m != '\0' && index <= tm.tm_mon;) {
            const char *end = strchr(m, ' ');
            size_t len = end != NULL ? (size_t)(end - m) : strlen(m);

            if (index == tm.tm_mon) {
                snprintf(month, sizeof(month), "%.*s", (int)len, m);
                break;
            }
            index++;
            m += len + (end != NULL ? 1 : 0);
        }
        snprintf(text, sizeof(text), "%02d:%02d · %s %d %s", tm.tm_hour, tm.tm_min,
                 l10n_day_short((clock_language_t)ctx->settings->language, tm.tm_wday), tm.tm_mday, month);
        kit_page_set_eyebrow(&st->pages[UI_ST_HOME], text);
        snprintf(text, sizeof(text), "v%s%s", TOUCH_CLOCK_FIRMWARE_VERSION, update ? " · " : "");
        kit_label_set_text_if_changed(st->home_footer, text);
        if (update) {
            snprintf(text, sizeof(text), ui_l10n_str(ctx, &s_update_ready), rt->ota_available_version);
        } else {
            text[0] = '\0';
        }
        kit_label_set_text_if_changed(st->home_footer_2, text);
        st_show(st->home_update, update);
        return;
    }

    if (st_atelier()) {
        snprintf(text, sizeof(text), "%s%s%s", ui_l10n_text(ctx, L10N_WIFI), net[0] != '\0' ? ": " : "", net);
        kit_label_set_text_if_changed(st->home_status, text);
        snprintf(text, sizeof(text), "· %s", sync);
        kit_label_set_text_if_changed(st->home_status_2, text);
        st_signal_set(st->home_led, connected ? st_signal_level(rt->wifi_rssi) : 0, false);
        st_show(st->home_update, update);
        if (update) {
            kit_label_set_text_if_changed(st->home_update_label, rt->ota_available_version);
        }
        return;
    }

    kit_label_set_text_if_changed(st->home_status, net);
    snprintf(text, sizeof(text), "%s%s", net[0] != '\0' ? "· " : "", sync);
    kit_label_set_text_if_changed(st->home_status_2, text);
    st_recolor(st->home_led, connected ? sk->accent : sk->text_3);
}

/* --- Navigation ------------------------------------------------------------------------ */

static void build_page(clock_ui_context_t *ctx, ui_st_page_t id)
{
    switch (id) {
    case UI_ST_HOME:
        build_home(ctx);
        break;
    case UI_ST_WIFI:
        st_wifi_build(ctx);
        break;
    case UI_ST_NIGHT:
        st_night_build(ctx);
        break;
    case UI_ST_OTHER:
        st_other_build(ctx);
        break;
    case UI_ST_TIMEZONE:
        st_timezone_build(ctx);
        break;
    case UI_ST_MANUAL_TIME:
        st_manual_build(ctx);
        break;
    case UI_ST_SCHEDULE:
        st_schedule_build(ctx);
        break;
    case UI_ST_FACE:
        st_face_build(ctx);
        break;
    case UI_ST_NIGHT_BRIGHT:
        st_night_bright_build(ctx);
        break;
    case UI_ST_SUNRISE:
        st_sunrise_build(ctx);
        break;
    case UI_ST_LANGUAGE:
        st_language_build(ctx);
        break;
    case UI_ST_STYLE:
        st_style_build(ctx);
        break;
    default:
        break;
    }
    if (id == UI_ST_WIFI || id == UI_ST_NIGHT || id == UI_ST_OTHER) {
        sync_eyebrows(ctx);
    }
}

static void sync_page(clock_ui_context_t *ctx, ui_st_page_t id, bool force)
{
    switch (id) {
    case UI_ST_HOME:
        sync_home(ctx);
        break;
    case UI_ST_WIFI:
        st_wifi_sync(ctx, force);
        break;
    case UI_ST_NIGHT:
        st_night_sync(ctx, force);
        break;
    case UI_ST_OTHER:
        st_other_sync(ctx, force);
        break;
    case UI_ST_TIMEZONE:
        st_timezone_sync(ctx);
        break;
    case UI_ST_MANUAL_TIME:
        st_manual_sync(ctx, force);
        break;
    case UI_ST_SCHEDULE:
        st_schedule_sync(ctx, force);
        break;
    case UI_ST_FACE:
        st_face_sync(ctx, force);
        break;
    case UI_ST_NIGHT_BRIGHT:
        st_night_bright_sync(ctx);
        break;
    case UI_ST_SUNRISE:
        st_sunrise_sync(ctx);
        break;
    case UI_ST_LANGUAGE:
        st_language_sync(ctx);
        break;
    case UI_ST_STYLE:
        st_style_sync(ctx);
        break;
    default:
        break;
    }
}

static void present(clock_ui_context_t *ctx, ui_st_page_t id, bool reset_scroll)
{
    clock_ui_settings_state_t *st = ST(ctx);
    bool from_face = true;

    for (int i = 0; i < UI_ST_PAGE_COUNT; i++) {
        if (st->built[i] && !lv_obj_has_flag(st->pages[i].root, LV_OBJ_FLAG_HIDDEN)) {
            from_face = false; /* another page was up */
        }
    }

    if (!st->built[id]) {
        build_page(ctx, id);
    }
    if (st->join_open) {
        st_join_close(ctx);
    }
    st->page = (uint8_t)id;
    st->scrolling = false;
    set_scrolling(ctx, NULL, false);
    sync_page(ctx, id, true);
    for (int i = 0; i < UI_ST_PAGE_COUNT; i++) {
        if (st->built[i] && i != (int)id) {
            st_show(st->pages[i].root, false);
        }
    }
    if (reset_scroll) {
        lv_obj_scroll_to_y(st->pages[id].scroll, 0, LV_ANIM_OFF);
        st_show(st->legend[id], false);
    }
    st_show(st->pages[id].root, true);
    lv_obj_move_foreground(st->pages[id].root);
    kit_page_glass_enter(&st->pages[id], from_face);
}

void st_show_page(clock_ui_context_t *ctx, ui_st_page_t id)
{
    present(ctx, id, true);
}

static void open_surface(clock_ui_context_t *ctx)
{
    if (ST(ctx)->open) {
        return;
    }
    ui_face_chrome_close_all(ctx);
    ui_alarms_close_all(ctx);
    ST(ctx)->open = true;
    set_root_ui_hidden(ctx, true);
}

static void close_surface(clock_ui_context_t *ctx)
{
    clock_ui_settings_state_t *st = ST(ctx);

    if (!st->open) {
        return;
    }
    if (st->join_open) {
        st_join_close(ctx);
    }
    st->open = false;
    st->night_drag = -1;
    set_scrolling(ctx, NULL, false);
    for (int i = 0; i < UI_ST_PAGE_COUNT; i++) {
        if (st->built[i]) {
            st_show(st->pages[i].root, false);
        }
    }
    /* The face pickers' thumbnails draw the hand-drawn faces into the view buffer they share,
     * and the face isn't drawn while covered: without this, the last face a picker drew showed
     * until the next tick. */
    update_face(ctx, tile_to_face(ctx, lv_tileview_get_tile_active(ctx->tileview)));
    set_root_ui_hidden(ctx, false);
    ui_face_chrome_show_controls(ctx);
}

void st_back(clock_ui_context_t *ctx)
{
    clock_ui_settings_state_t *st = ST(ctx);

    if (!st->open) {
        return;
    }
    if (st->join_open) {
        st_join_close(ctx);
        return;
    }
    if (st->page == UI_ST_HOME) {
        close_surface(ctx);
        return;
    }
    present(ctx, st_parent((ui_st_page_t)st->page), false);
}

/* --- Hooks ----------------------------------------------------------------------------- */

static void debug_open_settings(clock_ui_context_t *ctx)
{
    ui_settings_open_page(ctx, UI_SETTINGS_PAGE_HOME);
}

static void debug_open_wifi(clock_ui_context_t *ctx)
{
    ui_settings_open_page(ctx, UI_SETTINGS_PAGE_WIFI);
}

static void debug_open_night(clock_ui_context_t *ctx)
{
    ui_settings_open_page(ctx, UI_SETTINGS_PAGE_NIGHT);
}

static void debug_open_other(clock_ui_context_t *ctx)
{
    ui_settings_open_page(ctx, UI_SETTINGS_PAGE_OTHER);
}

static void debug_sub(clock_ui_context_t *ctx, ui_settings_page_t top, ui_st_page_t sub)
{
    ui_settings_open_page(ctx, top);
    st_show_page(ctx, sub);
}

static void debug_timezone(clock_ui_context_t *ctx)
{
    ui_settings_open_page(ctx, UI_SETTINGS_PAGE_WIFI);
    st_timezone_open(ctx);
}

static void debug_manual_time(clock_ui_context_t *ctx)
{
    debug_sub(ctx, UI_SETTINGS_PAGE_WIFI, UI_ST_MANUAL_TIME);
}

static void debug_schedule(clock_ui_context_t *ctx)
{
    ui_settings_open_page(ctx, UI_SETTINGS_PAGE_NIGHT);
    st_schedule_open(ctx, 0);
}

static void debug_face_picker(clock_ui_context_t *ctx)
{
    debug_sub(ctx, UI_SETTINGS_PAGE_NIGHT, UI_ST_FACE);
}

static void debug_night_brightness(clock_ui_context_t *ctx)
{
    debug_sub(ctx, UI_SETTINGS_PAGE_NIGHT, UI_ST_NIGHT_BRIGHT);
}

static void debug_sunrise(clock_ui_context_t *ctx)
{
    debug_sub(ctx, UI_SETTINGS_PAGE_NIGHT, UI_ST_SUNRISE);
}

static void debug_language(clock_ui_context_t *ctx)
{
    debug_sub(ctx, UI_SETTINGS_PAGE_OTHER, UI_ST_LANGUAGE);
}

static void debug_style(clock_ui_context_t *ctx)
{
    debug_sub(ctx, UI_SETTINGS_PAGE_OTHER, UI_ST_STYLE);
}

static void debug_join(clock_ui_context_t *ctx)
{
    const app_runtime_state_t *rt = ctx->runtime;
    size_t count = LV_MIN(rt->wifi_scan_count, (size_t)CLOCK_WIFI_SCAN_RESULT_MAX);
    size_t pick = 0;

    ui_settings_open_page(ctx, UI_SETTINGS_PAGE_WIFI);
    /* Prefer a network that is not the saved one ("Café Lumière" in the simulator). */
    for (size_t i = 0; i < count; i++) {
        if (strcmp(rt->wifi_scan_results[i].ssid, ctx->settings->wifi.ssid) != 0) {
            pick = i;
            if (i >= 3) {
                break;
            }
        }
    }
    if (count > 0) {
        st_join_open(ctx, rt->wifi_scan_results[pick].ssid, rt->wifi_scan_results[pick].rssi,
                     rt->wifi_scan_results[pick].authmode);
    } else {
        st_join_open(ctx, "Café Lumière", -71, 3);
    }
}

static void debug_join_type(clock_ui_context_t *ctx)
{
    if (ST(ctx)->join_open && ST(ctx)->join_field != NULL) {
        lv_textarea_set_text(ST(ctx)->join_field, "lumiere2026");
    }
}

static void debug_about(clock_ui_context_t *ctx)
{
    lv_obj_t *anchor;
    kit_page_t *p;

    ui_settings_open_page(ctx, UI_SETTINGS_PAGE_OTHER);
    anchor = st_other_about_anchor(ctx);
    p = st_page(ctx, UI_ST_OTHER);
    if (anchor != NULL) {
        lv_area_t a;
        lv_area_t s;

        lv_obj_update_layout(p->scroll);
        lv_obj_get_coords(anchor, &a);
        lv_obj_get_coords(p->scroll, &s);
        lv_obj_scroll_by_bounded(p->scroll, 0, -(a.y1 - s.y1 - 120), LV_ANIM_OFF);
    }
}

static void debug_scroll(clock_ui_context_t *ctx)
{
    if (ST(ctx)->open) {
        lv_obj_scroll_by_bounded(ST(ctx)->pages[ST(ctx)->page].scroll, 0, -300, LV_ANIM_OFF);
    }
}

static void debug_scroll_end(clock_ui_context_t *ctx)
{
    if (ST(ctx)->open) {
        lv_obj_t *scroll = ST(ctx)->pages[ST(ctx)->page].scroll;

        lv_obj_update_layout(scroll);
        lv_obj_scroll_by_bounded(scroll, 0, -lv_obj_get_scroll_bottom(scroll), LV_ANIM_OFF);
    }
}

/* Liquid Glass: while a page sits still, bakes ahead what comes next: the rows of this page
 * below the fold (where they come into view at the end of its scroll) and the other pages
 * already built. Within one budget a tick, as the face's idle prewarm. */
static void prewarm_timer_cb(lv_timer_t *timer)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_timer_get_user_data(timer);
    clock_ui_settings_state_t *st = ST(ctx);
    uint32_t start = lv_tick_get();

    if (!ui_skin_is_glass() || !ui_settings_is_open(ctx) || lv_anim_count_running() > 0 ||
        lv_display_get_inactive_time(NULL) < 400) {
        return;
    }
    for (int i = 0; i < UI_ST_PAGE_COUNT; i++) {
        kit_page_t *p = &st->pages[i];
        uint32_t spent = lv_tick_elaps(start);
        bool done;

        if (!st->built[i] || p->root == NULL) {
            continue;
        }
        if (spent >= 20) {
            return;
        }
        if (lv_obj_has_flag(p->root, LV_OBJ_FLAG_HIDDEN)) {
            done = ui_glass_prewarm(p->root, 20 - spent);
        } else {
            done = lv_obj_get_scroll_bottom(p->scroll) <= 0 ||
                   ui_glass_prewarm_at(p->scroll, -lv_obj_get_scroll_bottom(p->scroll), 20 - spent);
        }
        if (!done) {
            return;
        }
    }
}

void ui_settings_create(clock_ui_context_t *ctx)
{
    clock_ui_settings_state_t *st = ST(ctx);

    st->night_drag = -1;
    st->last_minute = -1;
    st->applied_language = (clock_language_t)ctx->settings->language;
    if (st_atelier()) {
        kit_minute_rim_prewarm(REGISTER_RIM_SKIP_FROM, REGISTER_RIM_SKIP_TO);
    }
    if (ui_skin_is_glass()) {
        ui_chrome_timer_create(ctx, prewarm_timer_cb, 250, ctx);
    }
    ui_debug_register("open_settings", debug_open_settings);
    ui_debug_register("open_wifi", debug_open_wifi);
    ui_debug_register("open_night", debug_open_night);
    ui_debug_register("open_other", debug_open_other);
    ui_debug_register("close_settings", ui_settings_close_all);
    ui_debug_register("settings_back", st_back);
    ui_debug_register("settings_scroll", debug_scroll);
    ui_debug_register("settings_scroll_end", debug_scroll_end);
    ui_debug_register("join_dialog", debug_join);
    ui_debug_register("join_type", debug_join_type);
    ui_debug_register("timezone_picker", debug_timezone);
    ui_debug_register("manual_time", debug_manual_time);
    ui_debug_register("night_schedule", debug_schedule);
    ui_debug_register("night_face_picker", debug_face_picker);
    ui_debug_register("night_brightness", debug_night_brightness);
    ui_debug_register("sunrise_options", debug_sunrise);
    ui_debug_register("language_picker", debug_language);
    ui_debug_register("style_picker", debug_style);
    ui_debug_register("about", debug_about);
    st_night_prewarm();
}

void ui_settings_destroy(clock_ui_context_t *ctx)
{
    st_night_destroy(ctx);
}

static void apply_language(clock_ui_context_t *ctx)
{
    clock_ui_settings_state_t *st = ST(ctx);

    if (ctx->cached_language != (clock_language_t)ctx->settings->language) {
        /* Relabels every bound label of every area, as before the redesign. */
        ui_l10n_apply(ctx);
        ctx->cached_language = (clock_language_t)ctx->settings->language;
    }
    if (st->applied_language != (clock_language_t)ctx->settings->language) {
        st->applied_language = (clock_language_t)ctx->settings->language;
        sync_eyebrows(ctx);
        /* Composed texts of every built page follow on the next sync of that page. */
        for (int i = 0; i < UI_ST_PAGE_COUNT; i++) {
            if (st->built[i]) {
                sync_page(ctx, (ui_st_page_t)i, true);
            }
        }
    }
}

void ui_settings_tick(clock_ui_context_t *ctx, time_t now)
{
    clock_ui_settings_state_t *st = ST(ctx);
    int32_t minute = (int32_t)(now / 60);

    if (!st->open || st->scrolling || st->night_drag >= 0) {
        return;
    }
    if (minute != st->last_minute) {
        st->last_minute = minute;
        if (st->page == UI_ST_HOME) {
            sync_home(ctx);
        }
    }
    sync_page(ctx, (ui_st_page_t)st->page, false);
}

void ui_settings_refresh(clock_ui_context_t *ctx)
{
    apply_language(ctx);
    if (ST(ctx)->open) {
        sync_page(ctx, (ui_st_page_t)ST(ctx)->page, false);
    }
}

void ui_settings_open(clock_ui_context_t *ctx)
{
    ui_settings_open_page(ctx, UI_SETTINGS_PAGE_HOME);
}

void ui_settings_open_page(clock_ui_context_t *ctx, ui_settings_page_t page)
{
    static const ui_st_page_t map[] = {
        [UI_SETTINGS_PAGE_NONE] = UI_ST_HOME,
        [UI_SETTINGS_PAGE_HOME] = UI_ST_HOME,
        [UI_SETTINGS_PAGE_WIFI] = UI_ST_WIFI,
        [UI_SETTINGS_PAGE_NIGHT] = UI_ST_NIGHT,
        [UI_SETTINGS_PAGE_OTHER] = UI_ST_OTHER,
    };

    if ((unsigned)page > UI_SETTINGS_PAGE_OTHER) {
        page = UI_SETTINGS_PAGE_HOME;
    }
    face_glass_capture(ctx, 1 /* UI_GLASS_PAGE */);
    open_surface(ctx);
    if (!ST(ctx)->built[UI_ST_HOME]) {
        build_page(ctx, UI_ST_HOME);
    }
    present(ctx, map[page], true);
}

ui_settings_page_t ui_settings_current_page(const clock_ui_context_t *ctx)
{
    const clock_ui_settings_state_t *st = &ctx->settings_ui;
    ui_st_page_t page;

    if (!st->open) {
        return UI_SETTINGS_PAGE_NONE;
    }
    page = (ui_st_page_t)st->page;
    if (page >= UI_ST_TIMEZONE) {
        page = st_parent(page);
    }
    switch (page) {
    case UI_ST_WIFI:
        return UI_SETTINGS_PAGE_WIFI;
    case UI_ST_NIGHT:
        return UI_SETTINGS_PAGE_NIGHT;
    case UI_ST_OTHER:
        return UI_SETTINGS_PAGE_OTHER;
    default:
        return UI_SETTINGS_PAGE_HOME;
    }
}

bool ui_settings_is_open(const clock_ui_context_t *ctx)
{
    return ctx->settings_ui.open;
}

/* Pages are built when first opened, which made that open wait for the build and (in Liquid
 * Glass) for every bake on it: about 110 ms. Built ahead, the idle prewarm bakes them too. The
 * Wi-Fi page is left to its first open: showing it starts a scan. */
bool ui_settings_prebuild_step(clock_ui_context_t *ctx)
{
    static const ui_st_page_t pages[] = {UI_ST_HOME, UI_ST_NIGHT, UI_ST_OTHER};
    clock_ui_settings_state_t *st = ST(ctx);

    for (size_t i = 0; i < sizeof(pages) / sizeof(pages[0]); i++) {
        if (!st->built[pages[i]]) {
            build_page(ctx, pages[i]);
            /* Laid out as it opens: built unsynced, Night's first row was 51 px taller, and
             * the glass baked ahead at idle was all baked again on its first open. */
            sync_page(ctx, pages[i], true);
            return false;
        }
    }
    return true;
}

void ui_settings_close_all(clock_ui_context_t *ctx)
{
    close_surface(ctx);
}
