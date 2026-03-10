static void sync_alarm_banner_style(clock_face_id_t face)
{
    bool dark_badge = (face == CLOCK_FACE_SLAVA);

    lv_obj_set_style_bg_color(s_ui.alarms.banner,
                              dark_badge ? lv_color_hex(0x101010) : lv_color_hex(0xF0E7D2),
                              0);
    lv_obj_set_style_bg_opa(s_ui.alarms.banner, dark_badge ? LV_OPA_90 : LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_ui.alarms.banner, 1, 0);
    lv_obj_set_style_border_color(s_ui.alarms.banner,
                                  dark_badge ? lv_color_hex(0x8F7230) : lv_color_hex(0xD4C2A1),
                                  0);
    lv_obj_set_style_border_opa(s_ui.alarms.banner, dark_badge ? LV_OPA_40 : LV_OPA_70, 0);
    lv_obj_set_style_shadow_width(s_ui.alarms.banner, dark_badge ? 28 : 22, 0);
    lv_obj_set_style_shadow_spread(s_ui.alarms.banner, dark_badge ? 3 : 1, 0);
    lv_obj_set_style_shadow_color(s_ui.alarms.banner,
                                  dark_badge ? lv_color_hex(0xC49B3A) : lv_color_hex(0xFFF3D4),
                                  0);
    lv_obj_set_style_shadow_opa(s_ui.alarms.banner, dark_badge ? LV_OPA_30 : LV_OPA_40, 0);
    lv_obj_set_style_shadow_offset_x(s_ui.alarms.banner, 0, 0);
    lv_obj_set_style_shadow_offset_y(s_ui.alarms.banner, 8, 0);
    lv_obj_set_style_text_color(s_ui.alarms.banner_label,
                                dark_badge ? lv_color_hex(0xE3C26A) : lv_color_hex(0x1E1A14),
                                0);
}

static void update_alarm_banner(time_t now)
{
    char text[96];

    if (s_ui.runtime->snooze_active && s_ui.runtime->snooze_deadline > now) {
        int minutes_left = (int)((s_ui.runtime->snooze_deadline - now + 59) / 60);

        if (minutes_left < 1) {
            minutes_left = 1;
        }
        snprintf(text, sizeof(text), "Waking up again in %d minute%s",
                 minutes_left,
                 (minutes_left == 1) ? "" : "s");
        lv_label_set_text(s_ui.alarms.banner_label, text);
        lv_obj_clear_flag(s_ui.alarms.banner, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (!s_ui.runtime->alarm_ringing &&
        s_ui.runtime->next_alarm_epoch > now &&
        (s_ui.runtime->next_alarm_epoch - now) <= 1800) {
        int minutes_left = (int)((s_ui.runtime->next_alarm_epoch - now + 59) / 60);

        if (minutes_left < 1) {
            minutes_left = 1;
        }
        snprintf(text, sizeof(text), "Waking you up in %d minute%s",
                 minutes_left,
                 (minutes_left == 1) ? "" : "s");
        lv_label_set_text(s_ui.alarms.banner_label, text);
        lv_obj_clear_flag(s_ui.alarms.banner, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_add_flag(s_ui.alarms.banner, LV_OBJ_FLAG_HIDDEN);
}

static void sync_alarm_overlay(time_t now)
{
    char subtitle[96] = "Alarm";

    if (s_ui.alarms.overlay == NULL) {
        return;
    }

    if (!s_ui.runtime->alarm_ringing) {
        lv_obj_add_flag(s_ui.alarms.overlay, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (s_ui.runtime->active_alarm_index >= 0 && s_ui.runtime->active_alarm_index < MAX_ALARMS) {
        const alarm_config_t *alarm = &s_ui.settings->alarms[s_ui.runtime->active_alarm_index];

        format_alarm_repeat_summary(subtitle, sizeof(subtitle), alarm);
    } else if (s_ui.runtime->snooze_active && s_ui.runtime->snooze_deadline > now) {
        struct tm snooze_tm;
        char time_text[24];

        localtime_r(&s_ui.runtime->snooze_deadline, &snooze_tm);
        format_alarm_time(time_text, sizeof(time_text), snooze_tm.tm_hour, snooze_tm.tm_min);
        snprintf(subtitle, sizeof(subtitle), "Snoozed until %s", time_text);
    }

    lv_label_set_text(s_ui.alarms.overlay_label, "Wake up");

    char snooze_text[32];
    snprintf(snooze_text, sizeof(snooze_text), "Snooze %u min", s_ui.settings->snooze_minutes);
    set_action_button_text(s_ui.alarms.snooze_btn, snooze_text);
    set_action_button_text(s_ui.alarms.stop_btn, "Off");
    lv_label_set_text(s_ui.alarms.overlay_subtitle, subtitle);
    lv_obj_clear_flag(s_ui.alarms.overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_ui.alarms.overlay);
}

static void sync_alarm_controls(void)
{
    char volume[32];
    char status[96];
    time_t now = 0;
    int enabled_count;

    if (s_ui.alarms.management_overlay == NULL) {
        return;
    }

    time(&now);

    if (s_ui.alarms.manage_snooze_btn != NULL) {
        char snooze_text[32];

        snprintf(snooze_text, sizeof(snooze_text), "Snooze %u min", s_ui.settings->snooze_minutes);
        set_action_button_text(s_ui.alarms.manage_snooze_btn, snooze_text);
    }

    if (s_ui.alarms.manage_volume_slider != NULL) {
        s_ui.suppress_events = true;
        lv_slider_set_value(s_ui.alarms.manage_volume_slider, s_ui.settings->alarm_volume, LV_ANIM_OFF);
        s_ui.suppress_events = false;
    }

    snprintf(volume, sizeof(volume), "%u%%", s_ui.settings->alarm_volume);
    if (s_ui.alarms.manage_volume_label != NULL) {
        lv_label_set_text(s_ui.alarms.manage_volume_label, volume);
    }
    if (s_ui.alarms.manage_test_btn != NULL) {
        set_action_button_text(s_ui.alarms.manage_test_btn,
                               s_ui.runtime->alarm_test_active ? "Stop test" : "Preview tone");
    }

    enabled_count = count_enabled_alarms();
    if (s_ui.runtime->alarm_ringing) {
        snprintf(status, sizeof(status), "Alarm is ringing now");
    } else if (s_ui.runtime->alarm_test_active) {
        snprintf(status, sizeof(status), "Previewing alarm sound");
    } else if (s_ui.runtime->snooze_active) {
        struct tm snooze_tm;
        char time_text[24];

        localtime_r(&s_ui.runtime->snooze_deadline, &snooze_tm);
        format_alarm_time(time_text, sizeof(time_text), snooze_tm.tm_hour, snooze_tm.tm_min);
        snprintf(status, sizeof(status), "Snoozed until %s", time_text);
    } else if (s_ui.runtime->next_alarm_epoch > 0) {
        struct tm next_tm;
        char time_text[24];

        localtime_r(&s_ui.runtime->next_alarm_epoch, &next_tm);
        format_alarm_time(time_text, sizeof(time_text), next_tm.tm_hour, next_tm.tm_min);
        snprintf(status, sizeof(status), "Next up %s %s", s_day_short[next_tm.tm_wday], time_text);
    } else if (enabled_count > 0) {
        snprintf(status, sizeof(status), "%d alarm%s active", enabled_count, (enabled_count == 1) ? "" : "s");
    } else {
        snprintf(status, sizeof(status), "No alarms scheduled");
    }

    if (s_ui.alarms.management_status != NULL) {
        lv_label_set_text(s_ui.alarms.management_status, status);
    }

    for (int i = 0; i < MAX_ALARMS; ++i) {
        const alarm_config_t *alarm = &s_ui.settings->alarms[i];
        char time_text[24];
        char meta[128];

        if (s_ui.alarms.list_time_label[i] == NULL) {
            continue;
        }

        format_alarm_time(time_text, sizeof(time_text), alarm->hour, alarm->minute);
        format_alarm_repeat_summary(meta, sizeof(meta), alarm);
        if (!alarm->enabled) {
            size_t meta_len = strlen(meta);

            if (meta_len < sizeof(meta)) {
                snprintf(meta + meta_len, sizeof(meta) - meta_len, "  Off");
            }
        }

        lv_label_set_text(s_ui.alarms.list_time_label[i], time_text);
        lv_label_set_text(s_ui.alarms.list_meta_label[i], meta);

        s_ui.suppress_events = true;
        if (alarm->enabled) {
            lv_obj_add_state(s_ui.alarms.list_toggle[i], LV_STATE_CHECKED);
        } else {
            lv_obj_remove_state(s_ui.alarms.list_toggle[i], LV_STATE_CHECKED);
        }
        s_ui.suppress_events = false;

        if (s_ui.runtime->next_alarm_index == i &&
            s_ui.runtime->next_alarm_epoch > now &&
            !s_ui.runtime->alarm_ringing) {
            lv_label_set_text(s_ui.alarms.list_badge_label[i], "Next");
            lv_obj_clear_flag(s_ui.alarms.list_badge[i], LV_OBJ_FLAG_HIDDEN);
        } else if (!alarm->enabled) {
            lv_label_set_text(s_ui.alarms.list_badge_label[i], "Off");
            lv_obj_clear_flag(s_ui.alarms.list_badge[i], LV_OBJ_FLAG_HIDDEN);
        } else if (alarm->repeat_mode == ALARM_REPEAT_ONCE) {
            lv_label_set_text(s_ui.alarms.list_badge_label[i], "Once");
            lv_obj_clear_flag(s_ui.alarms.list_badge[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_ui.alarms.list_badge[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (s_ui.alarms.focus_alarm_index >= 0 &&
        s_ui.alarms.focus_alarm_index < MAX_ALARMS &&
        s_ui.alarms.list_card[s_ui.alarms.focus_alarm_index] != NULL) {
        lv_obj_scroll_to_view_recursive(s_ui.alarms.list_card[s_ui.alarms.focus_alarm_index], LV_ANIM_ON);
        s_ui.alarms.focus_alarm_index = -1;
    }

    if (s_ui.alarms.editor_open && s_ui.alarms.editor_overlay != NULL) {
        char editor_time[24];
        char summary[64];
        uint8_t preset = alarm_repeat_preset_from_config(&s_ui.alarms.editor_draft);

        s_ui.suppress_events = true;
        lv_roller_set_selected(s_ui.alarms.editor_hour_roller, s_ui.alarms.editor_draft.hour, LV_ANIM_OFF);
        lv_roller_set_selected(s_ui.alarms.editor_minute_roller, s_ui.alarms.editor_draft.minute, LV_ANIM_OFF);
        if (s_ui.alarms.editor_draft.enabled) {
            lv_obj_add_state(s_ui.alarms.editor_enabled_sw, LV_STATE_CHECKED);
        } else {
            lv_obj_remove_state(s_ui.alarms.editor_enabled_sw, LV_STATE_CHECKED);
        }
        s_ui.suppress_events = false;

        for (int i = 0; i < 4; ++i) {
            if (preset == i) {
                lv_obj_add_state(s_ui.alarms.editor_repeat_btn[i], LV_STATE_CHECKED);
            } else {
                lv_obj_remove_state(s_ui.alarms.editor_repeat_btn[i], LV_STATE_CHECKED);
            }
        }

        for (int day = 0; day < 7; ++day) {
            if ((s_ui.alarms.editor_draft.days_mask & (1U << day)) != 0) {
                lv_obj_add_state(s_ui.alarms.editor_day_btn[day], LV_STATE_CHECKED);
            } else {
                lv_obj_remove_state(s_ui.alarms.editor_day_btn[day], LV_STATE_CHECKED);
            }
        }

        format_alarm_time(editor_time, sizeof(editor_time),
                          s_ui.alarms.editor_draft.hour,
                          s_ui.alarms.editor_draft.minute);
        format_alarm_repeat_summary(summary, sizeof(summary), &s_ui.alarms.editor_draft);

        lv_label_set_text(s_ui.alarms.editor_time_label, editor_time);
        lv_label_set_text(s_ui.alarms.editor_summary_label, summary);
        if (s_ui.alarms.editor_delete_btn != NULL) {
            if (s_ui.alarms.editor_is_new) {
                lv_obj_add_flag(s_ui.alarms.editor_delete_btn, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_clear_flag(s_ui.alarms.editor_delete_btn, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

static void alarm_snooze_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    if (s_ui.callbacks.on_alarm_snooze_requested != NULL) {
        s_ui.callbacks.on_alarm_snooze_requested(s_ui.user_ctx);
    }
}

static void alarm_stop_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    if (s_ui.callbacks.on_alarm_stop_requested != NULL) {
        s_ui.callbacks.on_alarm_stop_requested(s_ui.user_ctx);
    }
}

static void alarm_banner_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    alarm_management_open();
}

static void create_alarm_banner(void)
{
    s_ui.alarms.banner = lv_obj_create(s_ui.screen);
    lv_obj_set_size(s_ui.alarms.banner, 430, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(s_ui.alarms.banner, lv_color_hex(0x101010), 0);
    lv_obj_set_style_bg_opa(s_ui.alarms.banner, LV_OPA_90, 0);
    lv_obj_set_style_radius(s_ui.alarms.banner, 22, 0);
    lv_obj_set_style_pad_left(s_ui.alarms.banner, 22, 0);
    lv_obj_set_style_pad_right(s_ui.alarms.banner, 22, 0);
    lv_obj_set_style_pad_top(s_ui.alarms.banner, 13, 0);
    lv_obj_set_style_pad_bottom(s_ui.alarms.banner, 13, 0);
    lv_obj_align(s_ui.alarms.banner, LV_ALIGN_BOTTOM_MID, 0, -170);
    lv_obj_remove_flag(s_ui.alarms.banner, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_ui.alarms.banner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_ui.alarms.banner, alarm_banner_event_cb, LV_EVENT_CLICKED, NULL);

    s_ui.alarms.banner_label = lv_label_create(s_ui.alarms.banner);
    lv_obj_set_style_text_font(s_ui.alarms.banner_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_ui.alarms.banner_label, lv_color_hex(0xE3C26A), 0);
    lv_obj_set_style_text_letter_space(s_ui.alarms.banner_label, 1, 0);
    lv_obj_set_style_text_line_space(s_ui.alarms.banner_label, 3, 0);
    lv_obj_set_width(s_ui.alarms.banner_label, lv_pct(100));
    lv_obj_set_style_text_align(s_ui.alarms.banner_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_ui.alarms.banner_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(s_ui.alarms.banner_label, "");
}

static void create_alarm_overlay(void)
{
    lv_obj_t *actions;

    s_ui.alarms.overlay = lv_obj_create(s_ui.screen);
    lv_obj_set_size(s_ui.alarms.overlay, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_color(s_ui.alarms.overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_ui.alarms.overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_ui.alarms.overlay, 0, 0);
    lv_obj_set_style_radius(s_ui.alarms.overlay, 0, 0);
    lv_obj_set_style_pad_all(s_ui.alarms.overlay, 0, 0);
    lv_obj_set_scrollbar_mode(s_ui.alarms.overlay, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(s_ui.alarms.overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_ui.alarms.overlay, LV_OBJ_FLAG_HIDDEN);

    s_ui.alarms.overlay_label = lv_label_create(s_ui.alarms.overlay);
    lv_obj_set_style_text_font(s_ui.alarms.overlay_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_ui.alarms.overlay_label, lv_color_white(), 0);
    lv_obj_align(s_ui.alarms.overlay_label, LV_ALIGN_TOP_MID, 0, 150);
    lv_label_set_text(s_ui.alarms.overlay_label, "Wake up");

    s_ui.alarms.overlay_subtitle = lv_label_create(s_ui.alarms.overlay);
    lv_obj_set_style_text_font(s_ui.alarms.overlay_subtitle, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_ui.alarms.overlay_subtitle, lv_color_hex(0xB8B8B8), 0);
    lv_obj_align_to(s_ui.alarms.overlay_subtitle, s_ui.alarms.overlay_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 16);
    lv_label_set_text(s_ui.alarms.overlay_subtitle, "Alarm");

    actions = create_row(s_ui.alarms.overlay);
    lv_obj_set_style_pad_column(actions, 24, 0);
    lv_obj_set_flex_align(actions, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_width(actions, lv_pct(100));
    lv_obj_align(actions, LV_ALIGN_BOTTOM_MID, 0, -72);

    s_ui.alarms.snooze_btn = create_big_action_button(actions,
                                                      "Snooze 10 min",
                                                      lv_color_hex(0x535A6B),
                                                      alarm_snooze_event_cb,
                                                      NULL);
    s_ui.alarms.stop_btn = create_big_action_button(actions,
                                                    "Off",
                                                    lv_color_hex(0xB54B3D),
                                                    alarm_stop_event_cb,
                                                    NULL);
}

static lv_obj_t *create_filter_chip(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_t *label = lv_label_create(button);

    lv_obj_add_flag(button, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_height(button, 56);
    lv_obj_set_style_radius(button, 22, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x252525), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0xC8A248), LV_STATE_CHECKED);
    lv_obj_set_style_text_color(button, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(button, lv_color_black(), LV_STATE_CHECKED);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_pad_left(button, 20, 0);
    lv_obj_set_style_pad_right(button, 20, 0);
    lv_obj_set_style_pad_top(button, 14, 0);
    lv_obj_set_style_pad_bottom(button, 14, 0);
    lv_label_set_text(label, text);
    lv_obj_center(label);

    if (cb != NULL) {
        lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, user_data);
    }

    return button;
}

static void stop_event_bubble_cb(lv_event_t *event)
{
    lv_event_stop_bubbling(event);
}

static void alarm_manage_snooze_event_cb(lv_event_t *event)
{
    static const uint8_t options[] = {5, 10, 15, 20, 30};
    size_t current_index = 0;

    LV_UNUSED(event);

    for (size_t i = 0; i < sizeof(options); ++i) {
        if (options[i] == s_ui.settings->snooze_minutes) {
            current_index = i;
            break;
        }
    }

    s_ui.settings->snooze_minutes = options[(current_index + 1) % (sizeof(options) / sizeof(options[0]))];
    notify_settings_changed();
    sync_alarm_controls();
}

static void alarm_manage_volume_event_cb(lv_event_t *event)
{
    if (s_ui.suppress_events) {
        return;
    }

    s_ui.settings->alarm_volume = lv_slider_get_value(lv_event_get_target(event));
    notify_settings_changed();
    sync_alarm_controls();
}

static void alarm_manage_test_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);

    if (s_ui.callbacks.on_alarm_test_requested != NULL) {
        s_ui.callbacks.on_alarm_test_requested(s_ui.user_ctx);
    }
    sync_alarm_controls();
}

static void alarm_list_toggle_event_cb(lv_event_t *event)
{
    alarm_ctx_t *ctx = (alarm_ctx_t *)lv_event_get_user_data(event);

    if (s_ui.suppress_events || ctx == NULL || ctx->alarm_index >= MAX_ALARMS) {
        return;
    }

    s_ui.settings->alarms[ctx->alarm_index].enabled =
        lv_obj_has_state(lv_event_get_target(event), LV_STATE_CHECKED);
    notify_settings_changed();
    sync_alarm_controls();
}

static void alarm_list_card_event_cb(lv_event_t *event)
{
    alarm_ctx_t *ctx = (alarm_ctx_t *)lv_event_get_user_data(event);

    if (ctx == NULL || ctx->alarm_index >= MAX_ALARMS) {
        return;
    }

    open_alarm_editor(ctx->alarm_index, false);
}

static void alarm_editor_time_event_cb(lv_event_t *event)
{
    lv_obj_t *target = lv_event_get_target(event);

    if (s_ui.suppress_events) {
        return;
    }

    if (target == s_ui.alarms.editor_hour_roller) {
        s_ui.alarms.editor_draft.hour = lv_roller_get_selected(target);
    } else if (target == s_ui.alarms.editor_minute_roller) {
        s_ui.alarms.editor_draft.minute = lv_roller_get_selected(target);
    }

    sync_alarm_controls();
}

static void alarm_editor_enabled_event_cb(lv_event_t *event)
{
    if (s_ui.suppress_events) {
        return;
    }

    s_ui.alarms.editor_draft.enabled = lv_obj_has_state(lv_event_get_target(event), LV_STATE_CHECKED);
    sync_alarm_controls();
}

static void alarm_editor_repeat_event_cb(lv_event_t *event)
{
    uintptr_t preset = (uintptr_t)lv_event_get_user_data(event);

    apply_repeat_preset_to_alarm(&s_ui.alarms.editor_draft, (uint8_t)preset);
    sync_alarm_controls();
}

static void alarm_editor_day_event_cb(lv_event_t *event)
{
    uintptr_t day = (uintptr_t)lv_event_get_user_data(event);
    uint8_t mask;

    if (day >= 7) {
        return;
    }

    s_ui.alarms.editor_draft.repeat_mode = ALARM_REPEAT_WEEKLY;
    mask = (uint8_t)(1U << day);
    if ((s_ui.alarms.editor_draft.days_mask & mask) != 0) {
        s_ui.alarms.editor_draft.days_mask &= (uint8_t)~mask;
        if (s_ui.alarms.editor_draft.days_mask == 0) {
            s_ui.alarms.editor_draft.days_mask = mask;
        }
    } else {
        s_ui.alarms.editor_draft.days_mask |= mask;
    }

    sync_alarm_controls();
}

static void alarm_editor_save_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);

    if (s_ui.alarms.editor_index < 0 || s_ui.alarms.editor_index >= MAX_ALARMS) {
        return;
    }

    s_ui.settings->alarms[s_ui.alarms.editor_index] = s_ui.alarms.editor_draft;
    s_ui.alarms.focus_alarm_index = s_ui.alarms.editor_index;
    notify_settings_changed();
    alarm_editor_close();
    sync_alarm_controls();
}

static void alarm_editor_delete_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);

    if (s_ui.alarms.editor_index < 0 || s_ui.alarms.editor_index >= MAX_ALARMS) {
        return;
    }

    s_ui.settings->alarms[s_ui.alarms.editor_index].enabled = false;
    s_ui.settings->alarms[s_ui.alarms.editor_index].repeat_mode = ALARM_REPEAT_WEEKLY;
    s_ui.settings->alarms[s_ui.alarms.editor_index].days_mask = 0x7F;
    notify_settings_changed();
    alarm_editor_close();
    sync_alarm_controls();
}

static void alarm_editor_overlay_event_cb(lv_event_t *event)
{
    if (lv_event_get_target(event) == lv_event_get_current_target(event)) {
        alarm_editor_close();
    }
}

static void alarm_editor_close_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    alarm_editor_close();
}

static void close_alarm_surface(void)
{
    if (s_ui.alarms.editor_open) {
        alarm_editor_close();
    } else if (s_ui.alarms.open) {
        alarm_management_close();
    }
}

static void alarm_close_swipe_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_indev_t *indev = lv_event_get_indev(event);
    ui_surface_edge_t edge = (ui_surface_edge_t)(uintptr_t)lv_event_get_user_data(event);
    lv_point_t point;

    if ((!s_ui.alarms.open && !s_ui.alarms.editor_open) || indev == NULL) {
        return;
    }

    lv_indev_get_point(indev, &point);

    if (code == LV_EVENT_PRESSED) {
        s_ui.alarms.close_dragging = true;
        s_ui.alarms.close_drag_start_point = point;
        return;
    }

    if (!s_ui.alarms.close_dragging) {
        return;
    }

    if (code == LV_EVENT_PRESSING) {
        if (ui_surface_edge_swipe_trigger(edge,
                                          &s_ui.alarms.close_drag_start_point,
                                          &point,
                                          SETTINGS_CLOSE_SWIPE_TRIGGER)) {
            close_alarm_surface();
        }
        return;
    }

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        s_ui.alarms.close_dragging = false;
    }
}

static void style_alarm_roller(lv_obj_t *roller)
{
    lv_obj_set_style_bg_color(roller, lv_color_hex(0x101010), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(roller, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(roller, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(roller, 28, LV_PART_MAIN);
    lv_obj_set_style_text_color(roller, lv_color_hex(0xA39C8C), LV_PART_MAIN);
    lv_obj_set_style_text_font(roller, &lv_font_montserrat_36, LV_PART_MAIN);
    lv_obj_set_style_pad_top(roller, 18, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(roller, 18, LV_PART_MAIN);
    lv_obj_set_style_bg_color(roller, lv_color_hex(0xC8A248), LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(roller, LV_OPA_COVER, LV_PART_SELECTED);
    lv_obj_set_style_radius(roller, 20, LV_PART_SELECTED);
    lv_obj_set_style_text_color(roller, lv_color_black(), LV_PART_SELECTED);
    lv_obj_set_style_text_font(roller, &lv_font_montserrat_48, LV_PART_SELECTED);
    lv_obj_set_style_border_width(roller, 0, LV_PART_SELECTED);
}

static void alarm_custom_create_event_cb(lv_event_t *event)
{
    int slot;

    LV_UNUSED(event);

    slot = find_alarm_slot_for_new_alarm();
    if (slot < 0 || slot >= MAX_ALARMS) {
        return;
    }

    open_alarm_editor((uint8_t)slot, true);
}

static void alarm_management_open(void)
{
    brightness_overlay_hide_immediately();
    brightness_panel_hide();
    if (s_ui.settings_ui.open) {
        s_ui.settings_ui.open = false;
        lv_obj_add_flag(s_ui.settings_ui.overlay, LV_OBJ_FLAG_HIDDEN);
    }

    s_ui.alarms.open = true;
    sync_alarm_controls();
    lv_obj_clear_flag(s_ui.alarms.management_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_ui.alarms.management_overlay);
}

static void alarm_management_close(void)
{
    s_ui.alarms.open = false;
    lv_obj_add_flag(s_ui.alarms.management_overlay, LV_OBJ_FLAG_HIDDEN);
    if (!s_ui.alarms.editor_open) {
        show_affordances_temporarily();
    }
}

static void alarm_management_overlay_event_cb(lv_event_t *event)
{
    if (lv_event_get_target(event) == lv_event_get_current_target(event)) {
        alarm_management_close();
    }
}

static void alarm_management_close_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    alarm_management_close();
}

static void alarm_editor_close(void)
{
    s_ui.alarms.editor_open = false;
    lv_obj_add_flag(s_ui.alarms.editor_overlay, LV_OBJ_FLAG_HIDDEN);
    if (s_ui.alarms.open) {
        lv_obj_move_foreground(s_ui.alarms.management_overlay);
    } else {
        show_affordances_temporarily();
    }
}

static void open_alarm_editor(uint8_t alarm_index, bool is_new)
{
    time_t now;
    struct tm now_tm;

    if (alarm_index >= MAX_ALARMS) {
        return;
    }

    time(&now);
    localtime_r(&now, &now_tm);

    s_ui.alarms.editor_index = (int8_t)alarm_index;
    s_ui.alarms.editor_is_new = is_new;
    s_ui.alarms.editor_open = true;

    if (is_new) {
        int minute = ((now_tm.tm_min + 9) / 10) * 10;

        s_ui.alarms.editor_draft = s_ui.settings->alarms[alarm_index];
        s_ui.alarms.editor_draft.enabled = true;
        s_ui.alarms.editor_draft.repeat_mode = ALARM_REPEAT_WEEKLY;
        s_ui.alarms.editor_draft.days_mask = 0x7F;
        if (minute >= 60) {
            minute -= 60;
            now_tm.tm_hour = (now_tm.tm_hour + 1) % 24;
        }
        s_ui.alarms.editor_draft.hour = now_tm.tm_hour;
        s_ui.alarms.editor_draft.minute = (uint8_t)minute;
    } else {
        s_ui.alarms.editor_draft = s_ui.settings->alarms[alarm_index];
    }

    sync_alarm_controls();
    lv_obj_clear_flag(s_ui.alarms.editor_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_ui.alarms.editor_overlay);
}

static void create_alarm_management_overlay(void)
{
    ui_surface_t surface;
    lv_obj_t *header;
    lv_obj_t *title;
    lv_obj_t *content;
    lv_obj_t *card;
    lv_obj_t *row;
    lv_obj_t *label;
    lv_obj_t *custom_btn;

    ui_surface_create_fullscreen(&surface,
                                 s_ui.screen,
                                 lv_color_hex(0x070707),
                                 LV_OPA_COVER,
                                 lv_color_hex(0x0B0B0B),
                                 92,
                                 "Alarms",
                                 alarm_management_overlay_event_cb,
                                 alarm_management_close_event_cb);
    s_ui.alarms.management_overlay = surface.overlay;
    header = surface.header;
    title = surface.title;
    content = surface.content;

    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);

    s_ui.alarms.management_status = lv_label_create(header);
    lv_obj_set_width(s_ui.alarms.management_status, 420);
    lv_obj_set_style_text_color(s_ui.alarms.management_status, lv_color_hex(0xB1B1B1), 0);
    lv_obj_set_style_text_align(s_ui.alarms.management_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_ui.alarms.management_status, LV_LABEL_LONG_WRAP);
    lv_label_set_text(s_ui.alarms.management_status, "No alarms scheduled");
    lv_obj_align(s_ui.alarms.management_status, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_bottom(content, 28, 0);

    card = create_card(content);
    row = create_row(card);
    center_row(row);
    custom_btn = lv_button_create(row);
    lv_obj_set_size(custom_btn, 96, 96);
    lv_obj_set_style_radius(custom_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(custom_btn, lv_color_hex(0xC8A248), 0);
    lv_obj_set_style_bg_color(custom_btn, lv_color_hex(0xB69136), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(custom_btn, lv_color_black(), 0);
    lv_obj_set_style_border_width(custom_btn, 0, 0);
    lv_obj_add_event_cb(custom_btn, alarm_custom_create_event_cb, LV_EVENT_CLICKED, NULL);
    label = lv_label_create(custom_btn);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_48, 0);
    lv_label_set_text(label, LV_SYMBOL_PLUS);
    lv_obj_center(label);

    card = create_card(content);
    create_section_title(card, "Upcoming", NULL);
    s_ui.alarms.management_list = lv_obj_create(card);
    lv_obj_set_width(s_ui.alarms.management_list, lv_pct(100));
    lv_obj_set_style_bg_opa(s_ui.alarms.management_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ui.alarms.management_list, 0, 0);
    lv_obj_set_style_pad_all(s_ui.alarms.management_list, 0, 0);
    lv_obj_set_style_pad_row(s_ui.alarms.management_list, 14, 0);
    lv_obj_set_layout(s_ui.alarms.management_list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(s_ui.alarms.management_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(s_ui.alarms.management_list, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < MAX_ALARMS; ++i) {
        lv_obj_t *alarm_card;
        lv_obj_t *top_row;
        lv_obj_t *badge;

        s_ui.alarms.alarm_ctx[i].alarm_index = i;
        alarm_card = lv_obj_create(s_ui.alarms.management_list);
        s_ui.alarms.list_card[i] = alarm_card;
        lv_obj_set_width(alarm_card, lv_pct(100));
        lv_obj_set_height(alarm_card, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(alarm_card, lv_color_hex(0x171717), 0);
        lv_obj_set_style_border_width(alarm_card, 0, 0);
        lv_obj_set_style_radius(alarm_card, 30, 0);
        lv_obj_set_style_pad_all(alarm_card, 22, 0);
        lv_obj_set_style_pad_row(alarm_card, 8, 0);
        lv_obj_set_layout(alarm_card, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(alarm_card, LV_FLEX_FLOW_COLUMN);
        lv_obj_clear_flag(alarm_card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(alarm_card, alarm_list_card_event_cb, LV_EVENT_CLICKED, &s_ui.alarms.alarm_ctx[i]);

        top_row = create_row(alarm_card);
        lv_obj_set_flex_align(top_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        s_ui.alarms.list_time_label[i] = lv_label_create(top_row);
        lv_obj_set_style_text_font(s_ui.alarms.list_time_label[i], &lv_font_montserrat_36, 0);
        lv_obj_set_style_text_color(s_ui.alarms.list_time_label[i], lv_color_white(), 0);
        lv_label_set_text(s_ui.alarms.list_time_label[i], "7:00 AM");

        s_ui.alarms.list_toggle[i] = lv_switch_create(top_row);
        lv_obj_set_style_bg_color(s_ui.alarms.list_toggle[i], lv_color_hex(0x313131), LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_ui.alarms.list_toggle[i], lv_color_hex(0xC8A248), LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_add_event_cb(s_ui.alarms.list_toggle[i], stop_event_bubble_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(s_ui.alarms.list_toggle[i], alarm_list_toggle_event_cb, LV_EVENT_VALUE_CHANGED, &s_ui.alarms.alarm_ctx[i]);

        s_ui.alarms.list_meta_label[i] = lv_label_create(alarm_card);
        lv_obj_set_style_text_color(s_ui.alarms.list_meta_label[i], lv_color_hex(0xB7B7B7), 0);
        lv_obj_set_style_text_font(s_ui.alarms.list_meta_label[i], &lv_font_montserrat_20, 0);
        lv_label_set_text(s_ui.alarms.list_meta_label[i], "Weekdays");

        badge = lv_obj_create(alarm_card);
        s_ui.alarms.list_badge[i] = badge;
        lv_obj_set_size(badge, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(badge, lv_color_hex(0xC8A248), 0);
        lv_obj_set_style_text_color(badge, lv_color_black(), 0);
        lv_obj_set_style_border_width(badge, 0, 0);
        lv_obj_set_style_radius(badge, 16, 0);
        lv_obj_set_style_pad_left(badge, 12, 0);
        lv_obj_set_style_pad_right(badge, 12, 0);
        lv_obj_set_style_pad_top(badge, 6, 0);
        lv_obj_set_style_pad_bottom(badge, 6, 0);
        lv_obj_add_flag(badge, LV_OBJ_FLAG_HIDDEN);
        s_ui.alarms.list_badge_label[i] = lv_label_create(badge);
        lv_obj_set_style_text_font(s_ui.alarms.list_badge_label[i], &lv_font_montserrat_16, 0);
        lv_label_set_text(s_ui.alarms.list_badge_label[i], "Next");
    }

    card = create_card(content);
    create_section_title(card, "Wake behavior", NULL);
    row = create_row(card);
    label = lv_label_create(row);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, "Default snooze");
    s_ui.alarms.manage_snooze_btn = create_action_button(row, "Snooze 10 min", alarm_manage_snooze_event_cb, NULL);

    label = lv_label_create(card);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, "Alarm volume");
    s_ui.alarms.manage_volume_slider = lv_slider_create(card);
    lv_slider_set_range(s_ui.alarms.manage_volume_slider, 0, 100);
    lv_obj_set_width(s_ui.alarms.manage_volume_slider, lv_pct(100));
    style_slider(s_ui.alarms.manage_volume_slider);
    lv_obj_add_event_cb(s_ui.alarms.manage_volume_slider, alarm_manage_volume_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    s_ui.alarms.manage_volume_label = lv_label_create(card);
    lv_obj_set_style_text_color(s_ui.alarms.manage_volume_label, lv_color_hex(0xB7B7B7), 0);
    row = create_row(card);
    center_row(row);
    s_ui.alarms.manage_test_btn = create_action_button(row, "Preview tone", alarm_manage_test_event_cb, NULL);
    lv_obj_set_width(s_ui.alarms.manage_test_btn, 220);

    ui_surface_create_edge_sensor(s_ui.alarms.management_overlay,
                                  &s_ui.alarms.top_sensor,
                                  UI_SURFACE_EDGE_TOP,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  alarm_close_swipe_event_cb,
                                  (void *)UI_SURFACE_EDGE_TOP);
    ui_surface_create_edge_sensor(s_ui.alarms.management_overlay,
                                  &s_ui.alarms.bottom_sensor,
                                  UI_SURFACE_EDGE_BOTTOM,
                                  ALARM_CLOSE_BOTTOM_EDGE_ZONE,
                                  alarm_close_swipe_event_cb,
                                  (void *)UI_SURFACE_EDGE_BOTTOM);
    ui_surface_create_edge_sensor(s_ui.alarms.management_overlay,
                                  &s_ui.alarms.left_sensor,
                                  UI_SURFACE_EDGE_LEFT,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  alarm_close_swipe_event_cb,
                                  (void *)UI_SURFACE_EDGE_LEFT);
    ui_surface_create_edge_sensor(s_ui.alarms.management_overlay,
                                  &s_ui.alarms.right_sensor,
                                  UI_SURFACE_EDGE_RIGHT,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  alarm_close_swipe_event_cb,
                                  (void *)UI_SURFACE_EDGE_RIGHT);
}

static void create_alarm_editor_overlay(void)
{
    static const char *repeat_labels[4] = {"One time", "Every day", "Weekdays", "Weekends"};
    static const uint8_t display_day_order[7] = {1, 2, 3, 4, 5, 6, 0};
    ui_surface_t surface;
    lv_obj_t *title;
    lv_obj_t *content;
    lv_obj_t *card;
    lv_obj_t *row;
    lv_obj_t *label;
    lv_obj_t *actions;
    lv_obj_t *save_btn;
    lv_obj_t *panel;

    ui_surface_create_fullscreen(&surface,
                                 s_ui.screen,
                                 lv_color_black(),
                                 LV_OPA_80,
                                 lv_color_hex(0x0B0B0B),
                                 92,
                                 "Edit alarm",
                                 alarm_editor_overlay_event_cb,
                                 alarm_editor_close_event_cb);
    s_ui.alarms.editor_overlay = surface.overlay;
    panel = surface.panel;
    title = surface.title;
    content = surface.content;

    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);

    card = create_card(content);
    s_ui.alarms.editor_time_label = lv_label_create(card);
    lv_obj_set_width(s_ui.alarms.editor_time_label, lv_pct(100));
    lv_obj_set_style_text_font(s_ui.alarms.editor_time_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_ui.alarms.editor_time_label, lv_color_white(), 0);
    lv_obj_set_style_text_align(s_ui.alarms.editor_time_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_ui.alarms.editor_time_label, "7:00 AM");
    s_ui.alarms.editor_summary_label = lv_label_create(card);
    lv_obj_set_width(s_ui.alarms.editor_summary_label, lv_pct(100));
    lv_obj_set_style_text_color(s_ui.alarms.editor_summary_label, lv_color_hex(0xB7B7B7), 0);
    lv_obj_set_style_text_align(s_ui.alarms.editor_summary_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_ui.alarms.editor_summary_label, "Every day");

    row = create_row(card);
    center_row(row);
    label = lv_label_create(row);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, "Enabled");
    s_ui.alarms.editor_enabled_sw = lv_switch_create(row);
    lv_obj_add_event_cb(s_ui.alarms.editor_enabled_sw, alarm_editor_enabled_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    card = create_card(content);
    create_section_title(card, "Set time", NULL);
    row = create_row(card);
    center_row(row);
    lv_obj_set_style_pad_column(row, 20, 0);
    s_ui.alarms.editor_hour_roller = lv_roller_create(row);
    lv_roller_set_options(s_ui.alarms.editor_hour_roller, s_ui.hour_options, LV_ROLLER_MODE_NORMAL);
    lv_obj_set_width(s_ui.alarms.editor_hour_roller, 224);
    lv_obj_set_height(s_ui.alarms.editor_hour_roller, 236);
    lv_roller_set_visible_row_count(s_ui.alarms.editor_hour_roller, 5);
    style_alarm_roller(s_ui.alarms.editor_hour_roller);
    lv_obj_add_event_cb(s_ui.alarms.editor_hour_roller, alarm_editor_time_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_ui.alarms.editor_minute_roller = lv_roller_create(row);
    lv_roller_set_options(s_ui.alarms.editor_minute_roller, s_ui.minute_options, LV_ROLLER_MODE_NORMAL);
    lv_obj_set_width(s_ui.alarms.editor_minute_roller, 224);
    lv_obj_set_height(s_ui.alarms.editor_minute_roller, 236);
    lv_roller_set_visible_row_count(s_ui.alarms.editor_minute_roller, 5);
    style_alarm_roller(s_ui.alarms.editor_minute_roller);
    lv_obj_add_event_cb(s_ui.alarms.editor_minute_roller, alarm_editor_time_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    card = create_card(content);
    create_section_title(card, "Repeat", NULL);
    row = create_row(card);
    center_row(row);
    for (int i = 0; i < 4; ++i) {
        s_ui.alarms.editor_repeat_btn[i] = create_filter_chip(row,
                                                             repeat_labels[i],
                                                             alarm_editor_repeat_event_cb,
                                                             (void *)(uintptr_t)i);
    }

    row = create_row(card);
    center_row(row);
    for (int display_idx = 0; display_idx < 7; ++display_idx) {
        int day = display_day_order[display_idx];

        s_ui.alarms.editor_day_btn[day] = create_filter_chip(row,
                                                            s_day_short[day],
                                                            alarm_editor_day_event_cb,
                                                            (void *)(uintptr_t)day);
    }

    actions = lv_obj_create(panel);
    lv_obj_set_width(actions, lv_pct(100));
    lv_obj_set_height(actions, 96);
    lv_obj_set_style_bg_color(actions, lv_color_hex(0x101010), 0);
    lv_obj_set_style_border_width(actions, 0, 0);
    lv_obj_set_style_pad_left(actions, 24, 0);
    lv_obj_set_style_pad_right(actions, 24, 0);
    lv_obj_set_style_pad_top(actions, 18, 0);
    lv_obj_set_layout(actions, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(actions, 16, 0);
    center_row(actions);

    s_ui.alarms.editor_delete_btn = create_action_button(actions, "Delete", alarm_editor_delete_event_cb, NULL);
    lv_obj_set_width(s_ui.alarms.editor_delete_btn, 170);
    lv_obj_set_style_bg_color(s_ui.alarms.editor_delete_btn, lv_color_hex(0x3B1C1C), 0);
    save_btn = create_action_button(actions, "Save alarm", alarm_editor_save_event_cb, NULL);
    lv_obj_set_width(save_btn, 240);
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(0xC8A248), 0);
    lv_obj_set_style_text_color(save_btn, lv_color_black(), 0);

    ui_surface_create_edge_sensor(s_ui.alarms.editor_overlay,
                                  &s_ui.alarms.top_sensor,
                                  UI_SURFACE_EDGE_TOP,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  alarm_close_swipe_event_cb,
                                  (void *)UI_SURFACE_EDGE_TOP);
    ui_surface_create_edge_sensor(s_ui.alarms.editor_overlay,
                                  &s_ui.alarms.bottom_sensor,
                                  UI_SURFACE_EDGE_BOTTOM,
                                  ALARM_CLOSE_BOTTOM_EDGE_ZONE,
                                  alarm_close_swipe_event_cb,
                                  (void *)UI_SURFACE_EDGE_BOTTOM);
    ui_surface_create_edge_sensor(s_ui.alarms.editor_overlay,
                                  &s_ui.alarms.left_sensor,
                                  UI_SURFACE_EDGE_LEFT,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  alarm_close_swipe_event_cb,
                                  (void *)UI_SURFACE_EDGE_LEFT);
    ui_surface_create_edge_sensor(s_ui.alarms.editor_overlay,
                                  &s_ui.alarms.right_sensor,
                                  UI_SURFACE_EDGE_RIGHT,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  alarm_close_swipe_event_cb,
                                  (void *)UI_SURFACE_EDGE_RIGHT);
}
