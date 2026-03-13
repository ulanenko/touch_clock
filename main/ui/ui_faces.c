#include "ui/clock_ui_private.h"

static const char *s_day_caps[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
static const char *s_month_caps[12] = {
    "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
    "JUL", "AUG", "SEP", "OCT", "NOV", "DEC",
};
static const lv_coord_t s_digital_day_y_offsets[7] = {0, 0, 0, 0, 0, 0, 0};
static const int s_sternglas_even_hours[6] = {12, 2, 4, 6, 8, 10};
static const int s_sternglas_odd_hours[6] = {1, 3, 5, 7, 9, 11};
static lv_point_precise_t s_sternglas_minute_tick_pts[48][2];
static lv_point_precise_t s_sternglas_odd_hour_pts[6][2];
static lv_point_precise_t s_sternglas_radio_wave_pts[4][5];

static void update_matrix_face(clock_ui_context_t *ctx);
static void update_wharton_face(clock_ui_context_t *ctx);
static void update_sternglas_face(clock_ui_context_t *ctx);
static void update_avenir_face(clock_ui_context_t *ctx);
static void update_modern_silver_face(clock_ui_context_t *ctx);

#define STERNGLAS_SCALE 1.80f
#define STERNGLAS_OFFSET 0.0f
#define STERNGLAS_CENTER_X 360
#define STERNGLAS_CENTER_Y 360
#define STERNGLAS_HOUR_TAIL 33
#define STERNGLAS_HOUR_LEN 157
#define STERNGLAS_MIN_TAIL 41
#define STERNGLAS_MIN_LEN 256
#define STERNGLAS_HANDS_CANVAS_SIZE 640
#define STERNGLAS_HANDS_CANVAS_OFFSET ((SCREEN_SIZE - STERNGLAS_HANDS_CANVAS_SIZE) / 2)
#define STERNGLAS_HANDS_CENTER_X (STERNGLAS_CENTER_X - STERNGLAS_HANDS_CANVAS_OFFSET)
#define STERNGLAS_HANDS_CENTER_Y (STERNGLAS_CENTER_Y - STERNGLAS_HANDS_CANVAS_OFFSET)
#define AVENIR_SCALE ((float)SCREEN_SIZE / 340.0f)
#define AVENIR_OFFSET_X 0.0f
#define AVENIR_OFFSET_Y 0.0f
#define AVENIR_CENTER_SRC 170.0f
#define AVENIR_CENTER_X ((lv_coord_t)lrintf(AVENIR_OFFSET_X + AVENIR_CENTER_SRC * AVENIR_SCALE))
#define AVENIR_CENTER_Y ((lv_coord_t)lrintf(AVENIR_OFFSET_Y + AVENIR_CENTER_SRC * AVENIR_SCALE))
#define AVENIR_HANDS_CANVAS_SIZE 640
#define AVENIR_HANDS_CANVAS_OFFSET ((SCREEN_SIZE - AVENIR_HANDS_CANVAS_SIZE) / 2)
#define AVENIR_HANDS_CENTER_X (AVENIR_CENTER_X - AVENIR_HANDS_CANVAS_OFFSET)
#define AVENIR_HANDS_CENTER_Y (AVENIR_CENTER_Y - AVENIR_HANDS_CANVAS_OFFSET)
#define MODERN_SCALE ((float)SCREEN_SIZE / 340.0f)
#define MODERN_OFFSET_X 0.0f
#define MODERN_OFFSET_Y 0.0f
#define MODERN_CENTER_SRC 170.0f
#define MODERN_CENTER_X ((lv_coord_t)lrintf(MODERN_OFFSET_X + MODERN_CENTER_SRC * MODERN_SCALE))
#define MODERN_CENTER_Y ((lv_coord_t)lrintf(MODERN_OFFSET_Y + MODERN_CENTER_SRC * MODERN_SCALE))
#define MODERN_HANDS_CANVAS_SIZE 680
#define MODERN_HANDS_CANVAS_OFFSET ((SCREEN_SIZE - MODERN_HANDS_CANVAS_SIZE) / 2)
#define MODERN_HANDS_CENTER_X (MODERN_CENTER_X - MODERN_HANDS_CANVAS_OFFSET)
#define MODERN_HANDS_CENTER_Y (MODERN_CENTER_Y - MODERN_HANDS_CANVAS_OFFSET)

static void show_digital_face_live(clock_ui_context_t *ctx)
{
    if (ctx->faces.digital_live_root == NULL || ctx->faces.digital_snapshot_img == NULL) {
        return;
    }

    lv_obj_clear_flag(ctx->faces.digital_live_root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ctx->faces.digital_snapshot_img, LV_OBJ_FLAG_HIDDEN);
}

static void show_digital_face_snapshot(clock_ui_context_t *ctx)
{
    bool live_visible;

    if (ctx->faces.digital_live_root == NULL || ctx->faces.digital_snapshot_img == NULL) {
        return;
    }

    live_visible = !lv_obj_has_flag(ctx->faces.digital_live_root, LV_OBJ_FLAG_HIDDEN);
    if (live_visible) {
        refresh_digital_face_snapshot(ctx);
        return;
    }

    if (ctx->faces.digital_snapshot_buf != NULL) {
        lv_obj_clear_flag(ctx->faces.digital_snapshot_img, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_flag(ctx->faces.digital_live_root, LV_OBJ_FLAG_HIDDEN);
}

void sync_face_animation_state(clock_ui_context_t *ctx, clock_face_id_t face)
{
    bool brightness_overlay_visible = ctx->brightness.overlay != NULL &&
                                      !lv_obj_has_flag(ctx->brightness.overlay, LV_OBJ_FLAG_HIDDEN);
    bool keep_digital_live = face == CLOCK_FACE_DIGITAL &&
                             !ctx->faces.tileview_scrolling &&
                             !ctx->brightness.animating &&
                             !ctx->brightness.dragging &&
                             !brightness_overlay_visible &&
                             !brightness_panel_is_open(ctx);

    if (keep_digital_live) {
        show_digital_face_live(ctx);
    } else {
        show_digital_face_snapshot(ctx);
    }
}

static void refresh_face_composite_snapshot(clock_ui_context_t *ctx, clock_face_id_t face,
                                            lv_obj_t *snapshot_img,
                                            lv_obj_t *hands_canvas,
                                            lv_obj_t *composite_img,
                                            lv_draw_buf_t **composite_buf)
{
    lv_obj_t *tile;

    if (snapshot_img == NULL || hands_canvas == NULL || composite_img == NULL || composite_buf == NULL) {
        return;
    }

    tile = ctx->tiles[face];
    if (tile == NULL) {
        return;
    }

    lv_obj_clear_flag(snapshot_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(hands_canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(composite_img, LV_OBJ_FLAG_HIDDEN);

    if (*composite_buf == NULL) {
        *composite_buf = lv_snapshot_create_draw_buf(tile, LV_COLOR_FORMAT_RGB565);
        if (*composite_buf == NULL) {
            return;
        }
    }

    if (lv_snapshot_take_to_draw_buf(tile, LV_COLOR_FORMAT_RGB565, *composite_buf) != LV_RESULT_OK) {
        return;
    }

    lv_image_set_src(composite_img, *composite_buf);
    lv_obj_clear_flag(composite_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(snapshot_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(hands_canvas, LV_OBJ_FLAG_HIDDEN);
}

static void face_set_label_text_if_changed(lv_obj_t *label, const char *text)
{
    const char *current_text;

    if (label == NULL || text == NULL) {
        return;
    }

    current_text = lv_label_get_text(label);
    if (current_text == NULL || strcmp(current_text, text) != 0) {
        lv_label_set_text(label, text);
    }
}

static void translate_precise_points(lv_point_precise_t *points, size_t count, lv_coord_t dx, lv_coord_t dy)
{
    for (size_t i = 0; i < count; ++i) {
        points[i].x += dx;
        points[i].y += dy;
    }
}

static lv_coord_t sternglas_map(float value)
{
    return (lv_coord_t)lrintf(STERNGLAS_OFFSET + value * STERNGLAS_SCALE);
}

static void sternglas_rotate_point(float x, float y, float angle_deg, float *out_x, float *out_y)
{
    float rad = angle_deg * (M_PI / 180.0f);
    float dx = x - 200.0f;
    float dy = y - 200.0f;

    *out_x = 200.0f + dx * cosf(rad) - dy * sinf(rad);
    *out_y = 200.0f + dx * sinf(rad) + dy * cosf(rad);
}

static lv_obj_t *create_sternglas_label(lv_obj_t *parent,
                                        const char *text,
                                        const lv_font_t *font,
                                        lv_color_t color,
                                        lv_coord_t letter_space,
                                        float x,
                                        float y,
                                        float angle_deg)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(label, 0, 0);
    lv_obj_set_style_pad_all(label, 0, 0);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_letter_space(label, letter_space, 0);
    lv_label_set_text(label, text);
    lv_obj_update_layout(label);
    lv_obj_set_pos(label,
                   sternglas_map(x) - lv_obj_get_width(label) / 2,
                   sternglas_map(y) - lv_obj_get_height(label) / 2);

    if (angle_deg != 0.0f) {
        lv_obj_set_style_transform_pivot_x(label, lv_obj_get_width(label) / 2, 0);
        lv_obj_set_style_transform_pivot_y(label, lv_obj_get_height(label) / 2, 0);
        lv_obj_set_style_transform_rotation(label, (int32_t)lrintf(angle_deg * 10.0f), 0);
    }

    return label;
}

static void sternglas_init_wave_points(int index, float x0, float y0, float cx, float cy, float x1, float y1)
{
    for (int i = 0; i < 5; ++i) {
        float t = i / 4.0f;
        float mt = 1.0f - t;
        float px = mt * mt * x0 + 2.0f * mt * t * cx + t * t * x1;
        float py = mt * mt * y0 + 2.0f * mt * t * cy + t * t * y1;

        s_sternglas_radio_wave_pts[index][i].x = sternglas_map(px);
        s_sternglas_radio_wave_pts[index][i].y = sternglas_map(py);
    }
}

static void sternglas_prepare_static_geometry(void)
{
    int tick = 0;

    for (int i = 0; i < 60; ++i) {
        float angle = i * 6.0f;
        float outer_x;
        float outer_y;
        float inner_x;
        float inner_y;

        if ((i % 5) == 0) {
            continue;
        }

        sternglas_rotate_point(200.0f, 25.0f, angle, &outer_x, &outer_y);
        sternglas_rotate_point(200.0f, 33.0f, angle, &inner_x, &inner_y);
        s_sternglas_minute_tick_pts[tick][0].x = sternglas_map(outer_x);
        s_sternglas_minute_tick_pts[tick][0].y = sternglas_map(outer_y);
        s_sternglas_minute_tick_pts[tick][1].x = sternglas_map(inner_x);
        s_sternglas_minute_tick_pts[tick][1].y = sternglas_map(inner_y);
        tick++;
    }

    for (int i = 0; i < 6; ++i) {
        float x0;
        float y0;
        float x1;
        float y1;

        sternglas_rotate_point(200.0f, 25.0f, s_sternglas_odd_hours[i] * 30.0f, &x0, &y0);
        sternglas_rotate_point(200.0f, 85.0f, s_sternglas_odd_hours[i] * 30.0f, &x1, &y1);
        s_sternglas_odd_hour_pts[i][0].x = sternglas_map(x0);
        s_sternglas_odd_hour_pts[i][0].y = sternglas_map(y0);
        s_sternglas_odd_hour_pts[i][1].x = sternglas_map(x1);
        s_sternglas_odd_hour_pts[i][1].y = sternglas_map(y1);
    }

    sternglas_init_wave_points(0, 195.0f, 256.0f, 192.0f, 260.0f, 195.0f, 264.0f);
    sternglas_init_wave_points(1, 191.0f, 253.0f, 187.0f, 260.0f, 191.0f, 267.0f);
    sternglas_init_wave_points(2, 205.0f, 256.0f, 208.0f, 260.0f, 205.0f, 264.0f);
    sternglas_init_wave_points(3, 209.0f, 253.0f, 213.0f, 260.0f, 209.0f, 267.0f);
}

static lv_coord_t avenir_map_x(float value)
{
    return (lv_coord_t)lrintf(AVENIR_OFFSET_X + value * AVENIR_SCALE);
}

static lv_coord_t avenir_map_y(float value)
{
    return (lv_coord_t)lrintf(AVENIR_OFFSET_Y + value * AVENIR_SCALE);
}

static lv_coord_t avenir_size(float value)
{
    return (lv_coord_t)lrintf(value * AVENIR_SCALE);
}

static void avenir_rotate_point(float x, float y, float angle_deg, float *out_x, float *out_y)
{
    float rad = angle_deg * (M_PI / 180.0f);
    float dx = x - AVENIR_CENTER_SRC;
    float dy = y - AVENIR_CENTER_SRC;

    *out_x = AVENIR_CENTER_SRC + dx * cosf(rad) - dy * sinf(rad);
    *out_y = AVENIR_CENTER_SRC + dx * sinf(rad) + dy * cosf(rad);
}

static lv_obj_t *create_avenir_label(lv_obj_t *parent,
                                     const char *text,
                                     const lv_font_t *font,
                                     lv_color_t color,
                                     lv_coord_t letter_space,
                                     float x,
                                     float y)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(label, 0, 0);
    lv_obj_set_style_pad_all(label, 0, 0);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_letter_space(label, letter_space, 0);
    lv_label_set_text(label, text);
    lv_obj_update_layout(label);
    lv_obj_set_pos(label,
                   avenir_map_x(x) - lv_obj_get_width(label) / 2,
                   avenir_map_y(y) - lv_obj_get_height(label) / 2);

    return label;
}

static lv_coord_t modern_map_x(float value)
{
    return (lv_coord_t)lrintf(MODERN_OFFSET_X + value * MODERN_SCALE);
}

static lv_coord_t modern_map_y(float value)
{
    return (lv_coord_t)lrintf(MODERN_OFFSET_Y + value * MODERN_SCALE);
}

static lv_coord_t modern_size(float value)
{
    return (lv_coord_t)lrintf(value * MODERN_SCALE);
}

static void modern_rotate_point(float x, float y, float angle_deg, float *out_x, float *out_y)
{
    float rad = angle_deg * (M_PI / 180.0f);
    float dx = x - MODERN_CENTER_SRC;
    float dy = y - MODERN_CENTER_SRC;

    *out_x = MODERN_CENTER_SRC + dx * cosf(rad) - dy * sinf(rad);
    *out_y = MODERN_CENTER_SRC + dx * sinf(rad) + dy * cosf(rad);
}

static lv_obj_t *create_modern_label(lv_obj_t *parent,
                                     const char *text,
                                     const lv_font_t *font,
                                     lv_color_t color,
                                     lv_opa_t opa,
                                     lv_coord_t letter_space,
                                     float x,
                                     float y)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(label, 0, 0);
    lv_obj_set_style_pad_all(label, 0, 0);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_opa(label, opa, 0);
    lv_obj_set_style_text_letter_space(label, letter_space, 0);
    lv_label_set_text(label, text);
    lv_obj_update_layout(label);
    lv_obj_set_pos(label,
                   modern_map_x(x) - lv_obj_get_width(label) / 2,
                   modern_map_y(y) - lv_obj_get_height(label) / 2);

    return label;
}

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

void refresh_digital_face_snapshot(clock_ui_context_t *ctx)
{
    lv_coord_t ext_draw;

    if (ctx->faces.digital_live_root == NULL || ctx->faces.digital_snapshot_img == NULL) {
        return;
    }

    lv_obj_clear_flag(ctx->faces.digital_live_root, LV_OBJ_FLAG_HIDDEN);

    if (ctx->faces.digital_snapshot_buf == NULL) {
        ctx->faces.digital_snapshot_buf = lv_snapshot_create_draw_buf(ctx->faces.digital_live_root,
                                                                      LV_COLOR_FORMAT_RGB565);
        if (ctx->faces.digital_snapshot_buf == NULL) {
            lv_obj_add_flag(ctx->faces.digital_live_root, LV_OBJ_FLAG_HIDDEN);
            return;
        }
    }

    if (lv_snapshot_take_to_draw_buf(ctx->faces.digital_live_root,
                                     LV_COLOR_FORMAT_RGB565,
                                     ctx->faces.digital_snapshot_buf) != LV_RESULT_OK) {
        lv_obj_add_flag(ctx->faces.digital_live_root, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    ext_draw = (lv_coord_t)((int32_t)ctx->faces.digital_snapshot_buf->header.w - SCREEN_SIZE) / 2;
    if (ext_draw < 0) {
        ext_draw = 0;
    }
    ctx->faces.digital_snapshot_ext_draw = ext_draw;
    lv_image_set_src(ctx->faces.digital_snapshot_img, ctx->faces.digital_snapshot_buf);
    lv_obj_set_pos(ctx->faces.digital_snapshot_img, -ext_draw, -ext_draw);
    lv_obj_clear_flag(ctx->faces.digital_snapshot_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ctx->faces.digital_live_root, LV_OBJ_FLAG_HIDDEN);
}

const void *ui_face_preview_source(clock_ui_context_t *ctx, clock_face_id_t face)
{
    face = sanitize_enabled_face(face);
    update_face(ctx, face);

    switch (face) {
    case CLOCK_FACE_DIGITAL:
        refresh_digital_face_snapshot(ctx);
        return ctx->faces.digital_snapshot_buf;
    case CLOCK_FACE_MATRIX:
        return (ctx->faces.matrix_face_obj != NULL) ? lv_canvas_get_draw_buf(ctx->faces.matrix_face_obj) : NULL;
    case CLOCK_FACE_WHARTON:
        return (ctx->faces.wharton_face_obj != NULL) ? lv_canvas_get_draw_buf(ctx->faces.wharton_face_obj) : NULL;
    case CLOCK_FACE_STERNGLAS:
        return (ctx->faces.sternglas_composite_buf != NULL) ? ctx->faces.sternglas_composite_buf
                                                            : ctx->faces.sternglas_snapshot_buf;
    case CLOCK_FACE_AVENIR:
        return (ctx->faces.avenir_composite_buf != NULL) ? ctx->faces.avenir_composite_buf
                                                         : ctx->faces.avenir_snapshot_buf;
    case CLOCK_FACE_MODERN_SILVER:
        return (ctx->faces.modern_silver_composite_buf != NULL) ? ctx->faces.modern_silver_composite_buf
                                                                : ctx->faces.modern_silver_snapshot_buf;
    default:
        return ctx->faces.digital_snapshot_buf;
    }
}

void create_digital_face(clock_ui_context_t *ctx, lv_obj_t *parent)
{
    lv_obj_t *clock_center;
    lv_obj_t *time_holder;
    lv_obj_t *side_holder;
    lv_obj_t *days_bar;

    lv_obj_set_style_bg_color(parent, lv_color_hex(0x010401), 0);
    lv_obj_set_style_bg_grad_opa(parent, LV_OPA_TRANSP, 0);
    ctx->faces.digital_glow = NULL;
    ctx->faces.digital_cache_valid = false;
    ctx->faces.digital_snapshot_buf = NULL;
    ctx->faces.digital_snapshot_ext_draw = 0;
    ctx->faces.digital_snapshot_img = lv_image_create(parent);
    lv_obj_set_style_bg_opa(ctx->faces.digital_snapshot_img, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ctx->faces.digital_snapshot_img, 0, 0);
    lv_obj_set_style_pad_all(ctx->faces.digital_snapshot_img, 0, 0);
    lv_obj_clear_flag(ctx->faces.digital_snapshot_img, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ctx->faces.digital_snapshot_img, LV_OBJ_FLAG_HIDDEN);
    ctx->faces.digital_live_root = lv_obj_create(parent);
    lv_obj_set_size(ctx->faces.digital_live_root, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_color(ctx->faces.digital_live_root, lv_color_hex(0x010401), 0);
    lv_obj_set_style_bg_grad_opa(ctx->faces.digital_live_root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ctx->faces.digital_live_root, 0, 0);
    lv_obj_set_style_pad_all(ctx->faces.digital_live_root, 0, 0);
    lv_obj_clear_flag(ctx->faces.digital_live_root, LV_OBJ_FLAG_SCROLLABLE);
    clock_center = lv_obj_create(ctx->faces.digital_live_root);
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

    ctx->faces.digital_time_bg = create_digital_segment_label(time_holder, &seven_segment_font_112, lv_color_hex(0x143C14), 8);
    lv_label_set_text(ctx->faces.digital_time_bg, "88:88");
    lv_obj_align(ctx->faces.digital_time_bg, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_text_opa(ctx->faces.digital_time_bg, LV_OPA_40, 0);
    apply_digital_italic(ctx->faces.digital_time_bg, -120);

    ctx->faces.digital_time_glow = create_digital_segment_label(time_holder, &seven_segment_font_112, lv_color_hex(0x5CFB5C), 8);
    lv_obj_align(ctx->faces.digital_time_glow, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_text_opa(ctx->faces.digital_time_glow, 30, 0);
    apply_digital_italic(ctx->faces.digital_time_glow, -120);

    ctx->faces.digital_time_fg = create_digital_segment_label(time_holder, &seven_segment_font_112, lv_color_hex(0x5CFB5C), 8);
    lv_obj_align(ctx->faces.digital_time_fg, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    apply_digital_italic(ctx->faces.digital_time_fg, -120);

    side_holder = lv_obj_create(clock_center);
    lv_obj_set_size(side_holder, 134, 206);
    lv_obj_set_style_bg_opa(side_holder, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(side_holder, 0, 0);
    lv_obj_set_style_pad_all(side_holder, 0, 0);
    lv_obj_clear_flag(side_holder, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align_to(side_holder, time_holder, LV_ALIGN_OUT_RIGHT_BOTTOM, -58, -4);

    ctx->faces.digital_ampm_glow = NULL;

    ctx->faces.digital_ampm_label = create_digital_text_label(side_holder, &dseg14_classic_italic_36, lv_color_hex(0x5CFB5C), LV_OPA_80, 2);
    lv_label_set_text(ctx->faces.digital_ampm_label, "PM");
    lv_obj_align(ctx->faces.digital_ampm_label, LV_ALIGN_TOP_RIGHT, 0, 0);
    apply_digital_italic(ctx->faces.digital_ampm_label, -80);

    ctx->faces.digital_seconds_bg = create_digital_segment_label(side_holder, &seven_segment_font_56, lv_color_hex(0x143C14), 4);
    lv_label_set_text(ctx->faces.digital_seconds_bg, "88");
    lv_obj_align(ctx->faces.digital_seconds_bg, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_text_opa(ctx->faces.digital_seconds_bg, LV_OPA_40, 0);
    apply_digital_italic(ctx->faces.digital_seconds_bg, -120);

    ctx->faces.digital_seconds_glow = create_digital_segment_label(side_holder, &seven_segment_font_56, lv_color_hex(0x5CFB5C), 4);
    lv_obj_align(ctx->faces.digital_seconds_glow, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_text_opa(ctx->faces.digital_seconds_glow, 30, 0);
    apply_digital_italic(ctx->faces.digital_seconds_glow, -120);

    ctx->faces.digital_seconds_fg = create_digital_segment_label(side_holder, &seven_segment_font_56, lv_color_hex(0x5CFB5C), 4);
    lv_obj_align(ctx->faces.digital_seconds_fg, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    apply_digital_italic(ctx->faces.digital_seconds_fg, -120);

    days_bar = lv_obj_create(ctx->faces.digital_live_root);
    lv_obj_set_size(days_bar, 580, 36);
    lv_obj_set_style_bg_opa(days_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(days_bar, 0, 0);
    lv_obj_set_style_pad_all(days_bar, 0, 0);
    lv_obj_clear_flag(days_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(days_bar, LV_ALIGN_BOTTOM_MID, 0, -136);

    for (int i = 0; i < 7; ++i) {
        ctx->faces.digital_day_glow[i] = NULL;

        ctx->faces.digital_day_label[i] = create_digital_text_label(days_bar, &dseg14_classic_italic_20, lv_color_hex(0x143C14), LV_OPA_40, 1);
        lv_label_set_text(ctx->faces.digital_day_label[i], s_day_caps[i]);
        apply_digital_italic(ctx->faces.digital_day_label[i], -60);
    }

    ctx->faces.digital_date_glow = NULL;

    ctx->faces.digital_date_label = create_digital_text_label(ctx->faces.digital_live_root, &dseg14_classic_italic_24, lv_color_hex(0x5CFB5C), LV_OPA_90, 1);
    lv_label_set_text(ctx->faces.digital_date_label, "SUN, FEB 11");
    lv_obj_align(ctx->faces.digital_date_label, LV_ALIGN_TOP_MID, 0, 98);
    apply_digital_italic(ctx->faces.digital_date_label, -60);

    make_face_layer_passive(clock_center);
    make_face_layer_passive(days_bar);
    make_face_layer_passive(ctx->faces.digital_date_label);
    make_face_layer_passive(ctx->faces.digital_live_root);
}

static void update_digital_face(clock_ui_context_t *ctx)
{
    struct tm ti = get_local_time_now();
    char buf_time[16];
    char buf_seconds[8];
    char buf_date[32];
    int hour12 = ti.tm_hour % 12;
    bool is_pm = ti.tm_hour >= 12;
    bool time_changed;
    bool second_changed;
    bool date_changed;
    bool day_changed;
    bool content_changed;

    if (hour12 == 0) {
        hour12 = 12;
    }

    time_changed = !ctx->faces.digital_cache_valid ||
                   ctx->faces.digital_last_hour12 != hour12 ||
                   ctx->faces.digital_last_minute != ti.tm_min;
    second_changed = !ctx->faces.digital_cache_valid ||
                     ctx->faces.digital_last_second != ti.tm_sec;
    date_changed = !ctx->faces.digital_cache_valid ||
                   ctx->faces.digital_last_year != ti.tm_year ||
                   ctx->faces.digital_last_yday != ti.tm_yday;
    day_changed = !ctx->faces.digital_cache_valid ||
                  ctx->faces.digital_last_wday != ti.tm_wday;
    content_changed = time_changed || second_changed || date_changed || day_changed;

    snprintf(buf_time, sizeof(buf_time), "%2d:%02d", hour12, ti.tm_min);
    snprintf(buf_seconds, sizeof(buf_seconds), "%02d", ti.tm_sec);
    snprintf(buf_date, sizeof(buf_date), "%s, %s %d",
             s_day_caps[ti.tm_wday],
             s_month_caps[ti.tm_mon],
             ti.tm_mday);

    if (time_changed) {
        if (ctx->faces.digital_time_glow != NULL) {
            face_set_label_text_if_changed(ctx->faces.digital_time_glow, buf_time);
        }
        face_set_label_text_if_changed(ctx->faces.digital_time_fg, buf_time);
    }

    if (second_changed) {
        if (ctx->faces.digital_seconds_glow != NULL) {
            face_set_label_text_if_changed(ctx->faces.digital_seconds_glow, buf_seconds);
        }
        face_set_label_text_if_changed(ctx->faces.digital_seconds_fg, buf_seconds);
    }

    if (!ctx->faces.digital_cache_valid || ctx->faces.digital_last_pm != is_pm) {
        if (ctx->faces.digital_ampm_glow != NULL) {
            face_set_label_text_if_changed(ctx->faces.digital_ampm_glow, is_pm ? "PM" : "AM");
        }
        face_set_label_text_if_changed(ctx->faces.digital_ampm_label, is_pm ? "PM" : "AM");
    }

    if (date_changed) {
        if (ctx->faces.digital_date_glow != NULL) {
            face_set_label_text_if_changed(ctx->faces.digital_date_glow, buf_date);
            lv_obj_align(ctx->faces.digital_date_glow, LV_ALIGN_TOP_MID, 0, 98);
        }
        face_set_label_text_if_changed(ctx->faces.digital_date_label, buf_date);
        lv_obj_align(ctx->faces.digital_date_label, LV_ALIGN_TOP_MID, 0, 98);
    }

    if (day_changed) {
        for (int i = 0; i < 7; ++i) {
            bool active = (i == ti.tm_wday);
            lv_coord_t slot_w = 580 / 7;
            lv_coord_t x = (lv_coord_t)(i * slot_w + slot_w / 2);

            if (ctx->faces.digital_day_glow[i] != NULL) {
                lv_obj_set_style_text_color(ctx->faces.digital_day_glow[i], lv_color_hex(0x5CFB5C), 0);
                lv_obj_set_style_text_opa(ctx->faces.digital_day_glow[i], active ? 28 : 0, 0);
                lv_obj_align(ctx->faces.digital_day_glow[i],
                             LV_ALIGN_TOP_LEFT,
                             x - lv_obj_get_width(ctx->faces.digital_day_glow[i]) / 2,
                             s_digital_day_y_offsets[i]);
            }
            lv_obj_set_style_text_color(ctx->faces.digital_day_label[i],
                                        active ? lv_color_hex(0x5CFB5C) : lv_color_hex(0x143C14),
                                        0);
            lv_obj_set_style_text_opa(ctx->faces.digital_day_label[i],
                                      active ? LV_OPA_COVER : LV_OPA_40,
                                      0);
            lv_obj_align(ctx->faces.digital_day_label[i],
                         LV_ALIGN_TOP_LEFT,
                         x - lv_obj_get_width(ctx->faces.digital_day_label[i]) / 2,
                         s_digital_day_y_offsets[i]);
        }
    }

    ctx->faces.digital_last_hour12 = (uint8_t)hour12;
    ctx->faces.digital_last_minute = (uint8_t)ti.tm_min;
    ctx->faces.digital_last_second = (uint8_t)ti.tm_sec;
    ctx->faces.digital_last_pm = is_pm;
    ctx->faces.digital_last_year = (int16_t)ti.tm_year;
    ctx->faces.digital_last_yday = (int16_t)ti.tm_yday;
    ctx->faces.digital_last_wday = (int8_t)ti.tm_wday;
    ctx->faces.digital_cache_valid = true;

    LV_UNUSED(content_changed);
}

void update_face(clock_ui_context_t *ctx, clock_face_id_t face)
{
    face = sanitize_enabled_face(face);

    switch (face) {
    case CLOCK_FACE_DIGITAL:
        update_digital_face(ctx);
        break;
    case CLOCK_FACE_MATRIX:
        update_matrix_face(ctx);
        break;
    case CLOCK_FACE_WHARTON:
        update_wharton_face(ctx);
        break;
    case CLOCK_FACE_STERNGLAS:
        update_sternglas_face(ctx);
        break;
    case CLOCK_FACE_AVENIR:
        update_avenir_face(ctx);
        break;
    case CLOCK_FACE_MODERN_SILVER:
        update_modern_silver_face(ctx);
        break;
    default:
        update_digital_face(ctx);
        break;
    }
}

static void build_matrix_state(clock_ui_context_t *ctx)
{
    struct tm ti = get_local_time_now();
    int digits[4] = {ti.tm_hour / 10, ti.tm_hour % 10, ti.tm_min / 10, ti.tm_min % 10};

    memset(ctx->faces.matrix_on, 0, sizeof(ctx->faces.matrix_on));

    for (int d = 0; d < 4; ++d) {
        const uint8_t *glyph = s_matrix_font[digits[d]];
        int col0 = s_matrix_digit_col[d];

        for (int r = 0; r < MTX_DIGIT_H; ++r) {
            for (int c = 0; c < 5; ++c) {
                if ((glyph[r] >> (4 - c)) & 1U) {
                    ctx->faces.matrix_on[col0 + c][s_matrix_digit_row0 + r] = true;
                }
            }
        }
    }

    if ((ti.tm_sec & 1) == 0) {
        ctx->faces.matrix_on[s_matrix_colon_col][s_matrix_digit_row0 + 2] = true;
        ctx->faces.matrix_on[s_matrix_colon_col][s_matrix_digit_row0 + 4] = true;
    }
}

void create_matrix_face(clock_ui_context_t *ctx, lv_obj_t *parent)
{
    int total_w = (MTX_GRID_X - 1) * MTX_PITCH + MTX_DOT_SIZE;
    int total_h = (MTX_GRID_Y - 1) * MTX_PITCH + MTX_DOT_SIZE;

    lv_obj_set_style_bg_color(parent, lv_color_hex(0x050505), 0);
    ctx->faces.matrix_x0 = (SCREEN_SIZE - total_w) / 2;
    ctx->faces.matrix_y0 = (SCREEN_SIZE - total_h) / 2;

    if (ctx->faces.matrix_face_buf == NULL) {
        ctx->faces.matrix_face_buf = heap_caps_malloc(SCREEN_SIZE * SCREEN_SIZE * sizeof(lv_color16_t), MALLOC_CAP_SPIRAM);
    }

    ctx->faces.matrix_face_obj = lv_canvas_create(parent);
    lv_canvas_set_buffer(ctx->faces.matrix_face_obj, ctx->faces.matrix_face_buf, SCREEN_SIZE, SCREEN_SIZE, LV_COLOR_FORMAT_RGB565);
    lv_obj_clear_flag(ctx->faces.matrix_face_obj, LV_OBJ_FLAG_SCROLLABLE);
}

static void update_matrix_face(clock_ui_context_t *ctx)
{
    lv_layer_t layer;
    lv_draw_rect_dsc_t dsc;

    build_matrix_state(ctx);
    lv_canvas_fill_bg(ctx->faces.matrix_face_obj, lv_color_hex(0x050505), LV_OPA_COVER);
    lv_canvas_init_layer(ctx->faces.matrix_face_obj, &layer);
    lv_draw_rect_dsc_init(&dsc);
    dsc.radius = MTX_DOT_RAD;
    dsc.bg_opa = LV_OPA_COVER;
    dsc.border_width = 0;
    dsc.shadow_width = 0;
    dsc.outline_width = 0;

    for (int c = 0; c < MTX_GRID_X; ++c) {
        for (int r = 0; r < MTX_GRID_Y; ++r) {
            lv_area_t area;

            area.x1 = ctx->faces.matrix_x0 + c * MTX_PITCH;
            area.y1 = ctx->faces.matrix_y0 + r * MTX_PITCH;
            area.x2 = area.x1 + MTX_DOT_SIZE - 1;
            area.y2 = area.y1 + MTX_DOT_SIZE - 1;
            dsc.bg_color = lv_color_hex(ctx->faces.matrix_on[c][r] ? MTX_COL_ON : MTX_COL_OFF);
            lv_draw_rect(&layer, &dsc, &area);
        }
    }

    lv_canvas_finish_layer(ctx->faces.matrix_face_obj, &layer);
}

static void build_wharton_state(clock_ui_context_t *ctx)
{
    struct tm ti = get_local_time_now();
    int digits[4] = {ti.tm_hour / 10, ti.tm_hour % 10, ti.tm_min / 10, ti.tm_min % 10};

    for (int d = 0; d < 4; ++d) {
        const uint8_t *glyph = s_wharton_font[digits[d]];
        for (int r = 0; r < WH_DIGIT_ROWS; ++r) {
            for (int c = 0; c < WH_DIGIT_COLS; ++c) {
                ctx->faces.wharton_digit_dots[d][c][r] = ((glyph[r] >> (4 - c)) & 1U) != 0;
            }
        }
    }

    ctx->faces.wharton_colon_on = ((ti.tm_sec & 1) == 0);
    ctx->faces.wharton_second_count = ti.tm_sec;
}

void create_wharton_face(clock_ui_context_t *ctx, lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, lv_color_black(), 0);

    if (ctx->faces.wharton_face_buf == NULL) {
        ctx->faces.wharton_face_buf = heap_caps_malloc(SCREEN_SIZE * SCREEN_SIZE * sizeof(lv_color16_t), MALLOC_CAP_SPIRAM);
    }

    ctx->faces.wharton_face_obj = lv_canvas_create(parent);
    lv_canvas_set_buffer(ctx->faces.wharton_face_obj, ctx->faces.wharton_face_buf, SCREEN_SIZE, SCREEN_SIZE, LV_COLOR_FORMAT_RGB565);
    lv_obj_clear_flag(ctx->faces.wharton_face_obj, LV_OBJ_FLAG_SCROLLABLE);
}

static void update_wharton_face(clock_ui_context_t *ctx)
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

    build_wharton_state(ctx);
    lv_canvas_fill_bg(ctx->faces.wharton_face_obj, lv_color_black(), LV_OPA_COVER);
    lv_canvas_init_layer(ctx->faces.wharton_face_obj, &layer);
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_opa = LV_OPA_COVER;
    dsc.border_width = 0;
    dsc.shadow_width = 0;
    dsc.outline_width = 0;

    for (int i = 0; i < WH_RING_DOTS; ++i) {
        float angle = (i * 6.0f - 90.0f) * (M_PI / 180.0f);
        int cx = CENTER + (int)(WH_RING_R * cosf(angle));
        int cy = CENTER + (int)(WH_RING_R * sinf(angle));
        int radius = (i < ctx->faces.wharton_second_count) ? WH_DOT_LIT_R : WH_DOT_DIM_R;
        lv_area_t area;

        area.x1 = cx - radius;
        area.y1 = cy - radius;
        area.x2 = cx + radius;
        area.y2 = cy + radius;
        dsc.radius = radius;
        dsc.bg_color = lv_color_hex((i < ctx->faces.wharton_second_count) ? WH_COL_ON : WH_COL_OFF);
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
                dsc.bg_color = lv_color_hex(ctx->faces.wharton_digit_dots[d][c][r] ? WH_COL_ON : WH_COL_OFF);
                lv_draw_rect(&layer, &dsc, &area);
            }
        }
    }

    dsc.bg_color = lv_color_hex(ctx->faces.wharton_colon_on ? WH_COL_ON : WH_COL_OFF);
    for (int i = 0; i < 2; ++i) {
        lv_area_t area;
        int colon_y = base_y + ((i == 0) ? 2 : 4) * WH_DOT_PITCH;
        area.x1 = colon_x;
        area.y1 = colon_y;
        area.x2 = colon_x + WH_DOT_SZ - 1;
        area.y2 = colon_y + WH_DOT_SZ - 1;
        lv_draw_rect(&layer, &dsc, &area);
    }

    lv_canvas_finish_layer(ctx->faces.wharton_face_obj, &layer);
}

void create_sternglas_face(clock_ui_context_t *ctx, lv_obj_t *parent)
{
    lv_obj_t *root;
    lv_obj_t *dial_shadow;
    lv_obj_t *dial_outer;
    lv_obj_t *dial_inner;
    lv_obj_t *center_shadow;
    lv_obj_t *center_dot;
    lv_coord_t ext_draw = 0;

    sternglas_prepare_static_geometry();

    lv_obj_set_style_bg_color(parent, lv_color_hex(0xF5F5F5), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    root = lv_obj_create(parent);
    lv_obj_set_size(root, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_color(root, lv_color_hex(0xF5F5F5), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    dial_shadow = lv_obj_create(root);
    lv_obj_set_size(dial_shadow, sternglas_map(390.0f) - sternglas_map(0.0f), sternglas_map(390.0f) - sternglas_map(0.0f));
    lv_obj_center(dial_shadow);
    lv_obj_set_style_radius(dial_shadow, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dial_shadow, lv_color_hex(0xFBFBFA), 0);
    lv_obj_set_style_border_width(dial_shadow, 0, 0);
    lv_obj_set_style_shadow_color(dial_shadow, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(dial_shadow, LV_OPA_30, 0);
    lv_obj_set_style_shadow_width(dial_shadow, 42, 0);
    lv_obj_set_style_shadow_spread(dial_shadow, 2, 0);
    lv_obj_set_style_shadow_offset_x(dial_shadow, 4, 0);
    lv_obj_set_style_shadow_offset_y(dial_shadow, 8, 0);
    lv_obj_set_style_pad_all(dial_shadow, 0, 0);

    dial_outer = lv_obj_create(root);
    lv_obj_set_size(dial_outer, sternglas_map(394.0f) - sternglas_map(0.0f), sternglas_map(394.0f) - sternglas_map(0.0f));
    lv_obj_center(dial_outer);
    lv_obj_set_style_radius(dial_outer, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(dial_outer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dial_outer, 3, 0);
    lv_obj_set_style_border_color(dial_outer, lv_color_hex(0xD0D0D0), 0);
    lv_obj_set_style_pad_all(dial_outer, 0, 0);

    dial_inner = lv_obj_create(root);
    lv_obj_set_size(dial_inner, sternglas_map(388.0f) - sternglas_map(0.0f), sternglas_map(388.0f) - sternglas_map(0.0f));
    lv_obj_center(dial_inner);
    lv_obj_set_style_radius(dial_inner, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(dial_inner, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dial_inner, 10, 0);
    lv_obj_set_style_border_color(dial_inner, lv_color_hex(0xE6E6E6), 0);
    lv_obj_set_style_pad_all(dial_inner, 0, 0);

    for (int i = 0; i < 48; ++i) {
        lv_obj_t *tick = lv_line_create(root);

        lv_obj_set_style_line_width(tick, 2, 0);
        lv_obj_set_style_line_color(tick, lv_color_hex(0x333333), 0);
        lv_obj_set_style_line_rounded(tick, false, 0);
        lv_line_set_points(tick, s_sternglas_minute_tick_pts[i], 2);
    }

    for (int i = 0; i < 6; ++i) {
        char hour_text[3];
        float x = 200.0f;
        float y = 55.0f;
        float angle = 0.0f;

        if (s_sternglas_even_hours[i] == 6) {
            y = 345.0f;
        } else if (s_sternglas_even_hours[i] == 4 || s_sternglas_even_hours[i] == 8) {
            float rotated_x;
            float rotated_y;

            angle = s_sternglas_even_hours[i] * 30.0f - 180.0f;
            sternglas_rotate_point(200.0f, 345.0f, angle, &rotated_x, &rotated_y);
            x = rotated_x;
            y = rotated_y;
        } else {
            float rotated_x;
            float rotated_y;

            angle = s_sternglas_even_hours[i] * 30.0f;
            sternglas_rotate_point(200.0f, 55.0f, angle, &rotated_x, &rotated_y);
            x = rotated_x;
            y = rotated_y;
            if (s_sternglas_even_hours[i] == 12) {
                angle = 0.0f;
            }
        }

        snprintf(hour_text, sizeof(hour_text), "%02d", s_sternglas_even_hours[i]);
        create_sternglas_label(root, hour_text, &jost_regular_34, lv_color_hex(0x222222), 0, x, y, angle);
    }

    for (int i = 0; i < 6; ++i) {
        lv_obj_t *hour_line = lv_line_create(root);

        lv_obj_set_style_line_width(hour_line, 3, 0);
        lv_obj_set_style_line_color(hour_line, lv_color_hex(0x222222), 0);
        lv_obj_set_style_line_rounded(hour_line, false, 0);
        lv_line_set_points(hour_line, s_sternglas_odd_hour_pts[i], 2);
    }

    create_sternglas_label(root, "STERNGLAS", &jost_medium_20, lv_color_hex(0x111111), 7, 200.0f, 130.0f, 0.0f);
    create_sternglas_label(root, "ZEITMESSER", &jost_regular_11, lv_color_hex(0x555555), 4, 200.0f, 148.0f, 0.0f);

    center_shadow = lv_obj_create(root);
    lv_obj_set_size(center_shadow, 24, 24);
    lv_obj_set_style_radius(center_shadow, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(center_shadow, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(center_shadow, LV_OPA_20, 0);
    lv_obj_set_style_border_width(center_shadow, 0, 0);
    lv_obj_set_style_pad_all(center_shadow, 0, 0);
    lv_obj_align(center_shadow, LV_ALIGN_CENTER, 3, 5);

    center_dot = lv_obj_create(root);
    lv_obj_set_size(center_dot, 5, 5);
    lv_obj_set_style_radius(center_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(center_dot, lv_color_hex(0x444444), 0);
    lv_obj_set_style_border_width(center_dot, 0, 0);
    lv_obj_set_style_pad_all(center_dot, 0, 0);
    lv_obj_set_pos(center_dot, sternglas_map(200.0f) - 2, sternglas_map(260.0f) - 2);

    for (int i = 0; i < 4; ++i) {
        lv_obj_t *wave = lv_line_create(root);

        lv_obj_set_style_line_width(wave, 2, 0);
        lv_obj_set_style_line_color(wave, lv_color_hex(0x444444), 0);
        lv_obj_set_style_line_rounded(wave, true, 0);
        lv_line_set_points(wave, s_sternglas_radio_wave_pts[i], 5);
    }

    create_sternglas_label(root, "RADIO", &jost_medium_10, lv_color_hex(0x444444), 2, 200.0f, 278.0f, 0.0f);
    create_sternglas_label(root, "CONTROLLED", &jost_medium_10, lv_color_hex(0x444444), 2, 200.0f, 287.0f, 0.0f);

    make_face_layer_passive(root);
    lv_obj_update_layout(root);

    ctx->faces.sternglas_snapshot_buf = lv_snapshot_create_draw_buf(root, LV_COLOR_FORMAT_RGB565);
    if (ctx->faces.sternglas_snapshot_buf != NULL &&
        lv_snapshot_take_to_draw_buf(root, LV_COLOR_FORMAT_RGB565, ctx->faces.sternglas_snapshot_buf) == LV_RESULT_OK) {
        ext_draw = (lv_coord_t)((int32_t)ctx->faces.sternglas_snapshot_buf->header.w - SCREEN_SIZE) / 2;
        if (ext_draw < 0) {
            ext_draw = 0;
        }
        ctx->faces.sternglas_snapshot_img = lv_image_create(parent);
        lv_obj_set_style_bg_opa(ctx->faces.sternglas_snapshot_img, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(ctx->faces.sternglas_snapshot_img, 0, 0);
        lv_obj_set_style_pad_all(ctx->faces.sternglas_snapshot_img, 0, 0);
        lv_obj_clear_flag(ctx->faces.sternglas_snapshot_img, LV_OBJ_FLAG_SCROLLABLE);
        lv_image_set_src(ctx->faces.sternglas_snapshot_img, ctx->faces.sternglas_snapshot_buf);
        lv_obj_set_pos(ctx->faces.sternglas_snapshot_img, -ext_draw, -ext_draw);
    }

    lv_obj_delete(root);

    if (ctx->faces.sternglas_hands_buf == NULL) {
        ctx->faces.sternglas_hands_buf = heap_caps_malloc(STERNGLAS_HANDS_CANVAS_SIZE * STERNGLAS_HANDS_CANVAS_SIZE * sizeof(lv_color32_t),
                                                          MALLOC_CAP_SPIRAM);
    }
    ctx->faces.sternglas_hands_canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(ctx->faces.sternglas_hands_canvas,
                         ctx->faces.sternglas_hands_buf,
                         STERNGLAS_HANDS_CANVAS_SIZE,
                         STERNGLAS_HANDS_CANVAS_SIZE,
                         LV_COLOR_FORMAT_ARGB8888);
    lv_obj_set_style_bg_opa(ctx->faces.sternglas_hands_canvas, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ctx->faces.sternglas_hands_canvas, 0, 0);
    lv_obj_set_style_pad_all(ctx->faces.sternglas_hands_canvas, 0, 0);
    lv_obj_center(ctx->faces.sternglas_hands_canvas);
    ctx->faces.sternglas_composite_img = lv_image_create(parent);
    lv_obj_set_style_bg_opa(ctx->faces.sternglas_composite_img, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ctx->faces.sternglas_composite_img, 0, 0);
    lv_obj_set_style_pad_all(ctx->faces.sternglas_composite_img, 0, 0);
    lv_obj_clear_flag(ctx->faces.sternglas_composite_img, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ctx->faces.sternglas_composite_img, LV_OBJ_FLAG_HIDDEN);
}

static void sternglas_transform_point(float x,
                                      float y,
                                      float angle_deg,
                                      int shadow_dx,
                                      int shadow_dy,
                                      lv_point_precise_t *out)
{
    float rx;
    float ry;

    sternglas_rotate_point(x, y, angle_deg, &rx, &ry);
    out->x = sternglas_map(rx) + shadow_dx;
    out->y = sternglas_map(ry) + shadow_dy;
}

static void draw_face_quad(lv_layer_t *layer,
                           const lv_point_precise_t *points,
                           lv_color_t color,
                           lv_opa_t opa);

static void draw_sternglas_hand_polygon(lv_layer_t *layer,
                                        const lv_point_precise_t *points,
                                        lv_color_t color,
                                        lv_opa_t opa)
{
    lv_point_precise_t base_center;
    lv_point_precise_t shoulder_center;
    lv_draw_line_dsc_t line_dsc;
    lv_draw_triangle_dsc_t tri_dsc;
    float dx = points[1].x - points[0].x;
    float dy = points[1].y - points[0].y;
    lv_coord_t shaft_width = LV_MAX(1, (lv_coord_t)lrintf(sqrtf(dx * dx + dy * dy)));

    base_center.x = (points[0].x + points[1].x) * 0.5f;
    base_center.y = (points[0].y + points[1].y) * 0.5f;
    shoulder_center.x = (points[2].x + points[4].x) * 0.5f;
    shoulder_center.y = (points[2].y + points[4].y) * 0.5f;

    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = color;
    line_dsc.opa = opa;
    line_dsc.round_start = 0;
    line_dsc.round_end = 0;
    line_dsc.width = shaft_width;
    line_dsc.p1 = base_center;
    line_dsc.p2 = shoulder_center;
    lv_draw_line(layer, &line_dsc);

    lv_draw_triangle_dsc_init(&tri_dsc);
    tri_dsc.color = color;
    tri_dsc.opa = opa;
    tri_dsc.p[0] = points[4];
    tri_dsc.p[1] = points[2];
    tri_dsc.p[2] = points[3];
    lv_draw_triangle(layer, &tri_dsc);
}

static void update_sternglas_face(clock_ui_context_t *ctx)
{
    struct tm ti = get_local_time_now();
    float hour_angle = ((ti.tm_hour % 12) + (ti.tm_min + ti.tm_sec / 60.0f) / 60.0f) * 30.0f;
    float min_angle = (ti.tm_min + ti.tm_sec / 60.0f) * 6.0f;
    lv_layer_t layer;
    lv_draw_rect_dsc_t dot_dsc;
    static const float s_minute_hand[5][2] = {
        {198.0f, 225.0f},
        {202.0f, 225.0f},
        {202.0f, 55.0f},
        {200.0f, 45.0f},
        {198.0f, 55.0f},
    };
    static const float s_hour_hand[5][2] = {
        {197.0f, 220.0f},
        {203.0f, 220.0f},
        {203.0f, 115.0f},
        {200.0f, 105.0f},
        {197.0f, 115.0f},
    };
    static const int s_shadow_offsets[3][2] = {
        {1, 2},
        {2, 3},
        {3, 5},
    };
    static const lv_opa_t s_shadow_opas[3] = {
        8,
        LV_OPA_10,
        LV_OPA_10,
    };
    lv_point_precise_t min_pts[5];
    lv_point_precise_t hour_pts[5];
    lv_point_precise_t shadow_pts[5];

    if (ctx->faces.sternglas_hands_canvas == NULL) {
        return;
    }

    for (int i = 0; i < 5; ++i) {
        sternglas_transform_point(s_minute_hand[i][0], s_minute_hand[i][1], min_angle, 0, 0, &min_pts[i]);
        sternglas_transform_point(s_hour_hand[i][0], s_hour_hand[i][1], hour_angle, 0, 0, &hour_pts[i]);
    }
    translate_precise_points(min_pts, 5, -STERNGLAS_HANDS_CANVAS_OFFSET, -STERNGLAS_HANDS_CANVAS_OFFSET);
    translate_precise_points(hour_pts, 5, -STERNGLAS_HANDS_CANVAS_OFFSET, -STERNGLAS_HANDS_CANVAS_OFFSET);

    lv_canvas_fill_bg(ctx->faces.sternglas_hands_canvas, lv_color_black(), LV_OPA_TRANSP);
    lv_canvas_init_layer(ctx->faces.sternglas_hands_canvas, &layer);

    for (int layer_idx = 0; layer_idx < 3; ++layer_idx) {
        for (int i = 0; i < 5; ++i) {
            sternglas_transform_point(s_minute_hand[i][0], s_minute_hand[i][1], min_angle,
                                      s_shadow_offsets[layer_idx][0], s_shadow_offsets[layer_idx][1], &shadow_pts[i]);
        }
        translate_precise_points(shadow_pts, 5, -STERNGLAS_HANDS_CANVAS_OFFSET, -STERNGLAS_HANDS_CANVAS_OFFSET);
        draw_sternglas_hand_polygon(&layer, shadow_pts, lv_color_black(), s_shadow_opas[layer_idx]);

        for (int i = 0; i < 5; ++i) {
            sternglas_transform_point(s_hour_hand[i][0], s_hour_hand[i][1], hour_angle,
                                      s_shadow_offsets[layer_idx][0], s_shadow_offsets[layer_idx][1], &shadow_pts[i]);
        }
        translate_precise_points(shadow_pts, 5, -STERNGLAS_HANDS_CANVAS_OFFSET, -STERNGLAS_HANDS_CANVAS_OFFSET);
        draw_sternglas_hand_polygon(&layer, shadow_pts, lv_color_black(), s_shadow_opas[layer_idx]);
    }

    draw_sternglas_hand_polygon(&layer, min_pts, lv_color_hex(0x104F8C), LV_OPA_COVER);
    draw_sternglas_hand_polygon(&layer, hour_pts, lv_color_hex(0x104F8C), LV_OPA_COVER);

    lv_draw_rect_dsc_init(&dot_dsc);
    dot_dsc.radius = LV_RADIUS_CIRCLE;
    dot_dsc.border_width = 0;
    dot_dsc.shadow_width = 0;
    dot_dsc.outline_width = 0;

    dot_dsc.bg_color = lv_color_black();
    for (int layer_idx = 0; layer_idx < 3; ++layer_idx) {
        lv_area_t shadow_area = {
            .x1 = STERNGLAS_HANDS_CENTER_X - 12 + s_shadow_offsets[layer_idx][0],
            .y1 = STERNGLAS_HANDS_CENTER_Y - 12 + s_shadow_offsets[layer_idx][1],
            .x2 = STERNGLAS_HANDS_CENTER_X + 11 + s_shadow_offsets[layer_idx][0],
            .y2 = STERNGLAS_HANDS_CENTER_Y + 11 + s_shadow_offsets[layer_idx][1],
        };

        dot_dsc.bg_opa = s_shadow_opas[layer_idx];
        lv_draw_rect(&layer, &dot_dsc, &shadow_area);
    }

    dot_dsc.bg_color = lv_color_hex(0x104F8C);
    dot_dsc.bg_opa = LV_OPA_COVER;
    lv_area_t pivot_area = {.x1 = STERNGLAS_HANDS_CENTER_X - 12, .y1 = STERNGLAS_HANDS_CENTER_Y - 12,
                            .x2 = STERNGLAS_HANDS_CENTER_X + 11, .y2 = STERNGLAS_HANDS_CENTER_Y + 11};
    lv_draw_rect(&layer, &dot_dsc, &pivot_area);

    dot_dsc.bg_color = lv_color_hex(0x1862A8);
    lv_area_t inner_area = {.x1 = STERNGLAS_HANDS_CENTER_X - 5, .y1 = STERNGLAS_HANDS_CENTER_Y - 5,
                            .x2 = STERNGLAS_HANDS_CENTER_X + 4, .y2 = STERNGLAS_HANDS_CENTER_Y + 4};
    lv_draw_rect(&layer, &dot_dsc, &inner_area);

    lv_canvas_finish_layer(ctx->faces.sternglas_hands_canvas, &layer);
    refresh_face_composite_snapshot(ctx, CLOCK_FACE_STERNGLAS,
                                    ctx->faces.sternglas_snapshot_img,
                                    ctx->faces.sternglas_hands_canvas,
                                    ctx->faces.sternglas_composite_img,
                                    &ctx->faces.sternglas_composite_buf);
}

static void avenir_transform_point(float x,
                                   float y,
                                   float angle_deg,
                                   int shadow_dx,
                                   int shadow_dy,
                                   lv_point_precise_t *out)
{
    float rx;
    float ry;

    avenir_rotate_point(x, y, angle_deg, &rx, &ry);
    out->x = avenir_map_x(rx) + shadow_dx;
    out->y = avenir_map_y(ry) + shadow_dy;
}

static void draw_avenir_triangle(lv_layer_t *layer,
                                 const lv_point_precise_t *points,
                                 lv_color_t color,
                                 lv_opa_t opa)
{
    lv_draw_triangle_dsc_t dsc;

    lv_draw_triangle_dsc_init(&dsc);
    dsc.color = color;
    dsc.opa = opa;
    dsc.p[0] = points[0];
    dsc.p[1] = points[1];
    dsc.p[2] = points[2];
    lv_draw_triangle(layer, &dsc);
}

static void draw_face_quad(lv_layer_t *layer,
                           const lv_point_precise_t *points,
                           lv_color_t color,
                           lv_opa_t opa)
{
    lv_draw_triangle_dsc_t dsc;

    lv_draw_triangle_dsc_init(&dsc);
    dsc.color = color;
    dsc.opa = opa;

    dsc.p[0] = points[0];
    dsc.p[1] = points[1];
    dsc.p[2] = points[2];
    lv_draw_triangle(layer, &dsc);

    dsc.p[0] = points[0];
    dsc.p[1] = points[2];
    dsc.p[2] = points[3];
    lv_draw_triangle(layer, &dsc);
}

void create_avenir_face(clock_ui_context_t *ctx, lv_obj_t *parent)
{
    static const int s_numbers[12] = {12, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    lv_obj_t *root;
    lv_obj_t *face;
    lv_obj_t *outer_ring;
    lv_obj_t *inner_ring;
    lv_coord_t ext_draw = 0;

    lv_obj_set_style_bg_color(parent, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    root = lv_obj_create(parent);
    lv_obj_set_size(root, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    face = lv_obj_create(root);
    lv_obj_set_size(face, avenir_size(340.0f), avenir_size(340.0f));
    lv_obj_center(face);
    lv_obj_set_style_radius(face, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(face, lv_color_hex(0xF5A65C), 0);
    lv_obj_set_style_bg_opa(face, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(face, 0, 0);
    lv_obj_set_style_pad_all(face, 0, 0);

    outer_ring = lv_obj_create(root);
    lv_obj_set_size(outer_ring, avenir_size(338.0f), avenir_size(338.0f));
    lv_obj_center(outer_ring);
    lv_obj_set_style_radius(outer_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(outer_ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(outer_ring, LV_MAX(1, avenir_size(3.0f)), 0);
    lv_obj_set_style_border_color(outer_ring, lv_color_hex(0xFBE8CF), 0);
    lv_obj_set_style_border_opa(outer_ring, LV_OPA_90, 0);
    lv_obj_set_style_pad_all(outer_ring, 0, 0);

    inner_ring = lv_obj_create(root);
    lv_obj_set_size(inner_ring, avenir_size(324.0f), avenir_size(324.0f));
    lv_obj_center(inner_ring);
    lv_obj_set_style_radius(inner_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(inner_ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(inner_ring, LV_MAX(1, avenir_size(1.5f)), 0);
    lv_obj_set_style_border_color(inner_ring, lv_color_hex(0xC9782F), 0);
    lv_obj_set_style_border_opa(inner_ring, LV_OPA_80, 0);
    lv_obj_set_style_pad_all(inner_ring, 0, 0);

    for (int i = 0; i < 60; ++i) {
        float angle = i * 6.0f;
        float x;
        float y;
        float radius = 145.0f;
        float width = (i % 5) == 0 ? 2.0f : 1.0f;
        float height = (i % 5) == 0 ? 14.0f : 7.0f;
        lv_obj_t *tick = lv_obj_create(root);

        avenir_rotate_point(AVENIR_CENTER_SRC, AVENIR_CENTER_SRC - radius, angle, &x, &y);
        lv_obj_set_size(tick, LV_MAX(1, avenir_size(width)), LV_MAX(1, avenir_size(height)));
        lv_obj_set_style_radius(tick, 0, 0);
        lv_obj_set_style_bg_color(tick, lv_color_hex(0x2A2A2A), 0);
        lv_obj_set_style_bg_opa(tick, (i % 5) == 0 ? LV_OPA_COVER : LV_OPA_60, 0);
        lv_obj_set_style_border_width(tick, 0, 0);
        lv_obj_set_style_pad_all(tick, 0, 0);
        lv_obj_set_pos(tick, avenir_map_x(x) - lv_obj_get_width(tick) / 2, avenir_map_y(y) - lv_obj_get_height(tick) / 2);
        lv_obj_set_style_transform_pivot_x(tick, lv_obj_get_width(tick) / 2, 0);
        lv_obj_set_style_transform_pivot_y(tick, lv_obj_get_height(tick) / 2, 0);
        lv_obj_set_style_transform_rotation(tick, (int32_t)lrintf(angle * 10.0f), 0);
    }

    for (int i = 0; i < 12; ++i) {
        char hour_text[3];
        float x;
        float y;
        float angle = (i == 0 ? 0.0f : s_numbers[i] * 30.0f);

        avenir_rotate_point(AVENIR_CENTER_SRC, AVENIR_CENTER_SRC - 114.0f, angle, &x, &y);
        snprintf(hour_text, sizeof(hour_text), "%d", s_numbers[i]);
        create_avenir_label(root, hour_text, &avenir_book_22, lv_color_hex(0x2A2A2A), 0, x, y);
    }

    create_avenir_label(root, "QUARTZ", &avenir_book_9, lv_color_hex(0x2A2A2A), 1, 170.0f, 224.0f);
    create_avenir_label(root, "ALARM CLOCK", &avenir_book_8, lv_color_hex(0x2A2A2A), 1, 170.0f, 237.0f);

    make_face_layer_passive(root);
    lv_obj_update_layout(root);

    ctx->faces.avenir_snapshot_buf = lv_snapshot_create_draw_buf(root, LV_COLOR_FORMAT_RGB565);
    if (ctx->faces.avenir_snapshot_buf != NULL &&
        lv_snapshot_take_to_draw_buf(root, LV_COLOR_FORMAT_RGB565, ctx->faces.avenir_snapshot_buf) == LV_RESULT_OK) {
        ext_draw = (lv_coord_t)((int32_t)ctx->faces.avenir_snapshot_buf->header.w - SCREEN_SIZE) / 2;
        if (ext_draw < 0) {
            ext_draw = 0;
        }
        ctx->faces.avenir_snapshot_img = lv_image_create(parent);
        lv_obj_set_style_bg_opa(ctx->faces.avenir_snapshot_img, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(ctx->faces.avenir_snapshot_img, 0, 0);
        lv_obj_set_style_pad_all(ctx->faces.avenir_snapshot_img, 0, 0);
        lv_obj_clear_flag(ctx->faces.avenir_snapshot_img, LV_OBJ_FLAG_SCROLLABLE);
        lv_image_set_src(ctx->faces.avenir_snapshot_img, ctx->faces.avenir_snapshot_buf);
        lv_obj_set_pos(ctx->faces.avenir_snapshot_img, -ext_draw, -ext_draw);
    }

    lv_obj_delete(root);

    if (ctx->faces.avenir_hands_buf == NULL) {
        ctx->faces.avenir_hands_buf = heap_caps_malloc(AVENIR_HANDS_CANVAS_SIZE * AVENIR_HANDS_CANVAS_SIZE * sizeof(lv_color32_t),
                                                       MALLOC_CAP_SPIRAM);
    }
    ctx->faces.avenir_hands_canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(ctx->faces.avenir_hands_canvas,
                         ctx->faces.avenir_hands_buf,
                         AVENIR_HANDS_CANVAS_SIZE,
                         AVENIR_HANDS_CANVAS_SIZE,
                         LV_COLOR_FORMAT_ARGB8888);
    lv_obj_set_style_bg_opa(ctx->faces.avenir_hands_canvas, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ctx->faces.avenir_hands_canvas, 0, 0);
    lv_obj_set_style_pad_all(ctx->faces.avenir_hands_canvas, 0, 0);
    lv_obj_center(ctx->faces.avenir_hands_canvas);
    ctx->faces.avenir_composite_img = lv_image_create(parent);
    lv_obj_set_style_bg_opa(ctx->faces.avenir_composite_img, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ctx->faces.avenir_composite_img, 0, 0);
    lv_obj_set_style_pad_all(ctx->faces.avenir_composite_img, 0, 0);
    lv_obj_clear_flag(ctx->faces.avenir_composite_img, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ctx->faces.avenir_composite_img, LV_OBJ_FLAG_HIDDEN);
}

static void update_avenir_face(clock_ui_context_t *ctx)
{
    struct tm ti = get_local_time_now();
    float hour_angle = (ti.tm_hour % 12) * 30.0f + ti.tm_min * 0.5f;
    float minute_angle = (ti.tm_min + ti.tm_sec / 60.0f) * 6.0f;
    float second_angle = ti.tm_sec * 6.0f;
    static const float s_hour_hand[3][2] = {
        {170.0f, 85.0f},
        {181.0f, 182.0f},
        {159.0f, 182.0f},
    };
    static const float s_minute_hand[3][2] = {
        {170.0f, 35.0f},
        {179.0f, 182.0f},
        {161.0f, 182.0f},
    };
    static const int s_shadow_offsets[2][2] = {
        {1, 2},
        {2, 3},
    };
    static const lv_opa_t s_shadow_opas[2] = {
        LV_OPA_20,
        LV_OPA_10,
    };
    lv_layer_t layer;
    lv_draw_rect_dsc_t dot_dsc;
    lv_draw_line_dsc_t second_dsc;
    lv_point_precise_t hour_pts[3];
    lv_point_precise_t minute_pts[3];
    lv_point_precise_t second_pts[2];
    lv_point_precise_t shadow_tri[3];
    lv_point_precise_t second_shadow_pts[2];

    if (ctx->faces.avenir_hands_canvas == NULL) {
        return;
    }

    for (int i = 0; i < 3; ++i) {
        avenir_transform_point(s_hour_hand[i][0], s_hour_hand[i][1], hour_angle, 0, 0, &hour_pts[i]);
        avenir_transform_point(s_minute_hand[i][0], s_minute_hand[i][1], minute_angle, 0, 0, &minute_pts[i]);
    }
    avenir_transform_point(170.0f, 52.0f, second_angle, 0, 0, &second_pts[0]);
    avenir_transform_point(170.0f, 214.0f, second_angle, 0, 0, &second_pts[1]);
    translate_precise_points(hour_pts, 3, -AVENIR_HANDS_CANVAS_OFFSET, -AVENIR_HANDS_CANVAS_OFFSET);
    translate_precise_points(minute_pts, 3, -AVENIR_HANDS_CANVAS_OFFSET, -AVENIR_HANDS_CANVAS_OFFSET);
    translate_precise_points(second_pts, 2, -AVENIR_HANDS_CANVAS_OFFSET, -AVENIR_HANDS_CANVAS_OFFSET);

    lv_canvas_fill_bg(ctx->faces.avenir_hands_canvas, lv_color_black(), LV_OPA_TRANSP);
    lv_canvas_init_layer(ctx->faces.avenir_hands_canvas, &layer);

    for (int layer_idx = 0; layer_idx < 2; ++layer_idx) {
        for (int i = 0; i < 3; ++i) {
            avenir_transform_point(s_hour_hand[i][0], s_hour_hand[i][1], hour_angle,
                                   s_shadow_offsets[layer_idx][0], s_shadow_offsets[layer_idx][1], &shadow_tri[i]);
        }
        translate_precise_points(shadow_tri, 3, -AVENIR_HANDS_CANVAS_OFFSET, -AVENIR_HANDS_CANVAS_OFFSET);
        draw_avenir_triangle(&layer, shadow_tri, lv_color_black(), s_shadow_opas[layer_idx]);

        for (int i = 0; i < 3; ++i) {
            avenir_transform_point(s_minute_hand[i][0], s_minute_hand[i][1], minute_angle,
                                   s_shadow_offsets[layer_idx][0], s_shadow_offsets[layer_idx][1], &shadow_tri[i]);
        }
        translate_precise_points(shadow_tri, 3, -AVENIR_HANDS_CANVAS_OFFSET, -AVENIR_HANDS_CANVAS_OFFSET);
        draw_avenir_triangle(&layer, shadow_tri, lv_color_black(), s_shadow_opas[layer_idx]);
    }

    draw_avenir_triangle(&layer, hour_pts, lv_color_hex(0x2A2A2A), LV_OPA_COVER);
    draw_avenir_triangle(&layer, minute_pts, lv_color_hex(0x2A2A2A), LV_OPA_COVER);

    lv_draw_line_dsc_init(&second_dsc);
    second_dsc.color = lv_color_black();
    second_dsc.round_start = 0;
    second_dsc.round_end = 0;
    second_dsc.width = LV_MAX(1, avenir_size(2.0f));
    for (int layer_idx = 0; layer_idx < 2; ++layer_idx) {
        second_shadow_pts[0].x = second_pts[0].x + s_shadow_offsets[layer_idx][0];
        second_shadow_pts[0].y = second_pts[0].y + s_shadow_offsets[layer_idx][1];
        second_shadow_pts[1].x = second_pts[1].x + s_shadow_offsets[layer_idx][0];
        second_shadow_pts[1].y = second_pts[1].y + s_shadow_offsets[layer_idx][1];
        second_dsc.opa = s_shadow_opas[layer_idx];
        second_dsc.p1 = second_shadow_pts[0];
        second_dsc.p2 = second_shadow_pts[1];
        lv_draw_line(&layer, &second_dsc);
    }
    second_dsc.color = lv_color_hex(0x2A2A2A);
    second_dsc.opa = LV_OPA_COVER;
    second_dsc.p1 = second_pts[0];
    second_dsc.p2 = second_pts[1];
    lv_draw_line(&layer, &second_dsc);

    lv_draw_rect_dsc_init(&dot_dsc);
    dot_dsc.radius = LV_RADIUS_CIRCLE;
    dot_dsc.border_width = 0;
    dot_dsc.shadow_width = 0;
    dot_dsc.outline_width = 0;
    dot_dsc.bg_color = lv_color_black();
    dot_dsc.bg_opa = LV_OPA_20;
    lv_area_t shadow_area = {
        .x1 = AVENIR_HANDS_CENTER_X - avenir_size(7.0f) + 1,
        .y1 = AVENIR_HANDS_CENTER_Y - avenir_size(7.0f) + 2,
        .x2 = AVENIR_HANDS_CENTER_X + avenir_size(7.0f) - 1 + 1,
        .y2 = AVENIR_HANDS_CENTER_Y + avenir_size(7.0f) - 1 + 2,
    };
    lv_draw_rect(&layer, &dot_dsc, &shadow_area);

    dot_dsc.bg_color = lv_color_hex(0x2A2A2A);
    dot_dsc.bg_opa = LV_OPA_COVER;
    lv_area_t pivot_area = {
        .x1 = AVENIR_HANDS_CENTER_X - avenir_size(7.0f),
        .y1 = AVENIR_HANDS_CENTER_Y - avenir_size(7.0f),
        .x2 = AVENIR_HANDS_CENTER_X + avenir_size(7.0f) - 1,
        .y2 = AVENIR_HANDS_CENTER_Y + avenir_size(7.0f) - 1,
    };
    lv_draw_rect(&layer, &dot_dsc, &pivot_area);

    lv_canvas_finish_layer(ctx->faces.avenir_hands_canvas, &layer);
    refresh_face_composite_snapshot(ctx, CLOCK_FACE_AVENIR,
                                    ctx->faces.avenir_snapshot_img,
                                    ctx->faces.avenir_hands_canvas,
                                    ctx->faces.avenir_composite_img,
                                    &ctx->faces.avenir_composite_buf);
}

static void modern_transform_point(float x,
                                   float y,
                                   float angle_deg,
                                   int shadow_dx,
                                   int shadow_dy,
                                   lv_point_precise_t *out)
{
    float rx;
    float ry;

    modern_rotate_point(x, y, angle_deg, &rx, &ry);
    out->x = modern_map_x(rx) + shadow_dx;
    out->y = modern_map_y(ry) + shadow_dy;
}

void create_modern_silver_face(clock_ui_context_t *ctx, lv_obj_t *parent)
{
    lv_obj_t *root;
    lv_obj_t *outer_ring;
    lv_obj_t *inner_ring;
    lv_obj_t *dial;
    lv_coord_t ext_draw = 0;

    lv_obj_set_style_bg_color(parent, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    root = lv_obj_create(parent);
    lv_obj_set_size(root, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    outer_ring = lv_obj_create(root);
    lv_obj_set_size(outer_ring, modern_size(340.0f), modern_size(340.0f));
    lv_obj_center(outer_ring);
    lv_obj_set_style_radius(outer_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(outer_ring, lv_color_hex(0xE7EAED), 0);
    lv_obj_set_style_bg_opa(outer_ring, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(outer_ring, modern_size(2.0f), 0);
    lv_obj_set_style_border_color(outer_ring, lv_color_hex(0xC8CDD2), 0);
    lv_obj_set_style_shadow_width(outer_ring, 0, 0);
    lv_obj_set_style_pad_all(outer_ring, 0, 0);

    inner_ring = lv_obj_create(outer_ring);
    lv_obj_set_size(inner_ring, modern_size(324.0f), modern_size(324.0f));
    lv_obj_center(inner_ring);
    lv_obj_set_style_radius(inner_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(inner_ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(inner_ring, modern_size(1.0f), 0);
    lv_obj_set_style_border_color(inner_ring, lv_color_hex(0xD9DDE1), 0);
    lv_obj_set_style_pad_all(inner_ring, 0, 0);

    dial = lv_obj_create(outer_ring);
    lv_obj_set_size(dial, modern_size(314.0f), modern_size(314.0f));
    lv_obj_center(dial);
    lv_obj_set_style_radius(dial, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dial, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(dial, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dial, 0, 0);
    lv_obj_set_style_pad_all(dial, 0, 0);

    for (int i = 0; i < 60; ++i) {
        float angle = i * 6.0f;
        float x;
        float y;
        lv_obj_t *dot = lv_obj_create(root);
        float radius = 146.0f;
        float size = (i % 5) == 0 ? 6.0f : 3.0f;

        modern_rotate_point(MODERN_CENTER_SRC, MODERN_CENTER_SRC - radius, angle, &x, &y);
        lv_obj_set_size(dot, modern_size(size), modern_size(size));
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, lv_color_hex(0x111111), 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_set_style_pad_all(dot, 0, 0);
        lv_obj_set_pos(dot,
                       modern_map_x(x) - lv_obj_get_width(dot) / 2,
                       modern_map_y(y) - lv_obj_get_height(dot) / 2);
    }

    create_modern_label(root, "GEORG JENSEN", &helvetica_neue_10, lv_color_hex(0x111111), LV_OPA_80, 1, 170.0f, 228.0f);
    create_modern_label(root, "DENMARK", &helvetica_neue_6, lv_color_hex(0x666666), LV_OPA_80, 1, 170.0f, 241.0f);

    make_face_layer_passive(root);
    lv_obj_update_layout(root);

    ctx->faces.modern_silver_snapshot_buf = lv_snapshot_create_draw_buf(root, LV_COLOR_FORMAT_RGB565);
    if (ctx->faces.modern_silver_snapshot_buf != NULL &&
        lv_snapshot_take_to_draw_buf(root, LV_COLOR_FORMAT_RGB565, ctx->faces.modern_silver_snapshot_buf) == LV_RESULT_OK) {
        ext_draw = (lv_coord_t)((int32_t)ctx->faces.modern_silver_snapshot_buf->header.w - SCREEN_SIZE) / 2;
        if (ext_draw < 0) {
            ext_draw = 0;
        }
        ctx->faces.modern_silver_snapshot_img = lv_image_create(parent);
        lv_obj_set_style_bg_opa(ctx->faces.modern_silver_snapshot_img, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(ctx->faces.modern_silver_snapshot_img, 0, 0);
        lv_obj_set_style_pad_all(ctx->faces.modern_silver_snapshot_img, 0, 0);
        lv_obj_clear_flag(ctx->faces.modern_silver_snapshot_img, LV_OBJ_FLAG_SCROLLABLE);
        lv_image_set_src(ctx->faces.modern_silver_snapshot_img, ctx->faces.modern_silver_snapshot_buf);
        lv_obj_set_pos(ctx->faces.modern_silver_snapshot_img, -ext_draw, -ext_draw);
    }

    lv_obj_delete(root);

    if (ctx->faces.modern_silver_hands_buf == NULL) {
        ctx->faces.modern_silver_hands_buf = heap_caps_malloc(MODERN_HANDS_CANVAS_SIZE * MODERN_HANDS_CANVAS_SIZE * sizeof(lv_color32_t),
                                                              MALLOC_CAP_SPIRAM);
    }
    ctx->faces.modern_silver_hands_canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(ctx->faces.modern_silver_hands_canvas,
                         ctx->faces.modern_silver_hands_buf,
                         MODERN_HANDS_CANVAS_SIZE,
                         MODERN_HANDS_CANVAS_SIZE,
                         LV_COLOR_FORMAT_ARGB8888);
    lv_obj_set_style_bg_opa(ctx->faces.modern_silver_hands_canvas, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ctx->faces.modern_silver_hands_canvas, 0, 0);
    lv_obj_set_style_pad_all(ctx->faces.modern_silver_hands_canvas, 0, 0);
    lv_obj_center(ctx->faces.modern_silver_hands_canvas);
    ctx->faces.modern_silver_composite_img = lv_image_create(parent);
    lv_obj_set_style_bg_opa(ctx->faces.modern_silver_composite_img, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ctx->faces.modern_silver_composite_img, 0, 0);
    lv_obj_set_style_pad_all(ctx->faces.modern_silver_composite_img, 0, 0);
    lv_obj_clear_flag(ctx->faces.modern_silver_composite_img, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ctx->faces.modern_silver_composite_img, LV_OBJ_FLAG_HIDDEN);
}

static void update_modern_silver_face(clock_ui_context_t *ctx)
{
    struct tm ti = get_local_time_now();
    float hour_angle = (ti.tm_hour % 12) * 30.0f + ti.tm_min * 0.5f;
    float minute_angle = (ti.tm_min + ti.tm_sec / 60.0f) * 6.0f;
    float second_angle = ti.tm_sec * 6.0f;
    static const float s_hour_hand[4][2] = {
        {170.0f, 72.0f},
        {173.5f, 146.0f},
        {170.0f, 170.0f},
        {166.5f, 146.0f},
    };
    static const float s_minute_hand[4][2] = {
        {170.0f, 28.0f},
        {172.8f, 138.0f},
        {170.0f, 170.0f},
        {167.2f, 138.0f},
    };
    static const int s_shadow_offsets[2][2] = {
        {1, 2},
        {2, 3},
    };
    static const lv_opa_t s_shadow_opas[2] = {
        LV_OPA_20,
        LV_OPA_10,
    };
    lv_layer_t layer;
    lv_draw_rect_dsc_t dot_dsc;
    lv_draw_line_dsc_t second_dsc;
    lv_point_precise_t hour_pts[4];
    lv_point_precise_t minute_pts[4];
    lv_point_precise_t second_pts[2];
    lv_point_precise_t shadow_quad[4];
    lv_point_precise_t second_shadow_pts[2];

    if (ctx->faces.modern_silver_hands_canvas == NULL) {
        return;
    }

    for (int i = 0; i < 4; ++i) {
        modern_transform_point(s_hour_hand[i][0], s_hour_hand[i][1], hour_angle, 0, 0, &hour_pts[i]);
        modern_transform_point(s_minute_hand[i][0], s_minute_hand[i][1], minute_angle, 0, 0, &minute_pts[i]);
    }
    modern_transform_point(170.0f, 44.0f, second_angle, 0, 0, &second_pts[0]);
    modern_transform_point(170.0f, 198.0f, second_angle, 0, 0, &second_pts[1]);
    translate_precise_points(hour_pts, 4, -MODERN_HANDS_CANVAS_OFFSET, -MODERN_HANDS_CANVAS_OFFSET);
    translate_precise_points(minute_pts, 4, -MODERN_HANDS_CANVAS_OFFSET, -MODERN_HANDS_CANVAS_OFFSET);
    translate_precise_points(second_pts, 2, -MODERN_HANDS_CANVAS_OFFSET, -MODERN_HANDS_CANVAS_OFFSET);

    lv_canvas_fill_bg(ctx->faces.modern_silver_hands_canvas, lv_color_black(), LV_OPA_TRANSP);
    lv_canvas_init_layer(ctx->faces.modern_silver_hands_canvas, &layer);

    for (int layer_idx = 0; layer_idx < 2; ++layer_idx) {
        for (int i = 0; i < 4; ++i) {
            modern_transform_point(s_hour_hand[i][0], s_hour_hand[i][1], hour_angle,
                                   s_shadow_offsets[layer_idx][0], s_shadow_offsets[layer_idx][1], &shadow_quad[i]);
        }
        translate_precise_points(shadow_quad, 4, -MODERN_HANDS_CANVAS_OFFSET, -MODERN_HANDS_CANVAS_OFFSET);
        draw_face_quad(&layer, shadow_quad, lv_color_black(), s_shadow_opas[layer_idx]);

        for (int i = 0; i < 4; ++i) {
            modern_transform_point(s_minute_hand[i][0], s_minute_hand[i][1], minute_angle,
                                   s_shadow_offsets[layer_idx][0], s_shadow_offsets[layer_idx][1], &shadow_quad[i]);
        }
        translate_precise_points(shadow_quad, 4, -MODERN_HANDS_CANVAS_OFFSET, -MODERN_HANDS_CANVAS_OFFSET);
        draw_face_quad(&layer, shadow_quad, lv_color_black(), s_shadow_opas[layer_idx]);
    }

    draw_face_quad(&layer, hour_pts, lv_color_hex(0x111111), LV_OPA_COVER);
    draw_face_quad(&layer, minute_pts, lv_color_hex(0x111111), LV_OPA_COVER);

    lv_draw_line_dsc_init(&second_dsc);
    second_dsc.color = lv_color_hex(0x0095FF);
    second_dsc.round_start = 0;
    second_dsc.round_end = 0;
    second_dsc.width = LV_MAX(1, modern_size(2.0f));
    for (int layer_idx = 0; layer_idx < 2; ++layer_idx) {
        second_shadow_pts[0].x = second_pts[0].x + s_shadow_offsets[layer_idx][0];
        second_shadow_pts[0].y = second_pts[0].y + s_shadow_offsets[layer_idx][1];
        second_shadow_pts[1].x = second_pts[1].x + s_shadow_offsets[layer_idx][0];
        second_shadow_pts[1].y = second_pts[1].y + s_shadow_offsets[layer_idx][1];
        second_dsc.color = lv_color_black();
        second_dsc.opa = s_shadow_opas[layer_idx];
        second_dsc.p1 = second_shadow_pts[0];
        second_dsc.p2 = second_shadow_pts[1];
        lv_draw_line(&layer, &second_dsc);
    }
    second_dsc.color = lv_color_hex(0x0095FF);
    second_dsc.opa = LV_OPA_COVER;
    second_dsc.p1 = second_pts[0];
    second_dsc.p2 = second_pts[1];
    lv_draw_line(&layer, &second_dsc);

    lv_draw_rect_dsc_init(&dot_dsc);
    dot_dsc.radius = LV_RADIUS_CIRCLE;
    dot_dsc.border_width = 0;
    dot_dsc.shadow_width = 0;
    dot_dsc.outline_width = 0;
    dot_dsc.bg_color = lv_color_black();
    dot_dsc.bg_opa = LV_OPA_20;
    lv_area_t shadow_area = {
        .x1 = MODERN_HANDS_CENTER_X - modern_size(11.0f) + 1,
        .y1 = MODERN_HANDS_CENTER_Y - modern_size(11.0f) + 2,
        .x2 = MODERN_HANDS_CENTER_X + modern_size(11.0f) - 1 + 1,
        .y2 = MODERN_HANDS_CENTER_Y + modern_size(11.0f) - 1 + 2,
    };
    lv_draw_rect(&layer, &dot_dsc, &shadow_area);

    dot_dsc.bg_color = lv_color_hex(0x111111);
    dot_dsc.bg_opa = LV_OPA_COVER;
    lv_area_t pivot_area = {
        .x1 = MODERN_HANDS_CENTER_X - modern_size(11.0f),
        .y1 = MODERN_HANDS_CENTER_Y - modern_size(11.0f),
        .x2 = MODERN_HANDS_CENTER_X + modern_size(11.0f) - 1,
        .y2 = MODERN_HANDS_CENTER_Y + modern_size(11.0f) - 1,
    };
    lv_draw_rect(&layer, &dot_dsc, &pivot_area);

    lv_canvas_finish_layer(ctx->faces.modern_silver_hands_canvas, &layer);
    refresh_face_composite_snapshot(ctx, CLOCK_FACE_MODERN_SILVER,
                                    ctx->faces.modern_silver_snapshot_img,
                                    ctx->faces.modern_silver_hands_canvas,
                                    ctx->faces.modern_silver_composite_img,
                                    &ctx->faces.modern_silver_composite_buf);
}
