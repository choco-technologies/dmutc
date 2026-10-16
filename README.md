# dmutc

[![CI](https://github.com/choco-technologies/dmutc/actions/workflows/ci.yml/badge.svg)](https://github.com/choco-technologies/dmutc/actions/workflows/ci.yml)

UTC library and device-driven library service for DMOD. It converts Gregorian
UTC calendars to signed 64-bit Unix time and accesses the RTC selected by the
system's device/service rules. It has no driver DIF, hardware port, MCU code
or dependency on a particular RTC implementation.

Device discovery owns the binding:

1. dmdevfs probes `DMDRVI_IOCTL_RTC_GET_INFO` and reports class `rtc`, even if
   its calendar is unset. The device event carries the actual VFS path.
2. `dmutc.rules` starts `dmutc@%name` for that device.
3. `dmutc@.ini` is `type=library`, `exec=dmutc`, `args=%v`. libsystemd enables
   the shared library and calls its service START DIF with the expanded path.
4. Device removal calls the matching service STOP DIF and withdraws that source.

Applications consume UTC without selecting a driver or path:

```c
#include "dmutc.h"

dmutc_time_t now;
int rc = dmutc_get(&now);
if (rc == 0) {
    dmutc_calendar_t calendar;
    rc = dmutc_to_calendar(&now, &calendar);
}
```

`dmutc_get`, `dmutc_set` and `dmutc_status` return ENODEV when no service has
bound a source. There is no default path, fallback to `/dev/rtc`, directory
scan or application-side bind API. `dmutc_source()` copies the selected path
for diagnostics only. An API consumer may enable the module before the RTC
appears; later service START still binds it. Stopping one unit neither unloads
code used by consumers nor removes other units' sources.

Multiple active source units are ordered lexicographically by unit name.
Removing the selected one selects the next; removing the last makes UTC
unavailable. Rules can restrict which devices start a UTC service. The chosen
RTC must store UTC rather than local civil time. Stopped, unset and detected-lost
clocks are rejected; lack of loss detection never proves continuity. A failed
read clears its output. Successful UTC reads do not authenticate their source.

Time uses POSIX seconds since 1970-01-01T00:00:00Z, excluding leap seconds,
with normalized nanoseconds. Years 1..9999, negative epochs and post-2038 dates
work on 32-bit targets without libc time_t, timezone or locale state. Device
range and precision errors propagate without rounding. Conversion functions
work independently of whether any RTC service is active.

The library keeps a mutex-protected source registry. RTC operations own their
VFS handles and finish before a concurrent STOP can release the chosen path.
There is no background thread, timer or polling loop.

Install the packaged configs with the standard service/rules tags:

```text
dmutc service=dmutc@.ini
dmutc rules=dmutc.rules
```

The library uses the paired library-service lifecycle DIF in libsystemd; release
that interface and RTC reporting in dmdevfs before integrating this version.

Build with CMake:

```sh
cmake -S . -B build/host -DDMOD_TOOLS_NAME=arch/x86_64
cmake --build build/host --parallel
ctest --test-dir build/host --output-on-failure
```

Use `-DDMOD_TOOLS_NAME=arch/armv7/cortex-m7` for M7, or cortex-m4 for M4.
`-DDMOD_DIR=/path/to/dmod` selects a local checkout. No dependency version or
development-machine path is pinned in source. The initial library was generated
using dmod's new-module.sh with --type library and --github.

See [API reference](docs/api-reference.md) and [validation](docs/testing.md).
