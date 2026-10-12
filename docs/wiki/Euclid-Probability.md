# Euclid & Probability

Once the [ratio](Clock-Ratios.md) says *when* a channel's events land, Euclid and
probability decide **which of those events actually fire**. They stack: euclid
first, then probability, and only surviving events reach the mode.

An event that does **not** fire is a real rest — for stepped apps (`step`, `S&H`)
the value holds; for `env` there is no retrigger; for `pulse`/`logic` the gate
just stays low.

## Euclidean rhythm

Euclid spreads `fill` pulses as evenly as possible across `length` slots
(Bjorklund), turning an even stream into a syncopated pattern.

| Param | Range | Role |
|-------|-------|------|
| `euclid` | on/off | Enable it (off = every event fires, subject to probability). |
| `eu length` | 1–32 | Slots in the pattern. |
| `eu fill` | 0–length | Pulses placed in those slots. |
| `eu rot` | 0–length−1 | Rotate the pattern — shifts the accents without changing the shape. |
| `eu loop` | 0 / 1–32 | Stretch the cycle past `length` (0 = off). A `3-in-8` pattern with `loop 16` plays the tresillo then 8 rests, realigning every 16 steps — the trick for resetting non-4/4 grooves against a 4/4 grid. |

Examples (length / fill):

- `8 / 3` → the classic `x . . x . . x .` tresillo feel.
- `16 / 5` → a sparse, evenly-spread tech pulse.
- `7 / 4` against a `1/8` ratio → an odd-meter pattern that still lines up with
  everything else on the shared clock.

The pattern advances on the **event count**, not on the audio clock, so the euclid
loop is locked to the channel's own ratio. Raise the ratio (faster events) and
the euclid sequence speeds up with it; change `ratio` and the pattern re-frames.

> The `euclid` group only appears for **gate-based** apps (`pulse`, `step`, `S&H`,
> `env`, `note`, `logic`, `rwalk`). Continuous apps like `LFO`/`ramp`/`woggle`
> run off the position, not discrete events, so they skip the euclid group.

## Probability

`prob` (0–100 %) is an independent coin flip on each event that Euclid already
let through.

- `100` — always fires (off).
- `50` — roughly every other event, randomly — good for generative, breathing
  patterns.
- Low values turn a busy channel into sparse, unpredictable punctuation.

Probability is re-rolled per event, so a `step` sequence still walks forward on
the events that *do* fire (the held value just persists through the skips).

## Per-step probability (the `step` app)

On top of the channel-wide `prob`, the `step` app gives **each step its own**
trigger probability (`s1 prob … s16 prob`, 0–100 %, default 100). When a firing
event lands on a step whose roll fails, the step is *skipped*: the index still
advances but the gate stays low and the previous value holds — a per-hit rest for
breaking up a fixed sequence without touching its notes. Set one step to 0 % to
silence it, or 50 % to make it appear at random.

## Combining them

Set `ratio` for the grid, `euclid` for the groove, and `prob` for looseness —
together they’re the main generative lever, and every one of them can itself be
CV-controlled (see [CV Modulation](CV-Modulation.md), whose `prob` and `ratio`
targets are made for exactly this).

---

Prev: [Clock & Ratios](Clock-Ratios.md) · Next: [CV Modulation](CV-Modulation.md)
