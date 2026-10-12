# FLEX & Cross-ops

Two Pro-style systems that sit **between** the event engine and the DAC output:
**FLEX** warps *when* events land, **cross-ops** combine this output's *value*
with another signal. There is also a **global quantiser** post-filter. These are
per-channel features available to (almost) every app.

---

## FLEX micro-timing (the `flex` group)

`flex` picks a timing distortion, `flex amt` (0–255) sets its intensity. FLEX
runs on the channel's **event grid**, pushing output off the strict clock for a
more human — or more machine-gun — feel.

| `flex` | What it does | `flex amt` means |
|--------|--------------|------------------|
| `off` | straight grid | — |
| `human` | each event fires a random amount **late** ("slop") | max lateness, 0–255ths of a cycle |
| `swing` | every **other** event is delayed — shuffle/swing | delay on odd events, halves of a cycle |
| `accel` | rate climbs event-by-event, resets every 8 (a burst/ritardando ramp) | top speed-up (255 ≈ 4×) |
| `hump` | sinusoidal tempo bulge in the middle of each cycle | how fast the middle is |
| `fall` | gravity: accelerate through the cycle, snap back on the next event | fall depth |

Notes:
- `human`/`swing` shift the whole channel phase for that cycle, so they affect
  gate apps cleanly and add a lilt to continuous ones.
- `accel`/`hump`/`fall` scale the channel's own phase *rate* (off-grid tempo
  modulation); they stay phase-locked to the master on every cycle boundary.
- The **master swing** on the M page (see [Clock & Ratios](Clock-Ratios.md)) is a
  different, global thing: it warps the entire clock's eighth-note grid for every
  channel at once, while keeping quarter-note boundaries sample-locked.

## Cross-ops (the `cross` group)

`x op` combines this channel's final value with a source (`x src`: `CV1`, `CV2`,
or `out1…out4`). It applies to **any** app, after the shape/range/invert stage
and before `lag`, so it mixes CV the way the Pro's cross-output modulation does.

| `x op` | Result |
|--------|--------|
| `--` | off |
| `mix` | average of this output and the source (a 2-input mixer) |
| `mask` | pass this output only while the source is above ~0.1 V, else 0 |
| `not` | the inverse of mask (silence while the source is high) |
| `mult` | multiply (ring-mod / attenuverter feel; 0 V source ⇒ silence) |
| `hold` | freeze this output's value while the source is high |
| `s&h` | sample-and-hold: track the source, snapping on each rising edge |
| `add` | sum, clamped to 0–10 V (mixer/offset) |
| `sub` | difference, clamped at 0 |
| `and` | both high → pass, else 0 (logic on gate levels) |
| `or`  | source high → the louder of the two |
| `xor` | exactly one high → pass, else 0 |
| `seed` | a rising source edge **re-seeds the RNG** (regenerates random/rwalk/woggle) |

### Idea starters

- `note` masked by a `pulse` from another channel → only play notes on beats.
- `LFO` `mult` by CV1 → patch-cable VCA, no cable needed.
- `rwalk` `seed`ed by an `env` gate → a fresh random melody on every hit.
- Two `ramp`s in `mix` → a phasey composite sweep.

## Global quantiser (`quantise`, in the `range` group)

When on, the channel's **final voltage** is snapped to the nearest step of
`scale` across its `cv min`..`cv max` window — this works on *any* app, not just
`note`. Feed a raw `woggle`, `LFO` or `step` through it and chaos becomes
in-key. See [Modes](Modes.md#scales) for the scale list and the three editable
user scales (edited on the SYS page).

---

Prev: [Euclid & Probability](Euclid-Probability.md) · Next: [CV Modulation](CV-Modulation.md)
