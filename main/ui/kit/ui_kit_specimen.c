/*
 * Kit specimen: every kit part on a few pages, for review in the simulator.
 *
 *   call kit_specimen      settings home (rows with icons, chip, divider)
 *   call kit_specimen_2    list page (sections, every row trail, joined group)
 *   call kit_specimen_3    controls (switches, sliders, segmented, day keys, chips)
 *   call kit_specimen_4    buttons and round keys
 *   call kit_specimen_5    type roles and the icon set
 *   call kit_specimen_6    roller, field and keyboard
 *   call kit_specimen_7    dialog over a page
 *   call kit_specimen_close
 *
 * The pages are built once per style under the chrome root and are deleted with it.
 */
#include <stdio.h>

#include "ui/kit/ui_kit_internal.h"
#include "ui/ui_areas.h"

#define SPEC_PAGES 7

static lv_obj_t *s_holder;
static kit_page_t s_pages[SPEC_PAGES];

static void close_cb(lv_event_t *e)
{
    (void)e;
    if (s_holder != NULL) {
        lv_obj_add_flag(s_holder, LV_OBJ_FLAG_HIDDEN);
    }
}

static const char *page_eyebrow(int index)
{
    static char buf[8][24];

    snprintf(buf[index], sizeof(buf[index]), "Kit %d / %d", index + 1, SPEC_PAGES);
    return buf[index];
}

static lv_obj_t *hrow(lv_obj_t *parent, int32_t gap)
{
    lv_obj_t *r = kit_plain(parent);

    lv_obj_set_size(r, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(r, gap, 0);
    lv_obj_set_style_pad_row(r, gap, 0);
    lv_obj_set_style_pad_ver(r, 10, 0);
    return r;
}

/* 1: settings home. */
static void build_home(kit_page_t *p)
{
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *group;
    lv_obj_t *r;
    kit_row_t row;
    kit_row_opts_t o = {0};

    r = hrow(p->content, 0);
    lv_obj_set_style_pad_top(r, 0, 0);
    lv_obj_set_style_pad_bottom(r, ui_skin_is_orbit() ? 8 : 24, 0);
    kit_chip(r, KIT_ICON_NONE, "Wi-Fi  Home-5G · synced 14:13", true);

    group = kit_group(p->content);
    o.trail = KIT_TRAIL_CHEVRON;
    o.on_click = close_cb;
    o.icon = KIT_ICON_WIFI;
    o.tint = sk->tint_wifi;
    o.title = "Wi-Fi";
    o.subtitle = "Networks, clock, and time zone";
    kit_row_create(&row, group, &o);
    o.icon = KIT_ICON_MOON;
    o.tint = sk->tint_night;
    o.title = "Night mode";
    o.subtitle = "Schedule, face, and brightness";
    kit_row_create(&row, group, &o);
    o.icon = KIT_ICON_SETTINGS;
    o.tint = sk->tint_other;
    o.title = "Other";
    o.subtitle = "Language, sound, and device info";
    kit_row_create(&row, group, &o);

    kit_spacer(p->content, 16);
    kit_divider(p->content);
    r = hrow(p->content, 12);
    kit_icon_small(r, KIT_ICON_WIFI, sk->accent);
    kit_label(r, KIT_TEXT_BODY, "Home-5G · synced 14:13");
}

/* 2: every row trail, sections and a joined group. */
static void build_list(kit_page_t *p)
{
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *group;
    kit_row_t row;
    kit_row_opts_t o = {0};

    kit_section_label(p->content, "Clock");
    group = kit_group(p->content);
    o.title = "Time zone";
    o.trail = KIT_TRAIL_VALUE;
    o.value = "Warsaw, Berlin";
    o.value_chevron = true;
    o.on_click = close_cb;
    kit_row_create(&row, group, &o);
    o.on_click = NULL;
    o.title = "Auto sync";
    o.subtitle = "pool.ntp.org";
    o.trail = KIT_TRAIL_SWITCH;
    o.tint = sk->tint_wifi;
    kit_row_create(&row, group, &o);
    kit_switch_set(row.control, true);
    o.title = "Sync time";
    o.subtitle = "Last 14:13 today";
    o.trail = KIT_TRAIL_CUSTOM;
    kit_row_create(&row, group, &o);
    kit_button(row.trail, KIT_BUTTON_QUIET, KIT_ICON_SYNC, "Sync");

    kit_section_label(p->content, "Networks");
    group = kit_group(p->content);
    o = (kit_row_opts_t) {0};
    o.trail = KIT_TRAIL_CUSTOM;
    o.title = "Home-5G";
    o.subtitle = "Connected";
    o.icon = KIT_ICON_WIFI;
    o.tint = sk->tint_wifi;
    kit_row_create(&row, group, &o);
    lv_obj_set_style_text_color(row.subtitle, sk->accent, 0);
    kit_icon_small(row.trail, KIT_ICON_CHECK, sk->accent);
    o.icon = KIT_ICON_NONE;
    o.subtitle = NULL;
    o.title = "Café Lumière";
    kit_row_create(&row, group, &o);
    kit_icon_small(row.trail, KIT_ICON_LOCK, sk->text_3);
    kit_icon_small(row.trail, KIT_ICON_WIFI_2, sk->text);
    o.title = "iPhone (Anna)";
    kit_row_create(&row, group, &o);
    kit_icon_small(row.trail, KIT_ICON_LOCK, sk->text_3);
    kit_icon_small(row.trail, KIT_ICON_WIFI_1, sk->text);
    o.title = "Language";
    o.trail = KIT_TRAIL_VALUE;
    o.value = "English";
    o.value_chevron = false;
    o.icon = KIT_ICON_GLOBE;
    o.tint = sk->tint_other;
    kit_row_create(&row, group, &o);

    kit_section_label(p->content, "About this device");
    group = kit_group_joined(p->content);
    o = (kit_row_opts_t) {0};
    o.trail = KIT_TRAIL_VALUE;
    o.title = "Device ID";
    o.value = "TC-4F2A91";
    kit_row_create(&row, group, &o);
    o.title = "Version";
    o.value = "1.8.2";
    kit_row_create(&row, group, &o);
    o.title = "Target";
    o.value = "ESP32-P4";
    kit_row_create(&row, group, &o);
}

/* 3: controls. */
static void build_controls(kit_page_t *p)
{
    const ui_skin_t *sk = ui_skin();
    static const char *const repeat[] = {"Once", "Every day", "Weekdays", "Weekends"};
    static const char *const days[] = {"M", "T", "W", "T", "F", "S", "S"};
    lv_obj_t *r;
    lv_obj_t *sl;
    lv_obj_t *sw;

    kit_section_label(p->content, "Switches");
    r = hrow(p->content, 24);
    sw = kit_switch(r, sk->tint_wifi);
    kit_switch_set(sw, true);
    kit_switch(r, sk->tint_wifi);
    sw = kit_switch(r, sk->tint_night);
    kit_switch_set(sw, true);
    sw = kit_switch(r, sk->tint_other);
    kit_switch_set(sw, true);

    kit_section_label(p->content, "Sliders");
    r = hrow(p->content, 14);
    kit_icon_small(r, KIT_ICON_BRIGHTNESS, sk->text_2);
    sl = kit_slider(r, 0, 100, sk->accent);
    lv_obj_set_width(sl, 300);
    kit_slider_set(sl, 40);
    kit_label(r, KIT_TEXT_VALUE, "40%");
    r = hrow(p->content, 14);
    sl = kit_slider(r, 0, 100, sk->tint_night);
    lv_obj_set_width(sl, lv_pct(100));
    kit_slider_set(sl, 12);

    kit_section_label(p->content, "Segmented");
    kit_set_tint(kit_segmented(p->content, repeat, 4, 2), sk->tint_alarms);
    kit_spacer(p->content, 16);
    r = hrow(p->content, ui_skin_is_atelier() ? 0 : 8);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    for (int i = 0; i < 7; i++) {
        kit_set_tint(kit_day_toggle(r, days[i], i < 5), sk->tint_alarms);
    }

    kit_section_label(p->content, "Chips");
    r = hrow(p->content, 12);
    kit_chip(r, KIT_ICON_NONE, "Update 1.9", true);
    kit_chip(r, KIT_ICON_ALARM, "06:30 Tomorrow", false);
    kit_chip(r, KIT_ICON_NONE, "Synced", false);
}

/* 4: buttons. */
static void build_buttons(kit_page_t *p)
{
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *r;
    lv_obj_t *b;

    kit_section_label(p->content, "Buttons");
    r = hrow(p->content, 16);
    b = kit_button(r, KIT_BUTTON_PRIMARY, KIT_ICON_NONE, "Save");
    lv_obj_set_width(b, 168);
    b = kit_button(r, KIT_BUTTON_SECONDARY, KIT_ICON_NONE, "Cancel");
    lv_obj_set_width(b, 168);
    r = hrow(p->content, 16);
    kit_button(r, KIT_BUTTON_QUIET, KIT_ICON_PLAY, "Test");
    kit_button(r, KIT_BUTTON_DESTRUCTIVE, KIT_ICON_TRASH, "Delete");
    r = hrow(p->content, 16);
    b = kit_button(r, KIT_BUTTON_PRIMARY, KIT_ICON_DOWNLOAD, "Install 1.9.0");
    b = kit_button(r, KIT_BUTTON_QUIET, KIT_ICON_PLAY, "Test");
    kit_button_set_tint(b, sk->tint_alarms);

    kit_section_label(p->content, "Round keys");
    r = hrow(p->content, 20);
    b = kit_round_button(r, KIT_BUTTON_PRIMARY, KIT_ICON_PLUS, 88);
    kit_button_set_tint(b, sk->tint_alarms);
    kit_round_button(r, KIT_BUTTON_SECONDARY, KIT_ICON_ALARM, 72);
    kit_round_button(r, KIT_BUTTON_QUIET, KIT_ICON_BACK, 0);
    kit_round_button(r, KIT_BUTTON_DESTRUCTIVE, KIT_ICON_TRASH, 0);
    r = hrow(p->content, 20);
    kit_round_button(r, KIT_BUTTON_SECONDARY, KIT_ICON_MINUS, 72);
    kit_label(r, KIT_TEXT_TIME_M, "9");
    kit_label(r, KIT_TEXT_LABEL, "min");
    kit_round_button(r, KIT_BUTTON_SECONDARY, KIT_ICON_PLUS, 72);
}

/* 5: type roles and icons. */
static void build_type(kit_page_t *p)
{
    const ui_skin_t *sk = ui_skin();
    lv_obj_t *r;

    kit_label(p->content, KIT_TEXT_TIME_L, "06:30");
    kit_label(p->content, KIT_TEXT_TIME_M, "22:30 40%");
    kit_label(p->content, KIT_TEXT_TITLE, "Face themes · Нічний");
    kit_label(p->content, KIT_TEXT_VALUE, "22:30 → 07:00 · Ночь");
    kit_label(p->content, KIT_TEXT_HEADING, "Café Lumière · Źródło");
    kit_label(p->content, KIT_TEXT_BODY, "Networks, clock · Język · Мережі");
    kit_label(p->content, KIT_TEXT_BUTTON, "Connect · Zapisz");
    kit_label(p->content, KIT_TEXT_LABEL, "Next up · Яскравість · żółć");
    kit_label(p->content, KIT_TEXT_CAPTION, "TC-4F2A91 · −52 dBm");

    kit_section_label(p->content, "Icons");
    r = hrow(p->content, 20);
    lv_obj_set_style_pad_hor(r, 24, 0);
    for (int i = KIT_ICON_NONE + 1; i < KIT_ICON_COUNT; i++) {
        kit_icon(r, (kit_icon_t)i, sk->icon_fg);
    }
    r = hrow(p->content, 16);
    lv_obj_set_style_pad_hor(r, 24, 0);
    for (int i = KIT_ICON_NONE + 1; i < KIT_ICON_COUNT; i++) {
        kit_icon_small(r, (kit_icon_t)i, sk->text_2);
    }
}

/* 6: roller, field, keyboard. */
static void build_editor(kit_page_t *p)
{
    lv_obj_t *r;
    lv_obj_t *roller;
    lv_obj_t *ta;
    lv_obj_t *kb;
    lv_obj_t *eye;

    r = hrow(p->content, 8);
    roller = kit_roller(r, "04\n05\n06\n07\n08", 3, ui_skin()->font_time_l);
    lv_obj_set_width(roller, 128);
    lv_roller_set_selected(roller, 2, LV_ANIM_OFF);
    kit_label(r, KIT_TEXT_TIME_L, ":");
    roller = kit_roller(r, "28\n29\n30\n31\n32", 3, ui_skin()->font_time_l);
    lv_obj_set_width(roller, 128);
    lv_roller_set_selected(roller, 2, LV_ANIM_OFF);

    kit_section_label(p->content, "Password");
    r = hrow(p->content, 12);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    ta = lv_textarea_create(r);
    kit_style_field(ta);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_password_mode(ta, true);
    lv_textarea_set_text(ta, "lumiere2026");
    lv_obj_set_flex_grow(ta, 1);
    lv_obj_add_state(ta, LV_STATE_FOCUSED);
    eye = kit_round_button(r, KIT_BUTTON_SECONDARY, KIT_ICON_EYE, 72);
    (void)eye;

    kb = lv_keyboard_create(p->content);
    kit_style_keyboard(kb);
    lv_obj_set_size(kb, lv_pct(100), 4 * 64 + 3 * 6);
    lv_keyboard_set_textarea(kb, ta);
}

/* 7: dialog over the list page. */
static void build_dialog(kit_page_t *p)
{
    lv_obj_t *card;
    lv_obj_t *r;
    lv_obj_t *ta;
    lv_obj_t *b;

    build_list(p);
    card = kit_dialog(p->root, 528);
    kit_label(card, KIT_TEXT_LABEL, "Join network");
    kit_label(card, KIT_TEXT_TITLE, "Café Lumière");
    kit_label(card, KIT_TEXT_CAPTION, "WPA2 · 2.4 GHz · −71 dBm");
    r = hrow(card, 12);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    ta = lv_textarea_create(r);
    kit_style_field(ta);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_password_mode(ta, true);
    lv_textarea_set_text(ta, "lumiere2026");
    lv_obj_set_flex_grow(ta, 1);
    kit_round_button(r, KIT_BUTTON_SECONDARY, KIT_ICON_EYE, 72);
    r = hrow(card, 16);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    b = kit_button(r, KIT_BUTTON_SECONDARY, KIT_ICON_NONE, "Cancel");
    lv_obj_set_flex_grow(b, 1);
    b = kit_button(r, KIT_BUTTON_PRIMARY, KIT_ICON_NONE, "Connect");
    lv_obj_set_flex_grow(b, 1);
}

static void holder_deleted_cb(lv_event_t *e)
{
    (void)e;
    s_holder = NULL;
}

static void build(clock_ui_context_t *ctx)
{
    static const char *const titles[SPEC_PAGES] = {"Settings", "Wi-Fi", "Controls", "Buttons", "Type", "Editor",
                                                   "Dialog"};

    if (s_holder != NULL) {
        return;
    }
    s_holder = kit_plain(ui_chrome_root(ctx));
    lv_obj_add_event_cb(s_holder, holder_deleted_cb, LV_EVENT_DELETE, NULL);
    lv_obj_set_size(s_holder, KIT_SCREEN, KIT_SCREEN);
    for (int i = 0; i < SPEC_PAGES; i++) {
        kit_page_t *p = &s_pages[i];

        kit_page_create(p, s_holder, i == 0 ? "14:18 · Tue 27 Oct" : page_eyebrow(i), titles[i],
                        i == 0 ? KIT_NAV_CLOSE : KIT_NAV_BACK, close_cb, NULL);
        switch (i) {
        case 0:
            build_home(p);
            break;
        case 1:
            build_list(p);
            kit_page_set_action(p, KIT_ICON_SYNC, NULL, NULL);
            break;
        case 2:
            build_controls(p);
            break;
        case 3:
            build_buttons(p);
            break;
        case 4:
            build_type(p);
            break;
        case 5:
            build_editor(p);
            break;
        default:
            build_dialog(p);
            break;
        }
    }
}

static void show(clock_ui_context_t *ctx, int index)
{
    build(ctx);
    lv_obj_remove_flag(s_holder, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_holder);
    for (int i = 0; i < SPEC_PAGES; i++) {
        if (i == index) {
            lv_obj_remove_flag(s_pages[i].root, LV_OBJ_FLAG_HIDDEN);
            lv_obj_scroll_to_y(s_pages[i].scroll, 0, LV_ANIM_OFF);
        } else {
            lv_obj_add_flag(s_pages[i].root, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void show_2(clock_ui_context_t *ctx) { show(ctx, 1); }
static void show_3(clock_ui_context_t *ctx) { show(ctx, 2); }
static void show_4(clock_ui_context_t *ctx) { show(ctx, 3); }
static void show_5(clock_ui_context_t *ctx) { show(ctx, 4); }
static void show_6(clock_ui_context_t *ctx) { show(ctx, 5); }
static void show_7(clock_ui_context_t *ctx) { show(ctx, 6); }

static void close_all(clock_ui_context_t *ctx)
{
    (void)ctx;
    close_cb(NULL);
}

/* Scrolls the visible specimen page by 300 px (for screenshots of long pages). */
static void scroll_down(clock_ui_context_t *ctx)
{
    (void)ctx;
    for (int i = 0; i < SPEC_PAGES; i++) {
        if (s_holder != NULL && !lv_obj_has_flag(s_pages[i].root, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_scroll_by_bounded(s_pages[i].scroll, 0, -300, LV_ANIM_OFF);
        }
    }
}

void ui_kit_specimen_show(struct clock_ui_state_t *ctx)
{
    ui_debug_register("kit_specimen_2", show_2);
    ui_debug_register("kit_specimen_3", show_3);
    ui_debug_register("kit_specimen_4", show_4);
    ui_debug_register("kit_specimen_5", show_5);
    ui_debug_register("kit_specimen_6", show_6);
    ui_debug_register("kit_specimen_7", show_7);
    ui_debug_register("kit_specimen_scroll", scroll_down);
    ui_debug_register("kit_specimen_close", close_all);
    show(ctx, 0);
}
