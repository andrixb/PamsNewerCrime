#include "clock.h"

MasterClock clock;

void MasterClock::begin() {
  pinMode(PIN_CK_IN, INPUT);
}

void MasterClock::pulse_external() {
  ext_pulses++;
}

void MasterClock::reset_phase() {
  frac_acc = 0;
  consumed_pulses = ext_pulses;
  ext_avg_us = 0;
}

uint32_t MasterClock::take_delta() {
  uint32_t d = delta_x256;
  delta_x256 = 0;
  return d;
}

void MasterClock::tick() {
  if (source == 0) {
    if (!running) return;
    uint32_t bpm = constrain(bpm_cent, BPM_MIN, BPM_MAX);
    frac_acc += (uint64_t)TICKS_PER_QUARTER * 256 * bpm * 10;
    uint64_t denom = 60ULL * 1000ULL * (CONTROL_RATE_HZ / 1000) * 100;
    uint32_t whole = (uint32_t)(frac_acc / denom);
    if (whole) {
      frac_acc -= whole * denom;
      delta_x256 += whole;
    }
  } else {
    uint32_t pulses = ext_pulses;
    uint32_t newp = pulses - consumed_pulses;
    if (!newp) {
      if (ext_avg_us > 0) {
        uint32_t now = micros();
        uint64_t gap = (uint32_t)(now - ext_last_us);
        if (gap > (uint64_t)ext_avg_us * 4) { ext_avg_us = 0; ext_bpm_cent = 0; }
      }
      return;
    }
    consumed_pulses = pulses;
    uint32_t now = micros();
    if (ext_avg_us > 0) {
      uint32_t per = now - ext_last_us;
      if (per < (uint32_t)ext_avg_us * 4) ext_avg_us = ext_avg_us * 0.9f + per * 0.1f;
      else ext_avg_us = per;
    } else {
      ext_avg_us = 20000;
    }
    ext_last_us = now;

    uint8_t ppqn = constrain(ext_ppqn, 1, 48);
    uint32_t t256 = (uint32_t)TICKS_PER_QUARTER * 256 / ppqn;
    uint32_t frac = (uint32_t)TICKS_PER_QUARTER * 256 - t256 * ppqn;
    delta_x256 += newp * t256;
    frac_acc += (uint64_t)newp * frac;
    uint32_t extra = (uint32_t)(frac_acc / ppqn);
    if (extra) { frac_acc -= (uint64_t)extra * ppqn; delta_x256 += extra; }

    if (ext_avg_us > 200) {
      float beats = (float)ext_avg_us / 1e6f * ppqn;
      ext_bpm_cent = (uint16_t)constrain(60.0f * 100.0f / beats, 1, 60000);
    }
  }
}
