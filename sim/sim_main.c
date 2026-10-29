/*
 * Desktop simulator for the clock UI.
 *
 * Runs the real UI code (main/clock_ui.c, main/ui/) on LVGL 9.3 with the project's
 * sdkconfig, renders to a 720x720 RGB565 frame buffer and plays a scenario script:
 *
 *   sim <scenario.txt> <output-dir>
 *
 * Scenario commands (one per line, # starts a comment):
 *   wait <ms>                      advance simulated time
 *   tap <x> <y>                    press and release
 *   drag <x0> <y0> <x1> <y1> <ms>  press, move linearly, release
 *   press <x> <y> | move <x> <y> | release
 *   call <name>                    run a named debug call (ui_debug_register in the areas)
 *   set <key> <value>              change settings or runtime state; keys: style (0 Instrument,
 *                                  1 Orbit, 2 Atelier), face, language (0 en 1 pl 2 uk 3 ru 4 nl),
 *                                  night_active, alarm_ringing, alarm_math, ota_available,
 *                                  ota_progress, ota_configured, wifi_connected, wifi_scanning,
 *                                  wifi_ssid_empty, time_synced, time_sync_manual, brightness,
 *                                  night_enabled, snooze_minutes_left, next_alarm_minutes,
 *                                  click_sound, alarm_test, sunrise_active, alarm_delete (slot)
 *   shot <name>                    write <output-dir>/<name>.png
 *   alarms                         print the saved alarms
 *   probe <x> <y>                  print the clickable object under a point and its flags
 *   sliders                        drag every visible slider past both ends and print the range reached
 *   costs [n]                      hide each visible object in turn and print the n that cost the most
 *                                  render time (relative numbers; parents include their children)
 *   coldbench <name>               flush the dial cache and time the next frame (dial builds)
 *   bench <name> [n]               time n full-screen renders (default 20) and print the average
 *   benchdrag <name> <x0> <y0> <x1> <y1> <ms>
 *                                  drag and time the UI work (events, layout, render) per step
 *   clock <HH:MM:SS>               set the wall clock (date stays Tue 27 Oct 2026)
 */
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <malloc/malloc.h>
#include <stdio.h>
#include "ui/ui_microbench.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zlib.h>

#include "clock_ui.h"
#include "domain/settings_policy.h"
#include "domain/timezone_rules.h"
#include "lvgl.h"
#include "src/core/lv_obj_private.h"
#include "src/core/lv_obj_class_private.h"
#include "src/draw/lv_draw_private.h"
#include "src/misc/lv_area_private.h"
#include "ui/clock_ui_private.h"
#include "ui/ui_areas.h"
#include "ui/kit/ui_kit.h"

#undef time

#define SIM_W 720
#define SIM_H 720
#define SIM_STEP_MS 5

static uint16_t s_frame[SIM_W * SIM_H];
static uint32_t s_draw_tasks;
static uint16_t s_draw_buf[SIM_W * SIM_H];
static uint32_t s_tick_ms;
static time_t s_now;
static app_settings_t s_settings;
static app_runtime_state_t s_runtime;
static bool s_refresh_pending;
static const char *s_out_dir = ".";

static struct {
    bool pressed;
    int32_t x;
    int32_t y;
} s_pointer;

void sim_prof_mark(const char *func)
{
    if (func[0] == 'l' && strcmp(func, "lv_draw_add_task") == 0) {
        s_draw_tasks++;
    }
}

time_t sim_time(time_t *out)
{
    if (out != NULL) {
        *out = s_now;
    }
    return s_now;
}

int64_t esp_timer_get_time(void)
{
    return (int64_t)s_tick_ms * 1000;
}

void telemetry_update_get_device_id(char *out, size_t out_size)
{
    snprintf(out, out_size, "TC-4F2A91");
}

static uint32_t sim_tick_cb(void)
{
    return s_tick_ms;
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    const uint16_t *src = (const uint16_t *)px_map;
    int32_t w = lv_area_get_width(area);

    for (int32_t y = area->y1; y <= area->y2; y++) {
        memcpy(&s_frame[y * SIM_W + area->x1], &src[(y - area->y1) * w], (size_t)w * 2);
    }
    lv_display_flush_ready(disp);
}

static void pointer_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    LV_UNUSED(indev);
    data->point.x = s_pointer.x;
    data->point.y = s_pointer.y;
    data->state = s_pointer.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

/* --- Simulated controller ------------------------------------------------------------ */

static void mark_refresh(void)
{
    s_refresh_pending = true;
}

static void cb_base_brightness(void *u, uint8_t v) { (void)u; s_settings.base_brightness = v; s_runtime.effective_brightness = v; mark_refresh(); }
static void cb_runtime_night_brightness(void *u, uint8_t v) { (void)u; s_runtime.night_brightness_override = v; mark_refresh(); }
static void cb_temp_floor(void *u, bool e, uint8_t v) { (void)u; (void)e; (void)v; }
static void cb_current_face(void *u, clock_face_id_t f) { (void)u; s_settings.current_face = f; mark_refresh(); }
static void cb_face_theme(void *u, clock_face_id_t f, uint8_t t) { (void)u; s_settings.face_themes[f] = t; mark_refresh(); }
static void cb_night_face(void *u, clock_face_id_t f) { (void)u; s_settings.night_mode.face = f; mark_refresh(); }
static void cb_timezone(void *u, uint8_t id) { (void)u; s_settings.wifi.timezone_id = id; mark_refresh(); }
static void cb_sync_mode(void *u, time_sync_mode_t m) { (void)u; s_settings.wifi.time_sync_mode = (uint8_t)m; mark_refresh(); }
static void cb_manual_time(void *u, uint16_t y, uint8_t mo, uint8_t d, uint8_t h, uint8_t mi) { (void)u; (void)y; (void)mo; (void)d; (void)h; (void)mi; }
static void cb_wifi_save(void *u, const char *ssid, const char *pw)
{
    (void)u;
    snprintf(s_settings.wifi.ssid, sizeof(s_settings.wifi.ssid), "%s", ssid);
    snprintf(s_settings.wifi.password, sizeof(s_settings.wifi.password), "%s", pw);
    mark_refresh();
}
static void cb_noop(void *u) { (void)u; }
static void cb_click_sound(void *u, bool e) { (void)u; s_settings.ui_click_sound_enabled = e; mark_refresh(); }
static void cb_click_volume(void *u, uint8_t v) { (void)u; s_settings.ui_click_volume = v; mark_refresh(); }
static void cb_language(void *u, clock_language_t l) { (void)u; s_settings.language = (uint8_t)l; mark_refresh(); }
static void cb_ui_style(void *u, clock_ui_style_t st) { (void)u; s_settings.ui_style = (uint8_t)st; mark_refresh(); }
static void cb_default_sound(void *u, uint8_t s) { (void)u; s_settings.default_alarm_sound = s; mark_refresh(); }
static void cb_alarm_test_stop(void *u)
{
    (void)u;
    s_runtime.alarm_test_active = false;
    mark_refresh();
}
static void cb_alarm_test(void *u, uint8_t s)
{
    (void)u;
    printf("alarm test sound=%u\n", s);
    s_runtime.alarm_test_active = !s_runtime.alarm_test_active;
    mark_refresh();
}
static void cb_night_enabled(void *u, bool e) { (void)u; s_settings.night_mode.enabled = e; mark_refresh(); }
static void cb_night_now(void *u, bool n) { (void)u; s_runtime.in_night_mode = n; mark_refresh(); }
static void cb_night_schedule(void *u, uint8_t sh, uint8_t sm, uint8_t eh, uint8_t em)
{
    (void)u;
    s_settings.night_mode.start_hour = sh;
    s_settings.night_mode.start_minute = sm;
    s_settings.night_mode.end_hour = eh;
    s_settings.night_mode.end_minute = em;
    mark_refresh();
}
static void cb_night_brightness(void *u, uint8_t v) { (void)u; s_settings.night_mode.brightness = v; mark_refresh(); }
static void cb_sunrise(void *u, bool e) { (void)u; s_settings.night_mode.sunrise_brightness_enabled = e; mark_refresh(); }
static void cb_sunrise_options(void *u, uint8_t m, uint8_t p, bool c, uint8_t f)
{
    (void)u;
    s_settings.night_mode.sunrise_minutes = m;
    s_settings.night_mode.sunrise_peak = p;
    s_settings.night_mode.sunrise_chime = c;
    s_settings.night_mode.sunrise_face = f;
    mark_refresh();
}
static void cb_sunrise_preview(void *u, int16_t progress)
{
    static int16_t last = -1;

    (void)u;
    if ((progress < 0) != (last < 0) || (progress > 1000) != (last > 1000)) {
        printf("sunrise preview: %d\n", progress);
    }
    last = progress;
}
static void cb_alarm_volume(void *u, uint8_t v) { (void)u; s_settings.alarm_volume = v; mark_refresh(); }
static void cb_ascending(void *u, bool e) { (void)u; s_settings.ascending_alarm_enabled = e; mark_refresh(); }
static void cb_snooze(void *u, uint8_t m) { (void)u; s_settings.snooze_minutes = m; mark_refresh(); }
static void cb_alarm_enabled(void *u, uint8_t i, bool e) { (void)u; s_settings.alarms[i].enabled = e; mark_refresh(); }
static void cb_save_alarm(void *u, uint8_t i, const alarm_config_t *a) { (void)u; s_settings.alarms[i] = *a; mark_refresh(); }
static void cb_delete_alarm(void *u, uint8_t i) { (void)u; memset(&s_settings.alarms[i], 0, sizeof(s_settings.alarms[i])); mark_refresh(); }
static void cb_snooze_req(void *u) { (void)u; s_runtime.alarm_ringing = false; s_runtime.snooze_active = true; s_runtime.snooze_deadline = s_now + 9 * 60; mark_refresh(); }
static void cb_stop_req(void *u) { (void)u; s_runtime.alarm_ringing = false; s_runtime.active_alarm_index = -1; mark_refresh(); }

static const clock_ui_callbacks_t s_callbacks = {
    .on_set_base_brightness = cb_base_brightness,
    .on_set_runtime_night_brightness = cb_runtime_night_brightness,
    .on_set_temporary_brightness_floor = cb_temp_floor,
    .on_set_current_face = cb_current_face,
    .on_set_face_theme = cb_face_theme,
    .on_set_night_face = cb_night_face,
    .on_set_timezone = cb_timezone,
    .on_set_time_sync_mode = cb_sync_mode,
    .on_set_manual_time = cb_manual_time,
    .on_save_wifi_credentials = cb_wifi_save,
    .on_wifi_scan_requested = cb_noop,
    .on_wifi_forget_requested = cb_noop,
    .on_wifi_sync_requested = cb_noop,
    .on_ota_check_requested = cb_noop,
    .on_ota_install_requested = cb_noop,
    .on_ui_click_feedback = cb_noop,
    .on_set_ui_click_sound_enabled = cb_click_sound,
    .on_set_ui_click_volume = cb_click_volume,
    .on_set_language = cb_language,
    .on_set_ui_style = cb_ui_style,
    .on_set_default_alarm_sound = cb_default_sound,
    .on_set_night_mode_enabled = cb_night_enabled,
    .on_set_night_now = cb_night_now,
    .on_set_night_schedule = cb_night_schedule,
    .on_set_night_brightness = cb_night_brightness,
    .on_set_night_sunrise_brightness_enabled = cb_sunrise,
    .on_set_night_sunrise_options = cb_sunrise_options,
    .on_sunrise_preview = cb_sunrise_preview,
    .on_set_alarm_volume = cb_alarm_volume,
    .on_set_ascending_alarm_enabled = cb_ascending,
    .on_set_snooze_minutes = cb_snooze,
    .on_set_alarm_enabled = cb_alarm_enabled,
    .on_save_alarm = cb_save_alarm,
    .on_delete_alarm = cb_delete_alarm,
    .on_alarm_snooze_requested = cb_snooze_req,
    .on_alarm_stop_requested = cb_stop_req,
    .on_alarm_test_requested = cb_alarm_test,
    .on_alarm_test_stop_requested = cb_alarm_test_stop,
    .on_next_alarm_cancel_requested = cb_noop,
    .on_next_alarm_cancel_undo_requested = cb_noop,
};

static int s_year = 2026;
static int s_month = 10;
static int s_day = 27;

static time_t make_time(int hour, int minute, int second)
{
    struct tm tm = {0};

    tm.tm_year = s_year - 1900;
    tm.tm_mon = s_month - 1;
    tm.tm_mday = s_day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;
    tm.tm_isdst = 0;
    return timegm(&tm);
}

static void sample_state(void)
{
    static const struct {
        const char *ssid;
        int rssi;
        int auth;
    } networks[] = {
        {"Home-5G", -52, 3},
        {"Home-2G", -58, 3},
        {"Neighbours", -71, 3},
        {"Café Lumière", -71, 3},
        {"iPhone (Anna)", -78, 3},
    };

    settings_policy_set_defaults(&s_settings);
    s_settings.current_face = CLOCK_FACE_STERNGLAS;
    s_settings.base_brightness = 40;
    s_settings.ui_click_sound_enabled = true;
    s_settings.ui_click_volume = 60;
    s_settings.snooze_minutes = 9;
    s_settings.alarm_volume = 70;
    s_settings.ascending_alarm_enabled = true;
    s_settings.language = CLOCK_LANGUAGE_ENGLISH;
    snprintf(s_settings.wifi.ssid, sizeof(s_settings.wifi.ssid), "Home-5G");
    s_settings.wifi.timezone_id = CLOCK_TIMEZONE_ID_EUROPE_CENTRAL;
    s_settings.wifi.time_sync_mode = TIME_SYNC_MODE_AUTO;
    s_settings.night_mode = (night_mode_config_t) {
        .enabled = true,
        .sunrise_brightness_enabled = true,
        .sunrise_minutes = 20,
        .sunrise_peak = 60,
        .sunrise_chime = true,
        .sunrise_face = 1,
        .start_hour = 22,
        .start_minute = 30,
        .end_hour = 7,
        .end_minute = 0,
        .brightness = 30,
        .face = CLOCK_FACE_MATRIX,
    };
    memset(s_settings.alarms, 0, sizeof(s_settings.alarms));
    s_settings.alarms[0] = (alarm_config_t) {.enabled = true, .hour = 6, .minute = 30, .days_mask = 0x3E, .repeat_mode = ALARM_REPEAT_WEEKLY};
    s_settings.alarms[1] = (alarm_config_t) {.enabled = true, .hour = 7, .minute = 45, .days_mask = 0x41, .repeat_mode = ALARM_REPEAT_WEEKLY};
    s_settings.alarms[2] = (alarm_config_t) {.enabled = false, .hour = 9, .minute = 0, .days_mask = 0, .repeat_mode = ALARM_REPEAT_ONCE};
    s_settings.last_synced_epoch = make_time(14, 13, 0);
    s_settings.skipped_alarm_index = -1;

    memset(&s_runtime, 0, sizeof(s_runtime));
    s_runtime.wifi_connected = true;
    s_runtime.time_synced = true;
    s_runtime.wifi_rssi = -52;
    snprintf(s_runtime.wifi_ip, sizeof(s_runtime.wifi_ip), "192.168.1.42");
    snprintf(s_runtime.wifi_status, sizeof(s_runtime.wifi_status), "Connected to Home-5G");
    s_runtime.ota_configured = true;
    s_runtime.ota_update_available = true;
    snprintf(s_runtime.ota_available_version, sizeof(s_runtime.ota_available_version), "1.9.0");
    snprintf(s_runtime.ota_status, sizeof(s_runtime.ota_status), "Update 1.9.0 available");
    snprintf(s_runtime.ota_running_partition, sizeof(s_runtime.ota_running_partition), "ota_0");
    s_runtime.wifi_scan_generation = 1;
    s_runtime.wifi_scan_count = sizeof(networks) / sizeof(networks[0]);
    for (size_t i = 0; i < s_runtime.wifi_scan_count; i++) {
        snprintf(s_runtime.wifi_scan_results[i].ssid, sizeof(s_runtime.wifi_scan_results[i].ssid), "%s", networks[i].ssid);
        s_runtime.wifi_scan_results[i].rssi = networks[i].rssi;
        s_runtime.wifi_scan_results[i].authmode = networks[i].auth;
    }
    s_runtime.effective_brightness = 40;
    s_runtime.next_alarm_index = 0;
    s_runtime.next_alarm_epoch = make_time(6, 30, 0) + 24 * 3600;
    s_runtime.active_alarm_index = -1;
}

/* --- Scenario ------------------------------------------------------------------------- */

static void step(uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += SIM_STEP_MS) {
        uint32_t before = s_tick_ms / 1000;

        s_tick_ms += SIM_STEP_MS;
        if (s_tick_ms / 1000 != before) {
            s_now++;
            clock_ui_tick(s_now);
        }
        lv_timer_handler();
        if (s_refresh_pending) {
            s_refresh_pending = false;
            clock_ui_refresh();
        }
    }
}

static void write_png(const char *name)
{
    char path[512];
    const size_t row_bytes = SIM_W * 3 + 1;
    uint8_t *raw = malloc(row_bytes * SIM_H);
    uLongf zlen = compressBound(row_bytes * SIM_H);
    uint8_t *z = malloc(zlen);
    FILE *f;

    lv_refr_now(NULL);
    for (int y = 0; y < SIM_H; y++) {
        uint8_t *row = raw + y * row_bytes;

        row[0] = 0;
        for (int x = 0; x < SIM_W; x++) {
            uint16_t v = s_frame[y * SIM_W + x];

            row[1 + x * 3] = (uint8_t)(((v >> 11) & 0x1F) * 255 / 31);
            row[2 + x * 3] = (uint8_t)(((v >> 5) & 0x3F) * 255 / 63);
            row[3 + x * 3] = (uint8_t)((v & 0x1F) * 255 / 31);
        }
    }
    compress2(z, &zlen, raw, row_bytes * SIM_H, 6);

    snprintf(path, sizeof(path), "%s/%s.png", s_out_dir, name);
    f = fopen(path, "wb");
    if (f != NULL) {
        static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
        uint8_t ihdr[13] = {0};
        struct {
            const char *type;
            const uint8_t *data;
            uint32_t len;
        } chunks[3] = {{"IHDR", ihdr, 13}, {"IDAT", z, (uint32_t)zlen}, {"IEND", NULL, 0}};

        ihdr[0] = 0; ihdr[1] = 0; ihdr[2] = (SIM_W >> 8) & 0xFF; ihdr[3] = SIM_W & 0xFF;
        ihdr[4] = 0; ihdr[5] = 0; ihdr[6] = (SIM_H >> 8) & 0xFF; ihdr[7] = SIM_H & 0xFF;
        ihdr[8] = 8; ihdr[9] = 2;
        fwrite(sig, 1, 8, f);
        for (int i = 0; i < 3; i++) {
            uint8_t len_be[4] = {chunks[i].len >> 24, chunks[i].len >> 16, chunks[i].len >> 8, chunks[i].len};
            uint32_t crc = crc32(0, (const Bytef *)chunks[i].type, 4);
            uint8_t crc_be[4];

            if (chunks[i].len > 0) {
                crc = crc32(crc, chunks[i].data, chunks[i].len);
            }
            crc_be[0] = crc >> 24; crc_be[1] = crc >> 16; crc_be[2] = crc >> 8; crc_be[3] = crc;
            fwrite(len_be, 1, 4, f);
            fwrite(chunks[i].type, 1, 4, f);
            if (chunks[i].len > 0) {
                fwrite(chunks[i].data, 1, chunks[i].len, f);
            }
            fwrite(crc_be, 1, 4, f);
        }
        fclose(f);
        printf("shot %s\n", path);
    }
    free(raw);
    free(z);
}

static void gesture(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t ms)
{
    s_pointer.x = x0;
    s_pointer.y = y0;
    s_pointer.pressed = true;
    step(30);
    for (uint32_t t = SIM_STEP_MS; t <= ms; t += SIM_STEP_MS) {
        s_pointer.x = x0 + (x1 - x0) * (int32_t)t / (int32_t)ms;
        s_pointer.y = y0 + (y1 - y0) * (int32_t)t / (int32_t)ms;
        step(SIM_STEP_MS);
    }
    s_pointer.x = x1;
    s_pointer.y = y1;
    step(30);
    s_pointer.pressed = false;
    step(30);
}

static bool apply_set(const char *key, const char *value)
{
    int v = atoi(value);

    if (strcmp(key, "style") == 0) {
        s_settings.ui_style = (uint8_t)v;
    } else if (strcmp(key, "face") == 0) {
        s_settings.current_face = (clock_face_id_t)v;
    } else if (strcmp(key, "face_theme") == 0) {
        s_settings.face_themes[s_settings.current_face] = (uint8_t)v;
    } else if (strcmp(key, "language") == 0) {
        s_settings.language = (uint8_t)v;
    } else if (strcmp(key, "night_active") == 0) {
        s_runtime.in_night_mode = v != 0;
    } else if (strcmp(key, "alarm_ringing") == 0) {
        s_runtime.alarm_ringing = v != 0;
        s_runtime.active_alarm_index = v != 0 ? 0 : -1;
    } else if (strcmp(key, "alarm_math") == 0) {
        s_settings.alarms[0].math_unlock_enabled = v != 0;
    } else if (strcmp(key, "ota_available") == 0) {
        s_runtime.ota_update_available = v != 0;
    } else if (strcmp(key, "wifi_connected") == 0) {
        s_runtime.wifi_connected = v != 0;
    } else if (strcmp(key, "brightness") == 0) {
        s_settings.base_brightness = (uint8_t)v;
        s_runtime.effective_brightness = (uint8_t)v;
    } else if (strcmp(key, "night_enabled") == 0) {
        s_settings.night_mode.enabled = v != 0;
    } else if (strcmp(key, "ota_progress") == 0) {
        s_runtime.ota_busy = v > 0 && v < 100;
        s_runtime.ota_progress = (uint8_t)v;
    } else if (strcmp(key, "ota_configured") == 0) {
        s_runtime.ota_configured = v != 0;
    } else if (strcmp(key, "snooze_minutes_left") == 0) {
        s_runtime.snooze_active = v > 0;
        s_runtime.snooze_deadline = s_now + v * 60;
    } else if (strcmp(key, "next_alarm_minutes") == 0) {
        s_runtime.next_alarm_index = v > 0 ? 0 : -1;
        s_runtime.next_alarm_epoch = v > 0 ? s_now + v * 60 : 0;
    } else if (strcmp(key, "wifi_scanning") == 0) {
        s_runtime.wifi_scanning = v != 0;
    } else if (strcmp(key, "wifi_ssid_empty") == 0) {
        if (v != 0) {
            s_settings.wifi.ssid[0] = '\0';
            s_runtime.wifi_connected = false;
        }
    } else if (strcmp(key, "time_synced") == 0) {
        s_runtime.time_synced = v != 0;
    } else if (strcmp(key, "click_sound") == 0) {
        s_settings.ui_click_sound_enabled = v != 0;
    } else if (strcmp(key, "alarm_test") == 0) {
        s_runtime.alarm_test_active = v != 0;
    } else if (strcmp(key, "sunrise_active") == 0) {
        s_runtime.sunrise_active = v != 0;
    } else if (strcmp(key, "sunrise_from_night") == 0) {
        s_runtime.sunrise_from_night = v != 0;
    } else if (strcmp(key, "alarm_delete") == 0) {
        /* Deleted slots look like app_action_delete_alarm leaves them. */
        s_settings.alarms[v] = (alarm_config_t) {.enabled = false, .hour = 7, .minute = 0, .days_mask = 0x7F,
                                                 .repeat_mode = ALARM_REPEAT_ONCE};
    } else if (strcmp(key, "time_sync_manual") == 0) {
        s_settings.wifi.time_sync_mode = v != 0 ? TIME_SYNC_MODE_MANUAL : TIME_SYNC_MODE_AUTO;
    } else {
        return false;
    }
    clock_ui_refresh();
    return true;
}

static double now_ms(void)
{
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

static void bench_drag(const char *name, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t ms)
{
    double total = 0;
    double worst = 0;
    int steps = 0;

    s_pointer.x = x0;
    s_pointer.y = y0;
    s_pointer.pressed = true;
    step(30);
    for (uint32_t t = SIM_STEP_MS * 3; t <= ms; t += SIM_STEP_MS * 3) {
        double start;
        double spent;

        s_pointer.x = x0 + (x1 - x0) * (int32_t)t / (int32_t)ms;
        s_pointer.y = y0 + (y1 - y0) * (int32_t)t / (int32_t)ms;
        start = now_ms();
        step(SIM_STEP_MS * 3);
        lv_obj_invalidate(lv_screen_active());
        lv_refr_now(NULL);
        spent = now_ms() - start;
        total += spent;
        worst = spent > worst ? spent : worst;
        steps++;
    }
    s_pointer.pressed = false;
    step(300);
    printf("benchdrag %s avg %.2f ms max %.2f ms (%d steps)\n", name, total / steps, worst, steps);
}

static void find_sliders(lv_obj_t *obj, lv_obj_t **out, int *count, int max)
{
    if (obj == NULL || lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }
    if ((lv_obj_check_type(obj, &lv_slider_class) || lv_obj_check_type(obj, &lv_arc_class)) && *count < max) {
        out[(*count)++] = obj;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) {
        find_sliders(lv_obj_get_child(obj, (int32_t)i), out, count, max);
    }
}

static void test_sliders(void)
{
    lv_obj_t *found[16];
    int count = 0;

    find_sliders(lv_screen_active(), found, &count, 16);
    for (int i = 0; i < count; i++) {
        lv_obj_t *obj = found[i];
        bool arc = lv_obj_check_type(obj, &lv_arc_class);
        lv_area_t a;
        int32_t lo;
        int32_t hi;
        int32_t cy;

        lv_obj_get_coords(obj, &a);
        if (a.y2 < 0 || a.y1 > SIM_H) {
            continue;
        }
        if (arc) {
            /* Drag along the arc's radius from 25 degrees before its start to 25 after its end. */
            int32_t rot = lv_arc_get_rotation(obj);
            int32_t start = lv_arc_get_bg_angle_start(obj) + rot;
            int32_t end = lv_arc_get_bg_angle_end(obj) + rot;
            int32_t cx = (a.x1 + a.x2) / 2;
            int32_t cyy = (a.y1 + a.y2) / 2;
            int32_t r = (a.x2 - a.x1) / 2 - lv_obj_get_style_arc_width(obj, LV_PART_MAIN) / 2;
            int32_t first = -1;
            int32_t last = -1;

            if (end < start) {
                end += 360;
            }
            for (int pass = 0; pass < 2; pass++) {
                /* Press on the middle of the track, like a finger, then slide past one end. */
                int32_t from = (start + end) / 2;
                int32_t to = pass == 0 ? start - 25 : end + 25;
                int32_t steps = 60;

                s_pointer.pressed = true;
                for (int k = 0; k <= steps; k++) {
                    float deg = (float)(from + (to - from) * k / steps);
                    s_pointer.x = cx + (int32_t)(r * cosf(deg * 3.14159265f / 180.0f));
                    s_pointer.y = cyy + (int32_t)(r * sinf(deg * 3.14159265f / 180.0f));
                    step(SIM_STEP_MS * 3);
                }
                s_pointer.pressed = false;
                step(60);
                if (pass == 0) {
                    first = lv_arc_get_value(obj);
                } else {
                    last = lv_arc_get_value(obj);
                }
            }
            printf("arc %d angles %d..%d (rot %d) range %d..%d: start end -> %d, far end -> %d%s\n", i, (int)start, (int)end,
                   (int)rot, (int)lv_arc_get_min_value(obj), (int)lv_arc_get_max_value(obj), (int)first, (int)last,
                   ((first == lv_arc_get_min_value(obj) && last == lv_arc_get_max_value(obj)) ||
                    (last == lv_arc_get_min_value(obj) && first == lv_arc_get_max_value(obj))) ? "" : "  <-- NOT FULL RANGE");
            continue;
        }
        cy = (a.y1 + a.y2) / 2;
        gesture((a.x1 + a.x2) / 2, cy, a.x1, cy, 300);
        printf("slider %d at its left end x=%d -> %d\n", i, (int)a.x1, (int)lv_slider_get_value(obj));
        gesture((a.x1 + a.x2) / 2, cy, a.x2, cy, 300);
        printf("slider %d at its right end x=%d -> %d\n", i, (int)a.x2, (int)lv_slider_get_value(obj));
        gesture((a.x1 + a.x2) / 2, cy, a.x1 - 60, cy, 300);
        lo = lv_slider_get_value(obj);
        gesture((a.x1 + a.x2) / 2, cy, a.x2 + 60, cy, 300);
        hi = lv_slider_get_value(obj);
        printf("slider %d x %d..%d range %d..%d: dragged to %d .. %d%s\n", i, (int)a.x1, (int)a.x2,
               (int)lv_slider_get_min_value(obj), (int)lv_slider_get_max_value(obj), (int)lo, (int)hi,
               (lo == lv_slider_get_min_value(obj) && hi == lv_slider_get_max_value(obj)) ? "" : "  <-- NOT FULL RANGE");
    }
}

static double render_ms(int n)
{
    double start = now_ms();

    for (int i = 0; i < n; i++) {
        lv_obj_invalidate(lv_screen_active());
        lv_refr_now(NULL);
    }
    return (now_ms() - start) / n;
}

typedef struct {
    lv_obj_t *obj;
    double cost;
} obj_cost_t;

static void collect_visible(lv_obj_t *obj, lv_obj_t **out, int *count, int max)
{
    if (obj == NULL || lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN) || *count >= max) {
        return;
    }
    out[(*count)++] = obj;
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) {
        collect_visible(lv_obj_get_child(obj, (int32_t)i), out, count, max);
    }
}

static int cmp_cost(const void *a, const void *b)
{
    double d = ((const obj_cost_t *)b)->cost - ((const obj_cost_t *)a)->cost;
    return d > 0 ? 1 : d < 0 ? -1 : 0;
}

static void object_costs(int top)
{
    static lv_obj_t *objs[2048];
    static obj_cost_t costs[2048];
    int count = 0;
    double base;

    collect_visible(lv_screen_active(), objs, &count, 2048);
    render_ms(3);
    base = render_ms(12);
    for (int i = 1; i < count; i++) {
        lv_obj_add_flag(objs[i], LV_OBJ_FLAG_HIDDEN);
        costs[i] = (obj_cost_t) {.obj = objs[i], .cost = base - render_ms(6)};
        lv_obj_remove_flag(objs[i], LV_OBJ_FLAG_HIDDEN);
    }
    qsort(costs + 1, (size_t)(count - 1), sizeof(costs[0]), cmp_cost);
    printf("costs: %d objects, full render %.3f ms\n", count, base);
    for (int i = 1; i < count && i <= top; i++) {
        lv_area_t a;
        const char *text = "";
        lv_obj_t *o = costs[i].obj;

        lv_obj_get_coords(o, &a);
        if (lv_obj_check_type(o, &lv_label_class)) {
            text = lv_label_get_text(o);
        }
        printf("  %6.3f ms (%4.0f%%) %-10s %4dx%-4d at %4d,%-4d children %-3u %.30s\n", costs[i].cost,
               100.0 * costs[i].cost / base, o->class_p->name ? o->class_p->name : "?", (int)lv_area_get_width(&a),
               (int)lv_area_get_height(&a), (int)a.x1, (int)a.y1, (unsigned)lv_obj_get_child_count(o), text);
    }
}

/* What one frame draws, by object and draw-task type: the host's timings say little about
 * the device's, but the pixels each kind of task covers do. */
#define DRAWCOST_TYPES 16
typedef struct {
    lv_obj_t *obj;
    uint32_t count[DRAWCOST_TYPES];
    uint64_t px[DRAWCOST_TYPES];
    uint64_t weighted;
} drawcost_t;

static drawcost_t s_drawcost[2048];
static int s_drawcost_count;

static const char *drawcost_type_name(int type)
{
    switch (type) {
    case LV_DRAW_TASK_TYPE_FILL: return "fill";
    case LV_DRAW_TASK_TYPE_BORDER: return "border";
    case LV_DRAW_TASK_TYPE_BOX_SHADOW: return "shadow";
    case LV_DRAW_TASK_TYPE_LETTER: return "letter";
    case LV_DRAW_TASK_TYPE_LABEL: return "label";
    case LV_DRAW_TASK_TYPE_IMAGE: return "image";
    case LV_DRAW_TASK_TYPE_LAYER: return "layer";
    case LV_DRAW_TASK_TYPE_LINE: return "line";
    case LV_DRAW_TASK_TYPE_ARC: return "arc";
    case LV_DRAW_TASK_TYPE_TRIANGLE: return "triangle";
    case LV_DRAW_TASK_TYPE_MASK_RECTANGLE: return "maskrect";
    case LV_DRAW_TASK_TYPE_MASK_BITMAP: return "maskbmp";
    default: return "other";
    }
}

static void drawcost_cb(lv_event_t *e)
{
    lv_draw_task_t *t = lv_event_get_draw_task(e);
    drawcost_t *c = lv_event_get_user_data(e);
    int type = (int)lv_draw_task_get_type(t);
    lv_area_t a;
    uint64_t px;
    uint64_t weight = 1;

    if (!lv_area_intersect(&a, &t->area, &t->clip_area)) {
        return;
    }
    px = (uint64_t)lv_area_get_size(&a);
    if (type >= DRAWCOST_TYPES) {
        type = 0;
    }
    if (type == LV_DRAW_TASK_TYPE_FILL) {
        const lv_draw_fill_dsc_t *d = t->draw_dsc;
        weight = (d->radius > 0 ? 2 : 1) * (d->opa < LV_OPA_MAX || d->grad.dir != LV_GRAD_DIR_NONE ? 2 : 1);
    } else if (type == LV_DRAW_TASK_TYPE_BOX_SHADOW || type == LV_DRAW_TASK_TYPE_LAYER) {
        weight = 6;
    } else if (type == LV_DRAW_TASK_TYPE_BORDER || type == LV_DRAW_TASK_TYPE_ARC) {
        weight = 2;
    }
    c->count[type]++;
    c->px[type] += px;
    c->weighted += px * weight;
}

static void collect_all(lv_obj_t *obj, lv_obj_t **out, int *count, int max)
{
    if (*count >= max || lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }
    out[(*count)++] = obj;
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) {
        collect_all(lv_obj_get_child(obj, i), out, count, max);
    }
}

static int cmp_drawcost(const void *a, const void *b)
{
    const drawcost_t *x = a, *y = b;

    return x->weighted < y->weighted ? 1 : (x->weighted > y->weighted ? -1 : 0);
}

/* --- Device cost model ------------------------------------------------------------------
 * Every draw task of a frame falls in a class by what the clock does with it (the PPA for
 * full-width opaque blits, the sparse blender for registered ARGB tiles, LVGL's CPU paths for
 * the rest); a frame's device time is estimated as c0 + ctask x tasks + sum(c_k x pixels_k),
 * with coefficients fitted from microbenchmarks timed on the clock (sim/cost_fit.py, written to
 * sim/cost_model.txt). */
enum {
    CC_FILL, CC_FILL_PPA, CC_FILL_ROUND, CC_FILL_OPA, CC_BORDER, CC_SHADOW, CC_LABEL, CC_IMG565,
    CC_IMG565_PPA, CC_IMG565_CLIP, CC_IMG565_OPA, CC_ARGB, CC_ARGB_SPARSE, CC_TRANSFORM, CC_LAYER,
    CC_LINE, CC_ARC, CC_OTHER, CC_COUNT
};
static const char *const s_cc_names[CC_COUNT] = {
    "fill", "fill_ppa", "fill_round", "fill_opa", "border", "shadow", "label", "img565",
    "img565_ppa", "img565_clip", "img565_opa", "argb", "argb_sparse", "transform", "layer",
    "line", "arc", "other",
};
typedef struct {
    uint32_t tasks;
    double px[CC_COUNT];
} costfeat_t;
static costfeat_t s_feat;

static int cost_class(lv_draw_task_t *t, const lv_area_t *a)
{
    int32_t w = lv_area_get_width(a);
    int32_t h = lv_area_get_height(a);

    switch (lv_draw_task_get_type(t)) {
    case LV_DRAW_TASK_TYPE_FILL: {
        const lv_draw_fill_dsc_t *d = t->draw_dsc;

        if (d->opa < LV_OPA_MAX || d->grad.dir != LV_GRAD_DIR_NONE) {
            return CC_FILL_OPA;
        }
        if (d->radius > 0) {
            return CC_FILL_ROUND;
        }
        return w >= 700 && h >= 16 ? CC_FILL_PPA : CC_FILL;
    }
    case LV_DRAW_TASK_TYPE_BORDER:
        return CC_BORDER;
    case LV_DRAW_TASK_TYPE_BOX_SHADOW:
        return CC_SHADOW;
    case LV_DRAW_TASK_TYPE_LABEL:
    case LV_DRAW_TASK_TYPE_LETTER:
        return CC_LABEL;
    case LV_DRAW_TASK_TYPE_LAYER: {
        const lv_draw_image_dsc_t *d = t->draw_dsc;

        return d->scale_x != LV_SCALE_NONE || d->scale_y != LV_SCALE_NONE || d->rotation != 0 ? CC_TRANSFORM
                                                                                                 : CC_LAYER;
    }
    case LV_DRAW_TASK_TYPE_LINE:
    case LV_DRAW_TASK_TYPE_TRIANGLE:
        return CC_LINE;
    case LV_DRAW_TASK_TYPE_ARC:
        return CC_ARC;
    case LV_DRAW_TASK_TYPE_IMAGE: {
        const lv_draw_image_dsc_t *d = t->draw_dsc;
        const lv_image_header_t *hd = (const lv_image_header_t *)d->src;
        const void *data = NULL;
        lv_color_format_t cf = LV_COLOR_FORMAT_UNKNOWN;

        if (lv_image_src_get_type(d->src) == LV_IMAGE_SRC_VARIABLE) {
            /* An lv_draw_buf_t and an lv_image_dsc_t both start with the header. */
            cf = (lv_color_format_t)hd->cf;
            data = hd->magic == LV_IMAGE_HEADER_MAGIC ? ((const lv_draw_buf_t *)d->src)->data : NULL;
        }
        if (d->scale_x != LV_SCALE_NONE || d->scale_y != LV_SCALE_NONE || d->rotation != 0) {
            return CC_TRANSFORM;
        }
        if (cf == LV_COLOR_FORMAT_ARGB8888) {
            return data != NULL && ui_sparse_image_known(data) ? CC_ARGB_SPARSE : CC_ARGB;
        }
        if (d->clip_radius > 0) {
            return CC_IMG565_CLIP;
        }
        if (d->opa < LV_OPA_MAX) {
            return CC_IMG565_OPA;
        }
        return w >= 700 && h >= 16 ? CC_IMG565_PPA : CC_IMG565;
    }
    default:
        return CC_OTHER;
    }
}

/* A draw unit that takes no tasks: its evaluate hook sees every task LVGL creates (those of
 * draw callbacks and layers too), which is where the features are counted. */
static bool s_feat_on;

static int32_t cost_unit_evaluate(lv_draw_unit_t *u, lv_draw_task_t *t)
{
    lv_area_t a;

    LV_UNUSED(u);
    if (!s_feat_on || !lv_area_intersect(&a, &t->area, &t->clip_area)) {
        return 0;
    }
    s_feat.tasks++;
    s_feat.px[cost_class(t, &a)] += (double)lv_area_get_size(&a);
    return 0;
}

static int32_t cost_unit_dispatch(lv_draw_unit_t *u, lv_layer_t *layer)
{
    LV_UNUSED(u);
    LV_UNUSED(layer);
    return LV_DRAW_UNIT_IDLE;
}

static void cost_unit_install(void)
{
    static bool done;
    lv_draw_unit_t *u;

    if (done) {
        return;
    }
    u = lv_draw_create_unit(sizeof(lv_draw_unit_t));
    u->evaluate_cb = cost_unit_evaluate;
    u->dispatch_cb = cost_unit_dispatch;
    u->name = "cost";
    done = true;
}

/* Counts one full redraw of the screen. */
static void cost_features(costfeat_t *out)
{
    cost_unit_install();
    memset(&s_feat, 0, sizeof(s_feat));
    lv_obj_invalidate(lv_screen_active());
    lv_obj_invalidate(lv_layer_top());
    s_feat_on = true;
    lv_refr_now(NULL);
    s_feat_on = false;
    *out = s_feat;
}

static void cost_print(FILE *f, const char *name, const costfeat_t *c)
{
    fprintf(f, "%s\ttasks=%u", name, c->tasks);
    for (int k = 0; k < CC_COUNT; k++) {
        if (c->px[k] > 0) {
            fprintf(f, "\t%s=%.0f", s_cc_names[k], c->px[k]);
        }
    }
    fprintf(f, "\n");
}

/* Estimated device render time of one full frame, from sim/cost_model.txt. */
static void cost_estimate(const char *label)
{
    double coef[CC_COUNT] = {0};
    double c0 = 0;
    double ctask = 0;
    double total;
    char key[64];
    double value;
    costfeat_t c;
    FILE *f = fopen(SIM_COST_MODEL, "r");

    if (f == NULL) {
        printf("estimate: no %s (run sim/cost_fit.py)\n", SIM_COST_MODEL);
        return;
    }
    while (fscanf(f, "%63s %lf", key, &value) == 2) {
        if (strcmp(key, "c0") == 0) {
            c0 = value;
        } else if (strcmp(key, "task") == 0) {
            ctask = value;
        }
        for (int k = 0; k < CC_COUNT; k++) {
            if (strcmp(key, s_cc_names[k]) == 0) {
                coef[k] = value;
            }
        }
    }
    fclose(f);
    cost_features(&c);
    total = c0 + ctask * c.tasks;
    printf("estimate %s:", label);
    for (int k = 0; k < CC_COUNT; k++) {
        double us = coef[k] * c.px[k];

        total += us;
        if (us >= 300) {
            printf(" %s %.1f", s_cc_names[k], us / 1000.0);
        }
    }
    printf(" tasks %u %.1f | %.1f ms\n", c.tasks, ctask * c.tasks / 1000.0, total / 1000.0);
}

static double now_ms(void);
static int64_t host_us(void)
{
    return (int64_t)(now_ms() * 1000.0);
}

/* The microbenchmark scenes' features (and the CPU loop's host time), for sim/cost_fit.py. */
static void microbench(const char *path)
{
    FILE *f = fopen(path, "w");

    if (f == NULL) {
        return;
    }
    for (int i = 0; i < ui_microbench_count(); i++) {
        const char *name = ui_microbench_name(i);
        costfeat_t c;

        if (strcmp(name, "cpu_loop") == 0) {
            ui_microbench_hide();
            fprintf(f, "cpu_loop\thost_us=%lld\n", (long long)ui_microbench_cpu_loop(host_us));
            continue;
        }
        ui_microbench_show(i, false);
        lv_refr_now(NULL);
        cost_features(&c);
        cost_print(f, name, &c);
    }
    ui_microbench_hide();
    lv_refr_now(NULL);
    fclose(f);
    printf("microbench: %d scenes -> %s\n", ui_microbench_count(), path);
}

static void draw_costs(int top)
{
    static lv_obj_t *objs[2048];
    uint32_t total_count[DRAWCOST_TYPES] = {0};
    uint64_t total_px[DRAWCOST_TYPES] = {0};
    uint64_t total_weighted = 0;
    int count = 0;

    collect_all(lv_layer_top(), objs, &count, 1024);
    collect_all(lv_screen_active(), objs, &count, 2048);
    memset(s_drawcost, 0, sizeof(s_drawcost));
    s_drawcost_count = count;
    for (int i = 0; i < count; i++) {
        s_drawcost[i].obj = objs[i];
        lv_obj_add_flag(objs[i], LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
        lv_obj_add_event_cb(objs[i], drawcost_cb, LV_EVENT_DRAW_TASK_ADDED, &s_drawcost[i]);
    }
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(NULL);
    for (int i = 0; i < count; i++) {
        lv_obj_remove_event_cb_with_user_data(objs[i], drawcost_cb, &s_drawcost[i]);
        lv_obj_remove_flag(objs[i], LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
        for (int t = 0; t < DRAWCOST_TYPES; t++) {
            total_count[t] += s_drawcost[i].count[t];
            total_px[t] += s_drawcost[i].px[t];
        }
        total_weighted += s_drawcost[i].weighted;
    }
    printf("drawcost: %d objects, weighted %.2f Mpx\n", count, total_weighted / 1e6);
    for (int t = 0; t < DRAWCOST_TYPES; t++) {
        if (total_count[t] > 0) {
            printf("  %-9s %5u tasks %8.3f Mpx\n", drawcost_type_name(t), total_count[t], total_px[t] / 1e6);
        }
    }
    qsort(s_drawcost, (size_t)count, sizeof(s_drawcost[0]), cmp_drawcost);
    for (int i = 0; i < count && i < top; i++) {
        drawcost_t *c = &s_drawcost[i];
        lv_area_t a;
        char kinds[160] = "";
        size_t n = 0;

        lv_obj_get_coords(c->obj, &a);
        for (int t = 0; t < DRAWCOST_TYPES; t++) {
            if (c->count[t] > 0 && n < sizeof(kinds)) {
                n += (size_t)snprintf(kinds + n, sizeof(kinds) - n, " %s:%u/%.0fk", drawcost_type_name(t),
                                      c->count[t], c->px[t] / 1e3);
            }
        }
        printf("  %7.0fk %-10s %4dx%-4d at %4d,%-4d r=%d opa=%d%s %.24s\n", c->weighted / 1e3,
               c->obj->class_p->name ? c->obj->class_p->name : "?", (int)lv_area_get_width(&a),
               (int)lv_area_get_height(&a), (int)a.x1, (int)a.y1, (int)lv_obj_get_style_radius(c->obj, 0),
               (int)lv_obj_get_style_bg_opa(c->obj, 0), kinds,
               lv_obj_check_type(c->obj, &lv_label_class) ? lv_label_get_text(c->obj) : "");
    }
}

static int run_scenario(FILE *f)
{
    char line[256];
    int line_no = 0;

    while (fgets(line, sizeof(line), f) != NULL) {
        char cmd[32] = {0};
        char a[64] = {0};
        char b[64] = {0};
        int x0, y0, x1, y1, ms;
        char *hash = strchr(line, '#');

        line_no++;
        if (hash != NULL) {
            *hash = '\0';
        }
        if (sscanf(line, "%31s", cmd) != 1) {
            continue;
        }

        if (strcmp(cmd, "wait") == 0 && sscanf(line, "%*s %d", &ms) == 1) {
            step((uint32_t)ms);
        } else if (strcmp(cmd, "tap") == 0 && sscanf(line, "%*s %d %d", &x0, &y0) == 2) {
            gesture(x0, y0, x0, y0, 60);
        } else if (strcmp(cmd, "drag") == 0 && sscanf(line, "%*s %d %d %d %d %d", &x0, &y0, &x1, &y1, &ms) == 5) {
            gesture(x0, y0, x1, y1, (uint32_t)ms);
        } else if (strcmp(cmd, "press") == 0 && sscanf(line, "%*s %d %d", &x0, &y0) == 2) {
            s_pointer.x = x0;
            s_pointer.y = y0;
            s_pointer.pressed = true;
            step(30);
        } else if (strcmp(cmd, "move") == 0 && sscanf(line, "%*s %d %d", &x0, &y0) == 2) {
            s_pointer.x = x0;
            s_pointer.y = y0;
            step(SIM_STEP_MS * 2);
        } else if (strcmp(cmd, "release") == 0) {
            s_pointer.pressed = false;
            step(30);
        } else if (strcmp(cmd, "call") == 0 && sscanf(line, "%*s %63s", a) == 1) {
            if (!ui_debug_call(clock_ui_get_context(), a)) {
                fprintf(stderr, "line %d: unknown call %s\n", line_no, a);
                return 1;
            }
            step(SIM_STEP_MS);
        } else if (strcmp(cmd, "set") == 0 && sscanf(line, "%*s %63s %63s", a, b) == 2) {
            if (!apply_set(a, b)) {
                fprintf(stderr, "line %d: unknown setting %s\n", line_no, a);
                return 1;
            }
            step(SIM_STEP_MS);
        } else if (strcmp(cmd, "date") == 0 && sscanf(line, "%*s %d-%d-%d", &x0, &y0, &x1) == 3) {
            /* The day "clock" sets the time on. */
            s_year = x0;
            s_month = y0;
            s_day = x1;
        } else if (strcmp(cmd, "clock") == 0 && sscanf(line, "%*s %d:%d:%d", &x0, &y0, &x1) == 3) {
            s_now = make_time(x0, y0, x1);
            clock_ui_tick(s_now);
            step(SIM_STEP_MS);
        } else if (strcmp(cmd, "benchdrag") == 0 &&
                   sscanf(line, "%*s %63s %d %d %d %d %d", a, &x0, &y0, &x1, &y1, &ms) == 6) {
            bench_drag(a, x0, y0, x1, y1, (uint32_t)ms);
        } else if (strcmp(cmd, "hold") == 0 && sscanf(line, "%*s %d", &ms) == 1) {
            /* Real seconds to wait, e.g. to inspect the process's heap from outside. */
            fflush(stdout);
            sleep((unsigned)ms);
        } else if (strcmp(cmd, "mem") == 0) {
            /* Heap in use, to find what holds memory after a step. */
            malloc_statistics_t stats;

            malloc_zone_statistics(NULL, &stats);
            printf("mem %zu KB %s", stats.size_in_use / 1024, line + 4);
        } else if (strcmp(cmd, "microbench") == 0 && sscanf(line, "%*s %63s", a) == 1) {
            microbench(a);
        } else if (strcmp(cmd, "timecall") == 0 && sscanf(line, "%*s %63s", a) == 1) {
            /* A debug call's UI-thread time (CPU work: builds, bakes), scaled to the clock by the
             * cost model's cpu_scale; its first frame is "estimate"'s business. */
            double scale = 0;
            double t0;
            char key[64];
            double value;
            FILE *mf = fopen(SIM_COST_MODEL, "r");

            while (mf != NULL && fscanf(mf, "%63s %lf", key, &value) == 2) {
                if (strcmp(key, "cpu_scale") == 0) {
                    scale = value;
                }
            }
            if (mf != NULL) {
                fclose(mf);
            }
            t0 = now_ms();
            ui_debug_call(clock_ui_get_context(), a);
            printf("timecall %s: host %.2f ms, clock ~%.0f ms\n", a, now_ms() - t0, (now_ms() - t0) * scale);
        } else if (strcmp(cmd, "estimate") == 0) {
            if (sscanf(line, "%*s %63s", a) != 1) {
                strcpy(a, "screen");
            }
            cost_estimate(a);
        } else if (strcmp(cmd, "drawcost") == 0) {
            int n = 15;

            sscanf(line, "%*s %d", &n);
            draw_costs(n);
        } else if (strcmp(cmd, "costs") == 0) {
            int n = 12;

            sscanf(line, "%*s %d", &n);
            object_costs(n);
        } else if (strcmp(cmd, "probe") == 0 && sscanf(line, "%*s %d %d", &x0, &y0) == 2) {
            lv_point_t p = {x0, y0};
            lv_obj_t *o = lv_indev_search_obj(lv_screen_active(), &p);

            if (o == NULL) {
                printf("probe %d,%d: nothing\n", x0, y0);
            } else {
                lv_area_t ar;

                lv_obj_get_coords(o, &ar);
                printf("probe %d,%d: %s %dx%d at %d,%d press_lock=%d clickable=%d\n", x0, y0,
                       o->class_p->name ? o->class_p->name : "?", (int)lv_area_get_width(&ar),
                       (int)lv_area_get_height(&ar), (int)ar.x1, (int)ar.y1,
                       lv_obj_has_flag(o, LV_OBJ_FLAG_PRESS_LOCK), lv_obj_has_flag(o, LV_OBJ_FLAG_CLICKABLE));
            }
        } else if (strcmp(cmd, "sliders") == 0) {
            test_sliders();
        } else if (strcmp(cmd, "alarms") == 0) {
            for (int i = 0; i < MAX_ALARMS; i++) {
                const alarm_config_t *al = &s_settings.alarms[i];

                printf("alarm %d: %s %02u:%02u days=0x%02x repeat=%u math=%d\n", i, al->enabled ? "on " : "off",
                       al->hour, al->minute, al->days_mask, al->repeat_mode, al->math_unlock_enabled);
            }
        } else if (strcmp(cmd, "coldbench") == 0 && sscanf(line, "%*s %63s", a) == 1) {
            double start;

            kit_dial_cache_flush();
            start = now_ms();
            lv_obj_invalidate(lv_screen_active());
            lv_refr_now(NULL);
            printf("coldbench %s %.2f ms\n", a, now_ms() - start);
        } else if (strcmp(cmd, "bench") == 0 && sscanf(line, "%*s %63s", a) == 1) {
            int n = 20;
            struct timespec t0, t1;

            sscanf(line, "%*s %*s %d", &n);
            lv_refr_now(NULL);
            s_draw_tasks = 0;
            clock_gettime(CLOCK_MONOTONIC, &t0);
            for (int i = 0; i < n; i++) {
                lv_obj_invalidate(lv_screen_active());
                lv_refr_now(NULL);
            }
            clock_gettime(CLOCK_MONOTONIC, &t1);
            printf("bench %s %.2f ms, %u draw tasks per frame\n", a,
                   ((t1.tv_sec - t0.tv_sec) * 1e3 + (t1.tv_nsec - t0.tv_nsec) / 1e6) / n, s_draw_tasks / (unsigned)n);
        } else if (strcmp(cmd, "shot") == 0 && sscanf(line, "%*s %63s", a) == 1) {
            write_png(a);
        } else {
            fprintf(stderr, "line %d: cannot parse: %s", line_no, line);
            return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    lv_display_t *disp;
    lv_indev_t *indev;
    FILE *scenario;
    int rc;

    if (argc < 3) {
        fprintf(stderr, "usage: %s <scenario.txt> <output-dir>\n", argv[0]);
        return 2;
    }
    s_out_dir = argv[2];
    scenario = fopen(argv[1], "r");
    if (scenario == NULL) {
        perror(argv[1]);
        return 2;
    }

    setenv("TZ", "UTC0", 1);
    tzset();
    s_now = make_time(14, 13, 0);
    sample_state();

    lv_init();
    lv_tick_set_cb(sim_tick_cb);
    disp = lv_display_create(SIM_W, SIM_H);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(disp, s_draw_buf, NULL, sizeof(s_draw_buf), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(disp, flush_cb);
    indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, pointer_read_cb);
    lv_indev_set_display(indev, disp);

    clock_ui_begin_boot(&s_settings, &s_runtime, &s_callbacks, NULL);
    step(100);
    clock_ui_finish_boot();
    clock_ui_tick(s_now);
    step(200);

    rc = run_scenario(scenario);
    fclose(scenario);
    return rc;
}
