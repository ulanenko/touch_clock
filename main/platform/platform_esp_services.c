#include "platform/platform_esp_services.h"

#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#include "alarm_audio.h"
#include "app_settings.h"
#include "bsp/esp-bsp.h"
#include "esp_err.h"
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

static void esp_clock_apply_timezone(int8_t utc_offset_hours)
{
    app_settings_t settings;

    app_settings_set_defaults(&settings);
    settings.wifi.timezone_offset_hours = utc_offset_hours;
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

static int esp_audio_start_alarm(uint8_t volume)
{
    return alarm_audio_start_alarm(volume);
}

static int esp_audio_start_test(uint8_t volume)
{
    return alarm_audio_start_test(volume);
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

static int esp_display_set_brightness(uint8_t hw_percent)
{
    return bsp_display_brightness_set(hw_percent);
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
    .stop = esp_audio_stop,
    .is_alarm_active = esp_audio_is_alarm_active,
    .is_test_active = esp_audio_is_test_active,
};

static const wifi_service_t s_wifi_service = {
    .init = esp_wifi_init_service,
    .snapshot = esp_wifi_snapshot,
    .start_scan = esp_wifi_start_scan,
    .connect = esp_wifi_connect_service,
    .forget = esp_wifi_forget_service,
    .request_sync = esp_wifi_request_sync_service,
};

static const display_service_t s_display_service = {
    .set_brightness = esp_display_set_brightness,
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

const settings_store_t *platform_esp_settings_store(void)
{
    return &s_settings_store;
}
