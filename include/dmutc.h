#ifndef DMUTC_H
#define DMUTC_H
#include <stdint.h>
#include "dmutc_defs.h"
#ifdef __cplusplus
extern "C" {
#endif

/* POSIX UTC: seconds since 1970-01-01T00:00:00Z, excluding leap seconds.
 * Fractions are normalized: 0 <= nanosecond < 1e9, even before the epoch.
 * Example: -0.5 seconds is { -1, 500000000, 0 }. reserved must be zero. */
typedef struct {
    int64_t seconds;
    uint32_t nanosecond;
    uint32_t reserved;
} dmutc_time_t;

/* Proleptic Gregorian UTC calendar, years 1..9999. weekday is Monday=1 to
 * Sunday=7 on output and ignored on input. All reserved fields are zero. */
typedef struct {
    int32_t year;
    uint8_t month, day, hour, minute, second, weekday;
    uint16_t reserved;
    uint32_t nanosecond;
} dmutc_calendar_t;

#define DMUTC_CLOCK_RUNNING        (1u << 0)
#define DMUTC_CLOCK_TIME_SET       (1u << 1)
#define DMUTC_CLOCK_LOSS_DETECTION (1u << 2)
#define DMUTC_CLOCK_TIME_LOST      (1u << 3)
#define DMUTC_CLOCK_WRITABLE       (1u << 4)
/* TIME_LOST is meaningful only with LOSS_DETECTION. No flag asserts UTC
 * accuracy, authentication or uninterrupted power on other devices. */
typedef struct {
    uint64_t resolution_ns;
    int32_t min_year, max_year;
    uint32_t flags;
    uint32_t reserved;
} dmutc_status_t;

/* Conversion is independent of libc time_t, timezone, VFS and hardware.
 * -EINVAL: malformed fields; -ERANGE: time outside years 1..9999.
 * Output is cleared on failure. Input and output must not overlap. */
dmod_dmutc_api(1.0, int, _from_calendar, (const dmutc_calendar_t* calendar, dmutc_time_t* time));
dmod_dmutc_api(1.0, int, _to_calendar, (const dmutc_time_t* time, dmutc_calendar_t* calendar));

/* Stateless VFS operations: NULL device selects /dev/rtc. The chosen RTC
 * must store UTC, never local civil time. Each call opens/closes its own
 * handle; reads use rb, writes r+b. No dmrtc or port dependency.
 * -ENODEV: opening the device failed; -ENODATA: unset/lost/stopped clock;
 * other negative driver errors propagate. Outputs are cleared on failure.
 * A successful read provides configured UTC, not authenticated time. TLS
 * users must establish their trust policy independently of these APIs. */
dmod_dmutc_api(1.0, int, _get, (const char* device, dmutc_time_t* time));
dmod_dmutc_api(1.0, int, _set, (const char* device, const dmutc_time_t* time));
dmod_dmutc_api(1.0, int, _status, (const char* device, dmutc_status_t* status));
#ifdef __cplusplus
}
#endif
#endif
