# Workout-Time

Teensy 4.0 / 4.1 firmware inspired by the **Pamela's NEW Workout** (ALM-017) eurorack module:
a programmable, clocked modulation source. Multiple CV outputs generate pulses, LFOs,
ramps, stepped sequences and sample & holds, all synced to a BPM-based master clock
(internal or externally synced), with Euclidean patterns, phase/delay/swing, probability,
2 assignable CV inputs, banks and power-off persistence.

This is an independent, clean-room implementation written from the ALM-017 operation
manual and the original O_C `H1200` app concepts. It does **not** contain any code or
firmware from ALM Busy Circuits (their firmware is closed source and STM32-only).

## Features

- 4 or 8 CV outputs (compile-time `NUM_OUTPUTS`, 1..8)
- Master clock: 10.00 - 300.00 BPM (0.01 resolution), internal or external sync
  (1-48 PPQN clock input)
- Per output: 15 musical ratios (1/32 .. 4bar, incl. triplet & dotted), modes:
  `pulse / LFO / ramp / step / S&H / off`
- LFO waves: sine, triangle, saw, square; skew, phase, delay (swing), pulse width
- CV min/max range per output, invert, 0-4.09 V full scale
- Euclidean rhythm per output (length 1-32, fill, rotation)
- Probability per event (event skipping)
- 16-step sequence with per-step values and length
- 2 CV inputs, each assignable per output to: range / width / phase / delay / prob /
  ratio, with signed scale
- 2 save/load banks + automatic persistence (EEPROM emulation, CRC-checked, autosave
  after 3 s idle)
- 128x64 OLED status bar: live output voltage bars + gate indicators

## Hardware

Teensy 4 has no DAC, so CV outputs use SPI DACs (12-bit, buffered, gain x2).
Outputs are **0-4.09 V**; add an op-amp gain/offset stage (e.g. TL072 x2 + offset)
if you need full 0-8 V eurorack range.

```
Teensy 4.x        MCP4822 (one per 2 outputs)
--------------    --------------------------------
13 SCK  --------> SCK  (all chips)
11 MOSI --------> SDI  (all chips)
10 CS0 ---------> CS (output 1 & 2)
9  CS1 ---------> CS (output 3 & 4)
8  CS2 ---------> CS (output 5 & 6)   [8-out builds]
7  CS3 ---------> CS (output 7 & 8)   [8-out builds]
V+ 5V, VSS GND, LDAC/SHDN tied to V+, 100nF + 10uF decoupling per chip
DAC outputs (LA/LB) -> RC filter (100R/10nF) -> buffer -> jack
```

Other pins:

| Function            | Pin        | Notes |
|---------------------|------------|-------|
| SSD1306 OLED SDA    | 18         | I2C, 3.3 V module |
| SSD1306 OLED SCL    | 19         | |
| Encoder A / B / SW  | 4 / 5 / 6  | to GND, internal pullups |
| Button B2           | 2          | to GND |
| Clock input         | 3          | 3.3 V logic; use 74HC14 Schmitt trigger from eurorack 5-12 V |
| CV input 1 / 2      | A0 (16) / A1 (17) | divider 100k/150k -> 0-3.3 V for 0-5.5 V input |

## Controls

- **Encoder** — change selected parameter (turn while pressing = fine steps for BPM / voltage)
- **Encoder press** — next parameter; **hold press** — next page
- **B2 tap** — reset all channel phases (clock reset)
- **B2 hold** — start/stop master clock
- Pages: `M` (master), `OUT1..N`, `SYS`

SYS page: save/load bank A and B, restore defaults (rotate to select, press to execute).

## Building

PlatformIO (recommended):

```
pio run -e teensy40      # Teensy 4.0, 4 outputs
pio run -e teensy41      # Teensy 4.1, 8 outputs
pio run -e teensy40 -t upload
```

The `teensy40_8out` env shows Teensy 4.0 can also run 8 outputs (4 DAC chips).

## Notes / current limitations

- Update rate 1 kHz (round-robin-free, all outputs updated every tick)
- External clock jitter: measured with a smoothed period estimator
- No MIDI/DIN-sync expanders (ALM's optional hardware) — the ext-sync input covers
  the common case
- Verified by build only if you report back — hardware bring-up notes welcome as issues

## License

MIT (as in the original O_C-derived code conventions); schematics/hardware of the
commercial modules referenced above belong to their respective owners.
