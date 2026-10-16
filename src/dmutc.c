#define DMOD_ENABLE_REGISTRATION ON
#include "dmod.h"
#include "dmutc.h"
#include <errno.h>
#include <string.h>

static int64_t year_days(int year)
{
    int64_t y = year - 1;
    return 365 * y + y / 4 - y / 100 + y / 400;
}
static int leap(int year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}
static unsigned month_days(int year, unsigned month)
{
    static const uint8_t days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    return days[month - 1] + (month == 2 && leap(year));
}

dmod_dmutc_api_declaration(1.0, int, _from_calendar,
    (const dmutc_calendar_t* c, dmutc_time_t* out))
{
    if (out == NULL) return -EINVAL;
    memset(out, 0, sizeof(*out));
    if (c == NULL || c->reserved != 0 || c->year < 1 || c->year > 9999 ||
        c->month < 1 || c->month > 12 || c->day < 1 ||
        c->day > month_days(c->year, c->month) || c->hour > 23 ||
        c->minute > 59 || c->second > 59 || c->nanosecond >= 1000000000u) return -EINVAL;
    int64_t days = year_days(c->year) - 719162;
    for (unsigned m = 1; m < c->month; ++m) days += month_days(c->year, m);
    days += c->day - 1;
    out->seconds = days * 86400 + c->hour * 3600 + c->minute * 60 + c->second;
    out->nanosecond = c->nanosecond;
    return 0;
}

dmod_dmutc_api_declaration(1.0, int, _to_calendar,
    (const dmutc_time_t* time, dmutc_calendar_t* out))
{
    if (out == NULL) return -EINVAL;
    memset(out, 0, sizeof(*out));
    if (time == NULL || time->reserved != 0 || time->nanosecond >= 1000000000u) return -EINVAL;
    if (time->seconds < -62135596800LL || time->seconds > 253402300799LL) return -ERANGE;
    int64_t days = time->seconds / 86400;
    int64_t second = time->seconds % 86400;
    if (second < 0) { --days; second += 86400; }
    int64_t absolute = days + 719162;
    int low = 1, high = 10000;
    while (low + 1 < high) {
        int middle = low + (high - low) / 2;
        if (year_days(middle) <= absolute) low = middle;
        else high = middle;
    }
    unsigned remaining = (unsigned)(absolute - year_days(low));
    unsigned month = 1;
    while (remaining >= month_days(low, month)) {
        remaining -= month_days(low, month++);
    }
    out->year = low; out->month = (uint8_t)month; out->day = (uint8_t)(remaining + 1);
    out->hour = (uint8_t)(second / 3600);
    out->minute = (uint8_t)(second / 60 % 60); out->second = (uint8_t)(second % 60);
    int weekday = (int)((days + 3) % 7);
    if (weekday < 0) weekday += 7;
    out->weekday = (uint8_t)(weekday + 1); out->nanosecond = time->nanosecond;
    return 0;
}

int dmod_init(const Dmod_Config_t* config)
{
    (void)config;
    return 0;
}
int dmod_deinit(void)
{
    return 0;
}
