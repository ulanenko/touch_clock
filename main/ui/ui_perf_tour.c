#include "sdkconfig.h"

#if CONFIG_TOUCH_CLOCK_PERF_TOUR

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "clock_ui_private.h"

#if CONFIG_LV_USE_PROFILER && CONFIG_LV_USE_PROFILER_BUILTIN
#include "src/misc/lv_profiler_builtin.h"
#include "src/misc/lv_profiler_builtin_private.h"
#define PERF_PROFILE_FRAMES 3
#endif
#include "ui/ui_areas.h"

#define PERF_SEG_NAME_MAX 40
#define PERF_RENDER_SAMPLES 512
#define PERF_SLOW_FRAME_US 33000
#if CONFIG_TOUCH_CLOCK_PERF_TOUR_FACE_GALLERY
#define PERF_THUMB_STEP 1
#else
#define PERF_THUMB_STEP CONFIG_TOUCH_CLOCK_PERF_TOUR_THUMB_STEP
#endif

#if CONFIG_TOUCH_CLOCK_PERF_TOUR_STYLES
static const char *const s_style_names[CLOCK_UI_STYLE_COUNT] = {"instrument", "orbit", "atelier"};
#define PERF_STYLE_SETTLE_MS 2500
#endif

typedef enum {
    PERF_STEP_IDLE = 0,
    PERF_STEP_GESTURE,
    PERF_STEP_CALL,
    PERF_STEP_THUMB,
} perf_step_type_t;

typedef void (*perf_call_t)(clock_ui_context_t *ctx);

typedef struct {
    const char *segment;
    perf_step_type_t type;
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
    uint16_t duration_ms;
    perf_call_t call;
    uint16_t wait_ms;
} perf_step_t;

typedef struct {
    char name[PERF_SEG_NAME_MAX];
    char raw[PERF_SEG_NAME_MAX]; /* step name without the style prefix */
    uint32_t frames;
    uint32_t slow_frames;
    uint32_t white_frames;
    int64_t render_sum;
    int64_t render_max;
    int64_t pre_sum;
    int64_t pre_max;
    int64_t interval_max;
    uint32_t render_samples[PERF_RENDER_SAMPLES];
    uint32_t render_sample_count;
    size_t internal_free_min;
} perf_segment_t;

typedef struct {
    bool active;
    bool pressed;
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
    uint16_t duration_ms;
    uint32_t start_tick;
    lv_point_t last;
} perf_gesture_state_t;

static clock_ui_context_t *s_ctx;
static lv_indev_t *s_indev;
static lv_timer_t *s_timer;
static perf_gesture_state_t s_gesture;
static perf_segment_t s_seg;
static int s_step = -1;
static int s_loop;
static alarm_config_t s_saved_alarms[MAX_ALARMS];
static uint8_t s_saved_style;
static uint8_t s_saved_face;
#if CONFIG_LV_USE_PROFILER && CONFIG_LV_USE_PROFILER_BUILTIN
static int s_profile_frames_left;
#endif
static uint32_t s_hold_until;
static uint32_t s_step_start;
static bool s_step_started;
static bool s_manual;
static uint32_t s_manual_start;
static int64_t s_refr_start;
static int64_t s_render_start;
static int64_t s_last_refr_start;

#define DEBUG_CALL(fn_name, debug_name) \
    static void fn_name(clock_ui_context_t *ctx) { ui_debug_call(ctx, debug_name); }

/* Screens are opened through the areas' named debug calls (see ui_areas.h). */
DEBUG_CALL(call_open_settings_root, "open_settings")
DEBUG_CALL(call_open_wifi, "open_wifi")
DEBUG_CALL(call_open_night, "open_night")
DEBUG_CALL(call_open_other, "open_other")
DEBUG_CALL(call_close_settings, "close_settings")
DEBUG_CALL(call_open_alarms, "open_alarms")
DEBUG_CALL(call_open_alarm_editor, "new_alarm")
DEBUG_CALL(call_close_alarm_editor, "close_alarm_editor")
DEBUG_CALL(call_close_alarms, "close_alarms")
DEBUG_CALL(call_close_theme, "close_theme")
DEBUG_CALL(call_close_brightness, "close_quick")
DEBUG_CALL(call_show_controls, "show_controls")
DEBUG_CALL(call_join_dialog, "join_dialog")
DEBUG_CALL(call_night_face_picker, "night_face_picker")
DEBUG_CALL(call_style_picker, "style_picker")
DEBUG_CALL(call_settings_back, "settings_back")

#define IDLE(seg, ms) {seg, PERF_STEP_IDLE, 0, 0, 0, 0, 0, NULL, ms}
#define DRAG(seg, ax, ay, bx, by, dur, ms) {seg, PERF_STEP_GESTURE, ax, ay, bx, by, dur, NULL, ms}
#define CALL(seg, fn, ms) {seg, PERF_STEP_CALL, 0, 0, 0, 0, 0, fn, ms}
#define THUMB(name) {name, PERF_STEP_THUMB, 0, 0, 0, 0, 0, NULL, 0}

#define FLICK_UP(seg) DRAG(seg, 360, 620, 360, 220, 140, 700)
#define FLICK_DOWN(seg) DRAG(seg, 360, 220, 360, 620, 140, 700)
#define SLOW_UP(seg) DRAG(seg, 360, 620, 360, 260, 900, 500)
#define SLOW_DOWN(seg) DRAG(seg, 360, 260, 360, 620, 900, 500)
#define FLICK_BURST(seg) FLICK_UP(seg), FLICK_UP(seg), FLICK_DOWN(seg), FLICK_DOWN(seg), \
    DRAG(seg, 360, 620, 360, 220, 120, 150), DRAG(seg, 360, 220, 360, 620, 120, 150), \
    DRAG(seg, 360, 620, 360, 220, 120, 150), DRAG(seg, 360, 220, 360, 620, 120, 900)

#if CONFIG_TOUCH_CLOCK_PERF_TOUR_FACE_GALLERY
#define GALLERY_FACE_CALL(id) \
    static void call_show_face_##id(clock_ui_context_t *ctx) { request_set_current_face(ctx, (clock_face_id_t)(id)); }
GALLERY_FACE_CALL(0)
GALLERY_FACE_CALL(1)
GALLERY_FACE_CALL(2)
GALLERY_FACE_CALL(3)
GALLERY_FACE_CALL(4)
GALLERY_FACE_CALL(5)
GALLERY_FACE_CALL(6)
GALLERY_FACE_CALL(7)
#define GALLERY_FACE(id) CALL("gallery", call_show_face_##id, 2500), THUMB("face_" #id)

/* Full-resolution screenshots of every face, for design mockups. */
static const perf_step_t s_steps[] = {
    IDLE("gallery", 3000),
    GALLERY_FACE(0), GALLERY_FACE(1), GALLERY_FACE(2), GALLERY_FACE(3),
    GALLERY_FACE(4), GALLERY_FACE(5), GALLERY_FACE(6), GALLERY_FACE(7),
};
#elif CONFIG_TOUCH_CLOCK_PERF_TOUR_SHORT
/* A short tour around the night face picker, to check what stays slow after it. */
static const perf_step_t s_steps[] = {
    IDLE("idle_face", 4000),
    CALL("settings_tap", call_open_settings_root, 1200),
    CALL("settings_night_open", call_open_night, 1500),
    CALL("night_faces", call_night_face_picker, 2500),
    CALL("settings_close", call_close_settings, 1500),
    IDLE("idle_after", 4000),
    DRAG("face_tap", 360, 300, 362, 302, 60, 1500),
    DRAG("face_swipe", 600, 280, 120, 280, 220, 1200),
    DRAG("face_swipe", 120, 280, 600, 280, 220, 1200),
    CALL("alarms_open", call_open_alarms, 1200),
    FLICK_BURST("alarms_scroll"),
    CALL("alarms_close", call_close_alarms, 1200),
    IDLE("idle_face_end", 4000),
};
#else
static const perf_step_t s_steps[] = {
    IDLE("idle_face", 5000),
    THUMB("face"),
    DRAG("face_tap", 360, 300, 362, 302, 60, 1500),
    THUMB("face_controls"),
    DRAG("face_swipe", 600, 280, 120, 280, 220, 1200),
    DRAG("face_swipe", 600, 280, 120, 280, 220, 1200),
    DRAG("face_swipe", 120, 280, 600, 280, 220, 1200),
    DRAG("face_swipe", 120, 280, 600, 280, 220, 1200),
    DRAG("theme_pull", 360, 140, 360, 560, 400, 900),
    THUMB("theme_open"),
    CALL("theme_close", call_close_theme, 900),
    DRAG("theme_pull_cancel", 360, 140, 360, 300, 300, 900),
    DRAG("theme_pull", 360, 140, 360, 560, 250, 900),
    CALL("theme_close", call_close_theme, 900),
    DRAG("brightness_pull", 360, 700, 360, 260, 450, 1000),
    THUMB("brightness_open"),
    CALL("brightness_close", call_close_brightness, 900),
    DRAG("face_tap", 360, 300, 362, 302, 60, 500),
    CALL("settings_tap", call_open_settings_root, 1200),
    THUMB("settings_root"),
    CALL("settings_wifi_open", call_open_wifi, 1200),
    THUMB("settings_wifi"),
    FLICK_BURST("settings_wifi_scroll"),
    CALL("settings_night_open", call_open_night, 1200),
    THUMB("settings_night"),
    FLICK_BURST("settings_night_scroll"),
    SLOW_UP("settings_night_slow"),
    SLOW_DOWN("settings_night_slow"),
    CALL("settings_other_open", call_open_other, 1200),
    THUMB("settings_other"),
    FLICK_BURST("settings_other_scroll"),
    SLOW_UP("settings_other_slow"),
    SLOW_DOWN("settings_other_slow"),
    CALL("settings_join", call_join_dialog, 1500),
    THUMB("settings_join"),
    CALL("settings_back", call_settings_back, 800),
    CALL("settings_style", call_style_picker, 1200),
    THUMB("settings_style"),
    CALL("night_faces", call_night_face_picker, 2500),
    THUMB("night_faces"),
    CALL("settings_close", call_close_settings, 1200),
    CALL("settings_reopen", call_open_settings_root, 1200),
    CALL("settings_close", call_close_settings, 1200),
    CALL("alarms_open", call_open_alarms, 1200),
    THUMB("alarms"),
    FLICK_BURST("alarms_scroll"),
    CALL("alarm_editor_open", call_open_alarm_editor, 1500),
    THUMB("alarm_editor"),
    CALL("alarm_editor_close", call_close_alarm_editor, 1200),
    CALL("alarms_close", call_close_alarms, 1200),
    IDLE("idle_face_end", 5000),
};
#endif

#define PERF_STEP_COUNT ((int)(sizeof(s_steps) / sizeof(s_steps[0])))

#if CONFIG_LV_USE_PROFILER && CONFIG_LV_USE_PROFILER_BUILTIN
static uint64_t profile_tick(void)
{
    return (uint64_t)esp_timer_get_time();
}

static void profile_flush(const char *buf)
{
    printf("PROF %s", buf);
}

static int profile_tid(void)
{
    return (int)(((uintptr_t)xTaskGetCurrentTaskHandle() >> 4) & 0xFFFF);
}

static int profile_cpu(void)
{
    return (int)xPortGetCoreID();
}

static void profile_init(void)
{
    lv_profiler_builtin_config_t config;

    lv_profiler_builtin_uninit();
    lv_profiler_builtin_config_init(&config);
    config.buf_size = 512 * 1024;
    config.tick_per_sec = 1000000;
    config.tick_get_cb = profile_tick;
    config.flush_cb = profile_flush;
    config.tid_get_cb = profile_tid;
    config.cpu_get_cb = profile_cpu;
    lv_profiler_builtin_init(&config);
    lv_profiler_builtin_set_enable(false);
}

static bool profile_wanted(const char *raw)
{
    const char *list = CONFIG_TOUCH_CLOCK_PERF_TOUR_PROFILE_SEGMENTS;
    size_t len = strlen(raw);

    while (*list != '\0') {
        const char *end = strchr(list, ',');
        size_t item = end != NULL ? (size_t)(end - list) : strlen(list);

        if (item == len && strncmp(list, raw, len) == 0) {
            return true;
        }
        if (end == NULL) {
            break;
        }
        list = end + 1;
    }
    return false;
}

static void profile_stop(void)
{
    if (s_profile_frames_left > 0) {
        s_profile_frames_left = 0;
        lv_profiler_builtin_set_enable(false);
        printf("PROF_END\n");
        lv_profiler_builtin_flush();
    }
}
#endif

static void segment_reset(const char *name_in)
{
#if CONFIG_TOUCH_CLOCK_PERF_TOUR_STYLES
    char name[PERF_SEG_NAME_MAX];

    snprintf(name, sizeof(name), "%s.%s", s_style_names[(s_loop + CONFIG_TOUCH_CLOCK_PERF_TOUR_STYLE_FIRST) % CLOCK_UI_STYLE_COUNT], name_in);
#else
    const char *name = name_in;
#endif
    memset(&s_seg, 0, sizeof(s_seg));
    snprintf(s_seg.name, sizeof(s_seg.name), "%s", name);
    snprintf(s_seg.raw, sizeof(s_seg.raw), "%s", name_in);
#if CONFIG_LV_USE_PROFILER && CONFIG_LV_USE_PROFILER_BUILTIN
    profile_stop();
    if (profile_wanted(name_in)) {
        printf("PROF_BEGIN %s\n", name);
        s_profile_frames_left = PERF_PROFILE_FRAMES;
        lv_profiler_builtin_set_enable(true);
    }
#endif
    s_seg.internal_free_min = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    s_last_refr_start = 0;
}

static int compare_u32(const void *a, const void *b)
{
    uint32_t va = *(const uint32_t *)a;
    uint32_t vb = *(const uint32_t *)b;

    return (va > vb) - (va < vb);
}

#if CONFIG_FREERTOS_USE_TRACE_FACILITY && CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
#define PERF_MAX_TASKS 48
static TaskStatus_t s_task_prev[PERF_MAX_TASKS];
static TaskStatus_t s_task_now[PERF_MAX_TASKS];
static UBaseType_t s_task_prev_count;
static configRUN_TIME_COUNTER_TYPE s_task_prev_total;

static void report_task_cpu(const char *name)
{
    configRUN_TIME_COUNTER_TYPE total;
    UBaseType_t count = uxTaskGetSystemState(s_task_now, PERF_MAX_TASKS, &total);
    configRUN_TIME_COUNTER_TYPE elapsed = total - s_task_prev_total;

    if (s_task_prev_count > 0 && elapsed > 0) {
        for (UBaseType_t i = 0; i < count; ++i) {
            for (UBaseType_t j = 0; j < s_task_prev_count; ++j) {
                if (s_task_prev[j].xHandle != s_task_now[i].xHandle) {
                    continue;
                }
                uint32_t permille = (uint32_t)(((uint64_t)(s_task_now[i].ulRunTimeCounter - s_task_prev[j].ulRunTimeCounter) * 1000U) /
                                               elapsed);
                if (permille >= 20 && strncmp(s_task_now[i].pcTaskName, "IDLE", 4) != 0) {
                    printf("PERF cpu seg=%s task=%s prio=%u pct=%lu.%lu\n", name, s_task_now[i].pcTaskName,
                           (unsigned)s_task_now[i].uxCurrentPriority, (unsigned long)(permille / 10),
                           (unsigned long)(permille % 10));
                }
            }
        }
    }
    memcpy(s_task_prev, s_task_now, sizeof(TaskStatus_t) * count);
    s_task_prev_count = count;
    s_task_prev_total = total;
}
#else
static void report_task_cpu(const char *name)
{
    LV_UNUSED(name);
}
#endif

static void segment_report(void)
{
    uint32_t p95 = 0;

    if (s_seg.name[0] == '\0') {
        return;
    }
    if (s_seg.render_sample_count > 0) {
        qsort(s_seg.render_samples, s_seg.render_sample_count, sizeof(uint32_t), compare_u32);
        p95 = s_seg.render_samples[(s_seg.render_sample_count * 95) / 100];
    }
    report_task_cpu(s_seg.name);
    printf("PERF seg loop=%d name=%s frames=%lu render_avg=%lld render_p95=%lu render_max=%lld "
           "pre_avg=%lld pre_max=%lld interval_max=%lld slow=%lu white=%lu internal_free_min=%u\n",
           s_loop,
           s_seg.name,
           (unsigned long)s_seg.frames,
           s_seg.frames ? s_seg.render_sum / s_seg.frames : 0,
           (unsigned long)p95,
           s_seg.render_max,
           s_seg.frames ? s_seg.pre_sum / s_seg.frames : 0,
           s_seg.pre_max,
           s_seg.interval_max,
           (unsigned long)s_seg.slow_frames,
           (unsigned long)s_seg.white_frames,
           (unsigned)s_seg.internal_free_min);
    s_seg.name[0] = '\0';
}

static bool frame_looks_white(lv_display_t *disp)
{
    lv_draw_buf_t *buf = lv_display_get_buf_active(disp);
    uint32_t white = 0;
    uint32_t total = 0;

    if (buf == NULL || buf->data == NULL || buf->header.cf != LV_COLOR_FORMAT_RGB565) {
        return false;
    }
    for (uint32_t gy = 1; gy < 10; ++gy) {
        for (uint32_t gx = 1; gx < 10; ++gx) {
            uint32_t x = (buf->header.w * gx) / 10;
            uint32_t y = (buf->header.h * gy) / 10;
            const uint16_t *row = (const uint16_t *)(buf->data + (size_t)y * buf->header.stride);
            uint16_t px = row[x];

            total++;
            if ((px >> 11) >= 28 && ((px >> 5) & 0x3F) >= 56 && (px & 0x1F) >= 28) {
                white++;
            }
        }
    }
    return white * 10 >= total * 7;
}

static void display_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    int64_t now = esp_timer_get_time();

    if (code == LV_EVENT_REFR_START) {
        if (s_last_refr_start != 0 && now - s_last_refr_start > s_seg.interval_max) {
            s_seg.interval_max = now - s_last_refr_start;
        }
        s_last_refr_start = now;
        s_refr_start = now;
        s_render_start = 0;
    } else if (code == LV_EVENT_RENDER_START) {
        s_render_start = now;
    } else if (code == LV_EVENT_FLUSH_START) {
        if (s_seg.name[0] != '\0' && frame_looks_white(lv_event_get_target(event))) {
            s_seg.white_frames++;
            printf("PERF white_frame seg=%s t=%lld settings=%d alarms=%d theme=%d face=%d\n", s_seg.name, now,
                   ui_settings_is_open(s_ctx), ui_alarms_is_open(s_ctx),
                   ui_face_chrome_covers_face(s_ctx), (int)s_ctx->settings->current_face);
        }
    } else if (code == LV_EVENT_RENDER_READY) {
        size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        int64_t render;
        int64_t pre;

#if CONFIG_LV_USE_PROFILER && CONFIG_LV_USE_PROFILER_BUILTIN
        if (s_profile_frames_left > 0 && --s_profile_frames_left == 0) {
            s_profile_frames_left = 1;
            profile_stop();
        }
#endif
        if (s_render_start == 0 || s_seg.name[0] == '\0') {
            return;
        }
        render = now - s_render_start;
        pre = s_render_start - s_refr_start;
        s_seg.frames++;
        s_seg.render_sum += render;
        s_seg.pre_sum += pre;
        if (render > s_seg.render_max) {
            s_seg.render_max = render;
        }
        if (pre > s_seg.pre_max) {
            s_seg.pre_max = pre;
        }
        if (render > PERF_SLOW_FRAME_US) {
            s_seg.slow_frames++;
            if (render > 50000) {
                printf("PERF slow_frame seg=%s render=%lld pre=%lld pressed=%d step_ms=%lu scrolling=%d/%d\n",
                       s_seg.name, render, pre, s_gesture.pressed, (unsigned long)lv_tick_elaps(s_step_start),
                       s_ctx->settings_ui.night_scrolling, s_ctx->settings_ui.other_scrolling);
            }
        }
        if (s_seg.render_sample_count < PERF_RENDER_SAMPLES) {
            s_seg.render_samples[s_seg.render_sample_count++] = (uint32_t)render;
        }
        if (internal_free < s_seg.internal_free_min) {
            s_seg.internal_free_min = internal_free;
        }
    }
}

static void indev_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    LV_UNUSED(indev);

    data->point = s_gesture.last;
    data->state = LV_INDEV_STATE_RELEASED;
    if (!s_gesture.active) {
        return;
    }

    uint32_t elapsed = lv_tick_elaps(s_gesture.start_tick);
    if (elapsed <= s_gesture.duration_ms) {
        int32_t t = s_gesture.duration_ms ? (int32_t)((elapsed * 1024U) / s_gesture.duration_ms) : 1024;

        s_gesture.last.x = s_gesture.x0 + ((s_gesture.x1 - s_gesture.x0) * t) / 1024;
        s_gesture.last.y = s_gesture.y0 + ((s_gesture.y1 - s_gesture.y0) * t) / 1024;
        s_gesture.pressed = true;
        data->point = s_gesture.last;
        data->state = LV_INDEV_STATE_PRESSED;
        return;
    }

    s_gesture.last.x = s_gesture.x1;
    s_gesture.last.y = s_gesture.y1;
    data->point = s_gesture.last;
    s_gesture.active = false;
    s_gesture.pressed = false;
}

#if CONFIG_TOUCH_CLOCK_PERF_TOUR_THUMBNAILS
static void dump_thumbnail(const char *name_in)
{
#if CONFIG_TOUCH_CLOCK_PERF_TOUR_STYLES
    char name[48];

    snprintf(name, sizeof(name), "%s_%s", s_style_names[(s_loop + CONFIG_TOUCH_CLOCK_PERF_TOUR_STYLE_FIRST) % CLOCK_UI_STYLE_COUNT], name_in);
#else
    const char *name = name_in;
#endif
    lv_draw_buf_t *snap = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB565);
    uint32_t w;
    uint32_t h;
    static char line[(SCREEN_SIZE / PERF_THUMB_STEP) * 4 + 16];

    if (snap == NULL) {
        printf("PERF thumb_failed %s\n", name);
        return;
    }
    w = snap->header.w / PERF_THUMB_STEP;
    h = snap->header.h / PERF_THUMB_STEP;
    printf("PERF thumb %s %lu %lu\n", name, (unsigned long)w, (unsigned long)h);
    for (uint32_t y = 0; y < h; ++y) {
        const uint16_t *row = (const uint16_t *)(snap->data + (size_t)(y * PERF_THUMB_STEP) * snap->header.stride);
        size_t len = 0;

        for (uint32_t x = 0; x < w; ++x) {
            len += snprintf(line + len, sizeof(line) - len, "%04x", row[x * PERF_THUMB_STEP]);
        }
        printf("PERF px %s\n", line);
    }
    printf("PERF thumb_end %s\n", name);
    lv_draw_buf_destroy(snap);
}
#endif

static void start_step(const perf_step_t *step)
{
    if (step->type != PERF_STEP_THUMB && strcmp(step->segment, s_seg.raw) != 0) {
        segment_report();
        segment_reset(step->segment);
    }

    switch (step->type) {
    case PERF_STEP_GESTURE:
        s_gesture.x0 = step->x0;
        s_gesture.y0 = step->y0;
        s_gesture.x1 = step->x1;
        s_gesture.y1 = step->y1;
        s_gesture.duration_ms = step->duration_ms;
        s_gesture.last.x = step->x0;
        s_gesture.last.y = step->y0;
        s_gesture.start_tick = lv_tick_get();
        s_gesture.active = true;
        break;
    case PERF_STEP_CALL:
        if (step->call != NULL) {
            step->call(s_ctx);
        }
        break;
    case PERF_STEP_THUMB:
#if CONFIG_TOUCH_CLOCK_PERF_TOUR_THUMBNAILS
        dump_thumbnail(step->segment);
#endif
        break;
    case PERF_STEP_IDLE:
    default:
        break;
    }
}

/* The tour taps and drags through real screens, which can change real settings (an alarm
 * saved from the editor, the UI style). Put the user's alarms and style back at the end. */
static void perf_save_user_state(void)
{
    memcpy(s_saved_alarms, s_ctx->settings->alarms, sizeof(s_saved_alarms));
    s_saved_style = s_ctx->settings->ui_style;
    s_saved_face = s_ctx->settings->current_face;
    printf("PERF user_face face=%u\n", s_saved_face);
}

static void perf_restore_user_state(void)
{
    for (uint8_t i = 0; i < MAX_ALARMS; i++) {
        if (memcmp(&s_ctx->settings->alarms[i], &s_saved_alarms[i], sizeof(alarm_config_t)) != 0) {
            printf("PERF restore alarm %u\n", i);
            request_save_alarm(s_ctx, i, &s_saved_alarms[i]);
        }
    }
    if (s_ctx->settings->ui_style != s_saved_style) {
        request_set_ui_style(s_ctx, (clock_ui_style_t)s_saved_style);
    }
    if (s_ctx->settings->current_face != s_saved_face) {
        request_set_current_face(s_ctx, (clock_face_id_t)s_saved_face);
    }
    clock_ui_refresh();
}

/* With TOUCH_CLOCK_PERF_TOUR_STYLES each loop runs in the next UI style; the chrome rebuild
 * gets a moment to settle before the first step. */
static void perf_begin_loop(void)
{
    /* Every loop starts on the same face, so runs compare. */
    if (CONFIG_TOUCH_CLOCK_PERF_TOUR_START_FACE >= 0) {
        request_set_current_face(s_ctx, (clock_face_id_t)CONFIG_TOUCH_CLOCK_PERF_TOUR_START_FACE);
    }
    printf("PERF loop_face loop=%d face=%u\n", s_loop, s_ctx->settings->current_face);
#if CONFIG_TOUCH_CLOCK_PERF_TOUR_STYLES
    clock_ui_style_t style = (clock_ui_style_t)((s_loop + CONFIG_TOUCH_CLOCK_PERF_TOUR_STYLE_FIRST) % CLOCK_UI_STYLE_COUNT);

    printf("PERF style loop=%d style=%s\n", s_loop, s_style_names[style]);
    request_set_ui_style(s_ctx, style);
    clock_ui_refresh();
    s_hold_until = lv_tick_get() + PERF_STYLE_SETTLE_MS;
#endif
}

static void tour_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    if (s_manual) {
        if (lv_tick_elaps(s_manual_start) >= 10000) {
            segment_report();
            segment_reset("manual");
            s_manual_start = lv_tick_get();
        }
        return;
    }

    if (s_step < 0) {
        if (lv_tick_get() < (uint32_t)CONFIG_TOUCH_CLOCK_PERF_TOUR_START_DELAY_S * 1000U) {
            return;
        }
#if CONFIG_TOUCH_CLOCK_PERF_TOUR_MANUAL_ONLY
        printf("PERF manual_begin\n");
        s_manual = true;
        s_manual_start = lv_tick_get();
        segment_reset("manual");
        return;
#endif
        printf("PERF tour_begin loop=%d steps=%d\n", s_loop, PERF_STEP_COUNT);
        s_step = 0;
        s_step_started = false;
        perf_begin_loop();
        return;
    }
    if (lv_tick_get() < s_hold_until) {
        return;
    }

    if (s_step >= PERF_STEP_COUNT) {
        segment_report();
        printf("PERF loop_done loop=%d\n", s_loop);
        s_loop++;
        if (s_loop >= CONFIG_TOUCH_CLOCK_PERF_TOUR_LOOPS) {
            printf("PERF tour_done loops=%d\n", s_loop);
            perf_restore_user_state();
            s_manual = true;
            s_manual_start = lv_tick_get();
            segment_reset("manual");
            return;
        }
        s_step = 0;
        s_step_started = false;
        perf_begin_loop();
        return;
    }

    const perf_step_t *step = &s_steps[s_step];

    if (!s_step_started) {
        start_step(step);
        s_step_started = true;
        s_step_start = lv_tick_get();
        return;
    }

    if (s_gesture.active) {
        return;
    }
    if (lv_tick_elaps(s_step_start) < (uint32_t)step->duration_ms + step->wait_ms) {
        return;
    }
    s_step++;
    s_step_started = false;
}

void ui_perf_tour_start(clock_ui_context_t *ctx)
{
    lv_display_t *disp = lv_display_get_default();

    s_ctx = ctx;
    perf_save_user_state();
#if CONFIG_LV_USE_PROFILER && CONFIG_LV_USE_PROFILER_BUILTIN
    profile_init();
#endif
    ui_ppa_blend_benchmark();
    s_indev = lv_indev_create();
    lv_indev_set_type(s_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_indev, indev_read_cb);
    lv_indev_set_display(s_indev, disp);

    lv_display_add_event_cb(disp, display_event_cb, LV_EVENT_REFR_START, NULL);
    lv_display_add_event_cb(disp, display_event_cb, LV_EVENT_RENDER_START, NULL);
    lv_display_add_event_cb(disp, display_event_cb, LV_EVENT_FLUSH_START, NULL);
    lv_display_add_event_cb(disp, display_event_cb, LV_EVENT_RENDER_READY, NULL);

    s_timer = lv_timer_create(tour_timer_cb, 10, NULL);
    printf("PERF armed start_delay_s=%d loops=%d\n",
           CONFIG_TOUCH_CLOCK_PERF_TOUR_START_DELAY_S,
           CONFIG_TOUCH_CLOCK_PERF_TOUR_LOOPS);
}

#endif
