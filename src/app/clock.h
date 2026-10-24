#pragma once
#include <stdint.h>
#include "pam_config.h"

class MasterClock {
public:
  uint16_t bpm_cent = 12000;
  uint8_t source = 0;
  uint8_t ext_ppqn = 24;
  bool running = true;
  // Global swing: 0 = straight, up to 63 ≈ full hard-swing. Delays every odd
  // eighth by stretching the first half of each quarter; quarter boundaries stay
  // sample-accurate to the nominal tempo (internal source only).
  uint8_t swing = 0;

  void tick();
  void pulse_external();
  void reset_phase();
  uint32_t take_delta();
  uint16_t measured_bpm_cent() const { return ext_bpm_cent; }
  uint32_t measured_period_us() const { return (uint32_t)ext_avg_us; }
  bool external_active() const { return source == 1; }

private:
  uint32_t delta_x256 = 0;
  uint64_t frac_acc = 0;
  uint32_t raw_beat = 0;   // un-warped phase within the current quarter (x256)
  volatile uint32_t ext_pulses = 0;
  uint32_t consumed_pulses = 0;
  volatile uint32_t ext_last_pulse_us = 0;  // time of the most recent external pulse
  static constexpr uint8_t EXT_DT_HIST = 4;
  volatile uint32_t ext_dt_hist[EXT_DT_HIST] = {0};
  volatile uint8_t ext_dt_head = 0;
  volatile uint8_t ext_dt_count = 0;
  volatile uint32_t ext_dt_sum = 0;
  volatile float ext_avg_us = 0;            // rolling average of the last EXT_DT_HIST pulse intervals
  uint16_t ext_bpm_cent = 0;
};

extern MasterClock masterClock;

// Time port for the external-sync path (timeout + tempo measurement).
// Declared here, provided by the composition root / test harness:
// firmware returns micros(), native tests return their mock clock.
uint32_t clock_now_us();
