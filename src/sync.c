/**
 * @file sync.c
 * @brief External UTC sources (the dmutc_read_source DIF) and the thread
 *        that sets the RTC from the best of them.
 *
 * Sources are looked for on every call through Dmod_GetNextDifModule(), not
 * registered: a source module that is unloaded is simply not found again.
 *
 * ENABLE_DIF_REGISTRATIONS is set here, in the only file that uses
 * dmod_dmutc_read_source_sig: the loader does not relocate pointers in
 * .data, so the signature has to be seen with its initialiser where it is
 * used (see dmdns_servers.c).
 */
#define ENABLE_DIF_REGISTRATIONS ON
#include "dmod.h"
#include "dmosi.h"
#include "dmutc.h"
#include "service.h"
#include <errno.h>
#include <string.h>

#define SYNC_STACK      4096u           /* A source may do a network exchange in it */
#define SYNC_PRIORITY   1

static dmosi_thread_t       sync_thread;
static dmosi_semaphore_t    sync_wake;
static volatile bool        sync_quit;

/**
 * @brief Ask one source module: its time and error, or a negative errno
 */
static int read_one(Dmod_Context_t* module, dmutc_time_t* time, uint64_t* error_ns)
{
    dmod_dmutc_read_source_t read = (dmod_dmutc_read_source_t)
        Dmod_GetDifFunction(module, dmod_dmutc_read_source_sig);

    if (read == NULL)
        return -ENOSYS;

    memset(time, 0, sizeof(*time));
    *error_ns = UINT64_MAX;
    int rc = read(time, error_ns);
    if (rc == 0 && (time->nanosecond >= 1000000000u || time->reserved != 0))
        rc = -EIO;
    return rc;
}

dmod_dmutc_api_declaration(1.0, int, _read_sources, (dmutc_time_t* time, uint64_t* error_ns))
{
    if (time == NULL || error_ns == NULL)
        return -EINVAL;

    memset(time, 0, sizeof(*time));
    *error_ns = 0;

    int rc = -ENODATA;
    Dmod_Context_t* module = Dmod_GetNextDifModule(dmod_dmutc_read_source_sig, NULL);
    while (module != NULL)
    {
        dmutc_time_t t;
        uint64_t error;
        if (read_one(module, &t, &error) == 0 && (rc != 0 || error < *error_ns))
        {
            *time = t;
            *error_ns = error;
            rc = 0;
        }
        module = Dmod_GetNextDifModule(dmod_dmutc_read_source_sig, module);
    }
    return rc;
}

dmod_dmutc_api_declaration(1.0, int, _sync, (void))
{
    dmutc_time_t time;
    uint64_t error_ns;

    int rc = dmutc_read_sources(&time, &error_ns);
    if (rc == 0)
        rc = dmutc_set(&time);
    if (rc == 0)
    {
        DMOD_LOG_INFO("dmutc: RTC set from an external source (+/- %lu ms)\n",
                      (unsigned long)(error_ns / 1000000u));
    }
    return rc;
}

/**
 * @brief The sync thread: dmutc_sync() now, again after DMUTC_SYNC_RETRY_MS
 *        while it fails, then every DMUTC_SYNC_PERIOD_MS - and whenever an
 *        RTC is bound (dmutc_sync_now())
 */
static void sync_main(void* arg)
{
    (void)arg;
    uint32_t wait_ms = 0;

    while (!sync_quit)
    {
        (void)dmosi_semaphore_wait(sync_wake, 1, (int32_t)wait_ms);
        if (sync_quit)
            break;
        wait_ms = (dmutc_sync() == 0) ? DMUTC_SYNC_PERIOD_MS : DMUTC_SYNC_RETRY_MS;
    }
}

void dmutc_sync_now(void)
{
    if (sync_wake != NULL)
        (void)dmosi_semaphore_post(sync_wake, 1);
}

int dmutc_sync_start(void)
{
    sync_quit = false;
    sync_wake = dmosi_semaphore_create(0, 1);
    if (sync_wake == NULL)
        return -ENOMEM;

    sync_thread = dmosi_thread_create(sync_main, NULL, SYNC_PRIORITY, SYNC_STACK, "dmutc_sync", NULL);
    if (sync_thread == NULL)
    {
        dmosi_semaphore_destroy(sync_wake);
        sync_wake = NULL;
        return -ENOMEM;
    }
    return 0;
}

void dmutc_sync_stop(void)
{
    if (sync_thread != NULL)
    {
        sync_quit = true;
        (void)dmosi_semaphore_post(sync_wake, 1);
        (void)dmosi_thread_join(sync_thread);
        dmosi_thread_destroy(sync_thread);
        sync_thread = NULL;
    }
    if (sync_wake != NULL)
    {
        dmosi_semaphore_destroy(sync_wake);
        sync_wake = NULL;
    }
}
