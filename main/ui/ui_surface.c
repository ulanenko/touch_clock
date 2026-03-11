#include "ui/ui_surface.h"

void ui_surface_create_fullscreen(ui_surface_t *surface,
                                  lv_obj_t *parent,
                                  lv_color_t overlay_bg,
                                  lv_opa_t overlay_opa,
                                  lv_color_t panel_bg,
                                  lv_coord_t header_height,
                                  const char *title_text,
                                  lv_event_cb_t overlay_cb,
                                  lv_event_cb_t close_cb)
{
    lv_obj_t *close_label;

    surface->overlay = lv_obj_create(parent);
    lv_obj_set_size(surface->overlay, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(surface->overlay, overlay_bg, 0);
    lv_obj_set_style_bg_opa(surface->overlay, overlay_opa, 0);
    lv_obj_set_style_border_width(surface->overlay, 0, 0);
    lv_obj_set_style_radius(surface->overlay, 0, 0);
    lv_obj_set_style_pad_all(surface->overlay, 0, 0);
    lv_obj_add_flag(surface->overlay, LV_OBJ_FLAG_HIDDEN);
    if (overlay_cb != NULL) {
        lv_obj_add_event_cb(surface->overlay, overlay_cb, LV_EVENT_CLICKED, NULL);
    }

    surface->panel = lv_obj_create(surface->overlay);
    lv_obj_set_size(surface->panel, lv_pct(100), lv_pct(100));
    lv_obj_align(surface->panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(surface->panel, panel_bg, 0);
    lv_obj_set_style_bg_opa(surface->panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(surface->panel, 0, 0);
    lv_obj_set_style_radius(surface->panel, 0, 0);
    lv_obj_set_style_pad_all(surface->panel, 0, 0);
    lv_obj_set_layout(surface->panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(surface->panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(surface->panel, LV_OBJ_FLAG_SCROLLABLE);

    surface->header = lv_obj_create(surface->panel);
    lv_obj_set_width(surface->header, lv_pct(100));
    lv_obj_set_height(surface->header, header_height);
    lv_obj_set_style_bg_opa(surface->header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(surface->header, 0, 0);
    lv_obj_set_style_radius(surface->header, 0, 0);
    lv_obj_set_style_pad_left(surface->header, 28, 0);
    lv_obj_set_style_pad_right(surface->header, 28, 0);
    lv_obj_set_style_pad_top(surface->header, 18, 0);
    lv_obj_set_style_pad_bottom(surface->header, 0, 0);
    lv_obj_clear_flag(surface->header, LV_OBJ_FLAG_SCROLLABLE);

    surface->title = lv_label_create(surface->header);
    lv_obj_set_style_text_font(surface->title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(surface->title, lv_color_white(), 0);
    lv_obj_set_width(surface->title, 360);
    lv_obj_set_style_text_align(surface->title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(surface->title, title_text);
    lv_obj_align(surface->title, LV_ALIGN_TOP_MID, 0, 2);

    surface->close_btn = lv_button_create(surface->header);
    lv_obj_set_size(surface->close_btn, 54, 54);
    lv_obj_set_style_radius(surface->close_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(surface->close_btn, lv_color_hex(0x1D1D1D), 0);
    lv_obj_set_style_border_width(surface->close_btn, 0, 0);
    lv_obj_align(surface->close_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
    if (close_cb != NULL) {
        lv_obj_add_event_cb(surface->close_btn, close_cb, LV_EVENT_CLICKED, NULL);
    }
    close_label = lv_label_create(surface->close_btn);
    lv_label_set_text(close_label, LV_SYMBOL_CLOSE);
    lv_obj_center(close_label);

    surface->content = lv_obj_create(surface->panel);
    lv_obj_set_width(surface->content, lv_pct(100));
    lv_obj_set_flex_grow(surface->content, 1);
    lv_obj_set_style_bg_opa(surface->content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(surface->content, 0, 0);
    lv_obj_set_style_pad_left(surface->content, 24, 0);
    lv_obj_set_style_pad_right(surface->content, 24, 0);
    lv_obj_set_style_pad_bottom(surface->content, 24, 0);
    lv_obj_set_style_pad_row(surface->content, 18, 0);
    lv_obj_set_layout(surface->content, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(surface->content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(surface->content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(surface->content, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(surface->content, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_remove_flag(surface->content, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_set_scroll_snap_y(surface->content, LV_SCROLL_SNAP_NONE);
}

void ui_surface_create_edge_sensor(lv_obj_t *parent,
                                   lv_obj_t **sensor,
                                   ui_surface_edge_t edge,
                                   lv_coord_t thickness,
                                   lv_event_cb_t cb,
                                   void *user_data)
{
    *sensor = lv_obj_create(parent);
    if (edge == UI_SURFACE_EDGE_TOP || edge == UI_SURFACE_EDGE_BOTTOM) {
        lv_obj_set_size(*sensor, lv_pct(100), thickness);
    } else {
        lv_obj_set_size(*sensor, thickness, lv_pct(100));
    }

    switch (edge) {
    case UI_SURFACE_EDGE_TOP:
        lv_obj_align(*sensor, LV_ALIGN_TOP_MID, 0, 0);
        break;
    case UI_SURFACE_EDGE_BOTTOM:
        lv_obj_align(*sensor, LV_ALIGN_BOTTOM_MID, 0, 0);
        break;
    case UI_SURFACE_EDGE_LEFT:
        lv_obj_align(*sensor, LV_ALIGN_LEFT_MID, 0, 0);
        break;
    case UI_SURFACE_EDGE_RIGHT:
    default:
        lv_obj_align(*sensor, LV_ALIGN_RIGHT_MID, 0, 0);
        break;
    }

    lv_obj_set_style_bg_opa(*sensor, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(*sensor, 0, 0);
    lv_obj_set_style_radius(*sensor, 0, 0);
    lv_obj_set_style_pad_all(*sensor, 0, 0);
    lv_obj_set_scrollbar_mode(*sensor, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(*sensor, LV_OBJ_FLAG_SCROLLABLE);
    if (cb != NULL) {
        lv_obj_add_event_cb(*sensor, cb, LV_EVENT_PRESSED, user_data);
        lv_obj_add_event_cb(*sensor, cb, LV_EVENT_PRESSING, user_data);
        lv_obj_add_event_cb(*sensor, cb, LV_EVENT_RELEASED, user_data);
        lv_obj_add_event_cb(*sensor, cb, LV_EVENT_PRESS_LOST, user_data);
    }
}

bool ui_surface_edge_swipe_trigger(ui_surface_edge_t edge,
                                   const lv_point_t *start_point,
                                   const lv_point_t *point,
                                   int32_t trigger)
{
    int32_t dx = point->x - start_point->x;
    int32_t dy = point->y - start_point->y;

    switch (edge) {
    case UI_SURFACE_EDGE_TOP:
        return dy >= trigger;
    case UI_SURFACE_EDGE_BOTTOM:
        return dy <= -trigger;
    case UI_SURFACE_EDGE_LEFT:
        return dx >= trigger;
    case UI_SURFACE_EDGE_RIGHT:
        return dx <= -trigger;
    default:
        return false;
    }
}
