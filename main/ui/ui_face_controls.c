/*
 * Controls over the face, shown for a few seconds after a tap (screen 01 of the designs):
 * pull hints for the theme sheet (12 o'clock) and quick actions (6 o'clock), the Settings
 * and Alarms entries, the page dots, the next alarm and the update badge.
 *
 * One full-screen layer holds everything. Its own background is the face dim (the face
 * shows at 85 %), and its draw callback paints the rim decorations of the style (Instrument
 * tabs, Orbit bands, the Atelier bezel) under the controls. The controls come in as a sweep
 * round the dial from twelve o'clock: each springs in from the rim, growing and fading in,
 * as the sweep passes it (the page dots one by one). They leave with a quick fade when they
 * time out or are tapped away; anything else that hides them does so at once.
 *
 * Positions follow the designs and keep clear of the numerals and logos of the faces
 * (checked on face_0 digital, face_1 matrix, face_5 Sternglas and face_7 Georg Jensen).
 */
#include <math.h>
#include <string.h>

#include "domain/face_catalog.h"
#include "ui/ui_areas.h"
#include "ui/ui_face_chrome.h"
#include "ui/ui_format.h"
#include "ui/kit/ui_glass.h"

#define CONTROLS_VISIBLE_MS 4000
#define CONTROLS_SWEEP_MS 240 /* the entrance passes round the dial in this time */
#define CONTROLS_IN_MS 340    /* and each control takes this to come in */
#define CONTROLS_OUT_SWEEP_MS 60
#define CONTROLS_OUT_MS 170
#define CONTROLS_GLIDE 28     /* px a control springs in from the rim */
/* No dim under the controls: a full-screen translucent fill costs about 10 ms on every frame
 * they are visible, including every frame of a face swipe, and the plates read well without. */
#define CONTROLS_DIM_OPA LV_OPA_TRANSP

static const ui_l10n_str_t s_actions = UI_L10N_STR("Actions", "Akcje", "Дії", "Действия", "Acties");
static const ui_l10n_str_t s_update_version = UI_L10N_STR("Update %s", "Aktualizacja %s", "Оновлення %s",
                                                          "Обновление %s", "Update %s");
static const ui_l10n_str_t s_update_available = UI_L10N_STR("Update available", "Dostępna aktualizacja",
                                                            "Доступне оновлення", "Доступно обновление",
                                                            "Update beschikbaar");
static const ui_l10n_str_t s_compact_hm = UI_L10N_STR("%dh%02dm", "%dh%02dm", "%dг%02dхв", "%dч%02dм", "%du%02dm");
static const ui_l10n_str_t s_compact_m = UI_L10N_STR("%dm", "%dm", "%dхв", "%dм", "%dm");
static const ui_l10n_str_t s_compact_d = UI_L10N_STR("%dd", "%dd", "%dдн", "%dдн", "%dd");

/* --- Shared helpers --------------------------------------------------------------------- */

time_t face_now(void)
{
    return time(NULL);
}

lv_obj_t *face_plain(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);

    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE);
    return obj;
}

lv_obj_t *face_text(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_label_set_text(label, text != NULL ? text : "");
    return label;
}

const char *face_caps(char *buf, size_t size, const char *text)
{
    if (!ui_skin()->label_caps) {
        return text;
    }
    kit_text_upper(buf, size, text);
    return buf;
}

void face_fit_label(lv_obj_t *label, int32_t max_w, int32_t letter_space)
{
    const lv_font_t *font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
    const char *text = lv_label_get_text(label);
    lv_point_t size;

    for (; letter_space > 0; letter_space--) {
        lv_text_get_size(&size, text, font, letter_space, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (size.x <= max_w) {
            break;
        }
    }
    if (lv_obj_get_style_text_letter_space(label, LV_PART_MAIN) != letter_space) {
        lv_obj_set_style_text_letter_space(label, letter_space, 0);
    }
}

static void press_point_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(e);
    lv_indev_t *indev = lv_event_get_indev(e);

    if (indev != NULL) {
        lv_indev_get_point(indev, &ctx->face_chrome.press_point);
    }
}

void face_track_press(clock_ui_context_t *ctx, lv_obj_t *obj)
{
    lv_obj_add_event_cb(obj, press_point_cb, LV_EVENT_PRESSED, ctx);
}

bool face_click_is_tap(const clock_ui_context_t *ctx, lv_event_t *e)
{
    lv_indev_t *indev = lv_event_get_indev(e);
    lv_point_t p;

    if (indev == NULL) {
        return true;
    }
    lv_indev_get_point(indev, &p);
    return LV_ABS(p.x - ctx->face_chrome.press_point.x) < 24 && LV_ABS(p.y - ctx->face_chrome.press_point.y) < 24;
}

static void stop_bubble_cb(lv_event_t *e)
{
    lv_event_stop_bubbling(e);
}

void face_draw_rim_arc(lv_layer_t *layer, lv_point_t c, int32_t radius, int32_t width, int32_t start, int32_t end,
                       lv_color_t color, lv_opa_t opa, bool rounded)
{
    lv_draw_arc_dsc_t d;

    lv_draw_arc_dsc_init(&d);
    d.center = c;
    d.radius = (uint16_t)radius;
    d.width = width;
    d.start_angle = (lv_value_precise_t)((start % 360 + 360) % 360);
    d.end_angle = (lv_value_precise_t)((end % 360 + 360) % 360);
    if (end - start >= 360) {
        d.start_angle = 0;
        d.end_angle = 360;
    }
    d.color = color;
    d.opa = opa;
    d.rounded = rounded ? 1 : 0;
    kit_draw_arc(layer, &d);
}

void face_draw_tick(lv_layer_t *layer, lv_point_t ctr, int32_t r0, int32_t r1, float angle_deg, int32_t width,
                    lv_color_t color)
{
    lv_draw_line_dsc_t d;
    float a = angle_deg * 3.14159265f / 180.0f;
    float c = cosf(a);
    float s = sinf(a);

    lv_draw_line_dsc_init(&d);
    d.color = color;
    d.width = width;
    d.p1.x = (lv_value_precise_t)(ctr.x + r0 * c);
    d.p1.y = (lv_value_precise_t)(ctr.y + r0 * s);
    d.p2.x = (lv_value_precise_t)(ctr.x + r1 * c);
    d.p2.y = (lv_value_precise_t)(ctr.y + r1 * s);
    lv_draw_line(layer, &d);
}

void face_draw_dot(lv_layer_t *layer, int32_t x, int32_t y, int32_t r, lv_color_t color, lv_opa_t opa)
{
    lv_draw_rect_dsc_t d;
    lv_area_t a = {x - r, y - r, x + r - 1, y + r - 1};

    lv_draw_rect_dsc_init(&d);
    d.bg_color = color;
    d.bg_opa = opa;
    d.radius = LV_RADIUS_CIRCLE;
    lv_draw_rect(layer, &d, &a);
}

lv_obj_t *face_engraved_rule(lv_obj_t *parent, int32_t y, int32_t w)
{
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *rule = face_plain(parent);
    lv_obj_t *lit = face_plain(rule);
    lv_obj_t *lozenge;

    /* Two 1 px lines: a dark cut and its lit edge below. */
    lv_obj_set_size(rule, w, 2);
    lv_obj_align(rule, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_bg_color(rule, lv_color_mix(sk->bg, lv_color_black(), 96), 0);
    lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, 0);
    lv_obj_set_size(lit, w, 1);
    lv_obj_set_pos(lit, 0, 1);
    lv_obj_set_style_bg_color(lit, sk->line, 0);
    lv_obj_set_style_bg_opa(lit, LV_OPA_COVER, 0);

    lozenge = lv_image_create(parent);
    lv_image_set_src(lozenge, kit_image_src(KIT_IMAGE_LOZENGE));
    lv_obj_remove_flag(lozenge, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(lozenge, LV_ALIGN_TOP_MID, 0, y + 1 - (int32_t)kit_image_src(KIT_IMAGE_LOZENGE)->header.h / 2);
    return rule;
}

bool face_next_alarm(const clock_ui_context_t *ctx, time_t now, time_t *when)
{
    return ui_next_alarm(ctx, now, when, NULL);
}

void face_format_countdown_compact(const clock_ui_context_t *ctx, time_t now, time_t when, char *out, size_t size)
{
    long minutes = when > now ? (long)((when - now + 59) / 60) : 0;

    if (minutes >= 48 * 60) {
        snprintf(out, size, ui_l10n_str(ctx, &s_compact_d), (int)(minutes / (24 * 60)));
    } else if (minutes >= 60) {
        snprintf(out, size, ui_l10n_str(ctx, &s_compact_hm), (int)(minutes / 60), (int)(minutes % 60));
    } else {
        snprintf(out, size, ui_l10n_str(ctx, &s_compact_m), (int)(minutes < 1 ? 1 : minutes));
    }
}

/* The day word of ui_format_day_time ("Tomorrow" of "Tomorrow 06:30"). */
static void format_day_word(const clock_ui_context_t *ctx, time_t now, time_t when, char *out, size_t size)
{
    char day_time[48];
    char clock[8];
    size_t n;
    size_t c;

    ui_format_day_time(ctx, now, when, day_time, sizeof(day_time));
    ui_format_clock(when, clock, sizeof(clock));
    n = strlen(day_time);
    c = strlen(clock);
    if (n > c + 1 && strcmp(day_time + n - c, clock) == 0) {
        day_time[n - c - 1] = '\0';
    }
    snprintf(out, size, "%s", day_time);
}

/* --- Layer drawing ---------------------------------------------------------------------- */

static int8_t visible_index(clock_face_id_t face)
{
    int index = face_catalog_visible_id_to_index(face);

    return (int8_t)(index < 0 ? 0 : index);
}

/* Where a control at a clock angle (0 at twelve, clockwise) is in the entrance or exit:
 * 1 in place, 0 gone. */
static float reveal_x(const clock_ui_face_chrome_state_t *fc, float clock_deg)
{
    float t = (float)fc->reveal_ms;

    switch (fc->reveal_mode) {
    case 1:
        return LV_CLAMP(0.0f, (t - clock_deg / 360.0f * CONTROLS_SWEEP_MS) / CONTROLS_IN_MS, 1.0f);
    case 2:
        return 1.0f - LV_CLAMP(0.0f, (t - clock_deg / 360.0f * CONTROLS_OUT_SWEEP_MS) / CONTROLS_OUT_MS, 1.0f);
    default:
        return 1.0f;
    }
}

/* Ease-out with a small overshoot (about 8 %), for the spring in. */
static float reveal_back_out(float x)
{
    const float c1 = 1.4f;
    const float c3 = c1 + 1.0f;
    float u = x - 1.0f;

    return 1.0f + c3 * u * u * u + c1 * u * u;
}

static lv_opa_t reveal_opa(const clock_ui_face_chrome_state_t *fc, float x)
{
    float a = fc->reveal_mode == 1 ? LV_MIN(1.0f, x * 1.8f) : x;

    return (lv_opa_t)lroundf(a * 255.0f);
}

/* How far from its place a control still is, towards the rim. */
static float reveal_glide(const clock_ui_face_chrome_state_t *fc, float x)
{
    return fc->reveal_mode == 1 ? (1.0f - reveal_back_out(x)) * CONTROLS_GLIDE
                                : (1.0f - x) * (1.0f - x) * CONTROLS_GLIDE * 0.6f;
}

static float reveal_scale(const clock_ui_face_chrome_state_t *fc, float x)
{
    return fc->reveal_mode == 1 ? 0.75f + 0.25f * reveal_back_out(x) : 0.92f + 0.08f * x;
}

/* The clock angle of a point (0 at twelve, clockwise). Just left of twelve counts as twelve,
 * where the sweep starts, not as the end of the turn. */
static float clock_angle(float dx, float dy)
{
    float deg = atan2f(dx, -dy) * 180.0f / 3.14159265f;

    if (deg < 0.0f) {
        deg += 360.0f;
    }
    return deg > 350.0f ? 0.0f : deg;
}

static void draw_arc_dots(lv_layer_t *layer, const clock_ui_face_chrome_state_t *fc, float radius,
                          float step_deg, lv_color_t off, int32_t off_r, lv_color_t on, int32_t on_r, bool on_ring)
{
    float first = 90.0f + step_deg * (float)(fc->dot_count - 1) / 2.0f;

    for (int i = 0; i < fc->dot_count; i++) {
        float a = (first - step_deg * (float)i) * 3.14159265f / 180.0f;
        int32_t x = (int32_t)lroundf(KIT_CENTER + radius * cosf(a));
        int32_t y = (int32_t)lroundf(KIT_CENTER + radius * sinf(a));

        float dx = (float)(x - KIT_CENTER);
        float dy = (float)(y - KIT_CENTER);
        float rx = reveal_x(fc, clock_angle(dx, dy));
        float grow = fc->reveal_mode == 1 ? reveal_back_out(rx) : 1.0f;
        lv_opa_t opa = reveal_opa(fc, rx);

        if (opa <= LV_OPA_MIN) {
            continue;
        }
        if (i == fc->dot_active) {
            face_draw_dot(layer, x, y, LV_MAX(1, (int32_t)lroundf((float)on_r * grow)), on, opa);
            if (on_ring) {
                lv_draw_arc_dsc_t d;

                lv_draw_arc_dsc_init(&d);
                d.center.x = x;
                d.center.y = y;
                d.radius = (uint16_t)(on_r + 4);
                d.width = 1;
                d.start_angle = 0;
                d.end_angle = 360;
                d.color = on;
                d.opa = (lv_opa_t)(opa / 2);
                kit_draw_arc(layer, &d);
            }
        } else {
            face_draw_dot(layer, x, y, LV_MAX(1, (int32_t)lroundf((float)off_r * grow)), off, opa);
        }
    }
}

static void draw_rim_bands(lv_layer_t *layer, clock_ui_face_chrome_state_t *fc, int32_t r_mid, int32_t width,
                           float half_span, lv_color_t fill, lv_opa_t fill_opa, lv_color_t edge)
{
    lv_draw_image_dsc_t img;

    /* Pre-rendered once: wide round-capped arcs cost a per-row angle mask every frame. In Liquid
     * Glass they are smoked glass like the other face controls, holding nothing of the face. */
    for (int i = 0; i < 2; i++) {
        if (fc->rim_band[i] == NULL && ui_skin_is_glass()) {
            float mid = i == 0 ? 270.0f : 90.0f;

            fc->rim_band[i] = ui_glass_arc_shell(KIT_CENTER, KIT_CENTER, r_mid, width, mid - half_span, mid + half_span,
                                                 UI_GLASS_RAISED, lv_color_black(), &fc->rim_band_area[i]);
        }
        if (fc->rim_band[i] == NULL && !ui_skin_is_glass()) {
            float mid = i == 0 ? 270.0f : 90.0f;

            fc->rim_band[i] = kit_arc_band_image(KIT_CENTER, KIT_CENTER, r_mid, width, mid - half_span, mid + half_span,
                                                 fill, fill_opa, edge, LV_OPA_COVER, &fc->rim_band_area[i]);
        }
        if (fc->rim_band[i] != NULL) {
            lv_area_t area = fc->rim_band_area[i];
            float rx = reveal_x(fc, i == 0 ? 0.0f : 180.0f);

            /* The bands spring down (12) and up (6) from the rim. */
            lv_area_move(&area, 0, (i == 0 ? -1 : 1) * (int32_t)lroundf(reveal_glide(fc, rx)));
            lv_draw_image_dsc_init(&img);
            img.src = fc->rim_band[i];
            img.opa = reveal_opa(fc, rx);
            lv_draw_image(layer, &img, &area);
        }
    }
}

static void draw_instrument_rim(lv_layer_t *layer, clock_ui_face_chrome_state_t *fc)
{
    const ui_skin_t *sk = ui_skin();

    /* Pull tabs riding the bezel at 12 and 6: 36 px with an edge, rounded ends. */
    draw_rim_bands(layer, fc, 339, 36, 13.0f, sk->bg, 235, sk->line_strong);
}

static lv_color_t orbit_float_bg(void)
{
    const ui_skin_t *sk = ui_skin();

    if (sk->glass) {
        return sk->raised; /* raised glass */
    }
    return lv_color_mix(sk->raised, sk->surface, 128);
}

static void draw_orbit_rim(lv_layer_t *layer, const clock_ui_face_chrome_state_t *fc)
{
    const ui_skin_t *sk = ui_skin();

    /* Bands hugging the rim at 12 and 6 (r 322, 40 px with a 1 px rim). */
    draw_rim_bands(layer, (clock_ui_face_chrome_state_t *)fc, 322, 42, 12.0f, orbit_float_bg(), LV_OPA_COVER,
                   sk->line_strong);
    draw_arc_dots(layer, fc, 334.0f, 2.57f, lv_color_mix(sk->text_2, sk->text_3, 96), 4, sk->text, 6, false);
}

static bool atelier_tick_skipped(int i)
{
    static const int8_t skip[] = {0, 1, 59, 7, 8, 52, 53, 28, 29, 30, 31, 32};

    for (size_t k = 0; k < sizeof(skip); k++) {
        if (skip[k] == i) {
            return true;
        }
    }
    return false;
}

static void atelier_bezel_spec(kit_dial_t *dial)
{
    const ui_skin_t *sk = ui_skin();

    /* The ink bezel lifted over the dial's rim, its brass inner edge, outer hairline and minute
     * track, baked once into cached tiles. The band is opaque: its body is then filled
     * rather than blended over the face, about 146k pixels on every frame the controls show. */
    memset(dial, 0, sizeof(*dial));
    dial->cx = KIT_CENTER;
    dial->cy = KIT_CENTER;
    dial->rings[0] = (kit_dial_ring_t) {.r = 324, .width = 72, .end_deg = 360, .color = sk->bg, .opa = LV_OPA_COVER};
    dial->rings[1] = (kit_dial_ring_t) {.r = 289, .width = 1, .end_deg = 360, .color = sk->label, .opa = 230};
    dial->rings[2] = (kit_dial_ring_t) {.r = 357, .width = 1, .end_deg = 360, .color = sk->accent_deep, .opa = 180};
    dial->ticks[0] = (kit_dial_ticks_t) {
        .count = 60,
        .offset_deg = -90.0f,
        .r_in = 291,
        .r_out = 297,
        .major_every = 5,
        .r_in_major = 291,
        .r_out_major = 303,
        .width = 1,
        .width_major = 2,
        .color = sk->accent_deep,
        .color_major = sk->label,
        .opa = LV_OPA_COVER,
    };
    for (int i = 0; i < 60; i++) {
        if (atelier_tick_skipped(i)) {
            dial->ticks[0].skip |= 1ull << i;
        }
    }
}

static void draw_atelier_rim(lv_layer_t *layer, const clock_ui_face_chrome_state_t *fc)
{
    const ui_skin_t *sk = ui_skin();
    kit_dial_t dial;

    atelier_bezel_spec(&dial);
    /* The bezel leads the sweep in, and fades. */
    kit_dial_draw_opa(layer, &dial, (lv_opa_t)lroundf(LV_MIN(1.0f, reveal_x(fc, 0.0f) * 1.5f) * 255.0f));
    draw_arc_dots(layer, fc, 335.0f, 4.6f, sk->text_2, 4, sk->accent, 6, true);
}

static void layer_draw_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(e);
    lv_layer_t *layer = lv_event_get_layer(e);

    switch (ui_skin_style()) {
    case CLOCK_UI_STYLE_INSTRUMENT:
        draw_instrument_rim(layer, &ctx->face_chrome);
        break;
    case CLOCK_UI_STYLE_ATELIER:
        draw_atelier_rim(layer, &ctx->face_chrome);
        break;
    case CLOCK_UI_STYLE_ORBIT:
    default:
        draw_orbit_rim(layer, &ctx->face_chrome);
        break;
    }
}

/* --- Events ----------------------------------------------------------------------------- */

static void controls_leave(clock_ui_context_t *ctx);

static void hide_timer_cb(lv_timer_t *timer)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_timer_get_user_data(timer);

    controls_leave(ctx);
}

static void settings_click_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(e);

    if (!face_click_is_tap(ctx, e)) {
        return;
    }
    face_controls_hide(ctx);
    ui_settings_open(ctx);
}

static void alarms_click_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(e);

    if (!face_click_is_tap(ctx, e)) {
        return;
    }
    face_controls_hide(ctx);
    ui_alarms_open(ctx);
}

/* The update badge opens Settings › Other, where the update shows its version and Install. */
static void update_click_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(e);

    if (!face_click_is_tap(ctx, e)) {
        return;
    }
    face_controls_hide(ctx);
    ui_settings_open_page(ctx, UI_SETTINGS_PAGE_OTHER);
}

/* Instrument readout / Orbit status card: the update when one is shown, else the alarms. */
static void status_click_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(e);

    if (!face_click_is_tap(ctx, e)) {
        return;
    }
    if (ctx->face_chrome.update_shown) {
        update_click_cb(e);
    } else {
        alarms_click_cb(e);
    }
}

static void make_clickable(lv_obj_t *obj, lv_event_cb_t cb, clock_ui_context_t *ctx)
{
    kit_make_pressable(obj);
    face_track_press(ctx, obj);
    lv_obj_add_event_cb(obj, cb, LV_EVENT_CLICKED, ctx);
}

/* --- Building --------------------------------------------------------------------------- */

static lv_obj_t *tab_label(clock_ui_context_t *ctx, lv_obj_t *parent, const ui_l10n_str_t *str, l10n_text_id_t id,
                           lv_color_t color)
{
    lv_obj_t *label = kit_label(parent, KIT_TEXT_LABEL, "");

    lv_obj_set_style_text_color(label, color, 0);
    if (str != NULL) {
        ui_l10n_bind_kit_str(ctx, label, str);
    } else {
        ui_l10n_bind_kit(ctx, label, id);
    }
    return label;
}

static lv_obj_t *row_flex(lv_obj_t *parent, int32_t gap)
{
    lv_obj_t *row = face_plain(parent);

    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, gap, 0);
    lv_obj_add_flag(row, LV_OBJ_FLAG_EVENT_BUBBLE);
    return row;
}

/* The themes tab: a tap opens the sheet, a pull drags it (routed to the face swipe). */
static lv_obj_t *themes_tab(clock_ui_context_t *ctx, int32_t x, int32_t y, int32_t w, int32_t h)
{
    lv_obj_t *tab = face_plain(ctx->face_chrome.layer);

    lv_obj_set_pos(tab, x, y);
    lv_obj_set_size(tab, w, h);
    lv_obj_add_flag(tab, LV_OBJ_FLAG_CLICKABLE);
    ui_attach_click_feedback(tab, LV_EVENT_CLICKED);
    ctx->face_chrome.themes_tab = tab;
    return tab;
}

static void build_instrument(clock_ui_context_t *ctx)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_face_chrome_state_t *fc = &ctx->face_chrome;
    lv_obj_t *tab;
    lv_obj_t *row;
    lv_obj_t *label;
    lv_obj_t *led;

    /* Pull tabs: legends printed on the bezel tabs. */
    tab = themes_tab(ctx, 260, 0, 200, 56);
    row = row_flex(tab, 6);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 9);
    kit_icon_small(row, KIT_ICON_CHEVRON_DOWN, sk->text_2);
    tab_label(ctx, row, NULL, L10N_THEMES, sk->text_2);

    fc->actions_tab = row_flex(fc->layer, 6);
    lv_obj_align(fc->actions_tab, LV_ALIGN_TOP_MID, 0, 687);
    kit_icon_small(fc->actions_tab, KIT_ICON_CHEVRON_UP, sk->text_2);
    tab_label(ctx, fc->actions_tab, &s_actions, (l10n_text_id_t)0, sk->text_2);

    /* Readout plate in the empty zone under 12: next alarm and the update. */
    fc->status = face_plain(fc->layer);
    lv_obj_set_size(fc->status, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_min_width(fc->status, 176, 0);
    lv_obj_set_style_min_height(fc->status, 44, 0);
    lv_obj_set_style_radius(fc->status, 16, 0);
    lv_obj_set_style_bg_color(fc->status, sk->bg, 0);
    lv_obj_set_style_bg_opa(fc->status, 230, 0);
    lv_obj_set_style_border_width(fc->status, 1, 0);
    lv_obj_set_style_border_color(fc->status, sk->line_strong, 0);
    lv_obj_set_style_pad_hor(fc->status, 14, 0);
    lv_obj_set_style_pad_ver(fc->status, 6, 0);
    lv_obj_set_style_pad_row(fc->status, 2, 0);
    lv_obj_set_flex_flow(fc->status, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(fc->status, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_align(fc->status, LV_ALIGN_TOP_MID, 0, 138);
    lv_obj_set_ext_click_area(fc->status, 8);
    make_clickable(fc->status, status_click_cb, ctx);

    fc->status_alarm_row = row_flex(fc->status, 8);
    lv_obj_set_height(fc->status_alarm_row, 28);
    kit_icon_small(fc->status_alarm_row, KIT_ICON_ALARM, sk->text_2);
    fc->status_alarm_time = face_text(fc->status_alarm_row, sk->font_body, sk->text, "");
    fc->status_alarm_note = kit_label(fc->status_alarm_row, KIT_TEXT_LABEL, "");
    lv_obj_set_style_text_color(fc->status_alarm_note, sk->text_2, 0);
    lv_obj_set_style_text_letter_space(fc->status_alarm_note, 0, 0);

    fc->status_update_row = row_flex(fc->status, 8);
    lv_obj_set_height(fc->status_update_row, 28);
    led = face_plain(fc->status_update_row);
    lv_obj_set_size(led, 8, 8);
    lv_obj_set_style_radius(led, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(led, sk->accent, 0);
    lv_obj_set_style_bg_opa(led, LV_OPA_COVER, 0);
    lv_obj_set_style_margin_left(led, 6, 0);
    lv_obj_set_style_margin_right(led, 2, 0);
    fc->status_update_text = kit_label(fc->status_update_row, KIT_TEXT_LABEL, "");
    lv_obj_set_style_text_color(fc->status_update_text, sk->text, 0);
    lv_obj_set_style_text_letter_space(fc->status_update_text, 0, 0);

    /* Settings, page dots and Alarms on the minute track between the 5 and 7 marks. */
    fc->settings_key = kit_round_button(fc->layer, KIT_BUTTON_SECONDARY, KIT_ICON_SETTINGS, 72);
    lv_obj_set_pos(fc->settings_key, 234, 600);
    lv_obj_add_event_cb(fc->settings_key, settings_click_cb, LV_EVENT_CLICKED, ctx);
    face_track_press(ctx, fc->settings_key);
    fc->alarms_key = kit_round_button(fc->layer, KIT_BUTTON_SECONDARY, KIT_ICON_ALARM, 72);
    lv_obj_set_pos(fc->alarms_key, 414, 600);
    lv_obj_add_event_cb(fc->alarms_key, alarms_click_cb, LV_EVENT_CLICKED, ctx);
    face_track_press(ctx, fc->alarms_key);

    fc->dots_plate = face_plain(fc->layer);
    lv_obj_set_size(fc->dots_plate, LV_SIZE_CONTENT, 24);
    lv_obj_set_style_radius(fc->dots_plate, 12, 0);
    lv_obj_set_style_bg_color(fc->dots_plate, sk->bg, 0);
    lv_obj_set_style_bg_opa(fc->dots_plate, 230, 0);
    lv_obj_set_style_border_width(fc->dots_plate, 1, 0);
    lv_obj_set_style_border_color(fc->dots_plate, sk->line_strong, 0);
    lv_obj_set_style_pad_hor(fc->dots_plate, 9, 0);
    lv_obj_set_style_pad_column(fc->dots_plate, 4, 0);
    lv_obj_set_flex_flow(fc->dots_plate, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(fc->dots_plate, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(fc->dots_plate, LV_ALIGN_TOP_MID, 0, 642);
    for (int i = 0; i < fc->dot_count; i++) {
        fc->dots[i] = face_plain(fc->dots_plate);
        lv_obj_set_size(fc->dots[i], 6, 6);
        lv_obj_set_style_radius(fc->dots[i], 3, 0);
        lv_obj_set_style_bg_color(fc->dots[i], sk->text_3, 0);
        lv_obj_set_style_bg_opa(fc->dots[i], LV_OPA_COVER, 0);
    }
    LV_UNUSED(label);
}

/* Liquid Glass: the face controls are smoked glass on any dial, so their icons are white (a
 * coloured icon vanishes on a dial of its colour). */
static lv_color_t glass_ink(lv_color_t color)
{
    return ui_skin_is_glass() ? ui_skin()->text : color;
}

static lv_obj_t *orbit_float_key(clock_ui_context_t *ctx, kit_icon_t icon, lv_color_t tint, int32_t x, int32_t y,
                                 lv_event_cb_t cb)
{
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *key = kit_round_button(ctx->face_chrome.layer, KIT_BUTTON_SECONDARY, icon, 72);

    lv_obj_set_pos(key, x, y);
    lv_obj_set_style_bg_color(key, orbit_float_bg(), 0);
    lv_obj_set_style_border_width(key, 1, 0);
    lv_obj_set_style_border_color(key, sk->line_strong, 0);
    kit_button_set_tint(key, tint);
    lv_obj_add_event_cb(key, cb, LV_EVENT_CLICKED, ctx);
    face_track_press(ctx, key);
    return key;
}

static void build_orbit(clock_ui_context_t *ctx)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_face_chrome_state_t *fc = &ctx->face_chrome;
    lv_obj_t *tab;
    lv_obj_t *row;
    lv_obj_t *chev;

    tab = themes_tab(ctx, 270, 0, 180, 64);
    row = row_flex(tab, 6);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 26);
    kit_icon_small(row, KIT_ICON_CHEVRON_DOWN, sk->text_2);
    tab_label(ctx, row, NULL, L10N_THEMES, sk->text);

    fc->actions_tab = face_plain(fc->layer);
    lv_obj_set_size(fc->actions_tab, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    chev = kit_icon_small(fc->actions_tab, KIT_ICON_CHEVRON_UP, sk->text_2);
    LV_UNUSED(chev);
    lv_obj_align(fc->actions_tab, LV_ALIGN_TOP_MID, 0, 657);

    /* Alarms and Settings on the rim at 10:30 and 1:30. */
    fc->alarms_key = orbit_float_key(ctx, KIT_ICON_ALARM, glass_ink(sk->tint_alarms), 112, 112, alarms_click_cb);
    fc->settings_key = orbit_float_key(ctx, KIT_ICON_GEAR, sk->text, 536, 112, settings_click_cb);
    fc->badge = face_plain(fc->layer);
    lv_obj_set_size(fc->badge, 20, 20);
    lv_obj_set_pos(fc->badge, 536 + 72 - 19, 111);
    lv_obj_set_style_radius(fc->badge, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(fc->badge, sk->accent, 0);
    lv_obj_set_style_bg_opa(fc->badge, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(fc->badge, 3, 0);
    lv_obj_set_style_border_color(fc->badge, orbit_float_bg(), 0);

    /* One status card under the 12: next alarm and the update. */
    fc->status = face_plain(fc->layer);
    lv_obj_set_size(fc->status, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_min_width(fc->status, 200, 0);
    lv_obj_set_style_min_height(fc->status, 58, 0);
    lv_obj_set_style_radius(fc->status, 29, 0);
    lv_obj_set_style_bg_color(fc->status, orbit_float_bg(), 0);
    lv_obj_set_style_bg_opa(fc->status, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(fc->status, 1, 0);
    lv_obj_set_style_border_color(fc->status, sk->line_strong, 0);
    lv_obj_set_style_pad_left(fc->status, 14, 0);
    lv_obj_set_style_pad_right(fc->status, 18, 0);
    lv_obj_set_style_pad_ver(fc->status, 5, 0);
    lv_obj_set_flex_flow(fc->status, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(fc->status, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_align(fc->status, LV_ALIGN_TOP_MID, 0, 134);
    lv_obj_set_ext_click_area(fc->status, 8);
    make_clickable(fc->status, status_click_cb, ctx);

    fc->status_alarm_row = row_flex(fc->status, 6);
    lv_obj_set_height(fc->status_alarm_row, 24);
    kit_icon_small(fc->status_alarm_row, KIT_ICON_ALARM, glass_ink(sk->tint_alarms));
    fc->status_alarm_time = face_text(fc->status_alarm_row, sk->font_label, sk->text, "");
    fc->status_alarm_note = face_text(fc->status_alarm_row, sk->font_label, sk->text_2, "");

    fc->status_update_row = row_flex(fc->status, 6);
    lv_obj_set_height(fc->status_update_row, 24);
    kit_icon_small(fc->status_update_row, KIT_ICON_DOWNLOAD, glass_ink(sk->accent));
    fc->status_update_text = face_text(fc->status_update_row, sk->font_label, sk->text, "");
}

static void build_atelier(clock_ui_context_t *ctx)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_face_chrome_state_t *fc = &ctx->face_chrome;
    lv_obj_t *tab;
    lv_obj_t *label;
    lv_obj_t *chev;

    tab = themes_tab(ctx, 270, 6, 180, 70);
    label = tab_label(ctx, tab, NULL, L10N_THEMES, sk->value);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 14);
    chev = kit_icon_small(tab, KIT_ICON_CHEVRON_DOWN, sk->accent);
    lv_obj_align(chev, LV_ALIGN_TOP_MID, 0, 40);

    fc->actions_tab = kit_icon_small(fc->layer, KIT_ICON_CHEVRON_UP, sk->accent);
    lv_obj_align(fc->actions_tab, LV_ALIGN_TOP_MID, 0, 649);

    /* Pushers at ten and two, on the bezel. */
    fc->alarms_key = kit_round_button(fc->layer, KIT_BUTTON_SECONDARY, KIT_ICON_BELL, 64);
    lv_obj_set_pos(fc->alarms_key, 131 - 32, 131 - 32);
    lv_obj_add_event_cb(fc->alarms_key, alarms_click_cb, LV_EVENT_CLICKED, ctx);
    face_track_press(ctx, fc->alarms_key);
    fc->settings_key = kit_round_button(fc->layer, KIT_BUTTON_SECONDARY, KIT_ICON_GEAR, 64);
    lv_obj_set_pos(fc->settings_key, 589 - 32, 131 - 32);
    lv_obj_add_event_cb(fc->settings_key, settings_click_cb, LV_EVENT_CLICKED, ctx);
    face_track_press(ctx, fc->settings_key);
    fc->badge = face_plain(fc->layer);
    lv_obj_set_size(fc->badge, 18, 18);
    lv_obj_set_pos(fc->badge, 557, 100);
    lv_obj_set_style_radius(fc->badge, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(fc->badge, sk->danger, 0);
    lv_obj_set_style_bg_opa(fc->badge, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(fc->badge, 3, 0);
    lv_obj_set_style_border_color(fc->badge, sk->bg, 0);

    /* The update as a ruby-marked chip under the 12. */
    fc->update_chip = kit_chip(fc->layer, KIT_ICON_NONE, "", true);
    lv_obj_align(fc->update_chip, LV_ALIGN_TOP_MID, 0, 150);
    lv_obj_set_ext_click_area(fc->update_chip, 14);
    make_clickable(fc->update_chip, update_click_cb, ctx);

    /* The next alarm as a framed pill above the 6. */
    fc->alarm_chip = face_plain(fc->layer);
    lv_obj_set_size(fc->alarm_chip, LV_SIZE_CONTENT, 56);
    lv_obj_set_style_radius(fc->alarm_chip, 28, 0);
    lv_obj_set_style_bg_color(fc->alarm_chip, sk->surface, 0);
    lv_obj_set_style_bg_opa(fc->alarm_chip, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(fc->alarm_chip, 1, 0);
    lv_obj_set_style_border_color(fc->alarm_chip, sk->accent_deep, 0);
    lv_obj_set_style_pad_left(fc->alarm_chip, 20, 0);
    lv_obj_set_style_pad_right(fc->alarm_chip, 24, 0);
    lv_obj_set_style_pad_column(fc->alarm_chip, 12, 0);
    lv_obj_set_flex_flow(fc->alarm_chip, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(fc->alarm_chip, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(fc->alarm_chip, LV_ALIGN_TOP_MID, 0, 540);
    lv_obj_set_ext_click_area(fc->alarm_chip, 8);
    make_clickable(fc->alarm_chip, alarms_click_cb, ctx);
    kit_icon_small(fc->alarm_chip, KIT_ICON_BELL, sk->accent);
    fc->alarm_chip_time = face_text(fc->alarm_chip, sk->font_value, sk->value, "");
    lv_obj_set_style_translate_y(fc->alarm_chip_time, 1, 0);
    fc->alarm_chip_note = face_text(fc->alarm_chip, sk->font_body, sk->text_2, "");
}

void face_controls_create(clock_ui_context_t *ctx)
{
    clock_ui_face_chrome_state_t *fc = &ctx->face_chrome;

    fc->dot_count = (uint8_t)LV_MIN(face_catalog_visible_count(), CLOCK_FACE_COUNT);
    fc->dot_active = -1;
    fc->synced_minute = -1;
    fc->night_minute = -1;
    fc->active_face = sanitize_enabled_face(ctx->settings->current_face);

    fc->layer = face_plain(ui_chrome_root(ctx));
    lv_obj_set_size(fc->layer, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_color(fc->layer, lv_color_black(), 0);
    /* Instrument and Orbit draw the face at 85 % under the controls; the Atelier bezel
     * covers the rim instead and leaves the dial at full strength. */
    lv_obj_set_style_bg_opa(fc->layer, ui_skin_style() == CLOCK_UI_STYLE_ATELIER ? LV_OPA_TRANSP : CONTROLS_DIM_OPA, 0);
    lv_obj_add_flag(fc->layer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(fc->layer, layer_draw_cb, LV_EVENT_DRAW_MAIN_END, ctx);

    switch (ui_skin_style()) {
    case CLOCK_UI_STYLE_INSTRUMENT:
        build_instrument(ctx);
        break;
    case CLOCK_UI_STYLE_ATELIER: {
        kit_dial_t bezel;

        build_atelier(ctx);
        atelier_bezel_spec(&bezel);
        kit_dial_prewarm(&bezel);
        break;
    }
    case CLOCK_UI_STYLE_ORBIT:
    default:
        build_orbit(ctx);
        break;
    }

    fc->hide_timer = ui_chrome_timer_create(ctx, hide_timer_cb, CONTROLS_VISIBLE_MS, ctx);
    if (fc->hide_timer != NULL) {
        lv_timer_pause(fc->hide_timer);
    }
    face_controls_set_face(ctx, fc->active_face);
}

/* --- State ------------------------------------------------------------------------------ */

bool face_controls_visible(const clock_ui_context_t *ctx)
{
    return ctx->face_chrome.visible;
}

static bool controls_allowed(const clock_ui_context_t *ctx)
{
    return ctx->face_chrome.layer != NULL && !ctx->face_chrome.hidden && !ui_settings_is_open(ctx) &&
           !ui_alarms_is_open(ctx) && !face_themes_visible(ctx) && !face_quick_visible(ctx) &&
           !ctx->faces.face_slide_active && !(ctx->runtime != NULL && ctx->runtime->alarm_ringing);
}

/* Redraws the parts of the rim decoration that change in the entrance or exit: the Atelier
 * bezel while it fades (the whole screen), else only the tabs and the page dots. The controls
 * themselves redraw where they move as their styles change. */
static void reveal_invalidate_rim(clock_ui_context_t *ctx)
{
    clock_ui_face_chrome_state_t *fc = &ctx->face_chrome;
    lv_area_t dots = {120, 560, SCREEN_SIZE - 120, SCREEN_SIZE - 1};

    if (fc->reveal_mode == 2 || (ui_skin_style() == CLOCK_UI_STYLE_ATELIER && reveal_x(fc, 0.0f) * 1.5f < 1.0f)) {
        lv_obj_invalidate(fc->layer);
        return;
    }
    lv_obj_invalidate_area(fc->layer, &dots);
    for (int i = 0; i < 2; i++) {
        lv_area_t band = fc->rim_band_area[i];

        if (fc->rim_band[i] == NULL) {
            continue;
        }
        band.y1 -= CONTROLS_GLIDE + 4;
        band.y2 += CONTROLS_GLIDE + 4;
        lv_obj_invalidate_area(fc->layer, &band);
    }
}

/* Places the controls for the entrance or exit at reveal_ms; the layer's rim decoration
 * follows in its draw callback. */
static void reveal_apply(clock_ui_context_t *ctx)
{
    clock_ui_face_chrome_state_t *fc = &ctx->face_chrome;
    uint32_t count = lv_obj_get_child_count(fc->layer);

    for (uint32_t i = 0; i < count; i++) {
        lv_obj_t *child = lv_obj_get_child(fc->layer, (int32_t)i);
        lv_area_t c;
        float dx;
        float dy;
        float len;
        float x;
        float glide;

        /* Hidden ones too, so none is left part-way when it shows later. The child's centre
         * where it rests (its coordinates include the glide). */
        lv_obj_get_coords(child, &c);
        dx = (float)(c.x1 + c.x2) / 2.0f - (float)lv_obj_get_style_translate_x(child, 0) - (float)KIT_CENTER;
        dy = (float)(c.y1 + c.y2) / 2.0f - (float)lv_obj_get_style_translate_y(child, 0) - (float)KIT_CENTER;
        len = sqrtf(dx * dx + dy * dy);
        if (len < 1.0f) {
            dx = 0.0f;
            dy = -1.0f;
            len = 1.0f;
        }
        x = reveal_x(fc, clock_angle(dx, dy));
        glide = reveal_glide(fc, x);
        lv_obj_set_style_translate_x(child, (int32_t)lroundf(dx / len * glide), 0);
        lv_obj_set_style_translate_y(child, (int32_t)lroundf(dy / len * glide), 0);
        lv_obj_set_style_transform_pivot_x(child, lv_pct(50), 0);
        lv_obj_set_style_transform_pivot_y(child, lv_pct(50), 0);
        lv_obj_set_style_transform_scale(child, (int32_t)lroundf(reveal_scale(fc, x) * 256.0f), 0);
        lv_obj_set_style_opa(child, reveal_opa(fc, x), 0);
    }
    reveal_invalidate_rim(ctx);
}

static void reveal_anim_cb(void *var, int32_t value)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)var;

    ctx->face_chrome.reveal_ms = (int16_t)value;
    reveal_apply(ctx);
}

static void reveal_done_cb(lv_anim_t *anim)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_anim_get_user_data(anim);
    clock_ui_face_chrome_state_t *fc = &ctx->face_chrome;

    if (fc->reveal_mode == 2) {
        lv_obj_add_flag(fc->layer, LV_OBJ_FLAG_HIDDEN);
    }
    fc->reveal_mode = 0;
    reveal_apply(ctx);
}

/* Before a chrome rebuild: the reveal runs on the context, which outlives the controls. */
void face_controls_destroy(clock_ui_context_t *ctx)
{
    lv_anim_delete(ctx, reveal_anim_cb);
}

/* Starts the entrance (mode 1) or the exit (mode 2) from its beginning. */
static void reveal_start(clock_ui_context_t *ctx, uint8_t mode)
{
    clock_ui_face_chrome_state_t *fc = &ctx->face_chrome;
    uint32_t total = mode == 1 ? CONTROLS_SWEEP_MS + CONTROLS_IN_MS : CONTROLS_OUT_SWEEP_MS + CONTROLS_OUT_MS;
    lv_anim_t anim;

    lv_anim_delete(ctx, reveal_anim_cb);
    fc->reveal_mode = mode;
    fc->reveal_ms = 0;
    reveal_apply(ctx);
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, ctx);
    lv_anim_set_user_data(&anim, ctx);
    lv_anim_set_exec_cb(&anim, reveal_anim_cb);
    lv_anim_set_values(&anim, 0, (int32_t)total);
    lv_anim_set_duration(&anim, total);
    lv_anim_set_path_cb(&anim, lv_anim_path_linear);
    lv_anim_set_completed_cb(&anim, reveal_done_cb);
    lv_anim_start(&anim);
}

/* Liquid Glass over a light face: the controls' text and plain icons in dark ink, and back
 * to light over a dark one. Coloured icons keep their colour. */
static void controls_show(clock_ui_context_t *ctx, bool reveal);

void face_controls_show(clock_ui_context_t *ctx)
{
    controls_show(ctx, true);
}

void face_controls_show_still(clock_ui_context_t *ctx)
{
    controls_show(ctx, false);
}

static void controls_show(clock_ui_context_t *ctx, bool reveal)
{
    clock_ui_face_chrome_state_t *fc = &ctx->face_chrome;

    if (!controls_allowed(ctx)) {
        return;
    }
    face_controls_sync(ctx, face_now(), !fc->visible || !reveal);
    if (!fc->visible) {
        fc->visible = true;
        ctx->affordances_visible = true;
        lv_obj_move_foreground(fc->layer);
        lv_obj_remove_flag(fc->layer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_update_layout(fc->layer);
        if (reveal) {
            reveal_start(ctx, 1);
        } else {
            lv_anim_delete(ctx, reveal_anim_cb);
            fc->reveal_mode = 0;
            reveal_apply(ctx);
        }
        face_night_sync(ctx, face_now(), false);
    }
    if (fc->hide_timer != NULL) {
        lv_timer_set_period(fc->hide_timer, CONTROLS_VISIBLE_MS);
        lv_timer_reset(fc->hide_timer);
        lv_timer_resume(fc->hide_timer);
    }
}

/* Hides the controls at once: something else is about to take the screen. */
void face_controls_hold(clock_ui_context_t *ctx)
{
    if (ctx->face_chrome.hide_timer != NULL) {
        lv_timer_pause(ctx->face_chrome.hide_timer);
    }
}

void face_controls_hide(clock_ui_context_t *ctx)
{
    clock_ui_face_chrome_state_t *fc = &ctx->face_chrome;

    if (fc->hide_timer != NULL) {
        lv_timer_pause(fc->hide_timer);
    }
    if (fc->layer == NULL || (!fc->visible && lv_obj_has_flag(fc->layer, LV_OBJ_FLAG_HIDDEN))) {
        return;
    }
    lv_anim_delete(ctx, reveal_anim_cb);
    lv_obj_add_flag(fc->layer, LV_OBJ_FLAG_HIDDEN);
    if (fc->reveal_mode != 0) {
        fc->reveal_mode = 0;
        reveal_apply(ctx);
    }
    if (fc->visible) {
        fc->visible = false;
        ctx->affordances_visible = false;
        face_night_sync(ctx, face_now(), false);
    }
}

/* The controls glide out and fade: timed out, or tapped away. */
static void controls_leave(clock_ui_context_t *ctx)
{
    clock_ui_face_chrome_state_t *fc = &ctx->face_chrome;

    if (fc->hide_timer != NULL) {
        lv_timer_pause(fc->hide_timer);
    }
    if (!fc->visible || fc->layer == NULL) {
        return;
    }
    fc->visible = false;
    ctx->affordances_visible = false;
    reveal_start(ctx, 2);
    face_night_sync(ctx, face_now(), false);
}

void face_controls_toggle(clock_ui_context_t *ctx)
{
    if (ctx->face_chrome.visible) {
        controls_leave(ctx);
    } else {
        face_controls_show(ctx);
    }
}

void face_controls_set_hidden(clock_ui_context_t *ctx, bool hidden)
{
    ctx->face_chrome.hidden = hidden;
    if (hidden) {
        face_controls_hide(ctx);
    }
}

void face_controls_set_face(clock_ui_context_t *ctx, clock_face_id_t face)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_face_chrome_state_t *fc = &ctx->face_chrome;
    int8_t index = visible_index(face);

    fc->active_face = face;
    if (fc->layer == NULL || index == fc->dot_active) {
        return;
    }
    fc->dot_active = index;
    if (ui_skin_style() == CLOCK_UI_STYLE_INSTRUMENT) {
        for (int i = 0; i < fc->dot_count; i++) {
            bool on = i == index;

            if (fc->dots[i] == NULL) {
                continue;
            }
            lv_obj_set_width(fc->dots[i], on ? 12 : 6);
            lv_obj_set_style_bg_color(fc->dots[i], on ? glass_ink(sk->accent) : sk->text_3, 0);
        }
    } else {
        lv_obj_invalidate(fc->layer);
    }
}

static void set_shown(lv_obj_t *obj, bool shown)
{
    if (obj == NULL || shown == !lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }
    if (shown) {
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

static void set_text(lv_obj_t *label, const char *text)
{
    if (label != NULL) {
        kit_label_set_text_if_changed(label, text);
    }
}

/* Rebuilds the texts when the minute, the language or the update state changed. */
void face_controls_sync(clock_ui_context_t *ctx, time_t now, bool force)
{
    clock_ui_face_chrome_state_t *fc = &ctx->face_chrome;
    const app_runtime_state_t *rt = ctx->runtime;
    bool update = rt != NULL && rt->ota_update_available && !rt->ota_reboot_pending;
    long minute = (long)(now / 60);
    time_t when = 0;
    bool alarm;
    char clock[8];
    char note[64];
    char text[96];

    if (fc->layer == NULL) {
        return;
    }
    if (!force && minute == fc->synced_minute && update == fc->update_shown &&
        fc->synced_language == ctx->settings->language) {
        return;
    }
    fc->synced_minute = minute;
    fc->synced_language = ctx->settings->language;
    fc->update_shown = update;

    alarm = face_next_alarm(ctx, now, &when);
    fc->alarm_shown = alarm;
    clock[0] = '\0';
    note[0] = '\0';
    if (alarm) {
        ui_format_clock(when, clock, sizeof(clock));
    }

    switch (ui_skin_style()) {
    case CLOCK_UI_STYLE_INSTRUMENT:
        if (alarm) {
            face_format_countdown_compact(ctx, now, when, note, sizeof(note));
        }
        if (rt != NULL && rt->ota_busy) {
            snprintf(text, sizeof(text), "%s", ui_l10n_text(ctx, L10N_UPDATING));
        } else {
            char version[24];
            size_t n;

            /* "1.9.0" reads as "1.9" on the plate. */
            snprintf(version, sizeof(version), "%s", rt != NULL ? rt->ota_available_version : "");
            n = strlen(version);
            if (n > 2 && strcmp(version + n - 2, ".0") == 0 && strchr(version, '.') != version + n - 2) {
                version[n - 2] = '\0';
            }
            snprintf(text, sizeof(text), ui_l10n_str(ctx, &s_update_version), version);
        }
        break;
    case CLOCK_UI_STYLE_ATELIER:
        if (alarm) {
            ui_format_countdown(ctx, now, when, note, sizeof(note));
        }
        snprintf(text, sizeof(text), "%s",
                 rt != NULL && rt->ota_busy ? ui_l10n_text(ctx, L10N_UPDATING) : ui_l10n_str(ctx, &s_update_available));
        break;
    case CLOCK_UI_STYLE_ORBIT:
    default:
        if (alarm) {
            format_day_word(ctx, now, when, note, sizeof(note));
        }
        snprintf(text, sizeof(text), "%s",
                 rt != NULL && rt->ota_busy ? ui_l10n_text(ctx, L10N_UPDATING) : ui_l10n_str(ctx, &s_update_available));
        break;
    }

    set_text(fc->status_alarm_time, clock);
    set_text(fc->status_alarm_note, note);
    set_text(fc->status_update_text, text);
    set_shown(fc->status_alarm_row, alarm);
    set_shown(fc->status_update_row, update);
    set_shown(fc->status, alarm || update);
    set_shown(fc->badge, update);
    if (fc->update_chip != NULL) {
        kit_chip_set(fc->update_chip, text, true);
        set_shown(fc->update_chip, update);
    }
    set_text(fc->alarm_chip_time, clock);
    set_text(fc->alarm_chip_note, note);
    set_shown(fc->alarm_chip, alarm);
}
