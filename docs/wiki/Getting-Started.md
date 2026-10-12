# Getting Started

## You need

- A genuine **O_C**, **uO_c**, **O.R.N.8**, or any Phazerville-compatible board
  built around the same pin map, with a **Teensy 4.0** (or 4.1) in it.
- A USB cable (data, not charge-only) and a browser for the flasher.
- Eurorack power (the O_C board draws it from the bus; the Teensy alone will not
  run the DAC/OLED without the carrier).

## Flashing

The easiest path is the built-in **web flasher** — no software to install.

1. Go to <https://andrixb.github.io/PamsNewerCrime/>.
2. Put the Teensy into bootloader mode: power the board, then **press and hold
   the button on the Teensy itself for ~4 s** until the orange LED lights. (The
   firmware also offers a "reboot into bootloader" button that works over serial
   when the app is running.)
3. In the flasher pick a release (e.g. `teensy40` for a Teensy 4.0,
   `teensy41` for a 4.1), press **Flash**, and wait for "OK".
4. Unplug/replug or press reset; the app boots to the master page.

Prefer the command line? Build and upload with PlatformIO — see
[Building](Building.md).

## First run

On power-up you land on the **master page** (`M`) showing the tempo. The four
outputs are pre-configured as clocked `pulse` gates on different ratios.

A 60-second tour:

1. **Set tempo** — turn the **right encoder** to change BPM (hold its push while
   turning for fine steps). `10.00` … `300.00`.
2. **Start / stop** — tap the **left encoder push**. A running clock shows `>` in
   the page header.
3. **Pick an output** — press the **top / bottom buttons** to move between
   `M / OUT1 / OUT2 / OUT3 / OUT4 / SYS`.
4. **Choose an app** — on an output page, **hold the left encoder** to open the
   [app menu](Controls-UI.md#the-app-menu), scroll to `LFO` (for example) and
   press an encoder to assign it.
5. **Shape it** — now scroll the parameter groups with the left encoder and edit
   values with the right: `wave` (sine/tri/saw/sqr), `skew`, `range`, etc.
6. **Reset phases** — hold the **right encoder push** to snap every channel back
   to the start of its cycle (musically: re-align all the patterns at once).

## Patching basics

- Each output carries 0–10 V (or the channel's `cv min`..`cv max` window).
- Gates use the top of a channel's range; use `range` to set gate high/low if you
  feed a digital input.
- Feed any output into another module's CV, or into the module's own **CV1/CV2**
  jacks and route that voltage to modulate parameters — see
  [CV Modulation](CV-Modulation.md).

## Troubleshooting

| Symptom | Likely cause / fix |
|---------|--------------------|
| OLED stays black | Wrong hex for your board (4.0 vs 4.1), or OLED ribbon reversed. Re-flash matching `teensy40`/`teensy41`. |
| No CV out | Ensure carrier power, not just USB. Set `run` on and check the channel isn't `off`. |
| External clock ignores tempo | On the master page set `clock` to `ext` and set `ext ppqn` to match the source (24 for most, 1 for some drum machines). |
| Board won't flash | Hold the Teensy button ~4 s for the orange LED (bootloader) first. |

Next: [Controls & UI](Controls-UI.md).
