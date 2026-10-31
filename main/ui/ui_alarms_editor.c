/*
 * Alarm editor (screen 10), in the three styles.
 *
 *   Instrument: one page. Counter drums in a recessed window, Cancel and Delete flank it,
 *               then repeat presets, day keys, sound with Test, math, Save at six.
 *   Orbit:      two pages on a vertical pager: the 24 h ring with rollers, then the options
 *               (repeat, days, sound, math). Delete and Save stay at six on both pages.
 *   Atelier:    two pages: the watch-setting rings with Cancel/Confirm in the centre, then
 *               the details with Delete and Save.
 *
 * Existing alarms also show an Enabled switch and Delete; new alarms are enabled and show
 * neither. The time setters are in ui_alarms_timeset.c.
 */
#include "ui/ui_alarms_internal.h"

#include "assets/skin_fonts/skin_fonts.h"
#include "assets/ui_assets/alarms_assets.h"

/* Instrument page geometry (design/ui_concepts/instrument.html, screen 10). */
#define I_SAVE_Y_NEW 604
#define I_ROWS_Y 444
/* Orbit: options page geometry. */
#define O_KEYS_Y 564
/* Atelier: details page. */
#define A_ROWS_Y 371

static void atelier_rim_cb(lv_event_t *e)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_context_t *ctx = lv_event_get_user_data(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    bool details = ctx->alarms.editor_on_details;

    kit_draw_minute_rim(layer, details ? 54 : -1, details ? 126 : -1);
    if (details) {
        lv_draw_arc_dsc_t arc;

        /* Register at three o'clock: this page is the lower half of the editor. */
        lv_draw_arc_dsc_init(&arc);
        arc.center.x = KIT_CENTER;
        arc.center.y = KIT_CENTER;
        arc.radius = 344;
        arc.width = 4;
        arc.rounded = 1;
        arc.color = sk->line;
        arc.start_angle = 330;
        arc.end_angle = 30;
        kit_draw_arc(layer, &arc);
        arc.color = sk->accent;
        arc.start_angle = 6;
        arc.end_angle = 30;
        kit_draw_arc(layer, &arc);
    }
}

/* --- Callbacks --------------------------------------------------------------------------- */

static void close_cb(lv_event_t *e)
{
    alm_editor_close(lv_event_get_user_data(e));
}

static void save_cb(lv_event_t *e)
{
    alm_editor_save(lv_event_get_user_data(e));
}

static void delete_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);

    if (ctx->alarms.editor_index < 0 || ctx->alarms.editor_is_new) {
        return;
    }
    request_delete_alarm(ctx, (uint8_t)ctx->alarms.editor_index);
    alm_editor_close(ctx);
}

static void seg_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);
    int preset = kit_segmented_get(lv_event_get_target(e));

    if (preset >= 0 && preset < 4) {
        apply_repeat_preset_to_alarm(&ctx->alarms.editor_draft, (uint8_t)preset);
    }
    alm_editor_sync(ctx);
}

static void day_cb(lv_event_t *e)
{
    alarm_day_ctx_t *dctx = lv_event_get_user_data(e);
    clock_ui_context_t *ctx = dctx->ui;
    alarm_config_t *d = &ctx->alarms.editor_draft;
    uint8_t bit = (uint8_t)(1U << dctx->day_index);

    if (d->repeat_mode == ALARM_REPEAT_ONCE) {
        d->repeat_mode = ALARM_REPEAT_WEEKLY;
        d->days_mask = bit;
    } else if (d->days_mask & bit) {
        d->days_mask &= (uint8_t)~bit;
        if (d->days_mask == 0) {
            apply_repeat_preset_to_alarm(d, ALARM_REPEAT_PRESET_ONCE); /* no day left: once */
        }
    } else {
        d->days_mask |= bit;
    }
    alm_editor_sync(ctx);
}

static void test_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);

    alm_test_toggle(ctx);
    alm_editor_sync(ctx);
}

/* Tapping the sound row steps to the next sound and plays it. */
static void sound_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);
    alarm_config_t *d = &ctx->alarms.editor_draft;

    d->sound = (uint8_t)((d->sound + 1) % ALARM_SOUND_COUNT);
    alm_sound_preview(ctx);
    alm_editor_sync(ctx);
}

static void math_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);

    if (!ctx->suppress_events) {
        ctx->alarms.editor_draft.math_unlock_enabled = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    }
}

static void enabled_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);

    if (!ctx->suppress_events) {
        ctx->alarms.editor_draft.enabled = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    }
}

static void details_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);

    alm_editor_show_details(ctx, !ctx->alarms.editor_on_details, true);
}

/* The pager settles on a page: a third of a page commits the swipe, less springs back. */
static void pager_scroll_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);
    clock_ui_alarm_state_t *a = &ctx->alarms;
    int32_t y = lv_obj_get_scroll_y(a->editor_scroll);
    bool details;

    if (lv_event_get_code(e) == LV_EVENT_SCROLL_BEGIN) {
        return;
    }
    if (y != 0 && y != KIT_SCREEN) {
        lv_indev_t *indev = lv_indev_active();

        if (indev != NULL && lv_indev_get_scroll_obj(indev) == a->editor_scroll &&
            lv_indev_get_state(indev) == LV_INDEV_STATE_PRESSED) {
            return; /* still under the finger */
        }
        details = a->editor_on_details ? y > KIT_SCREEN * 2 / 3 : y > KIT_SCREEN / 3;
        lv_obj_scroll_to_y(a->editor_scroll, details ? KIT_SCREEN : 0, LV_ANIM_ON);
        return;
    }
    details = y == KIT_SCREEN;
    if (details == a->editor_on_details) {
        return;
    }
    a->editor_on_details = details;
    if (a->editor_dots[0] != NULL) {
        const ui_skin_t *sk = ui_skin();

        for (int i = 0; i < 2; i++) {
            bool cur = (i == 1) == details;

            lv_obj_set_style_bg_color(a->editor_dots[i], cur ? sk->text : sk->text_3, 0);
        }
    }
    kit_show(a->editor_bottom_scrim, details);
    if (ui_skin_is_atelier()) {
        lv_obj_invalidate(a->editor_root); /* rim register */
    }
}

/* --- Shared controls --------------------------------------------------------------------- */

static void create_repeat(clock_ui_context_t *ctx, lv_obj_t *parent, int32_t x, int32_t y, int32_t w)
{
    const ui_skin_t *sk = ui_skin();
    const char *labels[4] = {alm_text(ctx, L10N_ONCE), alm_text(ctx, L10N_EVERY_DAY), alm_text(ctx, L10N_WEEKDAYS),
                             alm_text(ctx, L10N_WEEKENDS)};
    lv_obj_t *seg = kit_segmented(parent, labels, 4, -1);

    ctx->alarms.editor_seg = seg;
    lv_obj_set_width(seg, w);
    lv_obj_set_pos(seg, x, y);
    kit_set_tint(seg, sk->tint_alarms);
    lv_obj_add_event_cb(seg, seg_cb, LV_EVENT_VALUE_CHANGED, ctx);
}

static void create_days(clock_ui_context_t *ctx, lv_obj_t *parent, int32_t x, int32_t y, int32_t w)
{
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *box = kit_plain(parent);

    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, w, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_ver(box, 4, 0); /* room for the Instrument key skirt */
    for (int i = 0; i < 7; i++) {
        int day = alm_display_day(i);
        lv_obj_t *key = kit_day_toggle(box, alm_day_key(ctx, day), false);

        ctx->alarms.day_ctx[day].ui = ctx;
        ctx->alarms.day_ctx[day].alarm_index = 0;
        ctx->alarms.day_ctx[day].day_index = (uint8_t)day;
        ctx->alarms.editor_day_btn[day] = key;
        kit_set_tint(key, sk->tint_alarms);
        lv_obj_add_event_cb(key, day_cb, LV_EVENT_VALUE_CHANGED, &ctx->alarms.day_ctx[day]);
    }
}

static void set_note_icon(lv_obj_t *holder)
{
    uint32_t n = holder != NULL ? lv_obj_get_child_count(holder) : 0;

    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *child = lv_obj_get_child(holder, (int32_t)i);

        if (lv_obj_check_type(child, &lv_image_class)) {
            const lv_image_dsc_t *cur = lv_image_get_src(child);

            lv_image_set_src(child, (cur != NULL && cur->header.w < 26) ? &ui_alarms_note_22 : &ui_alarms_note_28);
        }
    }
}

/* Sound with Test, math, and Enabled (existing alarms) in a group. */
static lv_obj_t *create_rows(clock_ui_context_t *ctx, lv_obj_t *parent)
{
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *g = kit_group(parent);
    bool icons = !ui_skin_is_instrument();
    kit_row_t row;
    lv_obj_t *col;

    kit_row_create(&row, g,
                   &(kit_row_opts_t) {.icon = icons ? KIT_ICON_SOUND : KIT_ICON_NONE,
                                      .tint = sk->tint_alarms,
                                      .title = alm_str(ctx, &alm_s_sound_name),
                                      .subtitle = ui_skin_is_orbit() ? "" : NULL,
                                      .trail = KIT_TRAIL_CUSTOM});
    set_note_icon(row.icon);
    ctx->alarms.editor_sound_title = row.title; /* the draft's sound, set in sync */
    ctx->alarms.editor_sound_sub = row.subtitle;
    kit_make_pressable(row.row);
    lv_obj_add_event_cb(row.row, sound_cb, LV_EVENT_CLICKED, ctx);
    if (row.subtitle != NULL) {
        alm_one_line(row.subtitle);
    }
    col = lv_obj_get_parent(row.title);
    if (ui_skin_is_instrument()) {
        lv_obj_t *eb = kit_label(col, KIT_TEXT_LABEL, alm_str(ctx, &alm_s_sound_tap));

        ui_l10n_bind_kit_str(ctx, eb, &alm_s_sound_tap);
        lv_obj_move_to_index(eb, 0);
        lv_obj_set_style_pad_row(col, 0, 0);
        lv_obj_set_style_min_height(col, 52, 0);
    }
    ctx->alarms.editor_test_btn = kit_button(row.trail, KIT_BUTTON_QUIET, KIT_ICON_PLAY, alm_str(ctx, &alm_s_test));
    kit_set_tint(ctx->alarms.editor_test_btn, sk->tint_alarms);
    lv_obj_add_event_cb(ctx->alarms.editor_test_btn, test_cb, LV_EVENT_CLICKED, ctx);

    kit_row_create(&row, g,
                   &(kit_row_opts_t) {.icon = icons ? KIT_ICON_MATH : KIT_ICON_NONE,
                                      .tint = sk->tint_alarms,
                                      .title = alm_text(ctx, L10N_MATH_TO_TURN_OFF),
                                      .trail = KIT_TRAIL_SWITCH});
    ui_l10n_bind_kit(ctx, row.title, L10N_MATH_TO_TURN_OFF);
    if (ui_skin_is_instrument()) {
        lv_obj_set_style_min_height(lv_obj_get_parent(row.title), 50, 0);
    }
    ctx->alarms.editor_math_sw = row.control;
    kit_set_tint(row.control, sk->tint_alarms);
    lv_obj_add_event_cb(row.control, math_cb, LV_EVENT_VALUE_CHANGED, ctx);

    kit_row_create(&row, g,
                   &(kit_row_opts_t) {.icon = icons ? KIT_ICON_ALARM : KIT_ICON_NONE,
                                      .tint = sk->tint_alarms,
                                      .title = alm_text(ctx, L10N_ENABLED),
                                      .trail = KIT_TRAIL_SWITCH});
    ui_l10n_bind_kit(ctx, row.title, L10N_ENABLED);
    if (ui_skin_is_instrument()) {
        lv_obj_set_style_min_height(lv_obj_get_parent(row.title), 50, 0);
    }
    ctx->alarms.editor_enabled_row = row.row;
    ctx->alarms.editor_enabled_sw = row.control;
    kit_set_tint(row.control, sk->tint_alarms);
    lv_obj_add_event_cb(row.control, enabled_cb, LV_EVENT_VALUE_CHANGED, ctx);
    return g;
}

static lv_obj_t *centered_label(lv_obj_t *parent, kit_text_role_t role, int32_t center_y, const char *text)
{
    lv_obj_t *l = kit_label(parent, role, text);

    lv_obj_set_width(l, 600);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, center_y - lv_font_get_line_height(kit_font(role)) / 2);
    return l;
}

/* --- Instrument --------------------------------------------------------------------------- */

static void create_instrument(clock_ui_context_t *ctx)
{
    clock_ui_alarm_state_t *a = &ctx->alarms;
    lv_obj_t *s = kit_plain(a->editor_root);
    lv_obj_t *g;

    a->editor_scroll = s;
    lv_obj_set_size(s, KIT_SCREEN, KIT_SCREEN);
    lv_obj_add_flag(s, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_MOMENTUM |
                           LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_set_scroll_dir(s, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s, LV_SCROLLBAR_MODE_OFF);

    a->editor_eyebrow = centered_label(s, KIT_TEXT_LABEL, 57, "");
    alm_timeset_create(ctx, s);

    a->editor_close_btn = kit_round_button(s, KIT_BUTTON_SECONDARY, KIT_ICON_CLOSE, 64);
    lv_obj_set_pos(a->editor_close_btn, 112, 136);
    lv_obj_add_event_cb(a->editor_close_btn, close_cb, LV_EVENT_CLICKED, ctx);
    a->editor_delete_btn = kit_round_button(s, KIT_BUTTON_SECONDARY, KIT_ICON_TRASH, 64);
    lv_obj_set_pos(a->editor_delete_btn, 544, 136);
    lv_obj_add_event_cb(a->editor_delete_btn, delete_cb, LV_EVENT_CLICKED, ctx);

    create_repeat(ctx, s, 104, 276, 512);
    create_days(ctx, s, 98, 364 - 4, 524);

    g = create_rows(ctx, s);
    lv_obj_set_pos(g, 104, I_ROWS_Y);
    lv_obj_set_width(g, 512);

    a->editor_save_btn = kit_button(s, KIT_BUTTON_PRIMARY, KIT_ICON_NONE, alm_text(ctx, L10N_SAVE));
    lv_obj_set_size(a->editor_save_btn, 180, 72);
    lv_obj_set_pos(a->editor_save_btn, 270, I_SAVE_Y_NEW);
    lv_obj_add_event_cb(a->editor_save_btn, save_cb, LV_EVENT_CLICKED, ctx);
    a->editor_save_label = lv_obj_get_child(a->editor_save_btn, 0);
    /* Bottom room: Save scrolls up to six o'clock when the page is longer. */
    lv_obj_set_style_pad_bottom(s, 44, 0);
}

/* --- Orbit and Atelier pager --------------------------------------------------------------- */

static lv_obj_t *pager_page(lv_obj_t *pager, int index)
{
    lv_obj_t *page = kit_plain(pager);

    lv_obj_set_size(page, KIT_SCREEN, KIT_SCREEN);
    lv_obj_set_pos(page, 0, index * KIT_SCREEN);
    return page;
}

static void create_pager(clock_ui_context_t *ctx, lv_obj_t **page1)
{
    clock_ui_alarm_state_t *a = &ctx->alarms;
    lv_obj_t *pager = kit_plain(a->editor_root);
    lv_obj_t *p2;

    a->editor_scroll = pager;
    lv_obj_set_size(pager, KIT_SCREEN, KIT_SCREEN);
    lv_obj_add_flag(pager, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(pager, LV_OBJ_FLAG_SCROLL_MOMENTUM | LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_set_scroll_dir(pager, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(pager, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(pager, pager_scroll_cb, LV_EVENT_SCROLL_END, ctx);

    *page1 = pager_page(pager, 0);
    p2 = pager_page(pager, 1);
    /* The details scroll inside the page when Enabled makes them longer. */
    lv_obj_add_flag(p2, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_MOMENTUM |
                            LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_set_scroll_dir(p2, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(p2, LV_SCROLLBAR_MODE_OFF);
    a->editor_page2 = p2;
}

static void create_orbit(clock_ui_context_t *ctx)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_alarm_state_t *a = &ctx->alarms;
    lv_obj_t *p1;
    lv_obj_t *p2;
    lv_obj_t *l;
    lv_obj_t *g;
    lv_obj_t *dots;

    create_pager(ctx, &p1);
    p2 = a->editor_page2;
    alm_timeset_create(ctx, p1);

    l = centered_label(p2, KIT_TEXT_LABEL, 112, alm_str(ctx, &alm_s_repeat));
    ui_l10n_bind_kit_str(ctx, l, &alm_s_repeat);
    create_repeat(ctx, p2, 80, 132, 560);
    create_days(ctx, p2, 105, 224 - 4, 510);
    g = create_rows(ctx, p2);
    lv_obj_set_pos(g, 110, 314);
    lv_obj_set_width(g, 500);
    lv_obj_set_style_pad_bottom(p2, 190, 0);

    /* Fixed on both pages: close at twelve, Delete and Save at six, page dots at three. */
    a->editor_bottom_scrim = kit_plain(a->editor_root);
    lv_obj_set_size(a->editor_bottom_scrim, KIT_SCREEN, 190);
    lv_obj_set_pos(a->editor_bottom_scrim, 0, KIT_SCREEN - 190);
    lv_obj_set_style_bg_opa(a->editor_bottom_scrim, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(a->editor_bottom_scrim, sk->bg, 0);
    lv_obj_set_style_bg_grad_color(a->editor_bottom_scrim, sk->bg, 0);
    lv_obj_set_style_bg_main_opa(a->editor_bottom_scrim, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_grad_opa(a->editor_bottom_scrim, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_main_stop(a->editor_bottom_scrim, 0, 0);
    lv_obj_set_style_bg_grad_stop(a->editor_bottom_scrim, 80, 0);
    lv_obj_set_style_bg_grad_dir(a->editor_bottom_scrim, LV_GRAD_DIR_VER, 0);
    lv_obj_add_flag(a->editor_bottom_scrim, LV_OBJ_FLAG_HIDDEN);

    l = kit_plain(a->editor_root);
    lv_obj_set_size(l, KIT_SCREEN, 112);
    lv_obj_set_style_bg_opa(l, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(l, sk->bg, 0);
    lv_obj_set_style_bg_grad_color(l, sk->bg, 0);
    lv_obj_set_style_bg_main_opa(l, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_opa(l, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_main_stop(l, 150, 0);
    lv_obj_set_style_bg_grad_dir(l, LV_GRAD_DIR_VER, 0);

    a->editor_close_btn = kit_round_button(a->editor_root, KIT_BUTTON_QUIET, KIT_ICON_CLOSE, 64);
    lv_obj_set_pos(a->editor_close_btn, 328, 24);
    lv_obj_set_style_image_recolor(lv_obj_get_child(a->editor_close_btn, 0), sk->text, 0);
    lv_obj_add_event_cb(a->editor_close_btn, close_cb, LV_EVENT_CLICKED, ctx);

    a->editor_delete_btn = kit_round_button(a->editor_root, KIT_BUTTON_DESTRUCTIVE, KIT_ICON_TRASH, 80);
    lv_obj_set_pos(a->editor_delete_btn, 260, O_KEYS_Y);
    lv_obj_add_event_cb(a->editor_delete_btn, delete_cb, LV_EVENT_CLICKED, ctx);
    a->editor_delete_label = kit_label(a->editor_root, KIT_TEXT_LABEL, alm_text(ctx, L10N_DELETE));
    ui_l10n_bind_kit(ctx, a->editor_delete_label, L10N_DELETE);
    lv_obj_set_style_text_color(a->editor_delete_label, sk->text_2, 0);

    a->editor_save_btn = kit_round_button(a->editor_root, KIT_BUTTON_PRIMARY, KIT_ICON_CHECK, 80);
    kit_set_tint(a->editor_save_btn, sk->tint_alarms);
    lv_obj_set_pos(a->editor_save_btn, 380, O_KEYS_Y);
    lv_obj_add_event_cb(a->editor_save_btn, save_cb, LV_EVENT_CLICKED, ctx);
    a->editor_save_label = kit_label(a->editor_root, KIT_TEXT_LABEL, alm_text(ctx, L10N_SAVE));
    ui_l10n_bind_kit(ctx, a->editor_save_label, L10N_SAVE);
    lv_obj_set_style_text_color(a->editor_save_label, sk->text, 0);

    dots = kit_plain(a->editor_root);
    lv_obj_set_size(dots, 24, 64);
    lv_obj_set_pos(dots, 668, 300);
    lv_obj_set_flex_flow(dots, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(dots, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(dots, 6, 0);
    lv_obj_add_flag(dots, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(dots, 24);
    lv_obj_add_event_cb(dots, details_cb, LV_EVENT_CLICKED, ctx);
    for (int i = 0; i < 2; i++) {
        a->editor_dots[i] = kit_plain(dots);
        /* Two equal capsules: a long one over a dot read as "!". */
        lv_obj_set_size(a->editor_dots[i], 8, 20);
        lv_obj_set_style_radius(a->editor_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(a->editor_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(a->editor_dots[i], i == 0 ? sk->text : sk->text_3, 0);
    }
}

static void create_atelier(clock_ui_context_t *ctx)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_alarm_state_t *a = &ctx->alarms;
    lv_obj_t *p1;
    lv_obj_t *p2;
    lv_obj_t *sec;
    lv_obj_t *l;
    lv_obj_t *col;
    lv_obj_t *keys;
    lv_obj_t *rule;

    lv_obj_add_event_cb(a->editor_root, atelier_rim_cb, LV_EVENT_DRAW_MAIN_END, ctx);
    create_pager(ctx, &p1);
    p2 = a->editor_page2;
    alm_timeset_create(ctx, p1);

    /* Page 1 centre: Cancel and Confirm pushers, the chevron to the details. */
    a->editor_close_btn = kit_round_button(p1, KIT_BUTTON_SECONDARY, KIT_ICON_CLOSE, 64);
    lv_obj_set_pos(a->editor_close_btn, 298 - 32, 474 - 32);
    lv_obj_add_event_cb(a->editor_close_btn, close_cb, LV_EVENT_CLICKED, ctx);
    a->editor_save2_btn = kit_round_button(p1, KIT_BUTTON_PRIMARY, KIT_ICON_CHECK, 64);
    lv_obj_set_pos(a->editor_save2_btn, 422 - 32, 474 - 32);
    lv_obj_add_event_cb(a->editor_save2_btn, save_cb, LV_EVENT_CLICKED, ctx);
    l = kit_icon_small(p1, KIT_ICON_CHEVRON_DOWN, sk->accent_deep);
    lv_obj_set_pos(l, 360 - 11, 538 - 11);
    lv_obj_add_flag(l, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(l, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_ext_click_area(l, 25);
    lv_obj_add_event_cb(l, details_cb, LV_EVENT_CLICKED, ctx);

    /* Page 2: details. */
    a->editor_eyebrow = centered_label(p2, KIT_TEXT_LABEL, 50, "");
    a->editor_time_small = alm_label(p2, sk->font_time_m, sk->text, "00:00");
    lv_obj_align(a->editor_time_small, LV_ALIGN_TOP_MID, 0, 100 - 28);
    lv_obj_add_flag(a->editor_time_small, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(a->editor_time_small, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(a->editor_time_small, details_cb, LV_EVENT_CLICKED, ctx);

    sec = kit_plain(p2);
    lv_obj_set_size(sec, 432, 40);
    lv_obj_set_pos(sec, 144, 170 - 34);
    lv_obj_set_flex_flow(sec, LV_FLEX_FLOW_COLUMN);
    l = kit_section_label(sec, alm_str(ctx, &alm_s_repeat));
    ui_l10n_bind_kit_str(ctx, l, &alm_s_repeat);

    create_repeat(ctx, p2, 122, 198, 476);
    create_days(ctx, p2, 108 + 8, 282 + 8 - 4, 488);
    rule = kit_plain(p2);
    lv_obj_set_size(rule, 432, 1);
    lv_obj_set_pos(rule, 144, 370);
    lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(rule, sk->line, 0);

    col = kit_plain(p2);
    lv_obj_set_pos(col, 144, A_ROWS_Y);
    lv_obj_set_size(col, 432, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    create_rows(ctx, col);

    keys = kit_plain(col);
    lv_obj_set_size(keys, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_top(keys, 26, 0);
    lv_obj_set_style_pad_bottom(keys, 8, 0);
    lv_obj_set_flex_flow(keys, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(keys, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(keys, 16, 0);
    a->editor_delete_btn = kit_button(keys, KIT_BUTTON_DESTRUCTIVE, KIT_ICON_TRASH, alm_text(ctx, L10N_DELETE));
    lv_obj_set_size(a->editor_delete_btn, 176, 72);
    lv_obj_add_event_cb(a->editor_delete_btn, delete_cb, LV_EVENT_CLICKED, ctx);
    a->editor_save_btn = kit_button(keys, KIT_BUTTON_PRIMARY, KIT_ICON_NONE, alm_text(ctx, L10N_SAVE));
    lv_obj_set_size(a->editor_save_btn, 176, 72);
    lv_obj_add_event_cb(a->editor_save_btn, save_cb, LV_EVENT_CLICKED, ctx);
    a->editor_save_label = lv_obj_get_child(a->editor_save_btn, 0);
    a->editor_delete_label = lv_obj_get_child(a->editor_delete_btn, 1);
    lv_obj_set_style_pad_bottom(p2, 96, 0);
}

void alm_editor_create(clock_ui_context_t *ctx)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_alarm_state_t *a = &ctx->alarms;
    lv_obj_t *root = kit_plain(ui_chrome_root(ctx));

    a->editor_root = root;
    lv_obj_set_size(root, KIT_SCREEN, KIT_SCREEN);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(root, sk->bg, 0);
    lv_obj_add_flag(root, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN);

    if (ui_skin_is_instrument()) {
        create_instrument(ctx);
    } else if (ui_skin_is_orbit()) {
        create_orbit(ctx);
    } else {
        create_atelier(ctx);
    }
    alm_add_edge_close(ctx, root);
}

/* --- Open, sync, tick ---------------------------------------------------------------------- */

void alm_editor_show_details(clock_ui_context_t *ctx, bool details, bool animate)
{
    clock_ui_alarm_state_t *a = &ctx->alarms;

    if (a->editor_page2 == NULL) {
        return;
    }
    lv_obj_scroll_to_y(a->editor_scroll, details ? KIT_SCREEN : 0, animate ? LV_ANIM_ON : LV_ANIM_OFF);
    if (!animate) {
        lv_obj_send_event(a->editor_scroll, LV_EVENT_SCROLL_END, NULL);
    }
}

void alm_editor_prepare(clock_ui_context_t *ctx)
{
    clock_ui_alarm_state_t *a = &ctx->alarms;

    a->ts.shown_hour = -1;
    a->ts.shown_minute = -1;
    a->ts.shown_now_angle = -1;
    lv_obj_scroll_to_y(a->editor_scroll, 0, LV_ANIM_OFF);
    if (a->editor_page2 != NULL) {
        lv_obj_scroll_to_y(a->editor_page2, 0, LV_ANIM_OFF);
        a->editor_on_details = true; /* forces the page-1 refresh below */
        lv_obj_send_event(a->editor_scroll, LV_EVENT_SCROLL_END, NULL);
    }
}

/* Next time the draft rings after now. */
static time_t draft_next(const alarm_config_t *d, time_t now)
{
    time_t t = alm_next_occurrence(now, d->hour, d->minute);

    if (d->repeat_mode == ALARM_REPEAT_ONCE || d->days_mask == 0) {
        return t;
    }
    for (int i = 0; i < 8; i++) {
        struct tm tm;

        localtime_r(&t, &tm);
        if (d->days_mask & (1U << tm.tm_wday)) {
            return t;
        }
        tm.tm_mday += 1;
        tm.tm_isdst = -1;
        t = mktime(&tm);
    }
    return t;
}

static void sync_countdown(clock_ui_context_t *ctx, time_t now)
{
    char text[48];

    if (ctx->alarms.editor_countdown == NULL) {
        return;
    }
    ui_format_countdown(ctx, now, draft_next(&ctx->alarms.editor_draft, now), text, sizeof(text));
    kit_label_set_text_if_changed(ctx->alarms.editor_countdown, text);
}

void alm_editor_time_changed(clock_ui_context_t *ctx)
{
    char text[8];

    sync_countdown(ctx, alm_now());
    if (ctx->alarms.editor_time_small != NULL) {
        alm_format_hm(text, sizeof(text), ctx->alarms.editor_draft.hour, ctx->alarms.editor_draft.minute);
        kit_label_set_text_if_changed(ctx->alarms.editor_time_small, text);
    }
}

static void sync_layout(clock_ui_context_t *ctx)
{
    clock_ui_alarm_state_t *a = &ctx->alarms;
    bool existing = !a->editor_is_new;

    kit_show(a->editor_delete_btn, existing);
    kit_show(a->editor_enabled_row, existing);
    if (ui_skin_is_instrument()) {
        int32_t y = existing ? I_SAVE_Y_NEW + 72 + 12 : I_SAVE_Y_NEW;

        if (lv_obj_get_y(a->editor_save_btn) != y) {
            lv_obj_set_y(a->editor_save_btn, y);
        }
    } else if (ui_skin_is_orbit()) {
        int32_t x = existing ? 380 : 320;

        kit_show(a->editor_delete_label, existing);
        if (lv_obj_get_x(a->editor_save_btn) != x) {
            lv_obj_set_x(a->editor_save_btn, x);
        }
        lv_obj_update_layout(a->editor_save_label);
        lv_obj_set_pos(a->editor_save_label, x + 40 - lv_obj_get_width(a->editor_save_label) / 2, 654);
        lv_obj_update_layout(a->editor_delete_label);
        lv_obj_set_pos(a->editor_delete_label, 300 - lv_obj_get_width(a->editor_delete_label) / 2, 654);
    }
}

void alm_editor_sync(clock_ui_context_t *ctx)
{
    clock_ui_alarm_state_t *a = &ctx->alarms;
    const alarm_config_t *d = &a->editor_draft;
    uint8_t preset = alarm_repeat_preset_from_config(d);
    bool testing = ctx->runtime->alarm_test_active;

    if (a->editor_root == NULL) {
        return;
    }
    kit_label_set_text_if_changed(a->editor_eyebrow,
                 a->editor_is_new ? alm_str(ctx, &alm_s_new_alarm) : alm_text(ctx, L10N_EDIT_ALARM));

    if (a->editor_language != ctx->settings->language + 1) {
        /* Setting a label re-measures the segments: only when the language changed. */
        a->editor_language = (uint8_t)(ctx->settings->language + 1);
        kit_segmented_set_label(a->editor_seg, 0, alm_text(ctx, L10N_ONCE));
        kit_segmented_set_label(a->editor_seg, 1, alm_text(ctx, L10N_EVERY_DAY));
        kit_segmented_set_label(a->editor_seg, 2, alm_text(ctx, L10N_WEEKDAYS));
        kit_segmented_set_label(a->editor_seg, 3, alm_text(ctx, L10N_WEEKENDS));
    }
    kit_segmented_set(a->editor_seg, preset < 4 ? (int)preset : -1);
    for (int day = 0; day < 7; day++) {
        lv_obj_t *key = a->editor_day_btn[day];
        bool on = d->repeat_mode == ALARM_REPEAT_WEEKLY && (d->days_mask & (1U << day)) != 0;

        if (key == NULL) {
            continue;
        }
        kit_label_set_text_if_changed(lv_obj_get_child(key, 0), alm_day_key(ctx, day));
        if (lv_obj_has_state(key, LV_STATE_CHECKED) != on) {
            lv_obj_set_state(key, LV_STATE_CHECKED, on);
        }
    }

    kit_label_set_text_if_changed(a->editor_sound_title, alm_str(ctx, alm_sound_name(d->sound)));
    if (a->editor_sound_sub != NULL) {
        kit_label_set_text_if_changed(a->editor_sound_sub, alm_str(ctx, &alm_s_tap_to_change));
    }
    kit_button_set_text(a->editor_test_btn, alm_str(ctx, testing ? &alm_s_stop : &alm_s_test));
    kit_icon_set(lv_obj_get_child(a->editor_test_btn, 0), testing ? KIT_ICON_STOP : KIT_ICON_PLAY);

    ctx->suppress_events = true;
    kit_switch_set(a->editor_math_sw, d->math_unlock_enabled);
    kit_switch_set(a->editor_enabled_sw, d->enabled);
    ctx->suppress_events = false;

    if (ui_skin_is_instrument() || ui_skin_is_atelier()) {
        kit_button_set_text(a->editor_save_btn, alm_text(ctx, L10N_SAVE));
        if (ui_skin_is_atelier()) {
            kit_button_set_text(a->editor_delete_btn, alm_text(ctx, L10N_DELETE));
        }
    }
    sync_layout(ctx);
    alm_timeset_sync(ctx);
    alm_editor_time_changed(ctx);
}

void alm_editor_tick(clock_ui_context_t *ctx, time_t now)
{
    long minute = (long)(now / 60);

    if (minute == ctx->alarms.editor_minute) {
        return;
    }
    ctx->alarms.editor_minute = minute;
    sync_countdown(ctx, now);
    alm_timeset_tick(ctx, now);
}

void alm_editor_save(clock_ui_context_t *ctx)
{
    clock_ui_alarm_state_t *a = &ctx->alarms;

    if (a->editor_index < 0 || a->editor_index >= MAX_ALARMS) {
        return;
    }
    if (a->editor_is_new) {
        a->editor_draft.enabled = true;
    }
    request_save_alarm(ctx, (uint8_t)a->editor_index, &a->editor_draft);
    a->focus_alarm_index = a->editor_index;
    alm_editor_close(ctx);
}

void alm_editor_free(clock_ui_context_t *ctx)
{
    alm_timeset_free(ctx);
}
