# Pam's Newer Crime

*Pam's Newer Crime* (PNC) is a programmable, clocked modulation source for eurorack, running on
Teensy 4.0 / 4.1 as a drop-in for the genuine **O_C** hardware. Up to 4 CV outputs produce
pulses, LFOs, ramps, stepped sequences, sample & holds, quantized notes, envelopes, logic
ops and chaos waves, all synced to a master clock (internal or external TRS), with 24
musical ratios including odd/prime divisions, Euclidean patterns, phase/delay/swing,
probability, cross-channel CV routing, banks and power-off persistence.

The name is a nod: the "C" continues the ornament-&-crime lineage (O_C / H1200
clocked-modulation apps), and everything else is left to the imagination. The
implementation is original code, written from scratch for Teensy 4 hardware.

## Features

- 4 CV outputs (O_C quad DAC, `NUM_OUTPUTS=1..4`), each fully independent
- Master clock: 10.00 - 300.00 BPM (0.01 resolution), internal or external sync
  (1-48 PPQN pulse input on TR1)
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

PNC targets the genuine **O_C** family hardware (14HP original or uO_c 8HP micro),
as a Teensy 4.x drop-in. Flash `pams-newer-crime-teensy40.hex` (or the 4.1 hex).
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

External clock via the TR jacks works (pin 0 is the TR1 jack, so there is no
separate MIDI input on this profile). Note: the O_C output stage calibration
differs from O_C firmware — outputs are linear 0-10 V without the module's
calibration table.

## Controls

Both knobs and both buttons are wired and live:

- **Right encoder** — change the selected parameter's value (hold its push while
  turning = fine steps for BPM / voltage)
- **Left encoder** — select the parameter; rolling past either end wraps into
  the neighbouring page
- **Left encoder push** — tap = start/stop master clock (on the SYS page: run
  the highlighted action); hold = reset all channel phases
- **Right encoder push** — tap = next parameter (SYS page: run the highlighted action)
- **Top button** — next page · **Bottom button** — previous page
- Pages: `M` (master), `OUT1..N`, `SYS`
- Master page params: `BPM`, `clock` (int / ext), `ext ppqn`, `run`

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
