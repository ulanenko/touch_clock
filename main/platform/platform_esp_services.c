#include "platform/platform_esp_services.h"

#include <math.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#include "alarm_audio.h"
#include "app_settings.h"
#include "bsp/esp-bsp.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "ota_update.h"
#include "telemetry_update.h"
#include "wifi_time.h"

static time_t esp_clock_now(void)
{
    time_t now;

    time(&now);
    return now;
}

static void esp_clock_set_epoch(time_t epoch)
{
    struct timeval tv = {
        .tv_sec = epoch,
        .tv_usec = 0,
    };

    settimeofday(&tv, NULL);
}

static void esp_clock_apply_timezone(uint8_t timezone_id)
{
    app_settings_t settings;

    app_settings_set_defaults(&settings);
    settings.wifi.timezone_id = timezone_id;
    app_settings_apply_timezone(&settings);
}

static int esp_audio_init(uint8_t volume)
{
    return alarm_audio_init(volume);
}

static void esp_audio_set_volume(uint8_t volume)
{
    alarm_audio_set_volume(volume);
}

static void esp_audio_set_ascending_enabled(bool enabled)
{
    alarm_audio_set_ascending_enabled(enabled);
}

static int esp_audio_start_alarm(uint8_t volume, uint8_t sound)
{
    return alarm_audio_start_alarm(volume, sound);
}

static int esp_audio_start_test(uint8_t volume, uint8_t sound)
{
    return alarm_audio_start_test(volume, sound);
}

static int esp_audio_play_start_chime(uint8_t volume)
{
    return alarm_audio_play_start_chime(volume);
}

static int esp_audio_play_sunrise_chime(uint8_t volume)
{
    return alarm_audio_play_sunrise_chime(volume);
}

static int esp_audio_play_ui_click(uint8_t volume)
{
    return alarm_audio_play_ui_click(volume);
}

static void esp_audio_stop(void)
{
    alarm_audio_stop();
}

static bool esp_audio_is_alarm_active(void)
{
    return alarm_audio_is_alarm_active();
}

static bool esp_audio_is_test_active(void)
{
    return alarm_audio_is_test_active();
}

static void esp_audio_keep_sounds(uint8_t mask)
{
    alarm_audio_keep_sounds(mask);
}

static int esp_wifi_init_service(const app_settings_t *settings)
{
    return wifi_time_init(settings);
}

static void esp_wifi_snapshot(app_runtime_state_t *runtime)
{
    wifi_time_snapshot(runtime);
}

static int esp_wifi_start_scan(void)
{
    return wifi_time_start_scan();
}

static int esp_wifi_connect_service(const char *ssid, const char *password)
{
    return wifi_time_connect(ssid, password);
}

static int esp_wifi_forget_service(void)
{
    return wifi_time_forget();
}

static int esp_wifi_request_sync_service(void)
{
    return wifi_time_request_sync();
}

static int esp_wifi_set_auto_sync_service(bool enabled)
{
    return wifi_time_set_auto_sync(enabled);
}

/* The boot reveal: the backlight's duty stepped every 8 ms on an esp_timer, perceptually even
 * (an ease-out in perceived light, the duty that light squared and a bit). */
static esp_timer_handle_t s_fade_timer;
static int64_t s_fade_start_us;
static uint32_t s_fade_ms;
static uint32_t s_fade_duty; /* the duty at the end, as bsp_display_brightness_set makes it */

static void fade_set_duty(uint32_t duty)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, CONFIG_BSP_DISPLAY_BRIGHTNESS_LEDC_CH, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, CONFIG_BSP_DISPLAY_BRIGHTNESS_LEDC_CH);
}

static void fade_timer_cb(void *arg)
{
    float t = (float)(esp_timer_get_time() - s_fade_start_us) / (1000.0f * (float)s_fade_ms);
    float u;
    float light;

    (void)arg;
    if (t >= 1.0f) {
        esp_timer_stop(s_fade_timer);
        fade_set_duty(s_fade_duty);
        return;
    }
    u = 1.0f - t;
    light = 1.0f - u * u * u;
    fade_set_duty((uint32_t)((float)s_fade_duty * powf(light, 2.2f) + 0.5f));
}

static void esp_display_fade_in(uint8_t hw_percent, uint32_t ms)
{
    if (s_fade_timer == NULL) {
        const esp_timer_create_args_t args = {.callback = fade_timer_cb, .name = "bl_fade"};

        if (esp_timer_create(&args, &s_fade_timer) != ESP_OK) {
            bsp_display_brightness_set(hw_percent);
            return;
        }
    }
    esp_timer_stop(s_fade_timer);
    s_fade_duty = (1023u * (hw_percent > 100 ? 100u : hw_percent)) / 100u;
    if (ms == 0) {
        fade_set_duty(0);
        return;
    }
    s_fade_ms = ms;
    s_fade_start_us = esp_timer_get_time();
    esp_timer_start_periodic(s_fade_timer, 8000);
}

static int esp_display_set_level_permille(uint16_t permille)
{
    if (s_fade_timer != NULL) {
        esp_timer_stop(s_fade_timer);
    }
    fade_set_duty((1023u * (permille > 1000 ? 1000u : permille)) / 1000u);
    return 0;
}

static int esp_display_set_brightness(uint8_t hw_percent)
{
    if (s_fade_timer != NULL) {
        esp_timer_stop(s_fade_timer);
    }
    return bsp_display_brightness_set(hw_percent);
}

static int esp_ota_init_service(void)
{
    return ota_update_init();
}

static void esp_ota_snapshot(app_runtime_state_t *runtime)
{
    ota_update_snapshot(runtime);
}

static int esp_ota_request_check(void)
{
    return ota_update_request_check();
}

static int esp_ota_request_install(void)
{
    return ota_update_request_install();
}

static int esp_telemetry_init_service(void)
{
    return telemetry_update_init();
}

static void esp_telemetry_tick_service(const app_runtime_state_t *runtime,
                                       const app_settings_t *settings,
                                       time_t now)
{
    telemetry_update_tick(runtime, settings, now);
}

static int esp_settings_load(app_settings_t *settings)
{
    return app_settings_load(settings);
}

static int esp_settings_save(const app_settings_t *settings)
{
    return app_settings_save(settings);
}

static const clock_time_service_t s_clock_time_service = {
    .now = esp_clock_now,
    .set_epoch = esp_clock_set_epoch,
    .apply_timezone = esp_clock_apply_timezone,
};

static const audio_service_t s_audio_service = {
    .init = esp_audio_init,
    .set_volume = esp_audio_set_volume,
    .set_ascending_enabled = esp_audio_set_ascending_enabled,
    .start_alarm = esp_audio_start_alarm,
    .start_test = esp_audio_start_test,
    .play_ui_click = esp_audio_play_ui_click,
    .play_start_chime = esp_audio_play_start_chime,
    .play_sunrise_chime = esp_audio_play_sunrise_chime,
    .stop = esp_audio_stop,
    .is_alarm_active = esp_audio_is_alarm_active,
    .is_test_active = esp_audio_is_test_active,
    .keep_sounds = esp_audio_keep_sounds,
};

static const wifi_service_t s_wifi_service = {
    .init = esp_wifi_init_service,
    .snapshot = esp_wifi_snapshot,
    .start_scan = esp_wifi_start_scan,
    .connect = esp_wifi_connect_service,
    .forget = esp_wifi_forget_service,
    .request_sync = esp_wifi_request_sync_service,
    .set_auto_sync = esp_wifi_set_auto_sync_service,
};

static const display_service_t s_display_service = {
    .set_brightness = esp_display_set_brightness,
    .fade_in = esp_display_fade_in,
    .set_level_permille = esp_display_set_level_permille,
};

static const ota_service_t s_ota_service = {
    .init = esp_ota_init_service,
    .snapshot = esp_ota_snapshot,
    .request_check = esp_ota_request_check,
    .request_install = esp_ota_request_install,
};

static const telemetry_service_t s_telemetry_service = {
    .init = esp_telemetry_init_service,
    .tick = esp_telemetry_tick_service,
};

static const settings_store_t s_settings_store = {
    .load = esp_settings_load,
    .save = esp_settings_save,
};

const clock_time_service_t *platform_esp_clock_time_service(void)
{
    return &s_clock_time_service;
}

const audio_service_t *platform_esp_audio_service(void)
{
    return &s_audio_service;
}

const wifi_service_t *platform_esp_wifi_service(void)
{
    return &s_wifi_service;
}

const display_service_t *platform_esp_display_service(void)
{
    return &s_display_service;
}

const ota_service_t *platform_esp_ota_service(void)
{
    return &s_ota_service;
}

const telemetry_service_t *platform_esp_telemetry_service(void)
{
    return &s_telemetry_service;
}

const settings_store_t *platform_esp_settings_store(void)
{
    return &s_settings_store;
}
