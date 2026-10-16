#define DMOD_ENABLE_REGISTRATION ON
#include "dmod_test.h"
#include "dmutc.h"
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>
#ifdef DMUTC_BOARD_TEST
#include "libsystemd.h"
#endif

_Static_assert(sizeof(dmutc_time_t) == 16, "time ABI");
_Static_assert(offsetof(dmutc_time_t, nanosecond) == 8, "fraction ABI");
_Static_assert(sizeof(dmutc_calendar_t) == 16, "calendar ABI");
_Static_assert(sizeof(dmutc_status_t) == 24, "status ABI");
DMOD_TEST_STEP(known_unix_values_before_epoch_after_2038_and_at_limits)
{
    static const struct { int y,m,d,h,min,s,weekday; int64_t seconds; } vectors[] = {
    {1,1,1,0,0,0,1,-62135596800LL},
    {1969,12,31,23,59,59,3,-1LL},
    {1970,1,1,0,0,0,4,0LL},
    {2000,2,29,0,0,0,2,951782400LL},
    {2038,1,19,3,14,8,2,2147483648LL},
    {2100,3,1,0,0,0,1,4107542400LL},
    {2400,2,29,12,0,0,2,13574606400LL},
    {9999,12,31,23,59,59,5,253402300799LL},
    };
    for (unsigned i = 0; i < sizeof(vectors)/sizeof(vectors[0]); ++i) {
        dmutc_calendar_t c = {vectors[i].y,vectors[i].m,vectors[i].d,vectors[i].h,
            vectors[i].min,vectors[i].s,99,0,123456789}, out;
        dmutc_time_t time;
        DMOD_TEST_EXPECT_EQ(dmutc_from_calendar(&c, &time), 0);
        DMOD_TEST_EXPECT_EQ(time.seconds, vectors[i].seconds);
        DMOD_TEST_EXPECT_EQ(time.nanosecond, 123456789);
        DMOD_TEST_EXPECT_EQ(time.reserved, 0);
        DMOD_TEST_EXPECT_EQ(dmutc_to_calendar(&time, &out), 0);
        DMOD_TEST_EXPECT_EQ(out.year,c.year); DMOD_TEST_EXPECT_EQ(out.month,c.month);
        DMOD_TEST_EXPECT_EQ(out.day,c.day); DMOD_TEST_EXPECT_EQ(out.hour,c.hour);
        DMOD_TEST_EXPECT_EQ(out.minute,c.minute); DMOD_TEST_EXPECT_EQ(out.second,c.second);
        DMOD_TEST_EXPECT_EQ(out.weekday,vectors[i].weekday);
        DMOD_TEST_EXPECT_EQ(out.nanosecond,c.nanosecond); DMOD_TEST_EXPECT_EQ(out.reserved,0);
    }
}
DMOD_TEST_STEP(gregorian_centuries_and_invalid_fields)
{
    dmutc_calendar_t c = {1900,2,29,0,0,0,0,0,0}; dmutc_time_t out;
    DMOD_TEST_EXPECT_EQ(dmutc_from_calendar(&c,&out),-EINVAL);
    c.year=2000; DMOD_TEST_EXPECT_EQ(dmutc_from_calendar(&c,&out),0);
    c.year=2100; DMOD_TEST_EXPECT_EQ(dmutc_from_calendar(&c,&out),-EINVAL);
    c.year=2400; DMOD_TEST_EXPECT_EQ(dmutc_from_calendar(&c,&out),0);
    c.year=0; DMOD_TEST_EXPECT_EQ(dmutc_from_calendar(&c,&out),-EINVAL);
    c.year=10000; DMOD_TEST_EXPECT_EQ(dmutc_from_calendar(&c,&out),-EINVAL);
    c.year=2026; c.month=4; c.day=31;
    DMOD_TEST_EXPECT_EQ(dmutc_from_calendar(&c,&out),-EINVAL);
    c.day=30; c.month=0; DMOD_TEST_EXPECT_EQ(dmutc_from_calendar(&c,&out),-EINVAL);
    c.month=13; DMOD_TEST_EXPECT_EQ(dmutc_from_calendar(&c,&out),-EINVAL);
    c.month=4; c.hour=24; DMOD_TEST_EXPECT_EQ(dmutc_from_calendar(&c,&out),-EINVAL);
    c.hour=0; c.minute=60; DMOD_TEST_EXPECT_EQ(dmutc_from_calendar(&c,&out),-EINVAL);
    c.minute=0; c.second=60; DMOD_TEST_EXPECT_EQ(dmutc_from_calendar(&c,&out),-EINVAL);
    c.second=0; c.nanosecond=1000000000u;
    DMOD_TEST_EXPECT_EQ(dmutc_from_calendar(&c,&out),-EINVAL);
    c.nanosecond=0; c.reserved=1;
    memset(&out,0xa5,sizeof(out)); DMOD_TEST_EXPECT_EQ(dmutc_from_calendar(&c,&out),-EINVAL);
    const dmutc_time_t zero={0}; DMOD_TEST_EXPECT_EQ(memcmp(&out,&zero,sizeof(out)),0);
}
DMOD_TEST_STEP(floor_normalization_and_overflow_rejection)
{
    dmutc_time_t t={-1,500000000,0}; dmutc_calendar_t c;
    DMOD_TEST_EXPECT_EQ(dmutc_to_calendar(&t,&c),0);
    DMOD_TEST_EXPECT_EQ(c.year,1969); DMOD_TEST_EXPECT_EQ(c.second,59);
    DMOD_TEST_EXPECT_EQ(c.nanosecond,500000000);
    const int64_t invalid[]={INT64_MIN,-62135596801LL,253402300800LL,INT64_MAX};
    for (unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i) {
        t.seconds=invalid[i]; memset(&c,0xa5,sizeof(c));
        DMOD_TEST_EXPECT_EQ(dmutc_to_calendar(&t,&c),-ERANGE);
        const dmutc_calendar_t zero={0}; DMOD_TEST_EXPECT_EQ(memcmp(&c,&zero,sizeof(c)),0);
    }
    t.seconds=0; t.nanosecond=1000000000u;
    DMOD_TEST_EXPECT_EQ(dmutc_to_calendar(&t,&c),-EINVAL);
    t.nanosecond=0; t.reserved=1;
    DMOD_TEST_EXPECT_EQ(dmutc_to_calendar(&t,&c),-EINVAL);
    DMOD_TEST_EXPECT_EQ(dmutc_to_calendar(NULL,&c),-EINVAL);
    DMOD_TEST_EXPECT_EQ(dmutc_to_calendar(&t,NULL),-EINVAL);
    DMOD_TEST_EXPECT_EQ(dmutc_from_calendar(NULL,&t),-EINVAL);
    DMOD_TEST_EXPECT_EQ(dmutc_from_calendar(&c,NULL),-EINVAL);
}
DMOD_TEST_STEP(month_boundaries_round_trip_across_the_supported_era)
{
    for (int year=1;year<=9999;year+=37) for (int month=1;month<=12;++month) {
        dmutc_calendar_t c={year,month,1,23,59,59,0,0,999999999}, out;
        dmutc_time_t time;
        DMOD_TEST_EXPECT_EQ(dmutc_from_calendar(&c,&time),0);
        DMOD_TEST_EXPECT_EQ(dmutc_to_calendar(&time,&out),0);
        DMOD_TEST_EXPECT_EQ(out.year,year); DMOD_TEST_EXPECT_EQ(out.month,month);
        DMOD_TEST_EXPECT_EQ(out.day,1); DMOD_TEST_EXPECT_EQ(out.second,59);
        time.seconds += 1;
        DMOD_TEST_EXPECT_EQ(dmutc_to_calendar(&time,&out),0);
        DMOD_TEST_EXPECT_EQ(out.day,2); DMOD_TEST_EXPECT_EQ(out.hour,0);
    }
}
#ifdef DMUTC_BOARD_TEST
struct selected_unit { char name[96]; unsigned count; };
static bool find_utc_unit(const libsystemd_service_info_t* info, void* user)
{
    struct selected_unit* found=user;
    if(strncmp(info->unit_name,"dmutc@",6)==0 && info->status.state==DMOSI_PROCESS_STATE_RUNNING) {
        ++found->count;
        size_t n=strlen(info->unit_name);
        if(n<sizeof(found->name))memcpy(found->name,info->unit_name,n+1);
    }
    return true;
}
DMOD_TEST_STEP(library_service_controls_binding_without_a_path_fallback)
{
    struct selected_unit unit={{0},0};
    DMOD_TEST_EXPECT_EQ(libsystemd_list(find_utc_unit,&unit),0);
    DMOD_TEST_EXPECT_EQ(unit.count,1);
    if(unit.count!=1 || unit.name[0]=='\0')return;
    char source[128]; dmutc_time_t now;
    DMOD_TEST_EXPECT_EQ(dmutc_source(source,sizeof(source)),0);
    Dmod_Printf("UTC_SOURCE unit=%s path=%s\n",unit.name,source);
    DMOD_TEST_EXPECT_EQ(libsystemd_stop_service(unit.name),0);
    DMOD_TEST_EXPECT_TRUE(Dmod_IsModuleEnabled("dmutc")); /* API consumer is still connected. */
    DMOD_TEST_EXPECT_TRUE(Dmod_FileAvailable(source)); /* RTC still exists. */
    DMOD_TEST_EXPECT_EQ(dmutc_get(&now),-ENODEV);
    DMOD_TEST_EXPECT_EQ(libsystemd_start_service(unit.name,NULL),0);
    DMOD_TEST_EXPECT_EQ(dmutc_get(&now),0);
    /* Replace the device event payload while keeping its identity: a stale
     * cached %v must not silently bind the old, still-existing RTC path. */
    const char* name=unit.name+6;
    DMOD_TEST_EXPECT_EQ(libsystemd_notify_device_removed("rtc",name),0);
    DMOD_TEST_EXPECT_EQ(libsystemd_notify_device_added("rtc",name,"/no-such-utc-source"),-ENODEV);
    DMOD_TEST_EXPECT_EQ(dmutc_get(&now),-ENODEV);
    DMOD_TEST_EXPECT_EQ(libsystemd_notify_device_added("rtc",name,source),0);
    DMOD_TEST_EXPECT_EQ(dmutc_get(&now),0);
}
DMOD_TEST_STEP(vfs_rtc_utc_round_trip_and_restore)
{
    dmutc_time_t saved, now; dmutc_status_t status;
    int rc=dmutc_get(&saved);
    DMOD_TEST_EXPECT_EQ(rc,0); if(rc!=0)return;
    uint64_t started=Dmod_GetUptime();
    DMOD_TEST_EXPECT_EQ(dmutc_status(&status),0);
    DMOD_TEST_EXPECT_TRUE(status.flags & DMUTC_CLOCK_RUNNING);
    DMOD_TEST_EXPECT_TRUE(status.flags & DMUTC_CLOCK_WRITABLE);
    const dmutc_time_t seed={951827696,0,0};
    DMOD_TEST_EXPECT_EQ(dmutc_set(&seed),0);
    DMOD_TEST_EXPECT_EQ(dmutc_get(&now),0);
    DMOD_TEST_EXPECT_TRUE(now.seconds>=seed.seconds && now.seconds<=seed.seconds+1);
    dmutc_time_t fraction=seed; fraction.nanosecond=1;
    DMOD_TEST_EXPECT_EQ(dmutc_set(&fraction),-ERANGE);
    saved.seconds+=(int64_t)((Dmod_GetUptime()-started)/1000);
    DMOD_TEST_EXPECT_EQ(dmutc_set(&saved),0);
    DMOD_TEST_EXPECT_EQ(dmutc_get(&now),0);
    DMOD_TEST_EXPECT_TRUE(now.seconds>=saved.seconds && now.seconds<=saved.seconds+1);
    Dmod_Printf("UTC_SAMPLE seconds=%lld nanosecond=%u resolution_ns=%llu\n",
        (long long)now.seconds,(unsigned)now.nanosecond,(unsigned long long)status.resolution_ns);
}
#endif
