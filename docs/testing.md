# Validation — 2026-10-16

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

STM32F7 library size: 5104 bytes DMF, 4054 bytes compressed DMFC. It has no
non-system runtime module dependencies, hardware port or driver registration.

- `dmutc.dmfc` SHA-256: `8f535b6495c99365c2ba8133cc5086b27e87177faff02f2c6912eb7fe046ba67`
- `test_dmutc.dmfc` SHA-256: `7ddab5049ac8b5894a577415d3ccf41975c15ee7b01b321b40c64db869d8e74f`
