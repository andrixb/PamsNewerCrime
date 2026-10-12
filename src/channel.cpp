#include "channel.h"
#include "euclid.h"
#include "dac.h"
#include <string.h>

const Ratio RATIO_TABLE[] = {
  { "1/64", 1536 },
  { "1/13", 1890 },
  { "1/32T", 2048 },
  { "1/11", 2234 },
  { "1/9", 2731 },
  { "1/32", 3072 },
  { "1/7", 3511 },
  { "1/16T", 4096 },
  { "1/32.", 4608 },
  { "1/5", 4915 },
  { "1/16", 6144 },
  { "1/8T", 8192 },
  { "1/16.", 9216 },
  { "1/8", 12288 },
  { "1/4T", 16384 },
  { "1/8.", 18432 },
  { "1/4", 24576 },
  { "1/2T", 32768 },
  { "1/4.", 36864 },
  { "1/2", 49152 },
  { "1/2.", 73728 },
  { "1bar", 98304 },
  { "2bar", 196608 },
  { "4bar", 393216 },
};
const uint8_t RATIO_COUNT = sizeof(RATIO_TABLE) / sizeof(RATIO_TABLE[0]);

Channel channels[NUM_OUTPUTS];

static const uint16_t SINE_LUT[65] = {
  0, 78, 156, 233, 310, 386, 461, 535, 608, 680, 750, 818, 884, 947, 1008, 1066,
  1120, 1171, 1218, 1261, 1300, 1334, 1364, 1389, 1409, 1424, 1434, 1440, 1440, 1434, 1424, 1409,
  1389, 1364, 1334, 1300, 1261, 1218, 1171, 1120, 1066, 1008, 947, 884, 818, 750,
  680, 608, 535, 461, 386, 310, 233, 156, 78, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const uint16_t SCALE_MASKS[] = {
  0xFFF,  // chromatic
  0xAB5,  // major
  0x5AD,  // minor
  0x6AD,  // dorian
  0x5AB,  // phrygian
  0xAD5,  // lydian
  0x6B5,  // mixolydian
  0x9AD,  // harmonic minor
  0x295,  // pentatonic major
  0x4A9,  // pentatonic minor
  0x4E9,  // blues
  0x555,  // whole tone
  0xAAD,  // melodic minor
  0x9B5,  // harmonic major
  0x6CD,  // dorian #4
  0x5B3,  // phrygian dominant
  0x56B,  // locrian
  0x55B,  // altered
  0x6D5,  // lydian dominant
  0x9B3,  // double harmonic
  0xD53,  // enigmatic
  0x18D,  // hirajoshi
  0x1A3,  // in
  0x8C3,  // iwato
  0x655,  // prometheus
  0x4D3,  // tritone
  0xB6D,  // diminished
  0x999,  // augmented
  0xAF5,  // blues major
};
const uint8_t SCALE_COUNT = (sizeof(SCALE_MASKS) / sizeof(SCALE_MASKS[0])) + USER_SCALE_COUNT;

uint16_t USER_SCALES[USER_SCALE_COUNT] = { 0xFFF, 0xAB5, 0x5AD };

// Resolve a scale index (built-in tables, then the 3 editable user slots) to a
// 12-bit semitone mask.
uint16_t scale_mask(uint8_t idx) {
  uint8_t nb = (uint8_t)(SCALE_COUNT - USER_SCALE_COUNT);
  if (idx < nb) return SCALE_MASKS[idx];
  uint8_t u = idx - nb;
  return USER_SCALES[u < USER_SCALE_COUNT ? u : 0];
}

static int quantize_semi(int pos, uint16_t mask) {
  int oct = pos / 12, m = pos % 12;
  for (int d = 0; d <= 6; d++) {
    int lo = m - d;
    if (lo >= 0 && ((mask >> lo) & 1)) return oct * 12 + lo;
    if (lo < 0 && oct > 0 && ((mask >> (lo + 12)) & 1)) return (oct - 1) * 12 + (lo + 12);
    int hi = m + d;
    if (hi <= 11 && ((mask >> hi) & 1)) return oct * 12 + hi;
    if (hi > 11 && ((mask >> (hi - 12)) & 1)) return (oct + 1) * 12 + (hi - 12);
  }
  return pos;
}

static int resolve_in(uint8_t sel, uint8_t cv1, uint8_t cv2) {
#if CV_BIPOLAR
  // O_C front-end: CV1/CV2 jacks are bipolar (0V = midscale ~128); re-center
  // to signed modulation. Cross-patched channel values stay unipolar 0..255.
  if (sel == 0) return (int)(int8_t)(cv1 ^ 0x80);
  if (sel == 1) return (int)(int8_t)(cv2 ^ 0x80);
#else
  if (sel == 0) return cv1;
  if (sel == 1) return cv2;
#endif
  uint8_t ch = sel - 2;
  if (ch >= NUM_OUTPUTS) return 0;
  uint16_t mv = channels[ch].out_mv;
  return mv >= DAC_MAX_MV ? 255 : (uint8_t)((uint32_t)mv * 255 / DAC_MAX_MV);
}

static void apply_cv(ChannelParams &p, uint8_t src, int8_t scale, int val) {
  if (!src || !scale) return;
  int32_t m = (int32_t)scale * val / 2;
  switch (src) {
    case CVP_RANGE: {
      int32_t span = (int32_t)p.cv_max - p.cv_min;
      if (span < 0) span = 0;
      int32_t c = ((int32_t)p.cv_max + p.cv_min) / 2 + m;
      if (c - span / 2 < 0) c = span / 2;
      if (c + span / 2 > DAC_MAX_MV) c = DAC_MAX_MV - span / 2;
      p.cv_min = c - span / 2;
      p.cv_max = c + span / 2;
      break;
    }
    case CVP_WIDTH: p.width  = constrain(p.width + (m >> 4), 1, 255); break;
    case CVP_PHASE: p.phase  = constrain(p.phase + (m >> 4), 0, 255); break;
    case CVP_DELAY: p.delay  = constrain(p.delay + (m >> 4), 0, 255); break;
    case CVP_PROB:  p.prob   = constrain(p.prob + (m >> 6), 0, 100); break;
    case CVP_RATIO: p.ratio  = constrain((int)p.ratio + (m >> 7), 0, RATIO_COUNT - 1); break;
    case CVP_DIV:   p.div_a  = constrain((int)p.div_a + (m >> 7), -32, 32); break;
    case CVP_SKEW:  p.skew   = constrain(p.skew + (m >> 4), 0, 255); break;
    case CVP_ATK:   p.env_a  = constrain(p.env_a + (m >> 4), 0, 255); break;
    case CVP_DEC:   p.env_d  = constrain(p.env_d + (m >> 4), 0, 255); break;
    case CVP_SLEW:  p.slew   = constrain(p.slew + (m >> 4), 0, 255); break;
    default: break;
  }
}

void channels_init() {
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
    Channel &c = channels[i];
    memset(&c.p, 0, sizeof(c.p));
    c.p.ratio = 6;
    c.p.mode = MODE_PULSE;
    c.p.width = 64;
    c.p.skew = 128;
    c.p.cv_min = 0;
    c.p.cv_max = 3000;
    c.p.prob = 100;
    c.p.eucl_len = 8;
    c.p.eucl_fill = 4;
    c.p.step_len = 8;
    for (int s = 0; s < MAX_STEPS; s++) { c.p.steps[s] = (s % 4) * 64; c.p.step_prob[s] = 100; }
    channel_rebuild_euclid(c);
  }
  channels_reset();
}

void channels_reset() {
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
    Channel &c = channels[i];
    c.pos_x256 = 0;
    c.event_count = 0;
    c.gate_state = 0;
    c.step_idx = 0;
    c.sh_val = 1;

    c.env_t = 0;
    c.wv = 128;
    c.dir = 1;
    c.last_note = 0;
    c.out_mv = 0;
    c.filt_mv = 0;
    c.hold_mv = 0;
    c.x_hi = 0;
    c.ev_off = 0;
    c.pend = 0;
    c.rate256 = 256;
    dac_write(i, 0);
  }
}

void channel_rebuild_euclid(Channel &c) {
  uint8_t len = c.p.eucl_len ? c.p.eucl_len : 1;
  if (len > EUCLID_MAX_LEN) len = EUCLID_MAX_LEN;
  uint8_t fill = c.p.eucl_fill;
  if (fill > len) fill = len;
  uint8_t pat[EUCLID_MAX_LEN];
  euclid(fill, len, pat);
  uint32_t mask = 0;
  for (uint8_t k = 0; k < len; k++)
    if (pat[k]) mask |= (1u << k);
  c.mask = mask;
}

static uint32_t rng = 0x9e3779b9;
static uint8_t rand8() {
  rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
  return rng >> 24;
}

static uint16_t wave_value(uint8_t wave, uint8_t shape, uint32_t ph256) {
  uint8_t ph = ph256 & 0xFF;
  switch (wave) {
    case LFO_SINE: return SINE_LUT[ph >> 2];
    case LFO_TRI: {
      uint32_t rise = constrain((int16_t)shape, 1, 255);
      if (ph < rise) return ph * 2048 / rise;
      return (255 - ph) * 2048 / (256 - rise);
    }
    case LFO_SAW:  return (uint32_t)ph * 2048 / 255;
    case LFO_SQR:  return ph < shape ? 2048 : 0;
    case LFO_TRAPZ: {
      // Plateau edges move with `shape`: ramp width = shape/2 at each end.
      uint32_t ramp = (uint32_t)constrain((int16_t)shape, 1, 127);
      if (ph < ramp) return ph * 2048 / ramp;
      if (ph < 256 - ramp) return 2048;
      return (uint32_t)(255 - ph) * 2048 / ramp;
    }
    case LFO_PARAB: {
      int32_t x = 2 * (int32_t)ph - 255;
      return (uint32_t)(4 * 16320 - x * x) * 2048 / (4 * 16320);
    }
    default: return 0;
  }
}

uint16_t channel_value(const Channel &c) {
  return c.out_mv;
}

// Effective event period for the clock div/mult app: quarter note scaled by
// both stages. Positive factor f divides the rate (period *= f), negative
// multiplies it (period /= |f|). Clamped to keep uint32 phase math safe.
static uint32_t div_period(const ChannelParams &p) {
  uint64_t per = (uint64_t)TICKS_PER_QUARTER * 256;
  int8_t fs[2] = { p.div_a, p.div_b };
  for (uint8_t k = 0; k < 2; k++) {
    int f = fs[k];
    if (f > 1) per *= (uint32_t)f;
    else if (f < -1) per /= (uint32_t)(-f);
  }
  if (per < 4) per = 4;              // ~x1024 max multiplication against 1bar
  if (per > 24576ULL * 4096) per = 24576ULL * 4096;
  return (uint32_t)per;
}

static int32_t scaled(const ChannelParams &p, uint16_t frac) {
  int32_t v = (int32_t)p.cv_min + (int32_t)frac * (p.cv_max - p.cv_min) / 2048;
  if (p.invert) v = (int32_t)p.cv_min + (p.cv_max - v);
  return constrain(v, 0, DAC_MAX_MV);
}

void channels_update(uint32_t delta_x256, uint8_t cv1, uint8_t cv2) {
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
    Channel &c = channels[i];
    ChannelParams p = c.p;

    if (p.mode == MODE_OFF) {
      if (c.out_mv) { c.out_mv = 0; dac_write(i, 0); }
      c.gate_state = 0;
      c.pos_x256 += delta_x256;
      continue;
    }

    apply_cv(p, p.cv_src1, p.cv_scale1, resolve_in(p.cv_in1, cv1, cv2));
    apply_cv(p, p.cv_src2, p.cv_scale2, resolve_in(p.cv_in2, cv1, cv2));

    uint32_t period = p.mode == MODE_DIV ? div_period(p)
                                         : RATIO_TABLE[p.ratio].period_x256;
    uint32_t phase_off = (uint32_t)p.phase * period / 256;

    // --- FLEX micro-timing, offset part: human + swing shift the gate start ---
    if (p.flex == FLEX_HUMAN && c.ev_off) phase_off = (phase_off + (uint32_t)c.ev_off * period / 256) % period;
    else if (p.flex == FLEX_SWING && (c.event_count & 1))
      phase_off = (phase_off + (uint32_t)p.flex_amt * period / 512) % period;

    // --- FLEX, rate part: accel / hump / falling scale the phase increment ---
    uint32_t d = delta_x256;
    if (p.flex_amt) {
      uint32_t ph0_256 = (uint32_t)(((uint64_t)(c.pos_x256 + phase_off) % period) * 256 / period);
      uint32_t rate = 256;
      if (p.flex == FLEX_ACCEL)       rate = c.rate256;
      else if (p.flex == FLEX_HUMP)   rate = 256 + (uint32_t)p.flex_amt * (ph0_256 * (256 - ph0_256) / 64) / 256;
      else if (p.flex == FLEX_FALL)   rate = 256 + (uint32_t)p.flex_amt * ph0_256 / 256;
      if (rate > 256) d = (uint32_t)(((uint64_t)d * rate) >> 8);
    }

    uint32_t e_old = (c.pos_x256 + phase_off) / period;
    c.pos_x256 += d;
    uint32_t e_new = (c.pos_x256 + phase_off) / period;
    uint32_t events = e_new - e_old;
    c.pos_x256 %= period;

    bool ev = events > 0;
    bool fire = ev;   // true only if this event passes euclid/probability
    if (ev) {
      c.event_count += events;
      if (p.eucl_en && p.eucl_len) {
        uint8_t loop = p.eucl_loop >= p.eucl_len ? p.eucl_loop : p.eucl_len;
        uint8_t idx = (uint8_t)((c.event_count - 1 + p.eucl_rot) % loop);
        fire = (idx < p.eucl_len) && ((c.mask >> idx) & 1);
      }
      if (fire && p.prob < 100 && (rand8() % 100) >= p.prob) fire = false;
      // Re-roll the per-event FLEX offsets/rate on each real event.
      if (p.flex == FLEX_HUMAN) c.ev_off = rand8() % ((uint16_t)p.flex_amt + 1);
      if (p.flex == FLEX_ACCEL) {
        if (c.event_count % 8 == 0) c.rate256 = 256;
        else c.rate256 = 256 + (uint16_t)(((uint32_t)p.flex_amt * (c.event_count % 8)) / 8);
      }
      // Latch the gate-open flag once per raw event for windowed gate modes:
      // an event that passes euclid/probability opens the window, a skipped
      // one closes it. (STEP/SH reuse sh_val as a value store, so skip those.)
      if (p.mode == MODE_PULSE || p.mode == MODE_DIV ||
          p.mode == MODE_LOGIC || p.mode == MODE_RATCHET)
        c.sh_val = fire ? 1 : 0;
    }

    uint32_t ph = (c.pos_x256 + phase_off) % period;
    uint32_t ph256 = ph * 256 / period;
    uint16_t frac = 0;

    switch (p.mode) {
      case MODE_PULSE: case MODE_DIV: {
        uint32_t gate_ph = (c.pos_x256 + period - (uint32_t)p.delay * period / 256) % period;
        uint32_t g256 = gate_ph * 256 / period;
        uint32_t w256 = p.width ? p.width : 1;
        bool on = c.sh_val && g256 < w256;
        c.gate_state = on;
        frac = on ? 2048 : 0;
        break;
      }
      case MODE_RATCHET: {
        uint8_t n = p.ratchets ? constrain((int)p.ratchets, 1, 16) : 1;
        uint32_t sub = 256 / n;
        uint32_t r256 = (ph256 % sub) * 256 / sub;
        bool on = c.sh_val && r256 < (uint32_t)(p.width ? p.width : 64);
        c.gate_state = on;
        frac = on ? 2048 : 0;
        break;
      }
      case MODE_LFO: {
        frac = wave_value(p.wave, p.skew ? p.skew : 128, ph256);
        c.gate_state = frac > 1024;
        break;
      }
      case MODE_RAMP: {
        uint32_t rise = constrain((int16_t)(p.skew ? p.skew : 128), 1, 254);
        if (ph256 < rise) frac = ph256 * 2048 / rise;
        else frac = (255 - ph256) * 2048 / (255 - rise);
        c.gate_state = ph256 < rise;
        break;
      }
      case MODE_STEP: {
        bool fired = fire;
        if (fire) {
          c.step_idx++;
          uint8_t slen = p.step_len ? p.step_len : 1;
          c.step_idx %= slen;
          // Per-step probability: a failed roll skips this step (index still
          // advances) so the sequence plays with a held value and no trigger.
          if (p.step_prob[c.step_idx % MAX_STEPS] < 100 &&
              (rand8() % 100) >= p.step_prob[c.step_idx % MAX_STEPS]) {
            fired = false;
          } else {
            c.sh_val = c.p.steps[c.step_idx % MAX_STEPS];
          }
        }
        frac = (uint32_t)c.sh_val * 2048 / 255;
        c.gate_state = fired;
        break;
      }
      case MODE_SH: {
        if (fire) c.sh_val = rand8();
        frac = (uint32_t)c.sh_val * 2048 / 255;
        c.gate_state = fire;
        break;
      }
      case MODE_NOTE: {
        uint32_t span = p.cv_max > p.cv_min ? (uint32_t)(p.cv_max - p.cv_min) : 0;
        uint32_t st = span / 83;
        if (st < 1) st = 1;
        int pos = (int)((st * ph256) >> 8);
        int q = quantize_semi(pos, scale_mask(p.scale % SCALE_COUNT));
        frac = (uint16_t)((uint32_t)q * 2048 / st);
        if (frac > 2048) frac = 2048;
        uint8_t note = q > 255 ? 255 : (uint8_t)q;
        c.gate_state = (note != c.last_note);
        c.last_note = note;
        break;
      }
      case MODE_QUANT: {
        // 1V/oct quantiser: read the selected source (CV1/CV2 jack or another
        // output), map 0..255 across the channel's cv window, snap to the scale.
        uint32_t span = p.cv_max > p.cv_min ? (uint32_t)(p.cv_max - p.cv_min) : 0;
        uint32_t st = span / 83;           // semitones available in the window
        if (st < 1) st = 1;
        int rv = resolve_in(p.quant_src, cv1, cv2);
        if (p.quant_src < 2) rv += 128;    // bipolar jacks: mid-scale -> 0..255
        rv = constrain(rv, 0, 255);
        int pos = (int)((uint32_t)rv * st / 255);
        int q = quantize_semi(pos, scale_mask(p.scale % SCALE_COUNT));
        frac = (uint16_t)((uint32_t)q * 2048 / st);
        if (frac > 2048) frac = 2048;
        uint8_t note = q > 255 ? 255 : (uint8_t)q;
        c.gate_state = (note != c.last_note);
        c.last_note = note;
        break;
      }
      case MODE_ENV: {
        uint32_t a = p.env_a ? (uint32_t)p.env_a * 32 : 1;
        uint32_t d = p.env_d ? (uint32_t)p.env_d * 32 : 1;
        if (fire) { c.env_t = 0; c.gate_state = 1; }
        if (c.gate_state) {
          c.env_t++;
          if (c.env_t <= a) frac = (uint16_t)((uint32_t)c.env_t * 2048 / a);
          else if (c.env_t <= a + d) frac = (uint16_t)((a + d - c.env_t) * 2048 / d);
          else { c.gate_state = 0; frac = 0; }
        }
        break;
      }
      case MODE_LOGIC: {
        uint32_t gate_ph = (c.pos_x256 + period - (uint32_t)p.delay * period / 256) % period;
        uint32_t g256 = gate_ph * 256 / period;
        uint32_t w256 = p.width ? p.width : 1;
        bool own = c.sh_val && g256 < w256;
        bool other = (p.logic_src >= 1 && p.logic_src <= NUM_OUTPUTS)
                     && channels[p.logic_src - 1].gate_state;
        bool on;
        switch (p.logic_op) {
          case LOGIC_AND: on = own && other; break;
          case LOGIC_OR:  on = own || other; break;
          case LOGIC_XOR: on = own != other; break;
          case LOGIC_NAND:on = !(own && other); break;
          case LOGIC_NOR: on = !(own || other); break;
          default:        on = own; break;
        }
        c.gate_state = on;
        frac = on ? 2048 : 0;
        break;
      }
      case MODE_WGGLE: {
        uint8_t r = rand8();
        if (fire || r < p.skew) {
          c.wv = rand8();
          c.dir = (rand8() & 1) ? 1 : -1;
        } else {
          c.wv += c.dir * ((int16_t)(p.width >> 4) + 1);
          if (c.wv > 255) { c.wv = 255; c.dir = -1; }
          if (c.wv < 0)   { c.wv = 0;   c.dir = 1; }
        }
        frac = (uint16_t)(c.wv + 1) * 8;
        c.gate_state = c.dir > 0;
        break;
      }
      case MODE_RWALK: {
        int16_t step = (int16_t)(p.width >> 4) + 1;
        c.wv += (rand8() & 1) ? step : -step;
        if (c.wv > 255) c.wv = 255;
        if (c.wv < 0) c.wv = 0;
        frac = (uint16_t)(c.wv + 1) * 8;
        c.gate_state = fire;
        break;
      }
      default:
        break;
    }

    uint16_t mv = scaled(p, frac);

    // --- Cross-op: combine this output's value with a source (CV jack or out) ---
    if (p.xop) {
      uint16_t a;
      if (p.xsrc < 2) {                 // CV jack: unipolar 0..255 ADC -> mV
        uint16_t adc = (p.xsrc == 0) ? cv1 : cv2;   // resolve_in already 0..255 scaled
        a = (uint16_t)((uint32_t)adc * DAC_MAX_MV / 255);
      } else {
        uint8_t ch = p.xsrc - 2;
        a = ch < NUM_OUTPUTS ? channels[ch].out_mv : 0;
      }
      bool ahi = a >= (DAC_MAX_MV / 2);  // gate-high threshold for logic ops
      bool rise = (a >= (DAC_MAX_MV / 10)) && !c.x_hi;
      c.x_hi = (a >= (DAC_MAX_MV / 10));
      uint32_t t;
      switch (p.xop) {
        case XOP_MIX:  mv = (uint16_t)(((uint32_t)mv + a) / 2); break;
        case XOP_MASK: if (a < (DAC_MAX_MV / 10)) mv = 0; break;
        case XOP_NOT:  if (a >= (DAC_MAX_MV / 10)) mv = 0; break;  // inverse of MASK
        case XOP_MULT: mv = (uint16_t)(((uint32_t)mv * a) / DAC_MAX_MV); break;
        case XOP_HOLD: if (a < (DAC_MAX_MV / 10)) mv = c.hold_mv; else c.hold_mv = mv; break;
        case XOP_SH:   if (rise) c.hold_mv = mv; mv = c.hold_mv; break;
        case XOP_ADD:  t = (uint32_t)mv + a; mv = t > DAC_MAX_MV ? DAC_MAX_MV : (uint16_t)t; break;
        case XOP_SUB:  t = (int32_t)mv - (int32_t)a; mv = t < 0 ? 0 : (uint16_t)t; break;
        case XOP_AND:  if (!(mv >= (DAC_MAX_MV / 10) && ahi)) mv = 0; break;
        case XOP_OR:   if (ahi) mv = mv > a ? mv : a; break;
        case XOP_XOR:  if (ahi == (mv >= (DAC_MAX_MV / 10))) mv = 0; break;
        case XOP_SEED: if (rise) rng = 0x9e3779b9u ^ (uint32_t)(mv + 1) * 2654435761u; break;
        default: break;
      }
    }

    // Global quantise: snap the final value onto the selected scale within the
    // channel's cv window (works on any mode as a post-filter).
    if (p.gquant) {
      uint32_t span = p.cv_max > p.cv_min ? (uint32_t)(p.cv_max - p.cv_min) : 0;
      uint32_t st = span / 83;
      if (st < 1) st = 1;
      int semis = (int)(((uint32_t)(mv > p.cv_min ? mv - p.cv_min : 0) * st) / (span ? span : 1));
      int q = quantize_semi(semis, scale_mask(p.scale));
      int32_t m = (int32_t)q * 83;
      if (m > p.cv_max - p.cv_min) m = p.cv_max - p.cv_min;
      mv = (uint16_t)(p.cv_min + m);
    }

    if (p.slew && p.mode != MODE_PULSE && p.mode != MODE_DIV && p.mode != MODE_RATCHET && p.mode != MODE_LOGIC) {
      int32_t diff = (int32_t)mv - (int32_t)c.filt_mv;
      c.filt_mv += (int16_t)((diff * (int32_t)p.slew) >> 8);
      mv = c.filt_mv;
    } else {
      c.filt_mv = mv;
    }
    c.out_mv = mv;
    dac_write(i, mv);
  }
}
