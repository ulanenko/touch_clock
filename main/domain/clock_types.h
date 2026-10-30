#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define MAX_ALARMS 5
#define DISPLAY_BRIGHTNESS_MIN_PERCENT 22
#define DISPLAY_BRIGHTNESS_MAX_PERCENT 100
#define CLOCK_WIFI_SCAN_RESULT_MAX 16
#define CLOCK_FACE_THEME_COUNT 3
/* 2023-11-14: a clock reading earlier has never been set (nor synced). */
#define CLOCK_EPOCH_VALID_MIN ((time_t)1700000000)
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
    CLOCK_FACE_EMBER = 8, /* the evening faces: dim, low-glare */
    CLOCK_FACE_NOCTURNE = 9,
    CLOCK_FACE_FILAMENT = 10,
    CLOCK_FACE_COUNT = 11,
} clock_face_id_t;

typedef enum {
    ALARM_REPEAT_WEEKLY = 0,
    ALARM_REPEAT_ONCE = 1,
} alarm_repeat_mode_t;

typedef enum {
    TIME_SYNC_MODE_AUTO = 0,
    TIME_SYNC_MODE_MANUAL = 1,
} time_sync_mode_t;

typedef enum {
    CLOCK_LANGUAGE_ENGLISH = 0,
    CLOCK_LANGUAGE_POLISH = 1,
    CLOCK_LANGUAGE_UKRAINIAN = 2,
    CLOCK_LANGUAGE_RUSSIAN = 3,
    CLOCK_LANGUAGE_DUTCH = 4,
    CLOCK_LANGUAGE_COUNT = 5,
} clock_language_t;

typedef enum {
    CLOCK_UI_STYLE_INSTRUMENT = 0,
    CLOCK_UI_STYLE_ORBIT = 1,
    CLOCK_UI_STYLE_ATELIER = 2,
    CLOCK_UI_STYLE_GLASS = 3, /* Liquid Glass: Orbit's layouts in glass over the face */
    CLOCK_UI_STYLE_COUNT = 4,
} clock_ui_style_t;

#define CLOCK_UI_STYLE_DEFAULT CLOCK_UI_STYLE_ORBIT

typedef enum {
    ALARM_SOUND_MORNING_LIGHT = 0, /* the recorded tone */
    ALARM_SOUND_WIND_CHIMES = 1,   /* the rest are synthesised on the device */
    ALARM_SOUND_MARIMBA = 2,
    ALARM_SOUND_SINGING_BOWL = 3,
    ALARM_SOUND_COUNT = 4,
} alarm_sound_t;

typedef struct {
    bool enabled;
    uint8_t hour;
    uint8_t minute;
    uint8_t days_mask;
    uint8_t repeat_mode;
    bool math_unlock_enabled;
    uint8_t sound; /* alarm_sound_t */
} alarm_config_t;

typedef struct {
    bool enabled;
    bool sunrise_brightness_enabled; /* the sunrise before an alarm, from the night */
    uint8_t sunrise_minutes;         /* its length: 10, 20 or 30 */
    uint8_t sunrise_peak;            /* its brightest, UI percent: 40, 60 or 80 */
    bool sunrise_chime;              /* a soft chime a minute before the alarm */
    uint8_t sunrise_face;            /* the wake-up face: 0 none, 1..3 Daybreak's theme */
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
    uint8_t timezone_id;
    uint8_t time_sync_mode;
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
    bool ota_configured;
    bool ota_busy;
    bool ota_update_available;
    bool ota_reboot_pending;
    int wifi_rssi;
    uint8_t ota_progress;
    char wifi_ip[16];
    char wifi_status[96];
    char ota_status[96];
    char ota_available_version[24];
    char ota_running_partition[16];
    bool alarm_ringing;
    bool alarm_test_active;
    bool snooze_active;
    time_t snooze_deadline;
    uint32_t wifi_scan_generation;
    size_t wifi_scan_count;
    clock_wifi_scan_result_t wifi_scan_results[CLOCK_WIFI_SCAN_RESULT_MAX];
    bool sunrise_active;
    /* The sunrise began in the night (only then does it light the screen); the alarm it is
     * for; and that alarm's soft chime has been played. */
    bool sunrise_from_night;
    time_t sunrise_alarm_epoch;
    time_t sunrise_chimed_epoch;
    bool in_night_mode;
    /* The quick Night key overrides the schedule until the night window next ends: 1 night
     * now, -1 day now, 0 the schedule decides. */
    int8_t night_now;
    time_t night_now_until;
    bool night_brightness_override_active;
    uint8_t effective_brightness;
    uint8_t night_brightness_override;
    time_t next_alarm_epoch;
    int8_t next_alarm_index;
    int8_t active_alarm_index;
    time_t last_trigger_epoch_minute;
} app_runtime_state_t;
