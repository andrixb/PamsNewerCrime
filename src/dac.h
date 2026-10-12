#pragma once
#include <Arduino.h>
#include "pam_config.h"

void dac_init();
void dac_write(uint8_t ch, uint16_t millivolts);
void dac_write_all(const uint16_t *vals_mv);
