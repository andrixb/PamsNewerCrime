// Native unit tests for Pam's Newer Crime.
//
// Builds the REAL production translation units (domain/euclid.cpp,
// domain/scales.cpp, app/clock.cpp, app/channel.cpp, app/serialization.cpp,
// app/patch.cpp, app/menu_model.cpp, infra/hal_storage.cpp) on the host
// against small Arduino/EEPROM/DAC mocks, and exercises their public
// behaviour. No production code is duplicated here: every assertion runs
// through the shipped engine.
//
//   make test
//
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>

#include "mock/EEPROM.h"

#include "app/channel.h"
#include "app/clock.h"
#include "domain/euclid.h"
#include "app/serialization.h"
#include "app/patch.h"
#include "app/menu_model.h"
#include "app/storage_port.h"   // raw slot write for the version-rejection test
#include "infra/dac.h"   // declarations only; definitions come from test/mock_hw.cpp

extern uint32_t mock_time_us;
extern uint16_t mock_dac_mv[];

static int g_checks = 0, g_failed = 0;
#define CHECK(cond)                                                        \
  do {                                                                     \
    g_checks++;                                                            \
    if (!(cond)) {                                                         \
      g_failed++;                                                          \
      printf("  FAIL %s:%d:  %s\n", __FILE__, __LINE__, #cond);            \
    }                                                                      \
  } while (0)

static void suite_ok(const char *name) { printf("[ok]   %s\n", name); }

// ---------------------------------------------------------------------------
// euclid
// ---------------------------------------------------------------------------
static int pat_sum(const uint8_t *p, int n) {
  int s = 0;
  for (int i = 0; i < n; i++) s += p[i];
  return s;
}

static void test_euclid() {
  uint8_t p[64];

  // Exact reference patterns (bucket Bjorklund).
  euclid(3, 8, p);
  const uint8_t e38[8] = { 1,0,0,1,0,0,1,0 };
  CHECK(memcmp(p, e38, 8) == 0);
  euclid(5, 8, p);
  const uint8_t e58[8] = { 1,0,1,1,0,1,1,0 };
  CHECK(memcmp(p, e58, 8) == 0);
  euclid(2, 5, p);
  const uint8_t e25[5] = { 1,0,1,0,0 };
  CHECK(memcmp(p, e25, 5) == 0);

  // Boundaries.
  euclid(0, 4, p);
  CHECK(pat_sum(p, 4) == 0);
  euclid(4, 4, p);
  CHECK(pat_sum(p, 4) == 4);
  euclid(9, 5, p);   // over-full clamps to all-ones
  CHECK(pat_sum(p, 5) == 5);

  // Property sweep: exactly `fill` pulses in `len` slots, and the pattern is
  // maximally even (max gap between pulses minus min gap <= 1).
  int bad = 0, uneven = 0;
  for (int len = 1; len <= 32; len++) {
    for (int fill = 0; fill <= len; fill++) {
      uint8_t q[32];
      euclid(fill, len, q);
      if (pat_sum(q, len) != fill) { bad++; continue; }
      if (fill >= 2) {
        int first = -1, prev = -1, mn = len, mx = 0;
        for (int i = 0; i < len; i++) if (q[i]) {
          if (prev >= 0) { int g = i - prev; if (g < mn) mn = g; if (g > mx) mx = g; }
          if (first < 0) first = i;
          prev = i;
        }
        int wrap = len - prev + first;
        if (wrap < mn) mn = wrap;
        if (wrap > mx) mx = wrap;
        if (mx - mn > 1) uneven++;
      }
    }
  }
  if (bad) printf("  %d (fill,len) pairs with wrong pulse count\n", bad);
  if (uneven) printf("  %d (fill,len) pairs not maximally even\n", uneven);
  CHECK(bad == 0);
  CHECK(uneven == 0);

  suite_ok("euclid");
}

// ---------------------------------------------------------------------------
// master clock (internal tempo, swing warp, external sync)
// ---------------------------------------------------------------------------
static void run_clock(int ticks, uint32_t *total_delta) {
  for (int i = 0; i < ticks; i++) {
    masterClock.tick();
    uint32_t d = masterClock.take_delta();
    if (total_delta) *total_delta += d;
  }
}

static void test_clock() {
  const uint32_t QT = (uint32_t)TICKS_PER_QUARTER * 256;  // 24576 units / quarter

  masterClock = MasterClock{};
  masterClock.bpm_cent = 12000;    // 120 BPM -> 2 quarters per second
  masterClock.source = 0;
  masterClock.running = true;
  masterClock.swing = 0;
  masterClock.reset_phase();

  uint32_t straight_first = 0;
  masterClock.tick();
  straight_first = masterClock.take_delta();   // per-tick unit advance
  CHECK(straight_first > 0);
  uint32_t straight_rest = 0;
  run_clock(499, &straight_rest);
  CHECK(straight_first + straight_rest == QT); // 500 ms = exactly one quarter @ 120

  // Swing must preserve the total phase per quarter (sync stays locked).
  masterClock.reset_phase();
  masterClock.swing = 63;          // hard swing
  uint32_t one_tick = 0;
  run_clock(1, &one_tick);
  CHECK(one_tick < straight_first); // first eighth advances SLOWER = eighths fall late
  uint32_t swung_q = 0;
  run_clock(499, &swung_q);
  CHECK(swung_q + one_tick == QT);  // quarter total unchanged by warp

  // Running stop gates the internal source.
  masterClock.running = false;
  uint32_t stopped = 12345;
  run_clock(10, &stopped);
  CHECK(stopped == 12345 + 0);
  masterClock.running = true;

  // External sync: one 24-PPQN pulse must advance exactly one 24th of a quarter.
  masterClock = MasterClock{};
  masterClock.source = 1;
  masterClock.ext_ppqn = 24;
  masterClock.reset_phase();
  mock_time_us = 1000000;
  masterClock.pulse_external();
  masterClock.tick();
  CHECK(masterClock.take_delta() == QT / 24);

  // ... and the measured tempo tracks 10 ms pulses (250.00 BPM at 24 PPQN).
  for (int i = 0; i < 60; i++) {
    mock_time_us += 10000;
    masterClock.pulse_external();
    masterClock.tick();
    masterClock.take_delta();
  }
  uint16_t bpm = masterClock.measured_bpm_cent();
  CHECK(bpm > 24000 && bpm < 26000);

  suite_ok("clock");
}

// ---------------------------------------------------------------------------
// external clock: period estimator, tempo read-out and timeout
// ---------------------------------------------------------------------------
static void test_clock_external() {
  const uint32_t QT = (uint32_t)TICKS_PER_QUARTER * 256;

  // 120 BPM at 24 PPQN -> 20833us pulse period.
  masterClock = MasterClock{};
  masterClock.source = 1;
  masterClock.ext_ppqn = 24;
  masterClock.reset_phase();
  mock_time_us = 1000000;
  const uint32_t period_120_24 = 60000000u / (120u * 24u);  // 20833us

  uint32_t total_delta = 0;
  for (int i = 0; i < 100; i++) {
    mock_time_us += period_120_24;
    masterClock.pulse_external();
    masterClock.tick();
    total_delta += masterClock.take_delta();
  }
  CHECK(total_delta == 100u * (QT / 24));
  uint16_t bpm = masterClock.measured_bpm_cent();
  CHECK(bpm >= 11950 && bpm <= 12050);

  // First pulse advances phase even before the tempo estimator has an interval.
  masterClock = MasterClock{};
  masterClock.source = 1;
  masterClock.ext_ppqn = 24;
  masterClock.reset_phase();
  mock_time_us = 1000000;
  masterClock.pulse_external();
  masterClock.tick();
  CHECK(masterClock.take_delta() == QT / 24);
  CHECK(masterClock.measured_bpm_cent() == 0);

  // 1 PPQN clock: each pulse is one quarter, derived tempo still 120 BPM.
  masterClock = MasterClock{};
  masterClock.source = 1;
  masterClock.ext_ppqn = 1;
  masterClock.reset_phase();
  mock_time_us = 0;
  for (int i = 0; i < 8; i++) {
    mock_time_us += 500000u;      // 500ms per quarter
    masterClock.pulse_external();
    masterClock.tick();
    CHECK(masterClock.take_delta() == QT);
  }
  CHECK(masterClock.measured_bpm_cent() >= 11900 && masterClock.measured_bpm_cent() <= 12100);

  // Sudden tempo change: rolling average locks to the new tempo in a few pulses.
  masterClock = MasterClock{};
  masterClock.source = 1;
  masterClock.ext_ppqn = 24;
  masterClock.reset_phase();
  mock_time_us = 0;
  for (int i = 0; i < 24; i++) {
    mock_time_us += period_120_24;
    masterClock.pulse_external();
    masterClock.tick();
    (void)masterClock.take_delta();
  }
  CHECK(masterClock.measured_bpm_cent() >= 11950 && masterClock.measured_bpm_cent() <= 12050);
  const uint32_t period_240_24 = 60000000u / (240u * 24u);  // 10417us
  for (int i = 0; i < 8; i++) {
    mock_time_us += period_240_24;
    masterClock.pulse_external();
    masterClock.tick();
    (void)masterClock.take_delta();
  }
  CHECK(masterClock.measured_bpm_cent() >= 23900 && masterClock.measured_bpm_cent() <= 24100);

  // Timeout: after >4x the average period without a pulse the estimator clears.
  masterClock = MasterClock{};
  masterClock.source = 1;
  masterClock.ext_ppqn = 24;
  masterClock.reset_phase();
  mock_time_us = 0;
  for (int i = 0; i < 20; i++) {
    mock_time_us += period_120_24;
    masterClock.pulse_external();
    masterClock.tick();
    (void)masterClock.take_delta();
  }
  CHECK(masterClock.measured_bpm_cent() > 0);
  mock_time_us += 100000u;        // > 4 * ~20833us
  masterClock.tick();
  CHECK(masterClock.measured_bpm_cent() == 0);

  suite_ok("clock: external sync");
}

// ---------------------------------------------------------------------------
// channel engine
// ---------------------------------------------------------------------------
static void drive(uint32_t ticks, uint32_t delta = 96, uint8_t cv1 = 128, uint8_t cv2 = 128) {
  // Mirrors the main.cpp composition: engine update (pure, no I/O), then the
  // infra flush that lands dirty outputs on the (mocked) DAC.
  for (uint32_t t = 0; t < ticks; t++) {
    channels_update(delta, cv1, cv2, {USER_SCALES});
    uint16_t vals[NUM_OUTPUTS];
    uint8_t dirty = channels_take_outputs(vals);
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++)
      if (dirty & (uint8_t)(1u << i)) dac_write(i, vals[i]);
  }
}

// Configure channel 0 from init defaults. delta=96 -> one 1/4 cycle per 256 ticks.
static void setup_pulse(Channel &c, uint8_t ratio, uint8_t width) {
  c.p.mode = MODE_PULSE; c.p.ratio = ratio; c.p.width = width; c.p.delay = 0;
  c.p.cv_min = 0; c.p.cv_max = 5000; c.p.prob = 100; c.p.eucl_en = 0; c.p.slew = 0;
}

static void test_channel_pulse() {
  channels_init(DEFAULT_RATIO);
  Channel &c = channels[0];
  setup_pulse(c, 16, 64);   // 1/4 note, 25% duty

  int on = 0, rising = 0;
  uint8_t prev = c.gate_state;
  uint32_t top_mv = 0;
  for (uint32_t t = 0; t < 640; t++) {
    drive(1);
    if (t >= 320) {                       // skip the initial latch window
      if (c.gate_state) on++;
      if (c.gate_state && !prev) rising++;
      if (c.out_mv > top_mv) top_mv = c.out_mv;
    }
    prev = c.gate_state;
  }
  CHECK(rising == 1);          // one gate per quarter
  CHECK(on == 64);             // 25% duty = 64 of 256 ticks
  CHECK(top_mv == 5000);       // gate-high hits cv_max
  CHECK(mock_dac_mv[0] == c.out_mv);   // every update lands on the DAC
  suite_ok("channel: pulse timing/duty");
}

static void test_channel_div() {
  channels_init(DEFAULT_RATIO);
  Channel &c = channels[0];
  c.p.mode = MODE_DIV; c.p.div_a = 4; c.p.div_b = 0; c.p.width = 64; c.p.slew = 0;
  c.p.cv_min = 0; c.p.cv_max = 5000;
  drive(3072);                          // 12 quarters
  CHECK(c.event_count == 3);            // quarter /4 -> one event per 4 quarters

  channels_init(DEFAULT_RATIO);
  c.p.mode = MODE_DIV; c.p.div_a = -4; c.p.div_b = 0; c.p.width = 64; c.p.slew = 0;
  drive(1024);                          // 4 quarters
  CHECK(c.event_count == 16);           // x4 -> 16 events

  // Stages compose: /3 then x4 -> net 4/3 events per quarter (period = 3/4 quarter).
  channels_init(DEFAULT_RATIO);
  c.p.mode = MODE_DIV; c.p.div_a = 3; c.p.div_b = -4; c.p.width = 64; c.p.slew = 0;
  c.event_count = 0;
  drive(3072);                          // 12 quarters -> 16 events
  CHECK(c.event_count == 16);

  // Extreme multiply clamps safely (x32 * x32 -> /1024 of a quarter).
  channels_init(DEFAULT_RATIO);
  c.p.mode = MODE_DIV; c.p.div_a = -32; c.p.div_b = -32; c.p.width = 64; c.p.slew = 0;
  drive(1024);
  CHECK(c.event_count == 1024u * 96u / 24u);   // total units / period (period clamps to 24)
  suite_ok("channel: clock div/mult");
}

static void test_channel_note() {
  channels_init(DEFAULT_RATIO);
  Channel &c = channels[0];
  c.p.mode = MODE_NOTE; c.p.ratio = 16; c.p.scale = 1;   // major
  c.p.cv_min = 0; c.p.cv_max = 2490; c.p.slew = 0;       // 30 semitones, 83 mV/semi

  static const bool in_major[12] = {1,0,1,0,1,1,0,1,0,1,0,1};
  bool seen[31] = { false };
  bool off_grid = false;
  for (uint32_t t = 0; t < 256; t++) {
    drive(1);
    int best = -1;
    for (int k = 0; k <= 30; k++)
      if (abs((int)c.out_mv - k * 83) <= 3) best = k;
    if (best < 0 || !in_major[best % 12]) off_grid = true;
    else seen[best] = true;
  }
  CHECK(!off_grid);                     // every sample sits on an in-scale grid note
  int n = 0;
  for (int k = 0; k <= 30; k++) if (seen[k]) n++;
  CHECK(n == 18);                       // all 18 major pitches over 2.5 octaves are hit
  suite_ok("channel: note quantiser");
}

static void test_channel_step_prob() {
  channels_init(DEFAULT_RATIO);
  Channel &c = channels[0];
  c.p.mode = MODE_STEP; c.p.ratio = 16;
  c.p.step_len = 2; c.p.steps[0] = 255; c.p.steps[1] = 0;
  c.p.step_prob[0] = 100; c.p.step_prob[1] = 0;   // step 2 never fires
  c.p.cv_min = 0; c.p.cv_max = 5000; c.p.slew = 0; c.p.eucl_en = 0; c.p.prob = 100;

  int rising = 0; uint8_t prev = 0;
  for (uint32_t t = 0; t < 1200; t++) {
    drive(1);
    if (t >= 200 && c.gate_state && !prev) rising++;
    prev = c.gate_state;
  }
  CHECK(rising == 2);          // of 4 events, only the step-0 events trigger
  CHECK(c.out_mv == 5000);     // held value is step 0's top
  suite_ok("channel: per-step probability");
}

static void test_channel_euclid() {
  channels_init(DEFAULT_RATIO);
  Channel &c = channels[0];
  setup_pulse(c, 16, 64);
  c.p.eucl_en = 1; c.p.eucl_len = 8; c.p.eucl_fill = 3; c.p.eucl_rot = 0;
  channel_rebuild_euclid(c);
  CHECK(c.mask == 0x49);       // 10010010 -> bits 0,3,6 (regression for the
                               // count bug where 3/8 emitted 5 pulses)

  int rising = 0; uint8_t prev = 0;
  for (uint32_t t = 0; t <= 2048; t++) {
    drive(1);
    if (t >= 200 && c.gate_state && !prev) rising++;
    prev = c.gate_state;
  }
  CHECK(rising == 3);          // 8 events -> 3 gates through the pattern
  suite_ok("channel: euclid gating");
}

static void test_channel_ratchet() {
  channels_init(DEFAULT_RATIO);
  Channel &c = channels[0];
  c.p.mode = MODE_RATCHET; c.p.ratio = 16; c.p.ratchets = 4; c.p.width = 64;
  c.p.delay = 0; c.p.cv_min = 0; c.p.cv_max = 5000; c.p.slew = 0;
  c.p.eucl_en = 0; c.p.prob = 100;

  // delta=96 -> one 1/4 cycle (period 24576) every 256 ticks; first event at
  // t=256. n=4 -> sub=64, so r256<64 means ph256%64 < 16 -> a 16-tick burst at
  // the top of each of the 4 sub-slots. Measure 3 settled cycles (t=256..1023).
  int rising = 0, on = 0; uint8_t prev = 0;
  for (uint32_t t = 0; t < 1024; t++) {
    drive(1);
    if (t >= 256) {
      if (c.gate_state) on++;
      if (c.gate_state && !prev) rising++;
    }
    prev = c.gate_state;
  }
  CHECK(rising == 12);         // 4 bursts/cycle x 3 cycles
  CHECK(on == 192);            // 16 ticks/burst x 12
  suite_ok("channel: ratchet");
}

static void test_channel_crossop() {
  channels_init(DEFAULT_RATIO);
  Channel &a = channels[0], &b = channels[1];
  setup_pulse(a, 16, 255);                    // essentially always high after 1st event
  a.p.cv_max = 10000;
  setup_pulse(b, 16, 64);
  b.p.xop = XOP_MASK; b.p.xsrc = 2;           // mask by out1

  int on_masked = 0;
  for (uint32_t t = 0; t < 1024; t++) {
    drive(1);
    if (t >= 256 && b.out_mv > 0) on_masked++;
  }
  CHECK(on_masked == 192);     // 3 gate windows of 64, unmasked (source high)

  channels_init(DEFAULT_RATIO);
  b.p.mode = MODE_PULSE; setup_pulse(b, 16, 64);
  b.p.xop = XOP_MASK; b.p.xsrc = 0;           // mask by unpatched CV1 (0V)
  int on_low = 0;
  drive(1024);
  for (uint32_t t = 0; t < 256; t++) {        // CV1 stays 0 throughout
    channels_update(96, 0, 0, {USER_SCALES});
    if (b.out_mv > 0) on_low++;
  }
  CHECK(on_low == 0);          // masked dead while source is low
  suite_ok("channel: cross-ops");
}

static void test_channel_quant_app() {
  channels_init(DEFAULT_RATIO);
  Channel &c = channels[1];
  c.p.mode = MODE_QUANT; c.p.quant_src = 0;   // CV1 jack
  c.p.scale = 1;                              // major
  c.p.cv_min = 0; c.p.cv_max = 2490; c.p.slew = 0;

  static const bool in_major[12] = {1,0,1,0,1,1,0,1,0,1,0,1};
  int prev_mv = -1;
  bool off_grid = false, nonmono = false;
  for (int s = 0; s <= 255; s++) {
    channels_update(0, (uint8_t)s, 0, {USER_SCALES});
    int best = -1;
    for (int k = 0; k <= 30; k++)
      if (abs((int)c.out_mv - k * 83) <= 3) best = k;
    if (best < 0 || !in_major[best % 12]) off_grid = true;
    if (prev_mv >= 0 && (int)c.out_mv < prev_mv) nonmono = true;
    prev_mv = c.out_mv;
  }
  CHECK(!off_grid);
  CHECK(!nonmono);
  suite_ok("channel: quantiser app");
}

static void test_channel_lfo() {
  channels_init(DEFAULT_RATIO);
  Channel &c = channels[0];
  c.p.mode = MODE_LFO; c.p.ratio = 16; c.p.wave = LFO_SQR; c.p.skew = 64;
  c.p.cv_min = 0; c.p.cv_max = 5000; c.p.slew = 0;

  int high = 0, above = 0, top = 0; (void)top;
  for (uint32_t t = 0; t < 256; t++) {
    drive(1);
    if (c.out_mv > 2500) high++;
    if (c.out_mv > 4990) top++;
    if (c.out_mv > 0) above++;
  }
  CHECK(high == 64);           // square duty = skew (ph < 64 of 256)
  CHECK(above == 64);

  // Trapezoid / parabola stay inside the window and peak.
  for (int w = LFO_TRAPZ; w <= LFO_PARAB; w++) {
    channels_init(DEFAULT_RATIO);
    c.p.mode = MODE_LFO; c.p.wave = w; c.p.skew = 128; c.p.cv_max = 5000;
    int lo = 999999, hi = -1;
    for (uint32_t t = 0; t < 256; t++) {
      drive(1);
      if ((int)c.out_mv < lo) lo = c.out_mv;
      if ((int)c.out_mv > hi) hi = c.out_mv;
    }
    CHECK(lo >= 0 && hi <= 5000);
    CHECK(hi > 4000 && lo < 100);   // full excursion within the window
  }
  suite_ok("channel: LFO waves");
}

// ---------------------------------------------------------------------------
// woggle (Make Noise Wogglebug-style stepped/smooth/burst engine)
// ---------------------------------------------------------------------------
static void test_channel_woggle() {
  channels_init(DEFAULT_RATIO);
  Channel &c = channels[0];
  c.p.mode = MODE_WGGLE; c.p.ratio = 16; c.p.slew = 0; c.p.prob = 100;
  c.p.eucl_en = 0; c.p.cv_min = 0; c.p.cv_max = 4000;
  c.p.skew = 0; c.p.smooth = 255; c.p.burst = 0; c.p.width = 255;

  // skew 0: the stepped target never re-rolls, so everything is frozen.
  drive(600);
  uint8_t frozen = c.wtar;
  drive(600);
  CHECK(c.wtar == frozen);
  CHECK(c.wv == frozen);

  // skew 255 + burst 255: new stepped value AND a gate on every event.
  // smooth 255 -> slow glide, so the smoothed value is caught mid-flight.
  c.p.skew = 255; c.p.burst = 255;
  int glide = 0, gated = 0;
  for (uint32_t t = 0; t < 600; t++) {
    drive(1);
    if (c.gate_state) gated++;
    if (c.wsm != c.wtar) glide++;
  }
  CHECK(c.wtar != frozen);
  CHECK(glide > 100);               // slow smooth lag between events
  CHECK(gated > 0);                 // burst gates fired

  // smooth 0 -> instant snap: smoothed value always equals the target.
  c.p.smooth = 0; c.p.burst = 0;
  int mid = 0, gated0 = 0;
  for (uint32_t t = 0; t < 600; t++) {
    drive(1);
    if (c.wsm != c.wtar) mid++;
    if (c.gate_state) gated0++;
  }
  CHECK(mid == 0);
  CHECK(gated0 == 0);               // burst 0: gate never opens

  // width slow (1 unit/tick): the woggle output chases the smooth value for
  // many ticks between events instead of arriving at once.
  c.p.width = 1;
  int chase = 0;
  for (uint32_t t = 0; t < 1600; t++) { drive(1); if (c.wv != c.wsm) chase++; }
  CHECK(chase > 250);
  CHECK(c.out_mv <= 4000);          // stays inside the cv window
  suite_ok("channel: woggle");
}

// ---------------------------------------------------------------------------
// storage
// ---------------------------------------------------------------------------
static void test_storage() {
  patch_init();

  masterClock.bpm_cent = 13456;
  masterClock.swing = 20;
  USER_SCALES[1] = 0x123;
  channels[0].p.mode = MODE_STEP;
  channels[0].p.ratio = 3;
  channels[0].p.steps[5] = 77;
  channels[0].p.div_a = -8;

  patch_save(SLOT_BANK1);

  masterClock.bpm_cent = 9999;
  masterClock.swing = 0;
  USER_SCALES[1] = 0;
  channels[0].p.mode = MODE_OFF;
  channels[0].p.ratio = 9;
  channels[0].p.steps[5] = 0;
  channels[0].p.div_a = 0;

  CHECK(patch_load(SLOT_BANK1));
  StoreBlob reloaded;
  capture_state(reloaded);
  CHECK(masterClock.bpm_cent == 13456);
  CHECK(masterClock.swing == 20);
  CHECK(USER_SCALES[1] == 0x123);
  CHECK(channels[0].p.mode == MODE_STEP);
  CHECK(channels[0].p.ratio == 3);
  CHECK(channels[0].p.steps[5] == 77);
  CHECK(channels[0].p.div_a == -8);

  // In-memory verification rejects tampering.
  CHECK(blob_verify(reloaded));
  reloaded.crc ^= 0xAA;
  CHECK(!blob_verify(reloaded));

  // Corrupted payload must be rejected by CRC. (SLOT_BANK1 lives at byte 512.)
  EEPROM.data[512 + 40] ^= 0xAA;
  CHECK(!patch_load(SLOT_BANK1));
  EEPROM.data[512 + 40] ^= 0xAA;   // restore
  CHECK(patch_load(SLOT_BANK1));

  // A future version must be rejected outright (written raw, bypassing capture).
  StoreBlob future;
  capture_state(future);
  future.version += 1;
  storage_port_write(store_slot_offset(SLOT_BANK2), &future, sizeof(future));
  CHECK(!patch_load(SLOT_BANK2));

  suite_ok("storage");
}

// ---------------------------------------------------------------------------
// Phase 0 safety net: representative engine modes the refactor must not change.
// (Appended after the pre-existing suites so their RNG sequence is untouched.)
// ---------------------------------------------------------------------------
static void test_channel_off() {
  channels_init(DEFAULT_RATIO);
  Channel &c = channels[0];
  c.p.mode = MODE_OFF;
  drive(300);
  CHECK(c.gate_state == 0);
  CHECK(c.out_mv == 0);
  CHECK(mock_dac_mv[0] == 0);
  suite_ok("channel: off");
}

static void test_channel_ramp() {
  channels_init(DEFAULT_RATIO);
  Channel &c = channels[0];
  c.p.mode = MODE_RAMP; c.p.ratio = 16; c.p.skew = 128;
  c.p.cv_min = 0; c.p.cv_max = 5000; c.p.slew = 0; c.p.prob = 100; c.p.eucl_en = 0;
  int on = 0, lo = 99999, hi = -1;
  for (uint32_t t = 0; t < 256; t++) {
    drive(1);
    if (c.gate_state) on++;
    if ((int)c.out_mv < lo) lo = c.out_mv;
    if ((int)c.out_mv > hi) hi = c.out_mv;
  }
  CHECK(on == 128);            // gate = rising half (ph < skew)
  CHECK(lo < 100);
  CHECK(hi > 4800 && hi <= 5000);
  suite_ok("channel: ramp");
}

static void test_channel_step_seq() {
  channels_init(DEFAULT_RATIO);
  Channel &c = channels[0];
  c.p.mode = MODE_STEP; c.p.ratio = 16; c.p.step_len = 4;
  c.p.steps[0] = 0; c.p.steps[1] = 85; c.p.steps[2] = 170; c.p.steps[3] = 255;
  c.p.cv_min = 0; c.p.cv_max = 5000; c.p.slew = 0; c.p.prob = 100; c.p.eucl_en = 0;
  const uint8_t expect[4] = { 85, 170, 255, 0 };
  for (int k = 0; k < 4; k++) {
    drive(256);                          // one 1/4 event per 256 ticks
    CHECK(c.sh_val == expect[k]);
  }
  CHECK(c.out_mv == 0);                  // last step holds 0
  suite_ok("channel: step sequence");
}

static void test_channel_sh() {
  channels_init(DEFAULT_RATIO);
  Channel &c = channels[0];
  c.p.mode = MODE_SH; c.p.ratio = 16;
  c.p.cv_min = 0; c.p.cv_max = 5000; c.p.slew = 0; c.p.prob = 100; c.p.eucl_en = 0;
  int gates = 0;
  for (uint32_t t = 0; t < 512; t++) {
    drive(1);
    if (c.gate_state) gates++;
    CHECK(c.out_mv <= 5000);
  }
  CHECK(gates == 2);                     // gate only on event ticks
  CHECK(mock_dac_mv[0] == c.out_mv);
  suite_ok("channel: S&H");
}

static void test_channel_env() {
  channels_init(DEFAULT_RATIO);
  Channel &c = channels[0];
  c.p.mode = MODE_ENV; c.p.ratio = 16; c.p.env_a = 2; c.p.env_d = 2;  // 64+64 ticks
  c.p.cv_min = 0; c.p.cv_max = 5000; c.p.slew = 0; c.p.prob = 100; c.p.eucl_en = 0;
  drive(256);                            // first event fires the envelope
  CHECK(c.event_count == 1);
  CHECK(c.gate_state == 1);
  CHECK(c.out_mv > 0 && c.out_mv < 5000);   // attack in progress
  int peak = 0, gated = 0;
  for (uint32_t t = 0; t < 200; t++) {
    drive(1);
    if (c.gate_state) gated++;
    if ((int)c.out_mv > peak) peak = c.out_mv;
  }
  CHECK(peak > 4000);                    // full excursion
  CHECK(gated == 127);                   // open for the rest of the 128-tick window
  CHECK(c.gate_state == 0 && c.out_mv == 0);
  suite_ok("channel: envelope");
}

static void test_channel_logic() {
  // AND against a silent partner never opens.
  channels_init(DEFAULT_RATIO);
  channels[0].p.mode = MODE_OFF;
  Channel &b = channels[1];
  b.p.mode = MODE_LOGIC; b.p.ratio = 16; b.p.width = 64; b.p.logic_src = 1;
  b.p.logic_op = LOGIC_AND; b.p.slew = 0; b.p.prob = 100; b.p.eucl_en = 0;
  b.p.cv_min = 0; b.p.cv_max = 5000;
  int on_and = 0;
  for (uint32_t t = 0; t < 512; t++) { drive(1); if (b.gate_state) on_and++; }
  CHECK(on_and == 0);
  // OR against a silent partner equals the own window (64 ticks/cycle).
  channels_init(DEFAULT_RATIO);
  channels[0].p.mode = MODE_OFF;
  Channel &o = channels[1];
  o.p.mode = MODE_LOGIC; o.p.ratio = 16; o.p.width = 64; o.p.logic_src = 1;
  o.p.logic_op = LOGIC_OR; o.p.slew = 0; o.p.prob = 100; o.p.eucl_en = 0;
  o.p.cv_min = 0; o.p.cv_max = 5000;
  int on_or = 0;
  for (uint32_t t = 0; t < 512; t++) { drive(1); if (o.gate_state) on_or++; }
  CHECK(on_or == 128);
  suite_ok("channel: logic");
}

static void test_channel_rwalk() {
  channels_init(DEFAULT_RATIO);
  Channel &c = channels[0];
  c.p.mode = MODE_RWALK; c.p.ratio = 16; c.p.width = 0;   // step of 1
  c.p.cv_min = 0; c.p.cv_max = 5000; c.p.slew = 0; c.p.prob = 100; c.p.eucl_en = 0;
  int gates = 0;
  bool moved = false;
  int16_t prev_wv = c.wv;
  for (uint32_t t = 0; t < 512; t++) {
    drive(1);
    if (c.gate_state) gates++;
    if (c.wv != prev_wv) moved = true;
    prev_wv = c.wv;
    CHECK(c.wv >= 0 && c.wv <= 255);
    CHECK(c.out_mv <= 5000);
  }
  CHECK(gates == 2);                     // gate only on event ticks
  CHECK(moved);                          // walk actually walks
  suite_ok("channel: random walk");
}

static void test_channel_slew() {
  channels_init(DEFAULT_RATIO);
  Channel &c = channels[0];
  c.p.mode = MODE_LFO; c.p.ratio = 16; c.p.wave = LFO_SQR; c.p.skew = 128;
  c.p.cv_min = 0; c.p.cv_max = 5000; c.p.slew = 0;
  drive(1);
  CHECK(c.out_mv == 5000);               // square opens high instantly
  channels_init(DEFAULT_RATIO);
  Channel &s = channels[0];
  s.p.mode = MODE_LFO; s.p.ratio = 16; s.p.wave = LFO_SQR; s.p.skew = 128;
  s.p.cv_min = 0; s.p.cv_max = 5000; s.p.slew = 16;
  drive(1);
  CHECK(s.out_mv == 312);                // one lag step: 5000*16/256
  suite_ok("channel: slew limiter");
}

static void test_channel_gquant() {
  channels_init(DEFAULT_RATIO);
  Channel &c = channels[0];
  c.p.mode = MODE_RAMP; c.p.ratio = 16; c.p.skew = 128;
  c.p.cv_min = 0; c.p.cv_max = 2490; c.p.slew = 0; c.p.prob = 100; c.p.eucl_en = 0;
  c.p.scale = 1; c.p.gquant = 1;         // major, post-filter on
  static const bool in_major[12] = {1,0,1,0,1,1,0,1,0,1,0,1};
  bool off_grid = false;
  for (uint32_t t = 0; t < 256; t++) {
    drive(1);
    int semi = c.out_mv / 83;
    if (c.out_mv % 83 != 0) off_grid = true;
    else if (semi < 0 || semi > 30 || !in_major[semi % 12]) off_grid = true;
  }
  CHECK(!off_grid);
  suite_ok("channel: global quantise");
}

static void test_channel_cvmod() {
  auto run = [](uint8_t cv) {
    channels_init(DEFAULT_RATIO);
    Channel &c = channels[0];
    c.p.mode = MODE_PULSE; c.p.ratio = 16; c.p.width = 64;
    c.p.cv_min = 0; c.p.cv_max = 5000; c.p.slew = 0; c.p.prob = 100; c.p.eucl_en = 0;
    c.p.cv_src1 = CVP_WIDTH; c.p.cv_scale1 = 4; c.p.cv_in1 = 0;  // CV1 -> width
    int on = 0;
    for (uint32_t t = 0; t < 512; t++) {
      channels_update(96, cv, 0, {USER_SCALES});
      if (t >= 256 && c.gate_state) on++;
    }
    return on;
  };
  CHECK(run(0) == 64);                   // unmodulated 25% duty
  CHECK(run(255) == 95);                 // full-scale CV widens the gate
  suite_ok("channel: CV modulation");
}

static void test_channel_flex() {
  // Straight-grid baseline: 4 events, 64 ticks/cycle over 1024 ticks.
  channels_init(DEFAULT_RATIO);
  Channel &c = channels[0];
  c.p.mode = MODE_PULSE; c.p.ratio = 16; c.p.width = 64;
  c.p.cv_min = 0; c.p.cv_max = 5000; c.p.slew = 0; c.p.prob = 100; c.p.eucl_en = 0;
  c.p.flex = FLEX_NONE; c.p.flex_amt = 0;
  int base_on = 0;
  for (uint32_t t = 0; t < 1024; t++) { drive(1); if (c.gate_state) base_on++; }
  CHECK(c.event_count == 4);
  CHECK(base_on == 256);
  // Every flex op keeps the engine bounded and gating (no crash, no stuck rail).
  for (uint8_t f = FLEX_HUMAN; f < FLEX_LAST; f++) {
    channels_init(DEFAULT_RATIO);
    Channel &x = channels[0];
    x.p.mode = MODE_PULSE; x.p.ratio = 16; x.p.width = 64;
    x.p.cv_min = 0; x.p.cv_max = 5000; x.p.slew = 0; x.p.prob = 100; x.p.eucl_en = 0;
    x.p.flex = f; x.p.flex_amt = 128;
    int on = 0, rises = 0;
    uint8_t prev = 0;
    uint16_t peak = 0;
    for (uint32_t t = 0; t < 1024; t++) {
      drive(1);
      if (x.gate_state) on++;
      if (x.gate_state && !prev) rises++;
      if (x.out_mv > peak) peak = x.out_mv;
      prev = x.gate_state;
    }
    CHECK(x.event_count >= 3 && x.event_count <= 8);
    CHECK(rises >= 2);
    CHECK(on > 0 && on <= 1024);
    CHECK(peak <= 5000);
  }
  // The rate warp provably bends time (accel stretches the open window).
  channels_init(DEFAULT_RATIO);
  Channel &a = channels[0];
  a.p.mode = MODE_PULSE; a.p.ratio = 16; a.p.width = 64;
  a.p.cv_min = 0; a.p.cv_max = 5000; a.p.slew = 0; a.p.prob = 100; a.p.eucl_en = 0;
  a.p.flex = FLEX_ACCEL; a.p.flex_amt = 128;
  int acc_on = 0;
  for (uint32_t t = 0; t < 1024; t++) { drive(1); if (a.gate_state) acc_on++; }
  CHECK(acc_on != base_on);
  suite_ok("channel: flex micro-timing");
}

static void test_engine_no_io() {
  // Architectural pin: the engine computes but never touches the DAC.
  // Outputs are only emitted through channels_take_outputs() + dac_write().
  channels_init(DEFAULT_RATIO);
  Channel &c = channels[0];
  c.p.mode = MODE_PULSE; c.p.ratio = 16; c.p.width = 64;
  c.p.cv_min = 0; c.p.cv_max = 5000; c.p.slew = 0; c.p.prob = 100; c.p.eucl_en = 0;
  mock_dac_mv[0] = 0xFFFF;             // sentinel: out of the 0..10000 range
  channels_update(96, 128, 128, {USER_SCALES});
  CHECK(c.out_mv == 5000);             // computed...
  CHECK(mock_dac_mv[0] == 0xFFFF);     // ...but not emitted
  uint16_t vals[NUM_OUTPUTS];
  CHECK(channels_take_outputs(vals) & 1);
  CHECK(vals[0] == 5000);
  CHECK(channels_take_outputs(vals) == 0);   // drain clears the flags
  // The mirrored flush (as in drive()/main.cpp) lands it on the DAC.
  channels_update(96, 128, 128, {USER_SCALES});
  uint8_t mask = channels_take_outputs(vals);
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++)
    if (mask & (uint8_t)(1u << i)) dac_write(i, vals[i]);
  CHECK(mock_dac_mv[0] == c.out_mv);
  suite_ok("engine: no I/O without flush");
}

// ---------------------------------------------------------------------------
// menu model (app/menu_model.cpp): group tables + get/set/label/format.
// Moved out of the UI in Phase 4; pinned here so the split cannot silently
// change what the panel browses or edits.
// ---------------------------------------------------------------------------
static void test_menu_groups() {
  // Every mode browses a non-empty list, except OFF which has no shape params.
  for (uint8_t m = 0; m < MODE_LAST; m++) {
    uint8_t n = chan_param_count(m, 8);
    if (m == MODE_OFF) CHECK(n == 0);
    else CHECK(n > 0 && n < CH_PARAM_COUNT);
  }
  // Flat positions resolve inside the id space with a named group.
  for (uint8_t m = 1; m < MODE_LAST; m++) {
    uint8_t n = chan_param_count(m, 8);
    for (uint8_t pos = 0; pos < n; pos++) {
      CHECK(app_param_at(m, pos, 8) < CH_PARAM_COUNT);
      const char *g = app_param_group(m, pos, 8);
      CHECK(g != nullptr && g[0] != '\0');
    }
  }
  // STEP mode expands the sequence into the shape group.
  CHECK(chan_param_count(MODE_STEP, 1) == 27);
  CHECK(chan_param_count(MODE_STEP, 8) == 41);
  CHECK(app_param_at(MODE_STEP, 0, 8) == P_SLEN);
  CHECK(app_param_at(MODE_STEP, 1, 8) == P_STEP0);
  CHECK(app_param_at(MODE_STEP, 2, 8) == P_SPROB0);
  CHECK(app_param_at(MODE_STEP, 3, 8) == P_STEP0 + 1);
  CHECK(strcmp(app_param_group(MODE_PULSE, 0, 8), "pulse") == 0);  // shape = mode name
  CHECK(strcmp(app_param_group(MODE_PULSE, 2, 8), "clock") == 0);  // width,delay then clock
  suite_ok("menu: groups");
}

static void test_menu_get_set() {
  channels_init(DEFAULT_RATIO);
  // Clamps pin the edit contract.
  menu_set_val(0, P_WIDTH, 300, MENU_CHANNEL);
  CHECK(menu_get_val(0, P_WIDTH, MENU_CHANNEL) == 255);
  menu_set_val(0, P_WIDTH, -5, MENU_CHANNEL);
  CHECK(menu_get_val(0, P_WIDTH, MENU_CHANNEL) == 1);
  menu_set_val(0, P_MODE, MODE_LAST + 5, MENU_CHANNEL);
  CHECK(menu_get_val(0, P_MODE, MENU_CHANNEL) == MODE_LAST - 1);
  menu_set_val(0, P_RATIO, 99, MENU_CHANNEL);
  CHECK(menu_get_val(0, P_RATIO, MENU_CHANNEL) == RATIO_COUNT - 1);
  // Euclid edits rebuild the pattern.
  menu_set_val(0, P_EULEN, 4, MENU_CHANNEL);
  CHECK(channels[0].mask == 0xF);   // 4/4 -> all four slots
  menu_set_val(0, P_EUFILL, 1, MENU_CHANNEL);
  CHECK(channels[0].mask == 0x1);   // 1/4 Bjorklund -> slot 0
  // Master page clamps.
  menu_set_val(0, 0, 5, MENU_MASTER);
  CHECK(menu_get_val(0, 0, MENU_MASTER) == BPM_MIN);
  menu_set_val(0, 0, 99999, MENU_MASTER);
  CHECK(menu_get_val(0, 0, MENU_MASTER) == BPM_MAX);   // no int16_t wrap at high BPM
  menu_set_val(0, 0, (int32_t)BPM_MAX + 100, MENU_MASTER);  // encoder step past max clamps
  CHECK(menu_get_val(0, 0, MENU_MASTER) == BPM_MAX);
  char bpmbuf[20];
  menu_fmt_val(0, 0, MENU_MASTER, bpmbuf, sizeof(bpmbuf));
  CHECK(strcmp(bpmbuf, "330.00") == 0);
  menu_set_val(0, 0, 20000, MENU_MASTER);
  CHECK(menu_get_val(0, 0, MENU_MASTER) == 20000);
  menu_set_val(0, 3, 99, MENU_MASTER);
  CHECK(menu_get_val(0, 3, MENU_MASTER) == 63);
  // Per-step values + probabilities.
  menu_set_val(0, P_STEP0 + 2, 200, MENU_CHANNEL);
  CHECK(menu_get_val(0, P_STEP0 + 2, MENU_CHANNEL) == 200);
  menu_set_val(0, P_SPROB0 + 2, 150, MENU_CHANNEL);
  CHECK(menu_get_val(0, P_SPROB0 + 2, MENU_CHANNEL) == 100);
  suite_ok("menu: get/set");
}

static void test_menu_labels() {
  char buf[20];
  channels_init(DEFAULT_RATIO);
  channels[0].p.mode = MODE_LFO;
  menu_fmt_val(0, P_MODE, MENU_CHANNEL, buf, sizeof(buf));
  CHECK(strcmp(buf, "LFO") == 0);
  menu_fmt_val(0, P_RATIO, MENU_CHANNEL, buf, sizeof(buf));
  CHECK(strcmp(buf, RATIO_TABLE[DEFAULT_RATIO].label) == 0);
  CHECK(strcmp(menu_label_of(0, P_RATIO, MENU_CHANNEL), "ratio") == 0);
  CHECK(strcmp(menu_label_of(0, P_SPROB0 + 3, MENU_CHANNEL), "s4 prob") == 0);
  masterClock.bpm_cent = 12000;
  menu_fmt_val(0, 0, MENU_MASTER, buf, sizeof(buf));
  CHECK(strcmp(buf, "120.00") == 0);
  CHECK(strcmp(menu_label_of(0, 0, MENU_MASTER), "BPM") == 0);
  menu_fmt_val(0, P_DIVA, MENU_CHANNEL, buf, sizeof(buf));
  CHECK(strcmp(buf, "1:1") == 0);   // default div_a = 0
  channels[0].p.div_a = 4;
  menu_fmt_val(0, P_DIVA, MENU_CHANNEL, buf, sizeof(buf));
  CHECK(strcmp(buf, "/4") == 0);
  suite_ok("menu: labels");
}

static void test_clock_tempo() {  const uint32_t QT = (uint32_t)TICKS_PER_QUARTER * 256;
  masterClock = MasterClock{};
  masterClock.bpm_cent = 6000;           // 60 BPM -> quarter == 1000 ticks
  masterClock.source = 0; masterClock.running = true; masterClock.swing = 0;
  masterClock.reset_phase();
  uint32_t tot = 0;
  run_clock(1000, &tot);
  CHECK(tot == QT);
  // BPM clamps pin the constrain() contract the engine relies on.
  masterClock = MasterClock{}; masterClock.bpm_cent = 0;   // clamps to BPM_MIN
  masterClock.source = 0; masterClock.running = true; masterClock.swing = 0;
  masterClock.reset_phase();
  tot = 0;
  run_clock(6000, &tot);
  CHECK(tot == QT);
  masterClock = MasterClock{}; masterClock.bpm_cent = 60000;  // clamps to BPM_MAX
  masterClock.source = 0; masterClock.running = true; masterClock.swing = 0;
  masterClock.reset_phase();
  tot = 0;
  int n = 0;
  while (tot < QT && n < 1000) { masterClock.tick(); tot += masterClock.take_delta(); n++; }
  CHECK(n == 182);
  // take_delta drains the accumulator.
  masterClock.tick();
  (void)masterClock.take_delta();
  CHECK(masterClock.take_delta() == 0);
  suite_ok("clock: tempo/clamp");
}

static void test_blob_golden() {
  // Byte-identical default patch: any layout/format change must break this.
  masterClock = MasterClock{};
  DEFAULT_RATIO = 6;
  USER_SCALES[0] = 0xFFF; USER_SCALES[1] = 0xAB5; USER_SCALES[2] = 0x5AD;
  channels_init(DEFAULT_RATIO);
  StoreBlob b;
  capture_state(b);
  CHECK(b.magic == 0x504B5731u);
  CHECK(b.version == 10u);
  CHECK(sizeof(StoreBlob) == 332u);
  CHECK(sizeof(StoreBlob) <= 512u);      // EEPROM slot stride
  CHECK(RATIO_COUNT == 24);
  CHECK(SCALE_COUNT == 32);
  CHECK(b.master.bpm_cent == 12000);
  CHECK(b.master.source == 0 && b.master.ext_ppqn == 24 && b.master.running == 1);
  CHECK(b.master.swing == 0 && b.master.default_ratio == 6);
  CHECK(b.master.user_scales[0] == 0xFFF && b.master.user_scales[1] == 0xAB5 &&
        b.master.user_scales[2] == 0x5AD);
  CHECK(b.ch[0].mode == MODE_PULSE && b.ch[0].ratio == 6 && b.ch[0].width == 64);
  CHECK(b.ch[0].cv_min == 0 && b.ch[0].cv_max == 3000 && b.ch[0].prob == 100);
  CHECK(b.crc == 0xeee026d8u);
  StoreBlob b2;
  capture_state(b2);
  CHECK(memcmp(&b, &b2, sizeof(b)) == 0);
  suite_ok("storage: blob golden");
}

static void test_storage_roundtrip() {
  patch_init();
  // Known pattern across every persisted field.
  masterClock.bpm_cent = 14100; masterClock.source = 1; masterClock.ext_ppqn = 12;
  masterClock.running = false; masterClock.swing = 33;
  DEFAULT_RATIO = 10;
  USER_SCALES[0] = 0x111; USER_SCALES[1] = 0x222; USER_SCALES[2] = 0x333;
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
    channels[i].p.mode = (uint8_t)((i + 1) % MODE_LAST);
    channels[i].p.ratio = i;
    channels[i].p.width = (uint8_t)(10 + i);
    channels[i].p.steps[i % MAX_STEPS] = (uint8_t)(50 + i);
    channels[i].p.div_a = (int8_t)((int)i - 2);
  }
  StoreBlob b1;
  capture_state(b1);
  patch_save(SLOT_ACTIVE);
  // Wipe everything, then restore.
  masterClock.bpm_cent = 1; masterClock.source = 0; masterClock.ext_ppqn = 24;
  masterClock.running = true; masterClock.swing = 0;
  DEFAULT_RATIO = 6;
  USER_SCALES[0] = 0; USER_SCALES[1] = 0; USER_SCALES[2] = 0;
  channels_init(DEFAULT_RATIO);
  CHECK(patch_load(SLOT_ACTIVE));
  StoreBlob b2;
  capture_state(b2);
  CHECK(memcmp(&b1, &b2, sizeof(StoreBlob)) == 0);
  CHECK(masterClock.bpm_cent == 14100 && masterClock.source == 1);
  CHECK(masterClock.ext_ppqn == 12 && !masterClock.running && masterClock.swing == 33);
  CHECK(DEFAULT_RATIO == 10);
  CHECK(USER_SCALES[0] == 0x111 && USER_SCALES[2] == 0x333);
  CHECK(channels[2].p.width == 12 && channels[2].p.steps[2 % MAX_STEPS] == 52);
  // Dirty flag pins the autosave contract used by main.cpp.
  mark_dirty();
  CHECK(state_dirty());
  // restore_state defends against dirty on-device values.
  StoreBlob odd = b1;
  odd.master.swing = 100; odd.master.default_ratio = 255;
  uint8_t keep_ratio = DEFAULT_RATIO;
  restore_state(odd);
  CHECK(masterClock.swing == 63);
  CHECK(DEFAULT_RATIO == keep_ratio);
  suite_ok("storage: round-trip");
}

// ---------------------------------------------------------------------------
int main() {
  test_euclid();
  test_clock();
  test_clock_external();
  test_channel_pulse();
  test_channel_div();
  test_channel_note();
  test_channel_step_prob();
  test_channel_euclid();
  test_channel_ratchet();
  test_channel_crossop();
  test_channel_quant_app();
  test_channel_lfo();
  test_channel_woggle();
  test_storage();
  test_channel_off();
  test_channel_ramp();
  test_channel_step_seq();
  test_channel_sh();
  test_channel_env();
  test_channel_logic();
  test_channel_rwalk();
  test_channel_slew();
  test_channel_gquant();
  test_channel_cvmod();
  test_channel_flex();
  test_engine_no_io();
  test_menu_groups();
  test_menu_get_set();
  test_menu_labels();
  test_clock_tempo();
  test_blob_golden();
  test_storage_roundtrip();
  printf("%d checks, %d failures\n", g_checks, g_failed);
  return g_failed ? 1 : 0;
}
