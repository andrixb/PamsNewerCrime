# Modes (the apps)

Pick an app per output from the [app menu](Controls-UI.md#the-app-menu). Each app
turns the channel's clock **events** and **position** into an output shape. Every
app shares the `clock` group (`ratio`, `phase`, `prob`), the
[`flex`](Flex-and-Cross.md) and [`cross`](Flex-and-Cross.md) groups, the `range`
group (`cv min`, `cv max`, `invert`, `lag`, `quantise`, `scale`) and optional
[CV modulation](CV-Modulation.md); the tables below list only each app's own
**shape** parameters.

A common thread: an *event* is one cycle of the channel's `ratio`. Some apps use
whether the event **fires** (after [Euclid & Probability](Euclid-Probability.md))
to trigger; others run continuously as a waveform and just use the position.

---

## pulse — clocked gate / trigger

The workhorse: a gate pulse on every firing event.

- **Shape params:** `width` (gate length, % of the cycle), `delay` (swing / offset).
- Output is high for `width` of each cycle, starting `delay` after the event.
- Fires on the ratio, subject to euclid & probability.
- Use `cv min`/`cv max` to set the gate low/high levels (e.g. 0 → 5 V for triggers).
- **Logic** of triggers: pulse gates keep their edges sharp — the `lag`/slew
  filter is deliberately bypassed for `pulse` and `logic`.

## LFO — continuous waveform

A cycling voltage, good for vibrato, filter sweep, or slow movement.

- **Shape params:** `wave` (`sine`, `tri`, `saw`, `sqr`, `trapz`, `parab`),
  `skew` (shape: pulse-width for `sqr`, rise-point for `tri`, ramp width for
  `trapz`; ignored by `parab`, which is a fixed arch).
- Runs continuously off the channel position (not just on events); `ratio`
  sets how many cycles per bar (from sub-audio to many Hz).
- The output gate flag goes high whenever the wave is above mid-scale (handy to
  drive a second trigger).

## ramp — rising / falling / triangle

- **Shape params:** `skew` — the point in the cycle where the ramp turns around
  (128 = triangle; low = slow rise / fast fall; high = fast rise / slow fall).
- Continuous like the LFO; `ratio` sets the speed.

## step — sequence

A stepped CV sequencer.

- **Shape params:** `seq len` (1–16) and the individual `step 1 … N` values.
- Advances one step on every firing event; holds the step's value between events.
- Each `step` value is 0–255, mapped across the channel's `cv min`..`cv max`.
- Gate pulse on each step change (so it also clocks things downstream).

## S&H — sample & hold (random)

- **Shape params:** none (pure clock group).
- On each firing event it grabs a new random value and holds it until the next.
- Great for random CV / melodic chaos; feed `cv1 from` a slow ramp and you get
  random-stepped voltages.

## note — quantized sequence / melodic ramp

A continuous voltage **quantized to a musical scale**, with a gate pulse on each
note change.

- **Shape params:** `scale` (see the scale list below).
- Sweeps its range and snaps to scale steps, so a slow LFO-rate `ratio` gives a
  glissing quantized line, a fast one gives an arpeggio-like note stream.
- **Gate** fires whenever the quantized note changes → pair with a VCA/voice.
- **Scale:** any of the 29 built-in scales or 3 user scales — see
  [Scales](Modes.md#scales) below.

## env — clock-synced AD envelope

- **Shape params:** `attack`, `decay` (each 0–255, ≈ 1 ms to 8.1 s).
- Retriggered on every **firing** event (so it respects euclid & probability).
- Rises to the top of the range over `attack`, falls to the bottom over `decay`;
  gate is high for the envelope's duration.

## logic — combine two channels' gates

- **Shape params:** `logic` (`and`, `or`, `xor`, `nand`, `nor`), `lg src`
  (which other output to combine with, 0 = none).
- Computes the chosen operation between **this** channel's pulse (it still
  behaves like `pulse`, with its own `width`/`delay`) and the source channel's
  current gate, and outputs the result as a gate.

## woggle — chaos wave

- **Shape params:** `skew` (chaos — chance to jump randomly), `width` (speed).
- A wogglebug-style wandering voltage: integrates in a direction until it hits a
  rail or takes a random jump; jumps are more likely on firing events.
- Gate flag = the current walk direction.

## rwalk — random walk

- **Shape params:** `width` (step size).
- A bounded random walk: on each firing event it steps up or down by a small
  amount and holds. Predictable, gentle randomness.

## div — clock divider / multiplier

A Phazerville-style dual-stage divider that bypasses `ratio` and defines its own
period from two signed factors:

- **`div a`** — stage 1: `+N` divides by N (slower, up to `/32`), `-N` multiplies
  by N (faster, up to `x32`); `0`/`±1` = 1:1. `CV mod` can target `div`.
- **`div b`** — stage 2, same range. The stages multiply, so `/4 · x2` = net `/2`;
  extremes reach 1:1024 (clamped for safety).
- **Other shape params:** `width` (gate length) and `delay`, exactly like `pulse`.
- Output is a gate pulse per division; euclid, probability, phase and
  [FLEX](Flex-and-Cross.md) still apply. Classic hardware-divider behaviour: `x4`
  for a hi-hat roll, `/4` for a slow downbeat.

## quant — quantiser over a CV source

A live 1V/oct-style quantiser driven by an external value.

- **Shape params:** `q in` (source: `CV1`, `CV2` or `out1…out4`) and `scale`.
- The source is mapped across the channel's `cv min`..`cv max` window and snapped
  to the chosen scale; the gate pulses whenever the note changes.
- Unlike `note` (which quantises its own internal ramp), `quant` follows whatever
  you feed it — a wing controller, another LFO, a footpedal. To quantise a
  channel's *own* shape, use the [global
  quantiser](Flex-and-Cross.md#global-quantiser-quantise-in-the-range-group).

## ratch — ratchet

Bursts of fast gates per event.

- **Shape params:** `ratchets` (1–16 pulses per cycle), `width` (each burst's
  duty), `delay`.
- Splits every cycle into `ratchets` sub-slots and opens a gate at the top of
  each — tremolo/roll textures off one slow ratio. Euclid & probability gate the
  whole burst.

## scales

The quantiser vocabulary shared by `note`, `quant` and the global quantise. **29
built-in** (12-TET masks) + **3 editable user scales**:

```
chro major minor dorian phryg lydian mixolyd harm penta pentmn blues whole
melmin harmaj dor#4 phrdom loc alter lyddom dblharm enigm hiraj in iwato
prom triton dim aug blsmaj        user1 user2 user3
```

The first twelve are the original set; the rest add melodic/harmonic minor, the
common jazz modes, double-harmonic/enigmatic, and Japanese scales (hirajoshi, in,
iwato). **User scales** are edited on the SYS page (see
[Controls & UI](Controls-UI.md#the-sys-page)) and saved with the state.

> These are 12-semitone masks. Microtonal scales (16-ED, 53-TET, just intonation)
> cannot be represented by this firmware's 12-bit quantiser.

## off

Output forced to 0 V, gate low. The channel's position keeps advancing, so it
stays phase-locked if you switch back.

---

Next: [Parameters](Parameters.md) for the exact meaning and range of every
control, or [CV Modulation](CV-Modulation.md) to make them voltage-controllable.
