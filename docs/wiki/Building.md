# Building

## Prerequisites

- [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation.html)
  (`pio`) or the PlatformIO extension in VS Code.
- The Teensyduino/`teensy` PlatformIO platform is pulled automatically on first
  build.

## Build environments

Only two, both targeting genuine O_C hardware (`platformio.ini`):

| Env | Board | Outputs |
|-----|-------|---------|
| `teensy40` | Teensy 4.0 | 4 (quad DAC8565) |
| `teensy41` | Teensy 4.1 | 4 (quad DAC8565) |

```bash
pio run -e teensy40            # compile -> .pio/build/teensy40/firmware.hex
pio run -e teensy40 -t upload  # compile and flash (Teensy in bootloader mode)
pio run                          # builds every environment
```

To open it in VS Code: install PlatformIO, then **Open Folder** on the repo root;
the environments appear in the PlatformIO toolbar.

## Configuration knobs

Set at compile time via `-D` flags / `platformio.ini`:

- `NUM_OUTPUTS` (1–4) — number of channels; the O_C quad DAC caps at 4.
- `ENC_CLICK_STEPS` — quadrature steps per physical detent (default 4). Raise if
  one click moves two values; lower if only every other click registers.
- `BPM_MIN` / `BPM_MAX` — tempo window (default 10.00 / 300.00).
- `TICKS_PER_QUARTER` — clock resolution (default 96 ppq).
- `CONTROL_RATE_HZ` — update rate (default 1000 Hz; all outputs every tick).

## How the code is laid out

| File | Responsibility |
|------|----------------|
| `main.cpp` | setup; the 1 kHz `control_tick` and `ui_tick` IntervalTimers; autosave loop |
| `clock.cpp/.h` | master clock: internal + external sync, fractional ppq position |
| `channel.cpp/.h` | the engine: ratios → events → euclid/probability → mode → CV → DAC |
| `euclid.cpp/.h` | Bjorklund Euclidean pattern generator |
| `dac.cpp/.h` | DAC8565 quad writes over SPI (shares the bus with the OLED) |
| `ui.cpp/.h` | SH1106 SPI display, encoders/buttons (Phazerville-style 1 kHz poll), pages, groups, app menu |
| `storage.cpp/.h` | CRC-checked blob save/load to EEPROM |
| `pam_config.h` | the O_C pin map + build options |

### A note on timing

Audio/CV and the UI update are **separate 1 kHz timers**. Encoder/button state is
latched only inside `ui_tick()` (the shared state is never touched from an
interrupt), and the 1 kHz `control_tick()` drains and applies it — so SPI DAC
activity can’t corrupt knob reads. Encoders use the same rolling-shift
debounce/direction logic as Phazerville rather than a hardware quadrature timer,
because the O_C knobs sit on pins that don’t support the Teensy’s hardware
quad-decoder.

## Releasing / CI

`.github/workflows/build.yml` builds the hexes on tags and `v*` pushes, attaches
them to a GitHub Release, and copies them into `docs/firmware/` for the
[web flasher](Getting-Started.md#flashing).

## Known limitations

- 1 kHz update rate — modulation source, not audio-rate synthesis.
- External clock uses a smoothed period estimator, so very jittery clocks show a
  little lag in the measured-tempo readout.
- No MIDI / DIN-sync clock **output** (and no MIDI input on O_C — pin 0 is TR1).
- Output is linear 0–10 V without the module’s per-unit calibration table.

---

Prev: [Storage & Banks](Storage-Banks.md) · [Home](Home.md)
