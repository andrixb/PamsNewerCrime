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

> **Full documentation lives in the [Wiki](docs/wiki/Home.md)** — an
> [overview](docs/wiki/Overview.md), plus deep dives on every
> [mode](docs/wiki/Modes.md), [parameter](docs/wiki/Parameters.md), the
> [clock](docs/wiki/Clock-Ratios.md), [CV modulation](docs/wiki/CV-Modulation.md)
> and more.

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
- CV min/max range per output, invert, 0-10 V full scale (O_C DAC8565)
- Euclidean rhythm per output (length 1-32, fill, rotation)
- Probability per event (event skipping)
- 16-step sequence with per-step values and length
- 2 physical CV inputs + **cross-channel routing**: each channel's two modulation
  sources can be CV1, CV2 or any other output's live voltage; each source is
  assignable to modulate range / width / phase / delay / prob / ratio, with signed scale
- 2 save/load banks + automatic persistence (EEPROM emulation, CRC-checked, autosave
  after 3 s idle; format v3)
- 128x64 OLED: a tall right-side voltage meter (live output level + gate strip,
  one bar per output) beside the parameter list; hidden on the SYS page

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
- **Left encoder** — select the parameter; it wraps within the current page
  only (the panel buttons change pages)
- **Left encoder push** — tap = start/stop master clock (SYS page: run the
  highlighted action); **hold = open the app menu**
- **Right encoder push** — tap = next parameter; hold = reset all channel phases
- **Top button** — next page · **Bottom button** — previous page
- Pages: `M` (master), `OUT1..N`, `SYS`
- Master page params: `BPM`, `clock` (int / ext), `ext ppqn`, `run`

### The app menu

Hold the **left encoder** on any output page to open the app menu — a list of
the modes (`off, pulse, LFO, ramp, step, S&H, note, env, logic, woggle, rwalk`),
one per output like a Phazerville "app". Rotate either encoder to scroll (the
output's current app is marked `*`), then **press either encoder** to assign it.
Either panel button cancels.

Once assigned, the output page is split into short labelled **groups** instead of
one long list. The highlighted parameter's group shows on the right of the
header. Scroll past the end of one group to enter the next:

| Group | Contents |
|-------|----------|
| *app name* (shape) | the params unique to the app — e.g. `wave`/`skew` for LFO, `width`/`delay` for pulse, `attack`/`decay` for env, `seq len` + `step 1..N` for step |
| `clock` | `ratio`, `phase`, `prob` |
| `euclid` | euclid en / length / fill / rotation (gate-based apps only) |
| `range` | `cv min`, `cv max`, `invert`, `lag` |
| `mod` | `cv1/cv2 from`, `cv in 1/2`, `cv1/cv2 scale` (CV modulation) |

Continuous apps (LFO/ramp/woggle) skip the `euclid` group; `S&H` has no shape
group. SYS page: save/load bank A and B, restore defaults.

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

PlatformIO (recommended). Two environments, both for genuine O_C hardware
(`teensy40`, `teensy41`), each 4 outputs (quad DAC8565):

```
pio run -e teensy40        # Teensy 4.0, 4 outs, on O_C / uO_c hardware
pio run -e teensy41        # Teensy 4.1, 4 outs
pio run -e teensy40 -t upload
```

See the [Building wiki page](docs/wiki/Building.md) for configuration flags, the
source layout, and CI.

## Notes / current limitations

- Update rate 1 kHz (round-robin-free, all outputs updated every tick)
- External clock jitter: measured with a smoothed period estimator
- No MIDI / DIN-sync clock input or output (on O_C, pin 0 is the TR1 clock jack);
  external sync is via the TR jack only
- WebHID flasher cannot recover a firmware that hangs before USB init — hold the
  program button ~15 s (orange LED) to force bootloader mode, then reflash; the
  bootloader is in ROM, so the board cannot be hard-bricked
- Verified by build only if you report back — hardware bring-up notes welcome as issues

## License

MIT. This project is an independent implementation; it contains no source code copied
from the O_C project and no firmware from any commercial product.
