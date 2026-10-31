#include "ui/ui_controls.h"

#include "themes/lv_theme_private.h"

void ui_play_click_feedback(void);

static void ui_click_feedback_event_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    ui_play_click_feedback();
}

void ui_attach_click_feedback(lv_obj_t *obj, lv_event_code_t code)
{
    if (obj == NULL) {
        return;
    }

    lv_obj_add_event_cb(obj, ui_click_feedback_event_cb, code, NULL);
}

static lv_theme_t s_theme_overrides;
static lv_style_t s_button_pressed_style;

static void theme_overrides_apply_cb(lv_theme_t *theme, lv_obj_t *obj)
{
    LV_UNUSED(theme);

    if (lv_obj_check_type(obj, &lv_button_class)) {
        lv_obj_add_style(obj, &s_button_pressed_style, LV_STATE_PRESSED);
    }
}

/* The default theme darkens pressed buttons with recolor, which LVGL applies to the button and every
 * child it contains for each frame the finger is down (~74 ms frames while dragging from a large
 * button). Dimming with plain opa scales the draw descriptors instead and costs nothing extra. */
void ui_install_theme_overrides(lv_display_t *disp)
{
    lv_theme_t *base = lv_display_get_theme(disp);

    if (base == NULL || lv_display_get_theme(disp) == &s_theme_overrides) {
        return;
    }

    lv_style_init(&s_button_pressed_style);
    lv_style_set_recolor_opa(&s_button_pressed_style, LV_OPA_TRANSP);
    lv_style_set_opa(&s_button_pressed_style, LV_OPA_70);

    s_theme_overrides = *base;
    lv_theme_set_parent(&s_theme_overrides, base);
    lv_theme_set_apply_cb(&s_theme_overrides, theme_overrides_apply_cb);
    lv_display_set_theme(disp, &s_theme_overrides);
}
