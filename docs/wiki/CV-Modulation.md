# CV Modulation

Any output channel can have up to **two** external voltages steering its
parameters live. This is what makes the patterns respond to a performance — a
foot pedal on CV1, an LFO patched into CV2, or another output feeding back.

Each channel has two independent modulation “chains” (call them **A** and **B**),
each with three controls, all in the **mod** group:

```
source  ──▶  destination  ×  depth(scale)  ──▶  bends that parameter every tick
(cv1/cv2 from)  (cv in 1/2)     (cv1/cv2 scale)
```

## 1. Source — `cv1 from` / `cv2 from`

Where the modulating voltage comes from:

- `CV1` / `CV2` — the physical front-panel CV jacks (bipolar on O_C: 0 V =
  neutral, ± ranges around it).
- `out1 … out4` — the **live voltage of another output**, folded back as a
  modulator. (This is PNC’s cross-channel routing — no patch cables needed for
  internal modulation.)

## 2. Destination — `cv in 1` / `cv in 2`

Which parameter the source bends:

| Destination | What gets modulated |
|-------------|---------------------|
| `--` | off |
| `range` | slides the whole `cv min`/`cv max` window up/down (span preserved, clamped to 0–10 V) — e.g. an LFO turning a pulse channel into a moving CV |
| `width` | gate width / woggle speed / rwalk step |
| `phase` | phase offset |
| `delay` | swing / gate offset |
| `prob` | fire probability |
| `ratio` | steps through the ratio list — a CV can *play* different divisions |

## 3. Depth & direction — `cv1 scale` / `cv2 scale`

A signed **−128 … +127** amount:

- `0` → no effect (a destination you have not dialed in).
- positive → source voltage pushes the parameter one way;
- negative → the opposite way (flip polarity without an inverter).
- magnitude → how far it reaches.

## Behaviour notes

- Modulation is **additive and applied per-tick** on a working copy of the
  channel state; it never rewrites the stored value, so when the CV returns to
  neutral you’re back where you were.
- `range` modulation keeps the min/max **span** constant and slides it, clamped
  so it never leaves the DAC window or crosses over.
- The two chains are independent — you can modulate `prob` from CV1 and `ratio`
  from an output simultaneously on one channel.

### Idea starters

- `env`'s `prob` from CV1 → density follows a knob/manual CV.
- `step`'s `ratio` from its own `note` output → self-playing rhythm variations.
- `pulse` `phase` from a slow LFO (via `out2`) → channels drift in and out of
  phase for generative interlock.
- `range` from CV2 → play the whole output like a keyboard.

---

Prev: [Euclid & Probability](Euclid-Probability.md) · Next: [Storage & Banks](Storage-Banks.md)
