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
  MODE_LAST
};

enum LfWave : uint8_t { LFO_SINE = 0, LFO_TRI, LFO_SAW, LFO_SQR, LFO_LAST };

struct Ratio {
  const char *label;
  uint32_t period_x256;
};

extern const Ratio RATIO_TABLE[];
extern const uint8_t RATIO_COUNT;

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
};

extern Channel channels[NUM_OUTPUTS];

void channels_init();
void channels_reset();
void channel_rebuild_euclid(Channel &c);
void channels_update(uint32_t master_delta_x256, uint8_t cv1, uint8_t cv2);
uint16_t channel_value(const Channel &c);
