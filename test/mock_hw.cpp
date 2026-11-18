// Mock hardware backing for the native test build: definitions for the
// symbols the Arduino/Teensy headers declare but production code only calls.
#include "mock/Arduino.h"
#include "mock/EEPROM.h"
#include "infra/dac.h"
#include <string.h>

uint32_t mock_time_us = 0;
uint64_t mock_pins_low = 0;
int mock_irq_masked = 0;
int mock_irq_mask_count = 0;
EepromMock EEPROM;

// Time port for the app clock: tests advance mock_time_us between ticks.
// mock_now_hook (one-shot) runs AFTER the time is sampled and before it is
// returned: it models an interrupt (e.g. a TR1 pulse) landing between the
// caller's time read and whatever it reads next.
void (*mock_now_hook)() = nullptr;
uint32_t clock_now_us() {
  const uint32_t t = mock_time_us;
  if (mock_now_hook) {
    void (*h)() = mock_now_hook;
    mock_now_hook = nullptr;
    h();
  }
  return t;
}

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
