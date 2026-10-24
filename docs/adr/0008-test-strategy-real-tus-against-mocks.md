# ADR-0008: Tests build the real TUs against mocks

- Status: Accepted
- Date: 2026-10-13

## Context

Duplicated-logic tests prove nothing: a copy of the engine in `test/` can
pass while the firmware is broken. The suite must exercise shipped code.

## Decision

- `make test` compiles the **real production TUs** (`domain/euclid.cpp`,
  `domain/scales.cpp`, `app/clock.cpp`, `app/channel.cpp`,
  `app/serialization.cpp`, `app/patch.cpp`, `app/menu_model.cpp`,
  `infra/hal_storage.cpp`) on the host against tiny port fakes
  (`test/mock/Arduino.h`, `test/mock/EEPROM.h`, `test/mock_hw.cpp`).
- Assertions use the `CHECK(cond)` macro in `test/test_main.cpp`, run through
  public APIs only — no production logic duplicated in tests. Current gate:
  `2368 checks, 0 failures` (count grows; never regress).
- Coverage anchors: Euclidean reference patterns + property sweep, engine
  modes, clock/swing math, `StoreBlob` golden bytes + round-trip +
  version rejection, menu get/set/label suites.

## Consequences

- A behavior change without a test change is a red flag in review; a test
  change that re-implements engine math instead of calling it is a defect.
- CI runs `make test` (proof included) before both Teensy builds, so engine
  regressions fail the pipeline with no board attached.

## Agent notes

When touching engine/storage/clock/menu semantics, extend
`test/test_main.cpp` through the public API and keep `make test` fully
green. If a correct change breaks the golden blob test, stop — the format
decision (ADR-0004) needs an explicit user decision, not a test edit.
