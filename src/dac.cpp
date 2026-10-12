#include "dac.h"
#include <SPI.h>

static const uint8_t CS_PINS[4] = { PIN_DAC_CS0, PIN_DAC_CS1, PIN_DAC_CS2, PIN_DAC_CS3 };
static const uint8_t NUM_CS = NUM_OUTPUTS > 4 ? 4 : (NUM_OUTPUTS + 1) / 2;

void dac_init() {
  SPI.begin();
  for (uint8_t i = 0; i < NUM_CS; i++) {
    pinMode(CS_PINS[i], OUTPUT);
    digitalWrite(CS_PINS[i], HIGH);
  }
}

static void xfer(uint8_t cs, uint16_t frame) {
  digitalWrite(cs, LOW);
  SPI.transfer16(frame);
  digitalWrite(cs, HIGH);
}

void dac_write(uint8_t ch, uint16_t v) {
  if (ch >= NUM_OUTPUTS) return;
  if (v > 4095) v = 4095;
  uint8_t chip = ch >> 1;
  uint8_t ab = ch & 1;
  uint16_t frame = (ab ? 0x8000 : 0) | 0x4000 | 0x1000 | v;
  SPI.beginTransaction(SPISettings(12000000, MSBFIRST, SPI_MODE0));
  xfer(CS_PINS[chip], frame);
  SPI.endTransaction();
}

void dac_write_all(const uint16_t *vals) {
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) dac_write(i, vals[i]);
}
