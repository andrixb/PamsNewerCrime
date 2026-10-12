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

enum LfWave : uint8_t { LFO_SINE = 0, LFO_TRI, LFO_SAW, LFO_SQR, LFO_LAST };

struct Ratio {
  const char *label;
  uint32_t period_x256;
};

extern const Ratio RATIO_TABLE[];
extern const uint8_t RATIO_COUNT;
extern const uint8_t SCALE_COUNT;

enum CvParam : uint8_t {
  CVP_NONE = 0,
  CVP_RANGE,
  CVP_WIDTH,
  CVP_PHASE,
  CVP_DELAY,
  CVP_PROB,
  CVP_RATIO,
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
  uint8_t step_len;
  uint8_t steps[MAX_STEPS];
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
  uint8_t skip_next;
  uint16_t env_t;    // envelope ms counter
  uint16_t filt_mv;  // slew filter state
  int16_t wv;        // woggle / random-walk value 0..255
  int8_t dir;        // woggle direction
  uint8_t last_note; // note mode: last quantized semitone for gate detection
};

extern Channel channels[NUM_OUTPUTS];

void channels_init();
void channels_reset();
void channel_rebuild_euclid(Channel &c);
void channels_update(uint32_t master_delta_x256, uint8_t cv1, uint8_t cv2);
uint16_t channel_value(const Channel &c);
