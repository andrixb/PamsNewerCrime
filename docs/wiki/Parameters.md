# Parameters

Every parameter, its group, range and what it does. Ranges/units below match the
firmware exactly (`src/ui.cpp`, `src/channel.cpp`).

Values are edited with the **right encoder** (hold its push for fine steps on
BPM and `cv min`/`cv max`).

## clock group

| Param | Range | Meaning |
|-------|-------|---------|
| `ratio` | 24 values, `1/64` … `4bar` | How many events per bar. Includes triplets (`T`), dotted (`.`) and odd/prime divisions (`1/5`, `1/7`, `1/9`, `1/11`, `1/13`). See [Clock & Ratios](Clock-Ratios.md). |
| `phase` | 0–255 | Offset into the cycle where the channel starts — rotate several channels out of step. |
| `prob` | 0–100 % | Chance an event actually fires. See [Euclid & Probability](Euclid-Probability.md). |

## range group

| Param | Range | Meaning |
|-------|-------|---------|
| `cv min` | 0–10000 mV | Bottom of the output window (also gate-low). |
| `cv max` | 0–10000 mV | Top of the output window (also gate-high). Kept ≥ `cv min`. |
| `invert` | on/off | Flip the shape within its range. |
| `lag` | off … ~2 s (`0`, `8…2040 ms`) | Exponential slew on the CV. Bypassed for `pulse`/`logic` so triggers stay sharp. |

## euclid group (gate-based apps)

| Param | Range | Meaning |
|-------|-------|---------|
| `euclid` | on/off | Enable Euclidean gating of events. |
| `eu length` | 1–32 | Steps in the pattern loop. |
| `eu fill` | 0–length | Pulses placed in the loop. |
| `eu rot` | 0–length−1 | Rotate the pattern to shift the accents. |

See [Euclid & Probability](Euclid-Probability.md).

## mod group (CV modulation)

Two independent modulation chains, A and B:

| Param | Values | Meaning |
|-------|--------|---------|
| `cv1 from` / `cv2 from` | `CV1`, `CV2`, `out1…out4` | Which voltage this chain reads. |
| `cv in 1` / `cv in 2` | `--`, `range`, `width`, `phase`, `delay`, `prob`, `ratio` | Which parameter it modulates. |
| `cv1 scale` / `cv2 scale` | −128…+127 | Bipolar depth & direction. |

See [CV Modulation](CV-Modulation.md).

## shape group (per app)

Only the parameters for the **current** app appear here.

| Param | Apps | Range | Meaning |
|-------|------|-------|---------|
| `width` | pulse, logic, woggle, rwalk | 0–255 | pulse/logic: gate length (% of cycle). woggle: speed. rwalk: step size. |
| `delay` | pulse, logic | 0–255 | Offset after the event before the gate starts (swing). |
| `wave` | LFO | sine/tri/saw/sqr | Waveform. |
| `skew` | LFO, ramp, woggle | 0–255 | LFO: shape (tri rise point / sqr width). ramp: turn-around point. woggle: chaos amount. |
| `seq len` | step | 1–16 | Number of active steps. |
| `step 1…N` | step | 0–255 | Value at that step (across the output's `cv min`..`cv max`). |
| `scale` | note | 12 scales | Quantizer scale. See [Modes](Modes.md#note--quantized-sequence--melodic-ramp). |
| `attack` | env | ~1 ms…8.1 s | Envelope rise time. |
| `decay` | env | ~1 ms…8.1 s | Envelope fall time. |
| `logic` | logic | and/or/xor/nand/nor | Boolean operation with the source channel's gate. |
| `lg src` | logic | `--`, `out1…out4` | The other channel. |

## master page params

| Param | Range | Meaning |
|-------|-------|---------|
| `BPM` | 10.00–300.00 | Master tempo (fine steps while holding right push). |
| `clock` | `int` / `ext` | Internal source or sync to TR1 pulses. |
| `ext ppqn` | 1–48 | Pulses-per-quarter-note of the external clock. |
| `run` | run / stop | Transport (tapping the left encoder is the shortcut). |

---

Prev: [Modes](Modes.md) · Next: [Clock & Ratios](Clock-Ratios.md)
