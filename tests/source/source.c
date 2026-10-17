/*
 * test_dmutc_source: an external UTC source with a fixed time
 * (2026-10-17T08:00:00.25Z) and an error of 1 ns - better than any real one.
 */
#define DMOD_ENABLE_REGISTRATION ON
#include "dmod.h"
#include "dmutc.h"

dmod_dmutc_dif_api_declaration(1.0, test_dmutc_source, int, _read_source, (dmutc_time_t* time, uint64_t* error_ns))
{
    time->seconds = 1792224000;
    time->nanosecond = 250000000u;
    *error_ns = 1;
    return 0;
}

int dmod_init(const Dmod_Config_t* config)
{
    (void)config;
    /* (a module that calls nothing of dmod is refused on Cortex-M) */
    Dmod_Printf("test_dmutc_source: a UTC source of 2026-10-17T08:00:00.25Z\n");
    return 0;
}

int dmod_deinit(void)
{
    return 0;
}
