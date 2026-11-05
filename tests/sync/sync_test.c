#include "dmod.h"
#include "dmutc.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

const char* dmod_dmutc_read_source_sig = "test_read_source";
static Dmod_Context_t source;
static dmutc_time_t sample, written;
static uint64_t resolution;
static int source_error, status_error, write_error;
static unsigned writes, statuses;
static int read_sample(dmutc_time_t* time, uint64_t* error_ns)
{
    *time = sample; *error_ns = 26091551;
    return source_error;
}
Dmod_Context_t* Dmod_GetNextDifModule(const char* sig, Dmod_Context_t* previous)
{
    assert(strcmp(sig, dmod_dmutc_read_source_sig) == 0);
    return previous == NULL ? &source : NULL;
}
void (*Dmod_GetDifFunction(Dmod_Context_t* module, const char* sig))(void)
{
    assert(module == &source && strcmp(sig, dmod_dmutc_read_source_sig) == 0);
    return (void (*)(void))read_sample;
}
int dmutc_status(dmutc_status_t* out)
{
    ++statuses;
    memset(out, 0, sizeof(*out)); out->resolution_ns = resolution;
    return status_error;
}
int dmutc_set(const dmutc_time_t* time)
{
    ++writes;
    /* A strict RTC rejects a fraction it cannot represent. */
    if (time->nanosecond % resolution != 0) return -ERANGE;
    written = *time;
    return write_error;
}
static void reset(void)
{
    sample = (dmutc_time_t){1793895771, 672893745, 0};
    written = (dmutc_time_t){0}; resolution = 1000000000;
    source_error = status_error = write_error = 0;
    writes = statuses = 0;
}
int main(void)
{
    reset(); assert(dmutc_sync() == 0);
    assert(writes == 1 && written.seconds == sample.seconds && written.nanosecond == 0);
    reset(); resolution = 1000000; assert(dmutc_sync() == 0);
    assert(written.seconds == sample.seconds && written.nanosecond == 672000000);
    reset(); resolution = 1; assert(dmutc_sync() == 0);
    assert(memcmp(&written, &sample, sizeof(sample)) == 0);
    reset(); resolution = 250000000; sample.nanosecond = 250000000;
    assert(dmutc_sync() == 0 && written.nanosecond == sample.nanosecond);
    reset(); sample.seconds = -1; sample.nanosecond = 500000000;
    assert(dmutc_sync() == 0 && written.seconds == -1 && written.nanosecond == 0);
    reset(); source_error = -ETIMEDOUT;
    assert(dmutc_sync() == -ENODATA && writes == 0 && statuses == 0);
    reset(); status_error = -ENODEV;
    assert(dmutc_sync() == -ENODEV && writes == 0);
    reset(); write_error = -EACCES;
    assert(dmutc_sync() == -EACCES && writes == 1);
    puts("sync: 8 scenarios passed");
    return 0;
}
