#include "domain/timezone_rules.h"

#include <stdio.h>

typedef struct {
    const char *label;
    const char *picker_label;
    const char *posix;
    int8_t standard_offset_hours;
} timezone_region_rule_t;

static const timezone_region_rule_t s_region_rules[] = {
    [CLOCK_TIMEZONE_ID_EUROPE_CENTRAL - CLOCK_TIMEZONE_FIXED_COUNT] = {
        .label = "Warsaw / Berlin",
        .picker_label = "Warsaw / Berlin (UTC+1/+2)",
        .posix = "CET-1CEST,M3.5.0/2,M10.5.0/3",
        .standard_offset_hours = 1,
    },
    {
        .label = "London",
        .picker_label = "London (UTC+0/+1)",
        .posix = "GMT0BST,M3.5.0/1,M10.5.0/2",
        .standard_offset_hours = 0,
    },
    {
        .label = "Kyiv / Athens",
        .picker_label = "Kyiv / Athens (UTC+2/+3)",
        .posix = "EET-2EEST,M3.5.0/3,M10.5.0/4",
        .standard_offset_hours = 2,
    },
    {
        .label = "Istanbul",
        .picker_label = "Istanbul (UTC+3)",
        .posix = "TRT-3",
        .standard_offset_hours = 3,
    },
    {
        .label = "Johannesburg",
        .picker_label = "Johannesburg (UTC+2)",
        .posix = "SAST-2",
        .standard_offset_hours = 2,
    },
    {
        .label = "Dubai",
        .picker_label = "Dubai (UTC+4)",
        .posix = "GST-4",
        .standard_offset_hours = 4,
    },
    {
        .label = "Mumbai / Kolkata",
        .picker_label = "Mumbai / Kolkata (UTC+5:30)",
        .posix = "IST-5:30",
        .standard_offset_hours = 5,
    },
    {
        .label = "Bangkok",
        .picker_label = "Bangkok (UTC+7)",
        .posix = "ICT-7",
        .standard_offset_hours = 7,
    },
    {
        .label = "Singapore / Beijing",
        .picker_label = "Singapore / Beijing (UTC+8)",
        .posix = "CST-8",
        .standard_offset_hours = 8,
    },
    {
        .label = "Tokyo",
        .picker_label = "Tokyo (UTC+9)",
        .posix = "JST-9",
        .standard_offset_hours = 9,
    },
    {
        .label = "Sydney",
        .picker_label = "Sydney (UTC+10/+11)",
        .posix = "AEST-10AEDT,M10.1.0/2,M4.1.0/3",
        .standard_offset_hours = 10,
    },
    {
        .label = "Auckland",
        .picker_label = "Auckland (UTC+12/+13)",
        .posix = "NZST-12NZDT,M9.5.0/2,M4.1.0/3",
        .standard_offset_hours = 12,
    },
    {
        .label = "New York",
        .picker_label = "New York (UTC-5/-4)",
        .posix = "EST5EDT,M3.2.0/2,M11.1.0/2",
        .standard_offset_hours = -5,
    },
    {
        .label = "Chicago",
        .picker_label = "Chicago (UTC-6/-5)",
        .posix = "CST6CDT,M3.2.0/2,M11.1.0/2",
        .standard_offset_hours = -6,
    },
    {
        .label = "Denver",
        .picker_label = "Denver (UTC-7/-6)",
        .posix = "MST7MDT,M3.2.0/2,M11.1.0/2",
        .standard_offset_hours = -7,
    },
    {
        .label = "Phoenix",
        .picker_label = "Phoenix (UTC-7)",
        .posix = "MST7",
        .standard_offset_hours = -7,
    },
    {
        .label = "Los Angeles",
        .picker_label = "Los Angeles (UTC-8/-7)",
        .posix = "PST8PDT,M3.2.0/2,M11.1.0/2",
        .standard_offset_hours = -8,
    },
    {
        .label = "Mexico City",
        .picker_label = "Mexico City (UTC-6)",
        .posix = "CST6",
        .standard_offset_hours = -6,
    },
    {
        .label = "Sao Paulo",
        .picker_label = "Sao Paulo (UTC-3)",
        .posix = "BRT3",
        .standard_offset_hours = -3,
    },
    {
        .label = "UTC",
        .picker_label = "UTC (UTC+0)",
        .posix = "UTC+0",
        .standard_offset_hours = 0,
    },
};

static const uint8_t s_picker_ids[] = {
    CLOCK_TIMEZONE_ID_EUROPE_CENTRAL,
    CLOCK_TIMEZONE_ID_EUROPE_LONDON,
    CLOCK_TIMEZONE_ID_EUROPE_KYIV,
    CLOCK_TIMEZONE_ID_EUROPE_ISTANBUL,
    CLOCK_TIMEZONE_ID_AFRICA_JOHANNESBURG,
    CLOCK_TIMEZONE_ID_ASIA_DUBAI,
    CLOCK_TIMEZONE_ID_ASIA_KOLKATA,
    CLOCK_TIMEZONE_ID_ASIA_BANGKOK,
    CLOCK_TIMEZONE_ID_ASIA_SHANGHAI,
    CLOCK_TIMEZONE_ID_ASIA_TOKYO,
    CLOCK_TIMEZONE_ID_AUSTRALIA_SYDNEY,
    CLOCK_TIMEZONE_ID_PACIFIC_AUCKLAND,
    CLOCK_TIMEZONE_ID_AMERICA_NEW_YORK,
    CLOCK_TIMEZONE_ID_AMERICA_CHICAGO,
    CLOCK_TIMEZONE_ID_AMERICA_DENVER,
    CLOCK_TIMEZONE_ID_AMERICA_PHOENIX,
    CLOCK_TIMEZONE_ID_AMERICA_LOS_ANGELES,
    CLOCK_TIMEZONE_ID_AMERICA_MEXICO_CITY,
    CLOCK_TIMEZONE_ID_AMERICA_SAO_PAULO,
    CLOCK_TIMEZONE_ID_UTC_CITY,
};

static bool timezone_is_fixed(uint8_t timezone_id)
{
    return timezone_id < CLOCK_TIMEZONE_FIXED_COUNT;
}

static int8_t fixed_offset_for_id(uint8_t timezone_id)
{
    return (int8_t)((int)timezone_id + CLOCK_TIMEZONE_FIXED_OFFSET_MIN);
}

bool timezone_id_is_valid(uint8_t timezone_id)
{
    return timezone_id < timezone_count();
}

uint8_t timezone_id_from_legacy_offset(int8_t utc_offset_hours)
{
    if (utc_offset_hours == 1) {
        return CLOCK_TIMEZONE_ID_EUROPE_CENTRAL;
    }
    if (utc_offset_hours == 0) {
        return CLOCK_TIMEZONE_ID_UTC_CITY;
    }
    if (utc_offset_hours < CLOCK_TIMEZONE_FIXED_OFFSET_MIN ||
        utc_offset_hours > CLOCK_TIMEZONE_FIXED_OFFSET_MAX) {
        return CLOCK_TIMEZONE_ID_UTC;
    }

    return (uint8_t)(utc_offset_hours - CLOCK_TIMEZONE_FIXED_OFFSET_MIN);
}

int8_t timezone_legacy_offset_hours(uint8_t timezone_id)
{
    if (timezone_is_fixed(timezone_id)) {
        return fixed_offset_for_id(timezone_id);
    }
    if (timezone_id_is_valid(timezone_id)) {
        return s_region_rules[timezone_id - CLOCK_TIMEZONE_FIXED_COUNT].standard_offset_hours;
    }
    return 0;
}

uint8_t timezone_count(void)
{
    return (uint8_t)(CLOCK_TIMEZONE_FIXED_COUNT +
                     (sizeof(s_region_rules) / sizeof(s_region_rules[0])));
}

uint8_t timezone_picker_count(void)
{
    return (uint8_t)(sizeof(s_picker_ids) / sizeof(s_picker_ids[0]));
}

uint8_t timezone_id_from_picker_index(uint8_t picker_index)
{
    if (picker_index >= timezone_picker_count()) {
        return CLOCK_TIMEZONE_ID_UTC_CITY;
    }

    return s_picker_ids[picker_index];
}

uint8_t timezone_picker_index_from_id(uint8_t timezone_id)
{
    for (uint8_t i = 0; i < timezone_picker_count(); ++i) {
        if (s_picker_ids[i] == timezone_id) {
            return i;
        }
    }

    return timezone_picker_index_from_id(CLOCK_TIMEZONE_ID_UTC_CITY);
}

bool timezone_format_posix(char *buffer, size_t size, uint8_t timezone_id)
{
    int written;

    if (buffer == NULL || size == 0) {
        return false;
    }

    if (!timezone_id_is_valid(timezone_id)) {
        timezone_id = CLOCK_TIMEZONE_ID_UTC_CITY;
    }
    if (timezone_is_fixed(timezone_id)) {
        written = snprintf(buffer, size, "UTC%+d", -fixed_offset_for_id(timezone_id));
        return written >= 0 && (size_t)written < size;
    }

    written = snprintf(buffer, size, "%s", s_region_rules[timezone_id - CLOCK_TIMEZONE_FIXED_COUNT].posix);
    return written >= 0 && (size_t)written < size;
}

bool timezone_format_label(char *buffer, size_t size, uint8_t timezone_id)
{
    int written;

    if (buffer == NULL || size == 0) {
        return false;
    }

    if (!timezone_id_is_valid(timezone_id)) {
        timezone_id = CLOCK_TIMEZONE_ID_UTC_CITY;
    }
    if (timezone_is_fixed(timezone_id)) {
        written = snprintf(buffer, size, "UTC%+d", fixed_offset_for_id(timezone_id));
        return written >= 0 && (size_t)written < size;
    }

    written = snprintf(buffer, size, "%s", s_region_rules[timezone_id - CLOCK_TIMEZONE_FIXED_COUNT].label);
    return written >= 0 && (size_t)written < size;
}

bool timezone_format_picker_label(char *buffer, size_t size, uint8_t picker_index)
{
    uint8_t timezone_id = timezone_id_from_picker_index(picker_index);
    int written;

    if (buffer == NULL || size == 0) {
        return false;
    }
    if (!timezone_id_is_valid(timezone_id)) {
        timezone_id = CLOCK_TIMEZONE_ID_UTC_CITY;
    }
    if (timezone_is_fixed(timezone_id)) {
        return timezone_format_label(buffer, size, timezone_id);
    }

    written = snprintf(buffer, size, "%s", s_region_rules[timezone_id - CLOCK_TIMEZONE_FIXED_COUNT].picker_label);
    return written >= 0 && (size_t)written < size;
}
