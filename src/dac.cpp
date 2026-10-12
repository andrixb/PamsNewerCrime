#include "dac.h"
#include <SPI.h>

// ---------------------------------------------------------------------------
// O_C hardware: DAC8565 quad 16-bit DAC, shares the SPI bus with the SH1106
// OLED. Output stage inverts, so the firmware pre-inverts too (matches
// Phazerville/O_C: raw = 0xFFFF - value). Frame is 24-bit:
//   byte0 = 0b00010000 | (channel << 1)   [write+update, ch A..D]
//   byte1..2 = 16-bit value, MSB first
// ---------------------------------------------------------------------------
void dac_init() {
  SPI.begin();
  pinMode(PIN_DAC_CS, OUTPUT);
  digitalWrite(PIN_DAC_CS, HIGH);
  pinMode(PIN_DAC_RST, OUTPUT);
  digitalWrite(PIN_DAC_RST, HIGH);   // 8565: RST high = normal operation
  delay(1);
}

void dac_write(uint8_t ch, uint16_t mv) {
  if (ch >= NUM_OUTPUTS) return;
  if (mv > DAC_MAX_MV) mv = DAC_MAX_MV;
  uint16_t raw = (uint16_t)(((uint32_t)mv * 65535UL) / DAC_MAX_MV);
  raw = 0xFFFF - raw;                 // output stage is inverting
  uint8_t cmd = 0x10 | (uint8_t)((ch & 3) << 1);
  SPISettings settings(12000000, MSBFIRST, SPI_MODE0);
  SPI.beginTransaction(settings);
  digitalWrite(PIN_DAC_CS, LOW);
  SPI.transfer(cmd);
  SPI.transfer16(raw);
  digitalWrite(PIN_DAC_CS, HIGH);
  SPI.endTransaction();
}

void dac_write_all(const uint16_t *vals) {
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) dac_write(i, vals[i]);
}
