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
extern const char *s_day_short[7];
extern const char *s_month_short[12];

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
void update_face(clock_ui_context_t *ctx, clock_face_id_t face);
void sync_face_animation_state(clock_ui_context_t *ctx, clock_face_id_t face);
bool brightness_panel_is_open(const clock_ui_context_t *ctx);
void update_brightness_ui(clock_ui_context_t *ctx);
void brightness_overlay_hide_immediately(clock_ui_context_t *ctx);
void brightness_panel_hide(clock_ui_context_t *ctx);
bool settings_surface_is_open(const clock_ui_context_t *ctx);
void settings_surface_close(clock_ui_context_t *ctx);
void open_settings_tab(clock_ui_context_t *ctx, uint32_t tab_idx);
void sync_alarm_controls(clock_ui_context_t *ctx);
void sync_alarm_banner_style(clock_ui_context_t *ctx, clock_face_id_t face);
void alarm_management_open(clock_ui_context_t *ctx);
void alarm_management_close(clock_ui_context_t *ctx);
void alarm_editor_close(clock_ui_context_t *ctx);
void open_alarm_editor(clock_ui_context_t *ctx, uint8_t alarm_index, bool is_new);
void refresh_settings_controls(clock_ui_context_t *ctx);
void sync_wifi_controls(clock_ui_context_t *ctx);
void sync_other_controls(clock_ui_context_t *ctx);
bool wifi_controls_need_sync(const clock_ui_context_t *ctx);
void sync_night_controls(clock_ui_context_t *ctx);
bool night_controls_need_sync(const clock_ui_context_t *ctx);
void update_alarm_banner(clock_ui_context_t *ctx, time_t now);
void sync_alarm_overlay(clock_ui_context_t *ctx, time_t now);
bool alarm_controls_need_sync(const clock_ui_context_t *ctx);
void build_root_ui(clock_ui_context_t *ctx);
void update_dots(clock_ui_context_t *ctx, clock_face_id_t active_face);
void settings_button_event_cb(lv_event_t *event);
bool face_theme_overlay_is_open(const clock_ui_context_t *ctx);
void hide_face_theme_button(clock_ui_context_t *ctx);
void face_theme_overlay_close(clock_ui_context_t *ctx);
void create_face_theme_button(clock_ui_context_t *ctx);
void create_update_button(clock_ui_context_t *ctx);
void sync_update_button(clock_ui_context_t *ctx);
void create_face_theme_overlay(clock_ui_context_t *ctx);
void create_brightness_pull_hint(clock_ui_context_t *ctx);
void create_brightness_edge_sensor(clock_ui_context_t *ctx);
void create_brightness_overlay(clock_ui_context_t *ctx);
void create_brightness_panel(clock_ui_context_t *ctx);
void create_alarm_banner(clock_ui_context_t *ctx);
void create_alarm_overlay(clock_ui_context_t *ctx);
void create_alarm_management_overlay(clock_ui_context_t *ctx);
void create_alarm_settings_overlay(clock_ui_context_t *ctx);
void create_alarm_editor_overlay(clock_ui_context_t *ctx);
void create_settings_overlay(clock_ui_context_t *ctx);
void create_digital_face(clock_ui_context_t *ctx, lv_obj_t *parent);
void create_matrix_face(clock_ui_context_t *ctx, lv_obj_t *parent);
void create_wharton_face(clock_ui_context_t *ctx, lv_obj_t *parent);
void create_sternglas_face(clock_ui_context_t *ctx, lv_obj_t *parent);
void create_avenir_face(clock_ui_context_t *ctx, lv_obj_t *parent);
void create_modern_silver_face(clock_ui_context_t *ctx, lv_obj_t *parent);
void ui_play_click_feedback(void);

clock_face_id_t sanitize_enabled_face(clock_face_id_t face);
struct tm get_local_time_now(void);
void hand_endpoint(int cx, int cy, int length, float angle_deg, lv_point_precise_t *p0, lv_point_precise_t *p1);
void hand_line_endpoints(int cx, int cy, int tail_length, int head_length,
                         float angle_deg, lv_point_precise_t *p0, lv_point_precise_t *p1);
uint8_t brightness_ui_to_hw(int ui_percent);
uint8_t brightness_hw_to_ui(int hw_percent);
bool alarm_surface_is_open(const clock_ui_context_t *ctx);
void format_alarm_time(char *buffer, size_t size, uint8_t hour, uint8_t minute);
void format_alarm_repeat_summary(char *buffer, size_t size, const alarm_config_t *alarm);
int count_enabled_alarms(const clock_ui_context_t *ctx);
int find_alarm_slot_for_new_alarm(const clock_ui_context_t *ctx);
uint8_t alarm_repeat_preset_from_config(const alarm_config_t *alarm);
void apply_repeat_preset_to_alarm(alarm_config_t *alarm, uint8_t preset);
void set_root_ui_hidden(clock_ui_context_t *ctx, bool hidden);
