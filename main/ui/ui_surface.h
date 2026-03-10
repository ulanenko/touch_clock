#pragma once

#include <stdbool.h>

#include "lvgl.h"

typedef enum {
    UI_SURFACE_EDGE_TOP = 0,
    UI_SURFACE_EDGE_BOTTOM = 1,
    UI_SURFACE_EDGE_LEFT = 2,
    UI_SURFACE_EDGE_RIGHT = 3,
} ui_surface_edge_t;

typedef struct {
    lv_obj_t *overlay;
    lv_obj_t *panel;
    lv_obj_t *header;
    lv_obj_t *title;
    lv_obj_t *close_btn;
    lv_obj_t *content;
} ui_surface_t;

void ui_surface_create_fullscreen(ui_surface_t *surface,
                                  lv_obj_t *parent,
                                  lv_color_t overlay_bg,
                                  lv_opa_t overlay_opa,
                                  lv_color_t panel_bg,
                                  lv_coord_t header_height,
                                  const char *title,
                                  lv_event_cb_t overlay_cb,
                                  lv_event_cb_t close_cb);
void ui_surface_create_edge_sensor(lv_obj_t *parent,
                                   lv_obj_t **sensor,
                                   ui_surface_edge_t edge,
                                   lv_coord_t thickness,
                                   lv_event_cb_t cb,
                                   void *user_data);
bool ui_surface_edge_swipe_trigger(ui_surface_edge_t edge,
                                   const lv_point_t *start_point,
                                   const lv_point_t *point,
                                   int32_t trigger);
