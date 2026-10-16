#include "dmod.h"
#include "dmutc.h"
#include "service.h"
#include "dmdrvi_ioctl.h"
#include <errno.h>
#include <string.h>

static void* open_source(const char* mode, int* rc)
{
    const char* path;
    *rc = dmutc_lock_source(&path);
    if (*rc != 0) return NULL;
    void* file = Dmod_FileOpen(path, mode);
    if (file == NULL) { *rc = -ENODEV; dmutc_unlock_source(); }
    return file;
}
static void close_source(void* file)
{
    Dmod_FileClose(file);
    dmutc_unlock_source();
}
static int read_status(void* file, dmutc_status_t* out)
{
    dmdrvi_rtc_info_t info = {0}; dmdrvi_rtc_status_t status = {0};
    int rc = Dmod_Ioctl(file, DMDRVI_IOCTL_RTC_GET_INFO, &info);
    if (rc == 0) rc = Dmod_Ioctl(file, DMDRVI_IOCTL_RTC_GET_STATUS, &status);
    if (rc != 0) return rc;
    if (info.reserved != 0 || status.reserved != 0 || info.resolution_ns == 0 ||
        info.min_year < 1 || info.max_year > 9999 || info.min_year > info.max_year) return -EIO;
    out->resolution_ns = info.resolution_ns;
    out->min_year = info.min_year; out->max_year = info.max_year;
    if (info.capabilities & DMDRVI_RTC_CAP_SET_TIME) out->flags |= DMUTC_CLOCK_WRITABLE;
    if (info.capabilities & DMDRVI_RTC_CAP_LOSS_DETECTION) {
        out->flags |= DMUTC_CLOCK_LOSS_DETECTION;
        if (status.flags & DMDRVI_RTC_STATUS_TIME_LOST) out->flags |= DMUTC_CLOCK_TIME_LOST;
    }
    if (status.flags & DMDRVI_RTC_STATUS_RUNNING) out->flags |= DMUTC_CLOCK_RUNNING;
    if (status.flags & DMDRVI_RTC_STATUS_TIME_SET) out->flags |= DMUTC_CLOCK_TIME_SET;
    return 0;
}

dmod_dmutc_api_declaration(1.0, int, _status, (dmutc_status_t* out))
{
    if (out == NULL) return -EINVAL;
    memset(out, 0, sizeof(*out));
    int rc;
    void* file = open_source("rb", &rc);
    if (file == NULL) return rc;
    rc = read_status(file, out);
    close_source(file);
    return rc;
}

dmod_dmutc_api_declaration(1.0, int, _get, (dmutc_time_t* out))
{
    if (out == NULL) return -EINVAL;
    memset(out, 0, sizeof(*out));
    int rc;
    void* file = open_source("rb", &rc);
    if (file == NULL) return rc;
    dmutc_status_t status = {0};
    rc = read_status(file, &status);
    if (rc == 0 && ((status.flags & (DMUTC_CLOCK_RUNNING | DMUTC_CLOCK_TIME_SET)) !=
        (DMUTC_CLOCK_RUNNING | DMUTC_CLOCK_TIME_SET) || (status.flags & DMUTC_CLOCK_TIME_LOST))) rc = -ENODATA;
    dmdrvi_rtc_time_t time = {0};
    if (rc == 0) rc = Dmod_Ioctl(file, DMDRVI_IOCTL_RTC_GET_TIME, &time);
    close_source(file);
    if (rc != 0) return rc;
    if (time.year < status.min_year || time.year > status.max_year) return -EIO;
    dmutc_calendar_t c = {time.year, time.month, time.day, time.hour, time.minute,
        time.second, 0, time.reserved, time.nanosecond};
    rc = dmutc_from_calendar(&c, out);
    return rc == 0 ? 0 : -EIO;
}

dmod_dmutc_api_declaration(1.0, int, _set, (const dmutc_time_t* time))
{
    dmutc_calendar_t c;
    int rc = dmutc_to_calendar(time, &c);
    if (rc != 0) return rc;
    void* file = open_source("r+b", &rc);
    if (file == NULL) return rc;
    dmutc_status_t status = {0};
    rc = read_status(file, &status);
    if (rc == 0 && !(status.flags & DMUTC_CLOCK_WRITABLE)) rc = -ENOTSUP;
    if (rc == 0 && (c.year < status.min_year || c.year > status.max_year)) rc = -ERANGE;
    if (rc == 0) {
        const dmdrvi_rtc_time_t value = {c.year, c.month, c.day, c.hour, c.minute,
            c.second, c.weekday, 0, c.nanosecond};
        rc = Dmod_Ioctl(file, DMDRVI_IOCTL_RTC_SET_TIME, (void*)&value);
    }
    close_source(file);
    return rc;
}

int dmutc_probe_source(const char* path)
{
    void* file = Dmod_FileOpen(path, "rb");
    if (file == NULL) return -ENODEV;
    dmutc_status_t status = {0};
    int rc = read_status(file, &status);
    Dmod_FileClose(file);
    return rc;
}
