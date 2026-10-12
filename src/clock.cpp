#include "clock.h"

MasterClock masterClock;

// Quarter-note length in the internal x256 phase unit (TICKS_PER_QUARTER * 256).
static const uint32_t QT = (uint32_t)TICKS_PER_QUARTER * 256;

// Map an un-warped phase rb in [0, QT] to swung output phase. The first eighth
// occupies rawA of the raw quarter (rawA = QT/2 * (1 + 2*swing/128)), the second
// eighth the remainder; both halves emit QT/2 of output, so a full quarter still
// emits exactly QT -> quarter boundaries stay locked to the nominal tempo.
static uint32_t warp_raw(uint32_t rb, uint32_t rawA, uint32_t half) {
  if (rb <= rawA) return (uint32_t)((uint64_t)rb * half / rawA);
  uint32_t seg2 = QT - rawA;
  return half + (uint32_t)((uint64_t)(rb - rawA) * half / seg2);
}

void MasterClock::begin() {
  pinMode(PIN_CK_IN, INPUT);
}

void MasterClock::pulse_external() {
  ext_pulses++;
}

void MasterClock::reset_phase() {
  frac_acc = 0;
  raw_beat = 0;
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
    // units/ms = 24576 (per quarter) * bpm_cent / (60000 ms/min * 100 cent)
    frac_acc += (uint64_t)TICKS_PER_QUARTER * 256 * bpm;
    uint64_t denom = 60ULL * 1000ULL * (CONTROL_RATE_HZ / 1000) * 100;
    uint32_t whole = (uint32_t)(frac_acc / denom);
    if (!whole) return;
    frac_acc -= (uint64_t)whole * denom;

    if (!swing) {
      delta_x256 += whole;
      raw_beat += whole;
      if (raw_beat >= QT) raw_beat -= QT;
      return;
    }

    // Distribute `whole` raw units through the swing warp, never crossing a
    // segment/grid boundary within a single delta step.
    uint32_t rawA = QT / 2 + (uint32_t)((uint64_t)QT * swing / 128);
    if (rawA >= QT) rawA = QT - 1;
    uint32_t half = QT / 2;
    uint32_t add = whole;
    while (add) {
      uint32_t bound = (raw_beat < rawA) ? rawA : QT;
      uint32_t seg_left = bound - raw_beat;
      uint32_t step = add < seg_left ? add : seg_left;
      uint32_t o0 = warp_raw(raw_beat, rawA, half);
      uint32_t rb2 = raw_beat + step;
      uint32_t o1 = warp_raw(rb2 < QT ? rb2 : QT, rawA, half);
      delta_x256 += (o1 - o0);
      raw_beat = rb2;
      if (raw_beat >= QT) raw_beat -= QT;
      add -= step;
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
