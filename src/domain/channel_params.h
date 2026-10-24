#pragma once
// Domain: pure patch vocabulary (modes, params). No hardware, no engine
// state, no I/O. Moved verbatim from channel.h (Phase 1).
#include <stdint.h>
#include "pam_config.h"

enum OutMode : uint8_t {
  MODE_OFF = 0,
  MODE_PULSE,
  MODE_LFO,
  MODE_RAMP,
  MODE_STEP,
  MODE_SH,
  MODE_NOTE,
  MODE_ENV,
  MODE_LOGIC,
  MODE_WGGLE,
  MODE_RWALK,
  MODE_DIV,
  MODE_QUANT,
  MODE_RATCHET,
  MODE_LAST
};

enum LogicOp : uint8_t {
  LOGIC_OFF = 0,
  LOGIC_AND,
  LOGIC_OR,
  LOGIC_XOR,
  LOGIC_NAND,
  LOGIC_NOR,
  LOGIC_LAST
};

// Cross-ops (Pro Workout): combine this output's final value with a source
// (CV jack or another output), applied to ANY mode before the slew filter.
enum XOp : uint8_t {
  XOP_NONE = 0,
  XOP_MIX,
  XOP_MASK,
  XOP_NOT,
  XOP_MULT,
  XOP_HOLD,
  XOP_SH,
  XOP_ADD,
  XOP_SUB,
  XOP_AND,
  XOP_OR,
  XOP_XOR,
  XOP_SEED,
  XOP_LAST
};

// FLEX micro-timing: per-channel timing distortion of the event grid.
enum FlexOp : uint8_t {
  FLEX_NONE = 0,
  FLEX_HUMAN,   // random late offset each event (slop)
  FLEX_SWING,   // delay every other event
  FLEX_ACCEL,   // rate climbs each event, resets every 8
  FLEX_HUMP,    // sinusoidal tempo bulge mid-cycle
  FLEX_FALL,    // gravity: accelerate through the cycle, snap back
  FLEX_LAST
};

enum LfWave : uint8_t { LFO_SINE = 0, LFO_TRI, LFO_SAW, LFO_SQR, LFO_TRAPZ, LFO_PARAB, LFO_LAST };

enum CvParam : uint8_t {
  CVP_NONE = 0,
  CVP_RANGE,
  CVP_WIDTH,
  CVP_PHASE,
  CVP_DELAY,
  CVP_PROB,
  CVP_RATIO,
  CVP_DIV,
  CVP_SKEW,
  CVP_ATK,
  CVP_DEC,
  CVP_SLEW,
  CVP_SMOOTH,
  CVP_BURST,
  CVP_LAST
};

struct ChannelParams {
  uint8_t ratio;
  uint8_t mode;
  uint8_t wave;
  uint8_t width;
  uint8_t skew;
  int16_t cv_min;
  int16_t cv_max;
  uint8_t phase;
  uint8_t delay;
  uint8_t prob;
  uint8_t invert;
  uint8_t eucl_en;
  uint8_t eucl_len;
  uint8_t eucl_fill;
  uint8_t eucl_rot;
  uint8_t eucl_loop;   // pattern repeats per loop cycle, 1..16 (rests after len)
  uint8_t step_len;
  uint8_t steps[MAX_STEPS];
  uint8_t step_prob[MAX_STEPS];  // per-step trigger probability 0..100
  uint8_t cv_src1;
  int8_t cv_scale1;
  uint8_t cv_src2;
  int8_t cv_scale2;
  uint8_t cv_in1;    // modulator source 1: 0=CV1 jack, 1=CV2 jack, 2..9=channel out 1..8
  uint8_t cv_in2;    // modulator source 2
  uint8_t slew;      // lag on CV output, 0=off (time constant ~ 256/slew ms)
  uint8_t scale;     // note mode scale index
  uint8_t env_a;     // env attack, ms = v * 32
  uint8_t env_d;     // env decay, ms = v * 32
  uint8_t logic_op;  // LogicOp
  uint8_t logic_src; // logic partner channel, 0=none, 1..NUM_OUTPUTS
  int8_t div_a;      // clock div app stage 1: +N = divide by N, -N = multiply by N, 0 = 1:1
  int8_t div_b;      // clock div app stage 2
  uint8_t quant_src; // quantiser input: 0=CV1, 1=CV2, 2..=out1.. (same as cv_in)
  uint8_t ratchets;  // ratchet app: pulses per event, 1..16 (0 = 1)
  uint8_t xop;       // XOp cross-operation applied to final value (0=none)
  uint8_t xsrc;      // cross source: 0=CV1, 1=CV2, 2..=out1.. (same as quant_src)
  uint8_t flex;      // FlexOp micro-timing (0=none)
  uint8_t flex_amt;  // flex intensity 0..255
  uint8_t gquant;    // global quantise post-filter: snap final value to `scale`
  uint8_t smooth;    // woggle: lag on the stepped value (S&H -> smooth out)
  uint8_t burst;     // woggle: chance of a random gate per clock event
};
