# ADR-0005: Storage port in app, adapter in infra

- Status: Accepted
- Date: 2026-10-13

## Context

Persistence needs EEPROM on-device but must stay host-testable. Letting
`app/` include EEPROM (or Arduino) would break ADR-0002.

## Decision

- `src/app/storage_port.h` declares the transport port
  (`storage_port_read` / `storage_port_write`) in terms of plain
  `offset/dst/n` — no EEPROM types, no Arduino.
- `src/infra/hal_storage.cpp` implements the port against `<EEPROM.h>`.
  It is the one app-adjacent TU excluded from `make proof`.
- Direction of dependency: **infra depends on app** (`hal_storage.cpp`
  includes the port); app→infra includes stay at zero.

## Consequences

- Serialization and patch use cases test natively against a fake transport;
  only the thin adapter touches real EEPROM.
- This is the template for any future hardware need in `app/`: declare a
  C-linkage-style port in `app/`, implement it in `infra/`.

## Agent notes

Need hardware access from `app/`? Add a port + infra adapter following this
shape — do not include `infra/` or Arduino headers from `app/`. Keep the
port narrow (bytes/offsets, no HW types) so fakes stay trivial.
