#pragma once

#include "ui/clock_ui_internal.h"

extern lv_style_t s_style_hour;
extern lv_style_t s_style_min;
extern lv_style_t s_style_sec;
extern const uint8_t s_matrix_font[10][MTX_DIGIT_H];
extern const uint8_t s_wharton_font[10][WH_DIGIT_ROWS];
extern const uint8_t s_matrix_digit_col[4];
extern const uint8_t s_matrix_colon_col;
extern const uint8_t s_matrix_digit_row0;

enum {
    ALARM_REPEAT_PRESET_ONCE = 0,
    ALARM_REPEAT_PRESET_EVERY_DAY = 1,
    ALARM_REPEAT_PRESET_WEEKDAYS = 2,
    ALARM_REPEAT_PRESET_WEEKENDS = 3,
};

void request_set_base_brightness(clock_ui_context_t *ctx, uint8_t hw_percent);
void request_set_runtime_night_brightness(clock_ui_context_t *ctx, uint8_t hw_percent);
void request_set_temporary_brightness_floor(clock_ui_context_t *ctx, bool enabled, uint8_t hw_percent);
void request_set_current_face(clock_ui_context_t *ctx, clock_face_id_t face);
void request_set_face_theme(clock_ui_context_t *ctx, clock_face_id_t face, uint8_t theme);
void request_set_night_face(clock_ui_context_t *ctx, clock_face_id_t face);
void request_set_timezone(clock_ui_context_t *ctx, uint8_t timezone_id);
void request_set_time_sync_mode(clock_ui_context_t *ctx, time_sync_mode_t mode);
void request_set_manual_time(clock_ui_context_t *ctx,
                             uint16_t year,
                             uint8_t month,
                             uint8_t day,
                             uint8_t hour,
                             uint8_t minute);
void request_save_wifi_credentials(clock_ui_context_t *ctx, const char *ssid, const char *password);
void request_ota_check(clock_ui_context_t *ctx);
void request_ota_install(clock_ui_context_t *ctx);
void request_ui_click_feedback(clock_ui_context_t *ctx);
void request_set_ui_click_sound_enabled(clock_ui_context_t *ctx, bool enabled);
void request_set_ui_click_volume(clock_ui_context_t *ctx, uint8_t volume);
void request_set_language(clock_ui_context_t *ctx, clock_language_t language);
void request_set_ui_style(clock_ui_context_t *ctx, clock_ui_style_t style);
void request_set_default_alarm_sound(clock_ui_context_t *ctx, uint8_t sound);
void request_set_night_mode_enabled(clock_ui_context_t *ctx, bool enabled);
void request_set_night_schedule(clock_ui_context_t *ctx,
                                uint8_t start_hour,
                                uint8_t start_minute,
                                uint8_t end_hour,
                                uint8_t end_minute);
void request_set_night_brightness(clock_ui_context_t *ctx, uint8_t hw_percent);
void request_set_night_sunrise_brightness_enabled(clock_ui_context_t *ctx, bool enabled);
void request_set_alarm_volume(clock_ui_context_t *ctx, uint8_t volume);
void request_set_ascending_alarm_enabled(clock_ui_context_t *ctx, bool enabled);
void request_set_snooze_minutes(clock_ui_context_t *ctx, uint8_t minutes);
void request_set_alarm_enabled(clock_ui_context_t *ctx, uint8_t alarm_index, bool enabled);
void request_save_alarm(clock_ui_context_t *ctx, uint8_t alarm_index, const alarm_config_t *alarm);
void request_delete_alarm(clock_ui_context_t *ctx, uint8_t alarm_index);

void affordance_hide_timer_cb(lv_timer_t *timer);
clock_face_id_t tile_to_face(clock_ui_context_t *ctx, lv_obj_t *tile);
void set_active_face(clock_ui_context_t *ctx, clock_face_id_t face, lv_anim_enable_t anim);
void show_affordances_temporarily(clock_ui_context_t *ctx);
void refresh_digital_face_snapshot(clock_ui_context_t *ctx);
const void *ui_face_preview_source(clock_ui_context_t *ctx, clock_face_id_t face);


ui_face_ink_t ui_face_ink(const clock_ui_context_t *ctx, clock_face_id_t face);
void update_face(clock_ui_context_t *ctx, clock_face_id_t face);
void sync_face_animation_state(clock_ui_context_t *ctx, clock_face_id_t face);
bool brightness_panel_is_open(const clock_ui_context_t *ctx);
void update_brightness_ui(clock_ui_context_t *ctx);
void brightness_overlay_hide_immediately(clock_ui_context_t *ctx);
void brightness_panel_hide(clock_ui_context_t *ctx);
bool settings_surface_is_open(const clock_ui_context_t *ctx);
void settings_surface_close(clock_ui_context_t *ctx);
void open_settings_tab(clock_ui_context_t *ctx, uint32_t tab_idx);
#if CONFIG_TOUCH_CLOCK_PERF_TOUR
void ui_perf_tour_start(clock_ui_context_t *ctx);
void ui_ppa_blend_benchmark(void);
/* Running totals of PPA operations and the microseconds spent in them. */
void ui_ppa_blend_stats(uint32_t *ops, uint32_t *us);
#endif
void refresh_settings_controls(clock_ui_context_t *ctx);
void build_root_ui(clock_ui_context_t *ctx);
clock_ui_context_t *clock_ui_get_context(void);
void ui_install_theme_overrides(lv_display_t *disp);
void ui_ppa_blend_install(void);
/* Registers a mostly transparent ARGB8888 image so blends of it touch only its visible
 * pixels; unregister before freeing it. */
void ui_sparse_image_register(const lv_draw_buf_t *buf);
void ui_sparse_image_unregister(const lv_draw_buf_t *buf);
lv_obj_t *ui_backdrop_create(lv_obj_t *overlay);
void ui_backdrop_set(clock_ui_context_t *ctx, lv_obj_t *overlay, lv_obj_t *backdrop, bool moving, lv_opa_t dim_opa);
void ui_l10n_reset(clock_ui_context_t *ctx);
const char *ui_l10n_text(const clock_ui_context_t *ctx, l10n_text_id_t text_id);
void ui_l10n_bind_label(clock_ui_context_t *ctx, lv_obj_t *label, l10n_text_id_t text_id);
void ui_l10n_bind_label_with_font(clock_ui_context_t *ctx,
                                  lv_obj_t *label,
                                  l10n_text_id_t text_id,
                                  const lv_font_t *font);
void ui_l10n_bind_button_label(clock_ui_context_t *ctx, lv_obj_t *button, l10n_text_id_t text_id);
void ui_l10n_apply(clock_ui_context_t *ctx);
const char *ui_l10n_str(const clock_ui_context_t *ctx, const ui_l10n_str_t *str);
/* Bind a kit label (kit_label) to a text, keeping the label's font and capitalisation. */
void ui_l10n_bind_kit(clock_ui_context_t *ctx, lv_obj_t *label, l10n_text_id_t text_id);
void ui_l10n_bind_kit_str(clock_ui_context_t *ctx, lv_obj_t *label, const ui_l10n_str_t *str);
void update_dots(clock_ui_context_t *ctx, clock_face_id_t active_face);
void settings_button_event_cb(lv_event_t *event);
bool face_theme_overlay_is_open(const clock_ui_context_t *ctx);
void hide_face_theme_button(clock_ui_context_t *ctx);
void face_theme_overlay_close(clock_ui_context_t *ctx);
void create_digital_face(clock_ui_context_t *ctx, lv_obj_t *parent);
void create_matrix_face(clock_ui_context_t *ctx, lv_obj_t *parent);
void create_wharton_face(clock_ui_context_t *ctx, lv_obj_t *parent);
void create_sternglas_face(clock_ui_context_t *ctx, lv_obj_t *parent);
void create_avenir_face(clock_ui_context_t *ctx, lv_obj_t *parent);
void create_modern_silver_face(clock_ui_context_t *ctx, lv_obj_t *parent);
/* The evening faces (ui_faces_evening.c): dim, low-glare. */
void create_ember_face(clock_ui_context_t *ctx, lv_obj_t *parent);
void create_nocturne_face(clock_ui_context_t *ctx, lv_obj_t *parent);
void create_filament_face(clock_ui_context_t *ctx, lv_obj_t *parent);
void update_evening_face(clock_ui_context_t *ctx, clock_face_id_t face);
const void *evening_face_preview_source(clock_ui_context_t *ctx, clock_face_id_t face);
bool evening_face_ink(const clock_ui_context_t *ctx, clock_face_id_t face, ui_face_ink_t *ink);
bool evening_face_preview_colors(clock_face_id_t face, uint8_t theme, uint32_t colors[3]);
/* A face shown as one RGB565 canvas over a static layer, repainted only where it changes
 * (ui_faces.c). Coordinates of `box` are the canvas's own, `offset` from the screen's. */
void face_hands_create(ui_face_hands_t *hands, lv_obj_t *parent);
void face_hands_box_init(lv_area_t *box);
void face_hands_box_points(lv_area_t *box, const lv_point_precise_t *points, size_t count);
void face_hands_box_area(lv_area_t *box, const lv_area_t *area);
bool face_hands_begin(ui_face_hands_t *hands, const lv_draw_buf_t *dial, lv_area_t box, int32_t offset,
                      int32_t shadow_dx, int32_t shadow_dy, lv_layer_t *layer, lv_area_t *dirty);
void face_hands_finish(ui_face_hands_t *hands, lv_obj_t *dial_img, lv_layer_t *layer, lv_area_t dirty,
                       int32_t offset);
void ui_play_click_feedback(void);

clock_face_id_t sanitize_enabled_face(clock_face_id_t face);
struct tm get_local_time_now(void);
void hand_endpoint(int cx, int cy, int length, float angle_deg, lv_point_precise_t *p0, lv_point_precise_t *p1);
void hand_line_endpoints(int cx, int cy, int tail_length, int head_length,
                         float angle_deg, lv_point_precise_t *p0, lv_point_precise_t *p1);
uint8_t brightness_ui_to_hw(int ui_percent);
uint8_t brightness_hw_to_ui(int hw_percent);
void format_alarm_time(char *buffer, size_t size, uint8_t hour, uint8_t minute);
int count_enabled_alarms(const clock_ui_context_t *ctx);
uint8_t alarm_repeat_preset_from_config(const alarm_config_t *alarm);
void apply_repeat_preset_to_alarm(alarm_config_t *alarm, uint8_t preset);
void set_root_ui_hidden(clock_ui_context_t *ctx, bool hidden);
