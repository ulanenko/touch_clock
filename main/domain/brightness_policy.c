#include "domain/brightness_policy.h"

static int clamp_brightness(int brightness)
{
    if (brightness < DISPLAY_BRIGHTNESS_MIN_PERCENT) {
        return DISPLAY_BRIGHTNESS_MIN_PERCENT;
    }
    if (brightness > DISPLAY_BRIGHTNESS_MAX_PERCENT) {
        return DISPLAY_BRIGHTNESS_MAX_PERCENT;
    }
    return brightness;
}

static int clamp_brightness_ui(int brightness)
{
    if (brightness < 0) {
        return 0;
    }
    if (brightness > 100) {
        return 100;
    }
    return brightness;
}

static uint8_t get_night_mode_brightness(const app_runtime_state_t *runtime,
                                         const app_settings_t *settings)
{
    if (runtime->night_brightness_override_active) {
        return runtime->night_brightness_override;
    }

    return settings->night_mode.brightness;
}

uint8_t brightness_policy_get_target(const app_runtime_state_t *runtime,
                                     const app_settings_t *settings,
                                     time_t now)
{
    if (runtime->alarm_ringing) {
        return DISPLAY_BRIGHTNESS_MAX_PERCENT;
    }

    if (runtime->sunrise_active && runtime->next_alarm_epoch > now) {
        int64_t seconds_left = (int64_t)(runtime->next_alarm_epoch - now);
        int64_t progress = 1800 - seconds_left;
        int minimum = settings->night_mode.enabled
                          ? get_night_mode_brightness(runtime, settings)
                          : DISPLAY_BRIGHTNESS_MIN_PERCENT;
        int span;

        if (minimum < DISPLAY_BRIGHTNESS_MIN_PERCENT) {
            minimum = DISPLAY_BRIGHTNESS_MIN_PERCENT;
        }
        if (minimum > settings->base_brightness) {
            minimum = settings->base_brightness;
        }
        if (progress < 0) {
            progress = 0;
        }
        if (progress > 1800) {
            progress = 1800;
        }

        span = (int)settings->base_brightness - minimum;
        return (uint8_t)(minimum + ((progress * span) / 1800));
    }

    if (runtime->in_night_mode) {
        return get_night_mode_brightness(runtime, settings);
    }

    return settings->base_brightness;
}

uint8_t brightness_policy_ui_to_hw(uint8_t ui_percent)
{
    int clamped = clamp_brightness_ui(ui_percent);
    int span = DISPLAY_BRIGHTNESS_MAX_PERCENT - DISPLAY_BRIGHTNESS_MIN_PERCENT;
    int hw = DISPLAY_BRIGHTNESS_MIN_PERCENT + ((clamped * span + 50) / 100);

    return (uint8_t)clamp_brightness(hw);
}

uint8_t brightness_policy_hw_to_ui(uint8_t hw_percent)
{
    int clamped = clamp_brightness(hw_percent);
    int span = DISPLAY_BRIGHTNESS_MAX_PERCENT - DISPLAY_BRIGHTNESS_MIN_PERCENT;

    if (span <= 0) {
        return 100;
    }

    return (uint8_t)clamp_brightness_ui(((clamped - DISPLAY_BRIGHTNESS_MIN_PERCENT) * 100 + (span / 2)) / span);
}
