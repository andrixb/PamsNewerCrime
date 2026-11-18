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
  // Restart the internal quarter (swing warp) at its first eighth, keeping
  // tempo and external-sync state. Call alongside channels_reset() so the
  // channels' phase 0 lines up with the start of the swung quarter.
  void restart_beat() { raw_beat = 0; out_beat = 0; beat_blend = false; }
  uint32_t take_delta();
  // Measured external tempo in 1/100 BPM, 0 = no tempo yet. Not limited to
  // the internal BPM range (a ppqn set too low reads as a very high tempo).
  uint32_t measured_bpm_cent() const { return ext_bpm_cent; }
  static constexpr uint32_t EXT_BPM_CENT_MAX = 9999999;   // 99999.99 BPM
  uint32_t measured_period_us() const { return (uint32_t)ext_avg_us; }
  bool external_active() const { return source == 1; }

private:
  uint32_t delta_x256 = 0;
  uint64_t frac_acc = 0;
  uint32_t raw_beat = 0;   // un-warped phase within the current quarter (x256)
  uint32_t out_beat = 0;   // warped phase already emitted in the current quarter
  uint8_t warp_swing = 0;  // swing amount out_beat was emitted under
  // Swing edited mid-quarter: finish that quarter on a straight line from
  // (blend_r0, blend_o0) to (QT, QT) instead of jumping onto the new warp.
  bool beat_blend = false;
  uint32_t blend_r0 = 0, blend_o0 = 0;
  volatile uint32_t ext_pulses = 0;
  uint32_t consumed_pulses = 0;
  volatile uint32_t ext_last_pulse_us = 0;  // time of the most recent external pulse
  static constexpr uint8_t EXT_DT_HIST = 4;
  volatile uint32_t ext_dt_hist[EXT_DT_HIST] = {0};
  volatile uint8_t ext_dt_head = 0;
  volatile uint8_t ext_dt_count = 0;
  volatile uint32_t ext_dt_sum = 0;
  volatile float ext_avg_us = 0;            // rolling average of the last EXT_DT_HIST pulse intervals
  // Set by tick() when the clock times out; the pulse ISR (sole writer of the
  // interval history) then starts measuring afresh on the next edge.
  volatile bool ext_restart = false;
  // Interpolation between pulses (tick() context only).
  uint32_t ext_ahead = 0;        // units emitted since the last pulse (< one pulse)
  uint64_t ext_interp_acc = 0;   // fractional unit accumulator (units * us)
  uint32_t ext_bpm_cent = 0;
};

extern MasterClock masterClock;

// Time port for the external-sync path (timeout + tempo measurement).
// Declared here, provided by the composition root / test harness:
// firmware returns micros(), native tests return their mock clock.
uint32_t clock_now_us();
