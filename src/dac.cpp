#include "dac.h"
#include <SPI.h>

#if defined(HARDWARE_O_C) && HARDWARE_O_C == 1
// ---------------------------------------------------------------------------
// Genuine O_C hardware: DAC8565 quad 16-bit DAC, shares the SPI bus with the
// SH1106 OLED. Output stage inverts, so the firmware pre-inverts too
// (matches Phazerville/O_C: raw = 0xFFFF - value). Frame is 24-bit:
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

#else
// ---------------------------------------------------------------------------
// DIY profile: MCP4822 dual 12-bit DACs, one CS per chip pair.
// ---------------------------------------------------------------------------
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

void dac_write(uint8_t ch, uint16_t mv) {
  if (ch >= NUM_OUTPUTS) return;
  if (mv > DAC_MAX_MV) mv = DAC_MAX_MV;
  uint16_t v12 = (uint16_t)(((uint32_t)mv * 4095UL) / DAC_MAX_MV);
  uint8_t chip = ch >> 1;
  uint8_t ab = ch & 1;
  uint16_t frame = (ab ? 0x8000 : 0) | 0x4000 | 0x1000 | v12;
  SPI.beginTransaction(SPISettings(12000000, MSBFIRST, SPI_MODE0));
  xfer(CS_PINS[chip], frame);
  SPI.endTransaction();
}
#endif

void dac_write_all(const uint16_t *vals) {
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) dac_write(i, vals[i]);
}
