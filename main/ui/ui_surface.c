#include "ui/ui_surface.h"

#include "ui/ui_controls.h"

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
