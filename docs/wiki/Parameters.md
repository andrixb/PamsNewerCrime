# Parameters

Every parameter, its group, range and what it does. Ranges/units below match the
firmware exactly (`src/ui.cpp`, `src/channel.cpp`).

Values are edited with the **right encoder** (hold its push for fine steps on
BPM and `cv min`/`cv max`).

## clock group

| Param | Range | Meaning |
|-------|-------|---------|
| `ratio` | 24 values, `1/64` … `4bar` | How many events per bar. Includes triplets (`T`), dotted (`.`) and odd/prime divisions (`1/5`, `1/7`, `1/9`, `1/11`, `1/13`). Bypassed by `div` (which uses `div a`/`div b`). See [Clock & Ratios](Clock-Ratios.md). |
| `phase` | 0–255 | Offset into the cycle where the channel starts — rotate several channels out of step. |
| `prob` | 0–100 % | Chance an event actually fires. See [Euclid & Probability](Euclid-Probability.md). |

## flex group

| Param | Range | Meaning |
|-------|-------|---------|
| `flex` | off / human / swing / accel / hump / fall | Micro-timing warp on this channel's event grid. See [FLEX & Cross-ops](Flex-and-Cross.md). |
| `flex amt` | 0–255 | Intensity of the chosen FLEX op. |

## cross group

| Param | Range | Meaning |
|-------|-------|---------|
| `x op` | -- / mix / mask / not / mult / hold / s&h / add / sub / and / or / xor / seed | How to combine this output's value with a source, on **any** app. See [FLEX & Cross-ops](Flex-and-Cross.md). |
| `x src` | `CV1`, `CV2`, `out1…out4` | The source for the cross-op. |

## euclid group (gate-based apps)

| Param | Range | Meaning |
|-------|-------|---------|
| `euclid` | on/off | Enable Euclidean gating of events. |
| `eu length` | 1–32 | Steps in the pattern loop. |
| `eu fill` | 0–length | Pulses placed in the loop. |
| `eu rot` | 0–length−1 | Rotate the pattern to shift the accents. |
| `eu loop` | 0 / 1–32 | Lengthen the cycle past `length` with trailing rests (0 = off). Lets a 3-in-8 pattern reset over a 16-step bar for non-4/4 feels. |

See [Euclid & Probability](Euclid-Probability.md).

## range group

| Param | Range | Meaning |
|-------|-------|---------|
| `cv min` | 0–10000 mV | Bottom of the output window (also gate-low). |
| `cv max` | 0–10000 mV | Top of the output window (also gate-high). Kept ≥ `cv min`. |
| `invert` | on/off | Flip the shape within its range. |
| `lag` | off … ~2 s (`0`, `8…2040 ms`) | Exponential slew on the CV. Bypassed for `pulse`/`div`/`ratch`/`logic` so triggers stay sharp. |
| `quantise` | on/off | Global post-quantiser: snap the final value to the `scale` window. See [FLEX & Cross-ops](Flex-and-Cross.md#global-quantiser-quantise-in-the-range-group). |
| `scale` | 32 values | Quantiser scale for `note`, `quant` and `quantise`. See [Scales](Modes.md#scales). |

## mod group (CV modulation)

Two independent modulation chains, A and B:

| Param | Values | Meaning |
|-------|--------|---------|
| `cv1 from` / `cv2 from` | `CV1`, `CV2`, `out1…out4` | Which voltage this chain reads. |
| `cv in 1` / `cv in 2` | `--`, `range`, `width`, `phase`, `delay`, `prob`, `ratio`, `div`, `skew`, `attack`, `decay`, `slew` | Which parameter it modulates. |
| `cv1 scale` / `cv2 scale` | −128…+127 | Bipolar depth & direction. |

See [CV Modulation](CV-Modulation.md).

## shape group (per app)

Only the parameters for the **current** app appear here.

| Param | Apps | Range | Meaning |
|-------|------|-------|---------|
| `width` | pulse, logic, woggle, rwalk, div, ratch | 0–255 | pulse/logic/div/ratch: gate length (% of cycle). woggle: speed. rwalk: step size. |
| `delay` | pulse, logic, div, ratch | 0–255 | Offset after the event before the gate starts. |
| `wave` | LFO | sine/tri/saw/sqr/trapz/parab | Waveform. |
| `skew` | LFO, ramp, woggle | 0–255 | LFO: shape (tri rise point / sqr width / trapz ramp). ramp: turn-around. woggle: chaos. |
| `seq len` | step | 1–16 | Number of active steps. |
| `step 1…N` | step | 0–255 | Value at that step (across the output's `cv min`..`cv max`). |
| `s1 prob…s16 prob` | step | 0–100 % | Per-step trigger probability (a failed step is skipped: index advances, value holds). |
| `scale` | note | 32 values | Quantizer scale. See [Scales](Modes.md#scales). |
| `attack` | env | ~1 ms…8.1 s | Envelope rise time. |
| `decay` | env | ~1 ms…8.1 s | Envelope fall time. |
| `logic` | logic | and/or/xor/nand/nor | Boolean operation with the source channel's gate. |
| `lg src` | logic | `--`, `out1…out4` | The other channel. |
| `div a` / `div b` | div | `-32` … `/32` (`x%d`/`/%d`/`1:1`) | Signed div/mult stages. See [Modes](Modes.md#div--clock-divider--multiplier). |
| `q in` | quant | `CV1`, `CV2`, `out1…out4` | Source to quantise. |
| `ratchets` | ratch | `1x`…`16x` | Bursts per event. |

## master page params

| Param | Range | Meaning |
|-------|-------|---------|
| `BPM` | 10.00–330.00 | Master tempo (fine steps while holding right push). |
| `clock` | `int` / `ext` | Internal source or sync to TR1 pulses. |
| `ext ppqn` | 1–48 | Pulses-per-quarter-note of the external clock. |
| `swing` | off … 100 % | Global eighth-note swing on the master grid (odd eighths fall late; quarter boundaries stay locked). Internal clock only. See [Clock & Ratios](Clock-Ratios.md). |
| `run` | run / stop | Transport (tapping the left encoder is the shortcut). |

---

Prev: [Modes](Modes.md) · Next: [Clock & Ratios](Clock-Ratios.md)
