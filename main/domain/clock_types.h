#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define MAX_ALARMS 5
#define DISPLAY_BRIGHTNESS_MIN_PERCENT 19
#define DISPLAY_BRIGHTNESS_MAX_PERCENT 100
#define CLOCK_WIFI_SCAN_RESULT_MAX 16
#define CLOCK_FACE_THEME_COUNT 3
#define CLOCK_FACE_ENABLE_SLAVA 0
#define CLOCK_FACE_ENABLE_SLAVA_DARK 0

typedef enum {
    CLOCK_FACE_DIGITAL = 0,
    CLOCK_FACE_MATRIX = 1,
    CLOCK_FACE_WHARTON = 2,
    CLOCK_FACE_SLAVA = 3,
    CLOCK_FACE_SLAVA_DARK = 4,
    CLOCK_FACE_STERNGLAS = 5,
    CLOCK_FACE_AVENIR = 6,
    CLOCK_FACE_MODERN_SILVER = 7,
    CLOCK_FACE_COUNT = 8,
} clock_face_id_t;

typedef enum {
    ALARM_REPEAT_WEEKLY = 0,
    ALARM_REPEAT_ONCE = 1,
} alarm_repeat_mode_t;

typedef struct {
    bool enabled;
    uint8_t hour;
    uint8_t minute;
    uint8_t days_mask;
    uint8_t repeat_mode;
} alarm_config_t;

typedef struct {
    bool enabled;
    uint8_t start_hour;
    uint8_t start_minute;
    uint8_t end_hour;
    uint8_t end_minute;
    uint8_t brightness;
    clock_face_id_t face;
} night_mode_config_t;

typedef struct {
    char ssid[33];
    char password[65];
    int8_t timezone_offset_hours;
} wifi_settings_t;

typedef struct {
    char ssid[33];
    int rssi;
    int authmode;
} clock_wifi_scan_result_t;

typedef struct {
    bool wifi_connected;
    bool wifi_connecting;
    bool wifi_scanning;
    bool time_synced;
    int wifi_rssi;
    char wifi_ip[16];
    char wifi_status[96];
    bool alarm_ringing;
    bool alarm_test_active;
    bool snooze_active;
    time_t snooze_deadline;
    uint32_t wifi_scan_generation;
    size_t wifi_scan_count;
    clock_wifi_scan_result_t wifi_scan_results[CLOCK_WIFI_SCAN_RESULT_MAX];
    bool sunrise_active;
    bool in_night_mode;
    bool night_brightness_override_active;
    uint8_t effective_brightness;
    uint8_t night_brightness_override;
    time_t next_alarm_epoch;
    int8_t next_alarm_index;
    int8_t active_alarm_index;
    time_t last_trigger_epoch_minute;
} app_runtime_state_t;
