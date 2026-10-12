#pragma once
#include <Arduino.h>
#include "pam_config.h"

void dac_init();
void dac_write(uint8_t ch, uint16_t millivolts);
void dac_write_all(const uint16_t *vals_mv);

// Blocking hardware bring-up test: before the clock/UI/ISR start, sweep each
// output 0 -> full -> 0 and log the exact SPI bytes. Enables/disables SPI
// transactions on the shared OLED bus to expose Teensy 4.x bus-contention or
// init faults. Enable with -DPNC_DAC_SELFTEST=1 (see main.cpp).
void dac_selftest();
