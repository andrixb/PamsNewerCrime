#include "domain/scales.h"

// Ratio + scale tables moved verbatim from channel.cpp (Phase 1). Pure data;
// the engine in app/channel.cpp only reads these through the declarations in
// domain/scales.h.

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
uint8_t DEFAULT_RATIO = 6;   // channels_init() default (kept from the old hard-coded ratio)

// Resolve a scale index (built-in tables, then the 3 editable user slots) to a
// 12-bit semitone mask.
uint16_t scale_mask(uint8_t idx, const uint16_t user_scales[USER_SCALE_COUNT]) {
  uint8_t nb = (uint8_t)(SCALE_COUNT - USER_SCALE_COUNT);
  if (idx < nb) return SCALE_MASKS[idx];
  uint8_t u = idx - nb;
  return user_scales[u < USER_SCALE_COUNT ? u : 0];
}
