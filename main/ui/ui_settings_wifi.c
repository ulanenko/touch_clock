/*
 * Settings › Wi-Fi: the connected network, the scanned networks, the clock (time zone, auto
 * sync, sync now, manual time) and Forget; the join network screen with its keyboard; the
 * time zone and manual time pages.
 */
#include "ui/ui_settings_internal.h"

#include <stdio.h>
#include <string.h>

#include "assets/ui_assets/settings_assets.h"
#include "domain/timezone_rules.h"

static const ui_l10n_str_t s_other_networks = UI_L10N_STR("Other networks", "Inne sieci", "Інші мережі",
                                                          "Другие сети", "Andere netwerken");
static const ui_l10n_str_t s_connected = UI_L10N_STR("Connected", "Połączono", "Підключено", "Подключено",
                                                     "Verbonden");
static const ui_l10n_str_t s_not_connected = UI_L10N_STR("Not connected", "Brak połączenia", "Не підключено",
                                                         "Не подключено", "Niet verbonden");
static const ui_l10n_str_t s_found = UI_L10N_STR("%u found", "Znaleziono %u", "Знайдено %u", "Найдено %u",
                                                 "%u gevonden");
static const ui_l10n_str_t s_scan_for = UI_L10N_STR("Scan for networks", "Szukaj sieci", "Пошук мереж",
                                                    "Поиск сетей", "Netwerken zoeken");
static const ui_l10n_str_t s_scanning = UI_L10N_STR("Scanning…", "Skanowanie…", "Сканування…", "Сканирование…",
                                                    "Zoeken…");
static const ui_l10n_str_t s_time = UI_L10N_STR("Time", "Czas", "Час", "Время", "Tijd");
static const ui_l10n_str_t s_time_clock = UI_L10N_STR("Time & clock", "Czas i zegar", "Час і годинник",
                                                      "Время и часы", "Tijd en klok");
static const ui_l10n_str_t s_sync = UI_L10N_STR("Sync", "Synchr.", "Синхр.", "Синхр.", "Sync");
static const ui_l10n_str_t s_sync_now = UI_L10N_STR("Sync now", "Synchr.", "Синхр.", "Синхр.", "Nu sync");
static const ui_l10n_str_t s_scan_short = UI_L10N_STR("Scan", "Skanuj", "Пошук", "Поиск", "Zoek");
static const ui_l10n_str_t s_last_synced = UI_L10N_STR("Last synced %s", "Ostatnio %s", "Остання синхр. %s",
                                                       "Последняя синхр. %s", "Laatst %s");
static const ui_l10n_str_t s_synced = UI_L10N_STR("Synced %s", "Synchr. %s", "Синхр. %s", "Синхр. %s",
                                                  "Gesynchr. %s");
static const ui_l10n_str_t s_set_date_time = UI_L10N_STR("Set date and time", "Ustaw datę i godzinę",
                                                         "Встановити дату й час", "Установить дату и время",
                                                         "Datum en tijd instellen");
static const ui_l10n_str_t s_forget = UI_L10N_STR("Forget network", "Zapomnij sieć", "Забути мережу", "Забыть сеть",
                                                  "Netwerk vergeten");
static const ui_l10n_str_t s_open = UI_L10N_STR("Open", "Otwarta", "Відкрита", "Открытая", "Open");
static const ui_l10n_str_t s_signal[3] = {
    UI_L10N_STR("weak signal", "słaby sygnał", "слабкий сигнал", "слабый сигнал", "zwak signaal"),
    UI_L10N_STR("good signal", "dobry sygnał", "добрий сигнал", "хороший сигнал", "goed signaal"),
    UI_L10N_STR("strong signal", "silny sygnał", "сильний сигнал", "сильный сигнал", "sterk signaal"),
};
static const ui_l10n_str_t s_date_time = UI_L10N_STR("Date and time", "Data i godzina", "Дата й час", "Дата и время",
                                                     "Datum en tijd");
static const ui_l10n_str_t s_date = UI_L10N_STR("Date", "Data", "Дата", "Дата", "Datum");

#define JOIN_KB_W 612

/* --- Helpers --------------------------------------------------------------------------- */

static bool wifi_connected(const clock_ui_context_t *ctx)
{
    return ctx->runtime->wifi_connected && ctx->settings->wifi.ssid[0] != '\0';
}

static const char *auth_name(const clock_ui_context_t *ctx, int authmode)
{
    switch (authmode) {
    case 0:
        return ui_l10n_str(ctx, &s_open);
    case 1:
        return "WEP";
    case 2:
        return "WPA";
    case 3:
        return "WPA2";
    case 4:
        return "WPA/WPA2";
    case 5:
        return "WPA2-Enterprise";
    case 6:
        return "WPA3";
    case 7:
        return "WPA2/WPA3";
    default:
        return "WPA";
    }
}

static lv_obj_t *button_label(lv_obj_t *button)
{
    uint32_t n = lv_obj_get_child_count(button);

    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *child = lv_obj_get_child(button, (int32_t)i);

        if (lv_obj_check_type(child, &lv_label_class)) {
            return child;
        }
    }
    return NULL;
}

/* --- Callbacks ------------------------------------------------------------------------- */

static void scan_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);

    if (ctx->runtime->wifi_scanning) {
        return;
    }
    if (ctx->callbacks.on_wifi_scan_requested != NULL) {
        ctx->callbacks.on_wifi_scan_requested(ctx->user_ctx);
    }
    st_wifi_sync(ctx, false);
}

static void sync_now_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);

    if (ctx->callbacks.on_wifi_sync_requested != NULL) {
        ctx->callbacks.on_wifi_sync_requested(ctx->user_ctx);
    }
}

static void forget_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);

    if (ctx->callbacks.on_wifi_forget_requested != NULL) {
        ctx->callbacks.on_wifi_forget_requested(ctx->user_ctx);
    }
    st_wifi_sync(ctx, true);
}

static void auto_sync_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);
    bool on = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);

    if (ctx->suppress_events) {
        return;
    }
    request_set_time_sync_mode(ctx, on ? TIME_SYNC_MODE_AUTO : TIME_SYNC_MODE_MANUAL);
    st_wifi_sync(ctx, false);
}

static void timezone_row_cb(lv_event_t *e)
{
    st_timezone_open(lv_event_get_user_data(e));
}

static void manual_row_cb(lv_event_t *e)
{
    st_show_page(lv_event_get_user_data(e), UI_ST_MANUAL_TIME);
}

static void network_cb(lv_event_t *e)
{
    ui_st_item_t *item = lv_event_get_user_data(e);
    clock_ui_context_t *ctx = item->ui;
    const app_runtime_state_t *rt = ctx->runtime;

    if (item->index >= rt->wifi_scan_count || item->index >= CLOCK_WIFI_SCAN_RESULT_MAX) {
        return;
    }
    st_join_open(ctx, rt->wifi_scan_results[item->index].ssid, rt->wifi_scan_results[item->index].rssi,
                 rt->wifi_scan_results[item->index].authmode);
}

/* --- Page ------------------------------------------------------------------------------ */

static void build_hero(clock_ui_context_t *ctx, kit_page_t *p)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_settings_state_t *st = ST(ctx);
    lv_obj_t *col;
    lv_obj_t *line;

    if (ui_skin_is_instrument()) {
        /* Readout panel: signal meter, network and its link details. */
        st->wifi_hero = kit_card(p->content);
        lv_obj_set_flex_flow(st->wifi_hero, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(st->wifi_hero, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(st->wifi_hero, 20, 0);
        lv_obj_set_style_min_height(st->wifi_hero, 104, 0);
        lv_obj_set_style_pad_ver(st->wifi_hero, 16, 0);
        st->wifi_hero_signal = st_signal(st->wifi_hero, 30);
        col = st_vbox(st->wifi_hero, 4);
        lv_obj_set_flex_grow(col, 1);
        line = st_hbox(col, 12);
        st->wifi_hero_title = kit_label(line, KIT_TEXT_TITLE, "");
        lv_obj_set_style_text_font(st->wifi_hero_title, sk->font_value, 0);
        lv_label_set_long_mode(st->wifi_hero_title, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_style_max_width(st->wifi_hero_title, 330, 0);
        st->wifi_hero_mark = st_dot(line, 8, sk->accent);
        st->wifi_hero_sub = kit_label(col, KIT_TEXT_CAPTION, "");
        lv_obj_set_style_text_color(st->wifi_hero_sub, sk->text_2, 0);
        lv_obj_set_width(st->wifi_hero_sub, lv_pct(100));
        lv_label_set_long_mode(st->wifi_hero_sub, LV_LABEL_LONG_MODE_DOTS);
    } else if (ui_skin_is_orbit()) {
        kit_row_t row;
        kit_row_opts_t o = {0};
        lv_obj_t *group = kit_group(p->content);

        o.icon = KIT_ICON_WIFI;
        o.tint = sk->tint_wifi;
        o.title = " ";
        o.subtitle = " ";
        o.trail = KIT_TRAIL_CUSTOM;
        kit_row_create(&row, group, &o);
        st->wifi_hero = row.row;
        st->wifi_hero_title = row.title;
        st->wifi_hero_sub = row.subtitle;
        st->wifi_hero_signal = st_signal(row.trail, 20);
        st->wifi_hero_mark = row.icon;
    } else {
        /* Centred lockup: signal, network name in the serif, check. */
        col = st_vbox(p->content, 6);
        lv_obj_set_width(col, lv_pct(100));
        lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_top(col, 4, 0);
        st->wifi_hero = col;
        line = st_hbox(col, 14);
        st->wifi_hero_signal = st_signal(line, 32);
        st->wifi_hero_title = kit_label(line, KIT_TEXT_TITLE, "");
        lv_label_set_long_mode(st->wifi_hero_title, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_style_max_width(st->wifi_hero_title, 320, 0);
        st->wifi_hero_mark = kit_icon_small(line, KIT_ICON_CHECK, sk->accent);
        st->wifi_hero_sub = kit_label(col, KIT_TEXT_BODY, "");
        lv_obj_set_style_max_width(st->wifi_hero_sub, sk->content_w, 0);
        lv_label_set_long_mode(st->wifi_hero_sub, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_style_text_align(st->wifi_hero_sub, LV_TEXT_ALIGN_CENTER, 0);
    }
}

static void build_clock(clock_ui_context_t *ctx, kit_page_t *p)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_settings_state_t *st = ST(ctx);
    lv_obj_t *group;
    lv_obj_t *col;
    lv_obj_t *button;
    kit_row_t row;
    kit_row_opts_t o = {0};

    st_bind(ctx, kit_section_label(p->content, ""),
            ui_skin_is_instrument() ? ST_ID(L10N_CLOCK) : ui_skin_is_orbit() ? ST_STR(&s_time) : ST_STR(&s_time_clock));
    group = kit_group(p->content);

    /* Time zone. */
    o.on_click = timezone_row_cb;
    o.user_data = ctx;
    if (ui_skin_is_instrument()) {
        o.trail = KIT_TRAIL_CUSTOM;
        st_row(ctx, &row, group, &o, ST_ID(L10N_TIME_ZONE), ST_NONE);
        col = st_vbox(row.trail, 0);
        lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
        st->wifi_tz_value = kit_label(col, KIT_TEXT_BODY, "");
        lv_obj_set_style_text_color(st->wifi_tz_value, sk->text, 0);
        st->wifi_tz_sub = kit_label(col, KIT_TEXT_CAPTION, "");
        kit_icon(row.trail, KIT_ICON_CHEVRON_RIGHT, sk->text_3);
    } else if (ui_skin_is_orbit()) {
        o.icon = KIT_ICON_GLOBE;
        o.tint = sk->tint_wifi;
        o.trail = KIT_TRAIL_CHEVRON;
        o.subtitle = " ";
        st_row(ctx, &row, group, &o, ST_ID(L10N_TIME_ZONE), ST_NONE);
        st->wifi_tz_sub = row.subtitle;
    } else {
        o.icon = KIT_ICON_GLOBE;
        o.trail = KIT_TRAIL_CHEVRON;
        o.title = " ";
        o.subtitle = " ";
        kit_row_create(&row, group, &o);
        st->wifi_tz_title = row.title;
        st->wifi_tz_sub = row.subtitle;
    }

    /* Auto sync. */
    o = (kit_row_opts_t) {0};
    o.trail = KIT_TRAIL_SWITCH;
    o.tint = sk->tint_wifi;
    if (!ui_skin_is_instrument()) {
        o.icon = KIT_ICON_CLOCK;
    } else {
        o.subtitle = " ";
    }
    st_row(ctx, &row, group, &o, ST_ID(L10N_AUTO_SYNC), ST_NONE);
    st->wifi_auto_sw = row.control;
    st->wifi_auto_sub = row.subtitle;
    if (row.subtitle != NULL) {
        lv_obj_set_style_text_font(row.subtitle, sk->font_caption, 0);
        lv_obj_set_style_text_color(row.subtitle, sk->text_3, 0);
    }
    lv_obj_add_event_cb(row.control, auto_sync_cb, LV_EVENT_VALUE_CHANGED, ctx);

    /* Sync time. */
    o = (kit_row_opts_t) {0};
    if (ui_skin_is_orbit()) {
        o.trail = KIT_TRAIL_VALUE;
        o.value = " ";
        o.on_click = sync_now_cb;
        o.user_data = ctx;
        st_row(ctx, &row, group, &o, ST_ID(L10N_SYNC_TIME), ST_NONE);
        lv_obj_set_style_text_color(row.title, sk->accent, 0);
        st->wifi_sync_sub = row.value;
    } else {
        o.trail = KIT_TRAIL_CUSTOM;
        o.subtitle = " ";
        if (ui_skin_is_atelier()) {
            o.icon = KIT_ICON_SYNC;
        }
        st_row(ctx, &row, group, &o, ST_ID(L10N_SYNC_TIME), ST_NONE);
        st->wifi_sync_sub = row.subtitle;
        if (ui_skin_is_instrument()) {
            lv_obj_set_style_text_font(row.subtitle, sk->font_caption, 0);
            lv_obj_set_style_text_color(row.subtitle, sk->text_3, 0);
        }
        button = kit_button(row.trail, KIT_BUTTON_QUIET, ui_skin_is_instrument() ? KIT_ICON_SYNC : KIT_ICON_NONE, "");
        st_bind(ctx, button_label(button), ST_STR(ui_skin_is_instrument() ? &s_sync : &s_sync_now));
        lv_obj_add_event_cb(button, sync_now_cb, LV_EVENT_CLICKED, ctx);
    }

    /* Manual date and time (manual mode only). */
    o = (kit_row_opts_t) {0};
    o.trail = KIT_TRAIL_CHEVRON;
    o.on_click = manual_row_cb;
    o.user_data = ctx;
    if (!ui_skin_is_instrument()) {
        o.icon = KIT_ICON_CLOCK;
        o.tint = sk->tint_wifi;
    }
    st_row(ctx, &row, group, &o, ST_STR(&s_set_date_time), ST_NONE);
    st->wifi_manual_row = row.row;
}

static void build_networks(clock_ui_context_t *ctx, kit_page_t *p)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_settings_state_t *st = ST(ctx);
    lv_obj_t *label;

    label = kit_section_label(p->content, "");
    st_bind(ctx, label, ui_skin_is_instrument() ? ST_ID(L10N_NETWORKS) : ST_STR(&s_other_networks));
    st->wifi_other_label = lv_obj_get_parent(label);
    if (ui_skin_is_instrument()) {
        st->wifi_net_count = kit_label(lv_obj_get_parent(label), KIT_TEXT_LABEL, "");
    }
    st->wifi_net_group = kit_group(p->content);

    if (ui_skin_is_instrument()) {
        lv_obj_t *wrap = st_hbox(p->content, 0);

        lv_obj_set_width(wrap, lv_pct(100));
        lv_obj_set_flex_align(wrap, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_ver(wrap, 24, 0);
        st->wifi_scan_btn = kit_button(wrap, KIT_BUTTON_QUIET, KIT_ICON_SYNC, "");
        lv_obj_set_height(st->wifi_scan_btn, 72);
        lv_obj_set_style_min_width(st->wifi_scan_btn, 176, 0);
        st->wifi_scan_label = button_label(st->wifi_scan_btn);
        lv_obj_set_style_text_color(st->wifi_scan_label, sk->text, 0);
    } else if (ui_skin_is_orbit()) {
        lv_obj_t *act = kit_group(p->content);

        st->wifi_scan_btn = act;
        lv_obj_set_height(act, 88);
        lv_obj_set_style_pad_bottom(act, 0, 0);
        lv_obj_set_style_margin_bottom(act, 16, 0);
        lv_obj_set_style_bg_opa(act, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(act, sk->accent_soft, 0);
        lv_obj_set_style_radius(act, 32, 0);
        lv_obj_set_flex_flow(act, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(act, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(act, 12, 0);
        kit_make_pressable(act);
        kit_icon(act, KIT_ICON_SYNC, sk->accent);
        st->wifi_scan_label = kit_label(act, KIT_TEXT_HEADING, "");
        lv_obj_set_style_text_color(st->wifi_scan_label, sk->accent, 0);
    } else {
        lv_obj_t *key = kit_page_set_action(p, KIT_ICON_SYNC, NULL, NULL);

        st->wifi_scan_btn = key;
        st->wifi_scan_label = kit_label(p->root, KIT_TEXT_LABEL, "");
        st_one_line(st->wifi_scan_label, 150);
        lv_obj_set_style_text_align(st->wifi_scan_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(st->wifi_scan_label, sk->text_2, 0);
        lv_obj_set_pos(st->wifi_scan_label, KIT_SCREEN - sk->nav_x - sk->round_button / 2 - 75,
                       sk->page_top + sk->round_button + 12);
    }
    lv_obj_add_event_cb(st->wifi_scan_btn, scan_cb, LV_EVENT_CLICKED, ctx);
}

void st_wifi_build(clock_ui_context_t *ctx)
{
    clock_ui_settings_state_t *st = ST(ctx);
    kit_page_t *p;
    lv_obj_t *wrap;

    p = st_page_begin(ctx, UI_ST_WIFI, ui_skin_is_atelier() ? ST_ID(L10N_SETTINGS) : ST_NONE, ST_ID(L10N_WIFI), KIT_NAV_BACK);
    if (ui_skin_is_instrument()) {
        kit_page_set_eyebrow(p, " ");
    }
    build_hero(ctx, p);
    if (ui_skin_is_instrument()) {
        build_clock(ctx, p);
        build_networks(ctx, p);
    } else {
        build_networks(ctx, p);
        build_clock(ctx, p);
    }

    wrap = st_hbox(p->content, 0);
    lv_obj_set_width(wrap, lv_pct(100));
    lv_obj_set_flex_align(wrap, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(wrap, ui_skin_is_instrument() ? 0 : 24, 0);
    st->wifi_forget_btn = kit_button(wrap, KIT_BUTTON_DESTRUCTIVE, KIT_ICON_NONE, "");
    st_bind(ctx, button_label(st->wifi_forget_btn), ST_STR(&s_forget));
    lv_obj_add_event_cb(st->wifi_forget_btn, forget_cb, LV_EVENT_CLICKED, ctx);
    st->wifi_list_valid = false;
}

static void rebuild_networks(clock_ui_context_t *ctx)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_settings_state_t *st = ST(ctx);
    const app_runtime_state_t *rt = ctx->runtime;
    size_t count = LV_MIN(rt->wifi_scan_count, (size_t)CLOCK_WIFI_SCAN_RESULT_MAX);
    bool connected = wifi_connected(ctx);
    size_t shown = 0;
    size_t listed = 0;
    char text[64];

    lv_obj_clean(st->wifi_net_group);
    for (size_t i = 0; i < count; i++) {
        const clock_wifi_scan_result_t *r = &rt->wifi_scan_results[i];
        bool is_current = connected && strcmp(r->ssid, ctx->settings->wifi.ssid) == 0;
        kit_row_t row;
        kit_row_opts_t o = {0};
        lv_obj_t *meter;

        if (is_current && !ui_skin_is_instrument()) {
            continue; /* shown as the hero */
        }
        listed++;
        if (shown >= UI_ST_NET_MAX) {
            continue;
        }
        st->net_item[shown].ui = ctx;
        st->net_item[shown].index = (uint8_t)i;
        o.title = r->ssid[0] != '\0' ? r->ssid : "—";
        o.subtitle = is_current ? ui_l10n_str(ctx, &s_connected) : NULL;
        o.trail = KIT_TRAIL_CUSTOM;
        o.on_click = network_cb;
        o.user_data = &st->net_item[shown];
        kit_row_create(&row, st->wifi_net_group, &o);
        if (row.subtitle != NULL) {
            lv_obj_set_style_text_font(row.subtitle, sk->font_caption, 0);
            lv_obj_set_style_text_color(row.subtitle, sk->accent, 0);
        }
        if (ui_skin_is_atelier()) {
            meter = st_signal(row.row, 28);
            lv_obj_move_to_index(meter, 0);
            st_signal_set(meter, st_signal_level(r->rssi), false);
            if (r->authmode != 0) {
                kit_icon_small(row.trail, KIT_ICON_LOCK, sk->text_3);
            }
        } else {
            if (is_current) {
                kit_icon_small(row.trail, KIT_ICON_CHECK, sk->accent);
            } else if (r->authmode != 0) {
                kit_icon_small(row.trail, KIT_ICON_LOCK, ui_skin_is_orbit() ? sk->text_2 : sk->text_3);
            }
            meter = st_signal(row.trail, ui_skin_is_orbit() ? 18 : 20);
            st_signal_set(meter, st_signal_level(r->rssi), false);
        }
        shown++;
    }

    if (shown == 0 || listed > shown) {
        lv_obj_t *label;

        if (shown == 0) {
            snprintf(text, sizeof(text), "%s", ui_l10n_text(ctx, L10N_NO_NETWORKS));
        } else {
            snprintf(text, sizeof(text), ui_l10n_text(ctx, L10N_SHOWING_NETWORKS), (unsigned)shown, (unsigned)listed);
        }
        label = kit_label(st->wifi_net_group, KIT_TEXT_BODY, text);
        lv_obj_set_width(label, lv_pct(100));
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_ver(label, 16, 0);
    }
    if (st->wifi_net_count != NULL) {
        snprintf(text, sizeof(text), ui_l10n_str(ctx, &s_found), (unsigned)listed);
        kit_label_set_text_if_changed(st->wifi_net_count, text);
    }
    st->wifi_list_gen = rt->wifi_scan_generation;
    st->wifi_list_connected = connected;
    snprintf(st->wifi_list_ssid, sizeof(st->wifi_list_ssid), "%s", ctx->settings->wifi.ssid);
    st->wifi_list_valid = true;
}

void st_wifi_sync(clock_ui_context_t *ctx, bool force)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_settings_state_t *st = ST(ctx);
    const app_runtime_state_t *rt = ctx->runtime;
    bool connected = wifi_connected(ctx);
    bool saved = ctx->settings->wifi.ssid[0] != '\0';
    bool manual = ctx->settings->wifi.time_sync_mode == TIME_SYNC_MODE_MANUAL;
    int level = connected ? st_signal_level(rt->wifi_rssi) : 0;
    char name[40];
    char offset[24];
    char text[128];

    if (!st->built[UI_ST_WIFI]) {
        return;
    }

    /* Hero. */
    kit_label_set_text_if_changed(st->wifi_hero_title, saved ? ctx->settings->wifi.ssid : ui_l10n_str(ctx, &s_not_connected));
    if (connected) {
        if (ui_skin_is_instrument()) {
            if (rt->wifi_ip[0] != '\0') {
                snprintf(text, sizeof(text), "\xE2\x88\x92%d dBm · %s", -rt->wifi_rssi, rt->wifi_ip);
            } else {
                snprintf(text, sizeof(text), "\xE2\x88\x92%d dBm", -rt->wifi_rssi);
            }
        } else if (ui_skin_is_orbit()) {
            snprintf(text, sizeof(text), "%s", ui_l10n_str(ctx, &s_connected));
        } else {
            snprintf(text, sizeof(text), "%s · %s", ui_l10n_str(ctx, &s_connected),
                     ui_l10n_str(ctx, &s_signal[level >= 4 ? 2 : level >= 3 ? 1 : 0]));
        }
    } else if (saved && rt->wifi_status[0] != '\0') {
        l10n_runtime_status(ctx->settings->language, rt->wifi_status, text, sizeof(text));
    } else {
        snprintf(text, sizeof(text), "%s", saved ? ui_l10n_str(ctx, &s_not_connected) : ui_l10n_text(ctx, L10N_SAVED_NONE));
    }
    kit_label_set_text_if_changed(st->wifi_hero_sub, text);
    st_signal_set(st->wifi_hero_signal, level, false);
    if (ui_skin_is_orbit()) {
        st_text_color(st->wifi_hero_sub, connected ? sk->accent : sk->text_2);
        st_bg_color(st->wifi_hero_mark, connected ? sk->tint_wifi : sk->raised);
    } else {
        kit_show(st->wifi_hero_mark, connected);
    }

    /* Networks: rebuilt only when a scan finished or the connection changed. */
    if (force || !st->wifi_list_valid || st->wifi_list_gen != rt->wifi_scan_generation ||
        st->wifi_list_connected != connected || strcmp(st->wifi_list_ssid, ctx->settings->wifi.ssid) != 0) {
        rebuild_networks(ctx);
    }
    if (ui_skin_is_orbit()) {
        snprintf(text, sizeof(text), "%s", ui_l10n_str(ctx, rt->wifi_scanning ? &s_scanning : &s_scan_for));
    } else {
        snprintf(text, sizeof(text), "%s", ui_l10n_str(ctx, rt->wifi_scanning ? &s_scanning : &s_scan_short));
    }
    kit_label_set_text_if_changed(st->wifi_scan_label, text);
    st_set_enabled(st->wifi_scan_btn, !rt->wifi_scanning);

    /* Clock. */
    st_format_tz(ctx->settings->wifi.timezone_id, name, sizeof(name), offset, sizeof(offset));
    if (ui_skin_is_instrument()) {
        kit_label_set_text_if_changed(st->wifi_tz_value, name);
        kit_label_set_text_if_changed(st->wifi_tz_sub, offset);
        kit_show(st->wifi_tz_sub, offset[0] != '\0');
    } else if (ui_skin_is_orbit()) {
        if (offset[0] != '\0') {
            snprintf(text, sizeof(text), "%s (%s)", name, offset);
        } else {
            snprintf(text, sizeof(text), "%s", name);
        }
        kit_label_set_text_if_changed(st->wifi_tz_sub, text);
    } else {
        kit_label_set_text_if_changed(st->wifi_tz_title, name);
        if (offset[0] != '\0') {
            snprintf(text, sizeof(text), "%s · %s", ui_l10n_text(ctx, L10N_TIME_ZONE), offset);
        } else {
            snprintf(text, sizeof(text), "%s", ui_l10n_text(ctx, L10N_TIME_ZONE));
        }
        kit_label_set_text_if_changed(st->wifi_tz_sub, text);
    }
    kit_switch_set(st->wifi_auto_sw, !manual);
    if (st->wifi_auto_sub != NULL) {
        kit_label_set_text_if_changed(st->wifi_auto_sub, manual ? ui_l10n_text(ctx, L10N_MANUAL_TIME) : "pool.ntp.org");
    }
    st_format_synced(ctx, text, sizeof(text), ui_skin_is_orbit() ? &s_synced : &s_last_synced);
    kit_label_set_text_if_changed(st->wifi_sync_sub, text);
    kit_show(st->wifi_manual_row, manual);
    kit_show(lv_obj_get_parent(st->wifi_forget_btn), saved);
}

/* --- Join network ---------------------------------------------------------------------- */

static void join_close_cb(lv_event_t *e)
{
    st_join_close(lv_event_get_user_data(e));
}

static void join_connect_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);
    clock_ui_settings_state_t *st = ST(ctx);
    const char *password;

    if (!st->join_open || st->join_ssid[0] == '\0') {
        return;
    }
    password = lv_textarea_get_text(st->join_field);
    request_save_wifi_credentials(ctx, st->join_ssid, password != NULL ? password : "");
    st_join_close(ctx);
    st_wifi_sync(ctx, true);
}

static void join_eye_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);
    clock_ui_settings_state_t *st = ST(ctx);

    st->join_show_password = !st->join_show_password;
    lv_textarea_set_password_mode(st->join_field, !st->join_show_password);
    kit_icon_set(lv_obj_get_child(st->join_eye, 0), st->join_show_password ? KIT_ICON_EYE_OFF : KIT_ICON_EYE);
}

static void join_build(clock_ui_context_t *ctx)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_settings_state_t *st = ST(ctx);
    lv_obj_t *root;
    lv_obj_t *box;
    lv_obj_t *b;
    lv_obj_t *label;
    int32_t kb_y;
    int32_t kb_h;
    int32_t field_y;

    root = lv_obj_create(ui_chrome_root(ctx));
    lv_obj_remove_style_all(root);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(root, KIT_SCREEN, KIT_SCREEN);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(root, sk->bg, 0);
    lv_obj_add_flag(root, LV_OBJ_FLAG_HIDDEN);
    st->join_root = root;

    if (ui_skin_is_instrument()) {
        box = st_hbox(root, 8);
        lv_obj_align(box, LV_ALIGN_TOP_MID, 0, 40);
        kit_icon_small(box, KIT_ICON_LOCK, sk->label);
        st_label(ctx, box, KIT_TEXT_LABEL, ST_ID(L10N_JOIN_NETWORK));
        field_y = 150;
        kb_y = 240;
        kb_h = 4 * 64 + 18;
    } else if (ui_skin_is_orbit()) {
        field_y = 124;
        kb_y = 224;
        kb_h = 4 * 64 + 24;
    } else {
        label = st_label(ctx, root, KIT_TEXT_LABEL, ST_ID(L10N_JOIN_NETWORK));
        lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 50);
        field_y = 132;
        kb_y = 226;
        kb_h = 4 * 70 + 18;
    }

    st->join_title = kit_label(root, KIT_TEXT_TITLE, "");
    lv_obj_set_width(st->join_title, ui_skin_is_orbit() ? 380 : 440);
    lv_label_set_long_mode(st->join_title, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(st->join_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(st->join_title, LV_ALIGN_TOP_MID, 0, ui_skin_is_instrument() ? 68 : ui_skin_is_orbit() ? 56 : 80);

    if (ui_skin_is_instrument()) {
        st->join_meta = kit_label(root, KIT_TEXT_CAPTION, "");
        lv_obj_set_style_text_color(st->join_meta, sk->text_2, 0);
        lv_obj_align(st->join_meta, LV_ALIGN_TOP_MID, 0, 112);
    }

    st->join_field = lv_textarea_create(root);
    kit_style_field(st->join_field);
    lv_textarea_set_one_line(st->join_field, true);
    lv_textarea_set_password_mode(st->join_field, true);
    lv_textarea_set_max_length(st->join_field, 64);
    lv_obj_add_state(st->join_field, LV_STATE_FOCUSED);
    lv_obj_remove_flag(st->join_field, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    st_keep_gestures(st->join_field);

    if (ui_skin_is_instrument()) {
        lv_obj_set_width(st->join_field, 364);
        lv_obj_set_pos(st->join_field, 136, field_y);
        st->join_eye = kit_round_button(root, KIT_BUTTON_SECONDARY, KIT_ICON_EYE, 72);
        lv_obj_set_style_radius(st->join_eye, sk->radius_s, 0);
        lv_obj_set_pos(st->join_eye, 512, field_y);
    } else {
        int32_t w = ui_skin_is_orbit() ? 520 : 420;
        int32_t h;

        lv_obj_set_width(st->join_field, w);
        lv_obj_update_layout(st->join_field);
        h = lv_obj_get_height(st->join_field);
        lv_obj_set_pos(st->join_field, (KIT_SCREEN - w) / 2, field_y);
        lv_obj_set_style_pad_right(st->join_field, 80, 0);
        if (ui_skin_is_atelier()) {
            lv_obj_t *lock = kit_icon_small(root, KIT_ICON_LOCK, sk->text_3);

            lv_obj_set_style_pad_left(st->join_field, 60, 0);
            lv_obj_set_pos(lock, (KIT_SCREEN - w) / 2 + 26, field_y + (h - 22) / 2);
        }
        st->join_eye = kit_round_button(root, KIT_BUTTON_QUIET, KIT_ICON_EYE, 56);
        lv_obj_set_style_bg_opa(st->join_eye, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(st->join_eye, ui_skin_is_orbit() ? sk->well : sk->raised, 0);
        lv_obj_set_style_border_width(st->join_eye, 0, 0);
        lv_obj_set_style_image_recolor(lv_obj_get_child(st->join_eye, 0), ui_skin_is_orbit() ? sk->text : sk->value, 0);
        lv_obj_set_pos(st->join_eye, (KIT_SCREEN + w) / 2 - 56 - (h - 56) / 2, field_y + (h - 56) / 2);
    }
    lv_obj_add_event_cb(st->join_eye, join_eye_cb, LV_EVENT_CLICKED, ctx);

    st->join_keyboard = lv_keyboard_create(root);
    kit_style_keyboard(st->join_keyboard);
    lv_obj_set_size(st->join_keyboard, JOIN_KB_W, kb_h);
    lv_obj_set_pos(st->join_keyboard, (KIT_SCREEN - JOIN_KB_W) / 2, kb_y);
    lv_keyboard_set_textarea(st->join_keyboard, st->join_field);
    lv_obj_add_event_cb(st->join_keyboard, join_connect_cb, LV_EVENT_READY, ctx);
    st_keep_gestures(st->join_keyboard);

    if (ui_skin_is_orbit()) {
        static const int32_t xs[2] = {232, 392};

        for (int i = 0; i < 2; i++) {
            b = kit_round_button(root, i == 0 ? KIT_BUTTON_SECONDARY : KIT_BUTTON_PRIMARY,
                                 i == 0 ? KIT_ICON_CLOSE : KIT_ICON_NONE, 96);
            lv_obj_set_pos(b, xs[i], kb_y + kb_h + 14);
            if (i == 1) {
                lv_obj_center(st_image(b, &ui_settings_arrow_36, sk->accent_text));
            }
            lv_obj_add_event_cb(b, i == 0 ? join_close_cb : join_connect_cb, LV_EVENT_CLICKED, ctx);
            label = st_label(ctx, root, KIT_TEXT_LABEL, ST_ID(i == 0 ? L10N_CANCEL : L10N_CONNECT));
            lv_obj_set_width(label, 150);
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_pos(label, xs[i] + 48 - 75, kb_y + kb_h + 14 + 100);
            if (i == 1) {
                lv_obj_set_style_text_color(label, sk->text, 0);
            }
        }
    } else {
        int32_t w = ui_skin_is_instrument() ? 216 : 180;

        box = st_hbox(root, 16);
        lv_obj_align(box, LV_ALIGN_TOP_MID, 0, kb_y + kb_h + (ui_skin_is_instrument() ? 16 : 22));
        b = kit_button(box, KIT_BUTTON_SECONDARY, KIT_ICON_NONE, "");
        st_bind(ctx, button_label(b), ST_ID(L10N_CANCEL));
        lv_obj_set_size(b, w, 72);
        lv_obj_add_event_cb(b, join_close_cb, LV_EVENT_CLICKED, ctx);
        b = kit_button(box, KIT_BUTTON_PRIMARY, KIT_ICON_NONE, "");
        st_bind(ctx, button_label(b), ST_ID(L10N_CONNECT));
        lv_obj_set_size(b, w, 72);
        lv_obj_add_event_cb(b, join_connect_cb, LV_EVENT_CLICKED, ctx);
    }
    lv_obj_remove_flag(root, LV_OBJ_FLAG_GESTURE_BUBBLE);
}

void st_join_open(clock_ui_context_t *ctx, const char *ssid, int rssi, int authmode)
{
    clock_ui_settings_state_t *st = ST(ctx);
    char text[64];

    if (st->join_root == NULL) {
        join_build(ctx);
    }
    snprintf(st->join_ssid, sizeof(st->join_ssid), "%s", ssid != NULL ? ssid : "");
    kit_label_set_text(st->join_title, st->join_ssid);
    if (st->join_meta != NULL) {
        snprintf(text, sizeof(text), "%s · \xE2\x88\x92%d dBm", auth_name(ctx, authmode), -rssi);
        kit_label_set_text(st->join_meta, text);
    }
    lv_textarea_set_placeholder_text(st->join_field, ui_l10n_text(ctx, L10N_PASSWORD));
    lv_textarea_set_text(st->join_field, "");
    st->join_show_password = false;
    lv_textarea_set_password_mode(st->join_field, true);
    kit_icon_set(lv_obj_get_child(st->join_eye, 0), KIT_ICON_EYE);
    lv_keyboard_set_mode(st->join_keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    st->join_open = true;
    kit_show(st->join_root, true);
    lv_obj_move_foreground(st->join_root);
}

void st_join_close(clock_ui_context_t *ctx)
{
    clock_ui_settings_state_t *st = ST(ctx);

    st->join_open = false;
    st->join_ssid[0] = '\0';
    if (st->join_field != NULL) {
        lv_textarea_set_text(st->join_field, "");
    }
    kit_show(st->join_root, false);
}

/* --- Time zone ------------------------------------------------------------------------- */

static void timezone_pick_cb(lv_event_t *e)
{
    ui_st_item_t *item = lv_event_get_user_data(e);
    clock_ui_context_t *ctx = item->ui;

    request_set_timezone(ctx, timezone_id_from_picker_index(item->index));
    st_timezone_sync(ctx);
    st_back(ctx);
}

void st_timezone_build(clock_ui_context_t *ctx)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_settings_state_t *st = ST(ctx);
    uint8_t count = LV_MIN(timezone_picker_count(), (uint8_t)UI_ST_TZ_MAX);
    kit_page_t *p;
    lv_obj_t *group;

    p = st_page_begin(ctx, UI_ST_TIMEZONE, ui_skin_is_orbit() ? ST_NONE : ST_ID(L10N_WIFI), ST_ID(L10N_TIME_ZONE),
                      KIT_NAV_BACK);
    group = kit_group(p->content);
    for (uint8_t i = 0; i < count; i++) {
        uint8_t id = timezone_id_from_picker_index(i);
        char name[40];
        char offset[24];
        kit_row_t row;
        kit_row_opts_t o = {0};

        st_format_tz(id, name, sizeof(name), offset, sizeof(offset));
        st->tz_item[i].ui = ctx;
        st->tz_item[i].index = i;
        o.title = name;
        o.trail = KIT_TRAIL_CUSTOM;
        o.on_click = timezone_pick_cb;
        o.user_data = &st->tz_item[i];
        kit_row_create(&row, group, &o);
        if (offset[0] != '\0') {
            lv_obj_t *value = kit_label(row.trail, KIT_TEXT_BODY, offset);

            lv_obj_set_style_text_color(value, ui_skin_is_orbit() ? sk->text_2 : sk->value, 0);
        }
        st->tz_check[i] = kit_icon_small(row.trail, KIT_ICON_CHECK, sk->accent);
        st->tz_row[i] = row.row;
    }
}

void st_timezone_sync(clock_ui_context_t *ctx)
{
    clock_ui_settings_state_t *st = ST(ctx);
    uint8_t count = LV_MIN(timezone_picker_count(), (uint8_t)UI_ST_TZ_MAX);
    uint8_t selected = timezone_picker_index_from_id(ctx->settings->wifi.timezone_id);

    if (!st->built[UI_ST_TIMEZONE]) {
        return;
    }
    for (uint8_t i = 0; i < count; i++) {
        lv_obj_t *check = st->tz_check[i];

        if (check == NULL) {
            continue;
        }
        /* Keep the space so the offsets stay aligned. */
        lv_obj_set_style_image_opa(check, i == selected ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }
}

void st_timezone_open(clock_ui_context_t *ctx)
{
    clock_ui_settings_state_t *st = ST(ctx);
    uint8_t selected;

    st_show_page(ctx, UI_ST_TIMEZONE);
    selected = timezone_picker_index_from_id(ctx->settings->wifi.timezone_id);
    if (selected < UI_ST_TZ_MAX && st->tz_row[selected] != NULL && selected > 2) {
        kit_page_t *p = st_page(ctx, UI_ST_TIMEZONE);
        lv_area_t a;
        lv_area_t s;

        lv_obj_update_layout(p->scroll);
        lv_obj_get_coords(st->tz_row[selected], &a);
        lv_obj_get_coords(p->scroll, &s);
        lv_obj_scroll_by_bounded(p->scroll, 0, -(a.y1 - s.y1 - 300), LV_ANIM_OFF);
    }
}

/* --- Manual time ----------------------------------------------------------------------- */

static uint8_t days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t s_days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (month == 2 && ((year % 4U == 0U && year % 100U != 0U) || year % 400U == 0U)) {
        return 29;
    }
    if (month < 1 || month > 12) {
        return 31;
    }
    return s_days[month - 1U];
}

static void manual_apply_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);
    clock_ui_settings_state_t *st = ST(ctx);
    uint16_t year = (uint16_t)(2024U + lv_roller_get_selected(st->man_year));
    uint8_t month = (uint8_t)(1U + lv_roller_get_selected(st->man_month));
    uint8_t day = (uint8_t)(1U + lv_roller_get_selected(st->man_day));
    uint8_t max_day = days_in_month(year, month);

    if (day > max_day) {
        day = max_day;
    }
    request_set_manual_time(ctx, year, month, day, (uint8_t)lv_roller_get_selected(st->man_hour),
                            (uint8_t)lv_roller_get_selected(st->man_minute));
    st_back(ctx);
}

static lv_obj_t *manual_roller(lv_obj_t *parent, const char *options, int32_t width)
{
    lv_obj_t *roller = kit_roller(parent, options, 3, ui_skin()->font_time_m);

    lv_obj_set_width(roller, width);
    st_keep_gestures(roller);
    return roller;
}

void st_manual_build(clock_ui_context_t *ctx)
{
    static const char *s_months = "01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12";
    const ui_skin_t *sk = ui_skin();
    clock_ui_settings_state_t *st = ST(ctx);
    kit_page_t *p;
    lv_obj_t *row;
    lv_obj_t *b;
    int32_t w2 = ui_skin_is_orbit() ? 96 : 90;

    p = st_page_begin(ctx, UI_ST_MANUAL_TIME, ui_skin_is_orbit() ? ST_NONE : ST_ID(L10N_WIFI), ST_STR(&s_date_time),
                      KIT_NAV_BACK);
    st_bind(ctx, kit_section_label(p->content, ""), ST_STR(&s_date));
    row = st_hbox(p->content, 4);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    st->man_day = manual_roller(row, ctx->day_options, w2);
    kit_label(row, KIT_TEXT_TIME_M, ".");
    st->man_month = manual_roller(row, s_months, w2);
    kit_label(row, KIT_TEXT_TIME_M, ".");
    st->man_year = manual_roller(row, ctx->year_options, w2 * 2 - 10);

    st_bind(ctx, kit_section_label(p->content, ""), ST_STR(&s_time));
    row = st_hbox(p->content, 4);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    st->man_hour = manual_roller(row, ctx->hour_options, w2);
    kit_label(row, KIT_TEXT_TIME_M, ":");
    st->man_minute = manual_roller(row, ctx->minute_options, w2);

    row = st_hbox(p->content, 0);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(row, sk->gap_m, 0);
    b = kit_button(row, KIT_BUTTON_PRIMARY, KIT_ICON_NONE, "");
    st_bind(ctx, button_label(b), ST_ID(L10N_SET_TIME));
    lv_obj_set_style_min_width(b, 240, 0);
    kit_set_tint(b, sk->tint_wifi);
    lv_obj_add_event_cb(b, manual_apply_cb, LV_EVENT_CLICKED, ctx);
}

void st_manual_sync(clock_ui_context_t *ctx, bool force)
{
    clock_ui_settings_state_t *st = ST(ctx);
    time_t now;
    struct tm tm;
    int year;

    if (!force || !st->built[UI_ST_MANUAL_TIME]) {
        return;
    }
    now = time(NULL);
    localtime_r(&now, &tm);
    year = LV_CLAMP(2024, tm.tm_year + 1900, 2035);
    lv_roller_set_selected(st->man_year, (uint32_t)(year - 2024), LV_ANIM_OFF);
    lv_roller_set_selected(st->man_month, (uint32_t)tm.tm_mon, LV_ANIM_OFF);
    lv_roller_set_selected(st->man_day, (uint32_t)(tm.tm_mday - 1), LV_ANIM_OFF);
    lv_roller_set_selected(st->man_hour, (uint32_t)tm.tm_hour, LV_ANIM_OFF);
    lv_roller_set_selected(st->man_minute, (uint32_t)tm.tm_min, LV_ANIM_OFF);
}
