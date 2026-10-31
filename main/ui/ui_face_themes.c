/*
 * Theme sheet (screen 02), pulled down from the top: the three themes of the current face,
 * each with a round preview, its name and three swatches (dial, marks, hands).
 *
 * The previews are pre-rendered (main/ui/assets_src/face/thumbs/f<face>_t<theme>.png: the
 * simulator's render of each face in each theme at 10:09:32, masked round and scaled to
 * 132 px; rebuild with `python3 scripts/ui_assets/build_ui_assets.py face`). They are opaque
 * RGB565 and drawn with a round clip, so the sheet needs no render of the faces when it
 * opens.
 */
#include <string.h>

#include "assets/ui_assets/face_assets.h"
#include "domain/face_catalog.h"
#include "ui/ui_areas.h"
#include "ui/ui_face_chrome.h"

#define THEME_AUTO_CLOSE_MS 10000
#define THUMB 132

static const ui_l10n_str_t s_eyebrow_instrument = UI_L10N_STR(
    "Face %02d / %02d · %s", "Tarcza %02d / %02d · %s", "Циферблат %02d / %02d · %s",
    "Циферблат %02d / %02d · %s", "Wijzerplaat %02d / %02d · %s");
static const ui_l10n_str_t s_eyebrow_atelier = UI_L10N_STR(
    "%s · face %d of %d", "%s · tarcza %d z %d", "%s · циферблат %d з %d", "%s · циферблат %d из %d",
    "%s · wijzerplaat %d van %d");

/* Theme names, by the colour that sets each theme apart. */
static const ui_l10n_str_t s_green = UI_L10N_STR("Green", "Zieleń", "Зелений", "Зелёный", "Groen");
static const ui_l10n_str_t s_ice = UI_L10N_STR("Ice", "Lód", "Лід", "Лёд", "IJs");
static const ui_l10n_str_t s_amber = UI_L10N_STR("Amber", "Bursztyn", "Бурштин", "Янтарь", "Amber");
static const ui_l10n_str_t s_gold = UI_L10N_STR("Gold", "Złoto", "Золото", "Золото", "Goud");
static const ui_l10n_str_t s_rose = UI_L10N_STR("Rose", "Róż", "Рожевий", "Розовый", "Roze");
static const ui_l10n_str_t s_porcelain = UI_L10N_STR("Porcelain", "Porcelana", "Порцеляна", "Фарфор", "Porselein");
static const ui_l10n_str_t s_copper = UI_L10N_STR("Copper", "Miedź", "Мідь", "Медь", "Koper");
static const ui_l10n_str_t s_midnight = UI_L10N_STR("Midnight", "Północ", "Опівніч", "Полночь", "Middernacht");
static const ui_l10n_str_t s_apricot = UI_L10N_STR("Apricot", "Morela", "Абрикос", "Абрикос", "Abrikoos");
static const ui_l10n_str_t s_navy = UI_L10N_STR("Navy", "Granat", "Темно-синій", "Тёмно-синий", "Marine");
static const ui_l10n_str_t s_forest = UI_L10N_STR("Forest", "Las", "Ліс", "Лес", "Woud");
static const ui_l10n_str_t s_blue = UI_L10N_STR("Blue", "Błękit", "Синій", "Синий", "Blauw");
static const ui_l10n_str_t s_red = UI_L10N_STR("Red", "Czerwień", "Червоний", "Красный", "Rood");
static const ui_l10n_str_t s_graphite = UI_L10N_STR("Graphite", "Grafit", "Графіт", "Графит", "Grafiet");
static const ui_l10n_str_t s_ember = UI_L10N_STR("Ember", "Żar", "Жар", "Жар", "Gloed");
static const ui_l10n_str_t s_rosewood = UI_L10N_STR("Rosewood", "Palisander", "Палісандр", "Палисандр", "Palissander");
static const ui_l10n_str_t s_kiln = UI_L10N_STR("Kiln", "Piec", "Піч", "Печь", "Oven");
static const ui_l10n_str_t s_indigo = UI_L10N_STR("Indigo", "Indygo", "Індиго", "Индиго", "Indigo");
static const ui_l10n_str_t s_candle = UI_L10N_STR("Candle", "Świeca", "Свічка", "Свеча", "Kaars");
static const ui_l10n_str_t s_sea_glass = UI_L10N_STR("Sea glass", "Szkło morskie", "Морське скло", "Морское стекло",
                                                     "Zeeglas");
static const ui_l10n_str_t s_argon = UI_L10N_STR("Argon", "Argon", "Аргон", "Аргон", "Argon");
static const ui_l10n_str_t s_mercury = UI_L10N_STR("Mercury", "Rtęć", "Ртуть", "Ртуть", "Kwik");

static const ui_l10n_str_t *theme_name(clock_face_id_t face, uint8_t theme)
{
    static const ui_l10n_str_t *const names[CLOCK_FACE_COUNT][CLOCK_FACE_THEME_COUNT] = {
        [CLOCK_FACE_DIGITAL] = {&s_green, &s_ice, &s_amber},
        [CLOCK_FACE_MATRIX] = {&s_green, &s_amber, &s_ice},
        [CLOCK_FACE_WHARTON] = {&s_gold, &s_rose, &s_ice},
        [CLOCK_FACE_STERNGLAS] = {&s_porcelain, &s_copper, &s_midnight},
        [CLOCK_FACE_AVENIR] = {&s_apricot, &s_navy, &s_forest},
        [CLOCK_FACE_MODERN_SILVER] = {&s_blue, &s_red, &s_graphite},
        [CLOCK_FACE_EMBER] = {&s_ember, &s_rosewood, &s_kiln},
        [CLOCK_FACE_NOCTURNE] = {&s_indigo, &s_candle, &s_sea_glass},
        [CLOCK_FACE_FILAMENT] = {&s_amber, &s_argon, &s_mercury},
    };

    if (face >= CLOCK_FACE_COUNT || theme >= CLOCK_FACE_THEME_COUNT) {
        return NULL;
    }
    return names[face][theme];
}

static const lv_image_dsc_t *theme_thumb(clock_face_id_t face, uint8_t theme)
{
    static const lv_image_dsc_t *const thumbs[CLOCK_FACE_COUNT][CLOCK_FACE_THEME_COUNT] = {
        [CLOCK_FACE_DIGITAL] = {&ui_face_thumbs_f0_t0, &ui_face_thumbs_f0_t1, &ui_face_thumbs_f0_t2},
        [CLOCK_FACE_MATRIX] = {&ui_face_thumbs_f1_t0, &ui_face_thumbs_f1_t1, &ui_face_thumbs_f1_t2},
        [CLOCK_FACE_WHARTON] = {&ui_face_thumbs_f2_t0, &ui_face_thumbs_f2_t1, &ui_face_thumbs_f2_t2},
        [CLOCK_FACE_STERNGLAS] = {&ui_face_thumbs_f5_t0, &ui_face_thumbs_f5_t1, &ui_face_thumbs_f5_t2},
        [CLOCK_FACE_AVENIR] = {&ui_face_thumbs_f6_t0, &ui_face_thumbs_f6_t1, &ui_face_thumbs_f6_t2},
        [CLOCK_FACE_MODERN_SILVER] = {&ui_face_thumbs_f7_t0, &ui_face_thumbs_f7_t1, &ui_face_thumbs_f7_t2},
        [CLOCK_FACE_EMBER] = {&ui_face_thumbs_f8_t0, &ui_face_thumbs_f8_t1, &ui_face_thumbs_f8_t2},
        [CLOCK_FACE_NOCTURNE] = {&ui_face_thumbs_f9_t0, &ui_face_thumbs_f9_t1, &ui_face_thumbs_f9_t2},
        [CLOCK_FACE_FILAMENT] = {&ui_face_thumbs_f10_t0, &ui_face_thumbs_f10_t1, &ui_face_thumbs_f10_t2},
    };

    if (face >= CLOCK_FACE_COUNT || theme >= CLOCK_FACE_THEME_COUNT) {
        return NULL;
    }
    return thumbs[face][theme];
}

static uint8_t face_theme_value(const clock_ui_context_t *ctx, clock_face_id_t face)
{
    if (!face_catalog_is_valid(face)) {
        return 0;
    }
    return ctx->settings->face_themes[face] < CLOCK_FACE_THEME_COUNT ? ctx->settings->face_themes[face] : 0;
}

/* --- Layout per style ------------------------------------------------------------------ */

typedef struct {
    int32_t cx[CLOCK_FACE_THEME_COUNT]; /* thumb centres, panel coordinates */
    int32_t cy;
    int32_t text_y;   /* top of the key / name column */
    int32_t text_w;
    int32_t name_gap; /* key to name */
    int32_t swatch_y; /* top of the swatches */
    int32_t swatch;
    int32_t swatch_gap;
    int32_t swatch_radius;
    int32_t ring_pad;
    int32_t ring_w;
} theme_layout_t;

static theme_layout_t theme_layout(void)
{
    switch (ui_skin_style()) {
    case CLOCK_UI_STYLE_INSTRUMENT:
        return (theme_layout_t) {{192, 360, 528}, 238, 332, 160, 6, 404, 20, 8, 4, 6, 2};
    case CLOCK_UI_STYLE_ATELIER:
        return (theme_layout_t) {{180, 360, 540}, 270, 361, 180, 8, 442, 20, 10, LV_RADIUS_CIRCLE, 7, 2};
    case CLOCK_UI_STYLE_ORBIT:
    default:
        /* Panel coordinates: the card's top is 88 px above the screen. */
        return (theme_layout_t) {{196 - 60, 360 - 60, 524 - 60}, 262 + 88, 350 + 88, 160, 0, 388 + 88, 22, 8,
                                 LV_RADIUS_CIRCLE, 5, 4};
    }
}

/* --- Selection -------------------------------------------------------------------------- */

static void apply_selection(clock_ui_context_t *ctx, uint8_t selected)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_face_theme_state_t *ft = &ctx->face_theme;
    theme_layout_t l = theme_layout();
    clock_ui_style_t style = ui_skin_style();
    lv_color_t ring = style == CLOCK_UI_STYLE_ORBIT ? sk->tint_themes : sk->accent;

    for (uint8_t t = 0; t < CLOCK_FACE_THEME_COUNT; t++) {
        bool on = t == selected;
        lv_obj_t *thumb = ft->option_thumb[t];

        if (on) {
            lv_obj_set_style_outline_width(thumb, l.ring_w, 0);
            lv_obj_set_style_outline_pad(thumb, l.ring_pad, 0);
            lv_obj_set_style_outline_color(thumb, ring, 0);
            lv_obj_set_style_outline_opa(thumb, LV_OPA_COVER, 0);
        } else if (style == CLOCK_UI_STYLE_ORBIT) {
            lv_obj_set_style_outline_width(thumb, 0, 0);
        } else {
            lv_obj_set_style_outline_width(thumb, 1, 0);
            lv_obj_set_style_outline_pad(thumb, 0, 0);
            lv_obj_set_style_outline_color(thumb, style == CLOCK_UI_STYLE_ATELIER ? lv_color_white() : sk->line_strong, 0);
            lv_obj_set_style_outline_opa(thumb, style == CLOCK_UI_STYLE_ATELIER ? 20 : LV_OPA_COVER, 0);
        }
        if (ft->option_key[t] != NULL) {
            lv_obj_set_style_text_color(ft->option_key[t], on ? sk->accent : sk->label, 0);
        }
        switch (style) {
        case CLOCK_UI_STYLE_INSTRUMENT:
            lv_obj_set_style_text_font(ft->option_name[t], on ? sk->font_value : sk->font_heading, 0);
            lv_obj_set_style_text_color(ft->option_name[t], on ? sk->text : sk->text_2, 0);
            break;
        case CLOCK_UI_STYLE_ATELIER:
            lv_obj_set_style_text_color(ft->option_name[t], on ? sk->text : sk->value, 0);
            break;
        default:
            lv_obj_set_style_text_color(ft->option_name[t], on ? sk->text : sk->text_2, 0);
            break;
        }
    }
    if (ft->check != NULL) {
        if (style == CLOCK_UI_STYLE_ORBIT) {
            lv_obj_set_pos(ft->check, l.cx[selected] + 32, l.cy + 28);
        } else {
            lv_obj_set_pos(ft->check, l.cx[selected] - 19, l.cy + 58);
        }
    }
    face_sheet_mark_dirty(&ft->sheet);
}

void face_themes_sync(clock_ui_context_t *ctx)
{
    clock_ui_face_theme_state_t *ft = &ctx->face_theme;
    clock_face_id_t face;
    uint8_t selected;
    uint32_t key;
    int index;
    char text[96];
    char caps[96];

    if (ft->panel == NULL) {
        return;
    }
    face = sanitize_enabled_face(ft->picker_face);
    selected = face_theme_value(ctx, face);
    key = 0x80000000U | ((uint32_t)ctx->settings->language << 16) | ((uint32_t)selected << 8) | (uint32_t)face;
    if (ft->synced_key == key) {
        return;
    }
    ft->synced_key = key;

    index = face_catalog_visible_id_to_index(face) + 1;
    switch (ui_skin_style()) {
    case CLOCK_UI_STYLE_INSTRUMENT:
        snprintf(text, sizeof(text), ui_l10n_str(ctx, &s_eyebrow_instrument), index, face_catalog_visible_count(),
                 face_catalog_name(face));
        break;
    case CLOCK_UI_STYLE_ATELIER:
        snprintf(text, sizeof(text), ui_l10n_str(ctx, &s_eyebrow_atelier), face_catalog_name(face), index,
                 face_catalog_visible_count());
        break;
    default:
        snprintf(text, sizeof(text), "%s", face_catalog_name(face));
        break;
    }
    if (ui_skin_style() == CLOCK_UI_STYLE_ORBIT) {
        kit_label_set_text_if_changed(ft->eyebrow, text);
    } else {
        kit_label_set_text_if_changed(ft->eyebrow, face_caps(caps, sizeof(caps), text));
    }

    for (uint8_t t = 0; t < CLOCK_FACE_THEME_COUNT; t++) {
        const ui_l10n_str_t *name = theme_name(face, t);
        const lv_image_dsc_t *thumb = theme_thumb(face, t);
        uint32_t swatches[3];

        ft->option_ctx[t].ui = ctx;
        ft->option_ctx[t].face = face;
        ft->option_ctx[t].theme = t;
        if (thumb != NULL && lv_image_get_src(ft->option_thumb[t]) != thumb) {
            lv_image_set_src(ft->option_thumb[t], thumb);
        }
        kit_label_set_text_if_changed(ft->option_name[t], name != NULL ? ui_l10n_str(ctx, name) : "");
        face_theme_preview_colors(face, t, swatches);
        for (int s = 0; s < 3; s++) {
            lv_obj_set_style_bg_color(ft->option_swatches[t][s], lv_color_hex(swatches[s]), 0);
        }
    }
    apply_selection(ctx, selected);
}

/* --- Events ----------------------------------------------------------------------------- */

static void option_event_cb(lv_event_t *e)
{
    face_theme_option_ctx_t *opt = (face_theme_option_ctx_t *)lv_event_get_user_data(e);
    clock_ui_context_t *ctx = opt->ui;
    clock_ui_face_theme_state_t *ft = &ctx->face_theme;
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *thumb = ft->option_thumb[opt->theme];

    if (code == LV_EVENT_PRESSED) {
        lv_obj_add_state(thumb, LV_STATE_PRESSED);
        return;
    }
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST || code == LV_EVENT_PRESSING) {
        if (code != LV_EVENT_PRESSING || ft->drag_moved) {
            lv_obj_remove_state(thumb, LV_STATE_PRESSED);
        }
        return;
    }
    if (code != LV_EVENT_CLICKED || face_sheet_click_blocked(&ft->sheet) || !ft->target_open) {
        return;
    }
    /* Show the new selection in the sheet as it closes; the face changes below it. */
    ft->synced_key = 0x80000000U | ((uint32_t)ctx->settings->language << 16) | ((uint32_t)opt->theme << 8) |
                     (uint32_t)opt->face;
    apply_selection(ctx, opt->theme);
    request_set_face_theme(ctx, opt->face, opt->theme);
    face_sheet_close(&ft->sheet, true);
}

static void themes_on_state(clock_ui_context_t *ctx)
{
    if (face_sheet_is_visible(&ctx->face_theme.sheet)) {
        face_controls_hide(ctx);
        face_quick_close(ctx, false);
    }
    face_shell_sync_face_visuals(ctx);
}

/* --- Building --------------------------------------------------------------------------- */

static lv_obj_t *centered_label(lv_obj_t *parent, kit_text_role_t role, int32_t y, int32_t parent_w)
{
    lv_obj_t *label = kit_label(parent, role, "");

    lv_obj_set_width(label, parent_w - 80);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(label, 40, y);
    return label;
}

static void build_option(clock_ui_context_t *ctx, uint8_t t, const theme_layout_t *l)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_face_theme_state_t *ft = &ctx->face_theme;
    lv_obj_t *panel = ft->panel;
    clock_ui_style_t style = ui_skin_style();
    int32_t cx = l->cx[t];
    int32_t col_x = cx - l->text_w / 2;
    int32_t y = l->text_y;
    int32_t sw_total = 3 * l->swatch + 2 * l->swatch_gap;
    lv_obj_t *thumb;
    lv_obj_t *card;

    thumb = lv_image_create(panel);
    lv_obj_remove_flag(thumb, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(thumb, THUMB, THUMB);
    lv_obj_set_pos(thumb, cx - THUMB / 2, l->cy - THUMB / 2);
    lv_obj_set_style_radius(thumb, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_image_opa(thumb, LV_OPA_70, LV_STATE_PRESSED);
    ft->option_thumb[t] = thumb;

    if (style != CLOCK_UI_STYLE_ORBIT) {
        static const l10n_text_id_t ids[CLOCK_FACE_THEME_COUNT] = {L10N_THEME_1, L10N_THEME_2, L10N_THEME_3};

        ft->option_key[t] = kit_label(panel, KIT_TEXT_LABEL, "");
        lv_obj_set_width(ft->option_key[t], l->text_w);
        lv_obj_set_style_text_align(ft->option_key[t], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(ft->option_key[t], col_x, y);
        ui_l10n_bind_kit(ctx, ft->option_key[t], ids[t]);
        y += lv_font_get_line_height(sk->font_label) + l->name_gap;
    }
    ft->option_name[t] = face_text(panel, style == CLOCK_UI_STYLE_INSTRUMENT ? sk->font_heading : sk->font_value,
                                   sk->text_2, "");
    lv_obj_set_width(ft->option_name[t], l->text_w + 16);
    lv_obj_set_style_text_align(ft->option_name[t], LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(ft->option_name[t], LV_LABEL_LONG_DOT);
    lv_obj_set_pos(ft->option_name[t], col_x - 8, y);

    for (int s = 0; s < 3; s++) {
        lv_obj_t *sw = kit_plain(panel);

        lv_obj_set_size(sw, l->swatch, l->swatch);
        lv_obj_set_pos(sw, cx - sw_total / 2 + s * (l->swatch + l->swatch_gap), l->swatch_y);
        lv_obj_set_style_radius(sw, l->swatch_radius, 0);
        lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(sw, 1, 0);
        if (style == CLOCK_UI_STYLE_ATELIER) {
            lv_obj_set_style_border_color(sw, sk->line_strong, 0);
        } else {
            lv_obj_set_style_border_color(sw, style == CLOCK_UI_STYLE_ORBIT ? lv_color_white() : sk->text, 0);
            lv_obj_set_style_border_opa(sw, 56, 0);
        }
        ft->option_swatches[t][s] = sw;
    }

    /* The touch target: thumb, names and swatches. Drags bubble to the sheet. */
    card = kit_plain(panel);
    lv_obj_set_pos(card, cx - 84, l->cy - THUMB / 2 - 12);
    lv_obj_set_size(card, 168, l->swatch_y + l->swatch + 12 - (l->cy - THUMB / 2 - 12));
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
    ui_attach_click_feedback(card, LV_EVENT_CLICKED);
    ft->option_ctx[t] = (face_theme_option_ctx_t) {.ui = ctx, .face = CLOCK_FACE_DIGITAL, .theme = t};
    lv_obj_add_event_cb(card, option_event_cb, LV_EVENT_PRESSED, &ft->option_ctx[t]);
    lv_obj_add_event_cb(card, option_event_cb, LV_EVENT_PRESSING, &ft->option_ctx[t]);
    lv_obj_add_event_cb(card, option_event_cb, LV_EVENT_RELEASED, &ft->option_ctx[t]);
    lv_obj_add_event_cb(card, option_event_cb, LV_EVENT_PRESS_LOST, &ft->option_ctx[t]);
    lv_obj_add_event_cb(card, option_event_cb, LV_EVENT_CLICKED, &ft->option_ctx[t]);
    ft->option_card[t] = card;
}

/* Ticks from 113 to 247 degrees (past 4 to before 8 o'clock) are left out. */
#define THEME_RIM_SKIP_FROM 113
#define THEME_RIM_SKIP_TO 247

static void atelier_rim_cb(lv_event_t *e)
{
    lv_obj_t *panel = lv_event_get_current_target(e);
    lv_area_t a;

    /* The chapter ring of the bezel continues across the sheet (from 8 to 4 o'clock), drawn
     * around the panel's own centre point so its snapshot matches. It is the kit's cached
     * minute rim: a live 712 px arc and 60 line ticks cost about 30 ms a frame. */
    lv_obj_get_coords(panel, &a);
    kit_draw_minute_rim_at(lv_event_get_layer(e), a.x1 + KIT_CENTER, a.y1 + KIT_CENTER, THEME_RIM_SKIP_FROM,
                           THEME_RIM_SKIP_TO);
}

void face_themes_create(clock_ui_context_t *ctx)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_face_theme_state_t *ft = &ctx->face_theme;
    clock_ui_style_t style = ui_skin_style();
    theme_layout_t l = theme_layout();
    face_sheet_opts_t o = {
        .from_top = true,
        .x = 0,
        .width = SCREEN_SIZE,
        .height = 504,
        .open_y = 0,
        .radius = 0,
        .color = sk->sheet,
        .border = sk->line_strong,
        .border_side = LV_BORDER_SIDE_BOTTOM,
        .auto_close_ms = THEME_AUTO_CLOSE_MS,
        .on_state = themes_on_state,
    };
    lv_obj_t *grab;

    if (style == CLOCK_UI_STYLE_ORBIT) {
        o.x = 60;
        o.width = 600;
        o.height = 572;
        o.open_y = -88;
        o.radius = 64;
        o.color = sk->surface;
        o.border_side = LV_BORDER_SIDE_NONE;
    } else if (style == CLOCK_UI_STYLE_ATELIER) {
        o.height = 512;
    }
    face_sheet_create(&ft->sheet, ctx, &o);
    ft->picker_face = sanitize_enabled_face(ctx->settings->current_face);

    switch (style) {
    case CLOCK_UI_STYLE_INSTRUMENT:
        ft->eyebrow = centered_label(ft->panel, KIT_TEXT_LABEL, 62, o.width);
        ft->title = centered_label(ft->panel, KIT_TEXT_TITLE, 90, o.width);
        break;
    case CLOCK_UI_STYLE_ATELIER:
        lv_obj_add_event_cb(ft->panel, atelier_rim_cb, LV_EVENT_DRAW_MAIN_END, NULL);
        kit_minute_rim_prewarm(THEME_RIM_SKIP_FROM, THEME_RIM_SKIP_TO);
        ft->eyebrow = centered_label(ft->panel, KIT_TEXT_LABEL, 62, o.width);
        ft->title = centered_label(ft->panel, KIT_TEXT_TITLE, 92, o.width);
        face_engraved_rule(ft->panel, 155, 240);
        break;
    case CLOCK_UI_STYLE_ORBIT:
    default:
        ft->title = centered_label(ft->panel, KIT_TEXT_TITLE, 88 + 88, o.width);
        ft->eyebrow = centered_label(ft->panel, KIT_TEXT_BODY, 138 + 88, o.width);
        break;
    }
    ui_l10n_bind_kit(ctx, ft->title, L10N_FACE_THEMES);

    for (uint8_t t = 0; t < CLOCK_FACE_THEME_COUNT; t++) {
        build_option(ctx, t, &l);
    }

    if (style != CLOCK_UI_STYLE_INSTRUMENT) {
        lv_obj_t *icon;

        ft->check = kit_plain(ft->panel);
        if (style == CLOCK_UI_STYLE_ORBIT) {
            lv_obj_set_size(ft->check, 40, 40);
            lv_obj_set_style_bg_color(ft->check, sk->tint_themes, 0);
            lv_obj_set_style_border_width(ft->check, 4, 0);
            lv_obj_set_style_border_color(ft->check, sk->surface, 0);
            icon = kit_icon_small(ft->check, KIT_ICON_CHECK, lv_color_white());
        } else {
            lv_obj_set_size(ft->check, 38, 38);
            lv_obj_set_style_bg_color(ft->check, sk->accent, 0);
            lv_obj_set_style_border_width(ft->check, 4, 0);
            lv_obj_set_style_border_color(ft->check, sk->sheet, 0);
            icon = kit_icon_small(ft->check, KIT_ICON_CHECK, sk->accent_text);
        }
        lv_obj_set_style_radius(ft->check, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(ft->check, LV_OPA_COVER, 0);
        lv_obj_center(icon);
    }

    grab = kit_plain(ft->panel);
    lv_obj_set_style_bg_opa(grab, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(grab, 3, 0);
    switch (style) {
    case CLOCK_UI_STYLE_INSTRUMENT:
        lv_obj_set_size(grab, 56, 4);
        lv_obj_set_pos(grab, 332, 472);
        lv_obj_set_style_bg_color(grab, lv_color_mix(sk->text_3, sk->line_strong, 80), 0);
        break;
    case CLOCK_UI_STYLE_ATELIER:
        lv_obj_set_size(grab, 56, 4);
        lv_obj_set_pos(grab, 332, 490);
        lv_obj_set_style_bg_color(grab, sk->accent_deep, 0);
        break;
    default:
        lv_obj_set_size(grab, 44, 5);
        lv_obj_set_pos(grab, 338 - 60, 458 + 88);
        lv_obj_set_style_bg_color(grab, lv_color_mix(sk->text_3, sk->line_strong, 128), 0);
        break;
    }

    face_themes_sync(ctx);
}

void face_themes_destroy(clock_ui_context_t *ctx)
{
    face_sheet_destroy(&ctx->face_theme.sheet);
}

void face_themes_prepare(clock_ui_context_t *ctx)
{
    face_glass_capture(ctx, 0 /* UI_GLASS_FACE: sheets are over the face */);
    ctx->face_theme.picker_face = face_shell_active_face(ctx);
    face_themes_sync(ctx);
}

void face_themes_open(clock_ui_context_t *ctx)
{
    if (ctx->face_theme.panel == NULL) {
        return;
    }
    face_themes_prepare(ctx);
    face_sheet_open(&ctx->face_theme.sheet, true);
}

void face_themes_close(clock_ui_context_t *ctx, bool animate)
{
    face_sheet_close(&ctx->face_theme.sheet, animate);
}

bool face_themes_visible(const clock_ui_context_t *ctx)
{
    return face_sheet_is_visible(&ctx->face_theme.sheet);
}

/* --- Old entry points (clock_ui_private.h) ----------------------------------------------- */
