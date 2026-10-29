#include "domain/brightness_policy.h"

#include "domain/alarm_scheduler.h"

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

/* The sunrise's backlight (per mille of full): from the night's level up to its brightest,
 * slowly at first (the cube of the way through), as daylight comes. */
static uint32_t sunrise_permille(const app_runtime_state_t *runtime, const app_settings_t *settings, time_t now)
{
    uint32_t p = alarm_scheduler_sunrise_progress(runtime, settings, now); /* 0..1000 */
    uint32_t from = (uint32_t)clamp_brightness(get_night_mode_brightness(runtime, settings)) * 10U;
    uint32_t to = (uint32_t)brightness_policy_ui_to_hw(settings->night_mode.sunrise_peak) * 10U;
    uint64_t eased = (uint64_t)p * p * p; /* x 10^9 */

    if (to <= from) {
        return from;
    }
    return from + (uint32_t)(((uint64_t)(to - from) * eased) / 1000000000ULL);
}

static bool sunrise_lights(const app_runtime_state_t *runtime, time_t now)
{
    return runtime->sunrise_active && runtime->sunrise_from_night && runtime->next_alarm_epoch > now;
}

uint16_t brightness_policy_get_target_permille(const app_runtime_state_t *runtime,
                                               const app_settings_t *settings,
                                               time_t now)
{
    if (!runtime->alarm_ringing && sunrise_lights(runtime, now)) {
        return (uint16_t)sunrise_permille(runtime, settings, now);
    }
    return (uint16_t)(brightness_policy_get_target(runtime, settings, now) * 10U);
}

uint8_t brightness_policy_get_target(const app_runtime_state_t *runtime,
                                     const app_settings_t *settings,
                                     time_t now)
{
    if (runtime->alarm_ringing) {
        return DISPLAY_BRIGHTNESS_MAX_PERCENT;
    }

    if (sunrise_lights(runtime, now)) {
        return (uint8_t)((sunrise_permille(runtime, settings, now) + 5U) / 10U);
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
