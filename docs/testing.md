# Service-driven RTC binding — 2026-10-16

The current implementation receives its source through dmdevfs RTC events,
libsystemd rules and a `type=library` unit. It has no default device path.

- Native ctest: 2/2 targets; DMOD conversion diagnostic 4/4 and provider model
  6/6, including multiple sources and STOP waiting for an in-flight read.
- libsystemd diagnostic: 62/62, including cold load, prior API enable,
  independent unit instances, failed START and refreshed event paths.
  systemd/service command diagnostics: 1/1 each.
- dmdevfs native suite: 10/10 targets, including RTC reporting with an unset
  calendar and report-mask filtering.
- Cortex-M4 and Cortex-M7 dmutc builds passed. libsystemd and dmdevfs M7 builds
  passed. No execution on M4 is claimed.
- STM32F746G-DISCO after flashing/reset: `dmutc@rtc` automatically running,
  type=library, pid=0. dmutc 6/6, RTC 12/12, RNG 6/6.
- Hardware checks stop the UTC unit while the RTC still exists, confirm
  ENODEV, restart it, remove its device notification, reject a re-add with a
  nonexistent path, then bind the actual path from a fresh event. The logged
  open error for `/no-such-utc-source` is an intentional negative test.
- The harness sets PC UTC before the round-trip tests and restores it after
  all diagnostics. RTC text reads and DHCP were checked afterwards. These
  checks do not establish authenticated time or oscillator accuracy.

[Current board console](validation/stm32f746g-disco-service-2026-10-16.txt).

Validation uses local build manifests for the new libsystemd lifecycle header
and unreleased modules. They are build-environment files, not source dependency
pins. Public CI needs the libsystemd change released before dmutc can resolve
that header from the package registry. Boot installs the packaged rule and
service through the normal service/rules tags.

Current Cortex-M7 artifacts:

- `dmutc.dmfc`: 5076 bytes; SHA-256 `2e0878df76e38b3c76955551e8ea6acc45cb44f4f0635eead3df6c4cdb66b2d4`.
- `test_dmutc.dmfc`: 6980 bytes; SHA-256 `2d5a4493e3e8071b145723d01ed9c0b67b6d2abb0c32ae19d90dabc426fbb5c6`.

# Initial conversion and explicit-path implementation — 2026-10-16

- Native ctest: 2/2 targets. The actual DMOD diagnostic passes 4/4 steps for
  known epochs, negative fractional times, leap centuries, invalid input,
  overflow and month transitions throughout years 1..9999.
- The actual RTC provider compiled against an independent VFS/RTC model passes
  4/4 scenarios: clocks outside STM32 limits with nanoseconds, unset/stopped/
  loss-detected states, read-only/range/precision behavior, errors, malformed
  calendar/metadata, output clearing and handle closure on all failure paths.
- An independent Python datetime oracle agrees with 10,000 seeded random
  calendar/epoch round trips across years 1..9999, including weekday and ns.
- Cortex-M4 and Cortex-M7 builds passed. No hardware execution on M4 is claimed.
- STM32F746G-DISCO: test_dmutc 5/5, RTC regression 12/12, RNG regression 6/6.
  UTC read/write traverses the real VFS/RTC interface; fractional writes beyond
  RTC precision return ERANGE. The test restores its saved clock.
- The board initially had an unset calendar; GET returned ENODATA. The board
  round-trip diagnostic requires an initialized RTC to preserve/restore time,
  so the harness sets PC UTC before running it and restores PC UTC afterwards.
  This does not claim authenticated UTC or measured oscillator accuracy.
- Flashed via OpenOCD with Verified OK; cat /dev/rtc and DHCP checked afterwards.

[Board console evidence](validation/stm32f746g-disco-2026-10-16.txt).

The firmware validation image stages dmutc and its diagnostic locally; no boot
configuration change or development-machine package path is committed. Public
package headers resolve dmdrvi 2.8 after refreshing a stale dmf-get environment
cache. Source dependencies remain unversioned in dmod_link_modules.

STM32F7 library size: 5104 bytes DMF, 4054 bytes compressed DMFC. At that stage it had no non-system runtime module dependencies, hardware port
or driver registration. The service integration below supersedes path selection.

- `dmutc.dmfc` SHA-256: `8f535b6495c99365c2ba8133cc5086b27e87177faff02f2c6912eb7fe046ba67`
- `test_dmutc.dmfc` SHA-256: `7ddab5049ac8b5894a577415d3ccf41975c15ee7b01b321b40c64db869d8e74f`
