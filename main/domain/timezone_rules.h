#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CLOCK_TIMEZONE_FIXED_OFFSET_MIN (-12)
#define CLOCK_TIMEZONE_FIXED_OFFSET_MAX 14
#define CLOCK_TIMEZONE_FIXED_COUNT \
    (CLOCK_TIMEZONE_FIXED_OFFSET_MAX - CLOCK_TIMEZONE_FIXED_OFFSET_MIN + 1)
#define CLOCK_TIMEZONE_ID_UTC 12U
#define CLOCK_TIMEZONE_ID_EUROPE_CENTRAL 27U
#define CLOCK_TIMEZONE_ID_EUROPE_LONDON 28U
#define CLOCK_TIMEZONE_ID_EUROPE_KYIV 29U
#define CLOCK_TIMEZONE_ID_EUROPE_ISTANBUL 30U
#define CLOCK_TIMEZONE_ID_AFRICA_JOHANNESBURG 31U
#define CLOCK_TIMEZONE_ID_ASIA_DUBAI 32U
#define CLOCK_TIMEZONE_ID_ASIA_KOLKATA 33U
#define CLOCK_TIMEZONE_ID_ASIA_BANGKOK 34U
#define CLOCK_TIMEZONE_ID_ASIA_SHANGHAI 35U
#define CLOCK_TIMEZONE_ID_ASIA_TOKYO 36U
#define CLOCK_TIMEZONE_ID_AUSTRALIA_SYDNEY 37U
#define CLOCK_TIMEZONE_ID_PACIFIC_AUCKLAND 38U
#define CLOCK_TIMEZONE_ID_AMERICA_NEW_YORK 39U
#define CLOCK_TIMEZONE_ID_AMERICA_CHICAGO 40U
#define CLOCK_TIMEZONE_ID_AMERICA_DENVER 41U
#define CLOCK_TIMEZONE_ID_AMERICA_PHOENIX 42U
#define CLOCK_TIMEZONE_ID_AMERICA_LOS_ANGELES 43U
#define CLOCK_TIMEZONE_ID_AMERICA_MEXICO_CITY 44U
#define CLOCK_TIMEZONE_ID_AMERICA_SAO_PAULO 45U
#define CLOCK_TIMEZONE_ID_UTC_CITY 46U
#define CLOCK_TIMEZONE_COUNT 47U

bool timezone_id_is_valid(uint8_t timezone_id);
uint8_t timezone_id_from_legacy_offset(int8_t utc_offset_hours);
int8_t timezone_legacy_offset_hours(uint8_t timezone_id);
uint8_t timezone_count(void);
uint8_t timezone_picker_count(void);
uint8_t timezone_id_from_picker_index(uint8_t picker_index);
uint8_t timezone_picker_index_from_id(uint8_t timezone_id);
bool timezone_format_posix(char *buffer, size_t size, uint8_t timezone_id);
bool timezone_format_label(char *buffer, size_t size, uint8_t timezone_id);
bool timezone_format_picker_label(char *buffer, size_t size, uint8_t picker_index);
