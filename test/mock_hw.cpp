// Mock hardware backing for the native test build: definitions for the
// symbols the Arduino/Teensy headers declare but production code only calls.
#include "mock/Arduino.h"
#include "mock/EEPROM.h"
#include "dac.h"
#include <string.h>

uint32_t mock_time_us = 0;
EepromMock EEPROM;

// DAC writes are recorded so tests can observe the final output stage.
uint16_t mock_dac_mv[NUM_OUTPUTS];
uint32_t mock_dac_writes = 0;

void dac_init() {}
void dac_write(uint8_t ch, uint16_t mv) {
  if (ch < NUM_OUTPUTS) mock_dac_mv[ch] = mv;
  mock_dac_writes++;
}
void dac_write_all(const uint16_t *vals) {
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) dac_write(i, vals[i]);
}
