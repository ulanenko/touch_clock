#include "ui/clock_ui_private.h"

#include <stdlib.h>

static void alarm_set_label_text_if_changed(lv_obj_t *label, const char *text);
static void alarm_set_button_text_if_changed(lv_obj_t *button, const char *text);
static void style_alarm_switch(lv_obj_t *sw);
static ui_edge_ctx_t *alarm_edge_ctx(clock_ui_context_t *ctx, ui_surface_edge_t edge);
static bool active_alarm_requires_math(const clock_ui_context_t *ctx);
static void alarm_overlay_set_math_visible(clock_ui_context_t *ctx, bool visible);
static void alarm_overlay_generate_math_problem(clock_ui_context_t *ctx);
static void alarm_overlay_reset_math_input(clock_ui_context_t *ctx);
static lv_obj_t *create_alarm_math_keypad_button(lv_obj_t *parent,
                                                 const char *text,
                                                 lv_event_cb_t cb,
                                                 void *user_data);

#define ALARM_MATH_MAX_INPUT_LEN 2U
#define ALARM_BANNER_MIN_BRIGHTNESS_UI_PERCENT 10

static void alarm_banner_set_visible(clock_ui_context_t *ctx, bool visible)
{
    if (ctx->alarms.banner == NULL) {
        return;
    }

    if (visible) {
        lv_obj_clear_flag(ctx->alarms.banner, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(ctx->alarms.banner, LV_OBJ_FLAG_HIDDEN);
    }

    if (ctx->alarms.banner_visible == visible) {
        return;
    }

    ctx->alarms.banner_visible = visible;
    request_set_temporary_brightness_floor(ctx,
                                           visible,
                                           brightness_ui_to_hw(ALARM_BANNER_MIN_BRIGHTNESS_UI_PERCENT));
}

void sync_alarm_banner_style(clock_ui_context_t *ctx, clock_face_id_t face)
{
    bool dark_badge = sanitize_enabled_face(face) >= CLOCK_FACE_STERNGLAS;

    lv_obj_set_style_bg_color(ctx->alarms.banner,
                              dark_badge ? lv_color_hex(0x101010) : lv_color_hex(0xECEFF2),
                              0);
    lv_obj_set_style_bg_opa(ctx->alarms.banner, dark_badge ? LV_OPA_90 : LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ctx->alarms.banner, 1, 0);
    lv_obj_set_style_border_color(ctx->alarms.banner,
                                  dark_badge ? lv_color_hex(UI_ACCENT_BORDER_COL) : lv_color_hex(0xC7CDD3),
                                  0);
    lv_obj_set_style_border_opa(ctx->alarms.banner, dark_badge ? LV_OPA_40 : LV_OPA_70, 0);
    lv_obj_set_style_shadow_width(ctx->alarms.banner, dark_badge ? 28 : 22, 0);
    lv_obj_set_style_shadow_spread(ctx->alarms.banner, dark_badge ? 3 : 1, 0);
    lv_obj_set_style_shadow_color(ctx->alarms.banner,
                                  dark_badge ? lv_color_hex(UI_ACCENT_BORDER_COL) : lv_color_hex(UI_ACCENT_GLOW_COL),
                                  0);
    lv_obj_set_style_shadow_opa(ctx->alarms.banner, dark_badge ? LV_OPA_30 : LV_OPA_40, 0);
    lv_obj_set_style_shadow_offset_x(ctx->alarms.banner, 0, 0);
    lv_obj_set_style_shadow_offset_y(ctx->alarms.banner, 8, 0);
    lv_obj_set_style_text_color(ctx->alarms.banner_label,
                                dark_badge ? lv_color_hex(UI_ACCENT_COL) : lv_color_hex(0x1E1A14),
                                0);
}

void update_alarm_banner(clock_ui_context_t *ctx, time_t now)
{
    char text[96];

    if (ctx->alarms.banner_feedback_until > now && ctx->alarms.banner_feedback_text[0] != '\0') {
        alarm_set_label_text_if_changed(ctx->alarms.banner_label, ctx->alarms.banner_feedback_text);
        alarm_banner_set_visible(ctx, true);
        return;
    }

    ctx->alarms.banner_feedback_until = 0;
    ctx->alarms.banner_feedback_revertible = false;
    ctx->alarms.banner_feedback_text[0] = '\0';

    if (ctx->runtime->snooze_active && ctx->runtime->snooze_deadline > now) {
        int minutes_left = (int)((ctx->runtime->snooze_deadline - now + 59) / 60);

        if (minutes_left < 1) {
            minutes_left = 1;
        }
        snprintf(text, sizeof(text), "Waking up again in %d minute%s",
                 minutes_left,
                 (minutes_left == 1) ? "" : "s");
        alarm_set_label_text_if_changed(ctx->alarms.banner_label, text);
        alarm_banner_set_visible(ctx, true);
        return;
    }

    if (!ctx->runtime->alarm_ringing &&
        ctx->runtime->next_alarm_epoch > now &&
        (ctx->runtime->next_alarm_epoch - now) <= 1800) {
        int minutes_left = (int)((ctx->runtime->next_alarm_epoch - now + 59) / 60);

        if (minutes_left < 1) {
            minutes_left = 1;
        }
        snprintf(text, sizeof(text), "Waking you up in %d minute%s",
                 minutes_left,
                 (minutes_left == 1) ? "" : "s");
        alarm_set_label_text_if_changed(ctx->alarms.banner_label, text);
        alarm_banner_set_visible(ctx, true);
        return;
    }

    alarm_banner_set_visible(ctx, false);
}

void sync_alarm_overlay(clock_ui_context_t *ctx, time_t now)
{
    bool requires_math;

    LV_UNUSED(now);

    if (ctx->alarms.overlay == NULL) {
        return;
    }

    if (!ctx->runtime->alarm_ringing) {
        alarm_overlay_set_math_visible(ctx, false);
        lv_obj_add_flag(ctx->alarms.overlay, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    requires_math = active_alarm_requires_math(ctx);

    char snooze_text[32];
    snprintf(snooze_text, sizeof(snooze_text), "Snooze %u min", ctx->settings->snooze_minutes);
    set_action_button_text(ctx->alarms.snooze_btn, snooze_text);
    if (ctx->alarms.overlay_math_snooze_btn != NULL) {
        set_action_button_text(ctx->alarms.overlay_math_snooze_btn, snooze_text);
    }
    set_action_button_text(ctx->alarms.stop_btn, "Off");
    if (ctx->alarms.overlay_label != NULL) {
        lv_obj_add_flag(ctx->alarms.overlay_label, LV_OBJ_FLAG_HIDDEN);
    }
    if (ctx->alarms.overlay_subtitle != NULL) {
        lv_obj_add_flag(ctx->alarms.overlay_subtitle, LV_OBJ_FLAG_HIDDEN);
    }
    if (!requires_math) {
        alarm_overlay_set_math_visible(ctx, false);
    }
    lv_obj_clear_flag(ctx->alarms.overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ctx->alarms.overlay);
}

#define ALARM_CARD_DELETE_REVEAL 108
#define ALARM_CARD_DELETE_TRIGGER 34
#define ALARM_CARD_SCROLL_CANCEL_TRIGGER 12
#define ALARM_CARD_CLOSED_OFFSET (ALARM_CARD_DELETE_REVEAL / 2)
#define ALARM_CARD_OPEN_SQUEEZE 28
#define ALARM_CARD_ANIM_MS 180
#define ALARM_MANAGEMENT_AUTO_CLOSE_MS 20000

static const alarm_config_t *active_alarm_config(const clock_ui_context_t *ctx)
{
    int8_t alarm_index;

    if (ctx == NULL || ctx->runtime == NULL || ctx->settings == NULL) {
        return NULL;
    }

    alarm_index = ctx->runtime->active_alarm_index;
    if (alarm_index < 0 || alarm_index >= MAX_ALARMS) {
        return NULL;
    }

    return &ctx->settings->alarms[alarm_index];
}

static bool active_alarm_requires_math(const clock_ui_context_t *ctx)
{
    const alarm_config_t *alarm = active_alarm_config(ctx);

    return alarm != NULL && alarm->math_unlock_enabled;
}

static void alarm_management_auto_close_pause(clock_ui_context_t *ctx)
{
    if (ctx->alarms.management_auto_close_timer == NULL) {
        return;
    }

    lv_timer_pause(ctx->alarms.management_auto_close_timer);
}

static void alarm_management_auto_close_reset(clock_ui_context_t *ctx)
{
    if (ctx->alarms.management_auto_close_timer == NULL) {
        return;
    }

    lv_timer_set_period(ctx->alarms.management_auto_close_timer, ALARM_MANAGEMENT_AUTO_CLOSE_MS);
    lv_timer_resume(ctx->alarms.management_auto_close_timer);
    lv_timer_reset(ctx->alarms.management_auto_close_timer);
}

static void alarm_management_auto_close_timer_cb(lv_timer_t *timer)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_timer_get_user_data(timer);

    if (ctx == NULL || !ctx->alarms.open || ctx->alarms.editor_open || ctx->alarms.settings_open) {
        lv_timer_pause(timer);
        return;
    }

    if (ctx->alarms.management_scrolling || ctx->alarms.close_dragging || ctx->alarms.list_swipe_dragging) {
        alarm_management_auto_close_reset(ctx);
        return;
    }

    alarm_management_close(ctx);
}

static void anim_alarm_card_width_cb(void *var, int32_t value)
{
    lv_obj_set_width((lv_obj_t *)var, (lv_coord_t)value);
}

static void anim_alarm_card_translate_cb(void *var, int32_t value)
{
    lv_obj_set_style_translate_x((lv_obj_t *)var, (lv_coord_t)value, 0);
}

static void animate_alarm_card_layout(clock_ui_context_t *ctx,
                                      uint8_t alarm_index,
                                      lv_coord_t target_width,
                                      lv_coord_t target_translate)
{
    lv_anim_t anim;
    lv_obj_t *content;
    lv_coord_t current_width;
    lv_coord_t current_translate;

    if (alarm_index >= MAX_ALARMS) {
        return;
    }

    content = ctx->alarms.list_content[alarm_index];
    if (content == NULL) {
        return;
    }

    current_width = lv_obj_get_width(content);
    current_translate = lv_obj_get_style_translate_x(content, LV_PART_MAIN);

    lv_anim_delete(content, anim_alarm_card_width_cb);
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, content);
    lv_anim_set_exec_cb(&anim, anim_alarm_card_width_cb);
    lv_anim_set_time(&anim, ALARM_CARD_ANIM_MS);
    lv_anim_set_values(&anim, current_width, target_width);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_start(&anim);

    lv_anim_delete(content, anim_alarm_card_translate_cb);
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, content);
    lv_anim_set_exec_cb(&anim, anim_alarm_card_translate_cb);
    lv_anim_set_time(&anim, ALARM_CARD_ANIM_MS);
    lv_anim_set_values(&anim, current_translate, target_translate);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_start(&anim);
}

static void close_alarm_delete_action(clock_ui_context_t *ctx, int8_t alarm_index)
{
    lv_coord_t base_width;

    if (alarm_index < 0 || alarm_index >= MAX_ALARMS) {
        return;
    }

    base_width = lv_obj_get_width(ctx->alarms.list_card[alarm_index]) - ALARM_CARD_DELETE_REVEAL;
    if (base_width < 0) {
        base_width = 0;
    }

    if (ctx->alarms.list_content[alarm_index] != NULL) {
        animate_alarm_card_layout(ctx, (uint8_t)alarm_index, base_width, ALARM_CARD_CLOSED_OFFSET);
    }
    if (ctx->alarms.list_delete_btn[alarm_index] != NULL) {
        lv_obj_add_flag(ctx->alarms.list_delete_btn[alarm_index], LV_OBJ_FLAG_HIDDEN);
    }
    if (ctx->alarms.swipe_open_index == alarm_index) {
        ctx->alarms.swipe_open_index = -1;
    }
}

static void open_alarm_delete_action(clock_ui_context_t *ctx, uint8_t alarm_index)
{
    lv_coord_t base_width;

    if (alarm_index >= MAX_ALARMS) {
        return;
    }

    if (ctx->alarms.swipe_open_index >= 0 && ctx->alarms.swipe_open_index != alarm_index) {
        close_alarm_delete_action(ctx, ctx->alarms.swipe_open_index);
    }

    base_width = lv_obj_get_width(ctx->alarms.list_card[alarm_index]) - ALARM_CARD_DELETE_REVEAL;
    if (base_width < ALARM_CARD_OPEN_SQUEEZE) {
        base_width = ALARM_CARD_OPEN_SQUEEZE;
    }

    if (ctx->alarms.list_delete_btn[alarm_index] != NULL) {
        lv_obj_clear_flag(ctx->alarms.list_delete_btn[alarm_index], LV_OBJ_FLAG_HIDDEN);
    }
    if (ctx->alarms.list_content[alarm_index] != NULL) {
        animate_alarm_card_layout(ctx, alarm_index, base_width - ALARM_CARD_OPEN_SQUEEZE, 0);
    }
    ctx->alarms.swipe_open_index = (int8_t)alarm_index;
}

static void update_alarm_list_focus_treatment(clock_ui_context_t *ctx)
{
    lv_coord_t available_width;

    if (ctx->alarms.management_content == NULL) {
        return;
    }

    available_width = lv_obj_get_content_width(ctx->alarms.management_content);
    if (available_width <= 0) {
        available_width = lv_obj_get_width(ctx->alarms.management_content);
    }
    if (available_width <= 0) {
        return;
    }

    for (uint8_t i = 0; i < ctx->alarms.management_card_count; ++i) {
        lv_obj_t *card = ctx->alarms.management_card[i];
        lv_obj_t *visual_card = card;
        bool is_alarm_wrapper = false;

        if (card == NULL) {
            continue;
        }

        for (int alarm_index = 0; alarm_index < MAX_ALARMS; ++alarm_index) {
            if (card == ctx->alarms.list_card[alarm_index] &&
                ctx->alarms.list_content[alarm_index] != NULL) {
                visual_card = ctx->alarms.list_content[alarm_index];
                is_alarm_wrapper = true;
                break;
            }
        }

        lv_coord_t width = (lv_coord_t)(((available_width + 1) / 2) * 2);
        lv_coord_t wrapper_width = is_alarm_wrapper ? (width + ALARM_CARD_DELETE_REVEAL) : width;
        if (LV_ABS(lv_obj_get_width(card) - wrapper_width) >= 2) {
            lv_obj_set_width(card, wrapper_width);
        }
        if (is_alarm_wrapper) {
            for (int alarm_index = 0; alarm_index < MAX_ALARMS; ++alarm_index) {
                if (card == ctx->alarms.list_card[alarm_index]) {
                    lv_coord_t visual_width =
                        width - ((ctx->alarms.swipe_open_index == alarm_index) ? ALARM_CARD_OPEN_SQUEEZE : 0);
                    lv_coord_t translate =
                        (ctx->alarms.swipe_open_index == alarm_index) ? 0 : ALARM_CARD_CLOSED_OFFSET;

                    if (LV_ABS(lv_obj_get_width(visual_card) - visual_width) >= 2) {
                        lv_obj_set_width(visual_card, visual_width);
                    }
                    if (lv_obj_get_style_translate_x(visual_card, LV_PART_MAIN) != translate) {
                        lv_obj_set_style_translate_x(visual_card, translate, 0);
                    }
                    break;
                }
            }
        } else {
            if (LV_ABS(lv_obj_get_width(visual_card) - width) >= 2) {
                lv_obj_set_width(visual_card, width);
            }
        }
        lv_obj_set_style_bg_color(visual_card, lv_color_hex(0x171717), 0);
        lv_obj_set_style_bg_opa(visual_card, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(visual_card, 0, 0);
        lv_obj_set_style_shadow_width(visual_card, 0, 0);
    }
}

static void refresh_alarm_management_layout(clock_ui_context_t *ctx)
{
    if (ctx->alarms.management_overlay == NULL || ctx->alarms.management_content == NULL) {
        return;
    }

    lv_obj_update_layout(ctx->screen);
    lv_obj_update_layout(ctx->alarms.management_overlay);
    lv_obj_update_layout(ctx->alarms.management_content);
    update_alarm_list_focus_treatment(ctx);
}

static void alarm_set_label_text_if_changed(lv_obj_t *label, const char *text)
{
    const char *current_text;

    if (label == NULL || text == NULL) {
        return;
    }

    current_text = lv_label_get_text(label);
    if (current_text == NULL || strcmp(current_text, text) != 0) {
        lv_label_set_text(label, text);
    }
}

static void alarm_set_button_text_if_changed(lv_obj_t *button, const char *text)
{
    lv_obj_t *label;

    if (button == NULL || text == NULL) {
        return;
    }

    label = lv_obj_get_child(button, 0);
    if (label == NULL) {
        return;
    }

    alarm_set_label_text_if_changed(label, text);
}

static void style_alarm_switch(lv_obj_t *sw)
{
    if (sw == NULL) {
        return;
    }

    lv_obj_set_size(sw, 96, 56);
    lv_obj_set_style_pad_all(sw, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0x2E2E2E), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(sw, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0x7D8894), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw, lv_color_hex(0xF4F6F8), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_shadow_width(sw, 0, LV_PART_KNOB);
    lv_obj_set_style_shadow_opa(sw, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_bg_color(sw, lv_color_white(), LV_PART_KNOB | LV_STATE_CHECKED);
}

static ui_edge_ctx_t *alarm_edge_ctx(clock_ui_context_t *ctx, ui_surface_edge_t edge)
{
    return &ctx->alarms.close_edge_ctx[edge];
}

static void alarm_set_switch_checked_if_changed(lv_obj_t *sw, bool checked)
{
    bool current_checked;

    if (sw == NULL) {
        return;
    }

    current_checked = lv_obj_has_state(sw, LV_STATE_CHECKED);
    if (current_checked == checked) {
        return;
    }

    if (checked) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(sw, LV_STATE_CHECKED);
    }
}

bool alarm_controls_need_sync(const clock_ui_context_t *ctx)
{
    if (!ctx->alarms.cache_valid) {
        return true;
    }

    if (memcmp(ctx->alarms.cached_alarms,
               ctx->settings->alarms,
               sizeof(ctx->alarms.cached_alarms)) != 0) {
        return true;
    }

    if (ctx->alarms.cached_alarm_ringing != ctx->runtime->alarm_ringing ||
        ctx->alarms.cached_alarm_test_active != ctx->runtime->alarm_test_active ||
        ctx->alarms.cached_snooze_active != ctx->runtime->snooze_active ||
        ctx->alarms.cached_snooze_deadline != ctx->runtime->snooze_deadline ||
        ctx->alarms.cached_next_alarm_epoch != ctx->runtime->next_alarm_epoch ||
        ctx->alarms.cached_next_alarm_index != ctx->runtime->next_alarm_index ||
        ctx->alarms.cached_active_alarm_index != ctx->runtime->active_alarm_index ||
        ctx->alarms.cached_alarm_volume != ctx->settings->alarm_volume ||
        ctx->alarms.cached_ascending_alarm_enabled != ctx->settings->ascending_alarm_enabled ||
        ctx->alarms.cached_snooze_minutes != ctx->settings->snooze_minutes ||
        ctx->alarms.cached_skipped_alarm_index != ctx->settings->skipped_alarm_index ||
        ctx->alarms.cached_skipped_alarm_epoch != ctx->settings->skipped_alarm_epoch) {
        return true;
    }

    return false;
}

static void alarm_set_checkable_state_if_changed(lv_obj_t *obj, bool checked)
{
    bool current_checked;

    if (obj == NULL) {
        return;
    }

    current_checked = lv_obj_has_state(obj, LV_STATE_CHECKED);
    if (current_checked == checked) {
        return;
    }

    if (checked) {
        lv_obj_add_state(obj, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(obj, LV_STATE_CHECKED);
    }
}

static void alarm_set_roller_selected_if_changed(lv_obj_t *roller, uint16_t selected)
{
    if (roller == NULL) {
        return;
    }

    if (lv_roller_get_selected(roller) != selected) {
        lv_roller_set_selected(roller, selected, LV_ANIM_OFF);
    }
}

static void alarm_overlay_update_math_answer(clock_ui_context_t *ctx)
{
    const char *text = "";

    if (ctx == NULL || ctx->alarms.overlay_math_answer == NULL) {
        return;
    }

    if (ctx->alarms.overlay_math_answer_len > 0) {
        text = ctx->alarms.overlay_math_answer_text;
    }

    alarm_set_label_text_if_changed(ctx->alarms.overlay_math_answer, text);
}

static void alarm_overlay_reset_math_input(clock_ui_context_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    ctx->alarms.overlay_math_answer_text[0] = '\0';
    ctx->alarms.overlay_math_answer_len = 0;
    alarm_overlay_update_math_answer(ctx);
    alarm_set_label_text_if_changed(ctx->alarms.overlay_math_error, "");
}

static void alarm_overlay_set_math_visible(clock_ui_context_t *ctx, bool visible)
{
    if (ctx == NULL) {
        return;
    }

    ctx->alarms.overlay_math_visible = visible;
    if (ctx->alarms.overlay_actions != NULL) {
        if (visible) {
            lv_obj_add_flag(ctx->alarms.overlay_actions, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(ctx->alarms.overlay_actions, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (ctx->alarms.overlay_math_card != NULL) {
        if (visible) {
            lv_obj_clear_flag(ctx->alarms.overlay_math_card, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(ctx->alarms.overlay_math_card, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (!visible) {
        alarm_overlay_reset_math_input(ctx);
    }
}

static void alarm_overlay_generate_math_problem(clock_ui_context_t *ctx)
{
    uint32_t seed;

    if (ctx == NULL) {
        return;
    }

    seed = lv_tick_get() + (uint32_t)time(NULL);
    if ((seed & 1U) == 0U) {
        ctx->alarms.overlay_math_operand_a = (uint8_t)(3U + (seed % 8U));
        ctx->alarms.overlay_math_operand_b = (uint8_t)(2U + ((seed / 7U) % 9U));
        ctx->alarms.overlay_math_expected_answer =
            (uint8_t)(ctx->alarms.overlay_math_operand_a + ctx->alarms.overlay_math_operand_b);
        snprintf(ctx->alarms.overlay_math_problem_text,
                 sizeof(ctx->alarms.overlay_math_problem_text),
                 "%u + %u =",
                 ctx->alarms.overlay_math_operand_a,
                 ctx->alarms.overlay_math_operand_b);
    } else {
        ctx->alarms.overlay_math_operand_b = (uint8_t)(2U + (seed % 7U));
        ctx->alarms.overlay_math_expected_answer = (uint8_t)(4U + ((seed / 5U) % 12U));
        ctx->alarms.overlay_math_operand_a =
            (uint8_t)(ctx->alarms.overlay_math_expected_answer + ctx->alarms.overlay_math_operand_b);
        snprintf(ctx->alarms.overlay_math_problem_text,
                 sizeof(ctx->alarms.overlay_math_problem_text),
                 "%u - %u =",
                 ctx->alarms.overlay_math_operand_a,
                 ctx->alarms.overlay_math_operand_b);
    }

    alarm_set_label_text_if_changed(ctx->alarms.overlay_math_problem, ctx->alarms.overlay_math_problem_text);
    alarm_overlay_reset_math_input(ctx);
}

static void alarm_overlay_submit_math_answer(clock_ui_context_t *ctx)
{
    unsigned long answer;

    if (ctx == NULL || ctx->alarms.overlay_math_answer_len == 0) {
        return;
    }

    answer = strtoul(ctx->alarms.overlay_math_answer_text, NULL, 10);
    if (answer == ctx->alarms.overlay_math_expected_answer) {
        if (ctx->callbacks.on_alarm_stop_requested != NULL) {
            ctx->callbacks.on_alarm_stop_requested(ctx->user_ctx);
        }
        return;
    }

    alarm_overlay_reset_math_input(ctx);
    alarm_set_label_text_if_changed(ctx->alarms.overlay_math_error, "Wrong answer. Try again.");
}

static void alarm_overlay_append_math_digit(clock_ui_context_t *ctx, uint8_t digit)
{
    uint8_t expected_len;

    if (ctx == NULL || ctx->alarms.overlay_math_answer_len >= ALARM_MATH_MAX_INPUT_LEN || digit > 9U) {
        return;
    }

    ctx->alarms.overlay_math_answer_text[ctx->alarms.overlay_math_answer_len++] = (char)('0' + digit);
    ctx->alarms.overlay_math_answer_text[ctx->alarms.overlay_math_answer_len] = '\0';
    alarm_set_label_text_if_changed(ctx->alarms.overlay_math_error, "");
    alarm_overlay_update_math_answer(ctx);

    expected_len = (ctx->alarms.overlay_math_expected_answer >= 10U) ? 2U : 1U;
    if (ctx->alarms.overlay_math_answer_len >= expected_len) {
        alarm_overlay_submit_math_answer(ctx);
    }
}

static void format_alarm_editor_summary(char *buffer, size_t size, const alarm_config_t *alarm)
{
    if (buffer == NULL || size == 0 || alarm == NULL) {
        return;
    }

    format_alarm_repeat_summary(buffer, size, alarm);
    if (!alarm->enabled) {
        size_t len = strlen(buffer);

        if (len < size) {
            snprintf(buffer + len, size - len, "  Off");
        }
    }
}

static bool alarm_slot_is_empty(const alarm_config_t *alarm)
{
    return !alarm->enabled &&
           alarm->hour == 7 &&
           alarm->minute == 0 &&
           alarm->repeat_mode == ALARM_REPEAT_ONCE &&
           alarm->days_mask == 0x7F;
}

static void alarm_list_swipe_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    alarm_ctx_t *alarm_ctx = (alarm_ctx_t *)lv_event_get_user_data(event);
    lv_indev_t *indev = lv_event_get_indev(event);
    clock_ui_context_t *ctx;
    lv_point_t point;

    if (alarm_ctx == NULL || alarm_ctx->ui == NULL || alarm_ctx->alarm_index >= MAX_ALARMS || indev == NULL) {
        return;
    }

    ctx = alarm_ctx->ui;
    lv_indev_get_point(indev, &point);
    alarm_management_auto_close_reset(ctx);

    if (code == LV_EVENT_PRESSED) {
        ctx->alarms.list_swipe_dragging = true;
        ctx->alarms.list_swipe_consumed = false;
        ctx->alarms.swipe_drag_index = (int8_t)alarm_ctx->alarm_index;
        ctx->alarms.list_swipe_start_point = point;
        return;
    }

    if (code == LV_EVENT_PRESSING &&
        ctx->alarms.list_swipe_dragging &&
        ctx->alarms.swipe_drag_index == (int8_t)alarm_ctx->alarm_index) {
        int32_t dx = point.x - ctx->alarms.list_swipe_start_point.x;
        int32_t dy = point.y - ctx->alarms.list_swipe_start_point.y;

        if (LV_ABS(dy) > LV_ABS(dx) && LV_ABS(dy) >= ALARM_CARD_SCROLL_CANCEL_TRIGGER) {
            ctx->alarms.list_swipe_dragging = false;
            ctx->alarms.swipe_drag_index = -1;
            return;
        }

        if (LV_ABS(dx) > LV_ABS(dy)) {
            if (dx <= -ALARM_CARD_DELETE_TRIGGER) {
                open_alarm_delete_action(ctx, alarm_ctx->alarm_index);
                ctx->alarms.list_swipe_consumed = true;
                ctx->alarms.list_swipe_dragging = false;
            } else if (dx >= ALARM_CARD_DELETE_TRIGGER &&
                       ctx->alarms.swipe_open_index == (int8_t)alarm_ctx->alarm_index) {
                close_alarm_delete_action(ctx, (int8_t)alarm_ctx->alarm_index);
                ctx->alarms.list_swipe_consumed = true;
                ctx->alarms.list_swipe_dragging = false;
            }
        }
        return;
    }

    if ((code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) &&
        ctx->alarms.swipe_drag_index == (int8_t)alarm_ctx->alarm_index) {
        ctx->alarms.list_swipe_dragging = false;
        ctx->alarms.swipe_drag_index = -1;
    }

    if (code == LV_EVENT_CLICKED) {
        if (ctx->alarms.list_swipe_consumed) {
            ctx->alarms.list_swipe_consumed = false;
            return;
        }

        if (ctx->alarms.swipe_open_index >= 0) {
            close_alarm_delete_action(ctx, ctx->alarms.swipe_open_index);
            return;
        }

        open_alarm_editor(ctx, alarm_ctx->alarm_index, false);
    }
}

void sync_alarm_controls(clock_ui_context_t *ctx)
{
    char volume[32];
    char status[96];
    time_t now = 0;
    int enabled_count;
    bool management_visible;
    bool editor_visible;

    if (ctx->alarms.management_overlay == NULL) {
        return;
    }

    management_visible = ctx->alarms.open && !ctx->alarms.editor_open;
    editor_visible = ctx->alarms.editor_open && ctx->alarms.editor_overlay != NULL;

    time(&now);

    if (ctx->alarms.manage_snooze_btn != NULL) {
        char snooze_text[32];

        snprintf(snooze_text, sizeof(snooze_text), "Snooze %u min", ctx->settings->snooze_minutes);
        alarm_set_button_text_if_changed(ctx->alarms.manage_snooze_btn, snooze_text);
    }

    if (ctx->alarms.manage_volume_slider != NULL) {
        if (lv_slider_get_value(ctx->alarms.manage_volume_slider) != ctx->settings->alarm_volume) {
            ctx->suppress_events = true;
            lv_slider_set_value(ctx->alarms.manage_volume_slider, ctx->settings->alarm_volume, LV_ANIM_OFF);
            ctx->suppress_events = false;
        }
    }

    snprintf(volume, sizeof(volume), "%u%%", ctx->settings->alarm_volume);
    alarm_set_label_text_if_changed(ctx->alarms.manage_volume_label, volume);
    if (ctx->alarms.manage_ascending_sw != NULL) {
        ctx->suppress_events = true;
        alarm_set_switch_checked_if_changed(ctx->alarms.manage_ascending_sw,
                                            ctx->settings->ascending_alarm_enabled);
        ctx->suppress_events = false;
    }
    if (ctx->alarms.manage_test_btn != NULL) {
        alarm_set_button_text_if_changed(ctx->alarms.manage_test_btn,
                                         ctx->runtime->alarm_test_active ? "Stop test" : "Test sound");
    }
    if (ctx->alarms.management_settings_summary != NULL) {
        char wake_summary[64];

        snprintf(wake_summary, sizeof(wake_summary), "Snooze %u min  Volume %u%%",
                 ctx->settings->snooze_minutes,
                 ctx->settings->alarm_volume);
        alarm_set_label_text_if_changed(ctx->alarms.management_settings_summary, wake_summary);
    }

    if (management_visible) {
        enabled_count = count_enabled_alarms(ctx);
        if (ctx->runtime->alarm_ringing) {
            snprintf(status, sizeof(status), "Alarm is ringing now");
        } else if (ctx->runtime->alarm_test_active) {
            snprintf(status, sizeof(status), "Testing alarm sound");
        } else if (ctx->runtime->snooze_active) {
            struct tm snooze_tm;
            char time_text[24];

            localtime_r(&ctx->runtime->snooze_deadline, &snooze_tm);
            format_alarm_time(time_text, sizeof(time_text), snooze_tm.tm_hour, snooze_tm.tm_min);
            snprintf(status, sizeof(status), "Snoozed until %s", time_text);
        } else if (ctx->runtime->next_alarm_epoch > 0) {
            struct tm next_tm;
            char time_text[24];

            localtime_r(&ctx->runtime->next_alarm_epoch, &next_tm);
            format_alarm_time(time_text, sizeof(time_text), next_tm.tm_hour, next_tm.tm_min);
            snprintf(status, sizeof(status), "Next up %s %s", s_day_short[next_tm.tm_wday], time_text);
        } else if (enabled_count > 0) {
            snprintf(status, sizeof(status), "%d alarm%s active", enabled_count, (enabled_count == 1) ? "" : "s");
        } else {
            snprintf(status, sizeof(status), "No alarms scheduled");
        }

        alarm_set_label_text_if_changed(ctx->alarms.management_status, status);

        for (int i = 0; i < MAX_ALARMS; ++i) {
            const alarm_config_t *alarm = &ctx->settings->alarms[i];
            char time_text[24];
            char meta[128];

            if (ctx->alarms.list_time_label[i] == NULL || ctx->alarms.list_card[i] == NULL) {
                continue;
            }

            if (alarm_slot_is_empty(alarm)) {
                close_alarm_delete_action(ctx, (int8_t)i);
                lv_obj_add_flag(ctx->alarms.list_card[i], LV_OBJ_FLAG_HIDDEN);
                continue;
            }

            lv_obj_clear_flag(ctx->alarms.list_card[i], LV_OBJ_FLAG_HIDDEN);

            format_alarm_time(time_text, sizeof(time_text), alarm->hour, alarm->minute);
            format_alarm_repeat_summary(meta, sizeof(meta), alarm);
            if (!alarm->enabled) {
                size_t meta_len = strlen(meta);

                if (meta_len < sizeof(meta)) {
                    snprintf(meta + meta_len, sizeof(meta) - meta_len, "  Off");
                }
            }

            alarm_set_label_text_if_changed(ctx->alarms.list_time_label[i], time_text);
            alarm_set_label_text_if_changed(ctx->alarms.list_meta_label[i], meta);

            ctx->suppress_events = true;
            alarm_set_switch_checked_if_changed(ctx->alarms.list_toggle[i], alarm->enabled);
            ctx->suppress_events = false;

            if (ctx->runtime->next_alarm_index == i &&
                ctx->runtime->next_alarm_epoch > now &&
                !ctx->runtime->alarm_ringing) {
                alarm_set_label_text_if_changed(ctx->alarms.list_badge_label[i], "Next");
                lv_obj_clear_flag(ctx->alarms.list_badge[i], LV_OBJ_FLAG_HIDDEN);
            } else if (!alarm->enabled) {
                alarm_set_label_text_if_changed(ctx->alarms.list_badge_label[i], "Off");
                lv_obj_clear_flag(ctx->alarms.list_badge[i], LV_OBJ_FLAG_HIDDEN);
            } else if (alarm->repeat_mode == ALARM_REPEAT_ONCE) {
                alarm_set_label_text_if_changed(ctx->alarms.list_badge_label[i], "Once");
                lv_obj_clear_flag(ctx->alarms.list_badge[i], LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(ctx->alarms.list_badge[i], LV_OBJ_FLAG_HIDDEN);
            }
        }

        if (ctx->alarms.focus_alarm_index >= 0 &&
            ctx->alarms.focus_alarm_index < MAX_ALARMS &&
            ctx->alarms.list_card[ctx->alarms.focus_alarm_index] != NULL &&
            !lv_obj_has_flag(ctx->alarms.list_card[ctx->alarms.focus_alarm_index], LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_scroll_to_view_recursive(ctx->alarms.list_card[ctx->alarms.focus_alarm_index], LV_ANIM_ON);
        }
        ctx->alarms.focus_alarm_index = -1;

        update_alarm_list_focus_treatment(ctx);
    }

    if (editor_visible) {
        char editor_time[24];
        char summary[64];
        uint8_t preset = alarm_repeat_preset_from_config(&ctx->alarms.editor_draft);

        ctx->suppress_events = true;
        alarm_set_roller_selected_if_changed(ctx->alarms.editor_hour_roller, ctx->alarms.editor_draft.hour);
        alarm_set_roller_selected_if_changed(ctx->alarms.editor_minute_roller, ctx->alarms.editor_draft.minute);
        ctx->suppress_events = false;
        for (int i = 0; i < 4; ++i) {
            alarm_set_checkable_state_if_changed(ctx->alarms.editor_repeat_btn[i], preset == i);
        }

        for (int day = 0; day < 7; ++day) {
            alarm_set_checkable_state_if_changed(ctx->alarms.editor_day_btn[day],
                                                 (ctx->alarms.editor_draft.days_mask & (1U << day)) != 0);
        }

        format_alarm_time(editor_time, sizeof(editor_time),
                          ctx->alarms.editor_draft.hour,
                          ctx->alarms.editor_draft.minute);
        format_alarm_editor_summary(summary, sizeof(summary), &ctx->alarms.editor_draft);

        alarm_set_label_text_if_changed(ctx->alarms.editor_time_label, editor_time);
        alarm_set_label_text_if_changed(ctx->alarms.editor_summary_label, summary);
        if (ctx->alarms.editor_save_btn != NULL) {
            alarm_set_button_text_if_changed(ctx->alarms.editor_save_btn,
                                             ctx->alarms.editor_is_new ? "Add" : "Save");
        }
        if (ctx->alarms.editor_delete_btn != NULL) {
            if (ctx->alarms.editor_is_new) {
                if (!lv_obj_has_flag(ctx->alarms.editor_delete_btn, LV_OBJ_FLAG_HIDDEN)) {
                    lv_obj_add_flag(ctx->alarms.editor_delete_btn, LV_OBJ_FLAG_HIDDEN);
                }
            } else {
                if (lv_obj_has_flag(ctx->alarms.editor_delete_btn, LV_OBJ_FLAG_HIDDEN)) {
                    lv_obj_clear_flag(ctx->alarms.editor_delete_btn, LV_OBJ_FLAG_HIDDEN);
                }
            }
        }
        if (ctx->alarms.editor_math_sw != NULL) {
            ctx->suppress_events = true;
            alarm_set_switch_checked_if_changed(ctx->alarms.editor_math_sw,
                                                ctx->alarms.editor_draft.math_unlock_enabled);
            ctx->suppress_events = false;
        }
    }

    memcpy(ctx->alarms.cached_alarms, ctx->settings->alarms, sizeof(ctx->alarms.cached_alarms));
    ctx->alarms.cached_alarm_ringing = ctx->runtime->alarm_ringing;
    ctx->alarms.cached_alarm_test_active = ctx->runtime->alarm_test_active;
    ctx->alarms.cached_snooze_active = ctx->runtime->snooze_active;
    ctx->alarms.cached_snooze_deadline = ctx->runtime->snooze_deadline;
    ctx->alarms.cached_next_alarm_epoch = ctx->runtime->next_alarm_epoch;
    ctx->alarms.cached_next_alarm_index = ctx->runtime->next_alarm_index;
    ctx->alarms.cached_active_alarm_index = ctx->runtime->active_alarm_index;
    ctx->alarms.cached_alarm_volume = ctx->settings->alarm_volume;
    ctx->alarms.cached_ascending_alarm_enabled = ctx->settings->ascending_alarm_enabled;
    ctx->alarms.cached_snooze_minutes = ctx->settings->snooze_minutes;
    ctx->alarms.cached_skipped_alarm_index = ctx->settings->skipped_alarm_index;
    ctx->alarms.cached_skipped_alarm_epoch = ctx->settings->skipped_alarm_epoch;
    ctx->alarms.cache_valid = true;
}

static void alarm_snooze_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);

    if (ctx == NULL) {
        return;
    }

    if (ctx->callbacks.on_alarm_snooze_requested != NULL) {
        ctx->callbacks.on_alarm_snooze_requested(ctx->user_ctx);
    }
}

static void alarm_stop_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);

    if (ctx == NULL) {
        return;
    }

    if (active_alarm_requires_math(ctx)) {
        alarm_overlay_generate_math_problem(ctx);
        alarm_overlay_set_math_visible(ctx, true);
        sync_alarm_overlay(ctx, 0);
        return;
    }

    if (ctx->callbacks.on_alarm_stop_requested != NULL) {
        ctx->callbacks.on_alarm_stop_requested(ctx->user_ctx);
    }
}

static void alarm_math_digit_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    lv_obj_t *target = lv_event_get_target(event);

    if (ctx == NULL) {
        return;
    }

    for (uint8_t digit = 0; digit <= 9; ++digit) {
        if (ctx->alarms.overlay_math_keypad[digit] == target) {
            alarm_overlay_append_math_digit(ctx, digit);
            return;
        }
    }
}

static void alarm_editor_math_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);

    if (ctx == NULL || ctx->suppress_events) {
        return;
    }

    ctx->alarms.editor_draft.math_unlock_enabled =
        lv_obj_has_state(ctx->alarms.editor_math_sw, LV_STATE_CHECKED);
    sync_alarm_controls(ctx);
}

static void alarm_banner_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    time_t now;

    if (ctx == NULL) {
        return;
    }

    time(&now);
    if (ctx->alarms.banner_feedback_until > now &&
        ctx->alarms.banner_feedback_revertible &&
        ctx->callbacks.on_next_alarm_cancel_undo_requested != NULL) {
        ctx->callbacks.on_next_alarm_cancel_undo_requested(ctx->user_ctx);
        ctx->alarms.banner_feedback_until = now + 2;
        ctx->alarms.banner_feedback_revertible = false;
        snprintf(ctx->alarms.banner_feedback_text,
                 sizeof(ctx->alarms.banner_feedback_text),
                 "Restored");
        update_alarm_banner(ctx, now);
        return;
    }

    if (ctx->callbacks.on_next_alarm_cancel_requested == NULL) {
        return;
    }

    ctx->callbacks.on_next_alarm_cancel_requested(ctx->user_ctx);
    ctx->alarms.banner_feedback_until = now + 2;
    ctx->alarms.banner_feedback_revertible = true;
    snprintf(ctx->alarms.banner_feedback_text,
             sizeof(ctx->alarms.banner_feedback_text),
             "Cancelled");
    update_alarm_banner(ctx, now);
}

void create_alarm_banner(clock_ui_context_t *ctx)
{
    ctx->alarms.banner = lv_obj_create(ctx->screen);
    lv_obj_set_size(ctx->alarms.banner, 430, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(ctx->alarms.banner, lv_color_hex(0x101010), 0);
    lv_obj_set_style_bg_opa(ctx->alarms.banner, LV_OPA_90, 0);
    lv_obj_set_style_radius(ctx->alarms.banner, 22, 0);
    lv_obj_set_style_pad_left(ctx->alarms.banner, 22, 0);
    lv_obj_set_style_pad_right(ctx->alarms.banner, 22, 0);
    lv_obj_set_style_pad_top(ctx->alarms.banner, 13, 0);
    lv_obj_set_style_pad_bottom(ctx->alarms.banner, 13, 0);
    lv_obj_align(ctx->alarms.banner, LV_ALIGN_BOTTOM_MID, 0, -170);
    lv_obj_remove_flag(ctx->alarms.banner, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ctx->alarms.banner, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(ctx->alarms.banner, alarm_banner_event_cb, LV_EVENT_CLICKED, ctx);

    ctx->alarms.banner_label = lv_label_create(ctx->alarms.banner);
    lv_obj_set_style_text_font(ctx->alarms.banner_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(ctx->alarms.banner_label, lv_color_hex(UI_ACCENT_COL), 0);
    lv_obj_set_style_text_letter_space(ctx->alarms.banner_label, 1, 0);
    lv_obj_set_style_text_line_space(ctx->alarms.banner_label, 3, 0);
    lv_obj_set_width(ctx->alarms.banner_label, lv_pct(100));
    lv_obj_set_style_text_align(ctx->alarms.banner_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(ctx->alarms.banner_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(ctx->alarms.banner_label, "");
}

void create_alarm_overlay(clock_ui_context_t *ctx)
{
    static const uint8_t keypad_layout[4][3] = {
        {1, 2, 3},
        {4, 5, 6},
        {7, 8, 9},
        {255, 0, 254},
    };
    lv_obj_t *actions;
    lv_obj_t *math_card;
    lv_obj_t *row;
    lv_obj_t *label;

    ctx->alarms.overlay = lv_obj_create(ctx->screen);
    lv_obj_set_size(ctx->alarms.overlay, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_color(ctx->alarms.overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(ctx->alarms.overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ctx->alarms.overlay, 0, 0);
    lv_obj_set_style_radius(ctx->alarms.overlay, 0, 0);
    lv_obj_set_style_pad_all(ctx->alarms.overlay, 0, 0);
    lv_obj_set_scrollbar_mode(ctx->alarms.overlay, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(ctx->alarms.overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ctx->alarms.overlay, LV_OBJ_FLAG_HIDDEN);

    ctx->alarms.overlay_label = lv_label_create(ctx->alarms.overlay);
    lv_obj_set_style_text_font(ctx->alarms.overlay_label, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(ctx->alarms.overlay_label, lv_color_white(), 0);
    lv_obj_align(ctx->alarms.overlay_label, LV_ALIGN_TOP_MID, 0, 74);
    lv_label_set_text(ctx->alarms.overlay_label, "Alarm");

    ctx->alarms.overlay_subtitle = lv_label_create(ctx->alarms.overlay);
    lv_obj_set_width(ctx->alarms.overlay_subtitle, 520);
    lv_obj_set_style_text_font(ctx->alarms.overlay_subtitle, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(ctx->alarms.overlay_subtitle, lv_color_hex(0xB9BDC2), 0);
    lv_obj_set_style_text_align(ctx->alarms.overlay_subtitle, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(ctx->alarms.overlay_subtitle, LV_ALIGN_TOP_MID, 0, 132);
    lv_label_set_text(ctx->alarms.overlay_subtitle, "Snooze it or turn it off");

    actions = create_row(ctx->alarms.overlay);
    ctx->alarms.overlay_actions = actions;
    lv_obj_set_style_pad_column(actions, 0, 0);
    lv_obj_set_style_pad_row(actions, 18, 0);
    lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(actions, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_width(actions, LV_SIZE_CONTENT);
    lv_obj_align(actions, LV_ALIGN_CENTER, 0, 24);

    ctx->alarms.snooze_btn = create_big_action_button(actions,
                                                      "Snooze 10 min",
                                                      lv_color_hex(0x535A6B),
                                                      alarm_snooze_event_cb,
                                                      ctx);
    ctx->alarms.stop_btn = create_big_action_button(actions,
                                                    "Off",
                                                    lv_color_hex(0xB54B3D),
                                                    alarm_stop_event_cb,
                                                    ctx);

    lv_obj_set_size(ctx->alarms.snooze_btn, 272, 272);
    lv_obj_set_style_radius(ctx->alarms.snooze_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(ctx->alarms.snooze_btn, 0, 0);
    lv_obj_set_style_shadow_width(ctx->alarms.snooze_btn, 28, 0);
    label = lv_obj_get_child(ctx->alarms.snooze_btn, 0);
    if (label != NULL) {
        lv_obj_set_width(label, 184);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_36, 0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        lv_obj_center(label);
    }

    lv_obj_set_size(ctx->alarms.stop_btn, 272, 272);
    lv_obj_set_style_radius(ctx->alarms.stop_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(ctx->alarms.stop_btn, 0, 0);
    lv_obj_set_style_shadow_width(ctx->alarms.stop_btn, 28, 0);
    label = lv_obj_get_child(ctx->alarms.stop_btn, 0);
    if (label != NULL) {
        lv_obj_set_width(label, 184);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_36, 0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        lv_obj_center(label);
    }

    math_card = create_card(ctx->alarms.overlay);
    ctx->alarms.overlay_math_card = math_card;
    lv_obj_set_size(math_card, 560, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(math_card, 26, 0);
    lv_obj_set_style_pad_row(math_card, 10, 0);
    lv_obj_align(math_card, LV_ALIGN_CENTER, 0, 24);
    lv_obj_add_flag(math_card, LV_OBJ_FLAG_HIDDEN);

    row = create_row(math_card);
    center_row(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 14, 0);

    ctx->alarms.overlay_math_problem = lv_label_create(row);
    lv_obj_set_style_text_font(ctx->alarms.overlay_math_problem, &montserrat_math_72, 0);
    lv_obj_set_style_text_color(ctx->alarms.overlay_math_problem, lv_color_white(), 0);
    lv_obj_set_style_text_align(ctx->alarms.overlay_math_problem, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_text(ctx->alarms.overlay_math_problem, "8 + 6 =");

    label = lv_label_create(row);
    ctx->alarms.overlay_math_answer = label;
    lv_obj_set_style_min_width(label, 94, 0);
    lv_obj_set_style_text_font(label, &montserrat_math_72, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xD8DDE3), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_text(label, "");

    ctx->alarms.overlay_math_error = lv_label_create(math_card);
    lv_obj_set_width(ctx->alarms.overlay_math_error, lv_pct(100));
    lv_obj_set_style_text_font(ctx->alarms.overlay_math_error, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(ctx->alarms.overlay_math_error, lv_color_hex(0xE28D84), 0);
    lv_obj_set_style_text_align(ctx->alarms.overlay_math_error, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(ctx->alarms.overlay_math_error, "");

    for (uint8_t layout_row = 0; layout_row < 4; ++layout_row) {
        row = create_row(math_card);
        center_row(row);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(row, 8, 0);
        for (uint8_t col = 0; col < 3; ++col) {
            uint8_t token = keypad_layout[layout_row][col];

            if (token <= 9U) {
                char text[2] = {(char)('0' + token), '\0'};

                ctx->alarms.overlay_math_keypad[token] =
                    create_alarm_math_keypad_button(row, text, alarm_math_digit_event_cb, ctx);
            } else {
                lv_obj_t *spacer = lv_obj_create(row);

                lv_obj_set_size(spacer, 108, 108);
                lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
                lv_obj_set_style_border_width(spacer, 0, 0);
                lv_obj_clear_flag(spacer, LV_OBJ_FLAG_SCROLLABLE);
            }
        }
    }

    row = create_row(math_card);
    center_row(row);
    ctx->alarms.overlay_math_snooze_btn =
        create_action_button(row, "Snooze 10 min", alarm_snooze_event_cb, ctx);
    lv_obj_set_size(ctx->alarms.overlay_math_snooze_btn, 320, 64);
    lv_obj_set_style_radius(ctx->alarms.overlay_math_snooze_btn, 22, 0);
    lv_obj_set_style_bg_color(ctx->alarms.overlay_math_snooze_btn, lv_color_hex(0x3E4552), 0);
    lv_obj_set_style_bg_color(ctx->alarms.overlay_math_snooze_btn, lv_color_hex(0x505865), LV_STATE_PRESSED);

    alarm_overlay_reset_math_input(ctx);
}

static lv_obj_t *create_filter_chip(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_t *label = lv_label_create(button);

    lv_obj_add_flag(button, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_height(button, 56);
    lv_obj_set_style_radius(button, 22, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x252525), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(UI_ACCENT_COL), LV_STATE_CHECKED);
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
    ui_attach_click_feedback(button, LV_EVENT_CLICKED);

    return button;
}

static lv_obj_t *create_alarm_math_keypad_button(lv_obj_t *parent,
                                                 const char *text,
                                                 lv_event_cb_t cb,
                                                 void *user_data)
{
    lv_obj_t *button = create_action_button(parent, text, cb, user_data);

    lv_obj_set_size(button, 108, 108);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x232323), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x343434), LV_STATE_PRESSED);
    lv_obj_set_style_pad_all(button, 0, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    if (lv_obj_get_child(button, 0) != NULL) {
        lv_obj_set_style_text_font(lv_obj_get_child(button, 0), &lv_font_montserrat_48, 0);
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
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);

    if (ctx == NULL) {
        return;
    }

    alarm_management_auto_close_reset(ctx);
    for (size_t i = 0; i < sizeof(options); ++i) {
        if (options[i] == ctx->settings->snooze_minutes) {
            current_index = i;
            break;
        }
    }

    request_set_snooze_minutes(ctx, options[(current_index + 1) % (sizeof(options) / sizeof(options[0]))]);
    sync_alarm_controls(ctx);
}

static void alarm_manage_snooze_step(clock_ui_context_t *ctx, int direction)
{
    static const uint8_t options[] = {5, 10, 15, 20, 30};
    size_t current_index = 0;

    for (size_t i = 0; i < sizeof(options) / sizeof(options[0]); ++i) {
        if (options[i] == ctx->settings->snooze_minutes) {
            current_index = i;
            break;
        }
    }

    if (direction < 0 && current_index > 0) {
        current_index--;
    } else if (direction > 0 && current_index + 1 < (sizeof(options) / sizeof(options[0]))) {
        current_index++;
    }

    request_set_snooze_minutes(ctx, options[current_index]);
    sync_alarm_controls(ctx);
}

static void alarm_manage_snooze_decrease_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);

    if (ctx == NULL) {
        return;
    }

    alarm_management_auto_close_reset(ctx);
    alarm_manage_snooze_step(ctx, -1);
}

static void alarm_manage_snooze_increase_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);

    if (ctx == NULL) {
        return;
    }

    alarm_management_auto_close_reset(ctx);
    alarm_manage_snooze_step(ctx, 1);
}

static void alarm_manage_volume_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);

    if (ctx == NULL) {
        return;
    }

    if (ctx->suppress_events) {
        return;
    }

    alarm_management_auto_close_reset(ctx);
    request_set_alarm_volume(ctx, (uint8_t)lv_slider_get_value(lv_event_get_target(event)));
    sync_alarm_controls(ctx);
}

static void alarm_manage_ascending_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);

    if (ctx == NULL || ctx->suppress_events) {
        return;
    }

    alarm_management_auto_close_reset(ctx);
    request_set_ascending_alarm_enabled(ctx, lv_obj_has_state(lv_event_get_target(event), LV_STATE_CHECKED));
    sync_alarm_controls(ctx);
}

static void alarm_manage_test_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);

    if (ctx == NULL) {
        return;
    }

    alarm_management_auto_close_reset(ctx);
    if (ctx->callbacks.on_alarm_test_requested != NULL) {
        ctx->callbacks.on_alarm_test_requested(ctx->user_ctx);
    }
    sync_alarm_controls(ctx);
}

static void alarm_settings_entry_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);

    if (ctx == NULL) {
        return;
    }

    if (ctx->alarms.settings_overlay == NULL) {
        return;
    }

    alarm_management_auto_close_pause(ctx);
    ctx->alarms.settings_open = true;
    ctx->alarms.close_swipe_consumed = false;
    if (ctx->alarms.management_overlay != NULL) {
        lv_obj_add_flag(ctx->alarms.management_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    sync_alarm_controls(ctx);
    lv_obj_clear_flag(ctx->alarms.settings_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ctx->alarms.settings_overlay);
}

static void alarm_list_toggle_event_cb(lv_event_t *event)
{
    alarm_ctx_t *alarm_ctx = (alarm_ctx_t *)lv_event_get_user_data(event);
    clock_ui_context_t *ctx;

    if (alarm_ctx == NULL || alarm_ctx->ui == NULL || alarm_ctx->alarm_index >= MAX_ALARMS) {
        return;
    }

    ctx = alarm_ctx->ui;
    if (ctx->suppress_events) {
        return;
    }

    alarm_management_auto_close_reset(ctx);
    request_set_alarm_enabled(ctx, alarm_ctx->alarm_index,
                              lv_obj_has_state(lv_event_get_target(event), LV_STATE_CHECKED));
    sync_alarm_controls(ctx);
}

static void alarm_list_delete_event_cb(lv_event_t *event)
{
    alarm_ctx_t *alarm_ctx = (alarm_ctx_t *)lv_event_get_user_data(event);
    clock_ui_context_t *ctx;

    if (alarm_ctx == NULL || alarm_ctx->ui == NULL || alarm_ctx->alarm_index >= MAX_ALARMS) {
        return;
    }

    ctx = alarm_ctx->ui;
    alarm_management_auto_close_reset(ctx);
    close_alarm_delete_action(ctx, (int8_t)alarm_ctx->alarm_index);
    request_delete_alarm(ctx, alarm_ctx->alarm_index);
    sync_alarm_controls(ctx);
}

static void alarm_editor_time_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    lv_obj_t *target = lv_event_get_target(event);

    if (ctx == NULL) {
        return;
    }

    if (ctx->suppress_events) {
        return;
    }

    if (target == ctx->alarms.editor_hour_roller) {
        ctx->alarms.editor_draft.hour = lv_roller_get_selected(target);
    } else if (target == ctx->alarms.editor_minute_roller) {
        ctx->alarms.editor_draft.minute = lv_roller_get_selected(target);
    }

    sync_alarm_controls(ctx);
}

static void alarm_editor_scroll_to_section_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    lv_obj_t *target = lv_event_get_target(event);
    lv_obj_t *section = NULL;

    if (ctx == NULL) {
        return;
    }

    if (target == ctx->alarms.editor_summary_label) {
        section = ctx->alarms.editor_repeat_card;
    } else if (target == ctx->alarms.editor_time_label) {
        section = ctx->alarms.editor_time_card;
    }

    if (section != NULL) {
        lv_obj_scroll_to_view_recursive(section, LV_ANIM_ON);
    }
}

static void alarm_editor_repeat_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    lv_obj_t *target = lv_event_get_target(event);
    uint8_t preset;

    if (ctx == NULL) {
        return;
    }

    for (preset = 0; preset < 4; ++preset) {
        if (ctx->alarms.editor_repeat_btn[preset] == target) {
            apply_repeat_preset_to_alarm(&ctx->alarms.editor_draft, preset);
            sync_alarm_controls(ctx);
            return;
        }
    }

}

static void alarm_editor_day_event_cb(lv_event_t *event)
{
    alarm_day_ctx_t *day_ctx = (alarm_day_ctx_t *)lv_event_get_user_data(event);
    clock_ui_context_t *ctx;
    uintptr_t day;
    uint8_t mask;

    if (day_ctx == NULL || day_ctx->ui == NULL) {
        return;
    }

    ctx = day_ctx->ui;
    day = day_ctx->day_index;
    if (day >= 7) {
        return;
    }

    ctx->alarms.editor_draft.repeat_mode = ALARM_REPEAT_WEEKLY;
    mask = (uint8_t)(1U << day);
    if ((ctx->alarms.editor_draft.days_mask & mask) != 0) {
        ctx->alarms.editor_draft.days_mask &= (uint8_t)~mask;
        if (ctx->alarms.editor_draft.days_mask == 0) {
            ctx->alarms.editor_draft.days_mask = mask;
        }
    } else {
        ctx->alarms.editor_draft.days_mask |= mask;
    }

    sync_alarm_controls(ctx);
}

static void alarm_editor_save_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);

    if (ctx == NULL) {
        return;
    }

    if (ctx->alarms.editor_index < 0 || ctx->alarms.editor_index >= MAX_ALARMS) {
        return;
    }

    request_save_alarm(ctx, (uint8_t)ctx->alarms.editor_index, &ctx->alarms.editor_draft);
    ctx->alarms.focus_alarm_index = ctx->alarms.editor_index;
    alarm_editor_close(ctx);
    sync_alarm_controls(ctx);
}

static void alarm_editor_delete_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);

    if (ctx == NULL) {
        return;
    }

    if (ctx->alarms.editor_index < 0 || ctx->alarms.editor_index >= MAX_ALARMS) {
        return;
    }

    request_delete_alarm(ctx, (uint8_t)ctx->alarms.editor_index);
    alarm_editor_close(ctx);
    sync_alarm_controls(ctx);
}

static void alarm_editor_overlay_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);

    if (ctx == NULL) {
        return;
    }

    if (lv_event_get_target(event) == lv_event_get_current_target(event)) {
        alarm_editor_close(ctx);
    }
}

static void alarm_editor_close_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);

    if (ctx == NULL) {
        return;
    }

    alarm_editor_close(ctx);
}

static void alarm_settings_close(clock_ui_context_t *ctx)
{
    if (ctx->runtime != NULL &&
        ctx->runtime->alarm_test_active &&
        ctx->callbacks.on_alarm_test_stop_requested != NULL) {
        ctx->callbacks.on_alarm_test_stop_requested(ctx->user_ctx);
    }

    ctx->alarms.settings_open = false;
    ctx->alarms.close_swipe_consumed = false;
    if (ctx->alarms.settings_overlay != NULL) {
        lv_obj_add_flag(ctx->alarms.settings_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    if (ctx->alarms.open && ctx->alarms.management_overlay != NULL) {
        lv_obj_clear_flag(ctx->alarms.management_overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(ctx->alarms.management_overlay);
        alarm_management_auto_close_reset(ctx);
    }
}

static void alarm_settings_overlay_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);

    if (ctx == NULL) {
        return;
    }

    if (lv_event_get_target(event) == lv_event_get_current_target(event)) {
        alarm_settings_close(ctx);
    }
}

static void alarm_settings_close_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);

    if (ctx == NULL) {
        return;
    }

    alarm_settings_close(ctx);
}

static void close_alarm_surface(clock_ui_context_t *ctx)
{
    if (ctx->alarms.editor_open) {
        alarm_editor_close(ctx);
    } else if (ctx->alarms.settings_open) {
        alarm_settings_close(ctx);
    } else if (ctx->alarms.open) {
        alarm_management_close(ctx);
    }
}

static void alarm_close_swipe_event_cb(lv_event_t *event)
{
    ui_edge_ctx_t *edge_ctx = (ui_edge_ctx_t *)lv_event_get_user_data(event);
    lv_event_code_t code = lv_event_get_code(event);
    lv_indev_t *indev = lv_event_get_indev(event);
    clock_ui_context_t *ctx;
    lv_point_t point;

    if (edge_ctx == NULL || edge_ctx->ui == NULL) {
        return;
    }

    ctx = edge_ctx->ui;
    if ((!ctx->alarms.open && !ctx->alarms.editor_open && !ctx->alarms.settings_open) || indev == NULL) {
        return;
    }

    if (ctx->alarms.open && !ctx->alarms.editor_open && !ctx->alarms.settings_open) {
        alarm_management_auto_close_reset(ctx);
    }

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        ctx->alarms.close_dragging = false;
        ctx->alarms.close_swipe_consumed = false;
        return;
    }

    if (ctx->alarms.close_swipe_consumed) {
        return;
    }

    lv_indev_get_point(indev, &point);

    if (code == LV_EVENT_PRESSED) {
        ctx->alarms.close_dragging = true;
        ctx->alarms.close_drag_start_point = point;
        ctx->alarms.close_swipe_consumed = false;
        return;
    }

    if (!ctx->alarms.close_dragging) {
        return;
    }

    if (code == LV_EVENT_PRESSING) {
        if (ui_surface_edge_swipe_trigger(edge_ctx->edge,
                                          &ctx->alarms.close_drag_start_point,
                                          &point,
                                          SETTINGS_CLOSE_SWIPE_TRIGGER)) {
            ctx->alarms.close_dragging = false;
            ctx->alarms.close_swipe_consumed = true;
            close_alarm_surface(ctx);
        }
        return;
    }
}

static void alarm_custom_create_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    int slot;

    if (ctx == NULL) {
        return;
    }

    alarm_management_auto_close_pause(ctx);
    slot = find_alarm_slot_for_new_alarm(ctx);
    if (slot < 0 || slot >= MAX_ALARMS) {
        return;
    }

    open_alarm_editor(ctx, (uint8_t)slot, true);
}

void alarm_management_open(clock_ui_context_t *ctx)
{
    brightness_overlay_hide_immediately(ctx);
    brightness_panel_hide(ctx);
    if (settings_surface_is_open(ctx)) {
        settings_surface_close(ctx);
    }

    close_alarm_delete_action(ctx, ctx->alarms.swipe_open_index);
    ctx->alarms.management_scrolling = false;
    ctx->alarms.close_swipe_consumed = false;
    ctx->alarms.open = true;
    alarm_settings_close(ctx);
    sync_alarm_controls(ctx);
    lv_obj_clear_flag(ctx->alarms.management_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ctx->alarms.management_overlay);
    alarm_management_auto_close_reset(ctx);
    refresh_alarm_management_layout(ctx);
}

void alarm_management_close(clock_ui_context_t *ctx)
{
    close_alarm_delete_action(ctx, ctx->alarms.swipe_open_index);
    alarm_settings_close(ctx);
    alarm_management_auto_close_pause(ctx);
    ctx->alarms.open = false;
    ctx->alarms.close_swipe_consumed = false;
    lv_obj_add_flag(ctx->alarms.management_overlay, LV_OBJ_FLAG_HIDDEN);
    if (!ctx->alarms.editor_open) {
        show_affordances_temporarily(ctx);
    }
}

static void alarm_management_overlay_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);

    if (ctx == NULL) {
        return;
    }

    if (lv_event_get_target(event) == lv_event_get_current_target(event)) {
        alarm_management_close(ctx);
    }
}

static void alarm_management_close_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);

    if (ctx == NULL) {
        return;
    }

    alarm_management_close(ctx);
}

static void alarm_management_scroll_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    lv_event_code_t code = lv_event_get_code(event);

    if (ctx == NULL) {
        return;
    }

    alarm_management_auto_close_reset(ctx);

    if (code == LV_EVENT_SCROLL_BEGIN || code == LV_EVENT_SCROLL) {
        ctx->alarms.management_scrolling = true;
        return;
    }

    if (code == LV_EVENT_SCROLL_END) {
        ctx->alarms.management_scrolling = false;
        refresh_alarm_management_layout(ctx);
    }
}

void alarm_editor_close(clock_ui_context_t *ctx)
{
    ctx->alarms.editor_open = false;
    ctx->alarms.close_swipe_consumed = false;
    lv_obj_add_flag(ctx->alarms.editor_overlay, LV_OBJ_FLAG_HIDDEN);
    if (ctx->alarms.open) {
        lv_obj_clear_flag(ctx->alarms.management_overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(ctx->alarms.management_overlay);
        alarm_management_auto_close_reset(ctx);
    } else {
        show_affordances_temporarily(ctx);
    }
}

void open_alarm_editor(clock_ui_context_t *ctx, uint8_t alarm_index, bool is_new)
{
    time_t now;
    struct tm now_tm;

    if (alarm_index >= MAX_ALARMS) {
        return;
    }

    time(&now);
    localtime_r(&now, &now_tm);

    close_alarm_delete_action(ctx, ctx->alarms.swipe_open_index);
    alarm_management_auto_close_pause(ctx);
    ctx->alarms.editor_index = (int8_t)alarm_index;
    ctx->alarms.editor_is_new = is_new;
    ctx->alarms.editor_open = true;
    ctx->alarms.close_swipe_consumed = false;
    if (ctx->alarms.management_overlay != NULL) {
        lv_obj_add_flag(ctx->alarms.management_overlay, LV_OBJ_FLAG_HIDDEN);
    }

    if (is_new) {
        int minute = ((now_tm.tm_min + 9) / 10) * 10;

        ctx->alarms.editor_draft = ctx->settings->alarms[alarm_index];
        ctx->alarms.editor_draft.enabled = true;
        ctx->alarms.editor_draft.repeat_mode = ALARM_REPEAT_ONCE;
        ctx->alarms.editor_draft.days_mask = 0x7F;
        ctx->alarms.editor_draft.math_unlock_enabled = false;
        if (minute >= 60) {
            minute -= 60;
            now_tm.tm_hour = (now_tm.tm_hour + 1) % 24;
        }
        ctx->alarms.editor_draft.hour = now_tm.tm_hour;
        ctx->alarms.editor_draft.minute = (uint8_t)minute;
    } else {
        ctx->alarms.editor_draft = ctx->settings->alarms[alarm_index];
    }

    sync_alarm_controls(ctx);
    lv_obj_clear_flag(ctx->alarms.editor_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ctx->alarms.editor_overlay);
}

void create_alarm_management_overlay(clock_ui_context_t *ctx)
{
    ui_surface_t surface;
    lv_obj_t *header;
    lv_obj_t *title;
    lv_obj_t *content;
    lv_obj_t *card;
    lv_obj_t *row;
    lv_obj_t *label;

    ui_surface_create_fullscreen(&surface,
                                 ctx->screen,
                                 lv_color_hex(0x070707),
                                 LV_OPA_COVER,
                                 lv_color_hex(0x0B0B0B),
                                 92,
                                 "Alarms",
                                 alarm_management_overlay_event_cb,
                                 alarm_management_close_event_cb,
                                 ctx);
    ctx->alarms.management_overlay = surface.overlay;
    header = surface.header;
    title = surface.title;
    content = surface.content;
    ctx->alarms.management_content = content;
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);

    ctx->alarms.management_status = lv_label_create(header);
    lv_obj_set_width(ctx->alarms.management_status, 520);
    lv_obj_set_style_text_font(ctx->alarms.management_status, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(ctx->alarms.management_status, lv_color_hex(0xB1B1B1), 0);
    lv_obj_set_style_text_align(ctx->alarms.management_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(ctx->alarms.management_status, 3, 0);
    lv_label_set_long_mode(ctx->alarms.management_status, LV_LABEL_LONG_WRAP);
    lv_label_set_text(ctx->alarms.management_status, "No alarms scheduled");
    lv_obj_align(ctx->alarms.management_status, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_bottom(content, 28, 0);
    ctx->alarms.management_card_count = 0;
    ctx->alarms.swipe_open_index = -1;
    ctx->alarms.swipe_drag_index = -1;

    card = create_card(content);
    lv_obj_set_style_bg_opa(card, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    row = create_row(card);
    center_row(row);
    create_icon_circle_button(row, LV_SYMBOL_PLUS, 128, alarm_custom_create_event_cb, ctx);

    ctx->alarms.management_list = NULL;
    lv_obj_add_event_cb(content, alarm_management_scroll_event_cb, LV_EVENT_SCROLL_BEGIN, ctx);
    lv_obj_add_event_cb(content, alarm_management_scroll_event_cb, LV_EVENT_SCROLL, ctx);
    lv_obj_add_event_cb(content, alarm_management_scroll_event_cb, LV_EVENT_SCROLL_END, ctx);

    for (int i = 0; i < MAX_ALARMS; ++i) {
        lv_obj_t *alarm_card;
        lv_obj_t *alarm_content;
        lv_obj_t *delete_btn;
        lv_obj_t *top_row;
        lv_obj_t *meta_row;
        lv_obj_t *badge;

        ctx->alarms.alarm_ctx[i].ui = ctx;
        ctx->alarms.alarm_ctx[i].alarm_index = i;
        alarm_card = lv_obj_create(content);
        ctx->alarms.list_card[i] = alarm_card;
        ctx->alarms.management_card[ctx->alarms.management_card_count++] = alarm_card;
        lv_obj_set_width(alarm_card, lv_pct(100));
        lv_obj_set_height(alarm_card, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(alarm_card, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(alarm_card, 0, 0);
        lv_obj_set_style_radius(alarm_card, 0, 0);
        lv_obj_set_style_pad_all(alarm_card, 0, 0);
        lv_obj_set_style_pad_column(alarm_card, 8, 0);
        lv_obj_set_style_pad_row(alarm_card, 0, 0);
        lv_obj_set_layout(alarm_card, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(alarm_card, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(alarm_card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(alarm_card, LV_OBJ_FLAG_SCROLLABLE);

        delete_btn = lv_button_create(alarm_card);
        ctx->alarms.list_delete_btn[i] = delete_btn;
        lv_obj_add_flag(delete_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(delete_btn, 100, 84);
        lv_obj_set_style_radius(delete_btn, 24, 0);
        lv_obj_set_style_bg_color(delete_btn, lv_color_hex(0x8E3535), 0);
        lv_obj_set_style_bg_color(delete_btn, lv_color_hex(0x732828), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(delete_btn, 0, 0);
        lv_obj_add_event_cb(delete_btn, stop_event_bubble_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(delete_btn, stop_event_bubble_cb, LV_EVENT_PRESSING, NULL);
        lv_obj_add_event_cb(delete_btn, stop_event_bubble_cb, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(delete_btn, stop_event_bubble_cb, LV_EVENT_PRESS_LOST, NULL);
        lv_obj_add_event_cb(delete_btn, alarm_list_delete_event_cb, LV_EVENT_CLICKED, &ctx->alarms.alarm_ctx[i]);
        ui_attach_click_feedback(delete_btn, LV_EVENT_CLICKED);
        label = lv_label_create(delete_btn);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_36, 0);
        lv_label_set_text(label, LV_SYMBOL_TRASH);
        lv_obj_center(label);

        alarm_content = create_card(alarm_card);
        ctx->alarms.list_content[i] = alarm_content;
        lv_obj_move_to_index(alarm_content, 0);
        lv_obj_set_style_radius(alarm_content, 30, 0);
        lv_obj_set_style_pad_all(alarm_content, 22, 0);
        lv_obj_set_style_pad_row(alarm_content, 8, 0);
        lv_obj_set_style_translate_x(alarm_content, ALARM_CARD_CLOSED_OFFSET, 0);
        lv_obj_add_event_cb(alarm_content, alarm_list_swipe_event_cb, LV_EVENT_PRESSED, &ctx->alarms.alarm_ctx[i]);
        lv_obj_add_event_cb(alarm_content, alarm_list_swipe_event_cb, LV_EVENT_PRESSING, &ctx->alarms.alarm_ctx[i]);
        lv_obj_add_event_cb(alarm_content, alarm_list_swipe_event_cb, LV_EVENT_RELEASED, &ctx->alarms.alarm_ctx[i]);
        lv_obj_add_event_cb(alarm_content, alarm_list_swipe_event_cb, LV_EVENT_PRESS_LOST, &ctx->alarms.alarm_ctx[i]);
        lv_obj_add_event_cb(alarm_content, alarm_list_swipe_event_cb, LV_EVENT_CLICKED, &ctx->alarms.alarm_ctx[i]);
        ui_attach_click_feedback(alarm_content, LV_EVENT_CLICKED);

        top_row = create_row(alarm_content);
        lv_obj_add_flag(top_row, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_flex_align(top_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_min_height(top_row, 60, 0);

        ctx->alarms.list_time_label[i] = lv_label_create(top_row);
        lv_obj_add_flag(ctx->alarms.list_time_label[i], LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_style_text_font(ctx->alarms.list_time_label[i], &montserrat_digits_56, 0);
        lv_obj_set_style_text_color(ctx->alarms.list_time_label[i], lv_color_white(), 0);
        lv_label_set_text(ctx->alarms.list_time_label[i], "07:00");

        ctx->alarms.list_toggle[i] = lv_switch_create(top_row);
        style_alarm_switch(ctx->alarms.list_toggle[i]);
        lv_obj_add_event_cb(ctx->alarms.list_toggle[i], stop_event_bubble_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(ctx->alarms.list_toggle[i], stop_event_bubble_cb, LV_EVENT_PRESSING, NULL);
        lv_obj_add_event_cb(ctx->alarms.list_toggle[i], stop_event_bubble_cb, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(ctx->alarms.list_toggle[i], stop_event_bubble_cb, LV_EVENT_PRESS_LOST, NULL);
        lv_obj_add_event_cb(ctx->alarms.list_toggle[i], stop_event_bubble_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(ctx->alarms.list_toggle[i], alarm_list_toggle_event_cb, LV_EVENT_VALUE_CHANGED, &ctx->alarms.alarm_ctx[i]);
        ui_attach_click_feedback(ctx->alarms.list_toggle[i], LV_EVENT_VALUE_CHANGED);

        meta_row = create_row(alarm_content);
        lv_obj_add_flag(meta_row, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_flex_align(meta_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(meta_row, 10, 0);

        ctx->alarms.list_meta_label[i] = lv_label_create(meta_row);
        lv_obj_add_flag(ctx->alarms.list_meta_label[i], LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_style_text_color(ctx->alarms.list_meta_label[i], lv_color_hex(0xB7B7B7), 0);
        lv_obj_set_style_text_font(ctx->alarms.list_meta_label[i], &lv_font_montserrat_24, 0);
        lv_label_set_text(ctx->alarms.list_meta_label[i], "Weekdays");

        badge = lv_obj_create(meta_row);
        lv_obj_add_flag(badge, LV_OBJ_FLAG_EVENT_BUBBLE);
        ctx->alarms.list_badge[i] = badge;
        lv_obj_set_size(badge, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(badge, lv_color_hex(UI_ACCENT_COL), 0);
        lv_obj_set_style_text_color(badge, lv_color_hex(UI_ACCENT_TEXT_COL), 0);
        lv_obj_set_style_border_width(badge, 0, 0);
        lv_obj_set_style_radius(badge, 16, 0);
        lv_obj_set_style_pad_left(badge, 12, 0);
        lv_obj_set_style_pad_right(badge, 12, 0);
        lv_obj_set_style_pad_top(badge, 6, 0);
        lv_obj_set_style_pad_bottom(badge, 6, 0);
        lv_obj_add_flag(badge, LV_OBJ_FLAG_HIDDEN);
        ctx->alarms.list_badge_label[i] = lv_label_create(badge);
        lv_obj_add_flag(ctx->alarms.list_badge_label[i], LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_style_text_font(ctx->alarms.list_badge_label[i], &lv_font_montserrat_20, 0);
        lv_label_set_text(ctx->alarms.list_badge_label[i], "Next");
    }

    card = create_card(content);
    lv_obj_set_style_bg_opa(card, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    row = create_row(card);
    center_row(row);
    ctx->alarms.management_settings_btn = create_icon_circle_button(row,
                                                                   LV_SYMBOL_SETTINGS,
                                                                   128,
                                                                   alarm_settings_entry_event_cb,
                                                                   ctx);
    ctx->alarms.management_settings_summary = NULL;

    for (int edge = UI_SURFACE_EDGE_TOP; edge <= UI_SURFACE_EDGE_RIGHT; ++edge) {
        ctx->alarms.close_edge_ctx[edge].ui = ctx;
        ctx->alarms.close_edge_ctx[edge].edge = (ui_surface_edge_t)edge;
    }

    ui_surface_create_edge_sensor(ctx->alarms.management_overlay,
                                  &ctx->alarms.top_sensor,
                                  UI_SURFACE_EDGE_TOP,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  alarm_close_swipe_event_cb,
                                  alarm_edge_ctx(ctx, UI_SURFACE_EDGE_TOP));
    ui_surface_create_edge_sensor(ctx->alarms.management_overlay,
                                  &ctx->alarms.bottom_sensor,
                                  UI_SURFACE_EDGE_BOTTOM,
                                  ALARM_CLOSE_BOTTOM_EDGE_ZONE,
                                  alarm_close_swipe_event_cb,
                                  alarm_edge_ctx(ctx, UI_SURFACE_EDGE_BOTTOM));
    ui_surface_create_edge_sensor(ctx->alarms.management_overlay,
                                  &ctx->alarms.left_sensor,
                                  UI_SURFACE_EDGE_LEFT,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  alarm_close_swipe_event_cb,
                                  alarm_edge_ctx(ctx, UI_SURFACE_EDGE_LEFT));
    ui_surface_create_edge_sensor(ctx->alarms.management_overlay,
                                  &ctx->alarms.right_sensor,
                                  UI_SURFACE_EDGE_RIGHT,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  alarm_close_swipe_event_cb,
                                  alarm_edge_ctx(ctx, UI_SURFACE_EDGE_RIGHT));
    ctx->alarms.management_auto_close_timer = lv_timer_create(alarm_management_auto_close_timer_cb,
                                                              ALARM_MANAGEMENT_AUTO_CLOSE_MS,
                                                              ctx);
    lv_timer_pause(ctx->alarms.management_auto_close_timer);
}

static lv_obj_t *create_alarm_settings_round_button(lv_obj_t *parent,
                                                    const char *text,
                                                    lv_event_cb_t cb,
                                                    void *user_data)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_t *label = lv_label_create(button);

    lv_obj_set_size(button, 78, 78);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x242424), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x343434), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_pad_all(button, 0, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_text_color(button, lv_color_white(), 0);
    if (cb != NULL) {
        lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, user_data);
    }
    ui_attach_click_feedback(button, LV_EVENT_CLICKED);

    lv_obj_set_style_text_font(label, &lv_font_montserrat_36, 0);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return button;
}

void create_alarm_settings_overlay(clock_ui_context_t *ctx)
{
    ui_surface_t surface;
    lv_obj_t *content;
    lv_obj_t *card;
    lv_obj_t *row;
    lv_obj_t *label;

    ui_surface_create_fullscreen(&surface,
                                 ctx->screen,
                                 lv_color_black(),
                                 LV_OPA_80,
                                 lv_color_hex(0x0B0B0B),
                                 92,
                                 "Wake settings",
                                 alarm_settings_overlay_event_cb,
                                 alarm_settings_close_event_cb,
                                 ctx);
    ctx->alarms.settings_overlay = surface.overlay;
    content = surface.content;
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    card = create_card(content);
    lv_obj_set_width(card, 540);
    lv_obj_set_style_pad_all(card, 24, 0);
    lv_obj_set_style_pad_row(card, 14, 0);

    label = lv_label_create(card);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xA8A8A8), 0);
    lv_label_set_text(label, "Snooze duration");

    row = create_row(card);
    center_row(row);
    lv_obj_set_style_pad_column(row, 18, 0);
    create_alarm_settings_round_button(row, "-", alarm_manage_snooze_decrease_event_cb, ctx);
    ctx->alarms.manage_snooze_btn = create_action_button(row, "Snooze 10 min", alarm_manage_snooze_event_cb, ctx);
    lv_obj_set_size(ctx->alarms.manage_snooze_btn, 280, 86);
    lv_obj_set_style_radius(ctx->alarms.manage_snooze_btn, 28, 0);
    lv_obj_set_style_bg_color(ctx->alarms.manage_snooze_btn, lv_color_hex(UI_ACCENT_COL), 0);
    lv_obj_set_style_bg_color(ctx->alarms.manage_snooze_btn, lv_color_hex(UI_ACCENT_COL_PRESSED), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(ctx->alarms.manage_snooze_btn, lv_color_hex(UI_ACCENT_TEXT_COL), 0);
    lv_obj_set_style_text_font(ctx->alarms.manage_snooze_btn, &lv_font_montserrat_24, 0);
    lv_obj_set_style_pad_top(ctx->alarms.manage_snooze_btn, 0, 0);
    lv_obj_set_style_pad_bottom(ctx->alarms.manage_snooze_btn, 0, 0);
    create_alarm_settings_round_button(row, "+", alarm_manage_snooze_increase_event_cb, ctx);

    card = create_card(content);
    lv_obj_set_width(card, 540);
    lv_obj_set_style_pad_all(card, 24, 0);
    lv_obj_set_style_pad_row(card, 16, 0);

    row = create_row(card);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    label = lv_label_create(row);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xA8A8A8), 0);
    lv_label_set_text(label, "Alarm sound");

    ctx->alarms.manage_volume_label = lv_label_create(row);
    lv_obj_set_style_text_font(ctx->alarms.manage_volume_label, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(ctx->alarms.manage_volume_label, lv_color_white(), 0);
    lv_label_set_text(ctx->alarms.manage_volume_label, "70%");

    ctx->alarms.manage_volume_slider = lv_slider_create(card);
    lv_slider_set_range(ctx->alarms.manage_volume_slider, 0, 100);
    lv_obj_set_width(ctx->alarms.manage_volume_slider, lv_pct(100));
    style_slider(ctx->alarms.manage_volume_slider);
    lv_obj_set_height(ctx->alarms.manage_volume_slider, 24);
    lv_obj_set_style_bg_color(ctx->alarms.manage_volume_slider, lv_color_hex(0x2D2D2D), LV_PART_MAIN);
    lv_obj_set_style_bg_color(ctx->alarms.manage_volume_slider, lv_color_hex(UI_ACCENT_COL), LV_PART_INDICATOR);
    lv_obj_set_style_shadow_width(ctx->alarms.manage_volume_slider, 16, LV_PART_KNOB);
    lv_obj_set_style_shadow_color(ctx->alarms.manage_volume_slider, lv_color_hex(UI_ACCENT_COL), LV_PART_KNOB);
    lv_obj_set_style_shadow_opa(ctx->alarms.manage_volume_slider, LV_OPA_20, LV_PART_KNOB);
    lv_obj_add_event_cb(ctx->alarms.manage_volume_slider, alarm_manage_volume_event_cb, LV_EVENT_VALUE_CHANGED, ctx);
    ui_attach_click_feedback(ctx->alarms.manage_volume_slider, LV_EVENT_VALUE_CHANGED);

    row = create_row(card);
    center_row(row);
    ctx->alarms.manage_test_btn = create_action_button(row, "Test sound", alarm_manage_test_event_cb, ctx);
    lv_obj_set_size(ctx->alarms.manage_test_btn, 340, 74);
    lv_obj_set_style_radius(ctx->alarms.manage_test_btn, 26, 0);
    lv_obj_set_style_bg_color(ctx->alarms.manage_test_btn, lv_color_hex(0x2D2D2D), 0);
    lv_obj_set_style_bg_color(ctx->alarms.manage_test_btn, lv_color_hex(0x3A3A3A), LV_STATE_PRESSED);
    lv_obj_set_style_text_font(ctx->alarms.manage_test_btn, &lv_font_montserrat_24, 0);
    lv_obj_set_style_pad_top(ctx->alarms.manage_test_btn, 0, 0);
    lv_obj_set_style_pad_bottom(ctx->alarms.manage_test_btn, 0, 0);

    card = create_card(content);
    lv_obj_set_width(card, 540);
    lv_obj_set_style_pad_all(card, 24, 0);

    row = create_labeled_trailing_control_row(card, "Ascending alarm", "Ramp up", 360, NULL);

    ctx->alarms.manage_ascending_sw = lv_switch_create(row);
    style_alarm_switch(ctx->alarms.manage_ascending_sw);
    lv_obj_add_event_cb(ctx->alarms.manage_ascending_sw, alarm_manage_ascending_event_cb, LV_EVENT_VALUE_CHANGED, ctx);
    ui_attach_click_feedback(ctx->alarms.manage_ascending_sw, LV_EVENT_VALUE_CHANGED);

    ui_surface_create_edge_sensor(ctx->alarms.settings_overlay,
                                  &ctx->alarms.settings_top_sensor,
                                  UI_SURFACE_EDGE_TOP,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  alarm_close_swipe_event_cb,
                                  alarm_edge_ctx(ctx, UI_SURFACE_EDGE_TOP));
    ui_surface_create_edge_sensor(ctx->alarms.settings_overlay,
                                  &ctx->alarms.settings_bottom_sensor,
                                  UI_SURFACE_EDGE_BOTTOM,
                                  ALARM_CLOSE_BOTTOM_EDGE_ZONE,
                                  alarm_close_swipe_event_cb,
                                  alarm_edge_ctx(ctx, UI_SURFACE_EDGE_BOTTOM));
    ui_surface_create_edge_sensor(ctx->alarms.settings_overlay,
                                  &ctx->alarms.settings_left_sensor,
                                  UI_SURFACE_EDGE_LEFT,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  alarm_close_swipe_event_cb,
                                  alarm_edge_ctx(ctx, UI_SURFACE_EDGE_LEFT));
    ui_surface_create_edge_sensor(ctx->alarms.settings_overlay,
                                  &ctx->alarms.settings_right_sensor,
                                  UI_SURFACE_EDGE_RIGHT,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  alarm_close_swipe_event_cb,
                                  alarm_edge_ctx(ctx, UI_SURFACE_EDGE_RIGHT));
}

void create_alarm_editor_overlay(clock_ui_context_t *ctx)
{
    static const char *repeat_labels[4] = {"One time", "Every day", "Weekdays", "Weekends"};
    static const uint8_t display_day_order[7] = {1, 2, 3, 4, 5, 6, 0};
    ui_surface_t surface;
    lv_obj_t *title;
    lv_obj_t *content;
    lv_obj_t *card;
    lv_obj_t *row;
    lv_obj_t *actions;
    lv_obj_t *save_btn;
    lv_obj_t *panel;

    ui_surface_create_fullscreen(&surface,
                                 ctx->screen,
                                 lv_color_black(),
                                 LV_OPA_80,
                                 lv_color_hex(0x0B0B0B),
                                 92,
                                 "Edit alarm",
                                 alarm_editor_overlay_event_cb,
                                 alarm_editor_close_event_cb,
                                 ctx);
    ctx->alarms.editor_overlay = surface.overlay;
    panel = surface.panel;
    title = surface.title;
    content = surface.content;

    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);

    card = create_card(content);
    ctx->alarms.editor_time_label = lv_label_create(card);
    lv_obj_set_width(ctx->alarms.editor_time_label, lv_pct(100));
    lv_obj_set_style_text_font(ctx->alarms.editor_time_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(ctx->alarms.editor_time_label, lv_color_white(), 0);
    lv_obj_set_style_text_align(ctx->alarms.editor_time_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(ctx->alarms.editor_time_label, "07:00");
    lv_obj_add_flag(ctx->alarms.editor_time_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ctx->alarms.editor_time_label,
                        alarm_editor_scroll_to_section_event_cb,
                        LV_EVENT_CLICKED,
                        ctx);
    ctx->alarms.editor_summary_label = lv_label_create(card);
    lv_obj_set_width(ctx->alarms.editor_summary_label, lv_pct(100));
    lv_obj_set_style_text_color(ctx->alarms.editor_summary_label, lv_color_hex(0xB7B7B7), 0);
    lv_obj_set_style_text_font(ctx->alarms.editor_summary_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(ctx->alarms.editor_summary_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(ctx->alarms.editor_summary_label, "Every day");
    lv_obj_add_flag(ctx->alarms.editor_summary_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ctx->alarms.editor_summary_label,
                        alarm_editor_scroll_to_section_event_cb,
                        LV_EVENT_CLICKED,
                        ctx);

    card = create_card(content);
    ctx->alarms.editor_repeat_card = card;
    create_section_title(card, "Repeat schedule", "Choose how often it rings");
    row = create_row(card);
    center_row(row);
    for (int i = 0; i < 4; ++i) {
        ctx->alarms.editor_repeat_btn[i] = create_filter_chip(row,
                                                             repeat_labels[i],
                                                             alarm_editor_repeat_event_cb,
                                                             ctx);
    }

    row = create_row(card);
    center_row(row);
    for (int display_idx = 0; display_idx < 7; ++display_idx) {
        int day = display_day_order[display_idx];

        ctx->alarms.alarm_day_ctx[0][day].ui = ctx;
        ctx->alarms.alarm_day_ctx[0][day].alarm_index = 0;
        ctx->alarms.alarm_day_ctx[0][day].day_index = (uint8_t)day;
        ctx->alarms.editor_day_btn[day] = create_filter_chip(row,
                                                            s_day_short[day],
                                                            alarm_editor_day_event_cb,
                                                            &ctx->alarms.alarm_day_ctx[0][day]);
    }

    card = create_card(content);
    ctx->alarms.editor_time_card = card;
    create_time_picker_section(card,
                               "Set time",
                               ctx->hour_options,
                               ctx->minute_options,
                               224,
                               20,
                               212,
                               alarm_editor_time_event_cb,
                               ctx,
                               &ctx->alarms.editor_hour_roller,
                               &ctx->alarms.editor_minute_roller);

    card = create_card(content);
    ctx->alarms.editor_math_card = card;
    row = create_labeled_trailing_control_row(card,
                                              "Math to turn off",
                                              "Solve a quick sum before the alarm stops",
                                              360,
                                              NULL);
    ctx->alarms.editor_math_sw = lv_switch_create(row);
    style_alarm_switch(ctx->alarms.editor_math_sw);
    lv_obj_add_event_cb(ctx->alarms.editor_math_sw, alarm_editor_math_event_cb, LV_EVENT_VALUE_CHANGED, ctx);
    ui_attach_click_feedback(ctx->alarms.editor_math_sw, LV_EVENT_VALUE_CHANGED);

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

    ctx->alarms.editor_delete_btn = create_action_button(actions, "Delete", alarm_editor_delete_event_cb, ctx);
    lv_obj_set_width(ctx->alarms.editor_delete_btn, 170);
    lv_obj_set_style_bg_color(ctx->alarms.editor_delete_btn, lv_color_hex(0x3B1C1C), 0);
    save_btn = create_action_button(actions, "Save", alarm_editor_save_event_cb, ctx);
    ctx->alarms.editor_save_btn = save_btn;
    lv_obj_set_width(save_btn, 200);
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(UI_ACCENT_COL), 0);
    lv_obj_set_style_text_color(save_btn, lv_color_hex(UI_ACCENT_TEXT_COL), 0);
    lv_obj_set_style_text_color(save_btn, lv_color_black(), 0);

    ui_surface_create_edge_sensor(ctx->alarms.editor_overlay,
                                  &ctx->alarms.top_sensor,
                                  UI_SURFACE_EDGE_TOP,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  alarm_close_swipe_event_cb,
                                  alarm_edge_ctx(ctx, UI_SURFACE_EDGE_TOP));
    ui_surface_create_edge_sensor(ctx->alarms.editor_overlay,
                                  &ctx->alarms.bottom_sensor,
                                  UI_SURFACE_EDGE_BOTTOM,
                                  ALARM_CLOSE_BOTTOM_EDGE_ZONE,
                                  alarm_close_swipe_event_cb,
                                  alarm_edge_ctx(ctx, UI_SURFACE_EDGE_BOTTOM));
    ui_surface_create_edge_sensor(ctx->alarms.editor_overlay,
                                  &ctx->alarms.left_sensor,
                                  UI_SURFACE_EDGE_LEFT,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  alarm_close_swipe_event_cb,
                                  alarm_edge_ctx(ctx, UI_SURFACE_EDGE_LEFT));
    ui_surface_create_edge_sensor(ctx->alarms.editor_overlay,
                                  &ctx->alarms.right_sensor,
                                  UI_SURFACE_EDGE_RIGHT,
                                  SETTINGS_CLOSE_EDGE_ZONE,
                                  alarm_close_swipe_event_cb,
                                  alarm_edge_ctx(ctx, UI_SURFACE_EDGE_RIGHT));
}
