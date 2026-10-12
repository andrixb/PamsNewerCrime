#include "channel.h"
#include "euclid.h"
#include "dac.h"
#include <string.h>

const Ratio RATIO_TABLE[] = {
  { "1/32", 3072 },
  { "1/16", 6144 },
  { "1/16T", 4096 },
  { "1/8", 12288 },
  { "1/8T", 8192 },
  { "1/8.", 18432 },
  { "1/4", 24576 },
  { "1/4T", 16384 },
  { "1/4.", 36864 },
  { "1/2", 49152 },
  { "1/2T", 32768 },
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
    for (int s = 0; s < MAX_STEPS; s++) c.p.steps[s] = (s % 4) * 64;
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
    c.skip_next = 1;
    c.out_mv = 0;
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
    default: return 0;
  }
}

uint16_t channel_value(const Channel &c) {
  return c.out_mv;
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

    apply_cv(p, p.cv_src1, p.cv_scale1, cv1);
    apply_cv(p, p.cv_src2, p.cv_scale2, cv2);

    uint32_t period = RATIO_TABLE[p.ratio].period_x256;
    uint32_t phase_off = (uint32_t)p.phase * period / 256;

    uint32_t e_old = (c.pos_x256 + phase_off) / period;
    c.pos_x256 += delta_x256;
    uint32_t e_new = (c.pos_x256 + phase_off) / period;
    uint32_t events = e_new - e_old;
    c.pos_x256 %= period;

    bool fire = events > 0;
    if (fire) {
      c.skip_next = !fire;
      c.event_count += events;
      if (p.eucl_en && p.eucl_len) {
        uint8_t idx = (c.event_count - 1 + p.eucl_rot) % p.eucl_len;
        fire = (c.mask >> idx) & 1;
      }
      if (fire && p.prob < 100 && (rand8() % 100) >= p.prob) { fire = false; c.skip_next = 1; }
    }

    uint32_t ph = (c.pos_x256 + phase_off) % period;
    uint32_t ph256 = ph * 256 / period;
    uint16_t frac = 0;

    switch (p.mode) {
      case MODE_PULSE: {
        uint32_t gate_ph = (c.pos_x256 + period - (uint32_t)p.delay * period / 256) % period;
        uint32_t g256 = gate_ph * 256 / period;
        uint32_t w256 = p.width ? p.width : 1;
        if (fire) c.sh_val = c.skip_next ? 0 : 1;
        bool on = c.sh_val && g256 < w256;
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
        if (fire) {
          c.step_idx++;
          uint8_t slen = p.step_len ? p.step_len : 1;
          c.step_idx %= slen;
          c.sh_val = c.p.steps[c.step_idx % MAX_STEPS];
        }
        frac = (uint32_t)c.sh_val * 2048 / 255;
        c.gate_state = fire;
        break;
      }
      case MODE_SH: {
        if (fire) c.sh_val = rand8();
        frac = (uint32_t)c.sh_val * 2048 / 255;
        c.gate_state = fire;
        break;
      }
      default:
        break;
    }

    uint16_t mv = scaled(p, frac);
    c.out_mv = mv;
    dac_write(i, (uint32_t)mv * 4095 / DAC_MAX_MV);
  }
}
