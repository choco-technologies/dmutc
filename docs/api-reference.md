# API reference

Include `dmutc.h` and declare `dmutc` using `dmod_link_modules()`.
All operations return zero on success and a negative errno-compatible code on
failure. Required output pointers must be non-NULL; outputs are cleared on
failure. Inputs/outputs of conversion functions must not overlap.

| Function | Behavior |
|---|---|
| `dmutc_from_calendar(calendar, time)` | Convert UTC Gregorian calendar to Unix seconds/nanoseconds. Invalid fields return EINVAL. |
| `dmutc_to_calendar(time, calendar)` | Convert normalized Unix time to UTC; timestamps outside years 1..9999 return ERANGE. |
| `dmutc_get(device, time)` | Read an initialized running RTC, interpreting its civil calendar as UTC. |
| `dmutc_set(device, time)` | Validate and convert Unix time, then write RTC through its generic ioctl. |
| `dmutc_status(device, status)` | Report device years, quantum, running/set/write/loss capabilities and state. Works when unset. |

`device == NULL` selects `/dev/rtc`. An empty path returns EINVAL; failed VFS
open returns ENODEV (the SAL open API does not expose a portable errno).
Driver errors such as EIO, ENODATA, ERANGE, ETIMEDOUT, EACCES and ENOTSUP
propagate. GET_INFO and GET_STATUS are required by the RTC device contract.
Malformed device metadata/calendar returns EIO. Loss is interpreted only when
the driver advertises loss detection. No status bit means authenticated UTC.

`dmutc_time_t` contains signed `int64_t seconds`, normalized `uint32_t
nanosecond` and a zero `reserved` field (16 bytes). `dmutc_calendar_t` contains
full year, month, day, hour, minute, second, weekday, zero reserved field and
nanoseconds (16 bytes). Weekday is Monday=1..Sunday=7 and is ignored on input.
Gregorian leap-century rules apply; the API does not encode leap seconds.

`dmutc_status_t` reports `resolution_ns`, inclusive `min_year`/`max_year`, flags
and zero reserved field (24 bytes). The quantum reports representable precision,
not oscillator accuracy. Flags are RUNNING, TIME_SET, LOSS_DETECTION, TIME_LOST
and WRITABLE, prefixed with `DMUTC_CLOCK_`.

Every RTC call uses its own file handle (rb for reads, r+b for writes) and
closes it on every exit after opening. Validation of input precedes opening
for writes. A device-range violation is rejected before SET_TIME; finer
precision is passed intact for the driver to accept or reject without rounding.
Metadata and status are observations, not an atomic promise about a later call:
an intervening driver change is handled by propagating GET_TIME/SET_TIME errors.

No global UTC cache, background worker, polling, clock discipline or hardware
port is introduced. Network/GNSS sources may call SET after validating samples;
the application decides which source to trust and when to accept it for TLS.
