#pragma once
#include <stdint.h>
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
  uint8_t out_dirty; // output stage: set when out_mv needs a DAC write
};

extern Channel channels[NUM_OUTPUTS];

// Explicit engine environment: everything the update path needs beyond the
// channel patch/state itself. user_scales borrows USER_SCALE_COUNT entries
// (normally USER_SCALES); the engine never reads that global directly.
struct EngineEnv {
  const uint16_t *user_scales;
};

void channels_init(uint8_t default_ratio);
void channels_reset();
void channel_rebuild_euclid(Channel &c);
// Pure computation: advances engine state and marks dirty outputs. Performs
// no I/O; call channels_take_outputs() + dac_write() to emit.
void channels_update(uint32_t master_delta_x256, uint8_t cv1, uint8_t cv2,
                     const EngineEnv &env);
// Drains dirty outputs into out_mv[] (caller-sized NUM_OUTPUTS), clears the
// flags, and returns a bitmask of channels that need a DAC write.
uint8_t channels_take_outputs(uint16_t out_mv[NUM_OUTPUTS]);
uint16_t channel_value(const Channel &c);
