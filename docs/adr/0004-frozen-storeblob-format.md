# ADR-0004: `StoreBlob` on-device format is frozen

- Status: Accepted
- Date: 2026-10-13

## Context

Patches persist to EEPROM (active slot + banks A/B, autosave after ~3 s
idle) and must survive firmware updates. A silent layout change corrupts or
orphans every saved patch on every device in the field.

## Decision

- `StoreBlob` (`src/app/serialization.h`: magic/version/CRC + `MasterPersist`
  + `ChannelParams ch[NUM_OUTPUTS]`) is a **frozen format**. Field order,
  sizes, slot stride (512 bytes), and slot geometry must not change.
- A golden test (`storage: blob golden` in `test/test_main.cpp`) pins the
  serialized bytes exactly; any layout change breaks it first, by design.
- `capture_state` / `restore_state` / `blob_verify` own the live-state ↔ blob
  mapping; bank/autosave use cases live in `src/app/patch.cpp`.
- Version rejection is tested: blobs with unknown versions are refused, not
  migrated silently.

## Consequences

- Adding a parameter means fitting it into the existing layout discipline
  (reserved/padding-aware) with an explicit migration decision — never a
  quiet struct edit.
- Round-trip (`storage: round-trip` suite) must stay green.

## Agent notes

Do not touch `serialization.h` structs, `STORE_SLOT_STRIDE`, or slot order
without an explicit user request. If asked to evolve the format, bump
`version`, add golden coverage for old-version rejection, and document the
migration in a new ADR that supersedes this one.
