/*
 * Quick actions (screen 03), pulled up from the bottom over the dimmed face: four keys
 * (Alarms, Night, Sound, Settings) and the brightness control on the lower rim.
 *
 * The rim control is an lv_arc used for its input only (angle to value, drag rate, hit band
 * on the ring); its look is drawn per style in one draw callback: Instrument scale ring with
 * ticks and a signal fill, Orbit capsule track, Atelier bezel scale with the jewel. Dragging
 * it sets the brightness live (the night override while night mode is active), as the old
 * brightness panel did; the panel is gone.
 *
 * The sheet mechanics (pull from the bottom edge, drag, snapshot while moving, dim, auto
 * close) are in ui_face_sheet.c.
 */
#include <math.h>

#include "ui/ui_areas.h"
#include "ui/ui_face_chrome.h"

#define QUICK_AUTO_CLOSE_MS 5000
#define ARC_START 30 /* LVGL degrees: lower right, 100 % */
#define ARC_END 150  /* lower left, 0 % */
#define EDGE_TRIGGER 12

static const ui_l10n_str_t s_night = UI_L10N_STR("Night", "Noc", "Ніч", "Ночь", "Nacht");
static const ui_l10n_str_t s_sound = UI_L10N_STR("Sound", "Dźwięk", "Звук", "Звук", "Geluid");
static const ui_l10n_str_t s_quiet = UI_L10N_STR("Quiet", "Cisza", "Тиша", "Тишина", "Stil");
static const ui_l10n_str_t s_night_on = UI_L10N_STR("Night on", "Noc wł.", "Ніч увімк.", "Ночь вкл.", "Nacht aan");
static const ui_l10n_str_t s_night_off = UI_L10N_STR("Night off", "Noc wył.", "Ніч вимк.", "Ночь выкл.", "Nacht uit");
static const ui_l10n_str_t s_sound_on = UI_L10N_STR("Sound on", "Dźwięk wł.", "Звук увімк.", "Звук вкл.", "Geluid aan");
static const ui_l10n_str_t s_sound_off = UI_L10N_STR("Sound off", "Dźwięk wył.", "Звук вимк.", "Звук выкл.",
                                                     "Geluid uit");

typedef struct {
    int32_t open_y;
    int32_t x;
    int32_t radius;   /* rim control: centre line radius */
    int32_t key;      /* key size */
    int32_t key_y;    /* key top (screen) */
    int32_t key_pitch;
    int32_t label_y;  /* key label top (screen) */
} quick_layout_t;

static quick_layout_t quick_layout(void)
{
    switch (ui_skin_style()) {
    case CLOCK_UI_STYLE_INSTRUMENT:
        return (quick_layout_t) {216, 0, 318, 88, 264, 112, 368};
    case CLOCK_UI_STYLE_ATELIER:
        return (quick_layout_t) {196, 0, 300, 84, 258, 116, 349};
    case CLOCK_UI_STYLE_ORBIT:
    default:
        return (quick_layout_t) {212, 60, 286, 92, 258, 120, 362};
    }
}

/* --- Brightness ------------------------------------------------------------------------- */

static uint8_t target_hw(const clock_ui_context_t *ctx)
{
    if (ctx->runtime != NULL && ctx->runtime->in_night_mode) {
        if (ctx->runtime->night_brightness_override_active) {
            return ctx->runtime->night_brightness_override;
        }
        return ctx->settings->night_mode.brightness;
    }
    return ctx->settings->base_brightness;
}

static void set_value_text(clock_ui_context_t *ctx, int32_t percent)
{
    clock_ui_brightness_state_t *q = &ctx->brightness;
    char buf[8];

    if (ui_skin_style() == CLOCK_UI_STYLE_ORBIT) {
        snprintf(buf, sizeof(buf), "%d%%", (int)percent);
    } else {
        snprintf(buf, sizeof(buf), "%d", (int)percent);
    }
    kit_label_set_text_if_changed(q->value, buf);
}

static float arc_angle(int32_t percent)
{
    return (float)ARC_END - (float)(percent * (ARC_END - ARC_START)) / 100.0f;
}

static lv_point_t arc_point(lv_point_t c, int32_t r, float deg)
{
    float a = deg * 3.14159265f / 180.0f;

    return (lv_point_t) {(int32_t)lroundf(c.x + r * cosf(a)), (int32_t)lroundf(c.y + r * sinf(a))};
}

static void draw_jewel(lv_layer_t *layer, lv_point_t p)
{
    const lv_image_dsc_t *src = kit_image_src(KIT_IMAGE_JEWEL_40);
    lv_draw_image_dsc_t d;
    lv_area_t a;

    lv_draw_image_dsc_init(&d);
    d.src = src;
    a.x1 = p.x - (int32_t)src->header.w / 2;
    a.y1 = p.y - (int32_t)src->header.h / 2;
    a.x2 = a.x1 + (int32_t)src->header.w - 1;
    a.y2 = a.y1 + (int32_t)src->header.h - 1;
    lv_draw_image(layer, &d, &a);
}

static void arc_draw_cb(lv_event_t *e)
{
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *arc = lv_event_get_current_target(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    int32_t v = lv_arc_get_value(arc);
    float av = arc_angle(v);
    int32_t r = lv_obj_get_width(arc) / 2;
    lv_area_t coords;
    lv_point_t c;
    lv_point_t k;

    lv_obj_get_coords(arc, &coords);
    c.x = coords.x1 + r;
    c.y = coords.y1 + r;
    k = arc_point(c, r, av);

    switch (ui_skin_style()) {
    case CLOCK_UI_STYLE_INSTRUMENT:
        /* Scale ring: 21 ticks (majors every 25 %), a hairline track and the signal fill. */
        for (int i = 0; i <= 20; i++) {
            bool major = (i % 5) == 0;

            face_draw_tick(layer, c, major ? r - 28 : r - 20, r - 12, (float)(ARC_END - i * 6), 2,
                           i * 5 <= v ? sk->text_2 : sk->line_strong);
        }
        face_draw_rim_arc(layer, c, r + 1, 2, ARC_START, ARC_END, sk->line_strong, LV_OPA_COVER, false);
        if (v > 0) {
            face_draw_rim_arc(layer, c, r + 3, 6, (int32_t)lroundf(av), ARC_END, sk->accent, LV_OPA_COVER, true);
        }
        face_draw_dot(layer, k.x, k.y, 17, sk->sheet, LV_OPA_COVER);
        face_draw_dot(layer, k.x, k.y, 13, sk->text, LV_OPA_COVER);
        face_draw_dot(layer, k.x, k.y, 3, sk->accent, LV_OPA_COVER);
        break;
    case CLOCK_UI_STYLE_ATELIER:
        /* Bezel scale: ticks lit in gilt up to the value, a fine track and the jewel. */
        for (int i = 0; i <= 20; i++) {
            bool major = (i % 2) == 0;
            float a = (float)(ARC_END - i * 6);
            bool lit = a >= av - 0.01f;

            face_draw_tick(layer, c, r + (major ? 27 : 32), r + 40, a, major ? 2 : 1,
                           lit ? (major ? sk->accent : sk->label) : (major ? sk->accent_deep : sk->line_strong));
        }
        face_draw_rim_arc(layer, c, r + 2, 4, ARC_START, ARC_END, sk->line_strong, LV_OPA_COVER, true);
        if (v > 0) {
            face_draw_rim_arc(layer, c, r + 2, 4, (int32_t)lroundf(av), ARC_END, sk->accent, LV_OPA_COVER, true);
        }
        draw_jewel(layer, k);
        break;
    case CLOCK_UI_STYLE_ORBIT:
    default:
        /* Capsule track around the lower rim in the brightness tint, white knob. */
        face_draw_rim_arc(layer, c, r + 18, 36, ARC_START, ARC_END, sk->well, LV_OPA_COVER, true);
        if (v > 0) {
            face_draw_rim_arc(layer, c, r + 18, 36, (int32_t)lroundf(av), ARC_END, sk->tint_brightness,
                              LV_OPA_COVER, true);
        }
        face_draw_dot(layer, k.x, k.y + 3, 25, lv_color_black(), LV_OPA_40);
        face_draw_dot(layer, k.x, k.y, 22, lv_color_white(), LV_OPA_COVER);
        break;
    }
}

/* The scale, the capsule and the knob reach past the arc object's square. */
static void arc_ext_draw_cb(lv_event_t *e)
{
    lv_event_set_ext_draw_size(e, 48);
}

static void arc_event_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(e);
    clock_ui_brightness_state_t *q = &ctx->brightness;
    lv_event_code_t code = lv_event_get_code(e);
    int32_t percent;
    uint8_t hw;

    face_sheet_activity(&q->sheet);
    if (code != LV_EVENT_VALUE_CHANGED || ctx->suppress_events) {
        return;
    }
    percent = lv_arc_get_value(q->arc);
    hw = brightness_ui_to_hw((int)percent);
    if (ctx->runtime != NULL && ctx->runtime->in_night_mode) {
        request_set_runtime_night_brightness(ctx, hw);
    } else {
        request_set_base_brightness(ctx, hw);
    }
    q->last_ui_percent = (uint8_t)percent;
    q->ui_synced = true;
    set_value_text(ctx, percent);
    lv_obj_invalidate(q->arc);
    face_sheet_mark_dirty(&q->sheet);
}

void update_brightness_ui(clock_ui_context_t *ctx)
{
    clock_ui_brightness_state_t *q = &ctx->brightness;
    uint8_t percent;

    if (q->arc == NULL || lv_obj_has_state(q->arc, LV_STATE_PRESSED)) {
        return;
    }
    percent = brightness_hw_to_ui(target_hw(ctx));
    if (q->ui_synced && q->last_ui_percent == percent) {
        return;
    }
    q->last_ui_percent = percent;
    q->ui_synced = true;
    ctx->suppress_events = true;
    lv_arc_set_value(q->arc, percent);
    ctx->suppress_events = false;
    set_value_text(ctx, percent);
    lv_obj_invalidate(q->arc);
    face_sheet_mark_dirty(&q->sheet);
}

/* --- Keys ------------------------------------------------------------------------------- */

static void set_led(lv_obj_t *led, bool on)
{
    const ui_skin_t *sk = ui_skin();

    if (led == NULL) {
        return;
    }
    lv_obj_set_style_bg_color(led, on ? sk->accent : sk->well, 0);
    lv_obj_set_style_border_width(led, on ? 0 : 1, 0);
}

static void style_key(clock_ui_context_t *ctx, quick_key_t key, bool on)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_brightness_state_t *q = &ctx->brightness;
    lv_obj_t *btn = q->keys[key];
    lv_obj_t *icon = q->key_icons[key];
    lv_obj_t *label = q->key_labels[key];

    switch (ui_skin_style()) {
    case CLOCK_UI_STYLE_INSTRUMENT:
        set_led(q->key_leds[key], on);
        lv_obj_set_style_text_color(label, on ? sk->text_2 : sk->label, 0);
        break;
    case CLOCK_UI_STYLE_ATELIER:
        lv_obj_set_style_bg_color(btn, on ? sk->accent : sk->surface, 0);
        lv_obj_set_style_border_color(btn, on ? sk->value : sk->accent_deep, 0);
        lv_obj_set_style_image_recolor(icon, on ? sk->accent_text : sk->value, 0);
        lv_obj_set_style_text_color(label, on ? sk->accent : sk->text_2, 0);
        break;
    case CLOCK_UI_STYLE_ORBIT:
    default: {
        lv_color_t tint = key == QUICK_KEY_NIGHT ? sk->tint_night : sk->tint_sound;

        lv_obj_set_style_bg_color(btn, on ? tint : sk->raised, 0);
        lv_obj_set_style_image_recolor(icon, on ? lv_color_white() : tint, 0);
        lv_obj_set_style_text_color(label, on ? sk->text : sk->text_2, 0);
        break;
    }
    }
}

/* Sets the key labels for the language and toggle states (the bindings set the same texts on
 * a language change; doing it here as well keeps the order of the refreshes irrelevant), and
 * fits long words into the key pitch. */
static void set_key_texts(clock_ui_context_t *ctx)
{
    clock_ui_brightness_state_t *q = &ctx->brightness;
    clock_ui_style_t style = ui_skin_style();
    quick_layout_t l = quick_layout();
    bool night = ctx->settings->night_mode.enabled;
    bool sound = ctx->settings->ui_click_sound_enabled;
    int32_t letter_space = style == CLOCK_UI_STYLE_ORBIT ? 0 : (style == CLOCK_UI_STYLE_ATELIER ? 2 : ui_skin()->label_letter_space);

    kit_label_set_text_if_changed(q->key_labels[QUICK_KEY_ALARMS], ui_l10n_text(ctx, L10N_ALARMS));
    kit_label_set_text_if_changed(q->key_labels[QUICK_KEY_SETTINGS], ui_l10n_text(ctx, L10N_SETTINGS));
    if (style == CLOCK_UI_STYLE_ORBIT) {
        kit_label_set_text_if_changed(q->key_labels[QUICK_KEY_NIGHT], ui_l10n_str(ctx, night ? &s_night_on : &s_night_off));
        kit_label_set_text_if_changed(q->key_labels[QUICK_KEY_SOUND], ui_l10n_str(ctx, sound ? &s_sound_on : &s_sound_off));
    } else {
        kit_label_set_text_if_changed(q->key_labels[QUICK_KEY_NIGHT], ui_l10n_str(ctx, &s_night));
        kit_label_set_text_if_changed(q->key_labels[QUICK_KEY_SOUND],
                                      ui_l10n_str(ctx, style == CLOCK_UI_STYLE_ATELIER ? &s_quiet : &s_sound));
    }
    for (int k = 0; k < QUICK_KEY_COUNT; k++) {
        face_fit_label(q->key_labels[k], l.key_pitch - 10, letter_space);
    }
}

static void sync_keys(clock_ui_context_t *ctx, bool force)
{
    clock_ui_brightness_state_t *q = &ctx->brightness;
    bool night = ctx->settings->night_mode.enabled;
    bool sound = ctx->settings->ui_click_sound_enabled;

    if (q->keys[QUICK_KEY_NIGHT] == NULL) {
        return;
    }
    if (!force && q->keys_synced && night == q->night_on && sound == q->sound_on &&
        q->last_language == ctx->settings->language) {
        return;
    }
    q->keys_synced = true;
    q->night_on = night;
    q->sound_on = sound;
    q->last_language = ctx->settings->language;
    set_key_texts(ctx);
    style_key(ctx, QUICK_KEY_NIGHT, night);
    /* Atelier shows the toggle as "Quiet": lit when the touch sound is off. */
    style_key(ctx, QUICK_KEY_SOUND, ui_skin_style() == CLOCK_UI_STYLE_ATELIER ? !sound : sound);
    face_sheet_mark_dirty(&q->sheet);
}

static void key_click_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(e);
    clock_ui_brightness_state_t *q = &ctx->brightness;
    lv_obj_t *btn = lv_event_get_current_target(e);

    if (!face_click_is_tap(ctx, e)) {
        return;
    }
    if (btn == q->keys[QUICK_KEY_ALARMS]) {
        ui_alarms_open(ctx);
    } else if (btn == q->keys[QUICK_KEY_SETTINGS]) {
        ui_settings_open(ctx);
    } else if (btn == q->keys[QUICK_KEY_NIGHT]) {
        request_set_night_mode_enabled(ctx, !ctx->settings->night_mode.enabled);
        face_sheet_activity(&q->sheet);
    } else if (btn == q->keys[QUICK_KEY_SOUND]) {
        request_set_ui_click_sound_enabled(ctx, !ctx->settings->ui_click_sound_enabled);
        face_sheet_activity(&q->sheet);
    }
}

static void key_press_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(e);

    face_sheet_activity(&ctx->brightness.sheet);
}

static void build_key(clock_ui_context_t *ctx, quick_key_t key, kit_icon_t icon, const quick_layout_t *l)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_brightness_state_t *q = &ctx->brightness;
    clock_ui_style_t style = ui_skin_style();
    int32_t cx = KIT_CENTER + ((int32_t)key * 2 - 3) * l->key_pitch / 2 - l->x;
    lv_obj_t *btn = kit_round_button(q->panel, KIT_BUTTON_SECONDARY, icon, l->key);
    lv_obj_t *label;

    lv_obj_set_pos(btn, cx - l->key / 2, l->key_y - l->open_y);
    lv_obj_add_event_cb(btn, key_click_cb, LV_EVENT_CLICKED, ctx);
    lv_obj_add_event_cb(btn, key_press_cb, LV_EVENT_PRESSED, ctx);
    face_track_press(ctx, btn);
    q->keys[key] = btn;
    q->key_icons[key] = lv_obj_get_child(btn, 0);

    if (style == CLOCK_UI_STYLE_INSTRUMENT) {
        /* Square keycaps; the toggles carry an LED. */
        lv_obj_set_style_radius(btn, sk->radius_s, 0);
        if (key == QUICK_KEY_NIGHT || key == QUICK_KEY_SOUND) {
            lv_obj_t *led = face_plain(btn);

            lv_obj_set_size(led, 8, 8);
            lv_obj_set_pos(led, l->key - 10 - 8 - 1, 10 - 1);
            lv_obj_set_style_radius(led, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_opa(led, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(led, sk->line_strong, 0);
            q->key_leds[key] = led;
        }
    } else if (style == CLOCK_UI_STYLE_ORBIT && key == QUICK_KEY_ALARMS) {
        kit_button_set_tint(btn, sk->tint_alarms);
    }

    /* One line, centred under the key; long words tighten their tracking (see fit_labels). */
    label = kit_label(q->panel, KIT_TEXT_LABEL, "");
    lv_obj_align(label, LV_ALIGN_TOP_MID, cx - (SCREEN_SIZE - 2 * l->x) / 2, l->label_y - l->open_y);
    if (style == CLOCK_UI_STYLE_ATELIER) {
        lv_obj_set_style_text_letter_space(label, 2, 0);
        lv_obj_set_style_text_color(label, sk->text_2, 0);
    } else if (style == CLOCK_UI_STYLE_ORBIT) {
        lv_obj_set_style_text_color(label, sk->text_2, 0);
    }
    q->key_labels[key] = label;
    switch (key) {
    case QUICK_KEY_ALARMS:
        ui_l10n_bind_kit(ctx, label, L10N_ALARMS);
        break;
    case QUICK_KEY_SETTINGS:
        ui_l10n_bind_kit(ctx, label, L10N_SETTINGS);
        break;
    case QUICK_KEY_NIGHT:
        if (style != CLOCK_UI_STYLE_ORBIT) {
            ui_l10n_bind_kit_str(ctx, label, &s_night);
        }
        break;
    case QUICK_KEY_SOUND:
        if (style != CLOCK_UI_STYLE_ORBIT) {
            ui_l10n_bind_kit_str(ctx, label, style == CLOCK_UI_STYLE_ATELIER ? &s_quiet : &s_sound);
        }
        break;
    default:
        break;
    }
}

/* --- Sheet ------------------------------------------------------------------------------ */

static void quick_on_state(clock_ui_context_t *ctx)
{
    if (face_sheet_is_visible(&ctx->brightness.sheet)) {
        face_controls_hide(ctx);
        face_themes_close(ctx, false);
    }
    face_shell_sync_face_visuals(ctx);
}

static lv_obj_t *end_icon(lv_obj_t *panel, kit_icon_t icon, bool small, lv_color_t color, int32_t cx, int32_t cy)
{
    lv_obj_t *img = small ? kit_icon_small(panel, icon, color) : kit_icon(panel, icon, color);
    const lv_image_dsc_t *src = lv_image_get_src(img);
    int32_t w = src != NULL ? (int32_t)src->header.w : 24;

    lv_obj_set_pos(img, cx - w / 2, cy - w / 2);
    return img;
}

static void build_brightness(clock_ui_context_t *ctx, const quick_layout_t *l)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_brightness_state_t *q = &ctx->brightness;
    clock_ui_style_t style = ui_skin_style();
    int32_t ox = l->x;
    int32_t oy = l->open_y;
    lv_obj_t *label;
    lv_obj_t *row;

    /* The arc is created first so the keys above it take their own presses. */
    q->arc = lv_arc_create(q->panel);
    lv_obj_remove_style_all(q->arc);
    lv_obj_set_size(q->arc, l->radius * 2, l->radius * 2);
    lv_obj_set_pos(q->arc, KIT_CENTER - l->radius - ox, KIT_CENTER - l->radius - oy);
    lv_arc_set_bg_angles(q->arc, ARC_START, ARC_END);
    lv_arc_set_mode(q->arc, LV_ARC_MODE_REVERSE);
    lv_arc_set_range(q->arc, 0, 100);
    lv_arc_set_change_rate(q->arc, 1080);
    /* Input only: a 36 px ring plus 40 px on each side takes touches, nothing is drawn. */
    lv_obj_set_style_arc_width(q->arc, 36, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(q->arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_width(q->arc, 40, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(q->arc, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(q->arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_add_flag(q->arc, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_set_ext_click_area(q->arc, 40);
    lv_obj_add_event_cb(q->arc, arc_ext_draw_cb, LV_EVENT_REFR_EXT_DRAW_SIZE, NULL);
    lv_obj_add_event_cb(q->arc, arc_draw_cb, LV_EVENT_DRAW_MAIN_END, ctx);
    lv_obj_refresh_ext_draw_size(q->arc);
    lv_obj_add_event_cb(q->arc, arc_event_cb, LV_EVENT_VALUE_CHANGED, ctx);
    lv_obj_add_event_cb(q->arc, arc_event_cb, LV_EVENT_PRESSED, ctx);
    lv_obj_add_event_cb(q->arc, arc_event_cb, LV_EVENT_RELEASED, ctx);
    ui_attach_click_feedback(q->arc, LV_EVENT_VALUE_CHANGED);

    switch (style) {
    case CLOCK_UI_STYLE_INSTRUMENT: {
        lv_point_t lo = arc_point(FACE_CENTER, 266, ARC_END);
        lv_point_t hi = arc_point(FACE_CENTER, 266, ARC_START);

        end_icon(q->panel, KIT_ICON_BRIGHTNESS, true, sk->text_3, lo.x - ox, lo.y - oy);
        end_icon(q->panel, KIT_ICON_SUN, true, sk->text_3, hi.x - ox, hi.y - oy);
        label = kit_label(q->panel, KIT_TEXT_LABEL, "");
        lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 470 - oy);
        ui_l10n_bind_kit(ctx, label, L10N_BRIGHTNESS);
        row = face_plain(q->panel);
        lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
        lv_obj_set_style_pad_column(row, 4, 0);
        lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 500 - oy);
        q->value = face_text(row, sk->font_time_m, sk->text, "");
        q->percent = face_text(row, sk->font_heading, sk->text_2, "%");
        lv_obj_set_style_translate_y(q->percent, -8, 0);
        break;
    }
    case CLOCK_UI_STYLE_ATELIER: {
        lv_point_t lo = arc_point(FACE_CENTER, 300, 158);
        lv_point_t hi = arc_point(FACE_CENTER, 300, 22);

        face_engraved_rule(q->panel, 410 - oy, 200);
        end_icon(q->panel, KIT_ICON_BRIGHTNESS, false, sk->text_2, lo.x - ox, lo.y - oy);
        end_icon(q->panel, KIT_ICON_SUN, false, sk->value, hi.x - ox, hi.y - oy);
        label = kit_label(q->panel, KIT_TEXT_LABEL, "");
        lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 459 - oy);
        ui_l10n_bind_kit(ctx, label, L10N_BRIGHTNESS);
        row = face_plain(q->panel);
        lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
        lv_obj_set_style_pad_column(row, 4, 0);
        lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 500 - oy);
        q->value = face_text(row, sk->font_time_m, sk->text, "");
        q->percent = face_text(row, sk->font_value, sk->text, "%");
        lv_obj_set_style_translate_y(q->percent, -7, 0);
        break;
    }
    case CLOCK_UI_STYLE_ORBIT:
    default:
        end_icon(q->panel, KIT_ICON_BRIGHTNESS, true, sk->text_2, 112 - ox, 455 - oy);
        end_icon(q->panel, KIT_ICON_SUN, false, sk->text_2, 608 - ox, 455 - oy);
        q->value = face_text(q->panel, sk->font_time_m, sk->text, "");
        lv_obj_align(q->value, LV_ALIGN_TOP_MID, 0, 456 - oy);
        label = kit_label(q->panel, KIT_TEXT_BODY, "");
        lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 522 - oy);
        ui_l10n_bind_kit(ctx, label, L10N_BRIGHTNESS);
        break;
    }
}

void face_quick_create(clock_ui_context_t *ctx)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_brightness_state_t *q = &ctx->brightness;
    clock_ui_style_t style = ui_skin_style();
    quick_layout_t l = quick_layout();
    face_sheet_opts_t o = {
        .from_top = false,
        .x = l.x,
        .width = SCREEN_SIZE - 2 * l.x,
        .height = SCREEN_SIZE - l.open_y,
        .open_y = l.open_y,
        .radius = 0,
        .color = sk->sheet,
        .border = sk->line_strong,
        .border_side = LV_BORDER_SIDE_TOP,
        .auto_close_ms = QUICK_AUTO_CLOSE_MS,
        .on_state = quick_on_state,
    };
    static const kit_icon_t icons[3][QUICK_KEY_COUNT] = {
        [CLOCK_UI_STYLE_INSTRUMENT] = {KIT_ICON_ALARM, KIT_ICON_MOON, KIT_ICON_SOUND, KIT_ICON_SETTINGS},
        [CLOCK_UI_STYLE_ORBIT] = {KIT_ICON_ALARM, KIT_ICON_MOON, KIT_ICON_SOUND, KIT_ICON_GEAR},
        [CLOCK_UI_STYLE_ATELIER] = {KIT_ICON_BELL, KIT_ICON_MOON, KIT_ICON_SOUND_OFF, KIT_ICON_GEAR},
    };
    unsigned si = (unsigned)style < 3 ? (unsigned)style : (unsigned)CLOCK_UI_STYLE_ORBIT;
    lv_obj_t *grab;

    if (style == CLOCK_UI_STYLE_ORBIT) {
        o.radius = 64;
        o.height += 64; /* the card's lower corners stay below the screen */
        o.color = sk->surface;
        o.border_side = LV_BORDER_SIDE_NONE;
    }
    face_sheet_create(&q->sheet, ctx, &o);

    grab = face_plain(q->panel);
    lv_obj_set_style_bg_opa(grab, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(grab, 3, 0);
    switch (style) {
    case CLOCK_UI_STYLE_INSTRUMENT:
        lv_obj_set_size(grab, 56, 4);
        lv_obj_set_pos(grab, 332, 232 - l.open_y);
        lv_obj_set_style_bg_color(grab, lv_color_mix(sk->text_3, sk->line_strong, 80), 0);
        break;
    case CLOCK_UI_STYLE_ATELIER:
        lv_obj_set_size(grab, 56, 4);
        lv_obj_set_pos(grab, 332, 214 - l.open_y);
        lv_obj_set_style_bg_color(grab, sk->accent_deep, 0);
        break;
    default:
        lv_obj_set_size(grab, 44, 5);
        lv_obj_set_pos(grab, 338 - l.x, 226 - l.open_y);
        lv_obj_set_style_bg_color(grab, lv_color_mix(sk->text_3, sk->line_strong, 128), 0);
        break;
    }

    build_brightness(ctx, &l);
    for (int k = 0; k < QUICK_KEY_COUNT; k++) {
        build_key(ctx, (quick_key_t)k, icons[si][k], &l);
    }
    update_brightness_ui(ctx);
    sync_keys(ctx, true);
}

void face_quick_destroy(clock_ui_context_t *ctx)
{
    face_sheet_destroy(&ctx->brightness.sheet);
}

void face_quick_sync(clock_ui_context_t *ctx)
{
    update_brightness_ui(ctx);
    sync_keys(ctx, false);
}

void face_quick_open(clock_ui_context_t *ctx)
{
    if (ctx->brightness.panel == NULL) {
        return;
    }
    face_quick_sync(ctx);
    face_sheet_open(&ctx->brightness.sheet, true);
}

void face_quick_close(clock_ui_context_t *ctx, bool animate)
{
    face_sheet_close(&ctx->brightness.sheet, animate);
}

bool face_quick_visible(const clock_ui_context_t *ctx)
{
    return face_sheet_is_visible(&ctx->brightness.sheet);
}

/* --- Bottom edge ------------------------------------------------------------------------- */

static void edge_event_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(e);
    clock_ui_brightness_state_t *q = &ctx->brightness;
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_event_get_indev(e);
    lv_point_t p;
    int32_t dx;
    int32_t dy;

    if (indev == NULL || ui_settings_is_open(ctx) || ui_alarms_is_open(ctx)) {
        q->edge_tracking = false;
        return;
    }
    lv_indev_get_point(indev, &p);
    if (code == LV_EVENT_PRESSED) {
        q->edge_start_point = p;
        q->edge_tracking = true;
        q->edge_pulling = false;
        return;
    }
    if (!q->edge_tracking) {
        return;
    }
    dx = p.x - q->edge_start_point.x;
    dy = p.y - q->edge_start_point.y;

    if (code == LV_EVENT_PRESSING) {
        if (!q->edge_pulling) {
            if (dy > -EDGE_TRIGGER || LV_ABS(dy) < LV_ABS(dx) + 8 || ctx->faces.face_reveal_animating) {
                return;
            }
            q->edge_pulling = true;
            face_quick_sync(ctx);
            face_sheet_drag_begin(&q->sheet, q->edge_start_point, true);
        }
        face_sheet_drag_to(&q->sheet, dy);
        return;
    }

    q->edge_tracking = false;
    if (q->edge_pulling) {
        q->edge_pulling = false;
        face_sheet_drag_end(&q->sheet, code == LV_EVENT_PRESS_LOST);
        return;
    }
    if (code != LV_EVENT_RELEASED) {
        return;
    }
    if (LV_ABS(dx) < 24 && LV_ABS(dy) < 24) {
        /* A tap on the pull hint at 6 opens the sheet; elsewhere it toggles the controls. */
        if (face_controls_visible(ctx) && p.y >= 636 && LV_ABS(p.x - KIT_CENTER) <= 110) {
            face_quick_open(ctx);
        } else {
            face_controls_toggle(ctx);
        }
        return;
    }
    if (LV_ABS(dx) >= 56 && LV_ABS(dx) > LV_ABS(dy) + 20) {
        face_shell_swipe_face(ctx, dx);
    }
}

void face_quick_create_edge_sensor(clock_ui_context_t *ctx)
{
    lv_obj_t *sensor = face_plain(ui_chrome_root(ctx));

    lv_obj_set_size(sensor, SCREEN_SIZE, BOTTOM_EDGE_ZONE);
    lv_obj_align(sensor, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(sensor, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(sensor, edge_event_cb, LV_EVENT_PRESSED, ctx);
    lv_obj_add_event_cb(sensor, edge_event_cb, LV_EVENT_PRESSING, ctx);
    lv_obj_add_event_cb(sensor, edge_event_cb, LV_EVENT_RELEASED, ctx);
    lv_obj_add_event_cb(sensor, edge_event_cb, LV_EVENT_PRESS_LOST, ctx);
    ctx->brightness.edge_sensor = sensor;
}

void face_quick_set_hidden(clock_ui_context_t *ctx, bool hidden)
{
    lv_obj_t *sensor = ctx->brightness.edge_sensor;

    if (sensor == NULL) {
        return;
    }
    if (hidden) {
        lv_obj_add_flag(sensor, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(sensor, LV_OBJ_FLAG_HIDDEN);
    }
}

/* --- Old entry points (clock_ui_private.h) ----------------------------------------------- */

bool brightness_panel_is_open(const clock_ui_context_t *ctx)
{
    return face_quick_visible(ctx);
}

void brightness_overlay_hide_immediately(clock_ui_context_t *ctx)
{
    face_quick_close(ctx, false);
}

void brightness_panel_hide(clock_ui_context_t *ctx)
{
    face_quick_close(ctx, false);
}

void show_affordances_temporarily(clock_ui_context_t *ctx)
{
    face_controls_show(ctx);
}
