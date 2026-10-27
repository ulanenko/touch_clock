/*
 * Night overlay (screen 12): while night mode is active, a minimal overlay on the night
 * face: a moon at 12 and the next alarm with the time until it at 6. The texts take the
 * face's own inks (its LCD glow, lit dots or printed lettering), so they read as part of
 * the face rather than as UI laid over it; the fonts stay the UI style's.
 *
 * Idle cost: the texts are rebuilt only when the minute, the alarm, the language or the face
 * change; nothing is touched on the other ticks. The overlay draws no fill of its own.
 */
#include "ui/ui_areas.h"
#include "ui/ui_face_chrome.h"
#include "ui/ui_format.h"

static bool s_night_forced; /* "night_overlay" debug call: show it outside night mode */

typedef struct {
    int16_t moon_y;  /* top of the moon icon */
    int16_t row_y;   /* top of the alarm row */
    int16_t count_y; /* top of the countdown */
} night_layout_t;

/* The design places the alarm at 6 o'clock; faces with text there get another spot, so the
 * overlay stays off each face's numerals, date and logo lines. */
static night_layout_t night_layout(clock_face_id_t face)
{
    clock_ui_style_t style = ui_skin_style();
    night_layout_t l;

    switch (style) {
    case CLOCK_UI_STYLE_ORBIT:
        l = (night_layout_t) {52, 564, 602};
        break;
    case CLOCK_UI_STYLE_ATELIER:
        l = (night_layout_t) {78, 557, 607};
        break;
    default:
        l = (night_layout_t) {76, 553, 598};
        break;
    }

    switch (face) {
    case CLOCK_FACE_DIGITAL:
        /* Moon above the date line, the alarm below the weekday row. */
        l.moon_y = 40;
        l.row_y += 40;
        l.count_y += 40;
        break;
    case CLOCK_FACE_STERNGLAS:
    case CLOCK_FACE_AVENIR: {
        /* A numeral sits at 6: the alarm goes in the open dial under the 12. */
        int16_t dy = (int16_t)(132 - l.row_y);

        /* Avenir has a tick at 12 where Sternglas leaves a gap: no moon on it. */
        l.moon_y = face == CLOCK_FACE_AVENIR ? -1 : 34;
        l.row_y = (int16_t)(l.row_y + dy);
        l.count_y = (int16_t)(l.count_y + dy);
        break;
    }
    case CLOCK_FACE_MODERN_SILVER:
        l.moon_y = 96; /* inside the dot ring */
        break;
    default:
        break;
    }
    return l;
}

void face_night_create(clock_ui_context_t *ctx)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_face_chrome_state_t *fc = &ctx->face_chrome;
    clock_ui_style_t style = ui_skin_style();
    lv_color_t ink = sk->night; /* until the first sync colours them for the face */

    fc->night = face_plain(ui_chrome_root(ctx));
    lv_obj_set_size(fc->night, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_add_flag(fc->night, LV_OBJ_FLAG_HIDDEN);

    fc->night_moon = style == CLOCK_UI_STYLE_INSTRUMENT ? kit_icon_small(fc->night, KIT_ICON_MOON, ink)
                                                        : kit_icon(fc->night, KIT_ICON_MOON, ink);

    fc->night_alarm_row = face_plain(fc->night);
    lv_obj_set_size(fc->night_alarm_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(fc->night_alarm_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(fc->night_alarm_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(fc->night_alarm_row, style == CLOCK_UI_STYLE_ORBIT ? 10 : 12, 0);
    switch (style) {
    case CLOCK_UI_STYLE_INSTRUMENT:
        fc->night_alarm_icon = kit_icon_small(fc->night_alarm_row, KIT_ICON_ALARM, ink);
        fc->night_time = face_text(fc->night_alarm_row, sk->font_heading, ink, "");
        fc->night_countdown = kit_label(fc->night, KIT_TEXT_LABEL, "");
        break;
    case CLOCK_UI_STYLE_ATELIER:
        fc->night_alarm_icon = kit_icon_small(fc->night_alarm_row, KIT_ICON_BELL, ink);
        fc->night_time = face_text(fc->night_alarm_row, sk->font_value, ink, "");
        fc->night_countdown = face_text(fc->night, sk->font_body, ink, "");
        break;
    case CLOCK_UI_STYLE_ORBIT:
    default:
        fc->night_alarm_icon = kit_icon(fc->night_alarm_row, KIT_ICON_ALARM, ink);
        fc->night_time = face_text(fc->night_alarm_row, sk->font_heading, ink, "");
        fc->night_countdown = face_text(fc->night, sk->font_body, ink, "");
        break;
    }
    fc->night_minute = -1;
    fc->night_face = CLOCK_FACE_COUNT;
    fc->night_inked = false;
}

/* Colours the texts with the face's inks; only when they change, as restyling redraws. */
static void night_ink(clock_ui_context_t *ctx, clock_face_id_t face)
{
    clock_ui_face_chrome_state_t *fc = &ctx->face_chrome;
    ui_face_ink_t ink = ui_face_ink(ctx, face);

    if (fc->night_inked && lv_color_eq(ink.text, fc->night_ink.text) && lv_color_eq(ink.accent, fc->night_ink.accent) &&
        lv_color_eq(ink.soft, fc->night_ink.soft)) {
        return;
    }
    fc->night_ink = ink;
    fc->night_inked = true;
    lv_obj_set_style_image_recolor(fc->night_moon, ink.soft, 0);
    lv_obj_set_style_image_recolor(fc->night_alarm_icon, ink.accent, 0);
    lv_obj_set_style_text_color(fc->night_time, ink.text, 0);
    lv_obj_set_style_text_color(fc->night_countdown, ink.soft, 0);
}

int32_t ui_face_chrome_alarm_slot_y(clock_face_id_t face)
{
    return night_layout(face).row_y;
}

static void night_show(lv_obj_t *obj, bool show)
{
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN) != show) {
        return;
    }
    if (show) {
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

static void night_place(clock_ui_context_t *ctx, clock_face_id_t face)
{
    clock_ui_face_chrome_state_t *fc = &ctx->face_chrome;
    night_layout_t l = night_layout(face);

    if (l.moon_y < 0) {
        lv_obj_add_flag(fc->night_moon, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(fc->night_moon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(fc->night_moon, LV_ALIGN_TOP_MID, 0, l.moon_y);
    }
    lv_obj_align(fc->night_alarm_row, LV_ALIGN_TOP_MID, 0, l.row_y);
    lv_obj_align(fc->night_countdown, LV_ALIGN_TOP_MID, 0, l.count_y);
}

void face_night_sync(clock_ui_context_t *ctx, time_t now, bool force)
{
    clock_ui_face_chrome_state_t *fc = &ctx->face_chrome;
    const app_runtime_state_t *rt = ctx->runtime;
    clock_face_id_t face = fc->active_face;
    bool show;
    time_t when = 0;
    bool has_alarm;
    long minute = (long)(now / 60);

    if (fc->night == NULL) {
        return;
    }
    show = (s_night_forced || (rt != NULL && rt->in_night_mode)) && !fc->hidden && !fc->visible &&
           !ui_settings_is_open(ctx) && !ui_alarms_is_open(ctx) && !(rt != NULL && rt->alarm_ringing);
    if (!show) {
        if (fc->night_visible) {
            fc->night_visible = false;
            lv_obj_add_flag(fc->night, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    has_alarm = face_next_alarm(ctx, now, &when);
    if (force || !fc->night_visible || minute != fc->night_minute || when != fc->night_alarm ||
        face != fc->night_face || fc->night_language != ctx->settings->language) {
        char clock[8] = "";
        char countdown[64] = "";

        if (face != fc->night_face) {
            night_place(ctx, face);
        }
        fc->night_minute = minute;
        fc->night_alarm = has_alarm ? when : 0;
        fc->night_face = face;
        fc->night_language = ctx->settings->language;
        if (has_alarm) {
            ui_format_clock(when, clock, sizeof(clock));
            ui_format_countdown(ctx, now, when, countdown, sizeof(countdown));
        }
        kit_label_set_text_if_changed(fc->night_time, clock);
        kit_label_set_text_if_changed(fc->night_countdown, countdown);
    }
    /* The banner (snoozed, or waking within half an hour) shows the next wake in this
     * spot; the overlay keeps only its moon then. */
    {
        bool row = fc->night_alarm != 0 && !ui_alarms_banner_shown(ctx, now);

        night_show(fc->night_alarm_row, row);
        night_show(fc->night_countdown, row);
    }
    night_ink(ctx, face);
    if (!fc->night_visible) {
        fc->night_visible = true;
        lv_obj_remove_flag(fc->night, LV_OBJ_FLAG_HIDDEN);
    }
}

static void debug_night_overlay(clock_ui_context_t *ctx)
{
    s_night_forced = !s_night_forced;
    face_night_sync(ctx, face_now(), true);
}

void face_night_register_debug(void)
{
    ui_debug_register("night_overlay", debug_night_overlay);
}
