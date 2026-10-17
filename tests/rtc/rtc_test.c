#define _POSIX_C_SOURCE 200809L
#include "dmod.h"
#include "dmutc.h"
#include "dmdrvi_ioctl.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include "service.h"
int dmod_init(const Dmod_Config_t* config);
int dmod_deinit(void);
void* Dmod_Malloc(size_t size) { return malloc(size); }
void Dmod_Free(void* p) { free(p); }
#include <string.h>

static pthread_mutex_t gate_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gate_changed=PTHREAD_COND_INITIALIZER;
static bool block_read, read_entered, release_read, stop_started, stop_done;
static dmdrvi_rtc_time_t clock_time;
static dmdrvi_rtc_status_t clock_status;
static dmdrvi_rtc_info_t clock_info;
static unsigned opens, closes, writes, reads;
static bool open_error, write_handle;
static int failing_command, command_error;
static char last_path[64];
/* sync.c (the external sources' thread) is not under this test */
int dmutc_sync_start(void) { return 0; }
void dmutc_sync_stop(void) {}
void dmutc_sync_now(void) {}
static void reset(void)
{
    dmutc_stop_source("dmutc@model");
    clock_time=(dmdrvi_rtc_time_t){2040,2,29,12,34,56,3,0,123456789};
    clock_status=(dmdrvi_rtc_status_t){DMDRVI_RTC_STATUS_RUNNING | DMDRVI_RTC_STATUS_TIME_SET,0};
    clock_info=(dmdrvi_rtc_info_t){1,DMDRVI_RTC_CAP_SET_TIME,1900,2199,0};
    opens=closes=writes=reads=0; open_error=false;
    failing_command=command_error=0;
    const char* argv[]={"dmutc","/devices/other-rtc"};
    assert(dmutc_start_source("dmutc@model",2,argv)==0);
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
    case DMDRVI_IOCTL_RTC_GET_TIME:
        if(block_read) {
            pthread_mutex_lock(&gate_lock);
            read_entered=true; pthread_cond_broadcast(&gate_changed);
            while(!release_read)pthread_cond_wait(&gate_changed,&gate_lock);
            pthread_mutex_unlock(&gate_lock);
        }
        ++reads; *(dmdrvi_rtc_time_t*)arg=clock_time; return 0;
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
    assert(dmutc_get(&time)==0);
    assert(strcmp(last_path,"/devices/other-rtc")==0);
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
    assert(dmutc_get(&time)==-ENODATA); assert_zero(&time);
    clock_status.flags=DMDRVI_RTC_STATUS_TIME_SET;
    assert(dmutc_get(&time)==-ENODATA); assert_zero(&time);
    clock_status.flags=DMDRVI_RTC_STATUS_RUNNING|DMDRVI_RTC_STATUS_TIME_SET|DMDRVI_RTC_STATUS_TIME_LOST;
    // A non-advertised loss flag is not continuity detection.
    dmutc_status_t status;
    assert(dmutc_status(&status)==0);
    assert(!(status.flags&(DMUTC_CLOCK_LOSS_DETECTION|DMUTC_CLOCK_TIME_LOST)));
    assert(dmutc_get(&time)==0);
    clock_info.capabilities|=DMDRVI_RTC_CAP_LOSS_DETECTION;
    assert(dmutc_get(&time)==-ENODATA); assert_zero(&time);
    assert(dmutc_status(&status)==0);
    assert(status.flags&DMUTC_CLOCK_TIME_LOST);
    assert(opens==closes);
}
static void write_validation_and_driver_precision_errors(void)
{
    reset(); dmutc_calendar_t calendar={2000,2,29,1,2,3,0,0,250000000}; dmutc_time_t time;
    assert(dmutc_from_calendar(&calendar,&time)==0);
    assert(dmutc_set(&time)==0);
    assert(strcmp(last_path,"/devices/other-rtc")==0 && clock_time.nanosecond==250000000 && writes==1);
    clock_info.resolution_ns=1000000000;
    assert(dmutc_set(&time)==-ERANGE && clock_time.nanosecond==250000000);
    clock_info.capabilities=0;
    assert(dmutc_set(&time)==-ENOTSUP);
    clock_info.capabilities=DMDRVI_RTC_CAP_SET_TIME;
    calendar.year=1800; calendar.day=28; assert(dmutc_from_calendar(&calendar,&time)==0);
    unsigned before=writes;
    assert(dmutc_set(&time)==-ERANGE && writes==before);
    time.nanosecond=1000000000u;
    before=opens;
    assert(dmutc_set(&time)==-EINVAL && opens==before);
    assert(opens==closes);
}
static void errors_close_handles_and_clear_outputs(void)
{
    reset(); dmutc_time_t time; dmutc_status_t status;
    const int commands[]={DMDRVI_IOCTL_RTC_GET_INFO,DMDRVI_IOCTL_RTC_GET_STATUS,DMDRVI_IOCTL_RTC_GET_TIME};
    for(unsigned i=0;i<sizeof(commands)/sizeof(commands[0]);++i) {
        failing_command=commands[i]; command_error=-ETIMEDOUT;
        memset(&time,0xa5,sizeof(time));
        assert(dmutc_get(&time)==-ETIMEDOUT); assert_zero(&time);
        assert(opens==closes);
    }
    failing_command=0; clock_time.day=30;
    assert(dmutc_get(&time)==-EIO); assert_zero(&time);
    clock_time.day=29; clock_time.reserved=1;
    assert(dmutc_get(&time)==-EIO); assert_zero(&time);
    clock_time.reserved=0; clock_info.resolution_ns=0;
    assert(dmutc_get(&time)==-EIO); assert_zero(&time);
    const dmutc_status_t zero={0};
    memset(&status,0xa5,sizeof(status));
    assert(dmutc_status(&status)==-EIO);
    assert(memcmp(&status,&zero,sizeof(status))==0);
    assert(opens==closes);
    open_error=true;
    assert(dmutc_get(&time)==-ENODEV); assert_zero(&time);
    assert(dmutc_status(&status)==-ENODEV);
    assert(memcmp(&status,&zero,sizeof(status))==0);
    assert(dmutc_get(NULL)==-EINVAL);
    assert(dmutc_status(NULL)==-EINVAL);
    assert(dmutc_set(NULL)==-EINVAL);
    assert(opens==closes);
}
static void service_instances_select_and_remove_sources(void)
{
    reset();
    assert(dmutc_stop_source("dmutc@model")==0);
    dmutc_time_t time; char path[64];
    assert(dmutc_get(&time)==-ENODEV); assert_zero(&time);
    assert(dmutc_source(path,sizeof(path))==-ENODEV && path[0]=='\0');
    const char* z[]={"dmutc","/clocks/z"};
    const char* a[]={"dmutc","/clocks/a"};
    assert(dmutc_start_source("dmutc@z",2,z)==0);
    assert(dmutc_start_source("dmutc@a",2,a)==0);
    assert(dmutc_source(path,sizeof(path))==0 && strcmp(path,"/clocks/a")==0);
    assert(dmutc_source(path,2)==-ENOSPC && path[0]=='\0');
    assert(dmutc_get(&time)==0 && strcmp(last_path,"/clocks/a")==0);
    assert(dmutc_stop_source("dmutc@a")==0);
    assert(dmutc_get(&time)==0 && strcmp(last_path,"/clocks/z")==0);
    clock_info.resolution_ns=0;
    assert(dmutc_start_source("dmutc@bad",2,a)==-EIO);
    assert(dmutc_source(path,sizeof(path))==0 && strcmp(path,"/clocks/z")==0);
    clock_info.resolution_ns=1;
    const char* changed[]={"dmutc","/clocks/changed"};
    assert(dmutc_start_source("dmutc@z",2,changed)==0);
    assert(dmutc_source(path,sizeof(path))==0 && strcmp(path,"/clocks/changed")==0);
    assert(dmutc_stop_source("dmutc@z")==0);
    assert(dmutc_get(&time)==-ENODEV);
    assert(dmutc_stop_source("missing")==-ENOENT);
    assert(dmutc_start_source(NULL,2,a)==-EINVAL);
    assert(dmutc_start_source("name",1,a)==-EINVAL);
    const char* relative[]={"dmutc","rtc"};
    assert(dmutc_start_source("name",2,relative)==-EINVAL);
    assert(opens==closes);
}
static int reader_result, stopper_result;
static void* blocked_reader(void* unused)
{
    (void)unused; dmutc_time_t now; reader_result=dmutc_get(&now); return NULL;
}
static void* stop_during_read(void* unused)
{
    (void)unused;
    pthread_mutex_lock(&gate_lock); stop_started=true;
    pthread_cond_broadcast(&gate_changed); pthread_mutex_unlock(&gate_lock);
    stopper_result=dmutc_stop_source("dmutc@model");
    pthread_mutex_lock(&gate_lock); stop_done=true;
    pthread_cond_broadcast(&gate_changed); pthread_mutex_unlock(&gate_lock);
    return NULL;
}
static void stop_waits_for_io_and_does_not_restore_a_default_source(void)
{
    reset();
    block_read=true; read_entered=release_read=stop_started=stop_done=false;
    pthread_t reader,stopper;
    assert(pthread_create(&reader,NULL,blocked_reader,NULL)==0);
    pthread_mutex_lock(&gate_lock);
    while(!read_entered)pthread_cond_wait(&gate_changed,&gate_lock);
    pthread_mutex_unlock(&gate_lock);
    assert(pthread_create(&stopper,NULL,stop_during_read,NULL)==0);
    pthread_mutex_lock(&gate_lock);
    while(!stop_started)pthread_cond_wait(&gate_changed,&gate_lock);
    struct timespec deadline; clock_gettime(CLOCK_REALTIME,&deadline);
    deadline.tv_nsec+=20000000;
    if(deadline.tv_nsec>=1000000000){++deadline.tv_sec;deadline.tv_nsec-=1000000000;}
    int rc=0;
    while(!stop_done && rc==0)rc=pthread_cond_timedwait(&gate_changed,&gate_lock,&deadline);
    assert(rc==ETIMEDOUT && !stop_done);
    release_read=true; pthread_cond_broadcast(&gate_changed);
    pthread_mutex_unlock(&gate_lock);
    assert(pthread_join(reader,NULL)==0 && pthread_join(stopper,NULL)==0);
    assert(reader_result==0 && stopper_result==0);
    block_read=false;
    dmutc_time_t now; assert(dmutc_get(&now)==-ENODEV); assert_zero(&now);
    assert(opens==closes);
}
int main(void)
{
    assert(dmod_init(NULL)==0);
    reads_utc_without_stm32_range_or_precision_assumptions();
    lost_unset_and_stopped_clocks_do_not_supply_utc();
    write_validation_and_driver_precision_errors();
    errors_close_handles_and_clear_outputs();
    service_instances_select_and_remove_sources();
    stop_waits_for_io_and_does_not_restore_a_default_source();
    assert(dmod_deinit()==0);
    puts("UTC RTC provider/service: 6/6 scenarios passed");
    return 0;
}
