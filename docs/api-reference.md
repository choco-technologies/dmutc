# API reference

Include `dmutc.h`; declare the module through `dmod_link_modules()`.
All calls return zero or a negative errno-compatible code. Required outputs
are cleared on failure; conversion input/output objects must not overlap.

| Function | Behavior |
|---|---|
| `dmutc_from_calendar(calendar, time)` | Gregorian UTC to normalized Unix seconds/nanoseconds. |
| `dmutc_to_calendar(time, calendar)` | Unix time to UTC; years outside 1..9999 fail with ERANGE. |
| `dmutc_get(time)` | Read the source bound by the active library service. |
| `dmutc_set(time)` | Validate, convert and write the bound RTC. |
| `dmutc_status(status)` | Observe the bound source's range, quantum and state/capabilities. |
| `dmutc_source(buffer, capacity)` | Copy the selected source path, including NUL; ENOSPC if too small. |

No source is implicitly selected. With no active source unit, device operations
return ENODEV; pure conversion remains available. Applications never pass a
path into UTC read/write/status calls. The service lifecycle accepts the path
from libsystemd's expanded `args=%v`, as supplied by the `rtc` device event.

`dmutc_time_t` is 16 bytes: signed int64_t seconds, uint32_t nanoseconds and
zero reserved field. Fractions are 0..999999999 even for negative epochs:
{-1,500000000,0} represents 1969-12-31T23:59:59.500000000Z. Calendars are 16
bytes with full Gregorian year, month/day/hour/minute/second, weekday
(Monday=1..Sunday=7, ignored on input), zero reserved field and nanoseconds.
Leap-century rules apply; POSIX time excludes leap seconds.

Status is 24 bytes: resolution_ns, inclusive min_year/max_year, flags and zero
reserved field. DMUTC_CLOCK_ flags are RUNNING, TIME_SET, LOSS_DETECTION,
TIME_LOST and WRITABLE. Quantum is representable granularity, not accuracy.
TIME_LOST is only meaningful when loss detection is advertised. No flag
attests UTC authenticity, accuracy or continuity on other devices.

SET rejects malformed time before accessing hardware. Range restrictions are
checked before SET_TIME; subsecond precision reaches the driver intact for it
to accept or reject. Driver errors propagate. Failed opens return ENODEV;
malformed metadata/calendar returns EIO; stopped/unset/detected-lost clocks
return ENODATA. Metadata is an observation, not an atomic promise about a later
ioctl; intervening device changes are handled by the driver error path.

Each source is owned by a libsystemd unit instance. Multiple units are sorted
by unit name, independent of arrival order; the first is selected. STOP waits
on the source mutex for active I/O, removes only its binding, and leaves shared
library code available to API consumers. No source is reconstructed from a
fixed path after STOP. Module deinitialization releases the registry and mutex.

Calls use VFS/mutexes and belong in thread context. There is no periodic worker,
network synchronization or implicit authentication of time. A TLS application
must establish which time sources it accepts through its system/trust policy.
