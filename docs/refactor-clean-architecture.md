# Clean Architecture Refactor Plan

Goal: restructure `src/` into layered clean architecture (domain → application →
infrastructure) with one-way dependencies, without changing runtime behavior.

## Status

- [x] Phase 0 — safety net (`make test`: 1676 checks, 0 failures; golden `StoreBlob` test)
- [x] Phase 1 — directory split (`src/domain|app|infra`, src-rooted includes, `make test` + `build40` + `build41` green)
- [x] Phase 2 — decouple the engine (no `dac.h` in app; drained outputs;
  explicit `EngineEnv`/`default_ratio`; `make test` + `build40` + `build41` green)
- [ ] Phase 3 — storage split
- [ ] Phase 4 — split ui.cpp
- [ ] Phase 5 — composition root + verification

## Current state

Flat `src/`, all modules coupled through globals and `<Arduino.h>`:

| File | Role | Coupling problems |
|---|---|---|
| `channel.cpp` (551) | engine: modes, Euclid, cross-ops, flex | reads `masterClock` state, `DEFAULT_RATIO`, `USER_SCALES`; calls `dac_write` directly |
| `clock.cpp` (110) | master clock, int/ext tempo | global `masterClock`, owns swing policy |
| `ui.cpp` (1268) | display + input + app menus | mutates `channels[]`, `ChannelParams`, clock, storage directly; SH1106 SPI driver embedded |
| `storage.cpp` (74) | patch persist/banks | EEPROM + `ChannelParams` + `MasterClock` fields entangled |
| `dac.cpp` / `euclid.cpp` | DAC8565 driver / pure util | dac is infra (ok), euclid is pure (ok) |
| `main.cpp` (109) | wiring | 1 kHz `control_tick` ISR drives everything |

Already good: `test/` builds the real engine TUs natively against mocks (`make test`),
and `test/mock/{Arduino.h,EEPROM.h}` already act as port fakes. Keep that harness.

## Embedded constraints (do not fight these)

- `control_tick()` runs in ISR context at 1 kHz: **no dynamic allocation, no virtual
  dispatch, no mutexes** in that path. "Dependency injection" here means compile-time
  wiring (globals/templates), not runtime DI containers.
- RAM/flash budget on Teensy 4.x is generous, but keep the UI off the audio-rate path.
- `StoreBlob` is a persisted on-device format: **layout must not change** (magic/version/crc).

## Target layout

```
src/
  domain/                  # pure: no Arduino, no HW, no I/O
    channel_params.h       # ChannelParams, OutMode/XOp/FlexOp/LfWave/CvParam enums, Ratio
    scales.h(.cpp)         # SCALE_NAMES/masks, USER_SCALES data
    euclid.h(.cpp)         # (moved as-is)
    channel_state.h        # Channel runtime state struct
  app/                     # engine + use cases; testable natively
    engine.h(.cpp)         # channels_update(): pure fn of (state, params, clock delta, cv in)
    clock.h(.cpp)          # MasterClock tempo math (swing policy here, not in engine)
    patch.h(.cpp)          # use cases: load/defaults/copy, bank save/load, dirty tracking
    serialization.h(.cpp)  # StoreBlob <-> params (format-stable)
  infra/                   # the only layer that touches Arduino/HW
    hal_dac.h(.cpp)        # DAC8565 (from dac.cpp) + output sink used by engine
    hal_input.h(.cpp)      # encoders/buttons/ADC (extracted from ui.cpp)
    hal_storage.h(.cpp)    # EEPROM backend for serialization
    display/
      sh1106_spi.h(.cpp)   # PncSpiOled driver class
      renderer.h(.cpp)     # page drawing (from ui.cpp draw code)
      ui_controller.h(.cpp)# input events -> app use-case calls; nav state (page/param)
  main.cpp                 # composition root: wiring, ISRs, timers, setup/loop
  pam_config.h             # stays: pin map + build flags
```

Dependency rule: `main → infra → app → domain`. Nothing includes upward.
`domain/` and `app/` must compile natively **without** `test/mock` on the include path
(this is the measurable definition of "clean" for this project).

## Phases

Each phase ends green: `make test && make build` (and `make build41`), zero behavior change,
separate commit/PR.

### Phase 0 — Safety net
- Expand `test/test_main.cpp` to pin down current behavior that the refactor touches:
  `channels_update` outputs for a few representative modes, autosave blob round-trip
  (`capture_state`/`restore_state`), clock delta math, swing.
- Add golden test: serialize a default patch, assert byte-identical `StoreBlob` (guards format).

### Phase 1 — Directory split, mechanical moves
- Create `domain/ app/ infra/`, move files with `git mv`, update includes. No logic changes.
- Move pure data (enums, `ChannelParams`, ratio/scale tables) into `domain/`.
- `euclid.cpp/h` → `domain/` untouched otherwise.

### Phase 2 — Decouple the engine (app layer)
- Split `Channel` into `ChannelParams` (domain) and `ChannelState` (app) if entangled.
- `channels_update()` takes explicit inputs: `(state[], params[], delta_x256, cv1, cv2, swing)`
  — remove its reads of `masterClock` globals; `main.cpp` passes the values.
- Engine no longer calls `dac_write` directly: it produces `out_mv[]`, `main.cpp`/`hal_dac` flushes.
- Move `DEFAULT_RATIO`, `USER_SCALES` access behind an app-level `Patch`/`Globals` struct
  passed explicitly.

### Phase 3 — Storage
- `serialization.cpp`: `StoreBlob <-> {master fields, ChannelParams[]}` + dirty flag.
- `hal_storage.cpp`: EEPROM read/write/verify only. Format frozen (Phase 0 golden test).
- `patch.cpp`: use cases (defaults, bank A/B save/load, active autosave) called by UI/main.

### Phase 4 — Split ui.cpp (biggest win, 1268 lines)
- `infra/display/sh1106_spi.cpp`: extract `PncSpiOled` class + `display_ok`.
- `infra/display/renderer.cpp`: all draw functions; reads a small `UiSnapshot` struct
  (page, params, bpm, running...) — never mutates engine state.
- `infra/hal_input.cpp`: `PhzEncoder`/`PhzButton`, encoder pin reads, ADC smoothing
  (currently in `main.cpp control_tick`).
- `infra/display/ui_controller.cpp`: nav state machine + input events → calls into
  `app/` use cases (set param, change page, save bank...). All the `mode_groups`/
  `app_param_at` menu-table logic goes to `app/` as pure, unit-testable code.

### Phase 5 — Composition root + verification
- `main.cpp`: setup, ISR wiring, schedule (1 kHz tick engine, ~30 Hz UI, autosave)
  only. No logic.
- Add native compile check: `domain/`+`app/` built with `-Isrc -Werror` and **no** mock dir;
  any `Arduino.h` leak fails the build. Wire into `make test` / CI.
- Hardware smoke: boot splash, screensaver, all 13 modes, banks, autosave across power cycle.

## Risks / non-goals

- **No behavior redesign** in this refactor (mode semantics, UI wording stay identical).
- Do not introduce RTOS tasks/queues; keep the timer-driven cooperative model.
- Biggest regression surface: Phase 4 UI input semantics (encoder accel, long-press,
  screensaver wake). Mitigate by moving code verbatim-first, cleaning up second.
- ISR safety review at end of each phase touching `control_tick` path.
