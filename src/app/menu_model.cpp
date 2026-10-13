#include "app/menu_model.h"
#include <cstdio>

const char *MODE_NAMES[] = { "off", "pulse", "LFO", "ramp", "step", "S&H", "note", "env", "logic", "woggle", "rwalk", "div", "quant", "ratch" };
const char *WAVE_NAMES[] = { "sin", "tri", "saw", "sqr", "trapz", "parab" };
const char *CVP_NAMES[]  = { "--", "range", "width", "phase", "delay", "prob", "ratio", "div", "skew", "attack", "decay", "slew", "smooth", "burst" };
const char *SCALE_NAMES[] = { "chro", "major", "minor", "dorian", "phryg", "lydian", "mixolyd", "harm", "penta", "pentmn", "blues", "whole", "melmin", "harmaj", "dor#4", "phrdom", "loc", "alter", "lyddom", "dblharm", "enigm", "hiraj", "in", "iwato", "prom", "triton", "dim", "aug", "blsmaj", "user1", "user2", "user3" };
const char *LOGIC_NAMES[] = { "--", "and", "or", "xor", "nand", "nor" };
const char *XOP_NAMES[] = { "--", "mix", "mask", "not", "mult", "hold", "s&h", "add", "sub", "and", "or", "xor", "seed" };
const char *FLEX_NAMES[] = { "off", "human", "swing", "accel", "hump", "fall" };
const char *SRC_NAMES[2] = { "int", "ext" };

const char *SYS_ACTIONS[] = { "save bank A", "save bank B", "load bank A", "load bank B", "defaults", "set def ratio", "edit scale 1", "edit scale 2", "edit scale 3" };
uint8_t sys_action_count() { return (uint8_t)(sizeof(SYS_ACTIONS) / sizeof(SYS_ACTIONS[0])); }

static const uint8_t A_CLOCK[]  = { P_RATIO, P_PHASE, P_PROB, 0xFF };
static const uint8_t A_EUCLID[] = { P_EUEN, P_EULEN, P_EUFILL, P_EUROT, P_EULOOP, 0xFF };
static const uint8_t A_FLEX[]   = { P_FLEX, P_FLEXA, 0xFF };
static const uint8_t A_XOP[]    = { P_XOP, P_XSRC, 0xFF };
static const uint8_t A_RANGE[]  = { P_CVMIN, P_CVMAX, P_INV, P_SLEW, P_GQ, P_SCALE, 0xFF };
static const uint8_t A_MOD[]    = { P_IN1S, P_CVS1, P_CVSC1, P_IN2S, P_CVS2, P_CVSC2, 0xFF };

// Per-mode list of groups (G_SHAPE always first; G_END terminates).
const uint8_t* mode_groups(uint8_t mode) {
  static const uint8_t off[]   = { G_SHAPE, G_END };
  static const uint8_t pul[]   = { G_SHAPE, G_CLOCK, G_EUCLID, G_FLEX, G_XOP, G_RANGE, G_MOD, G_END };
  static const uint8_t lfo[]   = { G_SHAPE, G_CLOCK, G_RANGE, G_XOP, G_MOD, G_END };
  static const uint8_t ramp[]  = { G_SHAPE, G_CLOCK, G_RANGE, G_XOP, G_END };
  static const uint8_t step[]  = { G_SHAPE, G_CLOCK, G_EUCLID, G_FLEX, G_XOP, G_RANGE, G_MOD, G_END };
  static const uint8_t sh[]    = { G_CLOCK, G_EUCLID, G_FLEX, G_XOP, G_RANGE, G_MOD, G_END };
  static const uint8_t note[]  = { G_SHAPE, G_CLOCK, G_EUCLID, G_FLEX, G_XOP, G_RANGE, G_MOD, G_END };
  static const uint8_t env[]   = { G_SHAPE, G_CLOCK, G_EUCLID, G_FLEX, G_XOP, G_RANGE, G_MOD, G_END };
  static const uint8_t logic[] = { G_SHAPE, G_CLOCK, G_EUCLID, G_FLEX, G_XOP, G_RANGE, G_MOD, G_END };
  static const uint8_t wg[]    = { G_SHAPE, G_CLOCK, G_FLEX, G_XOP, G_RANGE, G_MOD, G_END };
  static const uint8_t rw[]    = { G_SHAPE, G_CLOCK, G_EUCLID, G_FLEX, G_XOP, G_RANGE, G_MOD, G_END };
  static const uint8_t dv[]    = { G_SHAPE, G_CLOCK, G_EUCLID, G_FLEX, G_XOP, G_RANGE, G_MOD, G_END };
  static const uint8_t qt[]    = { G_SHAPE, G_RANGE, G_XOP, G_MOD, G_END };
  switch (mode) {
    case MODE_PULSE: return pul;
    case MODE_LFO:   return lfo;
    case MODE_RAMP:  return ramp;
    case MODE_STEP:  return step;
    case MODE_SH:    return sh;
    case MODE_NOTE:  return note;
    case MODE_ENV:   return env;
    case MODE_LOGIC: return logic;
    case MODE_WGGLE: return wg;
    case MODE_RWALK: return rw;
    case MODE_DIV:   return dv;
    case MODE_QUANT: return qt;
    case MODE_RATCHET: return pul;
    default:         return off;
  }
}

const uint8_t* shape_arr(uint8_t mode) {
  switch (mode) {
    case MODE_PULSE: { static const uint8_t v[] = { P_WIDTH, P_DELAY, 0xFF };  return v; }
    case MODE_LFO:   { static const uint8_t v[] = { P_WAVE, P_SKEW, 0xFF };    return v; }
    case MODE_RAMP:  { static const uint8_t v[] = { P_SKEW, 0xFF };            return v; }
    case MODE_NOTE:  { static const uint8_t v[] = { P_SCALE, 0xFF };           return v; }
    case MODE_ENV:   { static const uint8_t v[] = { P_ENV_A, P_ENV_D, 0xFF };  return v; }
    case MODE_LOGIC: { static const uint8_t v[] = { P_LGOP, P_LGSRC, P_WIDTH, P_DELAY, 0xFF }; return v; }
    case MODE_WGGLE: { static const uint8_t v[] = { P_WIDTH, P_SKEW, P_SMOOTH, P_BURST, 0xFF }; return v; }
    case MODE_RWALK: { static const uint8_t v[] = { P_WIDTH, 0xFF };           return v; }
    case MODE_DIV:   { static const uint8_t v[] = { P_DIVA, P_DIVB, P_WIDTH, P_DELAY, 0xFF }; return v; }
    case MODE_QUANT: { static const uint8_t v[] = { P_QSRC, P_SCALE, 0xFF };   return v; }
    case MODE_RATCHET: { static const uint8_t v[] = { P_RCH, P_WIDTH, P_DELAY, 0xFF }; return v; }
    default:         { static const uint8_t v[] = { 0xFF };                    return v; }
  }
}

uint8_t arr_len(const uint8_t *a) { uint8_t n = 0; while (a[n] != 0xFF) n++; return n; }

// Number of items in one group. Steps mode expands P_STEP0 into step_len slots.
uint8_t group_count(uint8_t code, uint8_t mode, uint8_t slen) {
  switch (code) {
    case G_SHAPE:
      if (mode == MODE_OFF)  return 0;
      if (mode == MODE_STEP) return (uint8_t)(1 + 2 * (slen ? slen : 1));  // seq len + (value,prob) per step
      return arr_len(shape_arr(mode));
    case G_CLOCK:  return arr_len(A_CLOCK);
    case G_EUCLID: return arr_len(A_EUCLID);
    case G_FLEX:   return arr_len(A_FLEX);
    case G_XOP:    return arr_len(A_XOP);
    case G_RANGE:  return arr_len(A_RANGE);
    case G_MOD:    return arr_len(A_MOD);
  }
  return 0;
}
const char* group_name(uint8_t code, uint8_t mode) {
  switch (code) {
    case G_CLOCK:  return "clock";
    case G_EUCLID: return "euclid";
    case G_FLEX:   return "flex";
    case G_XOP:    return "cross";
    case G_RANGE:  return "range";
    case G_MOD:    return "mod";
    default:       return MODE_NAMES[mode];
  }
}
// Resolve position `local` within group `code` to a raw P_* index.
uint8_t group_raw(uint8_t code, uint8_t local, uint8_t mode, uint8_t slen) {
  switch (code) {
    case G_SHAPE:
      if (mode == MODE_STEP) {
        if (local == 0) return P_SLEN;
        uint8_t s = (uint8_t)((local - 1) >> 1);        // step index
        return ((local - 1) & 1) ? (uint8_t)(P_SPROB0 + s) : (uint8_t)(P_STEP0 + s);
      }
      return shape_arr(mode)[local];
    case G_CLOCK:  return A_CLOCK[local];
    case G_EUCLID: return A_EUCLID[local];
    case G_FLEX:   return A_FLEX[local];
    case G_XOP:    return A_XOP[local];
    case G_RANGE:  return A_RANGE[local];
    case G_MOD:    return A_MOD[local];
  }
  return P_RATIO;
}

// Total browsable params for a mode (sum of its groups).
uint8_t chan_param_count(uint8_t mode, uint8_t slen) {
  const uint8_t *g = mode_groups(mode);
  uint8_t t = 0;
  for (; *g != G_END; g++) t += group_count(*g, mode, slen);
  return t;
}
// Flat index -> raw P_* index.
uint8_t app_param_at(uint8_t mode, uint8_t pos, uint8_t slen) {
  const uint8_t *g = mode_groups(mode);
  for (; *g != G_END; g++) {
    uint8_t c = group_count(*g, mode, slen);
    if (pos < c) return group_raw(*g, pos, mode, slen);
    pos -= c;
  }
  return P_RATIO;
}
// Flat index -> its group's display name (for the header).
const char* app_param_group(uint8_t mode, uint8_t pos, uint8_t slen) {
  const uint8_t *g = mode_groups(mode);
  for (; *g != G_END; g++) {
    uint8_t c = group_count(*g, mode, slen);
    if (pos < c) return group_name(*g, mode);
    pos -= c;
  }
  return MODE_NAMES[mode];
}

int16_t menu_get_val(uint8_t ch, uint8_t idx, MenuPage pg) {
  if (pg == MENU_MASTER) {
    switch (idx) {
      case 0: return masterClock.bpm_cent;
      case 1: return masterClock.source;
      case 2: return masterClock.ext_ppqn;
      case 3: return masterClock.swing;
      default: return masterClock.running;
    }
  }
  ChannelParams &p = channels[ch].p;
  if (idx >= P_STEP0 && idx < P_STEP0 + MAX_STEPS) return p.steps[idx - P_STEP0];
  if (idx >= P_SPROB0 && idx < P_SPROB0 + MAX_STEPS) return p.step_prob[idx - P_SPROB0];
  switch (idx) {
    case P_RATIO: return p.ratio;
    case P_MODE: return p.mode;
    case P_WAVE: return p.wave;
    case P_WIDTH: return p.width;
    case P_SKEW: return p.skew;
    case P_CVMIN: return p.cv_min;
    case P_CVMAX: return p.cv_max;
    case P_INV: return p.invert;
    case P_PHASE: return p.phase;
    case P_DELAY: return p.delay;
    case P_PROB: return p.prob;
    case P_EUEN: return p.eucl_en;
    case P_EULEN: return p.eucl_len;
    case P_EUFILL: return p.eucl_fill;
    case P_EUROT: return p.eucl_rot;
    case P_EULOOP: return p.eucl_loop;
    case P_SLEN: return p.step_len;
    case P_CVS1: return p.cv_src1;
    case P_CVSC1: return p.cv_scale1;
    case P_CVS2: return p.cv_src2;
    case P_SLEW: return p.slew;
    case P_SCALE: return p.scale;
    case P_ENV_A: return p.env_a;
    case P_ENV_D: return p.env_d;
    case P_LGOP: return p.logic_op;
    case P_LGSRC: return p.logic_src;
    case P_IN1S: return p.cv_in1;
    case P_IN2S: return p.cv_in2;
    case P_DIVA: return p.div_a;
    case P_DIVB: return p.div_b;
    case P_QSRC: return p.quant_src;
    case P_XOP: return p.xop;
    case P_XSRC: return p.xsrc;
    case P_FLEX: return p.flex;
    case P_FLEXA: return p.flex_amt;
    case P_RCH: return p.ratchets;
    case P_GQ: return p.gquant;
    case P_SMOOTH: return p.smooth;
    case P_BURST: return p.burst;
    default: return p.cv_scale2;
  }
}

void menu_set_val(uint8_t ch, uint8_t idx, int32_t v, MenuPage pg) {
  if (pg == MENU_MASTER) {
    switch (idx) {
      case 0: masterClock.bpm_cent = constrain(v, BPM_MIN, BPM_MAX); break;
      case 1: {
        uint8_t s = (uint8_t)(((v % SRC_COUNT) + SRC_COUNT) % SRC_COUNT);
        if (s != masterClock.source) { masterClock.source = s; masterClock.reset_phase(); }
        break;
      }
      case 2: masterClock.ext_ppqn = constrain(v, 1, 48); break;
      case 3: masterClock.swing = constrain(v, 0, 63); break;
      default: masterClock.running = v ? 1 : 0; break;
    }
    mark_dirty();
    return;
  }
  ChannelParams &p = channels[ch].p;
  bool touch_euclid = false;
  if (idx >= P_STEP0 && idx < P_STEP0 + MAX_STEPS) {
    p.steps[idx - P_STEP0] = constrain(v, 0, 255);
  } else if (idx >= P_SPROB0 && idx < P_SPROB0 + MAX_STEPS) {
    p.step_prob[idx - P_SPROB0] = constrain(v, 0, 100);
  } else switch (idx) {
    case P_RATIO: p.ratio = constrain(v, 0, RATIO_COUNT - 1); break;
    case P_MODE: p.mode = constrain(v, 0, MODE_LAST - 1); break;
    case P_WAVE: p.wave = constrain(v, 0, LFO_LAST - 1); break;
    case P_WIDTH: p.width = constrain(v, 1, 255); break;
    case P_SKEW: p.skew = constrain(v, 0, 255); break;
    case P_CVMIN: p.cv_min = constrain(v, 0, DAC_MAX_MV); if (p.cv_max < p.cv_min) p.cv_max = p.cv_min; break;
    case P_CVMAX: p.cv_max = constrain(v, 0, DAC_MAX_MV); if (p.cv_min > p.cv_max) p.cv_min = p.cv_max; break;
    case P_INV: p.invert = v ? 1 : 0; break;
    case P_PHASE: p.phase = constrain(v, 0, 255); break;
    case P_DELAY: p.delay = constrain(v, 0, 255); break;
    case P_PROB: p.prob = constrain(v, 0, 100); break;
    case P_EUEN: p.eucl_en = v ? 1 : 0; touch_euclid = true; break;
    case P_EULEN: p.eucl_len = constrain(v, 1, EUCLID_MAX_LEN);
                  if (p.eucl_fill > p.eucl_len) p.eucl_fill = p.eucl_len;
                  touch_euclid = true; break;
    case P_EUFILL: p.eucl_fill = constrain(v, 0, p.eucl_len); touch_euclid = true; break;
    case P_EUROT: p.eucl_rot = constrain(v, 0, p.eucl_len ? p.eucl_len - 1 : 0); break;
    case P_EULOOP: p.eucl_loop = constrain(v, 0, EUCLID_MAX_LEN); break;
    case P_SLEN: p.step_len = constrain(v, 1, MAX_STEPS); break;
    case P_CVS1: p.cv_src1 = constrain(v, 0, CVP_LAST - 1); break;
    case P_CVSC1: p.cv_scale1 = constrain(v, -128, 127); break;
    case P_CVS2: p.cv_src2 = constrain(v, 0, CVP_LAST - 1); break;
    case P_SLEW: p.slew = constrain(v, 0, 255); break;
    case P_SCALE: p.scale = constrain(v, 0, SCALE_COUNT - 1); break;
    case P_ENV_A: p.env_a = constrain(v, 0, 255); break;
    case P_ENV_D: p.env_d = constrain(v, 0, 255); break;
    case P_LGOP: p.logic_op = constrain(v, 0, LOGIC_LAST - 1); break;
    case P_LGSRC: p.logic_src = constrain(v, 0, NUM_OUTPUTS); break;
    case P_IN1S: p.cv_in1 = constrain(v, 0, 1 + NUM_OUTPUTS); break;
    case P_IN2S: p.cv_in2 = constrain(v, 0, 1 + NUM_OUTPUTS); break;
    case P_DIVA: p.div_a = constrain(v, -32, 32); break;
    case P_DIVB: p.div_b = constrain(v, -32, 32); break;
    case P_QSRC: p.quant_src = constrain(v, 0, 1 + NUM_OUTPUTS); break;
    case P_XOP: p.xop = constrain(v, 0, XOP_LAST - 1); break;
    case P_XSRC: p.xsrc = constrain(v, 0, 1 + NUM_OUTPUTS); break;
    case P_FLEX: p.flex = constrain(v, 0, FLEX_LAST - 1); break;
    case P_FLEXA: p.flex_amt = constrain(v, 0, 255); break;
    case P_RCH: p.ratchets = constrain(v, 1, 16); break;
    case P_GQ: p.gquant = v ? 1 : 0; break;
    case P_SMOOTH: p.smooth = constrain(v, 0, 255); break;
    case P_BURST: p.burst = constrain(v, 0, 255); break;
    default: p.cv_scale2 = constrain(v, -128, 127); break;
  }
  if (touch_euclid) channel_rebuild_euclid(channels[ch]);
  mark_dirty();
}

const char *menu_label_of(uint8_t ch, uint8_t idx, MenuPage pg) {
  static char buf[14];
  (void)ch;
  if (pg == MENU_MASTER) {
    switch (idx) {
      case 0: return "BPM";
      case 1: return "clock";
      case 2: return "ext ppqn";
      case 3: return "swing";
      default: return "run";
    }
  }
  if (idx >= P_STEP0 && idx < P_STEP0 + MAX_STEPS) {
    snprintf(buf, sizeof(buf), "step %d", idx - P_STEP0 + 1);
    return buf;
  }
  if (idx >= P_SPROB0 && idx < P_SPROB0 + MAX_STEPS) {
    snprintf(buf, sizeof(buf), "s%d prob", idx - P_SPROB0 + 1);
    return buf;
  }
  switch (idx) {
    case P_RATIO: return "ratio";
    case P_MODE: return "mode";
    case P_WAVE: return "wave";
    case P_WIDTH: return "width";
    case P_SKEW: return "skew";
    case P_CVMIN: return "cv min";
    case P_CVMAX: return "cv max";
    case P_INV: return "invert";
    case P_PHASE: return "phase";
    case P_DELAY: return "delay";
    case P_PROB: return "prob";
    case P_EUEN: return "euclid";
    case P_EULEN: return "eu length";
    case P_EUFILL: return "eu fill";
    case P_EUROT: return "eu rot";
    case P_EULOOP: return "eu loop";
    case P_SLEN: return "seq len";
    case P_CVS1: return "cv in 1";
    case P_CVSC1: return "cv1 scale";
    case P_CVS2: return "cv in 2";
    case P_SLEW: return "lag";
    case P_SCALE: return "scale";
    case P_ENV_A: return "attack";
    case P_ENV_D: return "decay";
    case P_LGOP: return "logic";
    case P_LGSRC: return "lg src";
    case P_IN1S: return "cv1 from";
    case P_IN2S: return "cv2 from";
    case P_DIVA: return "div a";
    case P_DIVB: return "div b";
    case P_QSRC: return "q in";
    case P_XOP: return "x op";
    case P_XSRC: return "x src";
    case P_FLEX: return "flex";
    case P_FLEXA: return "flex amt";
    case P_RCH: return "ratchets";
    case P_GQ: return "quantise";
    case P_SMOOTH: return "smooth";
    case P_BURST: return "burst";
    default: return "cv2 scale";
  }
}

void menu_fmt_val(uint8_t ch, uint8_t idx, MenuPage pg, char *buf, size_t n) {
  int16_t v = menu_get_val(ch, idx, pg);
  if (pg == MENU_MASTER) {
    switch (idx) {
      case 0: snprintf(buf, n, "%d.%02d", v / 100, v % 100); break;
      case 1: snprintf(buf, n, "%s", SRC_NAMES[v % SRC_COUNT]); break;
      case 2: snprintf(buf, n, "%d", v); break;
      case 3: snprintf(buf, n, v ? "%d%%" : "off", (int16_t)v * 100 / 63); break;
      default: snprintf(buf, n, v ? "run" : "stop"); break;
    }
    return;
  }
  ChannelParams &p = channels[ch].p;
  if (idx >= P_SPROB0 && idx < P_SPROB0 + MAX_STEPS) { snprintf(buf, n, "%d%%", v); return; }
  switch (idx) {
    case P_RATIO: snprintf(buf, n, "%s", RATIO_TABLE[p.ratio].label); break;
    case P_MODE: snprintf(buf, n, "%s", MODE_NAMES[p.mode]); break;
    case P_WAVE: snprintf(buf, n, "%s", WAVE_NAMES[p.wave]); break;
    case P_WIDTH: case P_SKEW: case P_PHASE: case P_DELAY: snprintf(buf, n, "%d", v); break;
    case P_CVMIN: case P_CVMAX: snprintf(buf, n, "%dmV", v); break;
    case P_PROB: snprintf(buf, n, "%d%%", v); break;
    case P_INV: case P_EUEN: case P_GQ: snprintf(buf, n, v ? "on" : "off"); break;
    case P_CVS1: case P_CVS2: snprintf(buf, n, "%s", CVP_NAMES[v]); break;
    case P_CVSC1: case P_CVSC2: snprintf(buf, n, "%d", v); break;
    case P_SLEW: snprintf(buf, n, v ? "%dms" : "off", v * 8); break;
    case P_SCALE: snprintf(buf, n, "%s", SCALE_NAMES[v]); break;
    case P_ENV_A: case P_ENV_D: snprintf(buf, n, "%dms", v * 32); break;
    case P_LGOP: snprintf(buf, n, "%s", LOGIC_NAMES[v]); break;
    case P_LGSRC: if (v) snprintf(buf, n, "out%d", v); else snprintf(buf, n, "--"); break;
    case P_DIVA: case P_DIVB:
      if (v > 1) snprintf(buf, n, "/%d", v);
      else if (v < -1) snprintf(buf, n, "x%d", -v);
      else snprintf(buf, n, "1:1");
      break;
    case P_QSRC: case P_XSRC:
      if (v == 0) snprintf(buf, n, "CV1");
      else if (v == 1) snprintf(buf, n, "CV2");
      else snprintf(buf, n, "out%d", v - 1);
      break;
    case P_XOP: snprintf(buf, n, "%s", XOP_NAMES[v]); break;
    case P_FLEX: snprintf(buf, n, "%s", FLEX_NAMES[v]); break;
    case P_RCH: snprintf(buf, n, "%dx", v ? v : 1); break;
    case P_EULOOP: snprintf(buf, n, v ? "%d" : "off", v); break;
    case P_IN1S: case P_IN2S:
      if (v == 0) snprintf(buf, n, "CV1");
      else if (v == 1) snprintf(buf, n, "CV2");
      else snprintf(buf, n, "out%d", v - 1);
      break;
    default: snprintf(buf, n, "%d", v); break;
  }
}
