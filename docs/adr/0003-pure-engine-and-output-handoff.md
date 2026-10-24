# ADR-0003: Pure engine + dirty-drain output handoff

- Status: Accepted
- Date: 2026-10-13

## Context

The engine used to read clock/scale globals directly and call `dac_write`
itself, so it could not run on the host and every engine test needed
hardware-adjacent setup.

## Decision

- `channels_update(delta, cv1, cv2, env)` (`src/app/channel.cpp`) is **pure
  computation**: explicit tick delta, resolved CV bytes, and an `EngineEnv`
  carrying what it needs (currently `user_scales`). It never reads globals
  like `USER_SCALES` and performs **no I/O**.
- Output handoff is a dirty-drain: the engine sets `out_mv` + `out_dirty`;
  `main.cpp` collects via `channels_take_outputs(outs)` (returns a bitmask)
  and `infra` emits via `dac_write()`. `app/` includes no `dac.h`.
- CV inputs arrive pre-resolved as 0–255 bytes (`hal_input_cv1/cv2`); ADC
  smoothing and inversion live in `infra/hal_input.cpp`, not the engine.

## Consequences

- The full engine runs natively in tests through its public API.
- Only one writer drives the DAC per tick; output batching is explicit.

## Agent notes

New engine inputs must arrive as parameters (extend `EngineEnv` or the
`channels_update` signature) — never by reaching into `infra` state or
globals. New outputs must go through the dirty-drain, not direct DAC calls.
Keep `control_tick()`-reachable code allocation-free (ADR-0007).
