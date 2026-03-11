static void update_dots(clock_face_id_t active_face)
{
    for (int i = 0; i < CLOCK_FACE_COUNT; ++i) {
        lv_obj_set_style_bg_color(s_ui.page_dots[i],
                                  (i == active_face) ? lv_color_white() : lv_color_hex(0x555555),
                                  0);
    }
}

static clock_face_id_t tile_to_face(lv_obj_t *tile)
{
    for (int i = 0; i < CLOCK_FACE_COUNT; ++i) {
        if (s_ui.tiles[i] == tile) {
            return (clock_face_id_t)i;
        }
    }

    return CLOCK_FACE_DIGITAL;
}

static void apply_face_navigation_mode(clock_face_id_t face)
{
    if (face == CLOCK_FACE_DIGITAL) {
        lv_obj_set_scroll_dir(s_ui.tileview, LV_DIR_NONE);
    } else {
        lv_obj_set_scroll_dir(s_ui.tileview, LV_DIR_HOR);
    }
}

static void set_active_face(clock_face_id_t face, lv_anim_enable_t anim)
{
    if (!clock_face_is_valid(face)) {
        face = CLOCK_FACE_DIGITAL;
    }

    s_ui.suppress_events = true;
    apply_face_navigation_mode(face);
    sync_face_animation_state(face);
    lv_tileview_set_tile_by_index(s_ui.tileview, face, 0, anim);
    update_dots(face);
    sync_alarm_banner_style(face);
    s_ui.suppress_events = false;
}

static void tileview_value_changed_cb(lv_event_t *event)
{
    lv_obj_t *active_tile;
    clock_face_id_t face;

    LV_UNUSED(event);

    if (s_ui.suppress_events) {
        return;
    }

    active_tile = lv_tileview_get_tile_active(s_ui.tileview);
    face = tile_to_face(active_tile);
    apply_face_navigation_mode(face);
    sync_face_animation_state(face);
    update_dots(face);
    sync_alarm_banner_style(face);
    show_affordances_temporarily();

    if (s_ui.runtime->in_night_mode) {
        if (s_ui.settings->night_mode.face != face) {
            s_ui.settings->night_mode.face = face;
            notify_settings_changed();
        }
    } else if (s_ui.settings->current_face != face) {
        s_ui.settings->current_face = face;
        notify_settings_changed();
    }
}

static void tileview_scroll_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    clock_face_id_t face;

    LV_UNUSED(event);
    if (code == LV_EVENT_SCROLL_BEGIN) {
        s_ui.faces.tileview_scrolling = true;
        show_affordances_temporarily();
        return;
    }

    if (code == LV_EVENT_SCROLL_END) {
        s_ui.faces.tileview_scrolling = false;
        face = tile_to_face(lv_tileview_get_tile_active(s_ui.tileview));
        update_face(face);
        update_dots(face);
        show_affordances_temporarily();
    }
}

static void create_dots(void)
{
    int start_x = -((CLOCK_FACE_COUNT - 1) * 18) / 2;

    for (int i = 0; i < CLOCK_FACE_COUNT; ++i) {
        s_ui.page_dots[i] = lv_obj_create(s_ui.screen);
        lv_obj_set_size(s_ui.page_dots[i], 10, 10);
        lv_obj_set_style_radius(s_ui.page_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(s_ui.page_dots[i], 0, 0);
        lv_obj_set_style_opa(s_ui.page_dots[i], LV_OPA_TRANSP, 0);
        lv_obj_set_scrollbar_mode(s_ui.page_dots[i], LV_SCROLLBAR_MODE_OFF);
        lv_obj_remove_flag(s_ui.page_dots[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(s_ui.page_dots[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(s_ui.page_dots[i], LV_ALIGN_BOTTOM_MID, start_x + i * 18, -38);
    }

    update_dots(s_ui.settings->current_face);
}

static void create_settings_button(void)
{
    lv_obj_t *label;

    s_ui.settings_button = lv_button_create(s_ui.screen);
    lv_obj_set_size(s_ui.settings_button, 56, 56);
    lv_obj_align(s_ui.settings_button, LV_ALIGN_TOP_RIGHT, -28, 28);
    lv_obj_set_style_radius(s_ui.settings_button, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_ui.settings_button, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_bg_opa(s_ui.settings_button, LV_OPA_70, 0);
    lv_obj_set_style_border_width(s_ui.settings_button, 0, 0);
    lv_obj_add_event_cb(s_ui.settings_button, settings_button_event_cb, LV_EVENT_CLICKED, NULL);

    label = lv_label_create(s_ui.settings_button);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, LV_SYMBOL_SETTINGS);
    lv_obj_center(label);
}

static void build_root_ui(void)
{
    s_ui.screen = lv_screen_active();
    lv_obj_set_style_bg_color(s_ui.screen, lv_color_black(), 0);
    lv_obj_set_style_pad_all(s_ui.screen, 0, 0);
    lv_obj_remove_flag(s_ui.screen, LV_OBJ_FLAG_SCROLLABLE);

    s_ui.tileview = lv_tileview_create(s_ui.screen);
    lv_obj_set_size(s_ui.tileview, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_color(s_ui.tileview, lv_color_black(), 0);
    lv_obj_set_style_pad_all(s_ui.tileview, 0, 0);
    lv_obj_set_style_border_width(s_ui.tileview, 0, 0);
    lv_obj_set_scrollbar_mode(s_ui.tileview, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(s_ui.tileview, LV_OBJ_FLAG_SCROLL_ONE);
    lv_obj_remove_flag(s_ui.tileview, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_set_scroll_snap_x(s_ui.tileview, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_scroll_snap_y(s_ui.tileview, LV_SCROLL_SNAP_NONE);
    lv_obj_align(s_ui.tileview, LV_ALIGN_CENTER, 0, 0);

    for (int face = 0; face < CLOCK_FACE_COUNT; ++face) {
        s_ui.tiles[face] = lv_tileview_add_tile(s_ui.tileview, face, 0, LV_DIR_HOR);
        lv_obj_set_style_pad_all(s_ui.tiles[face], 0, 0);
        lv_obj_set_style_border_width(s_ui.tiles[face], 0, 0);
    }

    create_digital_face(s_ui.tiles[CLOCK_FACE_DIGITAL]);
    create_matrix_face(s_ui.tiles[CLOCK_FACE_MATRIX]);
    create_wharton_face(s_ui.tiles[CLOCK_FACE_WHARTON]);
    create_slava_face(s_ui.tiles[CLOCK_FACE_SLAVA]);
    create_slava_dark_face(s_ui.tiles[CLOCK_FACE_SLAVA_DARK]);
    create_sternglas_face(s_ui.tiles[CLOCK_FACE_STERNGLAS]);
    create_avenir_face(s_ui.tiles[CLOCK_FACE_AVENIR]);
    create_modern_silver_face(s_ui.tiles[CLOCK_FACE_MODERN_SILVER]);

    lv_obj_add_event_cb(s_ui.tileview, tileview_value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_ui.tileview, tileview_scroll_event_cb, LV_EVENT_SCROLL_BEGIN, NULL);
    lv_obj_add_event_cb(s_ui.tileview, tileview_scroll_event_cb, LV_EVENT_SCROLL, NULL);
    lv_obj_add_event_cb(s_ui.tileview, tileview_scroll_event_cb, LV_EVENT_SCROLL_END, NULL);

    create_dots();
    create_brightness_pull_hint();
    create_brightness_edge_sensor();
    create_brightness_overlay();
    create_brightness_panel();
    create_settings_button();
    create_alarm_banner();
    create_alarm_management_overlay();
    create_alarm_settings_overlay();
    create_alarm_editor_overlay();
    create_alarm_overlay();
    create_settings_overlay();
    lv_obj_move_foreground(s_ui.brightness.pull_hint);
    lv_obj_move_foreground(s_ui.brightness.edge_sensor);
    set_active_face(s_ui.settings->current_face, LV_ANIM_OFF);
}
