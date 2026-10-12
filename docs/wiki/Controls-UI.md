# Controls & UI

The O_C panel has **two encoders** (each with a push-button) and **two buttons**
(left / right). PNC uses all four, plus both encoder pushes.

## The two encoders have fixed jobs

- **Right encoder = the value dial.** It always changes the value of the
  highlighted parameter.
- **Left encoder = the selector.** It moves the highlight through the current
  page's parameters, wrapping at the ends (it never changes page).

This split is deliberate: you never have to remember "is this knob value or
select" — it's always the same.

## Gesture map

| Control | Tap | Hold |
|---------|-----|------|
| **Right encoder turn** | change highlighted value | — |
| **Right encoder push** | next parameter | reset all channel phases |
| **Left encoder turn** | move the highlight | — |
| **Left encoder push** | start / stop the master clock | open the **app menu** |
| **Left button** | previous page | — |
| **Right button** | next page | — |

- **Fine steps** — hold the **right push** while turning the right encoder to
  edit BPM and `cv min`/`cv max` in small increments instead of the coarse step.
- **Phase reset** (hold right push) snaps every channel's cycle back to the
  start — useful to re-align several patterns at once.
- **Run / stop** (tap left push) is the same on every page. On the **SYS** page
  the left-push tap instead *runs the highlighted action*.

## Pages

Pages cycle `M → OUT1 → … → OUTn → SYS → M` (n is your output count, up to 4 on
O_C hardware). The left/right buttons move between them.

- **M (master)** — `BPM` (10.00–330.00), `clock` (`int`/`ext`), `ext ppqn`
  (1–48), `swing` (global eighth swing), `run`. See [Clock & Ratios](Clock-Ratios.md).
- **OUTn** — the chosen output's parameters, grouped. See
  [Parameters](Parameters.md).
- **SYS** — save / load bank A and B, restore defaults, and the three
  **user-scale editors** (see [Storage & Banks](Storage-Banks.md) and
  [The SYS page](#the-sys-page)). Rotate the right encoder to highlight an
  action, tap the left encoder to execute it.

## The page layout

Every output page shows three parameter lines at once, a header (`OUTn  <group>`
on the left — the group name doubles as the app name on shape pages), and a
**tall output meter** on the far right: one vertical bar per output, filled from
its `cv min` to `cv max` from the floor of the box up, with a gate strip at the
base. The meter is hidden on the SYS page and while the app menu is open.

## The app menu

Hold the **left encoder** on any output page. The screen lists the modes
(`off, pulse, LFO, ramp, step, S&H, note, env, logic, woggle, rwalk, div, quant, ratch`) — one
"app" per output, Phazerville-style.

- Scroll with **either encoder**.
- The output's **current** app is marked with a `*`.
- Press **either encoder** to assign the highlighted app and close the menu.
- Press either **panel button** to cancel without changing.

After assigning, that page rebuilds to show only the parameters that app uses.

## Parameter groups

Within an output page the parameters are split into short labelled groups
(shown on the right of the header as you scroll into them):

| Group | Contents |
|-------|----------|
| *(app name)* | the parameters unique to that mode — `wave`/`skew`, `width`/`delay`, `seq len` + `step 1..N` + `s.. prob`, `scale`, `attack`/`decay`, `logic`/`lg src`, `div a`/`div b`, `q in`, `ratchets`, etc. |
| `clock` | `ratio`, `phase`, `prob` |
| `euclid` | euclid on/length/fill/rotation/loop — **gate-based apps only** |
| `flex` | `flex`, `flex amt` — per-channel micro-timing ([FLEX & Cross-ops](Flex-and-Cross.md)) |
| `cross` | `x op`, `x src` — cross output modulation ([FLEX & Cross-ops](Flex-and-Cross.md)) |
| `range` | `cv min`, `cv max`, `invert`, `lag`, `quantise`, `scale` |
| `mod` | the two CV-modulation chains — see [CV Modulation](CV-Modulation.md) |

Continuous apps (LFO / ramp / woggle) skip `euclid`; `S&H` has no shape group.

## The SYS page

Beyond save/load banks and defaults, the SYS page holds the **user-scale editor**.
Scroll to `edit scale 1` (or 2, 3) and tap the left encoder to enter the editor:

- A row of 12 boxes shows the octave; a filled box is *in* the scale.
- **Left encoder** moves the note cursor, **right encoder** toggles that note.
- **Any encoder push or panel button** commits the scale and returns to the menu.

The three user scales appear as `user1/2/3` in every `scale` list and persist with
the rest of the state.

Next: [Modes](Modes.md).
