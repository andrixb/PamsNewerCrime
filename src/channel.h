#pragma once
#include <Arduino.h>
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

struct Ratio {
  const char *label;
  uint32_t period_x256;
};

extern const Ratio RATIO_TABLE[];
extern const uint8_t RATIO_COUNT;
extern const uint8_t SCALE_COUNT;

#define USER_SCALE_COUNT 3
extern uint16_t USER_SCALES[USER_SCALE_COUNT];
uint16_t scale_mask(uint8_t idx);

// Default ratio applied to every channel by channels_init() (the "defaults"
// SYS action). Set from the SYS page, persisted with the patch.
extern uint8_t DEFAULT_RATIO;

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

struct Channel {
  ChannelParams p;
  uint32_t pos_x256;
  uint32_t event_count;
  uint32_t mask;
  uint16_t out_mv;
  uint8_t gate_state;
  uint8_t step_idx;
  uint8_t sh_val;
  uint16_t env_t;    // envelope ms counter
  uint16_t filt_mv;  // slew filter state
  int16_t wv;        // woggle / random-walk value 0..255
  uint8_t wtar;      // woggle: stepped random target
  uint8_t wsm;       // woggle: smoothed value (chased by wv)
  uint8_t last_note; // note mode: last quantized semitone for gate detection
  uint16_t hold_mv;  // cross-op hold/s&h storage
  uint8_t x_hi;      // cross-op: source was high last tick (edge detect)
  uint16_t ev_off;   // flex: pending fire offset (256ths of cycle)
  uint8_t pend;      // flex: delayed fire pending
  uint16_t rate256;  // flex: accel rate multiplier (256 = 1x)
};

extern Channel channels[NUM_OUTPUTS];

void channels_init();
void channels_reset();
void channel_rebuild_euclid(Channel &c);
void channels_update(uint32_t master_delta_x256, uint8_t cv1, uint8_t cv2);
uint16_t channel_value(const Channel &c);
