# Overview

Pam's Newer Crime turns an O_C-style Teensy board into a **clocked modulation
source**: one master clock drives up to four independent CV outputs, and each
output runs one of fourteen "apps" (modes) that generate gates, waveforms,
sequences, divisions or chaos, all locked to that clock.

## The signal flow

```
        ┌──────────────────────────────────────────────┐
        │                MASTER CLOCK                   │
        │   internal BPM  ·  or external TR1 pulses     │
        │   + global swing  → a 96 ppq position         │
        └───────────────────────┬──────────────────────┘
                                │  delta (fractional ticks since last tick)
        ┌───────────────────────┴──────────────────────┐
        │              EACH OUTPUT CHANNEL (×1..4)      │
        │                                              │
        │  ratio (or div a/b) divides the clock into    │
        │  EVENTS; FLEX micro-timing warps the grid     │
        │        ↓                                     │
        │  euclid + probability decide if THIS event    │
        │  actually FIRES (step app: per-step prob too) │
        │        ↓                                     │
        │  MODE turns events + a running position into  │
        │  a value 0..2047 (the "shape")               │
        │        ↓                                     │
        │  cv-min / cv-max / invert map 0..2047 →       │
        │  a voltage; cross-ops combine it with another │
        │  signal; optional global quantise snaps it    │
        │  to a scale; then lag (slew) smooths it       │
        │        ↓                                     │
        │  DAC out  +  a gate boolean for the meter     │
        └──────────────────────────────────────────────┘
                                │
                  CV-in routing (mod): any channel's two
                  modulation sources can read CV1, CV2 or
                  another output and bend ratio/width/phase/
                  delay/prob/div/range in real time
```

## Building blocks

- **Master clock** — a single tempo in BPM (10.00–330.00) or an external pulse
  stream on TR1, with optional global **swing**. Internally it maintains a
  fractional "position" at 96 pulses per quarter note, so every channel sees the
  same continuous timeline. → [Clock & Ratios](Clock-Ratios.md)
- **Channel** — each of the 1–4 outputs is fully independent. It has a `mode`
  (the "app"), a `ratio`, its own parameters, and its own CV-modulation slots.
  → [Modes](Modes.md), [Parameters](Parameters.md)
- **Events** — the ratio (or the `div` app's factors) divides the clock; per-channel
  **FLEX** warps the grid; when the channel's position crosses an event boundary
  it would *fire*. Euclidean gating (with loop) and probability — global and
  per-step — can veto individual events.
  → [Euclid & Probability](Euclid-Probability.md), [FLEX & Cross-ops](Flex-and-Cross.md)
- **Shape → voltage** — the mode produces a number 0..2047 each tick;
  `cv min`/`cv max`/`invert` scale it to a real output voltage; **cross-ops** can
  combine that voltage with another signal; an optional **global quantise** snaps
  it to a scale; and `lag` optionally slews it. → [Parameters](Parameters.md)
- **Modulation** — two assignable sources per channel bend other parameters on
  the fly from CV1, CV2 or another output's live voltage.
  → [CV Modulation](CV-Modulation.md)
- **Persistence** — everything is saved automatically after the panel goes idle,
  plus two named banks (A/B) you can store and recall.
  → [Storage & Banks](Storage-Banks.md)

## UI model

There are no menus buried under menus: you are always either on a **page**
(master, an output, or SYS) looking at a short list of **grouped** parameters,
or you are in the **app menu** picking a mode. The [Controls & UI](Controls-UI.md)
page has the full gesture map.

## What PNC is *not*

- Not an O_C/Hemispheres recompile — it is original firmware; it shares the O_C
  *pin map* only, so it drops into the same hardware.
- No audio-rate output — it is a modulation/clock source (updates at 1 kHz).
- No MIDI / DIN-sync clock **output** (see [Building](Building.md) limitations).
