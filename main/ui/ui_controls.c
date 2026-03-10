#include "ui/ui_controls.h"

void create_section_title(lv_obj_t *parent, const char *title, const char *subtitle)
{
    lv_obj_t *heading = lv_label_create(parent);
    lv_obj_set_style_text_font(heading, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(heading, lv_color_white(), 0);
    lv_label_set_text(heading, title);

    if (subtitle != NULL) {
        lv_obj_t *desc = lv_label_create(parent);
        lv_obj_set_style_text_color(desc, lv_color_hex(0xA8A8A8), 0);
        lv_label_set_text(desc, subtitle);
    }
}

lv_obj_t *create_card(lv_obj_t *parent)
{
    lv_obj_t *card = lv_obj_create(parent);

    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x171717), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 24, 0);
    lv_obj_set_style_text_color(card, lv_color_white(), 0);
    lv_obj_set_style_pad_all(card, 20, 0);
    lv_obj_set_style_pad_row(card, 16, 0);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    return card;
}

lv_obj_t *create_row(lv_obj_t *parent)
{
    lv_obj_t *row = lv_obj_create(parent);

    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_set_style_text_color(row, lv_color_white(), 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_column(row, 12, 0);
    lv_obj_set_style_pad_row(row, 12, 0);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    return row;
}

lv_obj_t *create_action_button(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_t *label = lv_label_create(button);

    lv_obj_set_height(button, 48);
    lv_obj_set_style_radius(button, 18, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x303030), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x454545), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(button, lv_color_white(), 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_pad_left(button, 18, 0);
    lv_obj_set_style_pad_right(button, 18, 0);
    lv_obj_set_style_pad_top(button, 10, 0);
    lv_obj_set_style_pad_bottom(button, 10, 0);
    lv_label_set_text(label, text);
    lv_obj_center(label);

    if (cb != NULL) {
        lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, user_data);
    }

    return button;
}

lv_obj_t *create_big_action_button(lv_obj_t *parent,
                                   const char *text,
                                   lv_color_t color,
                                   lv_event_cb_t cb,
                                   void *user_data)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_t *label = lv_label_create(button);

    lv_obj_set_size(button, 250, 170);
    lv_obj_set_style_radius(button, 42, 0);
    lv_obj_set_style_bg_color(button, color, 0);
    lv_obj_set_style_bg_color(button, lv_color_darken(color, 25), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(button, lv_color_white(), 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_pad_all(button, 18, 0);
    lv_obj_set_style_shadow_width(button, 16, 0);
    lv_obj_set_style_shadow_opa(button, LV_OPA_20, 0);
    lv_obj_set_style_shadow_color(button, lv_color_black(), 0);

    lv_obj_set_width(label, lv_pct(100));
    lv_obj_set_style_text_font(label, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(label, text);
    lv_obj_center(label);

    if (cb != NULL) {
        lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, user_data);
    }

    return button;
}

lv_obj_t *create_dropdown(lv_obj_t *parent, const char *options, uint32_t width)
{
    lv_obj_t *dd = lv_dropdown_create(parent);

    lv_dropdown_set_options(dd, options);
    lv_obj_set_width(dd, width);
    lv_obj_set_height(dd, 48);
    lv_obj_set_style_bg_color(dd, lv_color_hex(0x232323), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(dd, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_color(dd, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_color(dd, lv_color_white(), LV_PART_INDICATOR);
    lv_obj_set_style_border_width(dd, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(dd, 16, LV_PART_MAIN);
    return dd;
}

void set_action_button_text(lv_obj_t *button, const char *text)
{
    lv_obj_t *label = lv_obj_get_child(button, 0);

    if (label != NULL) {
        lv_label_set_text(label, text);
    }
}

void style_slider(lv_obj_t *slider)
{
    lv_obj_set_height(slider, 16);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x363636), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_white(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_outline_width(slider, 0, LV_PART_KNOB);
}

void center_row(lv_obj_t *row)
{
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
}
