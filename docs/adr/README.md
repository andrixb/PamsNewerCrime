# Architecture Decision Records (ADRs)

Accepted decisions that constrain how this codebase evolves. **Agents: read
the relevant ADR before changing the area it covers** — they explain *why*
the patterns in `AGENTS.md` exist, and which rules are load-bearing.

| ADR | Title | Touches |
|-----|-------|---------|
| [0001](0001-clean-layered-architecture.md) | Clean layers `domain → app → infra` | all of `src/` |
| [0002](0002-proof-gate-arduino-free-app.md) | `make proof`: domain/app compile with no Arduino | `Makefile`, `domain/`, `app/` |
| [0003](0003-pure-engine-and-output-handoff.md) | Pure engine + dirty-drain output handoff | `app/channel.*`, `main.cpp` |
| [0004](0004-frozen-storeblob-format.md) | `StoreBlob` on-device format is frozen | `app/serialization.*`, `app/patch.*` |
| [0005](0005-storage-port-pattern.md) | Storage port in app, adapter in infra | `app/storage_port.h`, `infra/hal_storage.cpp` |
| [0006](0006-menu-model-split.md) | Testable menu model in app, rendering in infra | `app/menu_model.*`, `infra/display/`, `infra/ui.h` |
| [0007](0007-isr-and-timing-constraints.md) | 1 kHz ISR rules, UI off the tick path | `main.cpp`, `infra/hal_input.*` |
| [0008](0008-test-strategy-real-tus-against-mocks.md) | Tests build the real TUs against mocks | `test/`, `Makefile` |

## Adding an ADR

New load-bearing decision? Copy this format: `NNNN-title.md` with
`Status / Context / Decision / Consequences / Agent notes`. Keep it short
(half a page). Link it from the table above and from `AGENTS.md` if it adds
a repo-wide rule. Never rewrite an accepted ADR in place — supersede it with
a new one.
