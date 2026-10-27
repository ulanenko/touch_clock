/*
 * Settings › Other: language, interface style, touch sound and its volume, software update
 * and About this device; the language and interface style pickers.
 */
#include "ui/ui_settings_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "app_version.h"
#include "assets/ui_assets/settings_assets.h"
#include "esp_system.h"
#include "sdkconfig.h"
#include "telemetry_update.h"

static const ui_l10n_str_t s_interface_style = UI_L10N_STR("Interface style", "Styl interfejsu", "Стиль інтерфейсу",
                                                           "Стиль интерфейса", "Interfacestijl");
static const ui_l10n_str_t s_style_desc[CLOCK_UI_STYLE_COUNT] = {
    [CLOCK_UI_STYLE_INSTRUMENT] = UI_L10N_STR("Graphite keys, one orange signal", "Grafit i jeden pomarańczowy akcent",
                                              "Графіт і один помаранчевий акцент", "Графит и один оранжевый акцент",
                                              "Grafiet met één oranje signaal"),
    [CLOCK_UI_STYLE_ORBIT] = UI_L10N_STR("Round, bright and colourful", "Okrągły, jasny i kolorowy",
                                         "Круглий, яскравий і кольоровий", "Круглый, яркий и цветной",
                                         "Rond, helder en kleurrijk"),
    [CLOCK_UI_STYLE_ATELIER] = UI_L10N_STR("Ink and gilt, like a fine watch", "Atrament i złoto jak w zegarku",
                                           "Чорнило й позолота, як годинник", "Чернила и позолота, как часы",
                                           "Inkt en goud, als een fijn horloge"),
};
static const ui_l10n_str_t s_available = UI_L10N_STR("%s available", "Dostępna %s", "Доступна %s", "Доступна %s",
                                                     "%s beschikbaar");
static const ui_l10n_str_t s_installed = UI_L10N_STR("Installed %s", "Zainstalowana %s", "Встановлено %s",
                                                     "Установлена %s", "Geïnstalleerd %s");
static const ui_l10n_str_t s_available_have = UI_L10N_STR("%s available · you have %s", "Dostępna %s · masz %s",
                                                          "Доступна %s · у вас %s", "Доступна %s · у вас %s",
                                                          "%s beschikbaar · u heeft %s");
static const ui_l10n_str_t s_install_v = UI_L10N_STR("Install %s", "Zainstaluj %s", "Встановити %s", "Установить %s",
                                                     "%s installeren");
static const ui_l10n_str_t s_restarting = UI_L10N_STR("Restarting…", "Ponowne uruchamianie…", "Перезапуск…",
                                                      "Перезапуск…", "Herstarten…");
static const ui_l10n_str_t s_needs_wifi = UI_L10N_STR("Connect to Wi-Fi to check", "Połącz z Wi-Fi, aby sprawdzić",
                                                      "Підключіть Wi-Fi для перевірки", "Подключите Wi-Fi для проверки",
                                                      "Verbind met wifi om te controleren");
static const ui_l10n_str_t s_progress = UI_L10N_STR("%s: %u%%", "%s: %u%%", "%s: %u%%", "%s: %u%%", "%s: %u%%");

/* --- Callbacks ------------------------------------------------------------------------- */

static void language_row_cb(lv_event_t *e)
{
    st_show_page(lv_event_get_user_data(e), UI_ST_LANGUAGE);
}

static void style_row_cb(lv_event_t *e)
{
    st_show_page(lv_event_get_user_data(e), UI_ST_STYLE);
}

static void sound_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);

    if (ctx->suppress_events) {
        return;
    }
    request_set_ui_click_sound_enabled(ctx, lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED));
    st_other_sync(ctx, false);
}

static void volume_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);
    int32_t v = lv_slider_get_value(lv_event_get_target(e));
    char text[16];

    if (ctx->suppress_events) {
        return;
    }
    snprintf(text, sizeof(text), "%d%%", (int)v);
    kit_label_set_text_if_changed(ST(ctx)->other_vol_value, text);
    request_set_ui_click_volume(ctx, (uint8_t)v);
}

static void update_cb(lv_event_t *e)
{
    clock_ui_context_t *ctx = lv_event_get_user_data(e);
    const app_runtime_state_t *rt = ctx->runtime;

    if (rt->ota_busy || rt->ota_reboot_pending) {
        return;
    }
    if (rt->ota_update_available) {
        request_ota_install(ctx);
    } else {
        request_ota_check(ctx);
    }
    st_other_sync(ctx, false);
}

/* --- Page ------------------------------------------------------------------------------ */

static lv_obj_t *volume_line(clock_ui_context_t *ctx, lv_obj_t *parent)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_settings_state_t *st = ST(ctx);
    lv_obj_t *line = st_hbox(parent, st_instrument() ? 14 : 16);

    lv_obj_set_width(line, lv_pct(100));
    if (st_instrument()) {
        lv_obj_set_height(line, 80);
        lv_obj_set_style_border_side(line, LV_BORDER_SIDE_TOP, 0);
        lv_obj_set_style_border_width(line, 1, 0);
        lv_obj_set_style_border_color(line, sk->line, 0);
        st->other_vol_icons[0] = kit_icon_small(line, KIT_ICON_SOUND, sk->text_3);
    } else if (st_orbit()) {
        lv_obj_set_height(line, 72);
        lv_obj_set_style_pad_hor(line, 30, 0);
        lv_obj_set_style_pad_bottom(line, 16, 0);
        st->other_vol_icons[0] = kit_icon_small(line, KIT_ICON_SOUND_OFF, sk->text_2);
    } else {
        lv_obj_set_height(line, 56);
        st->other_vol_icons[0] = kit_icon_small(line, KIT_ICON_SOUND_OFF, sk->text_2);
    }
    st->other_vol_slider = kit_slider(line, 0, 100, st_orbit() ? sk->accent : sk->tint_sound);
    lv_obj_set_flex_grow(st->other_vol_slider, 1);
    st_keep_gestures(st->other_vol_slider);
    lv_obj_add_event_cb(st->other_vol_slider, volume_cb, LV_EVENT_VALUE_CHANGED, ctx);
    if (!st_instrument()) {
        st->other_vol_icons[1] = kit_icon_small(line, KIT_ICON_SOUND, st_orbit() ? sk->text_2 : sk->value);
    }
    if (!st_orbit()) {
        st->other_vol_value = kit_label(line, st_atelier() ? KIT_TEXT_VALUE : KIT_TEXT_BODY, "");
        lv_obj_set_style_text_color(st->other_vol_value, st_atelier() ? sk->value : sk->text, 0);
        lv_obj_set_style_min_width(st->other_vol_value, 60, 0);
        lv_obj_set_style_text_align(st->other_vol_value, LV_TEXT_ALIGN_RIGHT, 0);
    }
    return line;
}

static void build_update(clock_ui_context_t *ctx, kit_page_t *p)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_settings_state_t *st = ST(ctx);
    lv_obj_t *card = kit_card(p->content);
    lv_obj_t *col;
    lv_obj_t *line;
    lv_obj_t *label;

    if (st_orbit()) {
        kit_row_t row;
        kit_row_opts_t o = {0};
        lv_obj_t *group;

        lv_obj_set_style_pad_all(card, 0, 0);
        lv_obj_set_style_pad_bottom(card, 24, 0);
        lv_obj_set_style_margin_bottom(card, 16, 0);
        group = st_vbox(card, 0);
        lv_obj_set_width(group, lv_pct(100));
        o.icon = KIT_ICON_DOWNLOAD;
        o.tint = sk->tint_wifi;
        o.subtitle = " ";
        st_row(ctx, &row, group, &o, ST_ID(L10N_SOFTWARE_UPDATE), ST_NONE);
        lv_obj_set_style_bg_opa(row.row, LV_OPA_TRANSP, 0);
        st->upd_sub = row.subtitle;
        st->upd_btn = kit_button(card, KIT_BUTTON_PRIMARY, KIT_ICON_NONE, "");
        lv_obj_set_width(st->upd_btn, lv_pct(100));
        lv_obj_set_style_margin_left(st->upd_btn, 24, 0);
        lv_obj_set_style_margin_right(st->upd_btn, 24, 0);
        lv_obj_set_width(st->upd_btn, sk->content_w - 48);
    } else {
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(card, 16, 0);
        lv_obj_set_style_pad_row(card, 12, 0);
        lv_obj_set_style_pad_ver(card, st_instrument() ? 20 : 18, 0);
        lv_obj_set_style_pad_right(card, 18, 0);
        if (st_atelier()) {
            lv_obj_set_style_margin_top(card, 12, 0);
        }
        col = st_vbox(card, 2);
        lv_obj_set_flex_grow(col, 1);
        label = st_label(ctx, col, KIT_TEXT_LABEL, ST_ID(L10N_SOFTWARE_UPDATE));
        if (st_atelier()) {
            lv_obj_set_style_text_color(label, sk->accent, 0);
            line = st_hbox(col, 10);
            lv_obj_set_style_margin_top(line, 8, 0);
            st->upd_old = kit_label(line, KIT_TEXT_VALUE, "");
            lv_obj_set_style_text_color(st->upd_old, sk->text_2, 0);
            st->upd_arrow = st_image(line, &ui_settings_arrow_24, sk->accent);
            st->upd_head = kit_label(line, KIT_TEXT_VALUE, "");
            st->upd_sub = kit_label(col, KIT_TEXT_BODY, "");
        } else {
            st->upd_head = kit_label(col, KIT_TEXT_VALUE, "");
            lv_obj_set_style_text_color(st->upd_head, sk->text, 0);
            st->upd_sub = kit_label(col, KIT_TEXT_CAPTION, "");
        }
        lv_obj_set_width(st->upd_sub, lv_pct(100));
        lv_label_set_long_mode(st->upd_sub, LV_LABEL_LONG_MODE_WRAP);
        st->upd_btn = kit_button(card, KIT_BUTTON_PRIMARY, st_instrument() ? KIT_ICON_DOWNLOAD : KIT_ICON_NONE, "");
        lv_obj_set_height(st->upd_btn, 72);
    }
    lv_obj_add_event_cb(st->upd_btn, update_cb, LV_EVENT_CLICKED, ctx);

    /* Progress while installing. */
    st->upd_bar = lv_obj_create(card);
    lv_obj_remove_style_all(st->upd_bar);
    lv_obj_remove_flag(st->upd_bar, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(st->upd_bar, st_orbit() ? sk->content_w - 48 : lv_pct(100), 4);
    lv_obj_set_style_radius(st->upd_bar, 2, 0);
    lv_obj_set_style_bg_opa(st->upd_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(st->upd_bar, sk->line_strong, 0);
    if (st_orbit()) {
        lv_obj_set_style_margin_left(st->upd_bar, 24, 0);
        lv_obj_set_style_margin_top(st->upd_bar, 16, 0);
    }
    st->upd_bar_fill = lv_obj_create(st->upd_bar);
    lv_obj_remove_style_all(st->upd_bar_fill);
    lv_obj_remove_flag(st->upd_bar_fill, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(st->upd_bar_fill, 0, 4);
    lv_obj_set_style_radius(st->upd_bar_fill, 2, 0);
    lv_obj_set_style_bg_opa(st->upd_bar_fill, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(st->upd_bar_fill, st_orbit() ? sk->tint_wifi : sk->accent, 0);
    lv_obj_add_flag(st->upd_bar, LV_OBJ_FLAG_HIDDEN);
}

static void format_target(char *out, size_t size)
{
    const char *t = CONFIG_IDF_TARGET;
    size_t n = 0;

    for (size_t i = 0; t[i] != '\0' && n + 2 < size; i++) {
        if (i == 5 && strncmp(t, "esp32", 5) == 0) {
            out[n++] = '-';
        }
        out[n++] = (char)toupper((unsigned char)t[i]);
    }
    out[n] = '\0';
}

static void build_about(clock_ui_context_t *ctx, kit_page_t *p)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_settings_state_t *st = ST(ctx);
    static const l10n_text_id_t keys[5] = {L10N_DEVICE_ID, L10N_VERSION, L10N_BUILD, L10N_TARGET, L10N_ESP_IDF};
    char values[5][40];
    lv_obj_t *label;
    lv_obj_t *box;

    telemetry_update_get_device_id(values[0], sizeof(values[0]));
    snprintf(values[1], sizeof(values[1]), "%s", TOUCH_CLOCK_FIRMWARE_VERSION);
    snprintf(values[2], sizeof(values[2]), "%s", __DATE__);
    format_target(values[3], sizeof(values[3]));
    snprintf(values[4], sizeof(values[4]), "%s", esp_get_idf_version());

    label = kit_section_label(p->content, "");
    st_bind(ctx, label, ST_ID(L10N_ABOUT_DEVICE));
    st->about_anchor = lv_obj_get_parent(label);
    if (st_instrument()) {
        kit_icon_small(lv_obj_get_parent(label), KIT_ICON_INFO, sk->text_3);
        /* A rating plate: hairline box, mono legend left, value right. */
        box = st_vbox(p->content, 0);
        lv_obj_set_width(box, lv_pct(100));
        lv_obj_set_style_border_width(box, 1, 0);
        lv_obj_set_style_border_color(box, sk->line_strong, 0);
        lv_obj_set_style_radius(box, sk->radius_s, 0);
        for (int i = 0; i < 5; i++) {
            lv_obj_t *line = st_hbox(box, 12);

            lv_obj_set_size(line, lv_pct(100), 52);
            lv_obj_set_style_pad_hor(line, 20, 0);
            lv_obj_set_flex_align(line, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            if (i > 0) {
                lv_obj_set_style_border_side(line, LV_BORDER_SIDE_TOP, 0);
                lv_obj_set_style_border_width(line, 1, 0);
                lv_obj_set_style_border_color(line, sk->line, 0);
            }
            st_label(ctx, line, KIT_TEXT_LABEL, ST_ID(keys[i]));
            label = kit_label(line, KIT_TEXT_CAPTION, values[i]);
            lv_obj_set_style_text_color(label, sk->text, 0);
        }
    } else if (st_orbit()) {
        box = kit_group_joined(p->content);
        for (int i = 0; i < 5; i++) {
            kit_row_t row;
            kit_row_opts_t o = {0};

            o.trail = KIT_TRAIL_VALUE;
            o.value = values[i];
            st_row(ctx, &row, box, &o, ST_ID(keys[i]), ST_NONE);
            lv_obj_set_style_min_height(lv_obj_get_parent(row.title), 40, 0);
            lv_obj_set_style_text_font(row.title, sk->font_body, 0);
            lv_obj_set_style_text_color(row.title, sk->text_2, 0);
            lv_obj_set_style_text_font(row.value, sk->font_value, 0);
            lv_obj_set_style_text_color(row.value, sk->text, 0);
        }
    } else {
        /* A caseback engraving on a central axis. */
        box = st_vbox(p->content, 0);
        lv_obj_set_width(box, lv_pct(100));
        lv_obj_set_style_pad_top(box, 4, 0);
        for (int i = 0; i < 5; i++) {
            lv_obj_t *line = st_hbox(box, 0);
            lv_obj_t *mid;

            lv_obj_set_size(line, lv_pct(100), 30);
            label = st_label(ctx, line, KIT_TEXT_LABEL, ST_ID(keys[i]));
            lv_obj_set_width(label, sk->content_w / 2 - 10);
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
            lv_obj_set_style_text_color(label, sk->accent_deep, 0);
            lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
            mid = st_hbox(line, 0);
            lv_obj_set_size(mid, 20, 30);
            lv_obj_set_flex_align(mid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            st_dot(mid, 3, sk->accent_deep);
            label = kit_label(line, KIT_TEXT_BODY, values[i]);
            lv_obj_set_style_text_color(label, sk->value, 0);
        }
    }
}

void st_other_build(clock_ui_context_t *ctx)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_settings_state_t *st = ST(ctx);
    kit_page_t *p;
    lv_obj_t *group;
    kit_row_t row;
    kit_row_opts_t o = {0};

    p = st_page_begin(ctx, UI_ST_OTHER, st_atelier() ? ST_ID(L10N_SETTINGS) : ST_NONE, ST_ID(L10N_OTHER),
                      KIT_NAV_BACK);
    if (st_instrument()) {
        kit_page_set_eyebrow(p, " ");
    }
    group = kit_group(p->content);

    o.trail = KIT_TRAIL_VALUE;
    o.value = " ";
    o.value_chevron = true;
    o.on_click = language_row_cb;
    o.user_data = ctx;
    if (st_orbit()) {
        o.icon = KIT_ICON_GLOBE;
        o.tint = sk->tint_other;
    }
    st_row(ctx, &row, group, &o, ST_ID(L10N_LANGUAGE), ST_NONE);
    st->other_lang_value = row.value;
    o.on_click = style_row_cb;
    if (st_orbit()) {
        o.icon = KIT_ICON_STYLE;
    }
    st_row(ctx, &row, group, &o, ST_STR(&s_interface_style), ST_NONE);
    st->other_style_value = row.value;

    /* Touch sound and its volume. */
    if (st_orbit()) {
        group = kit_group_joined(p->content);
    }
    o = (kit_row_opts_t) {0};
    o.trail = KIT_TRAIL_SWITCH;
    o.tint = sk->tint_other;
    if (st_orbit()) {
        o.icon = KIT_ICON_SOUND;
    }
    st_row(ctx, &row, group, &o, ST_ID(L10N_TOUCH_SOUND), ST_NONE);
    st->other_sound_sw = row.control;
    st_keep_gestures(row.control);
    lv_obj_add_event_cb(row.control, sound_cb, LV_EVENT_VALUE_CHANGED, ctx);
    volume_line(ctx, group);
    if (st_instrument()) {
        kit_spacer(p->content, 16);
    } else if (st_atelier()) {
        lv_obj_set_style_border_width(lv_obj_get_parent(st->other_vol_slider), 0, 0);
    }

    build_update(ctx, p);
    build_about(ctx, p);
}

lv_obj_t *st_other_about_anchor(clock_ui_context_t *ctx)
{
    return ST(ctx)->about_anchor;
}

void st_other_sync(clock_ui_context_t *ctx, bool force)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_settings_state_t *st = ST(ctx);
    const app_runtime_state_t *rt = ctx->runtime;
    bool sound = ctx->settings->ui_click_sound_enabled;
    bool available = rt->ota_update_available && rt->ota_available_version[0] != '\0';
    bool can_act = rt->ota_configured && rt->wifi_connected && !rt->ota_busy && !rt->ota_reboot_pending;
    char text[96];
    const char *button;

    LV_UNUSED(force);
    if (!st->built[UI_ST_OTHER]) {
        return;
    }
    kit_label_set_text_if_changed(st->other_lang_value,
                                  l10n_language_name((clock_language_t)ctx->settings->language));
    kit_label_set_text_if_changed(st->other_style_value, ui_skin_style_name(ui_skin_style()));

    kit_switch_set(st->other_sound_sw, sound);
    if (!lv_slider_is_dragged(st->other_vol_slider)) {
        kit_slider_set(st->other_vol_slider, ctx->settings->ui_click_volume);
    }
    if (lv_obj_has_state(st->other_vol_slider, LV_STATE_DISABLED) == sound) {
        st_set_enabled(st->other_vol_slider, sound);
        kit_set_tint(st->other_vol_slider, sound ? (st_orbit() ? lv_color_white() : sk->tint_sound) : sk->text_3);
    }
    snprintf(text, sizeof(text), "%u%%", ctx->settings->ui_click_volume);
    kit_label_set_text_if_changed(st->other_vol_value, text);

    /* Software update. */
    if (st_atelier()) {
        if (available && !rt->ota_busy) {
            kit_label_set_text_if_changed(st->upd_old, TOUCH_CLOCK_FIRMWARE_VERSION);
            kit_label_set_text_if_changed(st->upd_head, rt->ota_available_version);
        } else {
            kit_label_set_text_if_changed(st->upd_head, TOUCH_CLOCK_FIRMWARE_VERSION);
        }
        st_show(st->upd_old, available && !rt->ota_busy);
        st_show(st->upd_arrow, available && !rt->ota_busy);
    } else if (st->upd_head != NULL) {
        if (rt->ota_busy) {
            snprintf(text, sizeof(text), "%s", ui_l10n_text(ctx, L10N_UPDATING));
        } else if (rt->ota_reboot_pending) {
            snprintf(text, sizeof(text), "%s", ui_l10n_str(ctx, &s_restarting));
        } else if (available) {
            snprintf(text, sizeof(text), ui_l10n_str(ctx, &s_available), rt->ota_available_version);
        } else {
            snprintf(text, sizeof(text), ui_l10n_str(ctx, &s_installed), TOUCH_CLOCK_FIRMWARE_VERSION);
        }
        kit_label_set_text_if_changed(st->upd_head, text);
    }

    if (rt->ota_busy) {
        snprintf(text, sizeof(text), ui_l10n_str(ctx, &s_progress), ui_l10n_text(ctx, L10N_PROGRESS),
                 (unsigned)rt->ota_progress);
    } else if (rt->ota_reboot_pending) {
        snprintf(text, sizeof(text), "%s", ui_l10n_str(ctx, &s_restarting));
    } else if (available) {
        if (st_orbit()) {
            snprintf(text, sizeof(text), ui_l10n_str(ctx, &s_available_have), rt->ota_available_version,
                     TOUCH_CLOCK_FIRMWARE_VERSION);
        } else if (st_instrument()) {
            snprintf(text, sizeof(text), ui_l10n_str(ctx, &s_installed), TOUCH_CLOCK_FIRMWARE_VERSION);
        } else {
            text[0] = '\0';
        }
    } else if (!rt->ota_configured) {
        snprintf(text, sizeof(text), "%s", ui_l10n_text(ctx, L10N_MANIFEST_NOT_CONFIGURED));
    } else if (!rt->wifi_connected) {
        snprintf(text, sizeof(text), "%s", ui_l10n_str(ctx, &s_needs_wifi));
    } else if (rt->ota_status[0] != '\0') {
        snprintf(text, sizeof(text), "%s", l10n_runtime_status(ctx->settings->language, rt->ota_status));
    } else if (st_orbit()) {
        snprintf(text, sizeof(text), ui_l10n_str(ctx, &s_installed), TOUCH_CLOCK_FIRMWARE_VERSION);
    } else {
        text[0] = '\0';
    }
    kit_label_set_text_if_changed(st->upd_sub, text);
    st_show(st->upd_sub, text[0] != '\0');

    if (available) {
        if (st_orbit()) {
            snprintf(text, sizeof(text), ui_l10n_str(ctx, &s_install_v), rt->ota_available_version);
            button = text;
        } else {
            button = ui_l10n_text(ctx, L10N_INSTALL);
        }
    } else {
        button = ui_l10n_text(ctx, L10N_CHECK);
    }
    kit_button_set_text(st->upd_btn, button);
    if (st_instrument()) {
        kit_icon_set(lv_obj_get_child(st->upd_btn, 0), available ? KIT_ICON_DOWNLOAD : KIT_ICON_SYNC);
    }
    /* The action only shows when it can run; the line above says why otherwise. */
    st_show(st->upd_btn, can_act);
    st_show(st->upd_bar, rt->ota_busy);
    if (rt->ota_busy) {
        int32_t w = lv_obj_get_width(st->upd_bar) * LV_MIN(rt->ota_progress, 100) / 100;

        if (lv_obj_get_width(st->upd_bar_fill) != w) {
            lv_obj_set_width(st->upd_bar_fill, w);
        }
    }
}

/* --- Language -------------------------------------------------------------------------- */

static void language_pick_cb(lv_event_t *e)
{
    ui_st_item_t *item = lv_event_get_user_data(e);
    clock_ui_context_t *ctx = item->ui;

    if (item->index != ctx->settings->language) {
        request_set_language(ctx, (clock_language_t)item->index);
        ui_settings_refresh(ctx);
    }
    st_back(ctx);
}

void st_language_build(clock_ui_context_t *ctx)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_settings_state_t *st = ST(ctx);
    kit_page_t *p;
    lv_obj_t *group;

    p = st_page_begin(ctx, UI_ST_LANGUAGE, st_orbit() ? ST_NONE : ST_ID(L10N_OTHER), ST_ID(L10N_LANGUAGE),
                      KIT_NAV_BACK);
    group = kit_group(p->content);
    for (int i = 0; i < CLOCK_LANGUAGE_COUNT; i++) {
        kit_row_t row;
        kit_row_opts_t o = {0};

        st->lang_item[i].ui = ctx;
        st->lang_item[i].index = (uint8_t)i;
        o.title = l10n_language_name((clock_language_t)i);
        o.trail = KIT_TRAIL_CUSTOM;
        o.on_click = language_pick_cb;
        o.user_data = &st->lang_item[i];
        kit_row_create(&row, group, &o);
        st->lang_check[i] = kit_icon_small(row.trail, KIT_ICON_CHECK, sk->accent);
    }
}

void st_language_sync(clock_ui_context_t *ctx)
{
    clock_ui_settings_state_t *st = ST(ctx);

    if (!st->built[UI_ST_LANGUAGE]) {
        return;
    }
    for (int i = 0; i < CLOCK_LANGUAGE_COUNT; i++) {
        st_show(st->lang_check[i], i == ctx->settings->language);
    }
}

/* --- Interface style ------------------------------------------------------------------- */

static void style_pick_cb(lv_event_t *e)
{
    ui_st_item_t *item = lv_event_get_user_data(e);
    clock_ui_context_t *ctx = item->ui;

    if (item->index != ui_skin_style()) {
        /* The core rebuilds the chrome in the new style and reopens Other. */
        request_set_ui_style(ctx, (clock_ui_style_t)item->index);
    } else {
        st_back(ctx);
    }
}

void st_style_build(clock_ui_context_t *ctx)
{
    static const clock_ui_style_t order[CLOCK_UI_STYLE_COUNT] = {CLOCK_UI_STYLE_INSTRUMENT, CLOCK_UI_STYLE_ORBIT,
                                                                 CLOCK_UI_STYLE_ATELIER};
    static const lv_image_dsc_t *const previews[CLOCK_UI_STYLE_COUNT] = {
        [CLOCK_UI_STYLE_INSTRUMENT] = &ui_settings_style_instrument,
        [CLOCK_UI_STYLE_ORBIT] = &ui_settings_style_orbit,
        [CLOCK_UI_STYLE_ATELIER] = &ui_settings_style_atelier,
    };
    const ui_skin_t *sk = ui_skin();
    clock_ui_settings_state_t *st = ST(ctx);
    kit_page_t *p;
    lv_obj_t *group;

    p = st_page_begin(ctx, UI_ST_STYLE, st_orbit() ? ST_NONE : ST_ID(L10N_OTHER), ST_STR(&s_interface_style),
                      KIT_NAV_BACK);
    group = kit_group(p->content);
    for (int i = 0; i < CLOCK_UI_STYLE_COUNT; i++) {
        clock_ui_style_t style = order[i];
        kit_row_t row;
        kit_row_opts_t o = {0};
        lv_obj_t *holder;
        lv_obj_t *img;

        st->style_item[style].ui = ctx;
        st->style_item[style].index = (uint8_t)style;
        o.title = ui_skin_style_name(style);
        o.subtitle = " ";
        o.trail = KIT_TRAIL_CUSTOM;
        o.on_click = style_pick_cb;
        o.user_data = &st->style_item[style];
        kit_row_create(&row, group, &o);
        st_bind(ctx, row.subtitle, ST_STR(&s_style_desc[style]));
        lv_obj_set_style_pad_ver(row.row, 14, 0);
        if (st_orbit()) {
            lv_obj_set_style_radius(row.row, sk->radius_m, 0);
            lv_obj_set_style_pad_left(row.row, 14, 0);
        }
        /* The preview: the settings home of that style, in a hairline ring. */
        holder = lv_obj_create(row.row);
        lv_obj_remove_style_all(holder);
        lv_obj_remove_flag(holder, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(holder, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_size(holder, 104, 104);
        lv_obj_set_style_radius(holder, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(holder, 1, 0);
        lv_obj_set_style_border_color(holder, sk->line_strong, 0);
        lv_obj_move_to_index(holder, 0);
        img = lv_image_create(holder);
        lv_image_set_src(img, previews[style]);
        lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(img, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_center(img);
        st->style_check[style] = kit_icon_small(row.trail, KIT_ICON_CHECK, sk->accent);
    }
}

void st_style_sync(clock_ui_context_t *ctx)
{
    const ui_skin_t *sk = ui_skin();
    clock_ui_settings_state_t *st = ST(ctx);

    if (!st->built[UI_ST_STYLE]) {
        return;
    }
    for (int i = 0; i < CLOCK_UI_STYLE_COUNT; i++) {
        lv_obj_t *holder;

        if (st->style_check[i] == NULL) {
            continue;
        }
        st_show(st->style_check[i], i == (int)ui_skin_style());
        holder = lv_obj_get_child(lv_obj_get_parent(lv_obj_get_parent(st->style_check[i])), 0);
        lv_obj_set_style_border_width(holder, i == (int)ui_skin_style() ? 3 : 1, 0);
        lv_obj_set_style_border_color(holder, i == (int)ui_skin_style() ? sk->accent : sk->line_strong, 0);
    }
}
