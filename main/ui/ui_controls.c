#include "ui/ui_controls.h"

#include <stdint.h>

void ui_play_click_feedback(void);

typedef struct {
    lv_obj_t *roller;
    uint32_t selected;
} ui_roller_feedback_state_t;

#define UI_ROLLER_FEEDBACK_SLOTS 8

static ui_roller_feedback_state_t g_ui_roller_feedback_states[UI_ROLLER_FEEDBACK_SLOTS];

static ui_roller_feedback_state_t *ui_roller_feedback_state_for(lv_obj_t *roller, bool create)
{
    ui_roller_feedback_state_t *empty = NULL;

    for (size_t i = 0; i < UI_ROLLER_FEEDBACK_SLOTS; ++i) {
        if (g_ui_roller_feedback_states[i].roller == roller) {
            return &g_ui_roller_feedback_states[i];
        }
        if (empty == NULL && g_ui_roller_feedback_states[i].roller == NULL) {
            empty = &g_ui_roller_feedback_states[i];
        }
    }

    if (!create) {
        return NULL;
    }

    if (empty != NULL) {
        empty->roller = roller;
        empty->selected = 0;
        return empty;
    }

    g_ui_roller_feedback_states[0].roller = roller;
    g_ui_roller_feedback_states[0].selected = 0;
    return &g_ui_roller_feedback_states[0];
}

static uint32_t ui_roller_feedback_selected(lv_obj_t *roller)
{
    ui_roller_feedback_state_t *state = ui_roller_feedback_state_for(roller, false);

    return state != NULL ? state->selected : lv_roller_get_selected(roller);
}

static void ui_roller_feedback_set_selected(lv_obj_t *roller, uint32_t selected)
{
    ui_roller_feedback_state_t *state = ui_roller_feedback_state_for(roller, true);

    if (state != NULL) {
        state->selected = selected;
    }
}

static void ui_roller_feedback_clear(lv_obj_t *roller)
{
    ui_roller_feedback_state_t *state = ui_roller_feedback_state_for(roller, false);

    if (state != NULL) {
        state->roller = NULL;
        state->selected = 0;
    }
}

static void ui_click_feedback_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    ui_play_click_feedback();
}

static uint32_t ui_roller_feedback_live_selected(lv_obj_t *roller)
{
    lv_obj_t *label = lv_obj_get_child(roller, 0);
    const lv_font_t *font;
    int32_t line_space;
    int32_t font_h;
    int32_t line_h;
    int32_t offset;
    int32_t id;
    uint32_t option_count;

    if (label == NULL) {
        return lv_roller_get_selected(roller);
    }

    font = lv_obj_get_style_text_font(roller, LV_PART_MAIN);
    font_h = font != NULL ? lv_font_get_line_height(font) : 0;
    line_space = lv_obj_get_style_text_line_space(roller, LV_PART_MAIN);
    line_h = font_h + line_space;
    if (line_h <= 0) {
        return lv_roller_get_selected(roller);
    }

    offset = lv_obj_get_content_height(roller) / 2 - font_h / 2 - lv_obj_get_y(label);
    if (offset >= 0) {
        id = (offset + line_h / 2) / line_h;
    } else {
        id = (offset - line_h / 2) / line_h;
    }

    option_count = lv_roller_get_option_count(roller);
    if (option_count == 0) {
        return 0;
    }

    if (id < 0) {
        id = 0;
    } else if ((uint32_t)id >= option_count) {
        id = (int32_t)option_count - 1;
    }

    return (uint32_t)id;
}

static void ui_roller_click_feedback_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t *roller = lv_event_get_current_target(event);
    uint32_t selected;

    if (roller == NULL) {
        return;
    }

    if (code == LV_EVENT_DELETE) {
        ui_roller_feedback_clear(roller);
        return;
    }

    if (code == LV_EVENT_PRESSED) {
        ui_roller_feedback_set_selected(roller, ui_roller_feedback_live_selected(roller));
        return;
    }

    if (code == LV_EVENT_PRESSING) {
        selected = ui_roller_feedback_live_selected(roller);
    } else if (code == LV_EVENT_VALUE_CHANGED) {
        selected = lv_roller_get_selected(roller);
    } else {
        return;
    }

    if (ui_roller_feedback_selected(roller) == selected) {
        return;
    }

    ui_roller_feedback_set_selected(roller, selected);
    ui_play_click_feedback();
}

void ui_attach_click_feedback(lv_obj_t *obj, lv_event_code_t code)
{
    if (obj == NULL) {
        return;
    }

    lv_obj_add_event_cb(obj, ui_click_feedback_event_cb, code, NULL);
}

void create_section_title(lv_obj_t *parent, const char *title, const char *subtitle)
{
    lv_obj_t *heading = lv_label_create(parent);
    lv_obj_set_style_text_font(heading, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(heading, lv_color_white(), 0);
    lv_obj_set_style_text_color(heading, lv_color_hex(0x7A7A7A), LV_STATE_DISABLED);
    lv_label_set_text(heading, title);

    if (subtitle != NULL) {
        lv_obj_t *desc = lv_label_create(parent);
        lv_obj_set_style_text_color(desc, lv_color_hex(0xA8A8A8), 0);
        lv_obj_set_style_text_color(desc, lv_color_hex(0x686868), LV_STATE_DISABLED);
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

lv_obj_t *create_labeled_trailing_control_row(lv_obj_t *parent,
                                              const char *title,
                                              const char *subtitle,
                                              lv_coord_t text_width,
                                              lv_obj_t **text_col_out)
{
    lv_obj_t *row = create_row(parent);
    lv_obj_t *text_col = lv_obj_create(row);
    lv_obj_t *label;

    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_set_style_bg_opa(text_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(text_col, 0, 0);
    lv_obj_set_style_pad_all(text_col, 0, 0);
    lv_obj_set_style_pad_row(text_col, 6, 0);
    lv_obj_set_width(text_col, text_width);
    lv_obj_set_layout(text_col, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(text_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(text_col, LV_OBJ_FLAG_SCROLLABLE);

    label = lv_label_create(text_col);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x7A7A7A), LV_STATE_DISABLED);
    lv_label_set_text(label, title != NULL ? title : "");

    if (subtitle != NULL) {
        label = lv_label_create(text_col);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0xA8A8A8), 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0x686868), LV_STATE_DISABLED);
        lv_obj_set_style_text_line_space(label, 4, 0);
        lv_label_set_text(label, subtitle);
    }

    if (text_col_out != NULL) {
        *text_col_out = text_col;
    }

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
    lv_obj_set_style_text_font(button, &lv_font_montserrat_24, 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_pad_left(button, 18, 0);
    lv_obj_set_style_pad_right(button, 18, 0);
    lv_obj_set_style_pad_top(button, 10, 0);
    lv_obj_set_style_pad_bottom(button, 10, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_label_set_text(label, text);
    lv_obj_center(label);

    if (cb != NULL) {
        lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, user_data);
    }
    ui_attach_click_feedback(button, LV_EVENT_CLICKED);

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
    ui_attach_click_feedback(button, LV_EVENT_CLICKED);

    return button;
}

lv_obj_t *create_icon_circle_button(lv_obj_t *parent,
                                    const char *symbol,
                                    lv_coord_t size,
                                    lv_event_cb_t cb,
                                    void *user_data)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_t *icon = lv_label_create(button);

    lv_obj_set_size(button, size, size);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x262626), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x3A3A3A), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_pad_all(button, 0, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_text_color(button, lv_color_white(), 0);

    lv_obj_set_style_text_font(icon, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(icon, lv_color_white(), 0);
    lv_label_set_text(icon, symbol);
    lv_obj_center(icon);

    if (cb != NULL) {
        lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, user_data);
    }
    ui_attach_click_feedback(button, LV_EVENT_CLICKED);

    return button;
}

lv_obj_t *create_time_roller(lv_obj_t *parent,
                             const char *options,
                             lv_coord_t width,
                             lv_event_cb_t cb,
                             void *user_data)
{
    lv_obj_t *roller = lv_roller_create(parent);

    lv_roller_set_options(roller, options, LV_ROLLER_MODE_NORMAL);
    lv_obj_set_width(roller, width);
    lv_obj_set_height(roller, 236);
    lv_roller_set_visible_row_count(roller, 5);
    lv_obj_set_style_bg_color(roller, lv_color_hex(0x101010), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(roller, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(roller, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(roller, 28, LV_PART_MAIN);
    lv_obj_set_style_text_color(roller, lv_color_hex(0xB2B9C1), LV_PART_MAIN);
    lv_obj_set_style_text_color(roller, lv_color_hex(0xB2B9C1), LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_style_text_font(roller, &lv_font_montserrat_36, LV_PART_MAIN);
    lv_obj_set_style_text_font(roller, &lv_font_montserrat_36, LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_style_pad_top(roller, 18, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(roller, 18, LV_PART_MAIN);
    lv_obj_set_style_bg_color(roller, lv_color_hex(0xD8DDE3), LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(roller, LV_OPA_COVER, LV_PART_SELECTED);
    lv_obj_set_style_radius(roller, 20, LV_PART_SELECTED);
    lv_obj_set_style_text_color(roller, lv_color_black(), LV_PART_SELECTED);
    lv_obj_set_style_text_color(roller, lv_color_black(), LV_PART_SELECTED | LV_STATE_DISABLED);
    lv_obj_set_style_text_font(roller, &lv_font_montserrat_48, LV_PART_SELECTED);
    lv_obj_set_style_text_font(roller, &lv_font_montserrat_48, LV_PART_SELECTED | LV_STATE_DISABLED);
    lv_obj_set_style_border_width(roller, 0, LV_PART_SELECTED);
    lv_obj_set_style_bg_color(roller, lv_color_hex(0x101010), LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_style_bg_opa(roller, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DISABLED);
    lv_obj_set_style_bg_color(roller, lv_color_hex(0xD8DDE3), LV_PART_SELECTED | LV_STATE_DISABLED);
    lv_obj_set_style_bg_opa(roller, LV_OPA_COVER, LV_PART_SELECTED | LV_STATE_DISABLED);
    ui_roller_feedback_set_selected(roller, lv_roller_get_selected(roller));

    if (cb != NULL) {
        lv_obj_add_event_cb(roller, cb, LV_EVENT_VALUE_CHANGED, user_data);
    }
    lv_obj_add_event_cb(roller, ui_roller_click_feedback_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(roller, ui_roller_click_feedback_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(roller, ui_roller_click_feedback_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(roller, ui_roller_click_feedback_event_cb, LV_EVENT_DELETE, NULL);

    return roller;
}

lv_obj_t *create_step_selector(lv_obj_t *parent,
                               lv_coord_t value_width,
                               lv_coord_t button_size,
                               lv_obj_t **value_label_out,
                               lv_event_cb_t prev_cb,
                               void *prev_user_data,
                               lv_event_cb_t next_cb,
                               void *next_user_data)
{
    lv_obj_t *row = create_row(parent);
    lv_obj_t *pill;
    lv_obj_t *label;

    lv_obj_set_width(row, LV_SIZE_CONTENT);
    center_row(row);
    lv_obj_set_style_pad_column(row, 18, 0);

    create_icon_circle_button(row, LV_SYMBOL_LEFT, button_size, prev_cb, prev_user_data);

    pill = lv_obj_create(row);
    lv_obj_set_size(pill, value_width, 86);
    lv_obj_set_style_radius(pill, 28, 0);
    lv_obj_set_style_bg_color(pill, lv_color_hex(0x2D2D2D), 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pill, 0, 0);
    lv_obj_set_style_pad_all(pill, 0, 0);
    lv_obj_set_style_shadow_width(pill, 0, 0);
    lv_obj_clear_flag(pill, LV_OBJ_FLAG_SCROLLABLE);

    label = lv_label_create(pill);
    lv_obj_set_width(label, lv_pct(100));
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_label_set_text(label, "");
    lv_obj_center(label);

    create_icon_circle_button(row, LV_SYMBOL_RIGHT, button_size, next_cb, next_user_data);

    if (value_label_out != NULL) {
        *value_label_out = label;
    }

    return row;
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
