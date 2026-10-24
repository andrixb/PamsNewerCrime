# ADR-0001: Clean layers `domain → app → infra`

- Status: Accepted
- Date: 2026-10-13 (records the completed refactor, see
  `docs/refactor-clean-architecture.md`)

## Context

`src/` was flat: engine, clock, UI, storage, and drivers coupled through
globals and `<Arduino.h>`. Changes to UI or persistence risked engine
behavior, and nothing was host-testable without Arduino headers.

## Decision

Split `src/` into three layers with one-way dependencies:

- `domain/` — pure vocabulary and utils (`channel_params.h`, `scales.*`,
  `euclid.*`). No Arduino, no hardware, no I/O.
- `app/` — engine and use cases (`channel.*`, `clock.*`, `patch.*`,
  `serialization.*`, `menu_model.*`, `storage_port.h`). Testable natively,
  Arduino-free (ADR-0002).
- `infra/` — the ONLY layer touching Arduino/HW (`dac.*`, `hal_input.*`,
  `hal_storage.*`, `ui.h`, `display/`).
- `main.cpp` — composition root: wiring, ISR, UI loop. Logic-free.
- `pam_config.h` — Arduino-free by design (`stdint` + macros only).

Rule: **`main → infra → app → domain`. Nothing includes upward.**

## Consequences

- Engine/storage/clock/menu logic is unit-testable on the host (ADR-0008).
- Hardware changes stay in `infra/`; behavior changes stay in `app/`/`domain/`.
- Known deviation (accepted): `Channel` runtime state lives in `app/channel.h`
  rather than a separate `channel_state.h` — it already separates `ChannelParams`
  (domain) from state cleanly. Do not "fix" this unprompted.

## Agent notes

Put new code in the innermost layer that can hold it. Testable logic belongs
in `app/`, never in `infra/display/`. If a change needs an upward include,
the layering is wrong — restructure instead.
