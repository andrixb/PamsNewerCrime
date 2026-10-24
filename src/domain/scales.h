#pragma once
// Domain: ratio/scale tables plus global patch defaults. Pure data, no
// hardware. Moved verbatim from channel.h / channel.cpp (Phase 1).
#include <stdint.h>
#include "pam_config.h"

struct Ratio {
  const char *label;
  uint32_t period_x256;
};

extern const Ratio RATIO_TABLE[];
extern const uint8_t RATIO_COUNT;
extern const uint8_t SCALE_COUNT;

#define USER_SCALE_COUNT 3
extern uint16_t USER_SCALES[USER_SCALE_COUNT];
// user_scales: USER_SCALE_COUNT entries (borrowed, usually USER_SCALES;
// passed explicitly so the engine does not read the global itself).
uint16_t scale_mask(uint8_t idx, const uint16_t user_scales[USER_SCALE_COUNT]);

// Default ratio applied to every channel by channels_init() (the "defaults"
// SYS action). Set from the SYS page, persisted with the patch.
extern uint8_t DEFAULT_RATIO;
