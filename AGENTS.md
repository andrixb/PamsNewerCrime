# AGENTS.md — AI-agent guide to Pam's Newer Crime (PNC)

This is the single source of truth for AI coding assistants working in this
repo (OpenCode, Claude Code, Cursor, Copilot, Gemini, …). Thin pointers in
`CLAUDE.md`, `GEMINI.md`, and `.github/copilot-instructions.md` redirect here
so the guidance never drifts between files.

## What this project is

PNC is a programmable, clocked modulation source for eurorack, running on
**Teensy 4.0 / 4.1 as a drop-in for genuine O_C hardware** (quad DAC8565,
SH1106 OLED, 2 encoders + 2 buttons, TR1 clock in, 2 CV ins, up to 4 CV outs).
Implementation is original, MIT-licensed. Full user docs live in
`docs/wiki/` (start at `docs/wiki/Home.md`); `README.md` is the overview.

## Commands (run these)

```bash
make test                 # REQUIRED gate: layer proof + native unit tests
make proof                # domain/+app/ compile with NO Arduino/mock on include path (-Werror)
pio run -e teensy40       # firmware build (default env)
pio run -e teensy41       # second hardware target
make flash                # build + flash connected Teensy (bootloader mode first)
make monitor              # 115200 serial console
make size                 # flash/RAM report
```

- `make test` runs `proof` first, then builds `test/test_main.cpp` plus the
  **real** production TUs against mocks and executes `build/test_pnc`.
  Expect `2368 checks, 0 failures` (count grows as tests are added).
- CI (`.github/workflows/build.yml`) runs `make test`, then both Teensy
  builds, on every PR/push; `v*` tags additionally cut a GitHub release and
  publish hexes to `docs/firmware/` for the web flasher. **Never commit
  hexes or `docs/firmware/` files by hand** — CI owns that directory.
- `pio` is resolved from `PATH` or `~/.platformio/penv` (see `Makefile`).

## Architecture (clean layers, one-way dependencies)

```
src/
  domain/   # pure: channel_params.h, scales.*, euclid.* — no Arduino, no HW, no I/O
  app/      # engine + use cases: channel.*, clock.*, patch.*, serialization.*, menu_model.*, storage_port.h
  infra/    # ONLY layer touching Arduino/HW: dac.*, hal_input.*, hal_storage.*, ui.h, display/
  main.cpp  # composition root: wiring, 1 kHz ISR, UI loop — logic-free
  pam_config.h  # Arduino-free by design (stdint + macros only; A5/A4 expand in firmware TUs)
test/
  test_main.cpp       # suites using a CHECK(cond) macro; no production logic duplicated
  mock_hw.cpp mock/Arduino.h mock/EEPROM.h   # port fakes for host builds
```

**Dependency rule: `main → infra → app → domain`. Nothing includes upward.**
`domain/` + `app/` must compile with `-Isrc` only — any `#include <Arduino.h>`
leak or use of a mock-only symbol (`micros`, `digitalRead`, …) fails
`make proof` (`-Wall -Wextra -Werror -fsyntax-only`). `infra/hal_storage.cpp`
is intentionally excluded from proof (it binds `<EEPROM.h>`).

Key engine facts:

- `channels_update(delta, cv1, cv2, env)` in `src/app/channel.cpp` is pure
  computation (no I/O); `main.cpp` passes an explicit `EngineEnv` (incl.
  `user_scales`) and flushes dirty outputs via `channels_take_outputs()` +
  `dac_write()`. The engine never reads globals like `USER_SCALES` directly.
- `control_tick()` runs at **1 kHz in ISR context: no dynamic allocation, no
  virtual dispatch, no mutexes** on that path. "DI" here means compile-time
  wiring, not runtime containers.
- `StoreBlob` (`src/app/serialization.*`) is a **frozen on-device format**
  (magic/version/CRC). Never change its layout; a golden byte-identical test
  pins it (`storage: blob golden` suite).

## Decisions (ADRs — read before changing an area)

`docs/adr/` records the load-bearing decisions behind the patterns above.
Read the relevant record before touching its area:

| Area | ADR |
|------|-----|
| layers, dependency rule | [0001](docs/adr/0001-clean-layered-architecture.md) |
| Arduino-free app, proof flags | [0002](docs/adr/0002-proof-gate-arduino-free-app.md) |
| engine inputs/outputs | [0003](docs/adr/0003-pure-engine-and-output-handoff.md) |
| persistence format | [0004](docs/adr/0004-frozen-storeblob-format.md) |
| hardware access from app | [0005](docs/adr/0005-storage-port-pattern.md) |
| menus / UI changes | [0006](docs/adr/0006-menu-model-split.md) |
| ISR / timing code | [0007](docs/adr/0007-isr-and-timing-constraints.md) |
| adding tests | [0008](docs/adr/0008-test-strategy-real-tus-against-mocks.md) |

## Conventions to follow

- Includes are **src-rooted**: `#include "app/channel.h"`, never relative `../`.
- Language: C++17 (`-std=gnu++17`), `-Wall -Wextra`; proof adds `-Werror`.
  Keep new code warning-clean under proof flags (recent real example: a
  `uint32_t` temp holding a signed subtraction made `t < 0` always false —
  use `int32_t` for signed temporaries).
- `NUM_OUTPUTS=4` for host builds (`-DNUM_OUTPUTS=4`); valid range 1–8, but
  O_C hardware caps at 4 (enforced by `#error` in `pam_config.h`).
- `constrain()` comes from Arduino on-device and from `pam_config.h` on host;
  both are guarded — do not redefine it.
- UI menu tables live in `app/menu_model.*` (pure, natively tested);
  `infra/display/` only renders. Keep that split: testable logic in `app/`.
- History/context: `docs/refactor-clean-architecture.md` records the
  completed domain→app→infra split and known as-built deviations — read it
  before restructuring anything.

## How to make a change (agent workflow)

1. Read the relevant `docs/wiki/*.md` page for intended behavior.
2. Locate code via the table above; prefer editing existing files over adding
   new ones. Keep `domain`/`app` Arduino-free.
3. If touching engine/storage/clock semantics, add or extend a suite in
   `test/test_main.cpp` through the public API (never by duplicating logic).
4. Run `make test` (includes proof) — it must be fully green before you stop.
   For hardware-touching changes also run both `pio run -e teensy40` and
   `pio run -e teensy41` if a toolchain is available.
5. Do not change `StoreBlob` layout, pin maps in `pam_config.h`, or CI
  publishing steps without an explicit user request — these are load-bearing.
