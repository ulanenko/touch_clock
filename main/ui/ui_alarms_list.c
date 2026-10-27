/*
 * Alarm list (screen 09) and alarm settings, in the three styles.
 *
 *   Instrument: next-up readout in a recessed window, hairline rows with 56 px light figures
 *               and a day strip, Add as the orange key at six.
 *   Orbit:      orange next-up line under the title, one platter per alarm, Add at six.
 *   Atelier:    gilt "Next up" lockup, ruled rows with gilt day strips, Add as the pusher at
 *               two, alarm settings close the page on a centred axis.
 *
 * Rows slide left to reveal a delete key; a tap edits, the switch enables.
 */
#include "ui/ui_alarms_internal.h"

#include <inttypes.h>

#include "assets/skin_fonts/skin_fonts.h"
#include "assets/ui_assets/alarms_assets.h"

#define DELETE_KEY 64
#define DELETE_SHIFT (DELETE_KEY + 16)
#define SWIPE_TRIGGER 34
#define SWIPE_CANCEL 12
#define SWIPE_ANIM_MS 180

static const ui_l10n_str_t s_count_on = UI_L10N_STR("%d on", "%d wł.", "%d увімк.", "%d вкл.", "%d aan");

static int32_t row_height(void)
{
    return ALM_IS_INSTRUMENT() ? 92 : ALM_IS_ORBIT() ? 88 : 100;
}

/* --- Swipe to delete ------------------------------------------------------------------- */

/* The rows slide out to the left of the column: their holders draw past their edges. */
static void overflow_cb(lv_event_t *e)
{
    lv_event_set_ext_draw_size(e, DELETE_SHIFT + 8);
}

static void allow_overflow(lv_obj_t *obj)
{
    lv_obj_add_flag(obj, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_add_event_cb(obj, overflow_cb, LV_EVENT_REFR_EXT_DRAW_SIZE, NULL);
    lv_obj_refresh_ext_draw_size(obj);
}

static void translate_cb(void *var, int32_t v)
{
    lv_obj_set_style_translate_x(var, v, 0);
}

static void slide_row(clock_ui_context_t *ctx, int8_t index, bool open, bool animate)
{
    alm_row_t *r;
    int32_t from;
    int32_t to = open ? -DELETE_SHIFT : 0;

    if (index < 0 || index >= MAX_ALARMS || ctx->alarms.rows[index].row == NULL) {
        return;
    }
    r = &ctx->alarms.rows[index];
    from = lv_obj_get_style_translate_x(r->row, 0);
    lv_anim_delete(r->row, translate_cb);
    alm_show(r->del, open);
    if (!animate || from == to) {
        lv_obj_set_style_translate_x(r->row, to, 0);
        return;
    }
    lv_anim_t a;

    lv_anim_init(&a);
    lv_anim_set_var(&a, r->row);
    lv_anim_set_exec_cb(&a, translate_cb);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, SWIPE_ANIM_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

void alm_list_close_swipe(clock_ui_context_t *ctx, int8_t index, bool animate)
{
    if (index < 0 || index >= MAX_ALARMS) {
        return;
    }
    slide_row(ctx, index, false, animate);
    if (ctx->alarms.swipe_open_index == index) {
        ctx->alarms.swipe_open_index = -1;
    }
}

static void open_swipe(clock_ui_context_t *ctx, int8_t index)
{
    if (ctx->alarms.swipe_open_index >= 0 && ctx->alarms.swipe_open_index != index) {
        alm_list_close_swipe(ctx, ctx->alarms.swipe_open_index, true);
    }
    slide_row(ctx, index, true, true);
    ctx->alarms.swipe_open_index = index;
}

static void row_event_cb(lv_event_t *e)
{
    alarm_ctx_t *actx = lv_event_get_user_data(e);
    clock_ui_context_t *ctx = actx->ui;
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_active();
    int8_t index = (int8_t)actx->alarm_index;
    lv_point_t p = {0, 0};

    if (indev != NULL) {
        lv_indev_get_point(indev, &p);
    }
    alm_auto_close_reset(ctx);

    if (code == LV_EVENT_PRESSED) {
        ctx->alarms.list_swipe_dragging = true;
        ctx->alarms.list_swipe_consumed = false;
        ctx->alarms.swipe_drag_index = index;
        ctx->alarms.list_swipe_start_point = p;
    } else if (code == LV_EVENT_PRESSING) {
        int32_t dx = p.x - ctx->alarms.list_swipe_start_point.x;
        int32_t dy = p.y - ctx->alarms.list_swipe_start_point.y;

        if (!ctx->alarms.list_swipe_dragging || ctx->alarms.swipe_drag_index != index) {
            return;
        }
        if (LV_ABS(dy) > LV_ABS(dx) && LV_ABS(dy) >= SWIPE_CANCEL) {
            ctx->alarms.list_swipe_dragging = false; /* the page scrolls */
            ctx->alarms.swipe_drag_index = -1;
            return;
        }
        if (LV_ABS(dx) > LV_ABS(dy)) {
            if (dx <= -SWIPE_TRIGGER) {
                open_swipe(ctx, index);
                ctx->alarms.list_swipe_consumed = true;
                ctx->alarms.list_swipe_dragging = false;
                lv_indev_wait_release(indev);
            } else if (dx >= SWIPE_TRIGGER && ctx->alarms.swipe_open_index == index) {
                alm_list_close_swipe(ctx, index, true);
                ctx->alarms.list_swipe_consumed = true;
                ctx->alarms.list_swipe_dragging = false;
                lv_indev_wait_release(indev);
            }
        }
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        ctx->alarms.list_swipe_dragging = false;
        ctx->alarms.swipe_drag_index = -1;
    } else if (code == LV_EVENT_CLICKED) {
        if (ctx->alarms.list_swipe_consumed) {
            ctx->alarms.list_swipe_consumed = false;
            return;
        }
        if (ctx->alarms.swipe_open_index >= 0) {
            alm_list_close_swipe(ctx, ctx->alarms.swipe_open_index, true);
            return;
        }
        alm_editor_open(ctx, actx->alarm_index, false);
    }
}

void alm_list_reveal_delete(clock_ui_context_t *ctx, int8_t index)
{
    if (index >= 0 && index < MAX_ALARMS && ctx->alarms.rows[index].row != NULL) {
        open_swipe(ctx, index);
    }
}

static void row_switch_cb(lv_event_t *e)
{
    alarm_ctx_t *actx = lv_event_get_user_data(e);
    clock_ui_context_t *ctx = actx->ui;

    if (ctx->suppress_events) {
        return;
    }
    alm_auto_close_reset(ctx);
    request_set_alarm_enabled(ctx, actx->alarm_index, lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED));
    alm_sync_all(ctx);
}

static void row_delete_cb(lv_event_t *e)
{
    alarm_ctx_t *actx = lv_event_get_user_data(e);
    clock_ui_context_t *ctx = actx->ui;

    alm_auto_close_reset(ctx);
    alm_list_close_swipe(ctx, (int8_t)actx->alarm_index, false);
    request_delete_alarm(ctx, actx->alarm_index);
    alm_sync_all(ctx);
}

/* --- Rows ------------------------------------------------------------------------------ */

/* Row look of the style: Instrument top rule, Orbit platter, Atelier rule between rows. */
static void style_row(lv_obj_t *row, bool first, int32_t height)
{
    const ui_skin_t *sk = ui_skin();

    lv_obj_set_size(row, lv_pct(100), height);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    if (ALM_IS_INSTRUMENT()) {
        lv_obj_set_style_pad_column(row, 20, 0);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_TOP, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, sk->line, 0);
        lv_obj_set_style_bg_color(row, sk->surface, LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_STATE_PRESSED);
    } else if (ALM_IS_ORBIT()) {
        lv_obj_set_style_pad_column(row, 12, 0);
        lv_obj_set_style_pad_left(row, 28, 0);
        lv_obj_set_style_pad_right(row, 24, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(row, sk->surface, 0);
        lv_obj_set_style_bg_color(row, sk->raised, LV_STATE_PRESSED);
        lv_obj_set_style_radius(row, 32, 0);
    } else {
        lv_obj_set_style_pad_column(row, 16, 0);
        if (!first) {
            lv_obj_set_style_border_side(row, LV_BORDER_SIDE_TOP, 0);
            lv_obj_set_style_border_width(row, 1, 0);
            lv_obj_set_style_border_color(row, sk->line, 0);
        }
        lv_obj_set_style_bg_color(row, sk->surface, LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_STATE_PRESSED);
    }
}

static void list_row_create(clock_ui_context_t *ctx, lv_obj_t *group, int index)
{
    const ui_skin_t *sk = ui_skin();
    alm_row_t *r = &ctx->alarms.rows[index];
    lv_obj_t *col;
    int32_t h = row_height();

    r->actx.ui = ctx;
    r->actx.alarm_index = (uint8_t)index;
    r->wrap = alm_box(group);
    lv_obj_set_size(r->wrap, lv_pct(100), h);
    allow_overflow(r->wrap);

    r->row = alm_box(r->wrap);
    lv_obj_add_flag(r->row, LV_OBJ_FLAG_CLICKABLE);
    style_row(r->row, index == 0, h);
    ui_attach_click_feedback(r->row, LV_EVENT_CLICKED);
    lv_obj_add_event_cb(r->row, row_event_cb, LV_EVENT_PRESSED, &r->actx);
    lv_obj_add_event_cb(r->row, row_event_cb, LV_EVENT_PRESSING, &r->actx);
    lv_obj_add_event_cb(r->row, row_event_cb, LV_EVENT_RELEASED, &r->actx);
    lv_obj_add_event_cb(r->row, row_event_cb, LV_EVENT_PRESS_LOST, &r->actx);
    lv_obj_add_event_cb(r->row, row_event_cb, LV_EVENT_CLICKED, &r->actx);

    r->time = alm_label(r->row, sk->font_time_m, sk->text, "00:00");
    if (ALM_IS_INSTRUMENT()) {
        lv_obj_set_width(r->time, 150);
    } else if (ALM_IS_ATELIER()) {
        lv_obj_set_width(r->time, 138);
    }

    col = alm_box(r->row);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(col, LV_OBJ_FLAG_EVENT_BUBBLE);
    if (ALM_IS_ORBIT()) {
        /* Time and summary share a baseline. */
        lv_obj_set_style_translate_y(col, 9, 0);
        r->title = alm_label(col, sk->font_body, sk->text_2, "");
    } else if (ALM_IS_INSTRUMENT()) {
        lv_obj_set_style_pad_row(col, 4, 0);
        r->title = kit_label(col, KIT_TEXT_LABEL, "");
        lv_obj_set_style_text_color(r->title, sk->text_2, 0);
    } else {
        lv_obj_set_style_pad_row(col, 4, 0);
        r->title = alm_label(col, sk->font_body, sk->text, "");
    }
    lv_obj_add_flag(r->title, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_width(r->title, lv_pct(100));
    alm_one_line(r->title);

    if (!ALM_IS_ORBIT()) {
        r->days_box = alm_box(col);
        lv_obj_set_size(r->days_box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(r->days_box, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(r->days_box, ALM_IS_INSTRUMENT() ? 7 : 6, 0);
        lv_obj_add_flag(r->days_box, LV_OBJ_FLAG_EVENT_BUBBLE);
        for (int i = 0; i < 7; i++) {
            r->days[i] = alm_label(r->days_box, sk->font_label, sk->text_3, "");
            if (ALM_IS_ATELIER()) {
                lv_obj_set_style_min_width(r->days[i], 16, 0);
                lv_obj_set_style_text_align(r->days[i], LV_TEXT_ALIGN_CENTER, 0);
            }
        }
        if (ALM_IS_INSTRUMENT()) {
            r->sub = alm_label(col, sk->font_caption, sk->text_3, "");
        } else {
            r->sub = kit_label(col, KIT_TEXT_LABEL, "");
            lv_obj_set_style_text_color(r->sub, sk->text_2, 0);
            lv_obj_set_style_text_letter_space(r->sub, 2, 0);
        }
        lv_obj_add_flag(r->sub, LV_OBJ_FLAG_EVENT_BUBBLE);
    }

    r->sw = kit_switch(r->row, sk->tint_alarms);
    kit_set_tint(r->sw, sk->tint_alarms);
    lv_obj_add_event_cb(r->sw, row_switch_cb, LV_EVENT_VALUE_CHANGED, &r->actx);

    r->del = kit_round_button(r->wrap, KIT_BUTTON_DESTRUCTIVE, KIT_ICON_TRASH, DELETE_KEY);
    lv_obj_align(r->del, LV_ALIGN_RIGHT_MID, ALM_IS_ATELIER() ? -6 : 0, 0);
    lv_obj_add_flag(r->del, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(r->del, row_delete_cb, LV_EVENT_CLICKED, &r->actx);
}

/* --- Page callbacks -------------------------------------------------------------------- */

static void nav_cb(lv_event_t *e)
{
    alm_list_close(lv_event_get_user_data(e));
}

static void add_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);
    int slot = alm_find_new_slot(ctx);

    if (slot >= 0) {
        alm_editor_open(ctx, (uint8_t)slot, true);
    }
}

static void settings_entry_cb(lv_event_t *e)
{
    alm_settings_open(lv_event_get_user_data(e));
}

static void scroll_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);

    alm_auto_close_reset(ctx);
    ctx->alarms.management_scrolling = code != LV_EVENT_SCROLL_END;
}

static lv_obj_t *settings_entry_row(clock_ui_context_t *ctx, lv_obj_t *parent)
{
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *row = alm_box(parent);
    lv_obj_t *col;
    lv_obj_t *icon;

    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, settings_entry_cb, LV_EVENT_CLICKED, ctx);
    ctx->alarms.list_settings_row = row;

    if (ALM_IS_ATELIER()) {
        lv_obj_t *line;

        /* Centred caseback lockup: title with a chevron, summary below. */
        lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_style_pad_ver(row, 10, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        kit_make_pressable(row);
        line = alm_box(row);
        lv_obj_set_size(line, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(line, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(line, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(line, 6, 0);
        lv_obj_add_flag(line, LV_OBJ_FLAG_EVENT_BUBBLE);
        ctx->alarms.list_settings_title = alm_label(line, sk->font_heading, sk->text, "");
        kit_icon_small(line, KIT_ICON_CHEVRON_RIGHT, sk->accent_deep);
        ctx->alarms.list_settings_sub = alm_label(row, sk->font_body, sk->text_2, "");
        lv_obj_set_style_margin_top(ctx->alarms.list_settings_sub, 2, 0);
        return row;
    }

    ui_attach_click_feedback(row, LV_EVENT_CLICKED);
    style_row(row, false, ALM_IS_INSTRUMENT() ? 84 : 104);
    if (ALM_IS_INSTRUMENT()) {
        lv_obj_set_style_pad_column(row, 16, 0);
        lv_obj_set_style_pad_hor(row, 12, 0);
        icon = kit_icon(row, KIT_ICON_SETTINGS, sk->text_2);
    } else {
        lv_obj_t *badge = alm_box(row);

        lv_obj_set_style_pad_left(row, 18, 0);
        lv_obj_set_style_pad_column(row, 18, 0);
        lv_obj_set_size(badge, 56, 56);
        lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(badge, sk->raised, 0);
        icon = kit_icon(badge, KIT_ICON_SETTINGS, sk->tint_alarms);
        lv_obj_center(icon);
    }
    (void)icon;
    col = alm_box(row);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(col, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_style_pad_row(col, ALM_IS_INSTRUMENT() ? 2 : 0, 0);
    ctx->alarms.list_settings_title = alm_label(col, sk->font_heading, sk->text, "");
    ctx->alarms.list_settings_sub =
        alm_label(col, ALM_IS_INSTRUMENT() ? sk->font_caption : sk->font_body, ALM_IS_INSTRUMENT() ? sk->text_3 : sk->text_2, "");
    lv_obj_set_width(ctx->alarms.list_settings_title, lv_pct(100));
    lv_obj_set_width(ctx->alarms.list_settings_sub, lv_pct(100));
    alm_one_line(ctx->alarms.list_settings_title);
    alm_one_line(ctx->alarms.list_settings_sub);
    if (ALM_IS_INSTRUMENT()) {
        kit_icon(row, KIT_ICON_CHEVRON_RIGHT, sk->text_3);
    } else {
        kit_icon_small(row, KIT_ICON_CHEVRON_RIGHT, sk->text_3);
    }
    return row;
}

void alm_list_create(clock_ui_context_t *ctx)
{
    const ui_skin_t *sk = ui_skin();
    kit_page_t *p = &ctx->alarms.list_page;
    lv_obj_t *content;

    kit_page_create(p, ui_chrome_root(ctx), ALM_IS_ORBIT() ? NULL : "", alm_text(ctx, L10N_ALARMS),
                    ALM_IS_ORBIT() ? KIT_NAV_CLOSE : KIT_NAV_BACK, nav_cb, ctx);
    ui_l10n_bind_kit(ctx, p->title, L10N_ALARMS);
    if (ALM_IS_ORBIT() && p->nav != NULL) {
        kit_set_tint(p->nav, sk->tint_alarms);
    }
    content = p->content;
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(p->scroll, scroll_cb, LV_EVENT_SCROLL_BEGIN, ctx);
    lv_obj_add_event_cb(p->scroll, scroll_cb, LV_EVENT_SCROLL_END, ctx);

    /* Next up. */
    if (ALM_IS_INSTRUMENT()) {
        lv_obj_t *well = alm_box(content);

        lv_obj_set_size(well, 496, 44);
        lv_obj_set_style_radius(well, 22, 0);
        lv_obj_set_style_bg_opa(well, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(well, sk->well, 0);
        lv_obj_set_style_border_width(well, 1, 0);
        lv_obj_set_style_border_color(well, sk->line, 0);
        lv_obj_set_flex_flow(well, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(well, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(well, 10, 0);
        lv_obj_set_style_pad_hor(well, 16, 0);
        lv_obj_set_style_margin_bottom(well, 20, 0);
        ctx->alarms.list_next_led = alm_box(well);
        lv_obj_set_size(ctx->alarms.list_next_led, 8, 8);
        lv_obj_set_style_radius(ctx->alarms.list_next_led, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(ctx->alarms.list_next_led, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(ctx->alarms.list_next_led, sk->accent, 0);
        ctx->alarms.list_next = alm_label(well, sk->font_caption, sk->text_2, "");
    } else if (ALM_IS_ORBIT()) {
        ctx->alarms.list_next = alm_label(p->scroll, &or_medium_22, sk->text, "");
        lv_obj_set_width(ctx->alarms.list_next, 560);
        lv_obj_set_style_text_align(ctx->alarms.list_next, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(ctx->alarms.list_next, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_align(ctx->alarms.list_next, LV_ALIGN_TOP_MID, 0, 154);
        kit_spacer(content, 24);
    } else {
        lv_obj_t *line = alm_box(content);
        lv_obj_t *eb;

        lv_obj_set_size(line, lv_pct(110), 28);
        lv_obj_set_flex_flow(line, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(line, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(line, 12, 0);
        lv_obj_set_style_margin_bottom(line, 10, 0);
        eb = kit_label(line, KIT_TEXT_LABEL, alm_text(ctx, L10N_NEXT_UP));
        lv_obj_set_style_text_color(eb, sk->accent, 0);
        ctx->alarms.list_next_led = eb;
        ctx->alarms.list_next = alm_label(line, sk->font_body, sk->text_2, "");
    }
    lv_label_set_recolor(ctx->alarms.list_next, true);

    /* Alarms. */
    ctx->alarms.list_group = kit_group(content);
    allow_overflow(ctx->alarms.list_group);
    allow_overflow(content);
    if (ALM_IS_ORBIT()) {
        lv_obj_set_style_pad_bottom(ctx->alarms.list_group, 8, 0);
    }
    for (int i = 0; i < MAX_ALARMS; i++) {
        list_row_create(ctx, ctx->alarms.list_group, i);
    }

    /* Alarm settings entry. */
    if (ALM_IS_ATELIER()) {
        lv_obj_t *div = kit_divider(content);

        lv_obj_set_style_margin_top(div, 8, 0);
        lv_obj_set_style_margin_bottom(div, 6, 0);
        settings_entry_row(ctx, content);
    } else if (ALM_IS_INSTRUMENT()) {
        lv_obj_t *g = kit_group(content);

        lv_obj_set_width(g, 488);
        settings_entry_row(ctx, g);
    } else {
        settings_entry_row(ctx, content);
    }

    /* Add. */
    if (ALM_IS_ATELIER()) {
        ctx->alarms.list_add = kit_page_set_action(p, KIT_ICON_PLUS, add_cb, ctx);
    } else {
        int32_t d = ALM_IS_INSTRUMENT() ? 72 : 88;

        ctx->alarms.list_add = kit_round_button(p->root, KIT_BUTTON_PRIMARY, KIT_ICON_PLUS, d);
        kit_set_tint(ctx->alarms.list_add, sk->tint_alarms);
        lv_obj_set_pos(ctx->alarms.list_add, (KIT_SCREEN - d) / 2, ALM_IS_INSTRUMENT() ? 616 : 604);
        lv_obj_add_event_cb(ctx->alarms.list_add, add_cb, LV_EVENT_CLICKED, ctx);
        /* Room to scroll the last row above the key. */
        kit_spacer(content, ALM_IS_INSTRUMENT() ? 40 : 30);
    }

    alm_add_edge_close(ctx, p->root);
}

/* --- Sync ------------------------------------------------------------------------------ */

static void next_up_text(clock_ui_context_t *ctx, time_t now, char *out, size_t size)
{
    const ui_skin_t *sk = ui_skin();
    const app_runtime_state_t *rt = ctx->runtime;
    char when[48];
    char in[48];
    time_t next;
    uint32_t c_head = lv_color_to_u32(ALM_IS_ORBIT() ? sk->tint_alarms : sk->text_3) & 0xFFFFFF;
    uint32_t c_main = lv_color_to_u32(sk->text) & 0xFFFFFF;
    const char *head = alm_text(ctx, L10N_NEXT_UP);
    char head_up[48];

    if (ALM_IS_INSTRUMENT()) {
        kit_text_upper(head_up, sizeof(head_up), head);
        head = head_up;
    }
    if (rt->alarm_ringing) {
        snprintf(out, size, "%s", alm_text(ctx, L10N_ALARM_RINGING_NOW));
        return;
    }
    if (rt->snooze_active && rt->snooze_deadline > now) {
        ui_format_clock(rt->snooze_deadline, when, sizeof(when));
        snprintf(out, size, "%s #%06" PRIX32 " %s#", alm_text(ctx, L10N_SNOOZED_UNTIL), c_main, when);
        return;
    }
    if (!ui_next_alarm(ctx, now, &next, NULL)) {
        snprintf(out, size, "%s", alm_text(ctx, L10N_NO_ALARMS_SCHEDULED));
        return;
    }
    ui_format_day_time(ctx, now, next, when, sizeof(when));
    ui_format_countdown(ctx, now, next, in, sizeof(in));
    if (ALM_IS_INSTRUMENT()) {
        lv_point_t sz;

        snprintf(out, size, "#%06" PRIX32 " %s#  #%06" PRIX32 " %s# \xC2\xB7 %s", c_head, head, c_main, when, in);
        lv_text_get_size(&sz, out, sk->font_caption, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_RECOLOR);
        if (sz.x > 446) {
            /* Long countdowns and languages: the LED leads, the legend goes. */
            snprintf(out, size, "#%06" PRIX32 " %s# \xC2\xB7 %s", c_main, when, in);
        }
    } else if (ALM_IS_ORBIT()) {
        snprintf(out, size, "#%06" PRIX32 " %s:# %s#%06" PRIX32 " , %s#", c_head, head, when,
                 lv_color_to_u32(sk->text_2) & 0xFFFFFF, in);
    } else {
        snprintf(out, size, "#%06" PRIX32 " %s#, %s", c_main, when, in);
    }
}

static void sync_next_up(clock_ui_context_t *ctx, time_t now)
{
    char text[160];

    next_up_text(ctx, now, text, sizeof(text));
    alm_set_text(ctx->alarms.list_next, text);
    if (ALM_IS_ATELIER()) {
        /* The gilt "Next up" only leads a scheduled alarm. */
        alm_show(ctx->alarms.list_next_led,
                 ui_next_alarm(ctx, now, NULL, NULL) && !ctx->runtime->alarm_ringing &&
                     !(ctx->runtime->snooze_active && ctx->runtime->snooze_deadline > now));
    } else if (ALM_IS_INSTRUMENT()) {
        alm_show(ctx->alarms.list_next_led, ui_next_alarm(ctx, now, NULL, NULL) || ctx->runtime->alarm_ringing);
    }
    ctx->alarms.cached_minute = (long)(now / 60);
}

static void sync_row(clock_ui_context_t *ctx, int i, time_t now)
{
    const ui_skin_t *sk = ui_skin();
    const alarm_config_t *alarm = &ctx->settings->alarms[i];
    alm_row_t *r = &ctx->alarms.rows[i];
    bool on = alarm->enabled;
    bool once = alarm->repeat_mode == ALARM_REPEAT_ONCE;
    char text[64];

    alm_format_hm(text, sizeof(text), alarm->hour, alarm->minute);
    alm_set_text(r->time, text);
    alm_set_color(r->time, on ? sk->text : (ALM_IS_ATELIER() ? sk->text_2 : sk->text_3), 0);

    if (ALM_IS_ORBIT()) {
        char sum[48];

        alm_repeat_summary(ctx, alarm, sum, sizeof(sum));
        if (once) {
            char date[32];

            alm_format_short_date(ctx, alm_next_occurrence(now, alarm->hour, alarm->minute), date, sizeof(date));
            snprintf(text, sizeof(text), "%s \xC2\xB7 %s", sum, date);
        } else {
            snprintf(text, sizeof(text), "%s", sum);
        }
        alm_set_text(r->title, text);
        alm_set_color(r->title, on ? sk->text_2 : sk->text_3, 0);
    } else {
        if (ALM_IS_ATELIER() && once) {
            char date[32];

            alm_format_short_date(ctx, alm_next_occurrence(now, alarm->hour, alarm->minute), date, sizeof(date));
            snprintf(text, sizeof(text), "%s \xC2\xB7 %s", alm_repeat_title(ctx, alarm), date);
            alm_set_text(r->title, text);
        } else {
            alm_set_text(r->title, alm_repeat_title(ctx, alarm));
        }
        if (ALM_IS_ATELIER()) {
            alm_set_color(r->title, on ? sk->text : sk->text_2, 0);
        } else {
            alm_set_color(r->title, on ? sk->text_2 : sk->text_3, 0);
        }
        alm_show(r->days_box, !once);
        for (int d = 0; d < 7 && !once; d++) {
            int day = alm_display_day(d);
            bool set = (alarm->days_mask & (1U << day)) != 0;
            lv_color_t c;

            alm_set_text(r->days[d], alm_day_key(ctx, day));
            if (ALM_IS_INSTRUMENT()) {
                c = set && on ? sk->text_2 : set ? sk->text_3 : sk->line_strong;
            } else {
                c = set && on ? sk->accent : set ? sk->text_2 : sk->text_3;
            }
            alm_set_color(r->days[d], c, 0);
        }
        if (once) {
            if (ALM_IS_INSTRUMENT()) {
                alm_format_short_date(ctx, alm_next_occurrence(now, alarm->hour, alarm->minute), text, sizeof(text));
                alm_set_text(r->sub, text);
            } else {
                alm_set_text(r->sub, on ? "" : alm_text(ctx, L10N_OFF));
            }
        }
        alm_show(r->sub, once && (ALM_IS_INSTRUMENT() || !on));
    }
    ctx->suppress_events = true;
    kit_switch_set(r->sw, on);
    ctx->suppress_events = false;
}

void alm_list_sync(clock_ui_context_t *ctx, time_t now)
{
    clock_ui_alarm_state_t *a = &ctx->alarms;
    int count = 0;
    int on = 0;
    char text[96];

    if (a->list_page.root == NULL) {
        return;
    }
    for (int i = 0; i < MAX_ALARMS; i++) {
        bool empty = alm_slot_is_empty(&ctx->settings->alarms[i]);

        if (empty) {
            alm_list_close_swipe(ctx, (int8_t)i, false);
        }
        alm_show(a->rows[i].wrap, !empty);
        if (!empty) {
            count++;
            on += ctx->settings->alarms[i].enabled ? 1 : 0;
            sync_row(ctx, i, now);
        }
    }
    alm_show(a->list_group, count > 0);
    alm_show(a->list_add, alm_find_new_slot(ctx) >= 0);

    if (!ALM_IS_ORBIT()) {
        alm_format_count(ctx, count, text, sizeof(text));
        if (ALM_IS_ATELIER() && count > 0) {
            size_t len = strlen(text);

            snprintf(text + len, sizeof(text) - len, " \xC2\xB7 ");
            len = strlen(text);
            snprintf(text + len, sizeof(text) - len, alm_str(ctx, &s_count_on), on);
        }
        kit_page_set_eyebrow(&a->list_page, text);
    }
    sync_next_up(ctx, now);

    alm_set_text(a->list_settings_title, alm_str(ctx, &alm_s_alarm_settings));
    {
        char mins[24];

        alm_format_minutes(ctx, ctx->settings->snooze_minutes, mins, sizeof(mins));
        if (ALM_IS_ATELIER()) {
            snprintf(text, sizeof(text), "%s \xC2\xB7 %s %s",
                     alm_str(ctx, alm_sound_name(ctx->settings->default_alarm_sound)), alm_text(ctx, L10N_SNOOZE), mins);
        } else {
            snprintf(text, sizeof(text), "%s %s \xC2\xB7 %s", alm_text(ctx, L10N_SNOOZE), mins,
                     alm_str(ctx, alm_sound_name(ctx->settings->default_alarm_sound)));
        }
        alm_set_text(a->list_settings_sub, text);
    }

    if (a->focus_alarm_index >= 0 && a->focus_alarm_index < MAX_ALARMS && a->open && !a->editor_open &&
        !lv_obj_has_flag(a->rows[a->focus_alarm_index].wrap, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_update_layout(a->list_page.root);
        lv_obj_scroll_to_view_recursive(a->rows[a->focus_alarm_index].wrap, LV_ANIM_ON);
        a->focus_alarm_index = -1;
    }
}

void alm_list_tick(clock_ui_context_t *ctx, time_t now)
{
    if ((long)(now / 60) != ctx->alarms.cached_minute && !ctx->alarms.management_scrolling) {
        sync_next_up(ctx, now);
    }
}

/* --- Alarm settings page ------------------------------------------------------------------ */

static const uint8_t s_snooze_steps[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 15, 20, 25, 30};

static void settings_nav_cb(lv_event_t *e)
{
    alm_settings_close(lv_event_get_user_data(e));
}

static void snooze_step(clock_ui_context_t *ctx, int dir)
{
    int cur = ctx->settings->snooze_minutes;
    int n = (int)sizeof(s_snooze_steps);
    int next = cur;

    if (dir > 0) {
        for (int i = 0; i < n; i++) {
            if (s_snooze_steps[i] > cur) {
                next = s_snooze_steps[i];
                break;
            }
        }
    } else {
        for (int i = n - 1; i >= 0; i--) {
            if (s_snooze_steps[i] < cur) {
                next = s_snooze_steps[i];
                break;
            }
        }
    }
    if (next != cur) {
        request_set_snooze_minutes(ctx, (uint8_t)next);
    }
    alm_sync_all(ctx);
}

static void snooze_minus_cb(lv_event_t *e)
{
    snooze_step(lv_event_get_user_data(e), -1);
}

static void snooze_plus_cb(lv_event_t *e)
{
    snooze_step(lv_event_get_user_data(e), 1);
}

static void volume_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);
    char text[8];
    int32_t v = lv_slider_get_value(lv_event_get_target(e));

    if (ctx->suppress_events) {
        return;
    }
    /* The readout follows the finger; the setting is stored when the value changes. */
    snprintf(text, sizeof(text), "%d%%", (int)v);
    alm_set_text(ctx->alarms.volume_value, text);
    if (v != ctx->settings->alarm_volume) {
        request_set_alarm_volume(ctx, (uint8_t)v);
    }
}

static void ascending_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);

    if (ctx->suppress_events) {
        return;
    }
    request_set_ascending_alarm_enabled(ctx, lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED));
    alm_sync_all(ctx);
}

/* Tapping the sound row steps the default sound for new alarms and plays it. */
static void settings_sound_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);
    uint8_t next = (uint8_t)((ctx->settings->default_alarm_sound + 1) % ALARM_SOUND_COUNT);

    request_set_default_alarm_sound(ctx, next);
    alm_sound_preview(ctx);
}

static void settings_test_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);

    alm_test_toggle(ctx);
    alm_sync_all(ctx);
}

/* Replaces the speaker of a kit row icon with a note (the alarm sound). */
static void row_icon_note(kit_row_t *row)
{
    lv_obj_t *holder = row->icon;
    uint32_t n;

    if (holder == NULL) {
        return;
    }
    n = lv_obj_get_child_count(holder);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *child = lv_obj_get_child(holder, (int32_t)i);

        if (lv_obj_check_type(child, &lv_image_class)) {
            const lv_image_dsc_t *cur = lv_image_get_src(child);

            lv_image_set_src(child, (cur != NULL && cur->header.w < 26) ? &ui_alarms_note_22 : &ui_alarms_note_28);
        }
    }
}

void alm_settings_create(clock_ui_context_t *ctx)
{
    const ui_skin_t *sk = ui_skin();
    kit_page_t *p = &ctx->alarms.settings_page;
    kit_row_t row;
    lv_obj_t *g;
    lv_obj_t *r;
    lv_obj_t *box;
    lv_obj_t *label;

    kit_page_create(p, ui_chrome_root(ctx), NULL, alm_str(ctx, &alm_s_settings_title),
                    KIT_NAV_BACK, settings_nav_cb, ctx);
    ui_l10n_bind_kit_str(ctx, p->title, &alm_s_settings_title);
    if (ALM_IS_ORBIT() && p->nav != NULL) {
        kit_set_tint(p->nav, sk->tint_alarms);
    }
    lv_obj_set_flex_align(p->content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Snooze: a stepper, minutes as a figure. */
    label = kit_section_label(p->content, alm_text(ctx, L10N_SNOOZE_DURATION));
    ui_l10n_bind_kit(ctx, label, L10N_SNOOZE_DURATION);
    g = ALM_IS_ORBIT() ? kit_group_joined(p->content) : kit_group(p->content);
    r = alm_box(g);
    lv_obj_set_size(r, lv_pct(100), 112);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(r, ALM_IS_ORBIT() ? 20 : ALM_IS_INSTRUMENT() ? 8 : 4, 0);
    if (ALM_IS_INSTRUMENT()) {
        lv_obj_set_style_border_side(r, LV_BORDER_SIDE_TOP, 0);
        lv_obj_set_style_border_width(r, 1, 0);
        lv_obj_set_style_border_color(r, sk->line, 0);
    }
    ctx->alarms.snooze_minus = kit_round_button(r, KIT_BUTTON_SECONDARY, KIT_ICON_MINUS, 72);
    lv_obj_add_event_cb(ctx->alarms.snooze_minus, snooze_minus_cb, LV_EVENT_CLICKED, ctx);
    box = alm_box(r);
    lv_obj_set_size(box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(box, 10, 0);
    ctx->alarms.snooze_value = alm_label(box, sk->font_time_m, sk->text, "9");
    ctx->alarms.snooze_unit = kit_label(box, KIT_TEXT_LABEL, "min");
    lv_obj_set_style_margin_bottom(ctx->alarms.snooze_unit, ALM_IS_ATELIER() ? 14 : 10, 0);
    ctx->alarms.snooze_plus = kit_round_button(r, KIT_BUTTON_SECONDARY, KIT_ICON_PLUS, 72);
    lv_obj_add_event_cb(ctx->alarms.snooze_plus, snooze_plus_cb, LV_EVENT_CLICKED, ctx);
    if (ALM_IS_ORBIT()) {
        kit_set_tint(ctx->alarms.snooze_minus, sk->tint_alarms);
        kit_set_tint(ctx->alarms.snooze_plus, sk->tint_alarms);
    }

    /* Sound: tone with Test, volume, ascending. */
    label = kit_section_label(p->content, alm_text(ctx, L10N_ALARM_SOUND));
    ui_l10n_bind_kit(ctx, label, L10N_ALARM_SOUND);
    g = kit_group(p->content);
    kit_row_create(&row, g,
                   &(kit_row_opts_t) {.icon = KIT_ICON_SOUND,
                                      .tint = sk->tint_alarms,
                                      .title = alm_str(ctx, &alm_s_sound_name),
                                      .subtitle = "",
                                      .trail = KIT_TRAIL_CUSTOM});
    row_icon_note(&row);
    ctx->alarms.settings_sound_title = row.title; /* the default sound, set in sync */
    ctx->alarms.settings_sound_sub = row.subtitle;
    kit_make_pressable(row.row);
    lv_obj_add_event_cb(row.row, settings_sound_cb, LV_EVENT_CLICKED, ctx);
    alm_one_line(row.subtitle);
    ctx->alarms.settings_test_btn = kit_button(row.trail, KIT_BUTTON_QUIET, KIT_ICON_PLAY, alm_str(ctx, &alm_s_test));
    kit_set_tint(ctx->alarms.settings_test_btn, sk->tint_alarms);
    lv_obj_add_event_cb(ctx->alarms.settings_test_btn, settings_test_cb, LV_EVENT_CLICKED, ctx);

    kit_row_create(&row, g,
                   &(kit_row_opts_t) {.icon = KIT_ICON_NONE,
                                      .title = alm_text(ctx, L10N_VOLUME),
                                      .trail = KIT_TRAIL_CUSTOM});
    ui_l10n_bind_kit(ctx, row.title, L10N_VOLUME);
    lv_obj_set_flex_grow(lv_obj_get_parent(row.title), 0);
    lv_obj_set_width(lv_obj_get_parent(row.title), LV_SIZE_CONTENT);
    lv_obj_set_width(row.title, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(row.trail, 1);
    lv_obj_set_style_pad_column(row.trail, 18, 0);
    ctx->alarms.volume_slider = kit_slider(row.trail, 0, 100, sk->tint_alarms);
    kit_set_tint(ctx->alarms.volume_slider, sk->tint_alarms);
    lv_obj_set_width(ctx->alarms.volume_slider, 10);
    lv_obj_set_flex_grow(ctx->alarms.volume_slider, 1);
    lv_obj_add_event_cb(ctx->alarms.volume_slider, volume_cb, LV_EVENT_VALUE_CHANGED, ctx);
    ctx->alarms.volume_value = alm_label(row.trail, ALM_IS_ATELIER() ? sk->font_value : sk->font_body,
                                         ALM_IS_INSTRUMENT() ? sk->text : sk->value, "70%");
    lv_obj_set_style_min_width(ctx->alarms.volume_value, 64, 0);
    lv_obj_set_style_text_align(ctx->alarms.volume_value, LV_TEXT_ALIGN_RIGHT, 0);

    kit_row_create(&row, g,
                   &(kit_row_opts_t) {.icon = KIT_ICON_NONE,
                                      .tint = sk->tint_alarms,
                                      .title = alm_text(ctx, L10N_ASCENDING_ALARM),
                                      .subtitle = alm_text(ctx, L10N_RAMP_UP),
                                      .trail = KIT_TRAIL_SWITCH});
    ui_l10n_bind_kit(ctx, row.title, L10N_ASCENDING_ALARM);
    ui_l10n_bind_kit(ctx, row.subtitle, L10N_RAMP_UP);
    ctx->alarms.ascending_sw = row.control;
    kit_set_tint(row.control, sk->tint_alarms);
    lv_obj_add_event_cb(row.control, ascending_cb, LV_EVENT_VALUE_CHANGED, ctx);

    alm_add_edge_close(ctx, p->root);
}

void alm_settings_sync(clock_ui_context_t *ctx)
{
    clock_ui_alarm_state_t *a = &ctx->alarms;
    const app_settings_t *st = ctx->settings;
    char text[64];
    bool testing = ctx->runtime->alarm_test_active;

    if (a->settings_page.root == NULL) {
        return;
    }
    snprintf(text, sizeof(text), "%u", st->snooze_minutes);
    alm_set_text(a->snooze_value, text);
    alm_set_text(a->snooze_unit, alm_text(ctx, L10N_MINUTE_ABBR));

    ctx->suppress_events = true;
    kit_slider_set(a->volume_slider, st->alarm_volume);
    kit_switch_set(a->ascending_sw, st->ascending_alarm_enabled);
    ctx->suppress_events = false;
    snprintf(text, sizeof(text), "%u%%", st->alarm_volume);
    alm_set_text(a->volume_value, text);

    alm_set_text(a->settings_sound_sub, alm_str(ctx, &alm_s_tap_to_change));
    alm_set_text(a->settings_sound_title, alm_str(ctx, alm_sound_name(st->default_alarm_sound)));
    kit_button_set_text(a->settings_test_btn, alm_str(ctx, testing ? &alm_s_stop : &alm_s_test));
    kit_icon_set(lv_obj_get_child(a->settings_test_btn, 0), testing ? KIT_ICON_STOP : KIT_ICON_PLAY);
}
