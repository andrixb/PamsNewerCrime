# Copilot instructions

Follow [AGENTS.md](../../AGENTS.md) — it is the single source of truth for
AI agents in this repo (layers, commands, constraints).

Essentials: `make test` is the required gate (proof + native tests, green
before stopping); `domain/` + `app/` must stay Arduino-free (`make proof`
enforces it); `src/` includes are root-based (`app/channel.h`); never change
the `StoreBlob` layout, O_C pin map, or CI hex-publishing without being asked.
