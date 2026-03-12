#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define EXPECT_TRUE(expr)                                                                          \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            fprintf(stderr, "%s:%d: expected true: %s\n", __FILE__, __LINE__, #expr);             \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

#define EXPECT_FALSE(expr) EXPECT_TRUE(!(expr))

#define EXPECT_EQ_INT(expected, actual)                                                            \
    do {                                                                                           \
        long long expected_value__ = (long long)(expected);                                        \
        long long actual_value__ = (long long)(actual);                                            \
        if (expected_value__ != actual_value__) {                                                  \
            fprintf(stderr,                                                                        \
                    "%s:%d: expected %s == %s (%lld != %lld)\n",                                   \
                    __FILE__,                                                                       \
                    __LINE__,                                                                       \
                    #expected,                                                                      \
                    #actual,                                                                        \
                    expected_value__,                                                               \
                    actual_value__);                                                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

#define EXPECT_STR_EQ(expected, actual)                                                            \
    do {                                                                                           \
        const char *expected_value__ = (expected);                                                 \
        const char *actual_value__ = (actual);                                                     \
        if (strcmp(expected_value__, actual_value__) != 0) {                                       \
            fprintf(stderr,                                                                        \
                    "%s:%d: expected strings to match (%s != %s)\n",                               \
                    __FILE__,                                                                       \
                    __LINE__,                                                                       \
                    expected_value__,                                                               \
                    actual_value__);                                                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static inline void test_use_utc(void)
{
    setenv("TZ", "UTC", 1);
    tzset();
}

static inline time_t make_utc_time(int year, int month, int day, int hour, int minute, int second)
{
    struct tm value = {0};

    test_use_utc();
    value.tm_year = year - 1900;
    value.tm_mon = month - 1;
    value.tm_mday = day;
    value.tm_hour = hour;
    value.tm_min = minute;
    value.tm_sec = second;
    value.tm_isdst = 0;
    return mktime(&value);
}
