#include "dmod.h"
#include "dmutc.h"
#include "dmdrvi_ioctl.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static dmdrvi_rtc_time_t clock_time;
static dmdrvi_rtc_status_t clock_status;
static dmdrvi_rtc_info_t clock_info;
static unsigned opens, closes, writes, reads;
static bool open_error, write_handle;
static int failing_command, command_error;
static char last_path[64];
static void reset(void)
{
    clock_time=(dmdrvi_rtc_time_t){2040,2,29,12,34,56,3,0,123456789};
    clock_status=(dmdrvi_rtc_status_t){DMDRVI_RTC_STATUS_RUNNING | DMDRVI_RTC_STATUS_TIME_SET,0};
    clock_info=(dmdrvi_rtc_info_t){1,DMDRVI_RTC_CAP_SET_TIME,1900,2199,0};
    opens=closes=writes=reads=0; open_error=false;
    failing_command=command_error=0;
}
void* Dmod_FileOpen(const char* path, const char* mode)
{
    snprintf(last_path,sizeof(last_path),"%s",path);
    if(open_error)return NULL;
    ++opens;
    write_handle=strcmp(mode,"r+b")==0;
    assert(write_handle || strcmp(mode,"rb")==0);
    return &clock_time;
}
void Dmod_FileClose(void* file)
{
    assert(file==&clock_time); ++closes;
}
int Dmod_Ioctl(void* file,int command,void* arg)
{
    assert(file==&clock_time);
    if(command==failing_command)return command_error;
    switch(command) {
    case DMDRVI_IOCTL_RTC_GET_INFO: *(dmdrvi_rtc_info_t*)arg=clock_info; return 0;
    case DMDRVI_IOCTL_RTC_GET_STATUS: *(dmdrvi_rtc_status_t*)arg=clock_status; return 0;
    case DMDRVI_IOCTL_RTC_GET_TIME: ++reads; *(dmdrvi_rtc_time_t*)arg=clock_time; return 0;
    case DMDRVI_IOCTL_RTC_SET_TIME: {
        assert(write_handle); ++writes;
        dmdrvi_rtc_time_t* time=arg;
        if(time->nanosecond%clock_info.resolution_ns!=0)return -ERANGE;
        clock_time=*time; return 0;
    }
    default:return -ENOTTY;
    }
}
static void assert_zero(const dmutc_time_t* t)
{
    const dmutc_time_t zero={0}; assert(memcmp(t,&zero,sizeof(*t))==0);
}
static void reads_utc_without_stm32_range_or_precision_assumptions(void)
{
    reset(); dmutc_time_t time; dmutc_calendar_t calendar;
    assert(dmutc_get("/dev/other-rtc",&time)==0);
    assert(strcmp(last_path,"/dev/other-rtc")==0);
    assert(time.seconds>INT32_MAX && time.nanosecond==123456789);
    assert(dmutc_to_calendar(&time,&calendar)==0);
    assert(calendar.year==2040 && calendar.month==2 && calendar.day==29);
    assert(calendar.hour==12 && calendar.minute==34 && calendar.second==56);
    assert(opens==closes && reads==1);
}
static void lost_unset_and_stopped_clocks_do_not_supply_utc(void)
{
    reset(); dmutc_time_t time;
    clock_status.flags=DMDRVI_RTC_STATUS_RUNNING;
    assert(dmutc_get(NULL,&time)==-ENODATA); assert_zero(&time);
    clock_status.flags=DMDRVI_RTC_STATUS_TIME_SET;
    assert(dmutc_get(NULL,&time)==-ENODATA); assert_zero(&time);
    clock_status.flags=DMDRVI_RTC_STATUS_RUNNING|DMDRVI_RTC_STATUS_TIME_SET|DMDRVI_RTC_STATUS_TIME_LOST;
    // A non-advertised loss flag is not continuity detection.
    dmutc_status_t status;
    assert(dmutc_status(NULL,&status)==0);
    assert(!(status.flags&(DMUTC_CLOCK_LOSS_DETECTION|DMUTC_CLOCK_TIME_LOST)));
    assert(dmutc_get(NULL,&time)==0);
    clock_info.capabilities|=DMDRVI_RTC_CAP_LOSS_DETECTION;
    assert(dmutc_get(NULL,&time)==-ENODATA); assert_zero(&time);
    assert(dmutc_status(NULL,&status)==0);
    assert(status.flags&DMUTC_CLOCK_TIME_LOST);
    assert(opens==closes);
}
static void write_validation_and_driver_precision_errors(void)
{
    reset(); dmutc_calendar_t calendar={2000,2,29,1,2,3,0,0,250000000}; dmutc_time_t time;
    assert(dmutc_from_calendar(&calendar,&time)==0);
    assert(dmutc_set(NULL,&time)==0);
    assert(strcmp(last_path,"/dev/rtc")==0 && clock_time.nanosecond==250000000 && writes==1);
    clock_info.resolution_ns=1000000000;
    assert(dmutc_set(NULL,&time)==-ERANGE && clock_time.nanosecond==250000000);
    clock_info.capabilities=0;
    assert(dmutc_set(NULL,&time)==-ENOTSUP);
    clock_info.capabilities=DMDRVI_RTC_CAP_SET_TIME;
    calendar.year=1800; calendar.day=28; assert(dmutc_from_calendar(&calendar,&time)==0);
    unsigned before=writes;
    assert(dmutc_set(NULL,&time)==-ERANGE && writes==before);
    time.nanosecond=1000000000u;
    before=opens;
    assert(dmutc_set(NULL,&time)==-EINVAL && opens==before);
    assert(opens==closes);
}
static void errors_close_handles_and_clear_outputs(void)
{
    reset(); dmutc_time_t time; dmutc_status_t status;
    const int commands[]={DMDRVI_IOCTL_RTC_GET_INFO,DMDRVI_IOCTL_RTC_GET_STATUS,DMDRVI_IOCTL_RTC_GET_TIME};
    for(unsigned i=0;i<sizeof(commands)/sizeof(commands[0]);++i) {
        failing_command=commands[i]; command_error=-ETIMEDOUT;
        memset(&time,0xa5,sizeof(time));
        assert(dmutc_get(NULL,&time)==-ETIMEDOUT); assert_zero(&time);
        assert(opens==closes);
    }
    failing_command=0; clock_time.day=30;
    assert(dmutc_get(NULL,&time)==-EIO); assert_zero(&time);
    clock_time.day=29; clock_time.reserved=1;
    assert(dmutc_get(NULL,&time)==-EIO); assert_zero(&time);
    clock_time.reserved=0; clock_info.resolution_ns=0;
    assert(dmutc_get(NULL,&time)==-EIO); assert_zero(&time);
    const dmutc_status_t zero={0};
    memset(&status,0xa5,sizeof(status));
    assert(dmutc_status(NULL,&status)==-EIO);
    assert(memcmp(&status,&zero,sizeof(status))==0);
    assert(opens==closes);
    open_error=true;
    assert(dmutc_get(NULL,&time)==-ENODEV); assert_zero(&time);
    assert(dmutc_status(NULL,&status)==-ENODEV);
    assert(memcmp(&status,&zero,sizeof(status))==0);
    assert(dmutc_get("",&time)==-EINVAL && dmutc_get(NULL,NULL)==-EINVAL);
    assert(dmutc_status("",&status)==-EINVAL && dmutc_status(NULL,NULL)==-EINVAL);
    assert(dmutc_set(NULL,NULL)==-EINVAL);
    assert(opens==closes);
}
int main(void)
{
    reads_utc_without_stm32_range_or_precision_assumptions();
    lost_unset_and_stopped_clocks_do_not_supply_utc();
    write_validation_and_driver_precision_errors();
    errors_close_handles_and_clear_outputs();
    puts("UTC RTC provider: 4/4 scenarios passed");
    return 0;
}
