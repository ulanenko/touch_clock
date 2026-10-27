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
 *                                  click_sound, alarm_test, sunrise_active
 *   shot <name>                    write <output-dir>/<name>.png
 *   clock <HH:MM:SS>               set the wall clock (date stays Tue 27 Oct 2026)
 */
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zlib.h>

#include "clock_ui.h"
#include "domain/settings_policy.h"
#include "domain/timezone_rules.h"
#include "lvgl.h"
#include "ui/clock_ui_private.h"
#include "ui/ui_areas.h"

#undef time

#define SIM_W 720
#define SIM_H 720
#define SIM_STEP_MS 5

static uint16_t s_frame[SIM_W * SIM_H];
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
static void cb_night_enabled(void *u, bool e) { (void)u; s_settings.night_mode.enabled = e; mark_refresh(); }
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
    .on_set_night_mode_enabled = cb_night_enabled,
    .on_set_night_schedule = cb_night_schedule,
    .on_set_night_brightness = cb_night_brightness,
    .on_set_night_sunrise_brightness_enabled = cb_sunrise,
    .on_set_alarm_volume = cb_alarm_volume,
    .on_set_ascending_alarm_enabled = cb_ascending,
    .on_set_snooze_minutes = cb_snooze,
    .on_set_alarm_enabled = cb_alarm_enabled,
    .on_save_alarm = cb_save_alarm,
    .on_delete_alarm = cb_delete_alarm,
    .on_alarm_snooze_requested = cb_snooze_req,
    .on_alarm_stop_requested = cb_stop_req,
    .on_alarm_test_requested = cb_noop,
    .on_alarm_test_stop_requested = cb_noop,
    .on_next_alarm_cancel_requested = cb_noop,
    .on_next_alarm_cancel_undo_requested = cb_noop,
};

static time_t make_time(int hour, int minute, int second)
{
    struct tm tm = {0};

    tm.tm_year = 2026 - 1900;
    tm.tm_mon = 9;
    tm.tm_mday = 27;
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
    } else if (strcmp(key, "time_sync_manual") == 0) {
        s_settings.wifi.time_sync_mode = v != 0 ? TIME_SYNC_MODE_MANUAL : TIME_SYNC_MODE_AUTO;
    } else {
        return false;
    }
    clock_ui_refresh();
    return true;
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
        } else if (strcmp(cmd, "clock") == 0 && sscanf(line, "%*s %d:%d:%d", &x0, &y0, &x1) == 3) {
            s_now = make_time(x0, y0, x1);
            clock_ui_tick(s_now);
            step(SIM_STEP_MS);
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
