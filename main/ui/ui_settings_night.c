/*
 * Settings › Night mode: enabled, schedule (Instrument mini 24 h dial, Orbit time tiles,
 * Atelier 24 h rim ring with draggable jewels), night face with a preview, night brightness
 * and the sunrise ramp; the schedule editor, the night face picker and the Orbit brightness
 * page. While Night mode is off the hint shows and everything else is dimmed and inert.
 */
#include "ui/ui_settings_internal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "assets/ui_assets/settings_assets.h"

static const ui_l10n_str_t s_tap_to_choose = UI_L10N_STR("Tap to choose", "Dotknij, aby wybrać",
                                                         "Торкніться, щоб вибрати", "Нажмите, чтобы выбрать",
                                                         "Tik om te kiezen");
static const ui_l10n_str_t s_duration = UI_L10N_STR("%d h %02d min", "%d h %02d min", "%d год %02d хв",
                                                    "%d ч %02d мин", "%d u %02d min");

static const ui_l10n_str_t s_starts = UI_L10N_STR("Starts", "Początek", "Початок", "Начало", "Begint");
static const ui_l10n_str_t s_sunrise = UI_L10N_STR("Sunrise", "Wschód słońca", "Світанок", "Рассвет", "Zonsopkomst");
static const ui_l10n_str_t s_sunrise_value = UI_L10N_STR("%d min · %d%%", "%d min · %d%%", "%d хв · %d%%", "%d мин · %d%%",
                                                         "%d min · %d%%");
static const ui_l10n_str_t s_sunrise_length = UI_L10N_STR("Length", "Długość", "Тривалість", "Длительность", "Duur");
static const ui_l10n_str_t s_sunrise_minutes = UI_L10N_STR("%d minutes before the alarm", "%d minut przed alarmem",
                                                           "За %d хвилин до будильника", "За %d минут до будильника",
                                                           "%d minuten voor de wekker");
static const ui_l10n_str_t s_sunrise_peak = UI_L10N_STR("Brightest", "Najjaśniej", "Найяскравіше", "Ярче всего",
                                                        "Helderst");
static const ui_l10n_str_t s_sunrise_chime = UI_L10N_STR("Soft chime", "Cichy dzwonek", "Тихий дзвін", "Тихий звон",
                                                         "Zacht belletje");
static const ui_l10n_str_t s_sunrise_chime_sub = UI_L10N_STR("A minute before the alarm", "Minutę przed alarmem",
                                                             "За хвилину до будильника", "За минуту до будильника",
                                                             "Een minuut voor de wekker");
static const ui_l10n_str_t s_sunrise_preview = UI_L10N_STR("Watch it now", "Zobacz teraz", "Переглянути зараз",
                                                          "Посмотреть сейчас", "Nu bekijken");
static const ui_l10n_str_t s_sunrise_preview_sub = UI_L10N_STR("The whole sunrise in a minute", "Cały wschód w minutę",
                                                              "Увесь світанок за хвилину", "Весь рассвет за минуту",
                                                              "De hele zonsopkomst in een minuut");
static const ui_l10n_str_t s_sunrise_face = UI_L10N_STR("Wake-up face", "Tarcza na pobudkę", "Циферблат пробудження",
                                                       "Циферблат пробуждения", "Wekscherm");
static const ui_l10n_str_t s_sunrise_faces[4] = {
    UI_L10N_STR("Daybreak · Dawn", "Daybreak · Świt", "Daybreak · Зоря", "Daybreak · Заря", "Daybreak · Dageraad"),
    UI_L10N_STR("Daybreak · Rose", "Daybreak · Róż", "Daybreak · Троянда", "Daybreak · Роза", "Daybreak · Roos"),
    UI_L10N_STR("Daybreak · Mist", "Daybreak · Mgła", "Daybreak · Туман", "Daybreak · Туман", "Daybreak · Mist"),
    UI_L10N_STR("None, just a warm glow", "Brak, tylko poświata", "Немає, лише сяйво", "Нет, только свечение",
                "Geen, alleen een gloed"),
};
static const uint8_t s_sunrise_lengths[3] = {10, 20, 30};
static const uint8_t s_sunrise_peaks[3] = {40, 60, 80};
static const ui_l10n_str_t s_ends = UI_L10N_STR("Ends", "Koniec", "Кінець", "Конец", "Eindigt");

#define FACE_THUMB_INSTRUMENT 60
#define FACE_THUMB_ORBIT 96
#define FACE_THUMB_ATELIER 64
#define DIAL_SIZE 52
#define GAUGE_SIZE 96
#define RING_R 334
#define JEWEL 36

/* --- Schedule helpers ------------------------------------------------------------------ */

static uint16_t start_minutes(const clock_ui_context_t *ctx)
{
    const clock_ui_settings_state_t *st = &ctx->settings_ui;

    if (st->night_drag >= 0) {
        return st->night_drag_min[0];
    }
    return (uint16_t)(ctx->settings->night_mode.start_hour * 60 + ctx->settings->night_mode.start_minute);
}

static uint16_t end_minutes(const clock_ui_context_t *ctx)
{
    const clock_ui_settings_state_t *st = &ctx->settings_ui;

    if (st->night_drag >= 0) {
        return st->night_drag_min[1];
    }
    return (uint16_t)(ctx->settings->night_mode.end_hour * 60 + ctx->settings->night_mode.end_minute);
}

static void format_range(const clock_ui_context_t *ctx, char *out, size_t size)
{
    uint16_t s = start_minutes(ctx);
    uint16_t e = end_minutes(ctx);

    snprintf(out, size, "%02u:%02u \xE2\x86\x92 %02u:%02u", s / 60U, s % 60U, e / 60U, e % 60U);
}

static void format_duration(const clock_ui_context_t *ctx, char *out, size_t size)
{
    int d = ((int)end_minutes(ctx) - (int)start_minutes(ctx) + 1440) % 1440;

    snprintf(out, size, ui_l10n_str(ctx, &s_duration), d / 60, d % 60);
}

static float minutes_to_lv_angle(uint16_t minutes)
{
    /* 24 h around the dial, midnight at the top; LVGL angles start at 3 o'clock. */
    return (float)minutes * 360.0f / 1440.0f - 90.0f;
}

static void arc_from(lv_layer_t *layer, int32_t cx, int32_t cy, int32_t radius, int32_t width, float a0, float a1,
                     lv_color_t color, bool rounded)
{
    lv_draw_arc_dsc_t arc;
    bool full = a1 - a0 >= 360.0f;

    while (a0 < 0) {
        a0 += 360.0f;
    }
    while (a1 < 0) {
        a1 += 360.0f;
    }
    a0 = fmodf(a0, 360.0f);
    /* A full turn kept as 0..360: normalised, its ends meet and nothing is drawn. */
    a1 = full ? a0 + 360.0f : fmodf(a1, 360.0f);
    lv_draw_arc_dsc_init(&arc);
    arc.color = color;
    arc.width = (int32_t)width;
    arc.rounded = rounded;
    arc.center.x = cx;
    arc.center.y = cy;
    arc.radius = (uint16_t)radius;
    arc.start_angle = (lv_value_precise_t)a0;
    arc.end_angle = (lv_value_precise_t)a1;
    kit_draw_arc(layer, &arc);
}

static void line_polar(lv_layer_t *layer, int32_t cx, int32_t cy, float r0, float r1, float deg, lv_color_t color,
                       int32_t width)
{
    lv_draw_line_dsc_t line;
    float rad = deg * 3.14159265f / 180.0f;
    float s = sinf(rad);
    float c = cosf(rad);

    lv_draw_line_dsc_init(&line);
    line.color = color;
    line.width = width;
    line.p1.x = (lv_value_precise_t)(cx + r0 * s);
    line.p1.y = (lv_value_precise_t)(cy - r0 * c);
    line.p2.x = (lv_value_precise_t)(cx + r1 * s);
    line.p2.y = (lv_value_precise_t)(cy - r1 * c);
    lv_draw_line(layer, &line);
}

/* Instrument: a 24 h mini dial with the night segment in signal orange. */
static void dial_draw_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *obj = lv_event_get_current_target(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t a;
    int32_t cx;
    int32_t cy;
    bool on = ctx->settings->night_mode.enabled;

    lv_obj_get_coords(obj, &a);
    cx = a.x1 + lv_area_get_width(&a) / 2;
    cy = a.y1 + lv_area_get_height(&a) / 2;
    arc_from(layer, cx, cy, 23, 2, 0, 360, sk->line_strong, false);
    arc_from(layer, cx, cy, 24, 4, minutes_to_lv_angle(start_minutes(ctx)), minutes_to_lv_angle(end_minutes(ctx)),
             on ? sk->accent : sk->text_3, false);
    for (int h = 0; h < 24; h += 6) {
        line_polar(layer, cx, cy, 14, 18, (float)h * 15.0f, sk->text_3, 2);
    }
}

/* Orbit: the brightness gauge, a 270° capsule arc. */
static void gauge_draw_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *obj = lv_event_get_current_target(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    int v = brightness_hw_to_ui(ctx->settings->night_mode.brightness);
    lv_area_t a;
    int32_t cx;
    int32_t cy;

    lv_obj_get_coords(obj, &a);
    cx = a.x1 + lv_area_get_width(&a) / 2;
    cy = a.y1 + lv_area_get_height(&a) / 2;
    arc_from(layer, cx, cy, 47, 10, 135, 45, sk->well, true);
    if (v > 0) {
        arc_from(layer, cx, cy, 47, 10, 135, 135 + 270.0f * (float)v / 100.0f,
                 ctx->settings->night_mode.enabled ? sk->tint_night : sk->text_3, true);
    }
}

/* Atelier: the rim becomes a 24 h ring with the schedule in gilt. The static ring, its hour
 * marks and dots are baked once; only the schedule arc is live. */
static void ring_dial_spec(kit_dial_t *dial)
{
    const ui_skin_t *sk = ui_skin();

    memset(dial, 0, sizeof(*dial));
    dial->cx = KIT_CENTER;
    dial->cy = KIT_CENTER;
    dial->rings[0] = (kit_dial_ring_t) {.r = 356, .width = 1, .end_deg = 360, .color = sk->accent_deep, .opa = 140};
    dial->rings[1] = (kit_dial_ring_t) {.r = RING_R, .width = 4, .end_deg = 360, .color = sk->line, .opa = LV_OPA_COVER};
    dial->ticks[0] = (kit_dial_ticks_t) {
        .count = 24,
        .offset_deg = -90.0f,
        .r_in = 326,
        .r_out = 342,
        .major_every = 6,
        .r_in_major = 322,
        .r_out_major = 346,
        .width = 1,
        .width_major = 2,
        .color = sk->accent_deep,
        .color_major = sk->label,
        .opa = LV_OPA_COVER,
    };
    dial->ticks[1] = (kit_dial_ticks_t) {
        .count = 24,
        .offset_deg = -90.0f + 7.5f,
        .r_in = RING_R - 1,
        .r_out = RING_R + 1,
        .width = 2,
        .color = sk->text_3,
        .opa = LV_OPA_COVER,
    };
}

void st_night_prewarm(void)
{
    kit_dial_t dial;

    if (ui_skin_style() == CLOCK_UI_STYLE_ATELIER) {
        ring_dial_spec(&dial);
        kit_dial_prewarm(&dial);
    }
}

static void ring_draw_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);
    const ui_skin_t *sk = ui_skin();
    lv_layer_t *layer = lv_event_get_layer(e);
    bool on = ctx->settings->night_mode.enabled;
    kit_dial_t dial;
    int32_t start;
    int32_t end;

    ring_dial_spec(&dial);
    kit_dial_draw(layer, &dial);
    /* The schedule as a cached ring too: a live arc this size costs the device a few hundred
     * ms a frame. Each new span while dragging a jewel builds one small dial. */
    start = (int32_t)lroundf(minutes_to_lv_angle(start_minutes(ctx)));
    end = (int32_t)lroundf(minutes_to_lv_angle(end_minutes(ctx)));
    if (end <= start) {
        end += 360;
    }
    kit_draw_ring(layer, KIT_CENTER, KIT_CENTER, RING_R, 4, start, end, on ? sk->accent : sk->text_3, LV_OPA_COVER);
}

static void place_jewels(clock_ui_context_t *ctx)
{
    clock_ui_settings_state_t *st = ST(ctx);
    uint16_t m[2] = {start_minutes(ctx), end_minutes(ctx)};

    for (int i = 0; i < 2; i++) {
        float rad = (float)m[i] * 2.0f * 3.14159265f / 1440.0f;

        if (st->night_jewel[i] == NULL) {
            continue;
        }
        lv_obj_set_pos(st->night_jewel[i], KIT_CENTER + (int32_t)lroundf(RING_R * sinf(rad)) - JEWEL / 2,
                       KIT_CENTER - (int32_t)lroundf(RING_R * cosf(rad)) - JEWEL / 2);
    }
}

static void jewel_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);
    clock_ui_settings_state_t *st = ST(ctx);
    lv_obj_t *jewel = lv_event_get_current_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    int which = jewel == st->night_jewel[1] ? 1 : 0;
    lv_indev_t *indev = lv_indev_active();
    lv_point_t p;

    if (indev == NULL || !ctx->settings->night_mode.enabled) {
        return;
    }
    if (code == LV_EVENT_PRESSED) {
        st->night_drag_min[0] = start_minutes(ctx);
        st->night_drag_min[1] = end_minutes(ctx);
        st->night_drag = (int8_t)which;
        return;
    }
    if (st->night_drag != which) {
        return;
    }
    if (code == LV_EVENT_PRESSING) {
        float deg;
        int minutes;

        lv_indev_get_point(indev, &p);
        if (LV_ABS(p.x - KIT_CENTER) + LV_ABS(p.y - KIT_CENTER) < 60) {
            return;
        }
        deg = atan2f((float)(p.x - KIT_CENTER), (float)(KIT_CENTER - p.y)) * 180.0f / 3.14159265f;
        if (deg < 0) {
            deg += 360.0f;
        }
        /* Snap to a quarter of an hour. */
        minutes = ((int)lroundf(deg * 1440.0f / 360.0f / 15.0f) * 15) % 1440;
        if (minutes != st->night_drag_min[which]) {
            st->night_drag_min[which] = (uint16_t)minutes;
            place_jewels(ctx);
            lv_obj_invalidate(st->pages[UI_ST_NIGHT].root);
            st_night_sync(ctx, false);
        }
        return;
    }
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        uint16_t s = st->night_drag_min[0];
        uint16_t en = st->night_drag_min[1];

        st->night_drag = -1;
        request_set_night_schedule(ctx, (uint8_t)(s / 60U), (uint8_t)(s % 60U), (uint8_t)(en / 60U),
                                   (uint8_t)(en % 60U));
        st_night_sync(ctx, false);
    }
}

/* --- Dimming --------------------------------------------------------------------------- */

static void dim_add(clock_ui_context_t *ctx, lv_obj_t *obj)
{
    clock_ui_settings_state_t *st = ST(ctx);
    ui_st_dim_t *d;

    if (obj == NULL || st->night_dim_count >= UI_ST_DIM_MAX) {
        return;
    }
    d = &st->night_dim[st->night_dim_count++];
    d->obj = obj;
    d->image = lv_obj_check_type(obj, &lv_image_class);
    d->color = d->image ? lv_obj_get_style_image_recolor(obj, 0) : lv_obj_get_style_text_color(obj, 0);
}

static void dim_row(clock_ui_context_t *ctx, const kit_row_t *row)
{
    dim_add(ctx, row->title);
    dim_add(ctx, row->subtitle);
    dim_add(ctx, row->value);
}

static void apply_enabled(clock_ui_context_t *ctx, bool on)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_settings_state_t *st = ST(ctx);

    for (int i = 0; i < st->night_dim_count; i++) {
        ui_st_dim_t *d = &st->night_dim[i];
        lv_color_t c = on ? d->color : sk->text_3;

        if (d->image) {
            lv_obj_set_style_image_recolor(d->obj, c, 0);
        } else {
            lv_obj_set_style_text_color(d->obj, c, 0);
        }
    }
    for (int i = 0; i < (int)(sizeof(st->night_click) / sizeof(st->night_click[0])); i++) {
        st_set_clickable(st->night_click[i], on);
    }
    st_set_enabled(st->night_bright_slider, on);
    st_set_enabled(st->night_sunrise_sw, on);
    kit_set_tint(st->night_bright_slider, on ? sk->tint_night : sk->text_3);
    kit_set_tint(st->night_sunrise_sw, on ? sk->tint_night : sk->text_3);
    if (st->night_face_img != NULL) {
        lv_obj_set_style_image_opa(st->night_face_img, on ? LV_OPA_COVER : LV_OPA_50, 0);
    }
    for (int i = 0; i < 2; i++) {
        if (st->night_jewel[i] != NULL) {
            lv_obj_set_style_bg_color(st->night_jewel[i], on ? sk->accent : sk->text_3, 0);
            st_set_clickable(st->night_jewel[i], on);
        }
    }
    kit_show(st->night_hint, !on);
    if (st->night_dial != NULL) {
        lv_obj_invalidate(st->night_dial);
    }
    if (st->night_gauge != NULL) {
        lv_obj_invalidate(st->night_gauge);
    }
    if (st->night_ring != NULL) {
        lv_obj_invalidate(st->pages[UI_ST_NIGHT].root);
    }
}

/* --- Callbacks ------------------------------------------------------------------------- */

static void enabled_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);

    if (ctx->suppress_events) {
        return;
    }
    request_set_night_mode_enabled(ctx, lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED));
    st_night_sync(ctx, false);
}

/* --- Sunrise ------------------------------------------------------------------------- */

static void sunrise_row_cb(lv_event_t *e)
{
    st_show_page(lv_event_get_user_data(e), UI_ST_SUNRISE);
}

/* The sunrise's options under its switch: "20 min · 60%", opening the page that sets them. */
static void sunrise_options_row(clock_ui_context_t *ctx, lv_obj_t *group)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_settings_state_t *st = ST(ctx);
    kit_row_t row;
    kit_row_opts_t o = {0};

    o.trail = KIT_TRAIL_VALUE;
    o.value = " ";
    o.value_chevron = true;
    o.tint = sk->tint_night;
    o.on_click = sunrise_row_cb;
    o.user_data = ctx;
    if (ui_skin_is_orbit()) {
        o.icon = KIT_ICON_SUN;
    }
    st_row(ctx, &row, group, &o, ST_STR(&s_sunrise), ST_NONE);
    if (ui_skin_is_orbit()) {
        lv_obj_set_style_radius(row.row, sk->radius_m, 0);
    }
    st->night_sunrise_value = row.value;
    dim_row(ctx, &row);
}

/* Changes one option (the others at -1 keep theirs). */
static void sunrise_set(clock_ui_context_t *ctx, int length, int peak, int chime, int face)
{
    const night_mode_config_t *nm = &ctx->settings->night_mode;

    request_set_night_sunrise_options(ctx, length >= 0 ? s_sunrise_lengths[length] : nm->sunrise_minutes,
                                      peak >= 0 ? s_sunrise_peaks[peak] : nm->sunrise_peak,
                                      chime >= 0 ? chime != 0 : nm->sunrise_chime,
                                      face >= 0 ? (uint8_t)face : nm->sunrise_face);
}

static void sunrise_len_cb(lv_event_t *e)
{
    ui_st_item_t *item = lv_event_get_user_data(e);

    sunrise_set(item->ui, item->index, -1, -1, -1);
}

static void sunrise_peak_cb(lv_event_t *e)
{
    ui_st_item_t *item = lv_event_get_user_data(e);

    sunrise_set(item->ui, -1, item->index, -1, -1);
}

/* Rows 0..2 are Daybreak's themes (setting 1..3), row 3 none (0). */
static void sunrise_face_cb(lv_event_t *e)
{
    ui_st_item_t *item = lv_event_get_user_data(e);

    sunrise_set(item->ui, -1, -1, -1, item->index < 3 ? item->index + 1 : 0);
}

static void sunrise_chime_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);

    sunrise_set(ctx, -1, -1, lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED) ? 1 : 0, -1);
}

/* Back on the Sunrise page once the preview is over. */
static void sunrise_preview_done(clock_ui_context_t *ctx)
{
    ui_settings_open_page(ctx, UI_SETTINGS_PAGE_NIGHT);
    st_show_page(ctx, UI_ST_SUNRISE);
}

static void sunrise_preview_cb(lv_event_t *e)
{
    ui_sunrise_preview(lv_event_get_user_data(e), sunrise_preview_done);
}

void st_sunrise_build(clock_ui_context_t *ctx)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_settings_state_t *st = ST(ctx);
    kit_page_t *p;
    lv_obj_t *group;
    lv_obj_t *label;
    kit_row_t row;
    kit_row_opts_t o = {0};
    char text[64];

    p = st_page_begin(ctx, UI_ST_SUNRISE, ui_skin_is_orbit() ? ST_NONE : ST_ID(L10N_NIGHT_MODE), ST_STR(&s_sunrise),
                      KIT_NAV_BACK);
    group = kit_group(p->content);
    o.icon = KIT_ICON_PLAY;
    o.tint = sk->tint_night;
    o.on_click = sunrise_preview_cb;
    o.user_data = ctx;
    st_row(ctx, &row, group, &o, ST_STR(&s_sunrise_preview), ST_STR(&s_sunrise_preview_sub));
    label = kit_section_label(p->content, "");
    st_bind(ctx, label, ST_STR(&s_sunrise_length));
    group = kit_group(p->content);
    for (int i = 0; i < 3; i++) {
        o = (kit_row_opts_t) {0};
        st->sunrise_len_item[i].ui = ctx;
        st->sunrise_len_item[i].index = (uint8_t)i;
        o.title = " ";
        o.trail = KIT_TRAIL_CUSTOM;
        o.on_click = sunrise_len_cb;
        o.user_data = &st->sunrise_len_item[i];
        kit_row_create(&row, group, &o);
        st->sunrise_len_title[i] = row.title;
        st->sunrise_len_check[i] = kit_icon_small(row.trail, KIT_ICON_CHECK, sk->accent);
    }
    label = kit_section_label(p->content, "");
    st_bind(ctx, label, ST_STR(&s_sunrise_peak));
    group = kit_group(p->content);
    for (int i = 0; i < 3; i++) {
        o = (kit_row_opts_t) {0};
        st->sunrise_peak_item[i].ui = ctx;
        st->sunrise_peak_item[i].index = (uint8_t)i;
        snprintf(text, sizeof(text), "%d%%", s_sunrise_peaks[i]);
        o.title = text;
        o.trail = KIT_TRAIL_CUSTOM;
        o.on_click = sunrise_peak_cb;
        o.user_data = &st->sunrise_peak_item[i];
        kit_row_create(&row, group, &o);
        st->sunrise_peak_check[i] = kit_icon_small(row.trail, KIT_ICON_CHECK, sk->accent);
    }
    label = kit_section_label(p->content, "");
    st_bind(ctx, label, ST_STR(&s_sunrise_face));
    group = kit_group(p->content);
    for (int i = 0; i < 4; i++) {
        o = (kit_row_opts_t) {0};
        st->sunrise_face_item[i].ui = ctx;
        st->sunrise_face_item[i].index = (uint8_t)i;
        o.title = " ";
        o.trail = KIT_TRAIL_CUSTOM;
        o.on_click = sunrise_face_cb;
        o.user_data = &st->sunrise_face_item[i];
        kit_row_create(&row, group, &o);
        st->sunrise_face_title[i] = row.title;
        st->sunrise_face_check[i] = kit_icon_small(row.trail, KIT_ICON_CHECK, sk->accent);
    }
    group = kit_group(p->content);
    o = (kit_row_opts_t) {0};
    o.trail = KIT_TRAIL_SWITCH;
    o.tint = sk->tint_night;
    st_row(ctx, &row, group, &o, ST_STR(&s_sunrise_chime), ST_STR(&s_sunrise_chime_sub));
    st->sunrise_chime_sw = row.control;
    st_keep_gestures(row.control);
    lv_obj_add_event_cb(row.control, sunrise_chime_cb, LV_EVENT_VALUE_CHANGED, ctx);
}

void st_sunrise_sync(clock_ui_context_t *ctx)
{
    clock_ui_settings_state_t *st = ST(ctx);
    const night_mode_config_t *nm = &ctx->settings->night_mode;
    char text[64];

    if (!st->built[UI_ST_SUNRISE]) {
        return;
    }
    for (int i = 0; i < 3; i++) {
        snprintf(text, sizeof(text), ui_l10n_str(ctx, &s_sunrise_minutes), s_sunrise_lengths[i]);
        kit_label_set_text_if_changed(st->sunrise_len_title[i], text);
        kit_show(st->sunrise_len_check[i], nm->sunrise_minutes == s_sunrise_lengths[i]);
        kit_show(st->sunrise_peak_check[i], nm->sunrise_peak == s_sunrise_peaks[i]);
    }
    for (int i = 0; i < 4; i++) {
        kit_label_set_text_if_changed(st->sunrise_face_title[i], ui_l10n_str(ctx, &s_sunrise_faces[i]));
        kit_show(st->sunrise_face_check[i], nm->sunrise_face == (i < 3 ? i + 1 : 0));
    }
    kit_switch_set(st->sunrise_chime_sw, nm->sunrise_chime);
}

static void sunrise_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);

    if (ctx->suppress_events) {
        return;
    }
    request_set_night_sunrise_brightness_enabled(ctx, lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED));
}

static void brightness_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);
    int32_t v = lv_slider_get_value(lv_event_get_target(e));
    char text[16];

    if (ctx->suppress_events) {
        return;
    }
    request_set_night_brightness(ctx, brightness_ui_to_hw((int)v));
    snprintf(text, sizeof(text), "%d%%", (int)v);
    kit_label_set_text_if_changed(ST(ctx)->night_bright_value, text);
    kit_label_set_text_if_changed(ST(ctx)->nb_value, text);
    if (ST(ctx)->night_gauge != NULL) {
        lv_obj_invalidate(ST(ctx)->night_gauge);
    }
}

static void schedule_cb(lv_event_t *e)
{
    st_schedule_open(lv_event_get_user_data(e), 0);
}

static void schedule_end_cb(lv_event_t *e)
{
    st_schedule_open(lv_event_get_user_data(e), 1);
}

static void face_cb(lv_event_t *e)
{
    st_show_page(lv_event_get_user_data(e), UI_ST_FACE);
}

static void bright_tile_cb(lv_event_t *e)
{
    st_show_page(lv_event_get_user_data(e), UI_ST_NIGHT_BRIGHT);
}

/* --- Page ------------------------------------------------------------------------------ */

static lv_obj_t *face_thumb(lv_obj_t *parent, int32_t size, int32_t ring_gap, lv_color_t ring)
{
    lv_obj_t *holder = lv_obj_create(parent);
    lv_obj_t *img;

    lv_obj_remove_style_all(holder);
    lv_obj_remove_flag(holder, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(holder, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_size(holder, size + 2 * ring_gap, size + 2 * ring_gap);
    lv_obj_set_style_radius(holder, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(holder, ring_gap > 0 ? 1 : 0, 0);
    lv_obj_set_style_border_color(holder, ring, 0);
    img = lv_image_create(holder);
    lv_obj_set_size(img, size, size);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(img, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_center(img);
    return img;
}

static lv_obj_t *orbit_tile(lv_obj_t *parent, int32_t h, lv_event_cb_t cb, void *user_data)
{
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *tile = lv_obj_create(parent);

    lv_obj_remove_style_all(tile);
    lv_obj_remove_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(tile, (sk->content_w - 8) / 2, h);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(tile, sk->surface, 0);
    lv_obj_set_style_radius(tile, sk->radius_m, 0);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(tile, 18, 0);
    kit_make_pressable(tile);
    lv_obj_add_event_cb(tile, cb, LV_EVENT_CLICKED, user_data);
    return tile;
}

static void build_orbit(clock_ui_context_t *ctx, kit_page_t *p)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_settings_state_t *st = ST(ctx);
    lv_obj_t *line;
    lv_obj_t *tile;
    lv_obj_t *head;
    lv_obj_t *label;
    lv_obj_t *arrow;
    lv_obj_t *group;
    kit_row_t row;
    kit_row_opts_t o = {0};

    st_bind(ctx, kit_section_label(p->content, ""), ST_ID(L10N_SCHEDULE));
    line = st_hbox(p->content, 8);
    lv_obj_set_width(line, lv_pct(100));
    lv_obj_set_style_pad_bottom(line, 16, 0);
    for (int i = 0; i < 2; i++) {
        tile = orbit_tile(line, 128, i == 0 ? schedule_cb : schedule_end_cb, ctx);
        st->night_click[i] = tile;
        head = st_hbox(tile, 7);
        kit_icon_small(head, i == 0 ? KIT_ICON_MOON : KIT_ICON_SUN, i == 0 ? sk->tint_night : sk->tint_brightness);
        label = st_label(ctx, head, KIT_TEXT_LABEL, ST_STR(i == 0 ? &s_starts : &s_ends));
        dim_add(ctx, label);
        label = kit_label(tile, KIT_TEXT_TIME_M, "");
        lv_obj_set_style_margin_top(label, 2, 0);
        dim_add(ctx, label);
        if (i == 0) {
            st->night_start_value = label;
        } else {
            st->night_end_value = label;
        }
    }
    /* The arrow between the tiles. */
    arrow = lv_obj_create(line);
    lv_obj_remove_style_all(arrow);
    lv_obj_remove_flag(arrow, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(arrow, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(arrow, 48, 48);
    lv_obj_set_style_radius(arrow, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(arrow, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(arrow, sk->raised, 0);
    lv_obj_set_style_border_width(arrow, 4, 0);
    lv_obj_set_style_border_color(arrow, sk->bg, 0);
    lv_obj_align(arrow, LV_ALIGN_CENTER, 0, -8);
    lv_obj_center(st_image(arrow, &ui_settings_arrow_24, sk->text_2));

    line = st_hbox(p->content, 8);
    lv_obj_set_width(line, lv_pct(100));
    lv_obj_set_style_pad_bottom(line, 16, 0);
    tile = orbit_tile(line, 208, face_cb, ctx);
    lv_obj_set_style_pad_top(tile, 22, 0);
    st->night_click[2] = tile;
    st->night_face_img = face_thumb(tile, FACE_THUMB_ORBIT, 2, sk->raised);
    lv_obj_set_style_border_width(lv_obj_get_parent(st->night_face_img), 2, 0);
    label = st_label(ctx, tile, KIT_TEXT_VALUE, ST_ID(L10N_NIGHT_FACE));
    lv_obj_set_style_text_color(label, sk->text, 0);
    st_one_line(label, (sk->content_w - 8) / 2 - 24);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_margin_top(label, 10, 0);
    dim_add(ctx, label);
    st->night_face_sub = st_label(ctx, tile, KIT_TEXT_LABEL, ST_STR(&s_tap_to_choose));
    st_one_line(st->night_face_sub, (sk->content_w - 8) / 2 - 24);
    lv_obj_set_style_text_align(st->night_face_sub, LV_TEXT_ALIGN_CENTER, 0);
    dim_add(ctx, st->night_face_sub);

    tile = orbit_tile(line, 208, bright_tile_cb, ctx);
    lv_obj_set_style_pad_top(tile, 22, 0);
    st->night_click[3] = tile;
    st->night_gauge = lv_obj_create(tile);
    lv_obj_remove_style_all(st->night_gauge);
    lv_obj_remove_flag(st->night_gauge, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(st->night_gauge, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_size(st->night_gauge, GAUGE_SIZE, GAUGE_SIZE);
    lv_obj_add_event_cb(st->night_gauge, gauge_draw_cb, LV_EVENT_DRAW_MAIN, ctx);
    lv_obj_center(kit_icon(st->night_gauge, KIT_ICON_SUN, sk->tint_night));
    dim_add(ctx, lv_obj_get_child(st->night_gauge, 0));
    label = st_label(ctx, tile, KIT_TEXT_VALUE, ST_ID(L10N_BRIGHTNESS));
    lv_obj_set_style_text_color(label, sk->text, 0);
    st_one_line(label, (sk->content_w - 8) / 2 - 24);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_margin_top(label, 10, 0);
    dim_add(ctx, label);
    st->night_bright_value = kit_label(tile, KIT_TEXT_LABEL, "");
    lv_obj_set_style_text_color(st->night_bright_value, sk->tint_night, 0);
    dim_add(ctx, st->night_bright_value);

    group = kit_group(p->content);
    o.icon = KIT_ICON_SUNRISE;
    o.tint = sk->tint_night;
    o.trail = KIT_TRAIL_SWITCH;
    st_row(ctx, &row, group, &o, ST_ID(L10N_SUNRISE_RAMP), ST_ID(L10N_SUNRISE_RAMP_SUBTITLE));
    lv_obj_set_style_radius(row.row, sk->radius_m, 0);
    lv_obj_set_style_pad_ver(row.row, 14, 0);
    st->night_sunrise_sw = row.control;
    dim_row(ctx, &row);
    sunrise_options_row(ctx, group);
}

static void build_list(clock_ui_context_t *ctx, kit_page_t *p, lv_obj_t *group)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_settings_state_t *st = ST(ctx);
    kit_row_t row;
    kit_row_opts_t o = {0};
    lv_obj_t *box;

    LV_UNUSED(p);
    /* Schedule. */
    o.trail = KIT_TRAIL_CUSTOM;
    o.on_click = schedule_cb;
    o.user_data = ctx;
    if (ui_skin_is_instrument()) {
        o.subtitle = " ";
    }
    st_row(ctx, &row, group, &o, ST_ID(L10N_SCHEDULE), ST_NONE);
    st->night_click[0] = row.row;
    st->night_sched_sub = row.subtitle;
    st->night_sched_value = kit_label(row.trail, KIT_TEXT_VALUE, "");
    dim_row(ctx, &row);
    dim_add(ctx, st->night_sched_value);
    if (ui_skin_is_instrument()) {
        lv_obj_set_style_text_font(row.subtitle, sk->font_caption, 0);
        lv_obj_set_style_text_color(row.subtitle, sk->text_3, 0);
        dim_add(ctx, row.subtitle);
        st->night_dial = lv_obj_create(row.trail);
        lv_obj_remove_style_all(st->night_dial);
        lv_obj_remove_flag(st->night_dial, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(st->night_dial, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_size(st->night_dial, DIAL_SIZE, DIAL_SIZE);
        lv_obj_add_event_cb(st->night_dial, dial_draw_cb, LV_EVENT_DRAW_MAIN, ctx);
    } else {
        lv_obj_set_style_text_color(st->night_sched_value, sk->value, 0);
        kit_icon_small(row.trail, KIT_ICON_CHEVRON_RIGHT, sk->accent_deep);
    }

    /* Night face. */
    o = (kit_row_opts_t) {0};
    o.on_click = face_cb;
    o.user_data = ctx;
    o.subtitle = " ";
    o.trail = ui_skin_is_instrument() ? KIT_TRAIL_CUSTOM : KIT_TRAIL_CHEVRON;
    st_row(ctx, &row, group, &o, ST_ID(L10N_NIGHT_FACE), ST_NONE);
    st->night_click[1] = row.row;
    st->night_face_sub = row.subtitle;
    dim_row(ctx, &row);
    if (ui_skin_is_instrument()) {
        st->night_face_img = face_thumb(row.trail, FACE_THUMB_INSTRUMENT, 1, sk->line_strong);
        lv_obj_set_style_pad_ver(row.row, 14, 0);
    } else {
        st->night_face_img = face_thumb(row.row, FACE_THUMB_ATELIER, 4, sk->label);
        lv_obj_move_to_index(lv_obj_get_parent(st->night_face_img), 0);
        lv_obj_set_style_margin_right(lv_obj_get_parent(st->night_face_img), 0, 0);
    }

    /* Brightness. */
    o = (kit_row_opts_t) {0};
    o.trail = KIT_TRAIL_CUSTOM;
    st_row(ctx, &row, group, &o, ST_ID(L10N_BRIGHTNESS), ST_NONE);
    dim_row(ctx, &row);
    if (ui_skin_is_instrument()) {
        lv_obj_set_flex_flow(row.row, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_style_pad_row(row.row, 4, 0);
        lv_obj_set_style_pad_top(row.row, 14, 0);
        st->night_bright_value = kit_label(row.trail, KIT_TEXT_CAPTION, "");
        lv_obj_set_style_text_color(st->night_bright_value, sk->text, 0);
        lv_obj_set_style_text_font(st->night_bright_value, sk->font_body, 0);
        st->night_bright_slider = kit_slider(row.row, 0, 100, sk->tint_night);
        lv_obj_set_width(st->night_bright_slider, lv_pct(100));
        lv_obj_set_style_min_height(lv_obj_get_parent(row.title), 36, 0);
    } else {
        lv_obj_set_flex_grow(lv_obj_get_parent(row.title), 0);
        lv_obj_set_width(lv_obj_get_parent(row.title), LV_SIZE_CONTENT);
        lv_obj_set_width(row.title, LV_SIZE_CONTENT);
        lv_obj_set_flex_grow(row.trail, 1);
        lv_obj_set_flex_align(row.trail, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(row.trail, 16, 0);
        st->night_bright_slider = kit_slider(row.trail, 0, 100, sk->tint_night);
        lv_obj_set_width(st->night_bright_slider, 150);
        st->night_bright_value = kit_label(row.trail, KIT_TEXT_VALUE, "");
        lv_obj_set_style_text_color(st->night_bright_value, sk->value, 0);
        lv_obj_set_style_min_width(st->night_bright_value, 64, 0);
        lv_obj_set_style_text_align(st->night_bright_value, LV_TEXT_ALIGN_RIGHT, 0);
    }
    dim_add(ctx, st->night_bright_value);
    st_keep_gestures(st->night_bright_slider);
    lv_obj_add_event_cb(st->night_bright_slider, brightness_cb, LV_EVENT_VALUE_CHANGED, ctx);

    /* Sunrise ramp. */
    o = (kit_row_opts_t) {0};
    o.trail = KIT_TRAIL_SWITCH;
    o.tint = sk->tint_night;
    st_row(ctx, &row, group, &o, ST_ID(L10N_SUNRISE_RAMP), ST_ID(L10N_SUNRISE_RAMP_SUBTITLE));
    st->night_sunrise_sw = row.control;
    dim_row(ctx, &row);
    box = lv_obj_get_parent(row.title);
    LV_UNUSED(box);
    sunrise_options_row(ctx, group);
}

static void build_ring(clock_ui_context_t *ctx, kit_page_t *p)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_settings_state_t *st = ST(ctx);
    static const char *const labels[3] = {"6", "12", "18"};

    /* The 24 h ring replaces the kit's minute track on this page. */
    kit_page_set_rim(p, false);
    st->night_ring = p->root;
    lv_obj_add_event_cb(p->root, ring_draw_cb, LV_EVENT_DRAW_POST, ctx);
    for (int i = 0; i < 3; i++) {
        float rad = (float)(i + 1) * 90.0f * 3.14159265f / 180.0f;
        lv_obj_t *label = kit_label(p->root, KIT_TEXT_LABEL, labels[i]);

        lv_obj_set_style_text_color(label, sk->text_3, 0);
        lv_obj_set_style_text_letter_space(label, 0, 0);
        lv_obj_update_layout(label);
        lv_obj_set_pos(label, KIT_CENTER + (int32_t)lroundf(306 * sinf(rad)) - lv_obj_get_width(label) / 2,
                       KIT_CENTER - (int32_t)lroundf(306 * cosf(rad)) - lv_obj_get_height(label) / 2);
    }
    for (int i = 0; i < 2; i++) {
        lv_obj_t *j = lv_obj_create(p->root);

        lv_obj_remove_style_all(j);
        lv_obj_remove_flag(j, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_add_flag(j, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(j, JEWEL, JEWEL);
        lv_obj_set_ext_click_area(j, 18);
        lv_obj_set_style_radius(j, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(j, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(j, sk->accent, 0);
        lv_obj_set_style_outline_width(j, 3, 0);
        lv_obj_set_style_outline_color(j, sk->bg, 0);
        lv_obj_center(kit_icon_small(j, i == 0 ? KIT_ICON_MOON : KIT_ICON_SUN, sk->accent_text));
        lv_obj_add_event_cb(j, jewel_cb, LV_EVENT_PRESSED, ctx);
        lv_obj_add_event_cb(j, jewel_cb, LV_EVENT_PRESSING, ctx);
        lv_obj_add_event_cb(j, jewel_cb, LV_EVENT_RELEASED, ctx);
        lv_obj_add_event_cb(j, jewel_cb, LV_EVENT_PRESS_LOST, ctx);
        st->night_jewel[i] = j;
    }
    /* The nav key stays above the ring. */
    if (p->nav != NULL) {
        lv_obj_move_foreground(p->nav);
    }
    place_jewels(ctx);
}

void st_night_build(clock_ui_context_t *ctx)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_settings_state_t *st = ST(ctx);
    kit_page_t *p;
    lv_obj_t *group;
    kit_row_t row;
    kit_row_opts_t o = {0};

    st->night_dim_count = 0;
    p = st_page_begin(ctx, UI_ST_NIGHT, ui_skin_is_atelier() ? ST_ID(L10N_SETTINGS) : ST_NONE, ST_ID(L10N_NIGHT_MODE),
                      KIT_NAV_BACK);
    if (ui_skin_is_instrument()) {
        kit_page_set_eyebrow(p, " ");
    }
    kit_set_tint(p->nav, sk->tint_night);

    /* Enabled, with the hint as its subtitle while off. */
    group = kit_group(p->content);
    o.trail = KIT_TRAIL_SWITCH;
    o.tint = sk->tint_night;
    o.subtitle = " ";
    if (ui_skin_is_orbit()) {
        o.icon = KIT_ICON_MOON;
    }
    st_row(ctx, &row, group, &o, ST_ID(L10N_ENABLED), ST_ID(L10N_NIGHT_DISABLED_HINT));
    st->night_sw = row.control;
    st->night_hint = row.subtitle;
    /* A single-line row while the hint is hidden. */
    lv_obj_set_style_min_height(lv_obj_get_parent(row.title), sk->row_h - 20 - (ui_skin_is_orbit() ? 0 : 1), 0);
    lv_label_set_long_mode(row.subtitle, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_add_event_cb(row.control, enabled_cb, LV_EVENT_VALUE_CHANGED, ctx);
    st_keep_gestures(row.control);

    if (ui_skin_is_orbit()) {
        build_orbit(ctx, p);
    } else {
        if (ui_skin_is_atelier()) {
            /* Atelier rows share one ruled list. */
            build_list(ctx, p, group);
        } else {
            build_list(ctx, p, group);
        }
    }
    lv_obj_add_event_cb(st->night_sunrise_sw, sunrise_cb, LV_EVENT_VALUE_CHANGED, ctx);
    st_keep_gestures(st->night_sunrise_sw);
    if (ui_skin_is_atelier()) {
        build_ring(ctx, p);
    }
    st->night_valid = false;
}

void st_night_sync(clock_ui_context_t *ctx, bool force)
{
    clock_ui_settings_state_t *st = ST(ctx);
    const night_mode_config_t *nm = &ctx->settings->night_mode;
    clock_face_id_t face = sanitize_enabled_face(nm->face);
    int ui_bright = brightness_hw_to_ui(nm->brightness);
    char text[64];

    if (!st->built[UI_ST_NIGHT]) {
        return;
    }
    kit_switch_set(st->night_sw, nm->enabled);
    kit_switch_set(st->night_sunrise_sw, nm->sunrise_brightness_enabled);
    if (st->night_sunrise_value != NULL) {
        snprintf(text, sizeof(text), ui_l10n_str(ctx, &s_sunrise_value), nm->sunrise_minutes, nm->sunrise_peak);
        kit_label_set_text_if_changed(st->night_sunrise_value, text);
    }

    if (st->night_sched_value != NULL) {
        format_range(ctx, text, sizeof(text));
        kit_label_set_text_if_changed(st->night_sched_value, text);
    }
    if (st->night_sched_sub != NULL) {
        format_duration(ctx, text, sizeof(text));
        kit_label_set_text_if_changed(st->night_sched_sub, text);
    }
    if (st->night_start_value != NULL) {
        snprintf(text, sizeof(text), "%02u:%02u", start_minutes(ctx) / 60U, start_minutes(ctx) % 60U);
        kit_label_set_text_if_changed(st->night_start_value, text);
        snprintf(text, sizeof(text), "%02u:%02u", end_minutes(ctx) / 60U, end_minutes(ctx) % 60U);
        kit_label_set_text_if_changed(st->night_end_value, text);
    }
    {
        uint32_t sched = (uint32_t)start_minutes(ctx) << 16 | end_minutes(ctx);

        if (force || !st->night_valid || sched != st->night_sched_shown) {
            st->night_sched_shown = sched;
            if (st->night_dial != NULL) {
                lv_obj_invalidate(st->night_dial);
            }
            if (st->night_ring != NULL && st->night_drag < 0) {
                place_jewels(ctx);
                lv_obj_invalidate(st->pages[UI_ST_NIGHT].root);
            }
        }
    }

    if (ui_skin_is_instrument()) {
        snprintf(text, sizeof(text), "%s · %s", face_catalog_name(face), ui_l10n_str(ctx, &s_tap_to_choose));
        kit_label_set_text_if_changed(st->night_face_sub, text);
    } else if (ui_skin_is_atelier()) {
        kit_label_set_text_if_changed(st->night_face_sub, ui_l10n_str(ctx, &s_tap_to_choose));
    }
    if (st->night_face_buf == NULL || !st->night_valid || st->night_face_shown != face ||
        st->night_face_key != st_face_key(ctx, face)) {
        int32_t size = ui_skin_is_instrument() ? FACE_THUMB_INSTRUMENT : ui_skin_is_orbit() ? FACE_THUMB_ORBIT : FACE_THUMB_ATELIER;

        st_render_face_thumb(ctx, st->night_face_img, &st->night_face_buf, size, face);
        st->night_face_shown = face;
        st->night_face_key = st_face_key(ctx, face);
    }

    if (st->night_bright_slider != NULL && !lv_slider_is_dragged(st->night_bright_slider)) {
        kit_slider_set(st->night_bright_slider, ui_bright);
    }
    snprintf(text, sizeof(text), "%d%%", ui_bright);
    kit_label_set_text_if_changed(st->night_bright_value, text);
    if (st->night_gauge != NULL && (force || st->night_bright_shown != ui_bright)) {
        lv_obj_invalidate(st->night_gauge);
    }
    st->night_bright_shown = (int16_t)ui_bright;

    if (force || !st->night_valid || st->night_enabled_shown != nm->enabled) {
        apply_enabled(ctx, nm->enabled);
        st->night_enabled_shown = nm->enabled;
    }
    st->night_valid = true;
}

/* --- Schedule editor ------------------------------------------------------------------- */

static void sched_values(const clock_ui_context_t *ctx, uint8_t which, uint8_t *hour, uint8_t *minute)
{
    const night_mode_config_t *nm = &ctx->settings->night_mode;

    *hour = which == 0 ? nm->start_hour : nm->end_hour;
    *minute = which == 0 ? nm->start_minute : nm->end_minute;
}

static void sched_seg_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);
    int sel = kit_segmented_get(lv_event_get_target(e));

    ST(ctx)->sched_which = sel == 1 ? 1 : 0;
    st_schedule_sync(ctx, true);
}

static void sched_roller_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);
    clock_ui_settings_state_t *st = ST(ctx);
    const night_mode_config_t *nm = &ctx->settings->night_mode;
    uint8_t h = (uint8_t)lv_roller_get_selected(st->sched_hour);
    uint8_t m = (uint8_t)lv_roller_get_selected(st->sched_minute);

    if (ctx->suppress_events) {
        return;
    }
    if (st->sched_which == 0) {
        request_set_night_schedule(ctx, h, m, nm->end_hour, nm->end_minute);
    } else {
        request_set_night_schedule(ctx, nm->start_hour, nm->start_minute, h, m);
    }
    st_schedule_sync(ctx, false);
}

void st_schedule_build(clock_ui_context_t *ctx)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_settings_state_t *st = ST(ctx);
    const char *labels[2] = {"", ""};
    kit_page_t *p;
    lv_obj_t *row;
    lv_obj_t *b;
    lv_obj_t *label;
    int32_t w = ui_skin_is_orbit() ? 170 : 160;

    p = st_page_begin(ctx, UI_ST_SCHEDULE, ui_skin_is_orbit() ? ST_NONE : ST_ID(L10N_NIGHT_MODE), ST_ID(L10N_NIGHT_SCHEDULE),
                      KIT_NAV_BACK);
    kit_set_tint(p->nav, sk->tint_night);
    st->sched_seg = kit_segmented(p->content, labels, 2, 0);
    kit_set_tint(st->sched_seg, sk->tint_night);
    lv_obj_add_event_cb(st->sched_seg, sched_seg_cb, LV_EVENT_VALUE_CHANGED, ctx);

    row = st_hbox(p->content, 8);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(row, sk->gap_m, 0);
    st->sched_hour = kit_roller(row, ctx->hour_options, 3, sk->font_time_l);
    lv_obj_set_width(st->sched_hour, w);
    label = kit_label(row, KIT_TEXT_TIME_L, ":");
    lv_obj_set_style_text_color(label, sk->text_3, 0);
    st->sched_minute = kit_roller(row, ctx->minute_options, 3, sk->font_time_l);
    lv_obj_set_width(st->sched_minute, w);
    for (int i = 0; i < 2; i++) {
        lv_obj_t *r = i == 0 ? st->sched_hour : st->sched_minute;

        st_keep_gestures(r);
        lv_obj_add_event_cb(r, sched_roller_cb, LV_EVENT_VALUE_CHANGED, ctx);
    }

    row = st_hbox(p->content, 0);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(row, sk->gap_m, 0);
    b = kit_button(row, KIT_BUTTON_PRIMARY, KIT_ICON_CHECK, "");
    st_bind(ctx, lv_obj_get_child(b, -1), ST_ID(L10N_DONE));
    lv_obj_set_style_min_width(b, 200, 0);
    kit_set_tint(b, sk->tint_night);
    lv_obj_add_event_cb(b, st_back_event_cb, LV_EVENT_CLICKED, ctx);
}

void st_schedule_sync(clock_ui_context_t *ctx, bool force)
{
    clock_ui_settings_state_t *st = ST(ctx);
    const night_mode_config_t *nm = &ctx->settings->night_mode;
    char text[48];
    uint8_t h;
    uint8_t m;

    if (!st->built[UI_ST_SCHEDULE]) {
        return;
    }
    snprintf(text, sizeof(text), "%s  %02u:%02u", ui_l10n_str(ctx, &s_starts), nm->start_hour, nm->start_minute);
    kit_segmented_set_label(st->sched_seg, 0, text);
    snprintf(text, sizeof(text), "%s  %02u:%02u", ui_l10n_str(ctx, &s_ends), nm->end_hour, nm->end_minute);
    kit_segmented_set_label(st->sched_seg, 1, text);
    kit_segmented_set(st->sched_seg, st->sched_which);
    if (!force) {
        return;
    }
    sched_values(ctx, st->sched_which, &h, &m);
    ctx->suppress_events = true;
    lv_roller_set_selected(st->sched_hour, h, LV_ANIM_OFF);
    lv_roller_set_selected(st->sched_minute, m, LV_ANIM_OFF);
    ctx->suppress_events = false;
}

void st_schedule_open(clock_ui_context_t *ctx, uint8_t which)
{
    ST(ctx)->sched_which = which;
    st_show_page(ctx, UI_ST_SCHEDULE);
}

/* --- Night face picker ----------------------------------------------------------------- */

static int32_t picker_thumb(void)
{
    const ui_skin_t *sk = ui_skin();
    int32_t col = (sk->content_w - 2 * sk->gap_s) / 3;

    return LV_MIN(col - 20, 128);
}

static void face_pick_cb(lv_event_t *e)
{
    ui_st_item_t *item = lv_event_get_user_data(e);
    clock_ui_context_t *ctx = item->ui;

    if (!face_catalog_is_enabled((clock_face_id_t)item->index)) {
        return;
    }
    request_set_night_face(ctx, (clock_face_id_t)item->index);
    st_face_sync(ctx, false);
    st_back(ctx);
}

void st_face_build(clock_ui_context_t *ctx)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_settings_state_t *st = ST(ctx);
    kit_page_t *p;
    lv_obj_t *grid;
    int visible = face_catalog_visible_count();
    int32_t thumb = picker_thumb();
    int32_t col = (sk->content_w - 2 * sk->gap_s) / 3;

    p = st_page_begin(ctx, UI_ST_FACE, ui_skin_is_orbit() ? ST_NONE : ST_ID(L10N_NIGHT_MODE), ST_ID(L10N_NIGHT_FACE),
                      KIT_NAV_BACK);
    kit_set_tint(p->nav, sk->tint_night);
    grid = lv_obj_create(p->content);
    lv_obj_remove_style_all(grid);
    lv_obj_remove_flag(grid, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(grid, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(grid, sk->gap_s, 0);
    lv_obj_set_style_pad_row(grid, sk->gap_m, 0);
    lv_obj_set_style_pad_top(grid, sk->gap_s, 0);

    for (int i = 0; i < visible && i < CLOCK_FACE_COUNT; i++) {
        clock_face_id_t face = face_catalog_visible_index_to_id(i);
        lv_obj_t *item = lv_obj_create(grid);
        lv_obj_t *ring;
        lv_obj_t *check;

        lv_obj_remove_style_all(item);
        lv_obj_remove_flag(item, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(item, col, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(item, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(item, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(item, 10, 0);
        kit_make_pressable(item);
        st->face_item[face].ui = ctx;
        st->face_item[face].index = (uint8_t)face;
        lv_obj_add_event_cb(item, face_pick_cb, LV_EVENT_CLICKED, &st->face_item[face]);

        st->face_img[face] = face_thumb(item, thumb, 7, sk->line);
        ring = lv_obj_get_parent(st->face_img[face]);
        st->face_ring[face] = ring;
        check = lv_obj_create(ring);
        lv_obj_remove_style_all(check);
        lv_obj_remove_flag(check, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(check, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_size(check, 34, 34);
        lv_obj_set_style_radius(check, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(check, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(check, ui_skin_is_orbit() ? sk->tint_night : sk->accent, 0);
        lv_obj_set_style_border_width(check, 3, 0);
        lv_obj_set_style_border_color(check, sk->bg, 0);
        lv_obj_align(check, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
        lv_obj_center(kit_icon_small(check, KIT_ICON_CHECK, ui_skin_is_orbit() ? sk->text : sk->accent_text));
        st->face_check[face] = check;
        st->face_name[face] = kit_label(item, KIT_TEXT_BODY, face_catalog_name(face));
        lv_obj_set_width(st->face_name[face], lv_pct(100));
        lv_obj_set_style_text_align(st->face_name[face], LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(st->face_name[face], LV_LABEL_LONG_MODE_DOTS);
    }
    memset(st->face_key, 0, sizeof(st->face_key));
}

/* What a face's thumbnail shows: the minute, the face's theme, the language of its date. */
uint32_t st_face_key(const clock_ui_context_t *ctx, clock_face_id_t face)
{
    uint32_t minute = (uint32_t)(time(NULL) / 60);

    return (minute & 0x1FFFFFFu) | (uint32_t)(ctx->settings->face_themes[face] & 7u) << 25 |
           (uint32_t)(ctx->settings->language & 7u) << 28 | 1u << 31;
}

/* The thumbnails not yet drawn, or gone stale (a new minute, a theme), are drawn one a tick
 * while the picker shows: opening it, or the minute turning, no longer waits for all eight at
 * once. */
static void face_timer_cb(lv_timer_t *timer)
{
    clock_ui_context_t *ctx = lv_timer_get_user_data(timer);
    clock_ui_settings_state_t *st = ST(ctx);

    if (st->built[UI_ST_FACE] && st->open && !lv_obj_has_flag(st->pages[UI_ST_FACE].root, LV_OBJ_FLAG_HIDDEN)) {
        for (int face = 0; face < CLOCK_FACE_COUNT; face++) {
            if (st->face_img[face] != NULL && st->face_key[face] != st->face_want[face]) {
                st_render_face_thumb(ctx, st->face_img[face], &st->face_buf[face], picker_thumb(),
                                     (clock_face_id_t)face);
                st->face_key[face] = st->face_want[face];
                return;
            }
        }
    }
    lv_timer_pause(timer);
}

void st_face_sync(clock_ui_context_t *ctx, bool force)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_settings_state_t *st = ST(ctx);
    clock_face_id_t selected = sanitize_enabled_face(ctx->settings->night_mode.face);
    lv_color_t accent = ui_skin_is_orbit() ? sk->tint_night : sk->accent;
    bool stale = false;

    LV_UNUSED(force);
    if (!st->built[UI_ST_FACE]) {
        return;
    }
    for (int face = 0; face < CLOCK_FACE_COUNT; face++) {
        bool on = face == (int)selected;

        if (st->face_ring[face] == NULL) {
            continue;
        }
        lv_obj_set_style_border_width(st->face_ring[face], on ? 3 : 1, 0);
        lv_obj_set_style_border_color(st->face_ring[face], on ? accent : sk->line, 0);
        kit_show(st->face_check[face], on);
        lv_obj_set_style_text_color(st->face_name[face], on ? sk->text : sk->text_2, 0);
        st->face_want[face] = st_face_key(ctx, (clock_face_id_t)face);
        if (on && st->face_key[face] != st->face_want[face]) {
            /* The chosen face's at once; the others follow a tick apart (eight together held
             * the first open for over half a second on the clock). */
            st_render_face_thumb(ctx, st->face_img[face], &st->face_buf[face], picker_thumb(), (clock_face_id_t)face);
            st->face_key[face] = st->face_want[face];
        }
        stale = stale || st->face_key[face] != st->face_want[face];
    }
    if (stale) {
        if (st->face_timer == NULL) {
            st->face_timer = lv_timer_create(face_timer_cb, 30, ctx);
        }
        if (st->face_timer != NULL) {
            lv_timer_resume(st->face_timer);
        }
    }
}

/* --- Orbit brightness page ------------------------------------------------------------- */

void st_night_bright_build(clock_ui_context_t *ctx)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_settings_state_t *st = ST(ctx);
    kit_page_t *p;
    lv_obj_t *row;

    p = st_page_begin(ctx, UI_ST_NIGHT_BRIGHT, ui_skin_is_orbit() ? ST_NONE : ST_ID(L10N_NIGHT_MODE), ST_ID(L10N_BRIGHTNESS),
                      KIT_NAV_BACK);
    kit_set_tint(p->nav, sk->tint_night);
    st->nb_value = kit_label(p->content, KIT_TEXT_TIME_M, "");
    lv_obj_set_width(st->nb_value, lv_pct(100));
    lv_obj_set_style_text_align(st->nb_value, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_ver(st->nb_value, sk->gap_m, 0);
    row = st_hbox(p->content, 16);
    lv_obj_set_width(row, lv_pct(100));
    kit_icon_small(row, KIT_ICON_SUN, sk->text_2);
    st->nb_slider = kit_slider(row, 0, 100, sk->tint_night);
    lv_obj_set_flex_grow(st->nb_slider, 1);
    kit_icon(row, KIT_ICON_SUN, sk->text);
    st_keep_gestures(st->nb_slider);
    lv_obj_add_event_cb(st->nb_slider, brightness_cb, LV_EVENT_VALUE_CHANGED, ctx);
    kit_label(p->content, KIT_TEXT_BODY, "");
    st_bind(ctx, lv_obj_get_child(p->content, -1), ST_ID(L10N_MAIN_SLIDER_MATCH));
    lv_obj_set_width(lv_obj_get_child(p->content, -1), lv_pct(100));
    lv_obj_set_style_text_align(lv_obj_get_child(p->content, -1), LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(lv_obj_get_child(p->content, -1), sk->gap_m, 0);
}

void st_night_bright_sync(clock_ui_context_t *ctx)
{
    clock_ui_settings_state_t *st = ST(ctx);
    int v = brightness_hw_to_ui(ctx->settings->night_mode.brightness);
    char text[16];

    if (!st->built[UI_ST_NIGHT_BRIGHT]) {
        return;
    }
    if (!lv_slider_is_dragged(st->nb_slider)) {
        kit_slider_set(st->nb_slider, v);
    }
    snprintf(text, sizeof(text), "%d%%", v);
    kit_label_set_text_if_changed(st->nb_value, text);
}

void st_night_destroy(clock_ui_context_t *ctx)
{
    clock_ui_settings_state_t *st = ST(ctx);

    if (st->face_timer != NULL) {
        lv_timer_delete(st->face_timer);
        st->face_timer = NULL;
    }
    if (st->night_face_buf != NULL) {
        lv_draw_buf_destroy(st->night_face_buf);
        st->night_face_buf = NULL;
    }
    for (int i = 0; i < CLOCK_FACE_COUNT; i++) {
        if (st->face_buf[i] != NULL) {
            lv_draw_buf_destroy(st->face_buf[i]);
            st->face_buf[i] = NULL;
        }
    }
}
