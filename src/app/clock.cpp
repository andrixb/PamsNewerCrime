#include "app/clock.h"

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

void MasterClock::pulse_external() {
  uint32_t now = clock_now_us();
  if (ext_restart) {
    // Clock stopped and timed out: drop the stale history so the long gap
    // before this edge is not averaged into the new tempo.
    ext_restart = false;
    ext_last_pulse_us = 0;
    for (uint8_t i = 0; i < EXT_DT_HIST; i++) ext_dt_hist[i] = 0;
    ext_dt_head = 0;
    ext_dt_count = 0;
    ext_dt_sum = 0;
  }
  uint32_t prev = ext_last_pulse_us;
  // Reject sub-100us glitches / bouncy edges BEFORE they count: a glitch must
  // neither advance the clock nor become the reference for the next interval.
  if (prev && now - prev < 100) return;
  ext_last_pulse_us = now;
  ext_pulses++;

  if (!prev) return;                       // first pulse after a reset: no interval yet
  uint32_t dt = now - prev;

  // Rolling average of the last EXT_DT_HIST intervals. This follows tempo
  // changes in just a few pulses while still smoothing jittery clocks.
  if (ext_dt_count < EXT_DT_HIST) {
    ext_dt_hist[ext_dt_head] = dt;
    ext_dt_sum += dt;
    ext_dt_count++;
  } else {
    uint32_t old_dt = ext_dt_hist[ext_dt_head];
    ext_dt_sum = ext_dt_sum + dt - old_dt;
    ext_dt_hist[ext_dt_head] = dt;
  }
  ext_dt_head = (ext_dt_head + 1) % EXT_DT_HIST;
  ext_avg_us = (float)ext_dt_sum / (float)ext_dt_count;
}

void MasterClock::reset_phase() {
  frac_acc = 0;
  raw_beat = 0;
  consumed_pulses = ext_pulses;
  ext_last_pulse_us = 0;
  for (uint8_t i = 0; i < EXT_DT_HIST; i++) ext_dt_hist[i] = 0;
  ext_dt_head = 0;
  ext_dt_count = 0;
  ext_dt_sum = 0;
  ext_avg_us = 0;
  ext_bpm_cent = 0;
  ext_restart = false;
  ext_ahead = 0;
  ext_interp_acc = 0;
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
    // External sync. Phase is SNAPPED to every input pulse and INTERPOLATED
    // between pulses from the measured period, so LFOs/ramps move smoothly and
    // multiplications fire between pulses instead of all at once on each edge.
    // Interpolation never passes the next expected pulse: a clock that slows
    // or stops holds just before the beat; one that speeds up is caught up by
    // the snap. The total phase per pulse is exactly one pulse's worth.
    uint8_t ppqn = constrain(ext_ppqn, 1, 48);
    uint32_t t256 = QT / ppqn;
    uint32_t pulses = ext_pulses;
    uint32_t newp = pulses - consumed_pulses;
    if (newp) {
      consumed_pulses = pulses;
      uint32_t frac = QT - t256 * ppqn;
      // ext_ahead was emitted against the pulse size in force at the time. If
      // ext ppqn was raised since, it can exceed the new (smaller) t256; the
      // unsigned subtraction then wrapped into a ~4e9 phase jump. Credit at
      // most what this pulse contributes.
      const uint32_t owed = newp * t256;
      delta_x256 += owed > ext_ahead ? owed - ext_ahead : 0;
      ext_ahead = 0;
      ext_interp_acc = 0;
      frac_acc += (uint64_t)newp * frac;
      uint32_t extra = (uint32_t)(frac_acc / ppqn);
      if (extra) { frac_acc -= (uint64_t)extra * ppqn; delta_x256 += extra; }

      if (ext_avg_us > 200.0f) {
        float beats = ext_avg_us / 1e6f * ppqn;
        // The read-out is not clamped to the internal BPM_MIN..BPM_MAX range:
        // it shows the real tempo implied by ext ppqn. Clamping at BPM_MAX made
        // every too-low ppqn read the same "330.00", so turning ext ppqn up
        // from 1 looked like it did nothing. Floor 1.00 (0 means "no tempo").
        const float cent = 60.0f * 100.0f / beats;
        ext_bpm_cent = cent >= (float)EXT_BPM_CENT_MAX ? EXT_BPM_CENT_MAX
                     : cent < 100.0f ? 100u : (uint32_t)cent;
      }
      return;   // the pulse tick lands exactly on the pulse position
    }

    const float avg = ext_avg_us;
    if (avg <= 0.0f) return;                   // no tempo yet: pulses only
    uint32_t gap = clock_now_us() - ext_last_pulse_us;
    if (gap > (uint32_t)(avg * 4.0f)) {        // clock stopped
      ext_avg_us = 0.0f;
      ext_bpm_cent = 0;
      ext_restart = true;
      return;
    }
    const uint32_t avg_us = (uint32_t)avg;
    if (!avg_us) return;
    ext_interp_acc += (uint64_t)t256 * (1000000u / CONTROL_RATE_HZ);
    uint32_t step = (uint32_t)(ext_interp_acc / avg_us);
    ext_interp_acc -= (uint64_t)step * avg_us;
    // ext_ahead can exceed t256 after ext ppqn is raised mid-pulse; hold
    // (room 0) until the next pulse instead of wrapping to a huge room.
    const uint32_t room = ext_ahead + 1 < t256 ? t256 - 1 - ext_ahead : 0;
    if (step > room) { step = room; ext_interp_acc = 0; }
    ext_ahead += step;
    delta_x256 += step;
  }
}
