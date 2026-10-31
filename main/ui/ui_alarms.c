/*
 * Alarms area: hooks for the core, navigation between the pages, sync from settings and
 * runtime state, and the strings and formatting the pages share.
 *
 * The pages are built in ui_alarms_list.c, ui_alarms_editor.c (+ ui_alarms_timeset.c) and
 * ui_alarms_ring.c, all in the current UI style. They are created with the chrome and only
 * shown and hidden afterwards; a style change rebuilds them.
 */
#include "ui/ui_alarms_internal.h"

#include <math.h>
#include <stdlib.h>

#include "esp_heap_caps.h"

#define ALM_AUTO_CLOSE_MS 20000

/* --- Strings -------------------------------------------------------------------------- */

const ui_l10n_str_t alm_s_alarm_settings =
    UI_L10N_STR("Alarm settings", "Ustawienia budzika", "Налаштування будильника", "Настройки будильника",
                "Wekkerinstellingen");
/* Page title: the page sits inside Alarms, so the other languages can drop "alarm". */
const ui_l10n_str_t alm_s_settings_title =
    UI_L10N_STR("Alarm settings", "Ustawienia", "Налаштування", "Настройки", "Instellingen");
const ui_l10n_str_t alm_s_sound_name =
    UI_L10N_STR("Morning Light", "Poranne światło", "Ранкове світло", "Утренний свет", "Ochtendlicht");
static const ui_l10n_str_t s_sound_chimes =
    UI_L10N_STR("Wind Chimes", "Dzwonki wietrzne", "Музика вітру", "Музыка ветра", "Windgong");
static const ui_l10n_str_t s_sound_marimba = UI_L10N_STR("Marimba", "Marimba", "Марімба", "Маримба", "Marimba");
static const ui_l10n_str_t s_sound_bowl =
    UI_L10N_STR("Singing Bowl", "Misa dźwiękowa", "Співоча чаша", "Поющая чаша", "Klankschaal");
const ui_l10n_str_t alm_s_sound = UI_L10N_STR("Sound", "Dźwięk", "Звук", "Звук", "Geluid");
const ui_l10n_str_t alm_s_tap_to_change =
    UI_L10N_STR("Tap to change", "Dotknij, aby zmienić", "Торкніться, щоб змінити", "Нажмите, чтобы сменить",
                "Tik om te wijzigen");
const ui_l10n_str_t alm_s_sound_tap =
    UI_L10N_STR("Sound · tap to change", "Dźwięk · dotknij, aby zmienić", "Звук · торкніться, щоб змінити",
                "Звук · нажмите, чтобы сменить", "Geluid · tik om te wijzigen");
const ui_l10n_str_t alm_s_repeat = UI_L10N_STR("Repeat", "Powtarzanie", "Повтор", "Повтор", "Herhalen");
const ui_l10n_str_t alm_s_new_alarm =
    UI_L10N_STR("New alarm", "Nowy budzik", "Новий будильник", "Новый будильник", "Nieuwe wekker");
const ui_l10n_str_t alm_s_test = UI_L10N_STR("Test", "Test", "Тест", "Тест", "Test");
const ui_l10n_str_t alm_s_stop = UI_L10N_STR("Stop", "Stop", "Стоп", "Стоп", "Stop");
const ui_l10n_str_t alm_s_hold = UI_L10N_STR("Hold", "Trzymaj", "Тримайте", "Держите", "Houd in");
const ui_l10n_str_t alm_s_slide_to_stop =
    UI_L10N_STR("slide to stop", "przesuń, aby wyłączyć", "проведіть, щоб вимкнути", "проведите, чтобы выключить",
                "schuif om te stoppen");
const ui_l10n_str_t alm_s_solve =
    UI_L10N_STR("Solve to stop", "Rozwiąż, aby wyłączyć", "Розв'яжіть, щоб вимкнути", "Решите, чтобы выключить",
                "Los op om te stoppen");
const ui_l10n_str_t alm_s_solve_long =
    UI_L10N_STR("Solve to stop the alarm", "Rozwiąż, aby wyłączyć budzik", "Розв'яжіть, щоб вимкнути",
                "Решите, чтобы выключить", "Los op om de wekker te stoppen");
const ui_l10n_str_t alm_s_tap_to_undo =
    UI_L10N_STR("tap to undo", "dotknij, aby przywrócić", "торкніться, щоб повернути", "нажмите для возврата",
                "tik om te herstellen");
const ui_l10n_str_t alm_s_custom = UI_L10N_STR("Custom", "Własne", "Власний", "Свой", "Aangepast");

/* Day keys by weekday (0 = Sunday). One letter where it is unambiguous, two otherwise. */
static const ui_l10n_str_t s_day_key[7] = {
    UI_L10N_STR("S", "Nd", "Нд", "Вс", "Zo"), UI_L10N_STR("M", "Pn", "Пн", "Пн", "Ma"),
    UI_L10N_STR("T", "Wt", "Вт", "Вт", "Di"), UI_L10N_STR("W", "Śr", "Ср", "Ср", "Wo"),
    UI_L10N_STR("T", "Cz", "Чт", "Чт", "Do"), UI_L10N_STR("F", "Pt", "Пт", "Пт", "Vr"),
    UI_L10N_STR("S", "So", "Сб", "Сб", "Za"),
};

/* "%d alarm(s)": one, few (2-4), many. Dutch and English only use the first two. */
static const ui_l10n_str_t s_count_one = UI_L10N_STR("%d alarm", "%d budzik", "%d будильник", "%d будильник",
                                                     "%d wekker");
static const ui_l10n_str_t s_count_few = UI_L10N_STR("%d alarms", "%d budziki", "%d будильники", "%d будильника",
                                                     "%d wekkers");
static const ui_l10n_str_t s_count_many = UI_L10N_STR("%d alarms", "%d budzików", "%d будильників",
                                                      "%d будильников", "%d wekkers");

const char *alm_str(const clock_ui_context_t *ctx, const ui_l10n_str_t *str)
{
    return ui_l10n_str(ctx, str);
}

const char *alm_text(const clock_ui_context_t *ctx, l10n_text_id_t id)
{
    return ui_l10n_text(ctx, id);
}

const char *alm_day_key(const clock_ui_context_t *ctx, int wday)
{
    return ui_l10n_str(ctx, &s_day_key[((wday % 7) + 7) % 7]);
}

int alm_display_day(int index)
{
    return (index + 1) % 7; /* Monday first */
}

/* --- Formatting ----------------------------------------------------------------------- */

void alm_format_hm(char *out, size_t size, int hour, int minute)
{
    snprintf(out, size, "%02d:%02d", hour % 24, minute % 60);
}

const char *alm_repeat_title(const clock_ui_context_t *ctx, const alarm_config_t *alarm)
{
    switch (alarm_repeat_preset_from_config(alarm)) {
    case ALARM_REPEAT_PRESET_ONCE:
        return alm_text(ctx, L10N_ONCE);
    case ALARM_REPEAT_PRESET_EVERY_DAY:
        return alm_text(ctx, L10N_EVERY_DAY);
    case ALARM_REPEAT_PRESET_WEEKDAYS:
        return alm_text(ctx, L10N_WEEKDAYS);
    case ALARM_REPEAT_PRESET_WEEKENDS:
        return alm_text(ctx, L10N_WEEKENDS);
    default:
        return alm_str(ctx, &alm_s_custom);
    }
}

void alm_repeat_summary(const clock_ui_context_t *ctx, const alarm_config_t *alarm, char *out, size_t size)
{
    size_t pos = 0;

    if (alarm_repeat_preset_from_config(alarm) != 0xFF) {
        snprintf(out, size, "%s", alm_repeat_title(ctx, alarm));
        return;
    }
    out[0] = '\0';
    for (int i = 0; i < 7 && pos < size; i++) {
        int day = alm_display_day(i);

        if ((alarm->days_mask & (1U << day)) == 0) {
            continue;
        }
        pos += (size_t)snprintf(out + pos, size - pos, "%s%s", pos > 0 ? " " : "",
                                l10n_day_short((clock_language_t)ctx->settings->language, day));
    }
}

time_t alm_next_occurrence(time_t now, int hour, int minute)
{
    struct tm tm;
    time_t t;

    localtime_r(&now, &tm);
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = 0;
    t = mktime(&tm);
    if (t <= now) {
        tm.tm_mday += 1;
        t = mktime(&tm);
    }
    return t;
}

void alm_format_short_date(const clock_ui_context_t *ctx, time_t when, char *out, size_t size)
{
    struct tm tm;

    localtime_r(&when, &tm);
    snprintf(out, size, "%s %d %s", l10n_day_short((clock_language_t)ctx->settings->language, tm.tm_wday),
             tm.tm_mday, ui_month_name(ctx, tm.tm_mon, false));
}

void alm_format_long_date(const clock_ui_context_t *ctx, time_t when, char *out, size_t size)
{
    ui_format_long_date(ctx, when, out, size);
}

void alm_format_count(const clock_ui_context_t *ctx, int count, char *out, size_t size)
{
    const ui_l10n_str_t *form = &s_count_many;
    clock_language_t lang = (clock_language_t)ctx->settings->language;
    int mod10 = count % 10;
    int mod100 = count % 100;

    if (lang == CLOCK_LANGUAGE_ENGLISH || lang == CLOCK_LANGUAGE_DUTCH) {
        form = count == 1 ? &s_count_one : &s_count_few;
    } else if (lang == CLOCK_LANGUAGE_POLISH) {
        form = count == 1 ? &s_count_one
               : (mod10 >= 2 && mod10 <= 4 && (mod100 < 12 || mod100 > 14)) ? &s_count_few
                                                                             : &s_count_many;
    } else {
        form = (mod10 == 1 && mod100 != 11) ? &s_count_one
               : (mod10 >= 2 && mod10 <= 4 && (mod100 < 12 || mod100 > 14)) ? &s_count_few
                                                                             : &s_count_many;
    }
    snprintf(out, size, alm_str(ctx, form), count);
}

void alm_format_minutes(const clock_ui_context_t *ctx, int minutes, char *out, size_t size)
{
    snprintf(out, size, "%d %s", minutes, alm_text(ctx, L10N_MINUTE_ABBR));
}

bool alm_slot_is_empty(const alarm_config_t *alarm)
{
    static const alarm_config_t zero;

    /* A deleted alarm (app_action_delete_alarm) or a never used, zeroed slot. */
    return (!alarm->enabled && alarm->hour == 7 && alarm->minute == 0 && alarm->repeat_mode == ALARM_REPEAT_ONCE &&
            alarm->days_mask == 0x7F && !alarm->math_unlock_enabled) ||
           memcmp(alarm, &zero, sizeof(zero)) == 0;
}

int alm_find_new_slot(const clock_ui_context_t *ctx)
{
    /* Only empty slots: find_alarm_slot_for_new_alarm() would reuse a disabled alarm. */
    for (int i = 0; i < MAX_ALARMS; i++) {
        if (alm_slot_is_empty(&ctx->settings->alarms[i])) {
            return i;
        }
    }
    return -1;
}

bool alm_ringing(const clock_ui_context_t *ctx)
{
    return ctx->runtime->alarm_ringing || ctx->alarms.debug_ringing;
}

const alarm_config_t *alm_active_alarm(const clock_ui_context_t *ctx)
{
    int8_t i = ctx->runtime->active_alarm_index;

    if (ctx->alarms.debug_ringing && !ctx->runtime->alarm_ringing) {
        i = 0;
    }
    if (i < 0 || i >= MAX_ALARMS) {
        return NULL;
    }
    return &ctx->settings->alarms[i];
}

time_t alm_now(void)
{
    time_t now;

    time(&now);
    return now;
}

/* --- Drawing helpers ------------------------------------------------------------------ */

lv_obj_t *alm_box(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);

    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE);
    return obj;
}

lv_obj_t *alm_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color, const char *text)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_label_set_text(label, text != NULL ? text : "");
    lv_obj_add_flag(label, LV_OBJ_FLAG_EVENT_BUBBLE);
    return label;
}

void alm_one_line(lv_obj_t *label)
{
    lv_obj_set_height(label, lv_font_get_line_height(lv_obj_get_style_text_font(label, 0)));
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
}

void alm_draw_rim(lv_layer_t *layer, int32_t skip_from_deg, int32_t skip_to_deg)
{
    kit_draw_minute_rim(layer, skip_from_deg, skip_to_deg);
}

void alm_set_text(lv_obj_t *label, const char *text)
{
    kit_label_set_text_if_changed(label, text);
}

void alm_set_color(lv_obj_t *obj, lv_color_t color, lv_part_t part)
{
    if (obj != NULL && !lv_color_eq(lv_obj_get_style_text_color(obj, part), color)) {
        lv_obj_set_style_text_color(obj, color, part);
    }
}

void alm_show(lv_obj_t *obj, bool show)
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

void alm_polar(int32_t cx, int32_t cy, int32_t r, int32_t angle_x10, int32_t *x, int32_t *y)
{
    float a = (float)angle_x10 * 3.14159265f / 1800.0f;

    *x = cx + (int32_t)lroundf((float)r * sinf(a));
    *y = cy - (int32_t)lroundf((float)r * cosf(a));
}

int32_t alm_angle_of(int32_t cx, int32_t cy, int32_t x, int32_t y, int32_t *dist)
{
    float dx = (float)(x - cx);
    float dy = (float)(y - cy);
    int32_t a = (int32_t)lroundf(atan2f(dx, -dy) * 1800.0f / 3.14159265f);

    if (dist != NULL) {
        *dist = (int32_t)lroundf(sqrtf(dx * dx + dy * dy));
    }
    return (a + 3600) % 3600;
}

lv_obj_t *alm_canvas(lv_obj_t *parent, int32_t w, int32_t h, void **buf)
{
    lv_obj_t *canvas;

    if (*buf == NULL) {
        /* Cache-line aligned, as LVGL draw buffers and the PPA expect. */
        *buf = heap_caps_aligned_alloc(CONFIG_CACHE_L2_CACHE_LINE_SIZE, (size_t)w * (size_t)h * sizeof(lv_color16_t),
                                       MALLOC_CAP_SPIRAM);
    }
    if (*buf == NULL) {
        return NULL;
    }
    canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(canvas, *buf, w, h, LV_COLOR_FORMAT_RGB565);
    lv_obj_remove_flag(canvas, LV_OBJ_FLAG_CLICKABLE);
    return canvas;
}

/* --- Auto close ----------------------------------------------------------------------- */

static void auto_close_cb(lv_timer_t *timer)
{
    clock_ui_context_t *ctx = lv_timer_get_user_data(timer);

    if (!ctx->alarms.open || ctx->alarms.editor_open || ctx->alarms.settings_open) {
        lv_timer_pause(timer);
        return;
    }
    if (ctx->alarms.management_scrolling || ctx->alarms.close_dragging || ctx->alarms.list_swipe_dragging) {
        alm_auto_close_reset(ctx);
        return;
    }
    alm_list_close(ctx);
}

void alm_auto_close_reset(clock_ui_context_t *ctx)
{
    if (ctx->alarms.auto_close_timer == NULL) {
        return;
    }
    lv_timer_set_period(ctx->alarms.auto_close_timer, ALM_AUTO_CLOSE_MS);
    lv_timer_resume(ctx->alarms.auto_close_timer);
    lv_timer_reset(ctx->alarms.auto_close_timer);
}

void alm_auto_close_pause(clock_ui_context_t *ctx)
{
    if (ctx->alarms.auto_close_timer != NULL) {
        lv_timer_pause(ctx->alarms.auto_close_timer);
    }
}

/* --- Edge swipe to close ---------------------------------------------------------------- */

static void close_top(clock_ui_context_t *ctx)
{
    if (ctx->alarms.editor_open) {
        alm_editor_close(ctx);
    } else if (ctx->alarms.settings_open) {
        alm_settings_close(ctx);
    } else if (ctx->alarms.open) {
        alm_list_close(ctx);
    }
}

static void edge_close_cb(lv_event_t *e)
{
    ui_edge_ctx_t *edge = lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_event_get_indev(e);
    clock_ui_context_t *ctx = edge->ui;
    lv_point_t p;

    if (indev == NULL || !ui_alarms_is_open(ctx)) {
        return;
    }
    if (ctx->alarms.open && !ctx->alarms.editor_open && !ctx->alarms.settings_open) {
        alm_auto_close_reset(ctx);
    }
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        ctx->alarms.close_dragging = false;
        ctx->alarms.close_swipe_consumed = false;
        return;
    }
    if (ctx->alarms.close_swipe_consumed) {
        return;
    }
    lv_indev_get_point(indev, &p);
    if (code == LV_EVENT_PRESSED) {
        ctx->alarms.close_dragging = true;
        ctx->alarms.close_drag_start_point = p;
        return;
    }
    if (code == LV_EVENT_PRESSING && ctx->alarms.close_dragging &&
        ui_surface_edge_swipe_trigger(edge->edge, &ctx->alarms.close_drag_start_point, &p,
                                      SETTINGS_CLOSE_SWIPE_TRIGGER)) {
        ctx->alarms.close_dragging = false;
        ctx->alarms.close_swipe_consumed = true;
        close_top(ctx);
    }
}

void alm_add_edge_close(clock_ui_context_t *ctx, lv_obj_t *page_root)
{
    static const int32_t zone[4] = {SETTINGS_CLOSE_EDGE_ZONE, ALARM_CLOSE_BOTTOM_EDGE_ZONE, SETTINGS_CLOSE_EDGE_ZONE,
                                    SETTINGS_CLOSE_EDGE_ZONE};

    for (int edge = UI_SURFACE_EDGE_TOP; edge <= UI_SURFACE_EDGE_RIGHT; edge++) {
        lv_obj_t *sensor;

        ctx->alarms.close_edge_ctx[edge].ui = ctx;
        ctx->alarms.close_edge_ctx[edge].edge = (ui_surface_edge_t)edge;
        ui_surface_create_edge_sensor(page_root, &sensor, (ui_surface_edge_t)edge, zone[edge], edge_close_cb,
                                      &ctx->alarms.close_edge_ctx[edge]);
    }
}

/* --- Navigation ----------------------------------------------------------------------- */

static void page_show(lv_obj_t *root, bool show)
{
    alm_show(root, show);
    if (show && root != NULL) {
        lv_obj_move_foreground(root);
    }
}

void alm_list_open(clock_ui_context_t *ctx)
{
    ui_face_chrome_close_all(ctx);
    ui_settings_close_all(ctx);
    if (ctx->alarms.editor_open) {
        ctx->alarms.editor_open = false;
        alm_show(ctx->alarms.editor_root, false);
    }
    if (ctx->alarms.settings_open) {
        alm_settings_close(ctx);
    }
    alm_list_close_swipe(ctx, ctx->alarms.swipe_open_index, false);
    ctx->alarms.management_scrolling = false;
    ctx->alarms.close_swipe_consumed = false;
    ctx->alarms.open = true;
    if (ctx->alarms.list_page.scroll != NULL) {
        lv_obj_scroll_to_y(ctx->alarms.list_page.scroll, 0, LV_ANIM_OFF);
    }
    alm_sync_all(ctx);
    page_show(ctx->alarms.list_page.root, true);
    kit_page_glass_enter(&ctx->alarms.list_page, true);
    alm_auto_close_reset(ctx);
}

void alm_list_close(clock_ui_context_t *ctx)
{
    alm_list_close_swipe(ctx, ctx->alarms.swipe_open_index, false);
    if (ctx->alarms.settings_open) {
        ctx->alarms.open = false; /* settings close must not reopen the list */
        alm_settings_close(ctx);
    }
    alm_auto_close_pause(ctx);
    ctx->alarms.open = false;
    ctx->alarms.close_swipe_consumed = false;
    alm_show(ctx->alarms.list_page.root, false);
    if (!ctx->alarms.editor_open) {
        ui_face_chrome_show_controls(ctx);
    }
}

void alm_settings_open(clock_ui_context_t *ctx)
{
    if (ctx->alarms.settings_page.root == NULL) {
        return;
    }
    alm_auto_close_pause(ctx);
    alm_list_close_swipe(ctx, ctx->alarms.swipe_open_index, false);
    ctx->alarms.settings_open = true;
    ctx->alarms.close_swipe_consumed = false;
    lv_obj_scroll_to_y(ctx->alarms.settings_page.scroll, 0, LV_ANIM_OFF);
    alm_sync_all(ctx);
    page_show(ctx->alarms.settings_page.root, true);
    alm_show(ctx->alarms.list_page.root, false);
}

void alm_settings_close(clock_ui_context_t *ctx)
{
    alm_test_stop(ctx);
    ctx->alarms.settings_open = false;
    ctx->alarms.close_swipe_consumed = false;
    alm_show(ctx->alarms.settings_page.root, false);
    if (ctx->alarms.open) {
        page_show(ctx->alarms.list_page.root, true);
        alm_auto_close_reset(ctx);
    } else if (!ctx->alarms.editor_open) {
        ui_face_chrome_show_controls(ctx);
    }
}

void alm_editor_open(clock_ui_context_t *ctx, uint8_t index, bool is_new)
{
    alarm_config_t *draft = &ctx->alarms.editor_draft;

    if (index >= MAX_ALARMS || ctx->alarms.editor_root == NULL) {
        return;
    }
    alm_list_close_swipe(ctx, ctx->alarms.swipe_open_index, false);
    alm_auto_close_pause(ctx);
    ctx->alarms.editor_index = (int8_t)index;
    ctx->alarms.editor_is_new = is_new;
    ctx->alarms.editor_open = true;
    ctx->alarms.close_swipe_consumed = false;
    *draft = ctx->settings->alarms[index];
    if (is_new) {
        draft->sound = ctx->settings->default_alarm_sound;
        time_t now = alm_now();
        struct tm tm;
        int minute;

        localtime_r(&now, &tm);
        minute = ((tm.tm_min + 9) / 10) * 10;
        if (minute >= 60) {
            minute -= 60;
            tm.tm_hour = (tm.tm_hour + 1) % 24;
        }
        draft->enabled = true; /* new alarms are on; the editor hides Enabled for them */
        draft->repeat_mode = ALARM_REPEAT_ONCE;
        draft->days_mask = 0x7F;
        draft->math_unlock_enabled = false;
        draft->hour = (uint8_t)tm.tm_hour;
        draft->minute = (uint8_t)minute;
    }
    alm_editor_prepare(ctx);
    alm_sync_all(ctx);
    page_show(ctx->alarms.editor_root, true);
    alm_show(ctx->alarms.list_page.root, false);
}

void alm_editor_close(clock_ui_context_t *ctx)
{
    alm_test_stop(ctx);
    ctx->alarms.editor_open = false;
    ctx->alarms.close_swipe_consumed = false;
    alm_show(ctx->alarms.editor_root, false);
    if (ctx->alarms.open) {
        alm_sync_all(ctx);
        page_show(ctx->alarms.list_page.root, true);
        alm_auto_close_reset(ctx);
    } else {
        ui_face_chrome_show_controls(ctx);
    }
}

const ui_l10n_str_t *alm_sound_name(uint8_t sound)
{
    switch (sound) {
    case ALARM_SOUND_WIND_CHIMES:
        return &s_sound_chimes;
    case ALARM_SOUND_MARIMBA:
        return &s_sound_marimba;
    case ALARM_SOUND_SINGING_BOWL:
        return &s_sound_bowl;
    default:
        return &alm_s_sound_name;
    }
}

/* The sound Test plays: the alarm being edited, else the default for new alarms. */
static uint8_t test_sound(const clock_ui_context_t *ctx)
{
    return ctx->alarms.editor_open ? ctx->alarms.editor_draft.sound : ctx->settings->default_alarm_sound;
}

void alm_test_toggle(clock_ui_context_t *ctx)
{
    if (ctx->callbacks.on_alarm_test_requested != NULL) {
        ctx->callbacks.on_alarm_test_requested(ctx->user_ctx, test_sound(ctx));
    }
}

void alm_sound_preview(clock_ui_context_t *ctx)
{
    alm_test_stop(ctx);
    alm_test_toggle(ctx);
}

void alm_test_stop(clock_ui_context_t *ctx)
{
    if (ctx->runtime != NULL && ctx->runtime->alarm_test_active && ctx->callbacks.on_alarm_test_stop_requested != NULL) {
        ctx->callbacks.on_alarm_test_stop_requested(ctx->user_ctx);
    }
}

/* --- Sync ----------------------------------------------------------------------------- */

static bool needs_sync(const clock_ui_context_t *ctx)
{
    const clock_ui_alarm_state_t *a = &ctx->alarms;
    const app_runtime_state_t *rt = ctx->runtime;
    const app_settings_t *st = ctx->settings;

    return !a->cache_valid || memcmp(a->cached_alarms, st->alarms, sizeof(a->cached_alarms)) != 0 ||
           a->cached_alarm_ringing != rt->alarm_ringing || a->cached_alarm_test_active != rt->alarm_test_active ||
           a->cached_snooze_active != rt->snooze_active || a->cached_snooze_deadline != rt->snooze_deadline ||
           a->cached_next_alarm_epoch != rt->next_alarm_epoch || a->cached_next_alarm_index != rt->next_alarm_index ||
           a->cached_active_alarm_index != rt->active_alarm_index || a->cached_alarm_volume != st->alarm_volume ||
           a->cached_ascending_alarm_enabled != st->ascending_alarm_enabled ||
           a->cached_snooze_minutes != st->snooze_minutes || a->cached_skipped_alarm_index != st->skipped_alarm_index ||
           a->cached_skipped_alarm_epoch != st->skipped_alarm_epoch || a->cached_language != st->language ||
           a->cached_default_sound != st->default_alarm_sound;
}

void alm_sync_all(clock_ui_context_t *ctx)
{
    clock_ui_alarm_state_t *a = &ctx->alarms;
    const app_runtime_state_t *rt = ctx->runtime;
    const app_settings_t *st = ctx->settings;
    time_t now = alm_now();

    alm_list_sync(ctx, now);
    alm_settings_sync(ctx);
    if (a->editor_open) {
        alm_editor_sync(ctx);
    }
    alm_ring_sync(ctx, now);

    memcpy(a->cached_alarms, st->alarms, sizeof(a->cached_alarms));
    a->cached_alarm_ringing = rt->alarm_ringing;
    a->cached_alarm_test_active = rt->alarm_test_active;
    a->cached_snooze_active = rt->snooze_active;
    a->cached_snooze_deadline = rt->snooze_deadline;
    a->cached_next_alarm_epoch = rt->next_alarm_epoch;
    a->cached_next_alarm_index = rt->next_alarm_index;
    a->cached_active_alarm_index = rt->active_alarm_index;
    a->cached_alarm_volume = st->alarm_volume;
    a->cached_ascending_alarm_enabled = st->ascending_alarm_enabled;
    a->cached_snooze_minutes = st->snooze_minutes;
    a->cached_skipped_alarm_index = st->skipped_alarm_index;
    a->cached_skipped_alarm_epoch = st->skipped_alarm_epoch;
    a->cached_language = st->language;
    a->cached_default_sound = st->default_alarm_sound;
    a->cache_valid = true;
}

/* --- Debug entry points (simulator, perf tour) ------------------------------------------ */

static void debug_open_editor(clock_ui_context_t *ctx)
{
    alm_list_open(ctx);
    alm_editor_open(ctx, 0, false);
}

static void debug_editor_details(clock_ui_context_t *ctx)
{
    debug_open_editor(ctx);
    alm_editor_show_details(ctx, true, false);
}

static void debug_new_alarm(clock_ui_context_t *ctx)
{
    int slot = alm_find_new_slot(ctx);

    alm_list_open(ctx);
    if (slot >= 0) {
        alm_editor_open(ctx, (uint8_t)slot, true);
    }
}

static void debug_new_alarm_details(clock_ui_context_t *ctx)
{
    debug_new_alarm(ctx);
    alm_editor_show_details(ctx, true, false);
}

static void debug_close_editor(clock_ui_context_t *ctx)
{
    if (ctx->alarms.editor_open) {
        alm_editor_close(ctx);
    }
}

static void debug_alarm_settings(clock_ui_context_t *ctx)
{
    alm_list_open(ctx);
    alm_settings_open(ctx);
}

static void debug_list_scroll(clock_ui_context_t *ctx)
{
    lv_obj_t *scroll = ctx->alarms.settings_open ? ctx->alarms.settings_page.scroll
                       : ctx->alarms.editor_open ? (ctx->alarms.editor_page2 != NULL && ctx->alarms.editor_on_details
                                                        ? ctx->alarms.editor_page2
                                                        : ctx->alarms.editor_scroll)
                                                 : ctx->alarms.list_page.scroll;

    if (scroll != NULL) {
        lv_obj_update_layout(scroll);
        lv_obj_scroll_to_y(scroll, LV_COORD_MAX, LV_ANIM_OFF);
    }
}

static void debug_ringing(clock_ui_context_t *ctx)
{
    ctx->alarms.debug_ringing = true;
    ctx->alarms.debug_math = false;
    alm_ring_sync(ctx, alm_now());
}

static void debug_ringing_math(clock_ui_context_t *ctx)
{
    ctx->alarms.debug_ringing = true;
    ctx->alarms.debug_math = true;
    alm_ring_sync(ctx, alm_now());
    alm_ring_show_math(ctx);
}

static void debug_ringing_off(clock_ui_context_t *ctx)
{
    ctx->alarms.debug_ringing = false;
    ctx->alarms.debug_math = false;
    alm_ring_sync(ctx, alm_now());
}

static void debug_banner_feedback(clock_ui_context_t *ctx)
{
    alm_banner_feedback(ctx, alm_text(ctx, L10N_CANCELLED), true, alm_now());
}

/* Taps the editor's Sound row: the next sound, previewed. */
static void debug_sound_next(clock_ui_context_t *ctx)
{
    ctx->alarms.editor_draft.sound = (uint8_t)((ctx->alarms.editor_draft.sound + 1) % ALARM_SOUND_COUNT);
    alm_sound_preview(ctx);
    alm_editor_sync(ctx);
}

static void debug_swipe_delete(clock_ui_context_t *ctx)
{
    alm_list_open(ctx);
    lv_obj_update_layout(ctx->alarms.list_page.root);
    alm_list_reveal_delete(ctx, 0);
}

/* --- Hooks ---------------------------------------------------------------------------- */

void ui_alarms_create(clock_ui_context_t *ctx)
{
    ctx->alarms.editor_index = -1;
    ctx->alarms.focus_alarm_index = -1;
    ctx->alarms.swipe_open_index = -1;
    ctx->alarms.swipe_drag_index = -1;
    ctx->alarms.banner_face = -1;
    ctx->alarms.ring_sec_shown = -1;

    alm_banner_create(ctx);
    alm_list_create(ctx);
    alm_settings_create(ctx);
    alm_editor_create(ctx);
    alm_ring_create(ctx);
    ctx->alarms.auto_close_timer = ui_chrome_timer_create(ctx, auto_close_cb, ALM_AUTO_CLOSE_MS, ctx);
    if (ctx->alarms.auto_close_timer != NULL) {
        lv_timer_pause(ctx->alarms.auto_close_timer);
    }

    ui_debug_register("open_alarms", ui_alarms_open);
    ui_debug_register("close_alarms", ui_alarms_close_all);
    ui_debug_register("open_alarm_editor", debug_open_editor);
    ui_debug_register("alarm_editor_details", debug_editor_details);
    ui_debug_register("new_alarm", debug_new_alarm);
    ui_debug_register("new_alarm_details", debug_new_alarm_details);
    ui_debug_register("close_alarm_editor", debug_close_editor);
    ui_debug_register("alarm_settings", debug_alarm_settings);
    ui_debug_register("alarms_scroll_end", debug_list_scroll);
    ui_debug_register("alarm_swipe_delete", debug_swipe_delete);
    ui_debug_register("ringing", debug_ringing);
    ui_debug_register("ringing_math", debug_ringing_math);
    ui_debug_register("ringing_off", debug_ringing_off);
    ui_debug_register("banner_feedback", debug_banner_feedback);
    ui_debug_register("alarm_sound_next", debug_sound_next);
    ui_debug_register("alarm_test_stop", alm_test_stop);
}

void ui_alarms_destroy(clock_ui_context_t *ctx)
{
    alm_editor_free(ctx);
    alm_ring_free(ctx);
}

void ui_alarms_tick(clock_ui_context_t *ctx, time_t now, bool face_covered)
{
    if (!face_covered) {
        alm_banner_style(ctx, ctx->face_chrome.active_face); /* follows theme changes; cheap when unchanged */
        alm_banner_update(ctx, now);
    }
    alm_ring_sync(ctx, now);
    alm_ring_tick(ctx, now);
    if ((ctx->alarms.open || ctx->alarms.settings_open || ctx->alarms.editor_open) &&
        !ctx->alarms.management_scrolling && !ctx->alarms.list_swipe_dragging && needs_sync(ctx)) {
        alm_sync_all(ctx);
    }
    if (ctx->alarms.open && !ctx->alarms.editor_open && !ctx->alarms.settings_open) {
        alm_list_tick(ctx, now);
    }
    if (ctx->alarms.editor_open) {
        alm_editor_tick(ctx, now);
    }
}

void ui_alarms_refresh(clock_ui_context_t *ctx)
{
    if (needs_sync(ctx)) {
        alm_sync_all(ctx);
    }
}

void ui_alarms_on_face_changed(clock_ui_context_t *ctx, clock_face_id_t face)
{
    alm_banner_style(ctx, face);
}

bool ui_alarms_banner_shown(const clock_ui_context_t *ctx, time_t now)
{
    return ctx->alarms.banner != NULL && alm_banner_wanted(ctx, now);
}

void ui_alarms_open(clock_ui_context_t *ctx)
{
    face_glass_capture(ctx, 1 /* UI_GLASS_PAGE */);
    alm_list_open(ctx);
}

bool ui_alarms_is_open(const clock_ui_context_t *ctx)
{
    return ctx->alarms.open || ctx->alarms.editor_open || ctx->alarms.settings_open;
}

void ui_alarms_close_all(clock_ui_context_t *ctx)
{
    if (ctx->alarms.editor_open) {
        bool was_open = ctx->alarms.open;

        ctx->alarms.open = false; /* go straight to the face */
        alm_editor_close(ctx);
        ctx->alarms.open = was_open;
    }
    if (ctx->alarms.open || ctx->alarms.settings_open) {
        alm_list_close(ctx);
    }
}
