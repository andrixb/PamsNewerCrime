# ADR-0002: `make proof` — domain/app compile with no Arduino

- Status: Accepted
- Date: 2026-10-13

## Context

"Arduino-free" as a comment convention rots: one stray `#include <Arduino.h>`
or use of `micros`/`digitalRead` in `app/` silently re-couples the engine to
hardware and breaks host testing.

## Decision

`make proof` compiles `domain/` + `app/` with `-Isrc` only — **no Arduino
headers, no `test/mock` on the include path** — under
`-Wall -Wextra -Werror -fsyntax-only`. Any leak fails the build. `make test`
runs `proof` first, and CI runs `make test`, so the rule is enforced on every
PR. `infra/hal_storage.cpp` is deliberately excluded (it binds `<EEPROM.h>`).

Supporting conventions:

- Includes are **src-rooted** (`#include "app/channel.h"`), never `../`.
- `pam_config.h` stays Arduino-free (`stdint` + macros; `A5`/`A4` expand only
  in firmware TUs that include `<Arduino.h>` themselves).
- `constrain()` is guarded on both sides — never redefine it.
- New code must be warning-clean under proof flags. Real example: a
  `uint32_t` temp holding `(int32_t)mv - (int32_t)a` made `t < 0` always
  false (`-Werror=type-limits`); signed temporaries must be `int32_t`.

## Consequences

- `domain/` + `app/` are genuinely portable C++17; mocks exist only for the
  test link, never for the layer check.
- Proof failures are build errors, not review comments.

## Agent notes

If `make proof` fails on your change, do not weaken the flags or add mock
include paths — remove the Arduino dependency from the code instead. Reach
for `EngineEnv`-style explicit parameters (ADR-0003) or a port (ADR-0005).
