# Storage & Banks

PNC remembers your patch across power cycles and lets you snapshot whole sets of
outputs into two named banks.

## Autosave (the live state)

- Every time you change something, the state is marked “dirty”.
- After **~3 seconds of no further edits**, the whole state is written to
  EEPROM automatically.
- On power-up that live state is restored before the UI starts, so you return to
  exactly where you left off — no save button, nothing to lose on a power cut
  (barring the last <3 s of edits).

This is the *only* always-on saving; you rarely think about it.

## Banks A / B (manual snapshots)

The **SYS** page lists these actions (scroll the right encoder to a line, tap the
left encoder to run it):

1. **save bank A** — copy the current state into bank A
2. **save bank B** — copy into bank B
3. **load bank A** — replace the current state from bank A
4. **load bank B** — replace from bank B
5. **defaults** — reset to 120 BPM, all channels back to their initial `pulse`
   configuration

Banks are *snapshots*: load copies the bank into your live state, but the live
state keeps autosaving on top, so a bank is never mutated until you save over it
again. This is how you park a whole patch (four apps, ratios, mod routing) and
recall it later, or A/B two ideas on the spot.

## What is stored

A single blob holds the master clock (BPM, source, ext ppqn, run) and every
channel's full parameter set (mode, ratio, all shape params, ranges, euclid,
probability, step values, and both modulation chains).

## Integrity & format

- Storage is **CRC-checked** with a format **version + magic**; loading a blob
  that is corrupt or from a different firmware version is rejected rather than
  applied, so a bad write or an upgrade falls back to defaults safely.
- Three slots live at fixed 512-byte offsets in the (emulated) EEPROM: live,
  bank A, bank B.

> Note: output *CV* is not stored (it is generated), and the Teensy's EEPROM
> emulation has limited write endurance — the 3-second debounce on autosave is
> intentional to keep writes reasonable.

---

Prev: [CV Modulation](CV-Modulation.md) · Next: [Building](Building.md)
