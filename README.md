# dmutc

[![CI](https://github.com/choco-technologies/dmutc/actions/workflows/ci.yml/badge.svg)](https://github.com/choco-technologies/dmutc/actions/workflows/ci.yml)

Portable UTC library for DMOD. It converts Gregorian UTC calendars to signed
64-bit Unix time and reads/sets a configured RTC through VFS and the generic
`DMDRVI_IOCTL_RTC_*` interface. It is a library, with no driver DIF, hardware
port, MCU registers or dependency on dmrtc. Other RTC implementations work
through the same contract.

```c
#include "dmutc.h"

dmutc_time_t now;
int rc = dmutc_get(NULL, &now);  /* /dev/rtc; seconds and nanoseconds */
if (rc == 0) {
    dmutc_calendar_t calendar;
    rc = dmutc_to_calendar(&now, &calendar);
}

/* Another RTC can be selected per call. Its calendar must store UTC. */
rc = dmutc_get("/dev/external-rtc", &now);
```

The library is stateless and allocates no persistent context, task, timer or
mutex. Each RTC operation opens and closes its own VFS handle. Pure conversion
functions require neither VFS nor an RTC. No libc `time_t`, timezone setting,
locale or `mktime()` is involved.

Time uses the POSIX convention: seconds since 1970-01-01T00:00:00Z, excluding
leap seconds, with nanoseconds in 0..999999999. Years 1..9999 are supported;
negative epochs and dates after 2038 work on 32-bit targets. For example,
`{-1, 500000000, 0}` is 1969-12-31T23:59:59.500000000Z. RTC hardware may have
narrower years/precision; restrictions and errors are preserved, never rounded.

`dmutc_get()` rejects stopped, unset and detected-lost clocks. Without a
hardware loss-detection capability, an unset loss flag does not establish
continuity. The chosen RTC must store UTC rather than local civil time.
Successful reads do **not** authenticate the clock or establish its accuracy.
A TLS caller must obtain its trust policy from the application; this library
does not turn a retained RTC marker or unauthenticated time into trusted UTC.
Network synchronization is a separate source of time, which can use
`dmutc_set()` after validating its sample.

Build and test using CMake:

```sh
cmake -S . -B build/host -DDMOD_TOOLS_NAME=arch/x86_64
cmake --build build/host --parallel
ctest --test-dir build/host --output-on-failure

cmake -S . -B build/stm32f7 -DDMOD_TOOLS_NAME=arch/armv7/cortex-m7
cmake --build build/stm32f7 --parallel
```

For local development, `-DDMOD_DIR=/path/to/dmod` selects an existing checkout;
no development-machine path or dependency version is pinned in source.
The module is scaffolded using DMOD's `new-module.sh --type library --github`.
Packages contain the public/generated headers, documentation, license and DMR.

See [API reference](docs/api-reference.md) and [validation](docs/testing.md).
