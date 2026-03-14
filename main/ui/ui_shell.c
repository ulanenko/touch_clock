#include "ui/clock_ui_private.h"

#include "domain/face_catalog.h"

static uint8_t face_theme_value(const clock_ui_context_t *ctx, clock_face_id_t face)
{
    if (ctx == NULL || ctx->settings == NULL || !clock_face_is_valid(face)) {
        return 0;
    }

    return (ctx->settings->face_themes[face] < CLOCK_FACE_THEME_COUNT) ? ctx->settings->face_themes[face] : 0;
}

static int32_t face_theme_current_y(const clock_ui_context_t *ctx);

static void sync_active_face_visual_state(clock_ui_context_t *ctx)
{
    if (ctx == NULL || ctx->tileview == NULL) {
        return;
    }

    sync_face_animation_state(ctx, tile_to_face(ctx, lv_tileview_get_tile_active(ctx->tileview)));
}

static void snapshot_obj_to_image(lv_obj_t *source, lv_obj_t *image, lv_draw_buf_t **draw_buf)
{
    lv_coord_t ext_draw;

    if (source == NULL || image == NULL || draw_buf == NULL) {
        return;
    }

    if (*draw_buf == NULL || lv_snapshot_reshape_draw_buf(source, *draw_buf) != LV_RESULT_OK) {
        if (*draw_buf != NULL) {
            lv_draw_buf_destroy(*draw_buf);
        }
        *draw_buf = lv_snapshot_create_draw_buf(source, LV_COLOR_FORMAT_RGB565);
        if (*draw_buf == NULL) {
            return;
        }
    }

    if (lv_snapshot_take_to_draw_buf(source, LV_COLOR_FORMAT_RGB565, *draw_buf) != LV_RESULT_OK) {
        return;
    }

    ext_draw = (lv_coord_t)((int32_t)(*draw_buf)->header.w - SCREEN_SIZE) / 2;
    if (ext_draw < 0) {
        ext_draw = 0;
    }

    lv_image_set_src(image, *draw_buf);
    lv_obj_set_pos(image, -ext_draw, -ext_draw);
}

static void face_theme_set_snapshot_visible(clock_ui_context_t *ctx, bool visible)
{
    if (ctx == NULL || ctx->face_theme.panel == NULL || ctx->face_theme.snapshot_img == NULL) {
        return;
    }

    if (ctx->face_theme.snapshot_visible == visible) {
        return;
    }

    if (visible) {
        if (ctx->face_theme.snapshot_dirty || ctx->face_theme.snapshot_buf == NULL) {
            snapshot_obj_to_image(ctx->face_theme.panel, ctx->face_theme.snapshot_img, &ctx->face_theme.snapshot_buf);
            ctx->face_theme.snapshot_dirty = false;
        }
        lv_obj_clear_flag(ctx->face_theme.snapshot_img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ctx->face_theme.panel, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_set_y(ctx->face_theme.panel, face_theme_current_y(ctx));
        lv_obj_add_flag(ctx->face_theme.snapshot_img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ctx->face_theme.panel, LV_OBJ_FLAG_HIDDEN);
    }

    ctx->face_theme.snapshot_visible = visible;
}

static const char *face_theme_name(clock_face_id_t face, uint8_t theme)
{
    switch (face) {
    case CLOCK_FACE_DIGITAL:
        switch (theme) {
        case 1U:
            return "Ice Blue";
        case 2U:
            return "Amber";
        default:
            return "Emerald";
        }
    case CLOCK_FACE_MATRIX:
        switch (theme) {
        case 1U:
            return "Amber Grid";
        case 2U:
            return "Blue Grid";
        default:
            return "Green Grid";
        }
    case CLOCK_FACE_WHARTON:
        switch (theme) {
        case 1U:
            return "Ruby Ring";
        case 2U:
            return "Polar Ring";
        default:
            return "Brass Ring";
        }
    case CLOCK_FACE_STERNGLAS:
        switch (theme) {
        case 1U:
            return "Copper Hands";
        case 2U:
            return "Night Dial";
        default:
            return "Blue Hands";
        }
    case CLOCK_FACE_AVENIR:
        switch (theme) {
        case 1U:
            return "Navy";
        case 2U:
            return "Forest Dial";
        default:
            return "Charcoal";
        }
    case CLOCK_FACE_MODERN_SILVER:
        switch (theme) {
        case 1U:
            return "Crimson Accent";
        case 2U:
            return "Graphite Dial";
        default:
            return "Azure Accent";
        }
    default:
        return (theme == 2U) ? "Theme 3" : ((theme == 1U) ? "Theme 2" : "Theme 1");
    }
}

static void face_theme_update_visual_state(clock_ui_context_t *ctx, int32_t panel_y)
{
    if (ctx == NULL || ctx->face_theme.overlay == NULL || ctx->face_theme.panel == NULL) {
        return;
    }

    int32_t clamped_y = LV_CLAMP(FACE_THEME_SHEET_CLOSED_Y, panel_y, FACE_THEME_SHEET_OPEN_Y);
    int32_t travel = FACE_THEME_SHEET_OPEN_Y - FACE_THEME_SHEET_CLOSED_Y;
    int32_t progress = clamped_y - FACE_THEME_SHEET_CLOSED_Y;
    lv_opa_t opa = (lv_opa_t)((progress * FACE_THEME_SCRIM_OPA) / travel);

    if (ctx->face_theme.snapshot_visible && ctx->face_theme.snapshot_img != NULL) {
        lv_coord_t ext_draw = 0;

        if (ctx->face_theme.snapshot_buf != NULL &&
            ctx->face_theme.snapshot_buf->header.w > SCREEN_SIZE) {
            ext_draw = (lv_coord_t)((int32_t)ctx->face_theme.snapshot_buf->header.w - SCREEN_SIZE) / 2;
        }
        lv_obj_set_pos(ctx->face_theme.snapshot_img, -ext_draw, clamped_y - ext_draw);
        lv_obj_set_style_bg_opa(ctx->face_theme.overlay, FACE_THEME_SCRIM_OPA, 0);
        return;
    }

    lv_obj_set_y(ctx->face_theme.panel, clamped_y);
    lv_obj_set_style_bg_opa(ctx->face_theme.overlay, opa, 0);
}

static int32_t face_theme_current_y(const clock_ui_context_t *ctx)
{
    lv_coord_t ext_draw = 0;

    if (ctx == NULL || ctx->face_theme.panel == NULL) {
        return FACE_THEME_SHEET_CLOSED_Y;
    }

    if (!ctx->face_theme.snapshot_visible || ctx->face_theme.snapshot_img == NULL) {
        return lv_obj_get_y(ctx->face_theme.panel);
    }

    if (ctx->face_theme.snapshot_buf != NULL &&
        ctx->face_theme.snapshot_buf->header.w > SCREEN_SIZE) {
        ext_draw = (lv_coord_t)((int32_t)ctx->face_theme.snapshot_buf->header.w - SCREEN_SIZE) / 2;
    }

    return lv_obj_get_y(ctx->face_theme.snapshot_img) + ext_draw;
}

static void face_theme_sheet_anim_cb(void *obj, int32_t value)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_obj_get_user_data((lv_obj_t *)obj);

    if (ctx == NULL) {
        return;
    }

    face_theme_update_visual_state(ctx, value);
}

static void face_theme_prepare_overlay(clock_ui_context_t *ctx)
{
    if (ctx == NULL || ctx->face_theme.overlay == NULL) {
        return;
    }

    ctx->face_theme.overlay_open = true;
    lv_obj_clear_flag(ctx->face_theme.overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ctx->face_theme.overlay);
    sync_active_face_visual_state(ctx);
}

static void face_theme_sheet_anim_ready_cb(lv_anim_t *anim)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_anim_get_user_data(anim);

    if (ctx == NULL) {
        return;
    }

    ctx->face_theme.animating = false;
    ctx->face_theme.dragging = false;
    ctx->face_theme.drag_from_face = false;
    face_theme_set_snapshot_visible(ctx, false);

    if (!ctx->face_theme.target_open) {
        ctx->face_theme.overlay_open = false;
        lv_obj_add_flag(ctx->face_theme.overlay, LV_OBJ_FLAG_HIDDEN);
        face_theme_update_visual_state(ctx, FACE_THEME_SHEET_CLOSED_Y);
    } else {
        face_theme_update_visual_state(ctx, FACE_THEME_SHEET_OPEN_Y);
    }

    sync_active_face_visual_state(ctx);
}

static uint32_t face_theme_sheet_anim_duration(int32_t from_y, int32_t to_y)
{
    int32_t distance = LV_ABS(to_y - from_y);
    int32_t travel = FACE_THEME_SHEET_OPEN_Y - FACE_THEME_SHEET_CLOSED_Y;

    if (travel <= 0) {
        return FACE_THEME_SHEET_SHOW_MS;
    }

    return (uint32_t)LV_CLAMP(110, 110 + ((distance * 150) / travel), 260);
}

static void face_theme_animate_to(clock_ui_context_t *ctx, int32_t target_y)
{
    lv_anim_t anim;
    int32_t current_y;

    if (ctx == NULL || ctx->face_theme.overlay == NULL || ctx->face_theme.panel == NULL) {
        return;
    }

    target_y = LV_CLAMP(FACE_THEME_SHEET_CLOSED_Y, target_y, FACE_THEME_SHEET_OPEN_Y);
    current_y = face_theme_current_y(ctx);

    lv_anim_delete(ctx->face_theme.panel, face_theme_sheet_anim_cb);
    ctx->face_theme.animating = false;

    if (current_y == target_y) {
        ctx->face_theme.target_open = (target_y >= FACE_THEME_SHEET_OPEN_Y);
        if (ctx->face_theme.target_open) {
            face_theme_prepare_overlay(ctx);
            face_theme_update_visual_state(ctx, FACE_THEME_SHEET_OPEN_Y);
        } else {
            ctx->face_theme.overlay_open = false;
            face_theme_update_visual_state(ctx, FACE_THEME_SHEET_CLOSED_Y);
            lv_obj_add_flag(ctx->face_theme.overlay, LV_OBJ_FLAG_HIDDEN);
        }
        ctx->face_theme.dragging = false;
        ctx->face_theme.drag_from_face = false;
        return;
    }

    face_theme_prepare_overlay(ctx);
    face_theme_set_snapshot_visible(ctx, true);
    ctx->face_theme.target_open = (target_y >= FACE_THEME_SHEET_OPEN_Y);
    ctx->face_theme.animating = true;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, ctx->face_theme.panel);
    lv_anim_set_user_data(&anim, ctx);
    lv_anim_set_exec_cb(&anim, face_theme_sheet_anim_cb);
    lv_anim_set_values(&anim, current_y, target_y);
    lv_anim_set_time(&anim, face_theme_sheet_anim_duration(current_y, target_y));
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_set_ready_cb(&anim, face_theme_sheet_anim_ready_cb);
    lv_anim_start(&anim);
}

static void face_theme_preview_colors(clock_face_id_t face, uint8_t theme, uint32_t colors[3])
{
    if (colors == NULL) {
        return;
    }

    switch (face) {
    case CLOCK_FACE_DIGITAL:
        if (theme == 1U) {
            colors[0] = 0x02060B;
            colors[1] = 0x123248;
            colors[2] = 0x6FD8FF;
        } else if (theme == 2U) {
            colors[0] = 0x090401;
            colors[1] = 0x4A2B12;
            colors[2] = 0xFFB347;
        } else {
            colors[0] = 0x010401;
            colors[1] = 0x143C14;
            colors[2] = 0x5CFB5C;
        }
        break;
    case CLOCK_FACE_MATRIX:
        if (theme == 1U) {
            colors[0] = 0x070502;
            colors[1] = 0x1C1205;
            colors[2] = 0xFFB200;
        } else if (theme == 2U) {
            colors[0] = 0x03070C;
            colors[1] = 0x0C1A28;
            colors[2] = 0x5CC8FF;
        } else {
            colors[0] = 0x050505;
            colors[1] = 0x071107;
            colors[2] = 0x00CC44;
        }
        break;
    case CLOCK_FACE_WHARTON:
        if (theme == 1U) {
            colors[0] = 0x0A0507;
            colors[1] = 0x1F0A10;
            colors[2] = 0xFF5C7A;
        } else if (theme == 2U) {
            colors[0] = 0x04090A;
            colors[1] = 0x0D2124;
            colors[2] = 0x7FE7FF;
        } else {
            colors[0] = 0x0A0907;
            colors[1] = 0x1E1600;
            colors[2] = 0xD4A017;
        }
        break;
    case CLOCK_FACE_STERNGLAS:
        if (theme == 1U) {
            colors[0] = 0xF5F5F5;
            colors[1] = 0x9A5B2A;
            colors[2] = 0xC97B42;
        } else if (theme == 2U) {
            colors[0] = 0x14181D;
            colors[1] = 0x87B7FF;
            colors[2] = 0xCFE2FF;
        } else {
            colors[0] = 0xF5F5F5;
            colors[1] = 0x104F8C;
            colors[2] = 0x1862A8;
        }
        break;
    case CLOCK_FACE_AVENIR:
        if (theme == 1U) {
            colors[0] = 0xF5A65C;
            colors[1] = 0x173A5E;
            colors[2] = 0x295B86;
        } else if (theme == 2U) {
            colors[0] = 0x26493F;
            colors[1] = 0xA4C3B0;
            colors[2] = 0xF3E7D0;
        } else {
            colors[0] = 0xF5A65C;
            colors[1] = 0xC9782F;
            colors[2] = 0x2A2A2A;
        }
        break;
    case CLOCK_FACE_MODERN_SILVER:
        if (theme == 1U) {
            colors[0] = 0xE7EAED;
            colors[1] = 0x1B1B1B;
            colors[2] = 0xE04545;
        } else if (theme == 2U) {
            colors[0] = 0x0F1215;
            colors[1] = 0xF1F3F5;
            colors[2] = 0xFF9B42;
        } else {
            colors[0] = 0xE7EAED;
            colors[1] = 0x111111;
            colors[2] = 0x0095FF;
        }
        break;
    default:
        colors[0] = 0x1A1A1A;
        colors[1] = 0x3A3A3A;
        colors[2] = 0xD8DDE3;
        break;
    }
}

bool face_theme_overlay_is_open(const clock_ui_context_t *ctx)
{
    return ctx != NULL &&
           ctx->face_theme.overlay_open &&
           ctx->face_theme.overlay != NULL &&
           !lv_obj_has_flag(ctx->face_theme.overlay, LV_OBJ_FLAG_HIDDEN);
}

void hide_face_theme_button(clock_ui_context_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    ctx->face_theme.button_visible = false;
    if (ctx->face_theme.button != NULL) {
        lv_obj_add_flag(ctx->face_theme.button, LV_OBJ_FLAG_HIDDEN);
    }
}

void face_theme_overlay_close(clock_ui_context_t *ctx)
{
    if (ctx == NULL || ctx->face_theme.overlay == NULL || ctx->face_theme.panel == NULL) {
        return;
    }

    if (lv_obj_has_flag(ctx->face_theme.overlay, LV_OBJ_FLAG_HIDDEN) && !ctx->face_theme.animating) {
        ctx->face_theme.overlay_open = false;
        return;
    }

    face_theme_animate_to(ctx, FACE_THEME_SHEET_CLOSED_Y);
}

static void sync_face_theme_overlay(clock_ui_context_t *ctx)
{
    char title[64];
    clock_face_id_t face;
    uint8_t selected_theme;

    if (ctx == NULL || ctx->face_theme.overlay == NULL) {
        return;
    }

    face = sanitize_enabled_face(ctx->face_theme.picker_face);
    selected_theme = face_theme_value(ctx, face);
    snprintf(title, sizeof(title), "Themes");
    lv_label_set_text(ctx->face_theme.title, title);

    for (uint8_t theme = 0; theme < CLOCK_FACE_THEME_COUNT; ++theme) {
        uint32_t swatches[3];
        bool selected = (theme == selected_theme);

        ctx->face_theme.option_ctx[theme].ui = ctx;
        ctx->face_theme.option_ctx[theme].face = face;
        ctx->face_theme.option_ctx[theme].theme = theme;
        lv_label_set_text(ctx->face_theme.option_title[theme], face_theme_name(face, theme));
        face_theme_preview_colors(face, theme, swatches);
        for (int swatch = 0; swatch < 3; ++swatch) {
            lv_obj_set_style_bg_color(ctx->face_theme.option_swatches[theme][swatch],
                                      lv_color_hex(swatches[swatch]),
                                      0);
        }

        lv_obj_set_style_border_width(ctx->face_theme.option_card[theme], selected ? 2 : 1, 0);
        lv_obj_set_style_border_color(ctx->face_theme.option_card[theme],
                                      selected ? lv_color_hex(UI_ACCENT_COL) : lv_color_hex(0x2C2C2C),
                                      0);
        lv_obj_set_style_bg_color(ctx->face_theme.option_card[theme],
                                  selected ? lv_color_hex(0x1F2428) : lv_color_hex(0x171717),
                                  0);
    }

    ctx->face_theme.snapshot_dirty = true;
}

static void face_theme_option_event_cb(lv_event_t *event)
{
    face_theme_option_ctx_t *option_ctx = (face_theme_option_ctx_t *)lv_event_get_user_data(event);

    if (option_ctx == NULL || option_ctx->ui == NULL) {
        return;
    }

    request_set_face_theme(option_ctx->ui, option_ctx->face, option_ctx->theme);
    face_theme_overlay_close(option_ctx->ui);
}

static void face_theme_overlay_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    lv_event_code_t code = lv_event_get_code(event);
    lv_indev_t *indev = lv_event_get_indev(event);
    lv_obj_t *target = lv_event_get_target(event);
    lv_point_t point;
    lv_coord_t dy;
    int32_t target_y;
    int32_t close_threshold_y = -((FACE_THEME_SHEET_OPEN_Y - FACE_THEME_SHEET_CLOSED_Y) * 20 / 100);

    if (ctx == NULL) {
        return;
    }

    if ((code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING ||
         code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) && indev != NULL) {
        lv_indev_get_point(indev, &point);
    }

    if (code == LV_EVENT_PRESSED) {
        if (ctx->face_theme.animating) {
            lv_anim_delete(ctx->face_theme.panel, face_theme_sheet_anim_cb);
            ctx->face_theme.animating = false;
        }
        ctx->face_theme.dragging = true;
        ctx->face_theme.drag_from_face = false;
        ctx->face_theme.drag_start_point = point;
        ctx->face_theme.drag_start_y = face_theme_current_y(ctx);
        if (target == ctx->face_theme.drag_handle) {
            face_theme_set_snapshot_visible(ctx, true);
        }
        return;
    }

    if (code == LV_EVENT_PRESS_LOST) {
        if (ctx->face_theme.dragging) {
            face_theme_animate_to(ctx, face_theme_current_y(ctx) <= close_threshold_y
                                           ? FACE_THEME_SHEET_CLOSED_Y
                                           : FACE_THEME_SHEET_OPEN_Y);
        }
        ctx->face_theme.dragging = false;
        ctx->face_theme.drag_from_face = false;
        return;
    }

    if (code == LV_EVENT_PRESSING && ctx->face_theme.dragging) {
        dy = point.y - ctx->face_theme.drag_start_point.y;
        target_y = ctx->face_theme.drag_start_y + dy;
        face_theme_update_visual_state(ctx, target_y);
        return;
    }

    if (code == LV_EVENT_RELEASED && ctx->face_theme.dragging) {
        ctx->face_theme.dragging = false;
        ctx->face_theme.drag_from_face = false;
        target_y = face_theme_current_y(ctx);
        if (target_y <= close_threshold_y) {
            face_theme_animate_to(ctx, FACE_THEME_SHEET_CLOSED_Y);
        } else {
            face_theme_animate_to(ctx, FACE_THEME_SHEET_OPEN_Y);
        }
        return;
    }

    if (code == LV_EVENT_CLICKED &&
        lv_event_get_current_target(event) == ctx->face_theme.overlay &&
        lv_event_get_target(event) == ctx->face_theme.overlay) {
        face_theme_overlay_close(ctx);
    }
}

static void face_theme_close_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);

    if (ctx == NULL) {
        return;
    }

    face_theme_overlay_close(ctx);
}

static void face_theme_button_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);

    if (ctx == NULL || ctx->face_theme.overlay == NULL || ctx->tileview == NULL) {
        return;
    }

    ctx->face_theme.picker_face = tile_to_face(ctx, lv_tileview_get_tile_active(ctx->tileview));
    sync_face_theme_overlay(ctx);
    face_theme_update_visual_state(ctx, FACE_THEME_SHEET_CLOSED_Y);
    face_theme_animate_to(ctx, FACE_THEME_SHEET_OPEN_Y);
}

static void open_face_theme_overlay_for_active_face(clock_ui_context_t *ctx)
{
    if (ctx == NULL || ctx->face_theme.overlay == NULL || ctx->tileview == NULL) {
        return;
    }

    ctx->face_theme.picker_face = tile_to_face(ctx, lv_tileview_get_tile_active(ctx->tileview));
    sync_face_theme_overlay(ctx);
    face_theme_update_visual_state(ctx, FACE_THEME_SHEET_CLOSED_Y);
    face_theme_animate_to(ctx, FACE_THEME_SHEET_OPEN_Y);
}

static void shell_settings_button_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);

    if (ctx == NULL) {
        return;
    }

    face_theme_overlay_close(ctx);
    settings_button_event_cb(event);
}

void update_dots(clock_ui_context_t *ctx, clock_face_id_t active_face)
{
    for (int i = 0; i < CLOCK_FACE_COUNT; ++i) {
        if (ctx->page_dots[i] == NULL) {
            continue;
        }

        lv_obj_set_style_bg_color(ctx->page_dots[i],
                                  (i == active_face) ? lv_color_white() : lv_color_hex(0x555555),
                                  0);
    }
}

clock_face_id_t tile_to_face(clock_ui_context_t *ctx, lv_obj_t *tile)
{
    for (int i = 0; i < CLOCK_FACE_COUNT; ++i) {
        if (!clock_face_is_enabled((clock_face_id_t)i)) {
            continue;
        }
        if (ctx->tiles[i] == tile) {
            return (clock_face_id_t)i;
        }
    }

    return clock_face_first_enabled();
}

static void apply_face_navigation_mode(clock_ui_context_t *ctx, clock_face_id_t face)
{
    LV_UNUSED(face);
    lv_obj_set_scroll_dir(ctx->tileview, LV_DIR_NONE);
}

static void face_swipe_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    lv_event_code_t code = lv_event_get_code(event);
    lv_indev_t *indev = lv_event_get_indev(event);
    lv_point_t point;
    lv_coord_t dx;
    lv_coord_t dy;
    clock_face_id_t current_face;
    clock_face_id_t target_face;

    if (settings_surface_is_open(ctx) || alarm_surface_is_open(ctx) || brightness_panel_is_open(ctx) ||
        (face_theme_overlay_is_open(ctx) && !ctx->face_theme.drag_from_face)) {
        ctx->faces.face_swipe_tracking = false;
        return;
    }

    if (indev == NULL) {
        ctx->faces.face_swipe_tracking = false;
        return;
    }

    if (code == LV_EVENT_PRESSED) {
        lv_indev_get_point(indev, &ctx->faces.face_swipe_start_point);
        ctx->faces.face_swipe_tracking = true;
        return;
    }

    if (code == LV_EVENT_PRESSING && ctx->faces.face_swipe_tracking) {
        int32_t target_y;

        lv_indev_get_point(indev, &point);
        dx = point.x - ctx->faces.face_swipe_start_point.x;
        dy = point.y - ctx->faces.face_swipe_start_point.y;

        if (!ctx->face_theme.drag_from_face) {
            if (dy < 18 || dy < LV_ABS(dx) + 12) {
                return;
            }

            ctx->face_theme.picker_face = tile_to_face(ctx, lv_tileview_get_tile_active(ctx->tileview));
            sync_face_theme_overlay(ctx);
            if (ctx->face_theme.animating) {
                lv_anim_delete(ctx->face_theme.panel, face_theme_sheet_anim_cb);
                ctx->face_theme.animating = false;
            }
            ctx->face_theme.dragging = true;
            ctx->face_theme.drag_from_face = true;
            ctx->face_theme.drag_start_point = ctx->faces.face_swipe_start_point;
            ctx->face_theme.drag_start_y = FACE_THEME_SHEET_CLOSED_Y;
            face_theme_prepare_overlay(ctx);
            face_theme_set_snapshot_visible(ctx, true);
            face_theme_update_visual_state(ctx, FACE_THEME_SHEET_CLOSED_Y);
        }

        target_y = ctx->face_theme.drag_start_y + dy;
        face_theme_update_visual_state(ctx, target_y);
        return;
    }

    if (code == LV_EVENT_PRESS_LOST) {
        ctx->faces.face_swipe_tracking = false;
        if (ctx->face_theme.drag_from_face) {
            face_theme_animate_to(ctx, FACE_THEME_SHEET_CLOSED_Y);
        }
        ctx->face_theme.dragging = false;
        ctx->face_theme.drag_from_face = false;
        return;
    }

    if (code != LV_EVENT_RELEASED || !ctx->faces.face_swipe_tracking) {
        return;
    }

    ctx->faces.face_swipe_tracking = false;
    lv_indev_get_point(indev, &point);
    dx = point.x - ctx->faces.face_swipe_start_point.x;
    dy = point.y - ctx->faces.face_swipe_start_point.y;

    if (ctx->face_theme.drag_from_face) {
        int32_t target_y = face_theme_current_y(ctx);
        int32_t open_threshold_y = FACE_THEME_SHEET_CLOSED_Y + ((FACE_THEME_SHEET_OPEN_Y - FACE_THEME_SHEET_CLOSED_Y) * 45) / 100;

        ctx->face_theme.dragging = false;
        ctx->face_theme.drag_from_face = false;
        if (target_y >= open_threshold_y) {
            face_theme_animate_to(ctx, FACE_THEME_SHEET_OPEN_Y);
        } else {
            face_theme_animate_to(ctx, FACE_THEME_SHEET_CLOSED_Y);
        }
        return;
    }

    if (dy >= 64 && dy >= LV_ABS(dx) + 20) {
        open_face_theme_overlay_for_active_face(ctx);
        return;
    }

    if (LV_ABS(dx) < 56 || LV_ABS(dx) <= LV_ABS(dy) + 20) {
        return;
    }

    current_face = ctx->runtime->in_night_mode ? ctx->settings->night_mode.face : ctx->settings->current_face;
    target_face = (dx < 0) ? clock_face_step_enabled(current_face, 1) : clock_face_step_enabled(current_face, -1);
    if (target_face == current_face) {
        return;
    }

    set_active_face(ctx, target_face, LV_ANIM_OFF);
    show_affordances_temporarily(ctx);

    if (ctx->runtime->in_night_mode) {
        if (ctx->settings->night_mode.face != target_face) {
            request_set_night_face(ctx, target_face);
        }
    } else if (ctx->settings->current_face != target_face) {
        request_set_current_face(ctx, target_face);
    }
}

static void create_face_swipe_layer(clock_ui_context_t *ctx)
{
    ctx->faces.face_swipe_layer = lv_obj_create(ctx->screen);
    lv_obj_set_size(ctx->faces.face_swipe_layer, SCREEN_SIZE, SCREEN_SIZE - BOTTOM_EDGE_ZONE - 8);
    lv_obj_align(ctx->faces.face_swipe_layer, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(ctx->faces.face_swipe_layer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ctx->faces.face_swipe_layer, 0, 0);
    lv_obj_set_style_radius(ctx->faces.face_swipe_layer, 0, 0);
    lv_obj_set_style_pad_all(ctx->faces.face_swipe_layer, 0, 0);
    lv_obj_clear_flag(ctx->faces.face_swipe_layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ctx->faces.face_swipe_layer, face_swipe_event_cb, LV_EVENT_PRESSED, ctx);
    lv_obj_add_event_cb(ctx->faces.face_swipe_layer, face_swipe_event_cb, LV_EVENT_PRESSING, ctx);
    lv_obj_add_event_cb(ctx->faces.face_swipe_layer, face_swipe_event_cb, LV_EVENT_RELEASED, ctx);
    lv_obj_add_event_cb(ctx->faces.face_swipe_layer, face_swipe_event_cb, LV_EVENT_PRESS_LOST, ctx);
}

void set_active_face(clock_ui_context_t *ctx, clock_face_id_t face, lv_anim_enable_t anim)
{
    int visible_index;

    if (!clock_face_is_valid(face)) {
        face = clock_face_first_enabled();
    }
    if (!clock_face_is_enabled(face)) {
        face = clock_face_first_enabled();
    }

    visible_index = clock_face_visible_id_to_index(face);
    if (visible_index < 0) {
        face = clock_face_first_enabled();
        visible_index = clock_face_visible_id_to_index(face);
    }

    face_theme_overlay_close(ctx);
    ctx->suppress_events = true;
    apply_face_navigation_mode(ctx, face);
    sync_face_animation_state(ctx, face);
    lv_tileview_set_tile_by_index(ctx->tileview, visible_index, 0, anim);
    update_dots(ctx, face);
    sync_alarm_banner_style(ctx, face);
    ctx->suppress_events = false;
}

static void tileview_value_changed_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    lv_obj_t *active_tile;
    clock_face_id_t face;

    LV_UNUSED(event);

    if (ctx->suppress_events) {
        return;
    }

    active_tile = lv_tileview_get_tile_active(ctx->tileview);
    face = tile_to_face(ctx, active_tile);
    apply_face_navigation_mode(ctx, face);
    sync_face_animation_state(ctx, face);
    update_dots(ctx, face);
    sync_alarm_banner_style(ctx, face);
    show_affordances_temporarily(ctx);

    if (ctx->runtime->in_night_mode) {
        if (ctx->settings->night_mode.face != face) {
            request_set_night_face(ctx, face);
        }
    } else if (ctx->settings->current_face != face) {
        request_set_current_face(ctx, face);
    }
}

static void tileview_scroll_event_cb(lv_event_t *event)
{
    clock_ui_context_t *ctx = (clock_ui_context_t *)lv_event_get_user_data(event);
    lv_event_code_t code = lv_event_get_code(event);
    clock_face_id_t face;

    LV_UNUSED(event);
    if (code == LV_EVENT_SCROLL_BEGIN) {
        ctx->faces.tileview_scrolling = true;
        face = tile_to_face(ctx, lv_tileview_get_tile_active(ctx->tileview));
        sync_face_animation_state(ctx, face);
        show_affordances_temporarily(ctx);
        return;
    }

    if (code == LV_EVENT_SCROLL_END) {
        ctx->faces.tileview_scrolling = false;
        face = tile_to_face(ctx, lv_tileview_get_tile_active(ctx->tileview));
        sync_face_animation_state(ctx, face);
        update_face(ctx, face);
        update_dots(ctx, face);
        show_affordances_temporarily(ctx);
    }
}

static void create_dots(clock_ui_context_t *ctx)
{
    int visible_count = clock_face_visible_count();
    int start_x = -((visible_count - 1) * 18) / 2;

    for (int index = 0; index < visible_count; ++index) {
        clock_face_id_t face = clock_face_visible_index_to_id(index);

        ctx->page_dots[face] = lv_obj_create(ctx->screen);
        lv_obj_set_size(ctx->page_dots[face], 10, 10);
        lv_obj_set_style_radius(ctx->page_dots[face], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(ctx->page_dots[face], 0, 0);
        lv_obj_set_style_opa(ctx->page_dots[face], LV_OPA_TRANSP, 0);
        lv_obj_set_scrollbar_mode(ctx->page_dots[face], LV_SCROLLBAR_MODE_OFF);
        lv_obj_remove_flag(ctx->page_dots[face], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(ctx->page_dots[face], LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(ctx->page_dots[face], LV_ALIGN_BOTTOM_MID, start_x + index * 18, -38);
    }

    update_dots(ctx, sanitize_enabled_face(ctx->settings->current_face));
}

static void create_settings_button(clock_ui_context_t *ctx)
{
    lv_obj_t *label;

    ctx->settings_button = lv_button_create(ctx->screen);
    lv_obj_set_size(ctx->settings_button, 56, 56);
    lv_obj_align(ctx->settings_button, LV_ALIGN_TOP_RIGHT, -28, 28);
    lv_obj_set_style_radius(ctx->settings_button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(ctx->settings_button, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_bg_opa(ctx->settings_button, LV_OPA_70, 0);
    lv_obj_set_style_border_width(ctx->settings_button, 0, 0);
    lv_obj_add_event_cb(ctx->settings_button, shell_settings_button_event_cb, LV_EVENT_CLICKED, ctx);

    label = lv_label_create(ctx->settings_button);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, LV_SYMBOL_SETTINGS);
    lv_obj_center(label);
}

void create_face_theme_button(clock_ui_context_t *ctx)
{
    lv_obj_t *label;

    ctx->face_theme.button = lv_button_create(ctx->screen);
    lv_obj_set_size(ctx->face_theme.button, 56, 56);
    lv_obj_align(ctx->face_theme.button, LV_ALIGN_LEFT_MID, 52, 72);
    lv_obj_set_style_radius(ctx->face_theme.button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(ctx->face_theme.button, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_bg_opa(ctx->face_theme.button, LV_OPA_70, 0);
    lv_obj_set_style_border_width(ctx->face_theme.button, 0, 0);
    lv_obj_add_event_cb(ctx->face_theme.button, face_theme_button_event_cb, LV_EVENT_CLICKED, ctx);

    label = lv_label_create(ctx->face_theme.button);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, LV_SYMBOL_SETTINGS);
    lv_obj_center(label);
    ctx->face_theme.button_visible = true;
}

void create_face_theme_overlay(clock_ui_context_t *ctx)
{
    ui_surface_t surface;

    ui_surface_create_fullscreen(&surface,
                                 ctx->screen,
                                 lv_color_black(),
                                 LV_OPA_70,
                                 lv_color_hex(0x0D0D0D),
                                 SETTINGS_HEADER_HEIGHT,
                                 "Face themes",
                                 face_theme_overlay_event_cb,
                                 face_theme_close_event_cb,
                                 ctx);
    ctx->face_theme.overlay = surface.overlay;
    ctx->face_theme.panel = surface.panel;
    ctx->face_theme.snapshot_img = lv_image_create(surface.overlay);
    ctx->face_theme.title = surface.title;
    ctx->face_theme.content = surface.content;
    lv_obj_set_user_data(surface.panel, ctx);
    lv_obj_add_flag(ctx->face_theme.snapshot_img, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
    lv_obj_clear_flag(ctx->face_theme.snapshot_img, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(surface.panel, lv_pct(100), FACE_THEME_SHEET_HEIGHT);
    lv_obj_align(surface.panel, LV_ALIGN_TOP_MID, 0, FACE_THEME_SHEET_CLOSED_Y);
    lv_obj_set_style_radius(surface.panel, 0, 0);
    lv_obj_set_style_bg_opa(surface.overlay, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_font(surface.title, &lv_font_montserrat_36, 0);
    lv_obj_set_width(surface.title, 440);
    lv_obj_align(surface.title, LV_ALIGN_TOP_MID, 0, 18);
    ctx->face_theme.grabber = lv_obj_create(surface.panel);
    lv_obj_set_size(ctx->face_theme.grabber, 72, 6);
    lv_obj_set_style_radius(ctx->face_theme.grabber, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ctx->face_theme.grabber, 0, 0);
    lv_obj_set_style_bg_color(ctx->face_theme.grabber, lv_color_hex(0x9C9C9C), 0);
    lv_obj_set_style_bg_opa(ctx->face_theme.grabber, LV_OPA_70, 0);
    lv_obj_set_style_shadow_width(ctx->face_theme.grabber, 0, 0);
    lv_obj_remove_flag(ctx->face_theme.grabber, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ctx->face_theme.grabber, LV_OBJ_FLAG_FLOATING | LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_align(ctx->face_theme.grabber, LV_ALIGN_TOP_MID, 0, 18);
    ctx->face_theme.drag_handle = lv_obj_create(surface.panel);
    lv_obj_set_size(ctx->face_theme.drag_handle, lv_pct(100), 72);
    lv_obj_align(ctx->face_theme.drag_handle, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(ctx->face_theme.drag_handle, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ctx->face_theme.drag_handle, 0, 0);
    lv_obj_set_style_radius(ctx->face_theme.drag_handle, 0, 0);
    lv_obj_set_style_pad_all(ctx->face_theme.drag_handle, 0, 0);
    lv_obj_set_scrollbar_mode(ctx->face_theme.drag_handle, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(ctx->face_theme.drag_handle, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ctx->face_theme.drag_handle, LV_OBJ_FLAG_FLOATING);
    lv_obj_add_event_cb(ctx->face_theme.drag_handle, face_theme_overlay_event_cb, LV_EVENT_PRESSED, ctx);
    lv_obj_add_event_cb(ctx->face_theme.drag_handle, face_theme_overlay_event_cb, LV_EVENT_PRESSING, ctx);
    lv_obj_add_event_cb(ctx->face_theme.drag_handle, face_theme_overlay_event_cb, LV_EVENT_RELEASED, ctx);
    lv_obj_add_event_cb(ctx->face_theme.drag_handle, face_theme_overlay_event_cb, LV_EVENT_PRESS_LOST, ctx);
    lv_obj_add_event_cb(surface.panel, face_theme_overlay_event_cb, LV_EVENT_PRESSED, ctx);
    lv_obj_add_event_cb(surface.panel, face_theme_overlay_event_cb, LV_EVENT_PRESSING, ctx);
    lv_obj_add_event_cb(surface.panel, face_theme_overlay_event_cb, LV_EVENT_RELEASED, ctx);
    lv_obj_add_event_cb(surface.panel, face_theme_overlay_event_cb, LV_EVENT_PRESS_LOST, ctx);
    lv_obj_add_event_cb(surface.content, face_theme_overlay_event_cb, LV_EVENT_PRESSED, ctx);
    lv_obj_add_event_cb(surface.content, face_theme_overlay_event_cb, LV_EVENT_PRESSING, ctx);
    lv_obj_add_event_cb(surface.content, face_theme_overlay_event_cb, LV_EVENT_RELEASED, ctx);
    lv_obj_add_event_cb(surface.content, face_theme_overlay_event_cb, LV_EVENT_PRESS_LOST, ctx);
    lv_obj_add_flag(surface.content, LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_set_style_pad_top(surface.content, 12, 0);
    lv_obj_set_style_pad_bottom(surface.content, 24, 0);
    lv_obj_set_style_pad_row(surface.content, 18, 0);
    lv_obj_set_flex_align(surface.content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(surface.content, LV_OBJ_FLAG_SCROLLABLE);

    for (uint8_t theme = 0; theme < CLOCK_FACE_THEME_COUNT; ++theme) {
        lv_obj_t *card = create_card(surface.content);
        lv_obj_t *row;

        ctx->face_theme.option_card[theme] = card;
        lv_obj_set_width(card, 540);
        lv_obj_set_style_pad_all(card, 24, 0);
        lv_obj_set_style_pad_row(card, 16, 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0x2C2C2C), 0);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x171717), 0);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x22272B), LV_STATE_PRESSED);
        lv_obj_add_flag(card, LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_add_event_cb(card, face_theme_option_event_cb, LV_EVENT_CLICKED, &ctx->face_theme.option_ctx[theme]);

        ctx->face_theme.option_title[theme] = lv_label_create(card);
        lv_obj_set_style_text_font(ctx->face_theme.option_title[theme], &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(ctx->face_theme.option_title[theme], lv_color_white(), 0);
        lv_label_set_text(ctx->face_theme.option_title[theme], "");
        lv_obj_add_flag(ctx->face_theme.option_title[theme], LV_OBJ_FLAG_EVENT_BUBBLE);

        row = create_row(card);
        lv_obj_set_style_pad_column(row, 14, 0);
        center_row(row);
        lv_obj_add_flag(row, LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
        for (int swatch = 0; swatch < 3; ++swatch) {
            ctx->face_theme.option_swatches[theme][swatch] = lv_obj_create(row);
            lv_obj_set_size(ctx->face_theme.option_swatches[theme][swatch], 34, 34);
            lv_obj_set_style_radius(ctx->face_theme.option_swatches[theme][swatch], LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_border_width(ctx->face_theme.option_swatches[theme][swatch], 2, 0);
            lv_obj_set_style_border_color(ctx->face_theme.option_swatches[theme][swatch], lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_border_opa(ctx->face_theme.option_swatches[theme][swatch], LV_OPA_20, 0);
            lv_obj_set_style_pad_all(ctx->face_theme.option_swatches[theme][swatch], 0, 0);
            lv_obj_clear_flag(ctx->face_theme.option_swatches[theme][swatch], LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(ctx->face_theme.option_swatches[theme][swatch],
                            LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
        }
    }

    ctx->face_theme.snapshot_dirty = true;
    face_theme_overlay_close(ctx);
}

void build_root_ui(clock_ui_context_t *ctx)
{
    ctx->screen = lv_screen_active();
    lv_obj_set_style_bg_color(ctx->screen, lv_color_black(), 0);
    lv_obj_set_style_pad_all(ctx->screen, 0, 0);
    lv_obj_remove_flag(ctx->screen, LV_OBJ_FLAG_SCROLLABLE);

    ctx->tileview = lv_tileview_create(ctx->screen);
    lv_obj_set_size(ctx->tileview, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_color(ctx->tileview, lv_color_black(), 0);
    lv_obj_set_style_pad_all(ctx->tileview, 0, 0);
    lv_obj_set_style_border_width(ctx->tileview, 0, 0);
    lv_obj_set_scrollbar_mode(ctx->tileview, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(ctx->tileview, LV_OBJ_FLAG_SCROLL_ONE);
    lv_obj_remove_flag(ctx->tileview, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_set_style_anim_duration(ctx->tileview, 0, 0);
    lv_obj_set_scroll_snap_x(ctx->tileview, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_scroll_snap_y(ctx->tileview, LV_SCROLL_SNAP_NONE);
    lv_obj_align(ctx->tileview, LV_ALIGN_CENTER, 0, 0);

    for (int index = 0; index < clock_face_visible_count(); ++index) {
        clock_face_id_t face = clock_face_visible_index_to_id(index);

        ctx->tiles[face] = lv_tileview_add_tile(ctx->tileview, index, 0, LV_DIR_HOR);
        lv_obj_set_style_pad_all(ctx->tiles[face], 0, 0);
        lv_obj_set_style_border_width(ctx->tiles[face], 0, 0);
    }

    create_digital_face(ctx, ctx->tiles[CLOCK_FACE_DIGITAL]);
    create_matrix_face(ctx, ctx->tiles[CLOCK_FACE_MATRIX]);
    create_wharton_face(ctx, ctx->tiles[CLOCK_FACE_WHARTON]);
    create_sternglas_face(ctx, ctx->tiles[CLOCK_FACE_STERNGLAS]);
    create_avenir_face(ctx, ctx->tiles[CLOCK_FACE_AVENIR]);
    create_modern_silver_face(ctx, ctx->tiles[CLOCK_FACE_MODERN_SILVER]);

    lv_obj_add_event_cb(ctx->tileview, tileview_value_changed_cb, LV_EVENT_VALUE_CHANGED, ctx);
    lv_obj_add_event_cb(ctx->tileview, tileview_scroll_event_cb, LV_EVENT_SCROLL_BEGIN, ctx);
    lv_obj_add_event_cb(ctx->tileview, tileview_scroll_event_cb, LV_EVENT_SCROLL, ctx);
    lv_obj_add_event_cb(ctx->tileview, tileview_scroll_event_cb, LV_EVENT_SCROLL_END, ctx);

    create_face_swipe_layer(ctx);
    create_dots(ctx);
    create_brightness_pull_hint(ctx);
    create_brightness_edge_sensor(ctx);
    create_brightness_overlay(ctx);
    create_brightness_panel(ctx);
    create_settings_button(ctx);
    create_alarm_banner(ctx);
    create_alarm_management_overlay(ctx);
    create_alarm_settings_overlay(ctx);
    create_alarm_editor_overlay(ctx);
    create_alarm_overlay(ctx);
    create_settings_overlay(ctx);
    create_face_theme_overlay(ctx);
    lv_obj_move_foreground(ctx->brightness.pull_hint);
    lv_obj_move_foreground(ctx->brightness.edge_sensor);
    set_active_face(ctx, ctx->settings->current_face, LV_ANIM_OFF);
}
