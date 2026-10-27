/*
 * Night overlay (screen 12): while night mode is active, a minimal, very dim overlay on the
 * night face: a moon at 12 and the next alarm with the time until it at 6. Orbit adds a
 * 24-hour rim ring with an arc from now to the alarm.
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

        l.moon_y = face == CLOCK_FACE_AVENIR ? 62 : 34;
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

static int16_t dial_deg(time_t t)
{
    struct tm tm;

    localtime_r(&t, &tm);
    return (int16_t)(((tm.tm_hour * 60 + tm.tm_min) * 360) / (24 * 60));
}

static void night_draw_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(e);
    const clock_ui_face_chrome_state_t *fc = &ctx->face_chrome;
    const ui_skin_t *sk = ui_skin();
    lv_layer_t *layer = lv_event_get_layer(e);
    int32_t start;
    int32_t end;
    float a;

    if (ui_skin_style() != CLOCK_UI_STYLE_ORBIT) {
        return;
    }
    /* A 24 h ring barely above black; the arc runs from now to the alarm. */
    kit_draw_ring(layer, KIT_CENTER, KIT_CENTER, 342, 4, 0, 360, lv_color_mix(sk->tint_night, lv_color_black(), 52), LV_OPA_COVER);
    if (!fc->night_has_alarm) {
        return;
    }
    start = fc->night_now_deg - 90;
    end = fc->night_alarm_deg - 90;
    if (end <= start) {
        end += 360;
    }
    kit_draw_ring(layer, KIT_CENTER, KIT_CENTER, 342, 4, start, end, lv_color_mix(sk->tint_night, lv_color_black(), 128),
                  LV_OPA_COVER);
    a = (float)(fc->night_alarm_deg - 90) * 3.14159265f / 180.0f;
    face_draw_dot(layer, (int32_t)lroundf(KIT_CENTER + 342.0f * cosf(a)), (int32_t)lroundf(KIT_CENTER + 342.0f * sinf(a)),
                  7, sk->tint_night, LV_OPA_COVER);
}

void face_night_create(clock_ui_context_t *ctx)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_face_chrome_state_t *fc = &ctx->face_chrome;
    clock_ui_style_t style = ui_skin_style();
    lv_color_t ink = sk->night;
    lv_color_t ink_moon;
    lv_color_t ink_count;

    if (style == CLOCK_UI_STYLE_ORBIT) {
        ink_moon = lv_color_mix(sk->tint_night, lv_color_black(), 180);
        ink_count = lv_color_mix(sk->night, lv_color_black(), 176);
    } else if (style == CLOCK_UI_STYLE_ATELIER) {
        ink_moon = lv_color_mix(sk->night, lv_color_black(), 200);
        ink_count = ink;
    } else {
        ink_moon = ink;
        ink_count = ink;
    }

    fc->night = face_plain(ui_chrome_root(ctx));
    lv_obj_set_size(fc->night, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_add_flag(fc->night, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(fc->night, night_draw_cb, LV_EVENT_DRAW_MAIN_END, ctx);

    fc->night_moon = style == CLOCK_UI_STYLE_INSTRUMENT ? kit_icon_small(fc->night, KIT_ICON_MOON, ink_moon)
                                                        : kit_icon(fc->night, KIT_ICON_MOON, ink_moon);

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
        fc->night_countdown = face_text(fc->night, sk->font_body, ink_count, "");
        break;
    case CLOCK_UI_STYLE_ORBIT:
    default:
        fc->night_alarm_icon = kit_icon(fc->night_alarm_row, KIT_ICON_ALARM, ink);
        fc->night_time = face_text(fc->night_alarm_row, sk->font_heading, ink, "");
        fc->night_countdown = face_text(fc->night, sk->font_body, ink_count, "");
        break;
    }
    lv_obj_set_style_text_color(fc->night_countdown, ink_count, 0);
    fc->night_minute = -1;
    fc->night_face = CLOCK_FACE_COUNT;
}

static void night_place(clock_ui_context_t *ctx, clock_face_id_t face)
{
    clock_ui_face_chrome_state_t *fc = &ctx->face_chrome;
    night_layout_t l = night_layout(face);

    lv_obj_align(fc->night_moon, LV_ALIGN_TOP_MID, 0, l.moon_y);
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
        fc->night_alarm = when;
        fc->night_face = face;
        fc->night_language = ctx->settings->language;
        fc->night_has_alarm = has_alarm;
        if (has_alarm) {
            ui_format_clock(when, clock, sizeof(clock));
            ui_format_countdown(ctx, now, when, countdown, sizeof(countdown));
            fc->night_now_deg = dial_deg(now);
            fc->night_alarm_deg = dial_deg(when);
        }
        kit_label_set_text_if_changed(fc->night_time, clock);
        kit_label_set_text_if_changed(fc->night_countdown, countdown);
        if (has_alarm) {
            lv_obj_remove_flag(fc->night_alarm_row, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(fc->night_countdown, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(fc->night_alarm_row, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(fc->night_countdown, LV_OBJ_FLAG_HIDDEN);
        }
        if (ui_skin_style() == CLOCK_UI_STYLE_ORBIT) {
            lv_obj_invalidate(fc->night);
        }
    }
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
