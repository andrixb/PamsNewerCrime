# ADR-0007: 1 kHz ISR rules, UI off the tick path

- Status: Accepted
- Date: 2026-10-13

## Context

`control_tick()` runs in ISR context at 1 kHz driving encoders, clock,
engine, and DAC. Anything blocking, allocating, or contending there causes
jitter or corruption on hardware that no host test can catch.

## Decision

- On the tick path: **no dynamic allocation, no virtual dispatch, no
  mutexes**. "DI" means compile-time wiring (globals/templates), not runtime
  containers.
- Cadence is fixed in `main.cpp`: `control_tick()` polls inputs
  (`ui_service`), samples CV every 8th tick, ticks `masterClock`, runs the
  pure engine (ADR-0003), and flushes dirty DAC outputs. Display drawing
  (`ui_draw`, ~30 Hz), pending UI work, and patch autosave run in `loop()`,
  never in the ISR.
- UI respects the split: encoder/button state is latched and applied without
  the ISR touching render state.

## Consequences

- Engine and input code must be bounded-time and allocation-free; keep lookup
  tables static const and state in fixed structs.
- Timing behavior (swing, external-clock smoothing, flex offsets) is owned by
  `app/clock.*` + engine, not by ISR scheduling tricks.

## Agent notes

Do not add `new`/`malloc`, virtual calls, or blocking waits to anything
reachable from `control_tick()`. Expensive or bursty work (drawing, EEPROM
writes, serial logging) belongs in `loop()` or behind a dirty flag.
