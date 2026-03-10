static const char *s_day_caps[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
static const char *s_month_caps[12] = {
    "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
    "JUL", "AUG", "SEP", "OCT", "NOV", "DEC",
};
static const lv_coord_t s_digital_day_y_offsets[7] = {0, 0, 0, 0, 0, 0, 0};

static lv_obj_t *create_digital_segment_label(lv_obj_t *parent,
                                              const lv_font_t *font,
                                              lv_color_t color,
                                              lv_coord_t letter_space)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_letter_space(label, letter_space, 0);
    lv_label_set_text(label, "");
    return label;
}

static lv_obj_t *create_digital_text_label(lv_obj_t *parent,
                                           const lv_font_t *font,
                                           lv_color_t color,
                                           lv_opa_t opa,
                                           lv_coord_t letter_space)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_opa(label, opa, 0);
    lv_obj_set_style_text_letter_space(label, letter_space, 0);
    lv_label_set_text(label, "");
    return label;
}

static void apply_digital_italic(lv_obj_t *obj, int32_t skew)
{
    LV_UNUSED(obj);
    LV_UNUSED(skew);
}

static void digital_face_texture_draw_cb(lv_event_t *event)
{
    lv_obj_t *obj = lv_event_get_target_obj(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_area_t coords;
    lv_draw_line_dsc_t mesh_dsc;
    lv_draw_line_dsc_t accent_dsc;

    if (layer == NULL) {
        return;
    }

    lv_obj_get_coords(obj, &coords);

    lv_draw_line_dsc_init(&mesh_dsc);
    mesh_dsc.base.layer = layer;
    mesh_dsc.color = lv_color_hex(0x0A140A);
    mesh_dsc.opa = LV_OPA_20;
    mesh_dsc.width = 1;
    mesh_dsc.round_start = 0;
    mesh_dsc.round_end = 0;

    lv_draw_line_dsc_init(&accent_dsc);
    accent_dsc.base.layer = layer;
    accent_dsc.color = lv_color_hex(0x000000);
    accent_dsc.opa = 20;
    accent_dsc.width = 1;
    accent_dsc.round_start = 0;
    accent_dsc.round_end = 0;

    for (int y = coords.y1 + 1; y < coords.y2; y += 3) {
        mesh_dsc.p1.x = coords.x1;
        mesh_dsc.p1.y = y;
        mesh_dsc.p2.x = coords.x2;
        mesh_dsc.p2.y = y;
        lv_draw_line(layer, &mesh_dsc);
    }

    for (int x = coords.x1 + 1; x < coords.x2; x += 3) {
        mesh_dsc.p1.x = x;
        mesh_dsc.p1.y = coords.y1;
        mesh_dsc.p2.x = x;
        mesh_dsc.p2.y = coords.y2;
        lv_draw_line(layer, &mesh_dsc);
    }

    for (int y = coords.y1 + 1; y < coords.y2; y += 6) {
        accent_dsc.p1.x = coords.x1;
        accent_dsc.p1.y = y;
        accent_dsc.p2.x = coords.x2;
        accent_dsc.p2.y = y;
        lv_draw_line(layer, &accent_dsc);
    }

    for (int x = coords.x1 + 1; x < coords.x2; x += 6) {
        accent_dsc.p1.x = x;
        accent_dsc.p1.y = coords.y1;
        accent_dsc.p2.x = x;
        accent_dsc.p2.y = coords.y2;
        lv_draw_line(layer, &accent_dsc);
    }
}

static void make_face_layer_passive(lv_obj_t *obj)
{
    uint32_t child_count;

    if (obj == NULL) {
        return;
    }

    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_GESTURE_BUBBLE);

    child_count = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < child_count; ++i) {
        make_face_layer_passive(lv_obj_get_child(obj, i));
    }
}

static void digital_face_swipe_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_indev_t *indev = lv_indev_active();
    lv_point_t point;
    lv_coord_t dx;
    lv_coord_t dy;
    clock_face_id_t current_face;
    clock_face_id_t target_face;

    if (indev == NULL) {
        s_ui.faces.digital_swipe_tracking = false;
        return;
    }

    if (code == LV_EVENT_PRESSED) {
        lv_indev_get_point(indev, &s_ui.faces.digital_swipe_start_point);
        s_ui.faces.digital_swipe_tracking = true;
        return;
    }

    if (code == LV_EVENT_PRESS_LOST) {
        s_ui.faces.digital_swipe_tracking = false;
        return;
    }

    if (code != LV_EVENT_RELEASED || !s_ui.faces.digital_swipe_tracking) {
        return;
    }

    s_ui.faces.digital_swipe_tracking = false;
    lv_indev_get_point(indev, &point);
    dx = point.x - s_ui.faces.digital_swipe_start_point.x;
    dy = point.y - s_ui.faces.digital_swipe_start_point.y;

    if (LV_ABS(dx) < 56 || LV_ABS(dx) <= LV_ABS(dy) + 20) {
        return;
    }

    current_face = s_ui.runtime->in_night_mode ? s_ui.settings->night_mode.face
                                               : tile_to_face(lv_tileview_get_tile_active(s_ui.tileview));
    target_face = current_face;
    if (dx < 0 && current_face < (CLOCK_FACE_COUNT - 1)) {
        target_face = (clock_face_id_t)(current_face + 1);
    } else if (dx > 0 && current_face > 0) {
        target_face = (clock_face_id_t)(current_face - 1);
    }

    if (target_face == current_face) {
        return;
    }

    set_active_face(target_face, LV_ANIM_ON);
    show_affordances_temporarily();
    if (!s_ui.runtime->in_night_mode && s_ui.settings->current_face != target_face) {
        s_ui.settings->current_face = target_face;
        notify_settings_changed();
    }
}

static void refresh_digital_face_snapshot(void)
{
    lv_coord_t ext_draw;

    if (s_ui.faces.digital_live_root == NULL || s_ui.faces.digital_snapshot_img == NULL) {
        return;
    }

    lv_obj_clear_flag(s_ui.faces.digital_live_root, LV_OBJ_FLAG_HIDDEN);

    if (s_ui.faces.digital_snapshot_buf == NULL) {
        s_ui.faces.digital_snapshot_buf = lv_snapshot_create_draw_buf(s_ui.faces.digital_live_root,
                                                                      LV_COLOR_FORMAT_RGB565);
        if (s_ui.faces.digital_snapshot_buf == NULL) {
            lv_obj_add_flag(s_ui.faces.digital_live_root, LV_OBJ_FLAG_HIDDEN);
            return;
        }
    }

    if (lv_snapshot_take_to_draw_buf(s_ui.faces.digital_live_root,
                                     LV_COLOR_FORMAT_RGB565,
                                     s_ui.faces.digital_snapshot_buf) != LV_RESULT_OK) {
        lv_obj_add_flag(s_ui.faces.digital_live_root, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    ext_draw = (lv_coord_t)((int32_t)s_ui.faces.digital_snapshot_buf->header.w - SCREEN_SIZE) / 2;
    if (ext_draw < 0) {
        ext_draw = 0;
    }
    s_ui.faces.digital_snapshot_ext_draw = ext_draw;
    lv_image_set_src(s_ui.faces.digital_snapshot_img, s_ui.faces.digital_snapshot_buf);
    lv_obj_set_pos(s_ui.faces.digital_snapshot_img, -ext_draw, -ext_draw);
    lv_obj_clear_flag(s_ui.faces.digital_snapshot_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_ui.faces.digital_live_root, LV_OBJ_FLAG_HIDDEN);
}

static void create_digital_face(lv_obj_t *parent)
{
    lv_obj_t *clock_center;
    lv_obj_t *time_holder;
    lv_obj_t *side_holder;
    lv_obj_t *days_bar;

    lv_obj_set_style_bg_color(parent, lv_color_hex(0x010401), 0);
    lv_obj_set_style_bg_grad_opa(parent, LV_OPA_TRANSP, 0);
    s_ui.faces.digital_glow = NULL;
    s_ui.faces.digital_snapshot_buf = NULL;
    s_ui.faces.digital_snapshot_ext_draw = 0;
    s_ui.faces.digital_snapshot_img = lv_image_create(parent);
    lv_obj_set_style_bg_opa(s_ui.faces.digital_snapshot_img, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ui.faces.digital_snapshot_img, 0, 0);
    lv_obj_set_style_pad_all(s_ui.faces.digital_snapshot_img, 0, 0);
    lv_obj_clear_flag(s_ui.faces.digital_snapshot_img, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_ui.faces.digital_snapshot_img, LV_OBJ_FLAG_HIDDEN);
    s_ui.faces.digital_live_root = lv_obj_create(parent);
    lv_obj_set_size(s_ui.faces.digital_live_root, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_color(s_ui.faces.digital_live_root, lv_color_hex(0x010401), 0);
    lv_obj_set_style_bg_grad_opa(s_ui.faces.digital_live_root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ui.faces.digital_live_root, 0, 0);
    lv_obj_set_style_pad_all(s_ui.faces.digital_live_root, 0, 0);
    lv_obj_clear_flag(s_ui.faces.digital_live_root, LV_OBJ_FLAG_SCROLLABLE);
    s_ui.faces.digital_swipe_layer = lv_obj_create(parent);
    lv_obj_set_size(s_ui.faces.digital_swipe_layer, SCREEN_SIZE, SCREEN_SIZE - BOTTOM_EDGE_ZONE - 8);
    lv_obj_align(s_ui.faces.digital_swipe_layer, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(s_ui.faces.digital_swipe_layer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ui.faces.digital_swipe_layer, 0, 0);
    lv_obj_set_style_radius(s_ui.faces.digital_swipe_layer, 0, 0);
    lv_obj_set_style_pad_all(s_ui.faces.digital_swipe_layer, 0, 0);
    lv_obj_clear_flag(s_ui.faces.digital_swipe_layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_ui.faces.digital_swipe_layer, digital_face_swipe_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_ui.faces.digital_swipe_layer, digital_face_swipe_event_cb, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(s_ui.faces.digital_swipe_layer, digital_face_swipe_event_cb, LV_EVENT_PRESS_LOST, NULL);

    clock_center = lv_obj_create(s_ui.faces.digital_live_root);
    lv_obj_set_size(clock_center, 664, 328);
    lv_obj_set_style_bg_opa(clock_center, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(clock_center, 0, 0);
    lv_obj_set_style_pad_all(clock_center, 0, 0);
    lv_obj_clear_flag(clock_center, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(clock_center, LV_ALIGN_CENTER, 0, -76);

    time_holder = lv_obj_create(clock_center);
    lv_obj_set_size(time_holder, 566, 194);
    lv_obj_set_style_bg_opa(time_holder, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(time_holder, 0, 0);
    lv_obj_set_style_pad_all(time_holder, 0, 0);
    lv_obj_clear_flag(time_holder, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(time_holder, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    s_ui.faces.digital_time_bg = create_digital_segment_label(time_holder, &seven_segment_font_112, lv_color_hex(0x143C14), 8);
    lv_label_set_text(s_ui.faces.digital_time_bg, "88:88");
    lv_obj_align(s_ui.faces.digital_time_bg, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_text_opa(s_ui.faces.digital_time_bg, LV_OPA_40, 0);
    apply_digital_italic(s_ui.faces.digital_time_bg, -120);

    s_ui.faces.digital_time_glow = create_digital_segment_label(time_holder, &seven_segment_font_112, lv_color_hex(0x5CFB5C), 8);
    lv_obj_align(s_ui.faces.digital_time_glow, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_text_opa(s_ui.faces.digital_time_glow, 30, 0);
    apply_digital_italic(s_ui.faces.digital_time_glow, -120);

    s_ui.faces.digital_time_fg = create_digital_segment_label(time_holder, &seven_segment_font_112, lv_color_hex(0x5CFB5C), 8);
    lv_obj_align(s_ui.faces.digital_time_fg, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    apply_digital_italic(s_ui.faces.digital_time_fg, -120);

    side_holder = lv_obj_create(clock_center);
    lv_obj_set_size(side_holder, 134, 206);
    lv_obj_set_style_bg_opa(side_holder, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(side_holder, 0, 0);
    lv_obj_set_style_pad_all(side_holder, 0, 0);
    lv_obj_clear_flag(side_holder, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align_to(side_holder, time_holder, LV_ALIGN_OUT_RIGHT_BOTTOM, -58, -4);

    s_ui.faces.digital_ampm_glow = NULL;

    s_ui.faces.digital_ampm_label = create_digital_text_label(side_holder, &dseg14_classic_italic_36, lv_color_hex(0x5CFB5C), LV_OPA_80, 2);
    lv_label_set_text(s_ui.faces.digital_ampm_label, "PM");
    lv_obj_align(s_ui.faces.digital_ampm_label, LV_ALIGN_TOP_RIGHT, 0, 0);
    apply_digital_italic(s_ui.faces.digital_ampm_label, -80);

    s_ui.faces.digital_seconds_bg = create_digital_segment_label(side_holder, &seven_segment_font_56, lv_color_hex(0x143C14), 4);
    lv_label_set_text(s_ui.faces.digital_seconds_bg, "88");
    lv_obj_align(s_ui.faces.digital_seconds_bg, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_text_opa(s_ui.faces.digital_seconds_bg, LV_OPA_40, 0);
    apply_digital_italic(s_ui.faces.digital_seconds_bg, -120);

    s_ui.faces.digital_seconds_glow = create_digital_segment_label(side_holder, &seven_segment_font_56, lv_color_hex(0x5CFB5C), 4);
    lv_obj_align(s_ui.faces.digital_seconds_glow, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_text_opa(s_ui.faces.digital_seconds_glow, 30, 0);
    apply_digital_italic(s_ui.faces.digital_seconds_glow, -120);

    s_ui.faces.digital_seconds_fg = create_digital_segment_label(side_holder, &seven_segment_font_56, lv_color_hex(0x5CFB5C), 4);
    lv_obj_align(s_ui.faces.digital_seconds_fg, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    apply_digital_italic(s_ui.faces.digital_seconds_fg, -120);

    days_bar = lv_obj_create(s_ui.faces.digital_live_root);
    lv_obj_set_size(days_bar, 580, 36);
    lv_obj_set_style_bg_opa(days_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(days_bar, 0, 0);
    lv_obj_set_style_pad_all(days_bar, 0, 0);
    lv_obj_clear_flag(days_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(days_bar, LV_ALIGN_BOTTOM_MID, 0, -136);

    for (int i = 0; i < 7; ++i) {
        s_ui.faces.digital_day_glow[i] = NULL;

        s_ui.faces.digital_day_label[i] = create_digital_text_label(days_bar, &dseg14_classic_italic_20, lv_color_hex(0x143C14), LV_OPA_40, 1);
        lv_label_set_text(s_ui.faces.digital_day_label[i], s_day_caps[i]);
        apply_digital_italic(s_ui.faces.digital_day_label[i], -60);
    }

    s_ui.faces.digital_date_glow = NULL;

    s_ui.faces.digital_date_label = create_digital_text_label(s_ui.faces.digital_live_root, &dseg14_classic_italic_24, lv_color_hex(0x5CFB5C), LV_OPA_90, 1);
    lv_label_set_text(s_ui.faces.digital_date_label, "SUN, FEB 11");
    lv_obj_align(s_ui.faces.digital_date_label, LV_ALIGN_TOP_MID, 0, 98);
    apply_digital_italic(s_ui.faces.digital_date_label, -60);

    make_face_layer_passive(clock_center);
    make_face_layer_passive(days_bar);
    make_face_layer_passive(s_ui.faces.digital_date_label);
    make_face_layer_passive(s_ui.faces.digital_live_root);
    lv_obj_move_foreground(s_ui.faces.digital_swipe_layer);
}

static void update_digital_face(void)
{
    struct tm ti = get_local_time_now();
    char buf_time[16];
    char buf_seconds[8];
    char buf_date[32];
    int hour12 = ti.tm_hour % 12;

    if (hour12 == 0) {
        hour12 = 12;
    }

    snprintf(buf_time, sizeof(buf_time), "%2d:%02d", hour12, ti.tm_min);
    snprintf(buf_seconds, sizeof(buf_seconds), "%02d", ti.tm_sec);
    snprintf(buf_date, sizeof(buf_date), "%s, %s %d",
             s_day_caps[ti.tm_wday],
             s_month_caps[ti.tm_mon],
             ti.tm_mday);

    if (s_ui.faces.digital_time_glow != NULL) {
        lv_label_set_text(s_ui.faces.digital_time_glow, buf_time);
    }
    lv_label_set_text(s_ui.faces.digital_time_fg, buf_time);
    if (s_ui.faces.digital_seconds_glow != NULL) {
        lv_label_set_text(s_ui.faces.digital_seconds_glow, buf_seconds);
    }
    lv_label_set_text(s_ui.faces.digital_seconds_fg, buf_seconds);
    if (s_ui.faces.digital_ampm_glow != NULL) {
        lv_label_set_text(s_ui.faces.digital_ampm_glow, (ti.tm_hour >= 12) ? "PM" : "AM");
    }
    lv_label_set_text(s_ui.faces.digital_ampm_label, (ti.tm_hour >= 12) ? "PM" : "AM");
    if (s_ui.faces.digital_date_glow != NULL) {
        lv_label_set_text(s_ui.faces.digital_date_glow, buf_date);
        lv_obj_align(s_ui.faces.digital_date_glow, LV_ALIGN_TOP_MID, 0, 98);
    }
    lv_label_set_text(s_ui.faces.digital_date_label, buf_date);
    lv_obj_align(s_ui.faces.digital_date_label, LV_ALIGN_TOP_MID, 0, 98);

    for (int i = 0; i < 7; ++i) {
        bool active = (i == ti.tm_wday);
        lv_coord_t slot_w = 580 / 7;
        lv_coord_t x = (lv_coord_t)(i * slot_w + slot_w / 2);

        if (s_ui.faces.digital_day_glow[i] != NULL) {
            lv_obj_set_style_text_color(s_ui.faces.digital_day_glow[i], lv_color_hex(0x5CFB5C), 0);
            lv_obj_set_style_text_opa(s_ui.faces.digital_day_glow[i], active ? 28 : 0, 0);
            lv_obj_align(s_ui.faces.digital_day_glow[i],
                         LV_ALIGN_TOP_LEFT,
                         x - lv_obj_get_width(s_ui.faces.digital_day_glow[i]) / 2,
                         s_digital_day_y_offsets[i]);
        }
        lv_obj_set_style_text_color(s_ui.faces.digital_day_label[i],
                                    active ? lv_color_hex(0x5CFB5C) : lv_color_hex(0x143C14),
                                    0);
        lv_obj_set_style_text_opa(s_ui.faces.digital_day_label[i],
                                  active ? LV_OPA_COVER : LV_OPA_40,
                                  0);
        lv_obj_align(s_ui.faces.digital_day_label[i],
                     LV_ALIGN_TOP_LEFT,
                     x - lv_obj_get_width(s_ui.faces.digital_day_label[i]) / 2,
                     s_digital_day_y_offsets[i]);
    }

    refresh_digital_face_snapshot();
}

static void update_face(clock_face_id_t face)
{
    switch (face) {
    case CLOCK_FACE_DIGITAL:
        update_digital_face();
        break;
    case CLOCK_FACE_MATRIX:
        update_matrix_face();
        break;
    case CLOCK_FACE_WHARTON:
        update_wharton_face();
        break;
    case CLOCK_FACE_SLAVA:
        update_slava_face();
        break;
    case CLOCK_FACE_SLAVA_DARK:
        update_slava_dark_face();
        break;
    default:
        update_digital_face();
        break;
    }
}

static void build_matrix_state(void)
{
    struct tm ti = get_local_time_now();
    int digits[4] = {ti.tm_hour / 10, ti.tm_hour % 10, ti.tm_min / 10, ti.tm_min % 10};

    memset(s_ui.faces.matrix_on, 0, sizeof(s_ui.faces.matrix_on));

    for (int d = 0; d < 4; ++d) {
        const uint8_t *glyph = s_matrix_font[digits[d]];
        int col0 = s_matrix_digit_col[d];

        for (int r = 0; r < MTX_DIGIT_H; ++r) {
            for (int c = 0; c < 5; ++c) {
                if ((glyph[r] >> (4 - c)) & 1U) {
                    s_ui.faces.matrix_on[col0 + c][s_matrix_digit_row0 + r] = true;
                }
            }
        }
    }

    if ((ti.tm_sec & 1) == 0) {
        s_ui.faces.matrix_on[s_matrix_colon_col][s_matrix_digit_row0 + 2] = true;
        s_ui.faces.matrix_on[s_matrix_colon_col][s_matrix_digit_row0 + 4] = true;
    }
}

static void create_matrix_face(lv_obj_t *parent)
{
    int total_w = (MTX_GRID_X - 1) * MTX_PITCH + MTX_DOT_SIZE;
    int total_h = (MTX_GRID_Y - 1) * MTX_PITCH + MTX_DOT_SIZE;

    lv_obj_set_style_bg_color(parent, lv_color_hex(0x050505), 0);
    s_ui.faces.matrix_x0 = (SCREEN_SIZE - total_w) / 2;
    s_ui.faces.matrix_y0 = (SCREEN_SIZE - total_h) / 2;

    if (s_ui.faces.matrix_face_buf == NULL) {
        s_ui.faces.matrix_face_buf = heap_caps_malloc(SCREEN_SIZE * SCREEN_SIZE * sizeof(lv_color16_t), MALLOC_CAP_SPIRAM);
    }

    s_ui.faces.matrix_face_obj = lv_canvas_create(parent);
    lv_canvas_set_buffer(s_ui.faces.matrix_face_obj, s_ui.faces.matrix_face_buf, SCREEN_SIZE, SCREEN_SIZE, LV_COLOR_FORMAT_RGB565);
    lv_obj_clear_flag(s_ui.faces.matrix_face_obj, LV_OBJ_FLAG_SCROLLABLE);
}

static void update_matrix_face(void)
{
    lv_layer_t layer;
    lv_draw_rect_dsc_t dsc;

    build_matrix_state();
    lv_canvas_fill_bg(s_ui.faces.matrix_face_obj, lv_color_hex(0x050505), LV_OPA_COVER);
    lv_canvas_init_layer(s_ui.faces.matrix_face_obj, &layer);
    lv_draw_rect_dsc_init(&dsc);
    dsc.radius = MTX_DOT_RAD;
    dsc.bg_opa = LV_OPA_COVER;
    dsc.border_width = 0;
    dsc.shadow_width = 0;
    dsc.outline_width = 0;

    for (int c = 0; c < MTX_GRID_X; ++c) {
        for (int r = 0; r < MTX_GRID_Y; ++r) {
            lv_area_t area;

            area.x1 = s_ui.faces.matrix_x0 + c * MTX_PITCH;
            area.y1 = s_ui.faces.matrix_y0 + r * MTX_PITCH;
            area.x2 = area.x1 + MTX_DOT_SIZE - 1;
            area.y2 = area.y1 + MTX_DOT_SIZE - 1;
            dsc.bg_color = lv_color_hex(s_ui.faces.matrix_on[c][r] ? MTX_COL_ON : MTX_COL_OFF);
            lv_draw_rect(&layer, &dsc, &area);
        }
    }

    lv_canvas_finish_layer(s_ui.faces.matrix_face_obj, &layer);
}

static void build_wharton_state(void)
{
    struct tm ti = get_local_time_now();
    int digits[4] = {ti.tm_hour / 10, ti.tm_hour % 10, ti.tm_min / 10, ti.tm_min % 10};

    for (int d = 0; d < 4; ++d) {
        const uint8_t *glyph = s_wharton_font[digits[d]];
        for (int r = 0; r < WH_DIGIT_ROWS; ++r) {
            for (int c = 0; c < WH_DIGIT_COLS; ++c) {
                s_ui.faces.wharton_digit_dots[d][c][r] = ((glyph[r] >> (4 - c)) & 1U) != 0;
            }
        }
    }

    s_ui.faces.wharton_colon_on = ((ti.tm_sec & 1) == 0);
    s_ui.faces.wharton_second_count = ti.tm_sec;
}

static void create_wharton_face(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, lv_color_black(), 0);

    if (s_ui.faces.wharton_face_buf == NULL) {
        s_ui.faces.wharton_face_buf = heap_caps_malloc(SCREEN_SIZE * SCREEN_SIZE * sizeof(lv_color16_t), MALLOC_CAP_SPIRAM);
    }

    s_ui.faces.wharton_face_obj = lv_canvas_create(parent);
    lv_canvas_set_buffer(s_ui.faces.wharton_face_obj, s_ui.faces.wharton_face_buf, SCREEN_SIZE, SCREEN_SIZE, LV_COLOR_FORMAT_RGB565);
    lv_obj_clear_flag(s_ui.faces.wharton_face_obj, LV_OBJ_FLAG_SCROLLABLE);
}

static void update_wharton_face(void)
{
    lv_layer_t layer;
    lv_draw_rect_dsc_t dsc;
    int digit_w;
    int colon_w;
    int pair_gap;
    int colon_gap;
    int total_w;
    int digit_h;
    int base_x;
    int base_y;
    int digit_x[4];
    int colon_x;

    build_wharton_state();
    lv_canvas_fill_bg(s_ui.faces.wharton_face_obj, lv_color_black(), LV_OPA_COVER);
    lv_canvas_init_layer(s_ui.faces.wharton_face_obj, &layer);
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_opa = LV_OPA_COVER;
    dsc.border_width = 0;
    dsc.shadow_width = 0;
    dsc.outline_width = 0;

    for (int i = 0; i < WH_RING_DOTS; ++i) {
        float angle = (i * 6.0f - 90.0f) * (M_PI / 180.0f);
        int cx = CENTER + (int)(WH_RING_R * cosf(angle));
        int cy = CENTER + (int)(WH_RING_R * sinf(angle));
        int radius = (i < s_ui.faces.wharton_second_count) ? WH_DOT_LIT_R : WH_DOT_DIM_R;
        lv_area_t area;

        area.x1 = cx - radius;
        area.y1 = cy - radius;
        area.x2 = cx + radius;
        area.y2 = cy + radius;
        dsc.radius = radius;
        dsc.bg_color = lv_color_hex((i < s_ui.faces.wharton_second_count) ? WH_COL_ON : WH_COL_OFF);
        lv_draw_rect(&layer, &dsc, &area);
    }

    digit_w = WH_DIGIT_COLS * WH_DOT_PITCH - WH_DOT_GAP;
    colon_w = WH_DOT_SZ;
    pair_gap = WH_DOT_PITCH;
    colon_gap = WH_DOT_PITCH + 2;
    total_w = 4 * digit_w + 2 * pair_gap + colon_w + 2 * colon_gap;
    digit_h = WH_DIGIT_ROWS * WH_DOT_PITCH - WH_DOT_GAP;
    base_x = (SCREEN_SIZE - total_w) / 2;
    base_y = (SCREEN_SIZE - digit_h) / 2 - 8;
    digit_x[0] = base_x;
    digit_x[1] = digit_x[0] + digit_w + pair_gap;
    colon_x = digit_x[1] + digit_w + colon_gap;
    digit_x[2] = colon_x + colon_w + colon_gap;
    digit_x[3] = digit_x[2] + digit_w + pair_gap;

    dsc.radius = WH_DOT_SZ / 2;
    for (int d = 0; d < 4; ++d) {
        for (int c = 0; c < WH_DIGIT_COLS; ++c) {
            for (int r = 0; r < WH_DIGIT_ROWS; ++r) {
                lv_area_t area;
                area.x1 = digit_x[d] + c * WH_DOT_PITCH;
                area.y1 = base_y + r * WH_DOT_PITCH;
                area.x2 = area.x1 + WH_DOT_SZ - 1;
                area.y2 = area.y1 + WH_DOT_SZ - 1;
                dsc.bg_color = lv_color_hex(s_ui.faces.wharton_digit_dots[d][c][r] ? WH_COL_ON : WH_COL_OFF);
                lv_draw_rect(&layer, &dsc, &area);
            }
        }
    }

    dsc.bg_color = lv_color_hex(s_ui.faces.wharton_colon_on ? WH_COL_ON : WH_COL_OFF);
    for (int i = 0; i < 2; ++i) {
        lv_area_t area;
        int colon_y = base_y + ((i == 0) ? 2 : 4) * WH_DOT_PITCH;
        area.x1 = colon_x;
        area.y1 = colon_y;
        area.x2 = colon_x + WH_DOT_SZ - 1;
        area.y2 = colon_y + WH_DOT_SZ - 1;
        lv_draw_rect(&layer, &dsc, &area);
    }

    lv_canvas_finish_layer(s_ui.faces.wharton_face_obj, &layer);
}

static void draw_segment_bar(lv_layer_t *layer, int x1, int y1, int x2, int y2, bool on)
{
    lv_draw_line_dsc_t dsc;

    lv_draw_line_dsc_init(&dsc);
    dsc.round_start = 1;
    dsc.round_end = 1;
    dsc.p1.x = x1;
    dsc.p1.y = y1;
    dsc.p2.x = x2;
    dsc.p2.y = y2;

    if (on) {
        dsc.color = lv_color_hex(SEG_COL_GLOW);
        dsc.width = SEG_DIGIT_THICK + 18;
        dsc.opa = LV_OPA_30;
        lv_draw_line(layer, &dsc);

        dsc.color = lv_color_hex(SEG_COL_ON);
        dsc.width = SEG_DIGIT_THICK;
        dsc.opa = LV_OPA_COVER;
        lv_draw_line(layer, &dsc);

        dsc.color = lv_color_hex(SEG_COL_HIGHLIGHT);
        dsc.width = SEG_DIGIT_THICK / 3;
        dsc.opa = LV_OPA_70;
        lv_draw_line(layer, &dsc);
    } else {
        dsc.color = lv_color_hex(SEG_COL_OFF);
        dsc.width = SEG_DIGIT_THICK;
        dsc.opa = LV_OPA_80;
        lv_draw_line(layer, &dsc);
    }
}

static void draw_segment_colon_dot(lv_layer_t *layer, int cx, int cy, bool on)
{
    lv_draw_rect_dsc_t dsc;
    lv_area_t area;
    int glow_r = SEG_COLON_SIZE / 2 + 8;
    int dot_r = SEG_COLON_SIZE / 2;

    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_opa = LV_OPA_COVER;
    dsc.border_width = 0;
    dsc.shadow_width = 0;
    dsc.outline_width = 0;

    area.x1 = cx - glow_r;
    area.y1 = cy - glow_r;
    area.x2 = cx + glow_r;
    area.y2 = cy + glow_r;
    dsc.radius = glow_r;
    dsc.bg_color = lv_color_hex(on ? SEG_COL_GLOW : SEG_COL_OFF);
    dsc.bg_opa = on ? LV_OPA_20 : LV_OPA_60;
    lv_draw_rect(layer, &dsc, &area);

    area.x1 = cx - dot_r;
    area.y1 = cy - dot_r;
    area.x2 = cx + dot_r;
    area.y2 = cy + dot_r;
    dsc.radius = dot_r;
    dsc.bg_color = lv_color_hex(on ? SEG_COL_ON : SEG_COL_OFF);
    dsc.bg_opa = on ? LV_OPA_COVER : LV_OPA_80;
    lv_draw_rect(layer, &dsc, &area);
}

static void draw_segment_digit(lv_layer_t *layer, int x, int y, uint8_t mask)
{
    int left = x;
    int right = x + SEG_DIGIT_W;
    int top = y;
    int middle = y + SEG_DIGIT_H / 2;
    int bottom = y + SEG_DIGIT_H;
    int inner_left = x + SEG_DIGIT_THICK / 2;
    int inner_right = x + SEG_DIGIT_W - SEG_DIGIT_THICK / 2;
    int upper_top = y + SEG_DIGIT_THICK / 2 + 6;
    int upper_bottom = middle - SEG_DIGIT_THICK / 2 - 6;
    int lower_top = middle + SEG_DIGIT_THICK / 2 + 6;
    int lower_bottom = bottom - SEG_DIGIT_THICK / 2 - 6;

    draw_segment_bar(layer, inner_left, top, inner_right, top, (mask & 0x01U) != 0);
    draw_segment_bar(layer, right, upper_top, right, upper_bottom, (mask & 0x02U) != 0);
    draw_segment_bar(layer, right, lower_top, right, lower_bottom, (mask & 0x04U) != 0);
    draw_segment_bar(layer, inner_left, bottom, inner_right, bottom, (mask & 0x08U) != 0);
    draw_segment_bar(layer, left, lower_top, left, lower_bottom, (mask & 0x10U) != 0);
    draw_segment_bar(layer, left, upper_top, left, upper_bottom, (mask & 0x20U) != 0);
    draw_segment_bar(layer, inner_left, middle, inner_right, middle, (mask & 0x40U) != 0);
}

static void create_seven_segment_face(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x020403), 0);
    lv_obj_set_style_bg_grad_color(parent, lv_color_hex(0x0B1410), 0);
    lv_obj_set_style_bg_grad_dir(parent, LV_GRAD_DIR_VER, 0);

    s_ui.faces.segment_panel = lv_obj_create(parent);
    lv_obj_set_size(s_ui.faces.segment_panel, SEG_PANEL_W, SEG_PANEL_H);
    lv_obj_set_style_radius(s_ui.faces.segment_panel, 34, 0);
    lv_obj_set_style_bg_color(s_ui.faces.segment_panel, lv_color_hex(SEG_PANEL_BG), 0);
    lv_obj_set_style_bg_opa(s_ui.faces.segment_panel, LV_OPA_90, 0);
    lv_obj_set_style_border_width(s_ui.faces.segment_panel, 2, 0);
    lv_obj_set_style_border_color(s_ui.faces.segment_panel, lv_color_hex(0x1E6A44), 0);
    lv_obj_set_style_shadow_width(s_ui.faces.segment_panel, 42, 0);
    lv_obj_set_style_shadow_spread(s_ui.faces.segment_panel, 0, 0);
    lv_obj_set_style_shadow_color(s_ui.faces.segment_panel, lv_color_hex(0x1D8D56), 0);
    lv_obj_set_style_shadow_opa(s_ui.faces.segment_panel, LV_OPA_20, 0);
    lv_obj_set_style_pad_all(s_ui.faces.segment_panel, 0, 0);
    lv_obj_set_style_outline_width(s_ui.faces.segment_panel, 0, 0);
    lv_obj_clear_flag(s_ui.faces.segment_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(s_ui.faces.segment_panel, LV_ALIGN_CENTER, 0, -28);

    if (s_ui.faces.segment_face_buf == NULL) {
        s_ui.faces.segment_face_buf = heap_caps_malloc(SEG_CANVAS_W * SEG_CANVAS_H * sizeof(lv_color16_t), MALLOC_CAP_SPIRAM);
    }

    s_ui.faces.segment_face_obj = lv_canvas_create(s_ui.faces.segment_panel);
    lv_canvas_set_buffer(s_ui.faces.segment_face_obj, s_ui.faces.segment_face_buf, SEG_CANVAS_W, SEG_CANVAS_H, LV_COLOR_FORMAT_RGB565);
    lv_obj_clear_flag(s_ui.faces.segment_face_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(s_ui.faces.segment_face_obj);

    s_ui.faces.segment_date_label = lv_label_create(parent);
    lv_obj_set_style_text_font(s_ui.faces.segment_date_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_ui.faces.segment_date_label, lv_color_hex(0x8FD7AF), 0);
    lv_obj_set_style_text_letter_space(s_ui.faces.segment_date_label, 6, 0);
    lv_label_set_text(s_ui.faces.segment_date_label, "--- -- ---");
    lv_obj_align_to(s_ui.faces.segment_date_label, s_ui.faces.segment_panel, LV_ALIGN_OUT_BOTTOM_MID, 0, 24);
}

static void update_seven_segment_face(void)
{
    struct tm ti = get_local_time_now();
    lv_layer_t layer;
    lv_draw_rect_dsc_t frame_dsc;
    char date_buf[32];
    int digits[4] = {ti.tm_hour / 10, ti.tm_hour % 10, ti.tm_min / 10, ti.tm_min % 10};
    int total_w = 4 * SEG_DIGIT_W + 2 * SEG_DIGIT_GAP + SEG_DIGIT_PAIR_GAP + 2 * SEG_COLON_GAP + SEG_COLON_SIZE;
    int start_x = (SEG_CANVAS_W - total_w) / 2;
    int y = (SEG_CANVAS_H - SEG_DIGIT_H) / 2;
    int digit_x[4];
    int colon_x;
    lv_area_t frame_area = {
        .x1 = 10,
        .y1 = 10,
        .x2 = SEG_CANVAS_W - 11,
        .y2 = SEG_CANVAS_H - 11,
    };

    digit_x[0] = start_x;
    digit_x[1] = digit_x[0] + SEG_DIGIT_W + SEG_DIGIT_GAP;
    colon_x = digit_x[1] + SEG_DIGIT_W + SEG_COLON_GAP + SEG_COLON_SIZE / 2;
    digit_x[2] = digit_x[1] + SEG_DIGIT_W + 2 * SEG_COLON_GAP + SEG_COLON_SIZE + SEG_DIGIT_PAIR_GAP;
    digit_x[3] = digit_x[2] + SEG_DIGIT_W + SEG_DIGIT_GAP;

    lv_canvas_fill_bg(s_ui.faces.segment_face_obj, lv_color_hex(SEG_CANVAS_BG), LV_OPA_COVER);
    lv_canvas_init_layer(s_ui.faces.segment_face_obj, &layer);

    lv_draw_rect_dsc_init(&frame_dsc);
    frame_dsc.bg_opa = LV_OPA_TRANSP;
    frame_dsc.border_width = 2;
    frame_dsc.border_opa = LV_OPA_20;
    frame_dsc.border_color = lv_color_hex(0x24553A);
    frame_dsc.radius = 28;
    frame_dsc.shadow_width = 0;
    frame_dsc.outline_width = 0;
    lv_draw_rect(&layer, &frame_dsc, &frame_area);

    draw_segment_digit(&layer, digit_x[0], y, s_segment_font[digits[0]]);
    draw_segment_digit(&layer, digit_x[1], y, s_segment_font[digits[1]]);
    draw_segment_colon_dot(&layer, colon_x, y + SEG_DIGIT_H / 2 - 34, (ti.tm_sec & 1) == 0);
    draw_segment_colon_dot(&layer, colon_x, y + SEG_DIGIT_H / 2 + 34, (ti.tm_sec & 1) == 0);
    draw_segment_digit(&layer, digit_x[2], y, s_segment_font[digits[2]]);
    draw_segment_digit(&layer, digit_x[3], y, s_segment_font[digits[3]]);

    lv_canvas_finish_layer(s_ui.faces.segment_face_obj, &layer);

    snprintf(date_buf, sizeof(date_buf), "%s %02d %s",
             s_day_short[ti.tm_wday],
             ti.tm_mday,
             s_month_short[ti.tm_mon]);
    lv_label_set_text(s_ui.faces.segment_date_label, date_buf);
    lv_obj_align_to(s_ui.faces.segment_date_label, s_ui.faces.segment_panel, LV_ALIGN_OUT_BOTTOM_MID, 0, 24);
}

static void create_slava_face(lv_obj_t *parent)
{
    lv_obj_t *face = lv_image_create(parent);

    lv_obj_set_style_bg_color(parent, lv_color_black(), 0);
    lv_image_set_src(face, &slava_face_img);
    lv_obj_center(face);

    s_ui.faces.slava_line_hour = lv_line_create(parent);
    lv_obj_set_style_line_width(s_ui.faces.slava_line_hour, 7, 0);
    lv_obj_set_style_line_color(s_ui.faces.slava_line_hour, lv_color_hex(SLAVA_HAND_COL), 0);
    lv_obj_set_style_line_rounded(s_ui.faces.slava_line_hour, true, 0);

    s_ui.faces.slava_line_min = lv_line_create(parent);
    lv_obj_set_style_line_width(s_ui.faces.slava_line_min, 5, 0);
    lv_obj_set_style_line_color(s_ui.faces.slava_line_min, lv_color_hex(SLAVA_HAND_COL), 0);
    lv_obj_set_style_line_rounded(s_ui.faces.slava_line_min, true, 0);

    s_ui.faces.slava_line_sec = lv_line_create(parent);
    lv_obj_set_style_line_width(s_ui.faces.slava_line_sec, 3, 0);
    lv_obj_set_style_line_color(s_ui.faces.slava_line_sec, lv_color_hex(SLAVA_SEC_COL), 0);
    lv_obj_set_style_line_rounded(s_ui.faces.slava_line_sec, true, 0);

    s_ui.faces.slava_center_dot = lv_obj_create(parent);
    lv_obj_set_size(s_ui.faces.slava_center_dot, 16, 16);
    lv_obj_set_style_radius(s_ui.faces.slava_center_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_ui.faces.slava_center_dot, lv_color_hex(SLAVA_HAND_COL), 0);
    lv_obj_set_style_border_width(s_ui.faces.slava_center_dot, 0, 0);
    lv_obj_set_style_pad_all(s_ui.faces.slava_center_dot, 0, 0);
    lv_obj_align(s_ui.faces.slava_center_dot, LV_ALIGN_CENTER, 0, 0);
}

static void update_slava_face(void)
{
    struct tm ti = get_local_time_now();
    float hour_angle = ((ti.tm_hour % 12) + ti.tm_min / 60.0f) * 30.0f;
    float min_angle = (ti.tm_min + ti.tm_sec / 60.0f) * 6.0f;
    float sec_angle = ti.tm_sec * 6.0f;

    hand_endpoint(CENTER, CENTER, SLAVA_HOUR_HAND_LEN, hour_angle, &s_ui.faces.slava_hour_pts[0], &s_ui.faces.slava_hour_pts[1]);
    hand_endpoint(CENTER, CENTER, SLAVA_MIN_HAND_LEN, min_angle, &s_ui.faces.slava_min_pts[0], &s_ui.faces.slava_min_pts[1]);
    hand_line_endpoints(CENTER, CENTER, SLAVA_SEC_TAIL_LEN, SLAVA_SEC_HAND_LEN, sec_angle,
                        &s_ui.faces.slava_sec_pts[0], &s_ui.faces.slava_sec_pts[1]);

    lv_line_set_points(s_ui.faces.slava_line_hour, s_ui.faces.slava_hour_pts, 2);
    lv_line_set_points(s_ui.faces.slava_line_min, s_ui.faces.slava_min_pts, 2);
    lv_line_set_points(s_ui.faces.slava_line_sec, s_ui.faces.slava_sec_pts, 2);
}

static void create_slava_dark_face(lv_obj_t *parent)
{
    lv_obj_t *face = lv_image_create(parent);

    lv_obj_set_style_bg_color(parent, lv_color_black(), 0);
    lv_image_set_src(face, &slava_dark_face_img);
    lv_obj_center(face);

    s_ui.faces.slava_dark_line_hour = lv_line_create(parent);
    lv_obj_set_style_line_width(s_ui.faces.slava_dark_line_hour, 7, 0);
    lv_obj_set_style_line_color(s_ui.faces.slava_dark_line_hour, lv_color_hex(SLAVA_DARK_HAND_COL), 0);
    lv_obj_set_style_line_rounded(s_ui.faces.slava_dark_line_hour, true, 0);

    s_ui.faces.slava_dark_line_min = lv_line_create(parent);
    lv_obj_set_style_line_width(s_ui.faces.slava_dark_line_min, 5, 0);
    lv_obj_set_style_line_color(s_ui.faces.slava_dark_line_min, lv_color_hex(SLAVA_DARK_HAND_COL), 0);
    lv_obj_set_style_line_rounded(s_ui.faces.slava_dark_line_min, true, 0);

    s_ui.faces.slava_dark_line_sec = lv_line_create(parent);
    lv_obj_set_style_line_width(s_ui.faces.slava_dark_line_sec, 3, 0);
    lv_obj_set_style_line_color(s_ui.faces.slava_dark_line_sec, lv_color_hex(SLAVA_SEC_COL), 0);
    lv_obj_set_style_line_rounded(s_ui.faces.slava_dark_line_sec, true, 0);

    s_ui.faces.slava_dark_center_dot = lv_obj_create(parent);
    lv_obj_set_size(s_ui.faces.slava_dark_center_dot, 16, 16);
    lv_obj_set_style_radius(s_ui.faces.slava_dark_center_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_ui.faces.slava_dark_center_dot, lv_color_hex(SLAVA_DARK_HAND_COL), 0);
    lv_obj_set_style_border_width(s_ui.faces.slava_dark_center_dot, 0, 0);
    lv_obj_set_style_pad_all(s_ui.faces.slava_dark_center_dot, 0, 0);
    lv_obj_align(s_ui.faces.slava_dark_center_dot, LV_ALIGN_CENTER, 0, 0);
}

static void update_slava_dark_face(void)
{
    struct tm ti = get_local_time_now();
    float hour_angle = ((ti.tm_hour % 12) + ti.tm_min / 60.0f) * 30.0f;
    float min_angle = (ti.tm_min + ti.tm_sec / 60.0f) * 6.0f;
    float sec_angle = ti.tm_sec * 6.0f;

    hand_endpoint(CENTER, CENTER, SLAVA_HOUR_HAND_LEN, hour_angle, &s_ui.faces.slava_dark_hour_pts[0], &s_ui.faces.slava_dark_hour_pts[1]);
    hand_endpoint(CENTER, CENTER, SLAVA_MIN_HAND_LEN, min_angle, &s_ui.faces.slava_dark_min_pts[0], &s_ui.faces.slava_dark_min_pts[1]);
    hand_line_endpoints(CENTER, CENTER, SLAVA_SEC_TAIL_LEN, SLAVA_SEC_HAND_LEN, sec_angle,
                        &s_ui.faces.slava_dark_sec_pts[0], &s_ui.faces.slava_dark_sec_pts[1]);

    lv_line_set_points(s_ui.faces.slava_dark_line_hour, s_ui.faces.slava_dark_hour_pts, 2);
    lv_line_set_points(s_ui.faces.slava_dark_line_min, s_ui.faces.slava_dark_min_pts, 2);
    lv_line_set_points(s_ui.faces.slava_dark_line_sec, s_ui.faces.slava_dark_sec_pts, 2);
}
