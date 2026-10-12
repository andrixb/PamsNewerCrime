# Pam's Newer Crime

*Pam's Newer Crime* (PNC) is a programmable, clocked modulation source for eurorack, running on
Teensy 4.0 / 4.1. Multiple CV outputs produce pulses, LFOs, ramps, stepped sequences,
sample & holds, quantized notes, envelopes, logic ops and chaos waves, all synced to a
master clock (internal, external TRS or MIDI), with 24 musical ratios including
odd/prime divisions, Euclidean patterns, phase/delay/swing, probability, cross-channel CV
routing, banks and power-off persistence.

The name is a nod: the "C" continues the ornament-&-crime lineage (O_C / H1200
clocked-modulation apps), and everything else is left to the imagination. The
implementation is original code, written from scratch for Teensy 4 hardware.

## Features

- 4 or 8 CV outputs (compile-time `NUM_OUTPUTS`, 1..8), each fully independent
- Master clock: 10.00 - 300.00 BPM (0.01 resolution), internal, external sync
  (1-48 PPQN pulse input) or **MIDI clock** (24 PPQN, Start/Continue/Stop transport)
- Per output: 24 musical ratios (1/64 .. 4bar), incl. triplets, dots and odd/prime
  divisions (1/5, 1/7, 1/9, 1/11, 1/13)
- Modes per output:
  `pulse / LFO / ramp / step / S&H / note / env / logic / woggle / rwalk / off`
  - **note** — quantized 1V/oct ramp across the CV range, 12 scales
    (chromatic, major, minor, dorian, phrygian, lydian, mixolydian, harmonic minor,
    pentatonic maj/min, blues, whole tone); gate pulses on each note change
  - **env** — clock-synced AD envelope, attack/decay 1 ms - 8.1 s, retriggers on each
    event (respects euclid & probability)
  - **logic** — AND / OR / XOR / NAND / NOR of this channel's pulse with any other
    channel's gate
  - **woggle** — Wogglebug-style chaos wave (skew = chaos, width = speed, jumps on
    each event)
  - **rwalk** — bounded random walk (RNG out)
- LFO waves: sine, triangle, saw, square; skew, phase, delay (swing), pulse width
- Per-output slew/lag (exponential filter; triggers stay sharp)
- CV min/max range per output, invert, 0-4.09 V full scale
- Euclidean rhythm per output (length 1-32, fill, rotation)
- Probability per event (event skipping)
- 16-step sequence with per-step values and length
- 2 physical CV inputs + **cross-channel routing**: each channel's two modulation
  sources can be CV1, CV2 or any other output's live voltage; each source is
  assignable to modulate range / width / phase / delay / prob / ratio, with signed scale
- 2 save/load banks + automatic persistence (EEPROM emulation, CRC-checked, autosave
  after 3 s idle; format v3)
- 128x64 OLED status bar: live output voltage bars + gate indicators

## Hardware

Two hardware profiles are supported, selected at build time:

| Build env | Target | Display | DAC |
|-----------|--------|---------|-----|
| `teensy40` / `teensy41` | **genuine O_C board** (14HP original or uO_c 8HP micro), Teensy 4.x drop-in (default) | SH1106 on SPI, CS=8 DC=6 RST=7 | DAC8565 quad on same SPI, CS=10 RST=9, **0-10 V** |
| `teensy40_diy` / `teensy41_diy` | DIY breadboard per the wiring table below | SSD1306 or SH1106 on I2C (18/19), chosen at runtime on the SYS page | MCP4822 x2-x4 on CS 10/9/8/7, **0-4.09 V** |

### O_C hardware profile (default: `teensy40`)

If your Teensy 4.0 sits on an O_C / uO_c shield, flash `pams-newer-crime-teensy40.hex`.
It uses the exact pin map of the O_C family (verified against Phazerville):

| Function | Teensy 4.0 pin | O_C side |
|----------|----------------|----------|
| OLED CS / DC / RST | 8 / 6 / 7 | SH1106 SPI panel |
| DAC CS / RST | 10 / 9 | DAC8565 (outputs 1-4, 0-10 V) |
| Left encoder A / B / push | 22 / 21 / 23 | left encoder |
| Right encoder A / B / push | 16 / 15 / 14 | right encoder |
| Buttons top / bottom | 5 / 4 | two panel buttons |
| Clock input | 0 | TR1 jack (active-low triggers) |
| CV in 1 / 2 | A3 / A4 (CV1/CV2 jacks) | bipolar, 0 V = mid |

MIDI input is unavailable on this profile (pin 0 is the TR1 jack); external clock
via TR jacks works. Note: O_C output stage calibration differs from O_C firmware —
outputs are linear 0-10 V without the module's calibration table.

### DIY breadboard profile (`teensy40_diy` / `teensy41_diy`)

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
| SSD1306 OLED SDA/SCL| 18 / 19    | I2C, 3.3 V module |
| Encoder A / B / SW  | 4 / 5 / 6  | to GND, internal pullups |
| Button B2           | 2          | to GND |
| Clock input         | 3          | 3.3 V logic; use 74HC14 Schmitt trigger from eurorack 5-12 V |
| MIDI in (RX1)       | 0          | 31250 baud; standard 6N138/6N139 optocoupler front-end |
| CV input 1 / 2      | A0 (16) / A1 (17) | divider 100k/150k -> 0-3.3 V for 0-5.5 V input |

### Blank I2C screen? (SSD1306 vs SH1106)

Many "128x64 I2C OLED" modules carry an **SH1106** instead of the SSD1306 its
label claims. An SH1106 acknowledges I2C at 0x3C and `begin()` reports success,
yet stays completely blank (132-column RAM with +2 offset, page addressing only).
The DIY build handles both: on **first boot** the panel flashes **1x = SSD1306**,
**2x = SH1106** (also logged to the debug console). Pick the one that flashed on
the **SYS page** (`oled: ssd1306` / `oled: sh1106`); the choice is saved to EEPROM
and the probe only runs again after the stored choice is cleared.

### MIDI clock input (DIY profile)

Connect a DIN-5 MIDI socket through an optocoupler to pin 0 (RX1). Real-time Clock
(0xF8), Start (0xFA), Continue (0xFB) and Stop (0xFC) are handled; other messages are
ignored. Receiving **Start** switches the master clock to MIDI source automatically;
each channel then divides the MIDI transport with its ratio. The header shows `MIDI`
and the measured tempo.

## Controls

O_C dual-knob panel (both knobs and both buttons are wired and live):

- **Left encoder** — change the selected parameter's value (hold its push while
  turning = fine steps for BPM / voltage)
- **Right encoder** — select the parameter; rolling past either end wraps into
  the neighbouring page
- **Right encoder push** — tap = start/stop master clock (on the SYS page: run
  the highlighted action); hold = reset all channel phases
- **Top button** — next page · **Bottom button** — previous page
- Pages: `M` (master), `OUT1..N`, `SYS`

DIY single-encoder profile: left encoder edits values, its press steps
parameters, holding it changes page; **B2** tap resets phases, hold runs/stops.
- Master page params: `BPM`, `clock` (int / ext / midi), `ext ppqn`, `run`

Channel page params, in order: `ratio`, `mode`, `wave`, `width`, `skew`, `cv min`,
`cv max`, `invert`, `phase`, `delay`, `prob`, `euclid`, `eu length`, `eu fill`,
`eu rot`, `seq len`, `step 1..16`, `cv in 1` + `cv1 scale`, `cv in 2` + `cv2 scale`
(modulation *destinations*), `lag`, `scale`, `attack`, `decay`, `logic`, `lg src`,
`cv1 from`, `cv2 from` (modulation *sources*: CV1 / CV2 / out1..outN).
Parameters not used by the current mode are still shown but have no effect
(e.g. `scale` only matters in `note` mode, `skew`/`width` = chaos/speed in `woggle`).

SYS page: save/load bank A and B, restore defaults (rotate to select, press to execute).

## Web flasher

`docs/index.html` is a zero-install browser flasher (WebHID + halfkdf protocol,
same as Teensy Loader). Flash straight from GitHub Pages:

1. Push a tag (`v0.1.0`) — CI builds both hexes, attaches them to a GitHub release,
   and commits copies to `docs/firmware/` (served same-origin, since GitHub's release
   CDN blocks browser fetches with CORS).
2. Enable Pages once: repo **Settings -> Pages -> Source: GitHub Actions** is not
   needed; use **Deploy from a branch / main / /docs**.
3. Open `https://andrixb.github.io/PamsNewerCrime/`, hold the Teensy program button
   ~4 s so the bootloader (16c0:0478) appears, pick the release hex, Flash.

If the release fetch fails (private repo, rate limit, Pages copy not published yet),
use the **Local file** option: download `*.hex` from the release page and pick it.

Requirements: Chrome/Edge/Brave over HTTPS (WebHID is not available in
Firefox/Safari or from `file://`). Works with any Teensy 4.0/4.1 board, also for
other Teensy sketches.

## Building

PlatformIO (recommended):

```
pio run -e teensy40        # Teensy 4.0, 4 outs, DIY wiring, I2C SSD1306
pio run -e teensy40_oc     # Teensy 4.0 on genuine O_C / uO_c hardware
pio run -e teensy41        # Teensy 4.1, 8 outs, DIY wiring
pio run -e teensy40 -t upload
```

The `teensy40_8out` env shows Teensy 4.0 can also run 8 outputs (4 DAC chips).

## Notes / current limitations

- Update rate 1 kHz (round-robin-free, all outputs updated every tick)
- External clock jitter: measured with a smoothed period estimator
- MIDI **input** only (clock + transport); no MIDI/DIN-sync clock *outputs* yet
- WebHID flasher cannot recover a firmware that hangs before USB init — hold the
  program button ~15 s (orange LED) to force bootloader mode, then reflash; the
  bootloader is in ROM, so the board cannot be hard-bricked
- Verified by build only if you report back — hardware bring-up notes welcome as issues

## License

MIT. This project is an independent implementation; it contains no source code copied
from the O_C project and no firmware from any commercial product.
