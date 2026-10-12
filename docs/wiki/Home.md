# Pam's Newer Crime — Wiki

*Pam's Newer Crime* (PNC) is a programmable, clocked modulation source for
Eurorack, running on Teensy 4.0 / 4.1 as a drop-in for genuine **O_C** hardware.

Start with the [Overview](Overview.md), then drill in:

| Page | What it covers |
|------|----------------|
| [Overview](Overview.md) | The big picture: how the clock, outputs and modulation fit together |
| [Getting Started](Getting-Started.md) | Hardware, flashing the firmware, first patch |
| [Controls & UI](Controls-UI.md) | The two encoders, two buttons, pages and the app menu |
| [Modes](Modes.md) | Every app in detail: pulse, LFO, ramp, step, S&H, note, env, logic, woggle, rwalk |
| [Parameters](Parameters.md) | Every parameter, what it does, its range, and which group it lives in |
| [Clock & Ratios](Clock-Ratios.md) | Master clock, external sync, the 24 musical ratios, phase, delay |
| [Euclid & Probability](Euclid-Probability.md) | Euclidean rhythm generation and per-event probability |
| [CV Modulation](CV-Modulation.md) | Assigning CV inputs / other outputs to modulate any channel's params |
| [Storage & Banks](Storage-Banks.md) | Autosave, save/load banks A/B, what persists |
| [Building](Building.md) | Compiling from source with PlatformIO |

---

This wiki lives in `docs/wiki/` in the repository. To publish it as a real
GitHub Wiki, copy the `docs/wiki/*.md` files into the repo's `*.wiki.git`
repository (GitHub exposes a wiki's git remote on the **Wiki** tab); the
cross-links above use relative Markdown links so they also render fine on the
plain `docs/wiki/` folder on GitHub.
