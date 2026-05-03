#include <time.h>

#include "domain/timezone_rules.h"
#include "test_support.h"

static int local_hour_for(const char *tz, time_t epoch)
{
    struct tm value;

    setenv("TZ", tz, 1);
    tzset();
    localtime_r(&epoch, &value);
    return value.tm_hour;
}

static int local_isdst_for(const char *tz, time_t epoch)
{
    struct tm value;

    setenv("TZ", tz, 1);
    tzset();
    localtime_r(&epoch, &value);
    return value.tm_isdst > 0 ? 1 : 0;
}

static int test_fixed_offset_timezone(void)
{
    char tz[48];
    char label[24];
    time_t winter = make_utc_time(2026, 1, 15, 12, 0, 0);
    time_t summer = make_utc_time(2026, 7, 15, 12, 0, 0);

    EXPECT_TRUE(timezone_format_posix(tz, sizeof(tz), timezone_id_from_legacy_offset(2)));
    EXPECT_STR_EQ("UTC-2", tz);
    EXPECT_TRUE(timezone_format_label(label, sizeof(label), timezone_id_from_legacy_offset(2)));
    EXPECT_STR_EQ("UTC+2", label);
    EXPECT_EQ_INT(14, local_hour_for(tz, winter));
    EXPECT_EQ_INT(14, local_hour_for(tz, summer));
    return 0;
}

static int test_central_europe_uses_summer_time(void)
{
    char tz[48];
    char label[24];
    time_t winter = make_utc_time(2026, 1, 15, 12, 0, 0);
    time_t summer = make_utc_time(2026, 7, 15, 12, 0, 0);

    EXPECT_TRUE(timezone_format_posix(tz, sizeof(tz), CLOCK_TIMEZONE_ID_EUROPE_CENTRAL));
    EXPECT_STR_EQ("CET-1CEST,M3.5.0/2,M10.5.0/3", tz);
    EXPECT_TRUE(timezone_format_label(label, sizeof(label), CLOCK_TIMEZONE_ID_EUROPE_CENTRAL));
    EXPECT_STR_EQ("Warsaw / Berlin", label);
    EXPECT_EQ_INT(13, local_hour_for(tz, winter));
    EXPECT_EQ_INT(14, local_hour_for(tz, summer));
    return 0;
}

static int test_city_picker_order(void)
{
    char label[32];

    EXPECT_EQ_INT(CLOCK_TIMEZONE_ID_EUROPE_CENTRAL, timezone_id_from_picker_index(0));
    EXPECT_TRUE(timezone_format_picker_label(label, sizeof(label), 0));
    EXPECT_STR_EQ("Warsaw / Berlin (UTC+1/+2)", label);
    EXPECT_EQ_INT(CLOCK_TIMEZONE_ID_AMERICA_NEW_YORK, timezone_id_from_picker_index(12));
    EXPECT_TRUE(timezone_format_picker_label(label, sizeof(label), 12));
    EXPECT_STR_EQ("New York (UTC-5/-4)", label);
    EXPECT_EQ_INT(CLOCK_TIMEZONE_ID_UTC_CITY, timezone_id_from_picker_index(19));
    EXPECT_TRUE(timezone_format_picker_label(label, sizeof(label), 19));
    EXPECT_STR_EQ("UTC (UTC+0)", label);
    EXPECT_EQ_INT(20, timezone_picker_count());
    EXPECT_EQ_INT(0, timezone_picker_index_from_id(CLOCK_TIMEZONE_ID_EUROPE_CENTRAL));
    EXPECT_EQ_INT(12, timezone_picker_index_from_id(CLOCK_TIMEZONE_ID_AMERICA_NEW_YORK));
    EXPECT_EQ_INT(19, timezone_picker_index_from_id(CLOCK_TIMEZONE_ID_UTC_CITY));
    return 0;
}

static int test_posix_rules_match_representative_iana_zones(void)
{
    static const struct {
        uint8_t timezone_id;
        const char *iana_name;
    } cases[] = {
        {CLOCK_TIMEZONE_ID_EUROPE_CENTRAL, "Europe/Warsaw"},
        {CLOCK_TIMEZONE_ID_EUROPE_LONDON, "Europe/London"},
        {CLOCK_TIMEZONE_ID_AMERICA_NEW_YORK, "America/New_York"},
        {CLOCK_TIMEZONE_ID_AMERICA_LOS_ANGELES, "America/Los_Angeles"},
        {CLOCK_TIMEZONE_ID_ASIA_TOKYO, "Asia/Tokyo"},
        {CLOCK_TIMEZONE_ID_AUSTRALIA_SYDNEY, "Australia/Sydney"},
    };
    char posix[48];
    time_t winter = make_utc_time(2026, 1, 15, 12, 0, 0);
    time_t summer = make_utc_time(2026, 7, 15, 12, 0, 0);

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        EXPECT_TRUE(timezone_format_posix(posix, sizeof(posix), cases[i].timezone_id));
        EXPECT_EQ_INT(local_hour_for(cases[i].iana_name, winter), local_hour_for(posix, winter));
        EXPECT_EQ_INT(local_hour_for(cases[i].iana_name, summer), local_hour_for(posix, summer));
        EXPECT_EQ_INT(local_isdst_for(cases[i].iana_name, winter), local_isdst_for(posix, winter));
        EXPECT_EQ_INT(local_isdst_for(cases[i].iana_name, summer), local_isdst_for(posix, summer));
    }
    return 0;
}

int main(void)
{
    int status;

    test_use_utc();

    status = test_fixed_offset_timezone();
    if (status != 0) {
        return status;
    }

    status = test_central_europe_uses_summer_time();
    if (status != 0) {
        return status;
    }

    status = test_city_picker_order();
    if (status != 0) {
        return status;
    }

    return test_posix_rules_match_representative_iana_zones();
}
