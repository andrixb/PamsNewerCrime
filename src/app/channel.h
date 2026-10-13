#pragma once
#include <Arduino.h>
#include "pam_config.h"
#include "domain/channel_params.h"
#include "domain/scales.h"

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
