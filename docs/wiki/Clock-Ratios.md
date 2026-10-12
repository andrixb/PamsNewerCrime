# Clock & Ratios

## The master clock

A single timeline drives everything. Its resolution is **96 pulses per quarter
note** (ppq), carried as a fractional position so slow ratios stay sample-exact.

Two sources, chosen on the **master page** → `clock`:

- **int** — internal, runs from the `BPM` value (10.00–330.00). Tap the left
  encoder (or set `run`) to start/stop.
- **ext** — locked to pulses on the **TR1** jack. Set `ext ppqn` (1–48) to match
  the source: 24 is the common Eurorack/“sync” rate, 1 fits some drum machines,
  48 for fine-grained clocks. PNC measures the incoming period, shows the derived
  tempo, and self-organises after ~4 missing pulses if the clock stops.

## Global swing (master page → `swing`)

Off by default. When raised (0–100 %, internal clock only), it warps the master
grid so every **odd eighth-note falls late**, pushing all channels into a shuffle
at once — the difference between straight and swung *feel* for the whole patch.

- It is a **sync-preserving phase warp**: the first half of each quarter advances
  slower and the second half faster, so quarter-note boundaries stay sample-exact
  and the pattern still realigns perfectly with any external beat.
- ~`33 %` ≈ triplet swing; `63 %`-ish is hard swing.
- This is separate from the per-channel `flex` (see
  [FLEX & Cross-ops](Flex-and-Cross.md)) — master swing bends the global grid,
  per-channel flex is off-grid humanising of one channel, and the two stack.

> On O_C hardware pin 0 is the TR1 jack, so there is **no MIDI input**; external
> sync is via the TR jack only.

## What a “ratio” is

Each channel divides the master timeline by its `ratio` to decide how often an
**event** happens. `ratio` is one of 24 values, from fast divisions to long
multiples:

```
1/64  1/13  1/32T 1/11  1/9   1/32  1/7   1/16T 1/32. 1/5   1/16  1/8T
1/16. 1/8   1/4T  1/8.  1/4   1/2T  1/4.  1/2   1/2.  1bar  2bar  4bar
```

- Plain fractions are **per-bar** (a bar = 4/4 = one quarter note × 4).
  `1/16` = 16 events per bar, `1bar` = one event per bar.
- `T` = triplet (¾ of the plain value), `.` = dotted (1½× the period).
- Odd/prime divisions (`1/5`, `1/7`, `1/9`, `1/11`, `1/13`) let a channel run
  against the grid for polyrhythms.

Because ratios share one clock, two channels set to `1/4` and `1/3` phase-lock
and realign every 12 steps — the core PNC trick for interlocking patterns.

> The `div` app ignores the ratio list entirely: it derives its period from two
> signed divide/multiply factors instead. See
> [Modes](Modes.md#div--clock-divider--multiplier).

## phase & delay — moving things in time

- **`phase`** (0–255) rotates *where in the cycle the channel starts*. Set two
  channels to the same ratio with a phase offset for a “rolling” figure.
- **`delay`** (0–255, on `pulse`/`logic`) shifts the *gate* after each event —
  this is how you get swing / humanised lag on a single channel without changing
  its timing relative to others.

Combine `phase` (channel A/B offset) and per-channel `delay` for shuffle feels.

## Resetting phase

Hold the **right encoder** anywhere to reset **all** channels to phase 0 at once
— a one-button “realign the whole patch”. Changing the clock source or loading a
bank also re-aligns.

---

Prev: [Parameters](Parameters.md) · Next: [Euclid & Probability](Euclid-Probability.md)
