#pragma once
#include <Arduino.h>
#include "pam_config.h"

class MasterClock {
public:
  uint16_t bpm_cent = 12000;
  uint8_t source = 0;
  uint8_t ext_ppqn = 24;
  bool running = true;

  void begin();
  void tick();
  void pulse_external();
  void reset_phase();
  uint32_t take_delta();
  uint16_t measured_bpm_cent() const { return ext_bpm_cent; }
  bool external_active() const { return source == 1; }

private:
  uint32_t delta_x256 = 0;
  uint64_t frac_acc = 0;
  volatile uint32_t ext_pulses = 0;
  uint32_t consumed_pulses = 0;
  uint64_t ext_last_us = 0;
  float ext_avg_us = 0;
  uint16_t ext_bpm_cent = 0;
};

extern MasterClock masterClock;
