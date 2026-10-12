# Modes (the apps)

Pick an app per output from the [app menu](Controls-UI.md#the-app-menu). Each app
turns the channel's clock **events** and **position** into an output shape. Every
app shares the `clock` group (`ratio`, `phase`, `prob`), the `range` group
(`cv min`, `cv max`, `invert`, `lag`) and optional
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

- **Shape params:** `wave` (`sine`, `tri`, `saw`, `sqr`), `skew` (shape:
  pulse-width for sqr, rise-point for tri).
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
- **Scales (12-TET subset):** chromatic, major, minor, dorian, phrygian, lydian,
  mixolydian, harmonic minor, pentatonic maj/min, blues, whole tone.
  > These are exactly the 12-TET members of the O_C/Braids scale set. The rest of
  > that library is microtonal (16-ED, 53-TET, Bohlen-Pierce, just-intoned ragas)
  > and cannot be represented by this firmware's 12-semitone quantizer.

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

## off

Output forced to 0 V, gate low. The channel's position keeps advancing, so it
stays phase-locked if you switch back.

---

Next: [Parameters](Parameters.md) for the exact meaning and range of every
control, or [CV Modulation](CV-Modulation.md) to make them voltage-controllable.
