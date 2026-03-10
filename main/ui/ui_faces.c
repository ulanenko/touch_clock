static void create_digital_face(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x0D0D0D), 0);
    lv_obj_set_style_bg_grad_color(parent, lv_color_hex(0x0D0D0D), 0);
    lv_obj_set_style_bg_grad_dir(parent, LV_GRAD_DIR_NONE, 0);

    s_ui.faces.digital_time_label = lv_label_create(parent);
    lv_obj_set_style_text_font(s_ui.faces.digital_time_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_ui.faces.digital_time_label, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_text_letter_space(s_ui.faces.digital_time_label, 3, 0);
    lv_label_set_text(s_ui.faces.digital_time_label, "--:--");
    lv_obj_center(s_ui.faces.digital_time_label);
}

static void update_digital_face(void)
{
    struct tm ti = get_local_time_now();
    char buf_time[16];

    snprintf(buf_time, sizeof(buf_time), "%02d:%02d", ti.tm_hour, ti.tm_min);

    lv_label_set_text(s_ui.faces.digital_time_label, buf_time);
    lv_obj_center(s_ui.faces.digital_time_label);
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

