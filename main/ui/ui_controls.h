#pragma once

#include "lvgl.h"

/* Plays the UI click on obj's event `code`. */
void ui_attach_click_feedback(lv_obj_t *obj, lv_event_code_t code);
