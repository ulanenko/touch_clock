#pragma once

#include "lvgl.h"

void create_section_title(lv_obj_t *parent, const char *title, const char *subtitle);
lv_obj_t *create_card(lv_obj_t *parent);
lv_obj_t *create_row(lv_obj_t *parent);
lv_obj_t *create_action_button(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *user_data);
lv_obj_t *create_big_action_button(lv_obj_t *parent,
                                   const char *text,
                                   lv_color_t color,
                                   lv_event_cb_t cb,
                                   void *user_data);
lv_obj_t *create_dropdown(lv_obj_t *parent, const char *options, uint32_t width);
void set_action_button_text(lv_obj_t *button, const char *text);
void style_slider(lv_obj_t *slider);
void center_row(lv_obj_t *row);
