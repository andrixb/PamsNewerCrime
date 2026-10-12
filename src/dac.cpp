#include "dac.h"
#include <SPI.h>
#include <Arduino.h>

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
  // Matches Phazerville OC_DAC.cpp for the DAC8565/8563 in O_C hardware:
  // RESET is active-low on this part, so normal operation holds RST HIGH.
  // (Only the pin-strapped DAC8564 variant wants RST LOW.)
  digitalWrite(PIN_DAC_RST, HIGH);
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

// ---- hardware bring-up self-test -----------------------------------------
// Runs BEFORE the 1 kHz control timer is armed, so there is no ISR/UI bus
// contention. Sweeps each output 0 -> 5 V -> 10 V -> 0 and prints the exact
// control byte + 16-bit code so you can confirm with a meter AND against the
// O_C reference (CHA=0x10, CHB=0x12, CHC=0x14, CHD=0x16; code = 0xFFFF - mv).
void dac_selftest() {
  Serial.println(F("pnc: DAC self-test (OUT1 sweeps 0/5/10V forever; patch OUT1->CV1 or use a meter)"));
  digitalWrite(PIN_DAC_RST, HIGH);   // 8565: RST high = normal operation
  delay(2);
  // Hold the engine off (this runs with ctrl_timer stopped), and keep OUT1 on a
  // slow 0 -> 5V -> 10V loop forever so the ADC readback / a meter always has a
  // steady value to sample regardless of when the serial port attaches.
  const uint16_t steps[] = { 0, 5000, 10000 };
  while (true) {
    for (uint8_t s = 0; s < 3; s++) {
      uint16_t mv = steps[s];
      uint16_t code = 0xFFFF - (uint16_t)(((uint32_t)mv * 65535UL) / DAC_MAX_MV);
      for (uint8_t ch = 0; ch < NUM_OUTPUTS; ch++) dac_write(ch, ch == 0 ? mv : 0);
      delay(200);
      Serial.printf("out1=%5umV code=0x%04X  ADC1=%4u ADC2=%4u\n",
                    mv, code, analogRead(PIN_CV_IN1), analogRead(PIN_CV_IN2));
      delay(3000);
    }
  }
}
