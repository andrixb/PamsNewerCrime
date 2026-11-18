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
#include "infra/ui.h"                    // real controller + renderer (test/mock_ui.cpp fakes the panel)
#include "infra/hal_input.h"
#include "infra/display/ui_nav.h"
#include "infra/display/oled.h"
#include <sys/wait.h>
#include <unistd.h>

extern uint32_t mock_time_us;
extern uint16_t mock_dac_mv[];
extern void (*mock_now_hook)();
extern uint64_t mock_pins_low;

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

  // ext ppqn set too low for the source: the read-out shows the real implied
  // tempo instead of saturating at BPM_MAX, so every ppqn step is visible
  // (24 PPQN @ 120 source read at ppqn 4 -> 720 BPM, at 8 -> 360 BPM).
  for (uint8_t pp = 4; pp <= 8; pp += 4) {
    masterClock = MasterClock{};
    masterClock.source = 1;
    masterClock.ext_ppqn = pp;
    masterClock.reset_phase();
    mock_time_us = 0;
    for (int i = 0; i < 20; i++) {
      mock_time_us += period_120_24;
      masterClock.pulse_external();
      masterClock.tick();
      (void)masterClock.take_delta();
    }
    const uint32_t want = 12000u * 24u / pp;
    CHECK(masterClock.measured_bpm_cent() > BPM_MAX);
    CHECK(masterClock.measured_bpm_cent() >= want - want / 100 &&
          masterClock.measured_bpm_cent() <= want + want / 100);
  }

  // Raising ext ppqn mid-pulse (4 -> 24): interpolation already emitted more
  // than one new, smaller pulse; the next pulse must not wrap delta (used to
  // jump ~4e9 units) and interpolation must hold instead of running away.
  masterClock = MasterClock{};
  masterClock.source = 1;
  masterClock.ext_ppqn = 4;
  masterClock.reset_phase();
  mock_time_us = 0;
  const uint32_t period_120_4 = 60000000u / (120u * 4u);   // 125ms
  for (int i = 0; i < 8; i++) {
    mock_time_us += period_120_4;
    masterClock.pulse_external();
    masterClock.tick();
    (void)masterClock.take_delta();
  }
  for (int t = 0; t < 100; t++) {   // ~80% through the pulse
    mock_time_us += 1000;
    masterClock.tick();
  }
  (void)masterClock.take_delta();
  masterClock.ext_ppqn = 24;        // t256 shrinks 6144 -> 1024
  uint32_t worst = 0;
  for (int t = 0; t < 20; t++) {
    mock_time_us += 1000;
    masterClock.tick();
    uint32_t d = masterClock.take_delta();
    if (d > worst) worst = d;
  }
  mock_time_us += 5000;
  masterClock.pulse_external();
  masterClock.tick();
  uint32_t d = masterClock.take_delta();
  if (d > worst) worst = d;
  CHECK(worst <= QT / 4);

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

  // Extreme divide (/32 x /32) caps at UINT32_MAX/256 so the gate math does
  // not overflow: 50% width with a 50% delay must give a 50% duty gate.
  channels_init(DEFAULT_RATIO);
  c.p.mode = MODE_DIV; c.p.div_a = 32; c.p.div_b = 32; c.p.width = 128;
  c.p.delay = 128; c.p.slew = 0;
  const uint32_t cap = 0xFFFFFFFFu / 256;
  const uint32_t step = cap / 1000;     // 1000 steps stay inside one cycle
  drive(1, cap - 10);                   // just before the first event
  drive(2, 10);                         // fire it
  int on = 0;
  for (int t = 0; t < 1000; t++) { drive(1, step); if (c.gate_state) on++; }
  CHECK(c.event_count == 1);
  CHECK(on >= 495 && on <= 505);
  suite_ok("channel: clock div/mult");
}

// 1 V/oct grid: semitone k sits at round(k * 1000 / 12) mV; -1 if off-grid.
static int grid_semi(uint16_t mv) {
  int k = (mv * 12 + 500) / 1000;
  return ((k * 1000 + 6) / 12 == mv) ? k : -1;
}

static void test_channel_note() {
  channels_init(DEFAULT_RATIO);
  Channel &c = channels[0];
  c.p.mode = MODE_NOTE; c.p.ratio = 16; c.p.scale = 1;   // major
  c.p.cv_min = 0; c.p.cv_max = 2490; c.p.slew = 0;       // semitones 0..29 fit

  static const bool in_major[12] = {1,0,1,0,1,1,0,1,0,1,0,1};
  bool seen[31] = { false };
  bool off_grid = false;
  for (uint32_t t = 0; t < 256; t++) {
    drive(1);
    int best = grid_semi(c.out_mv);       // exact 1 V/oct grid note, or -1
    if (best < 0 || !in_major[best % 12]) off_grid = true;
    else seen[best] = true;
  }
  CHECK(!off_grid);                     // every sample sits on an in-scale grid note
  int n = 0;
  for (int k = 0; k <= 30; k++) if (seen[k]) n++;
  CHECK(n == 18);                       // all 18 major pitches over 2.5 octaves are hit

  // Full 10 V chromatic window: the ramp walks all 121 grid notes, top = 10 V.
  channels_init(DEFAULT_RATIO);
  c.p.mode = MODE_NOTE; c.p.ratio = 16; c.p.scale = 0;
  c.p.cv_min = 0; c.p.cv_max = 10000; c.p.slew = 0;
  bool hit[121] = { false };
  bool on_grid = true;
  uint16_t top = 0;
  for (uint32_t t = 0; t < 24576 / 4; t++) {
    drive(1, 4);
    int k = grid_semi(c.out_mv);
    if (k < 0 || k > 120) on_grid = false; else hit[k] = true;
    if (c.out_mv > top) top = c.out_mv;
  }
  int all = 0;
  for (int k = 0; k <= 120; k++) if (hit[k]) all++;
  CHECK(on_grid);
  CHECK(all == 121);
  CHECK(top == 10000);
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
    int best = grid_semi(c.out_mv);       // exact 1 V/oct grid note, or -1
    if (best < 0 || !in_major[best % 12]) off_grid = true;
    if (prev_mv >= 0 && (int)c.out_mv < prev_mv) nonmono = true;
    prev_mv = c.out_mv;
  }
  CHECK(!off_grid);
  CHECK(!nonmono);

  // 12-bit input over a full 10 V chromatic window: the CV for semitone k
  // (k/120 of full scale) must land exactly on note k, for all 121 notes.
  // 8 bits (~39 mV/step) cannot place every semitone centre.
  c.p.cv_max = 10000; c.p.scale = 0;          // chromatic
  bool exact = true;
  for (int k = 0; k <= 120; k++) {
    EngineEnv env = { USER_SCALES };
    env.cv1_12 = (uint16_t)((k * 4095 + 60) / 120);
    env.cv_hires = 1;
    channels_update(0, 0, 0, env);
    if (grid_semi(c.out_mv) != k) exact = false;
  }
  CHECK(exact);
  CHECK(c.out_mv == 10000);
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

  // Sine: full 0..cv_max excursion, peak mid-cycle, no flat dead zone (the old
  // table peaked at ~70% and sat at 0 for the last eighth of every cycle).
  channels_init(DEFAULT_RATIO);
  c.p.mode = MODE_LFO; c.p.ratio = 16; c.p.wave = LFO_SINE;
  c.p.cv_min = 0; c.p.cv_max = 10000; c.p.slew = 0;
  channels_reset();
  uint16_t sv[256];
  for (uint32_t t = 0; t < 256; t++) { drive(1); sv[t] = c.out_mv; }
  uint16_t smin = 65535, smax = 0;
  int zeros = 0, peak_at = 0;
  for (int t = 0; t < 256; t++) {
    if (sv[t] < smin) smin = sv[t];
    if (sv[t] > smax) { smax = sv[t]; peak_at = t; }
    if (sv[t] == 0) zeros++;
  }
  CHECK(smin == 0 && smax == 10000);
  CHECK(zeros <= 2);                     // touches zero, does not dwell there
  CHECK(peak_at >= 126 && peak_at <= 130);
  bool sym = true;                       // mirror-symmetric about the peak
  for (int k = 1; k < 120; k++)
    if (abs((int)sv[peak_at - k] - (int)sv[peak_at + k]) > 60) sym = false;
  CHECK(sym);

  // Slow LFO resolution: a 4-bar triangle at 120 BPM (~8 s) used to move in
  // at most 256 steps per cycle; 16-bit phase gives thousands of levels.
  channels_init(DEFAULT_RATIO);
  c.p.mode = MODE_LFO; c.p.ratio = 23; c.p.wave = LFO_TRI; c.p.skew = 128;
  c.p.cv_min = 0; c.p.cv_max = 10000; c.p.slew = 0;
  channels_reset();
  int levels = 0;
  uint16_t lastv = 0xFFFF;
  for (uint32_t t = 0; t < 393216 / 49; t++) {
    drive(1, 49);                        // ~120 BPM in x256 units per ms
    if (c.out_mv != lastv) { levels++; lastv = c.out_mv; }
  }
  CHECK(levels > 2000);
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
  CHECK(s.out_mv == 39);                 // one lag step, tau = 16*8 ms: 5000/128 = 39.06

  // Slow slew must actually reach its target, rising and falling (whole-mV
  // state used to stall up to 256/slew mV short on the way up).
  static const uint8_t slews[] = { 1, 8, 64 };
  for (uint8_t slew : slews) {
    channels_init(DEFAULT_RATIO);
    Channel &k = channels[0];
    k.p.mode = MODE_STEP; k.p.cv_min = 0; k.p.cv_max = 10000; k.p.slew = slew;
    for (int i = 0; i < MAX_STEPS; i++) k.p.steps[i] = 255;
    k.sh_val = 255;                      // delta 0: hold the step value
    drive(20000, 0);
    CHECK(k.out_mv >= 9999);
    for (int i = 0; i < MAX_STEPS; i++) k.p.steps[i] = 0;
    k.sh_val = 0;
    drive(20000, 0);
    CHECK(k.out_mv <= 1);
  }
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
    int semi = grid_semi(c.out_mv);
    if (semi < 0 || c.out_mv > 2490 || !in_major[semi % 12]) off_grid = true;
  }
  CHECK(!off_grid);

  // Accuracy at the top of a full 10 V window: 10 V is exactly 120 semitones.
  channels_init(DEFAULT_RATIO);
  c.p.mode = MODE_STEP; c.p.cv_min = 0; c.p.cv_max = 10000; c.p.slew = 0;
  c.p.scale = 0; c.p.gquant = 1;         // chromatic
  for (int i = 0; i < MAX_STEPS; i++) c.p.steps[i] = 255;
  c.sh_val = 255;                        // delta 0: hold the step value
  drive(2, 0);
  CHECK(c.out_mv == 10000);
  // 5 V (60 semitones) lands exactly on 5000 mV.
  for (int i = 0; i < MAX_STEPS; i++) c.p.steps[i] = 128;   // ~5020 mV in
  c.sh_val = 128;
  drive(1, 0);
  CHECK(c.out_mv == 5000);
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
    // Offset ops must never move the event grid: a re-rolled slop used to push
    // the just-crossed boundary backwards and recount the same event.
    if (f == FLEX_HUMAN || f == FLEX_SWING) CHECK(x.event_count == 4);
  }
  // swing provably delays alternate events: the gate starts at two distinct
  // points in the cycle instead of always on the grid.
  channels_init(DEFAULT_RATIO);
  Channel &s = channels[0];
  s.p.mode = MODE_PULSE; s.p.ratio = 16; s.p.width = 64;
  s.p.cv_min = 0; s.p.cv_max = 5000; s.p.slew = 0; s.p.prob = 100; s.p.eucl_en = 0;
  s.p.flex = FLEX_SWING; s.p.flex_amt = 128;
  bool saw_straight = false, saw_late = false;
  uint8_t sprev = 0;
  for (uint32_t t = 0; t < 1024; t++) {
    drive(1);
    if (s.gate_state && !sprev) {
      uint32_t at = t % 256;
      if (at < 8) saw_straight = true;
      else if (at > 40 && at < 100) saw_late = true;
    }
    sprev = s.gate_state;
  }
  CHECK(s.event_count == 4);
  CHECK(saw_straight && saw_late);
  // Euclid + human: the pattern must still advance on the real event grid
  // (3 of every 8), not at ~2x from recounting.
  channels_init(DEFAULT_RATIO);
  Channel &e = channels[0];
  e.p.mode = MODE_PULSE; e.p.ratio = 16; e.p.width = 64;
  e.p.cv_min = 0; e.p.cv_max = 5000; e.p.slew = 0; e.p.prob = 100;
  e.p.eucl_en = 1; e.p.eucl_len = 8; e.p.eucl_fill = 3; e.p.eucl_rot = 0;
  e.p.flex = FLEX_HUMAN; e.p.flex_amt = 128;
  channel_rebuild_euclid(e);
  int e_rises = 0;
  uint8_t eprev = 0;
  for (uint32_t t = 0; t < 8192; t++) {
    drive(1);
    if (e.gate_state && !eprev) e_rises++;
    eprev = e.gate_state;
  }
  CHECK(e.event_count == 32);       // 32 real grid events, not 64
  CHECK(e_rises >= 11 && e_rises <= 15);  // ~3/8 of them fire
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
  // Ramp behaves as the continuous LFO shape and must expose the mod chains.
  uint8_t rn = chan_param_count(MODE_RAMP, 8);
  CHECK(strcmp(app_param_group(MODE_RAMP, rn - 1, 8), "mod") == 0);
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

  // Out-of-range channel/master fields are clamped on load, and the engine
  // then runs on them without indexing past its tables.
  StoreBlob bad = b1;
  bad.master.bpm_cent = 60000; bad.master.source = 7; bad.master.ext_ppqn = 0;
  ChannelParams &bp = bad.ch[1];
  bp.ratio = 200; bp.mode = 99; bp.wave = 42; bp.scale = 250;
  bp.cv_min = 9000; bp.cv_max = 20000; bp.prob = 180;
  bp.eucl_len = 0; bp.eucl_fill = 40; bp.eucl_rot = 60; bp.step_len = 0;
  bp.cv_src1 = 77; bp.cv_in1 = 30; bp.logic_op = 9; bp.logic_src = 9;
  bp.div_a = 100; bp.div_b = -100; bp.quant_src = 30; bp.ratchets = 40;
  bp.xop = 50; bp.xsrc = 30; bp.flex = 20; bp.step_prob[3] = 255;
  restore_state(bad);
  const ChannelParams &rp = channels[1].p;
  CHECK(masterClock.bpm_cent == BPM_MAX && masterClock.source == 1);
  CHECK(masterClock.ext_ppqn == 1);
  CHECK(rp.ratio == RATIO_COUNT - 1 && rp.mode == MODE_OFF && rp.wave == LFO_SINE);
  CHECK(rp.scale == 0 && rp.prob == 100);
  CHECK(rp.cv_min == 9000 && rp.cv_max == DAC_MAX_MV);
  CHECK(rp.eucl_len == 1 && rp.eucl_fill == 1 && rp.eucl_rot == 0 && rp.step_len == 1);
  CHECK(rp.cv_src1 == CVP_NONE && rp.cv_in1 == 0);
  CHECK(rp.logic_op == LOGIC_OFF && rp.logic_src == 0);
  CHECK(rp.div_a == 32 && rp.div_b == -32 && rp.quant_src == 0 && rp.ratchets == 16);
  CHECK(rp.xop == XOP_NONE && rp.xsrc == 0 && rp.flex == FLEX_NONE);
  CHECK(rp.step_prob[3] == 100);
  for (int m = 0; m < MODE_LAST; m++) {         // every mode runs on the result
    channels[1].p.mode = (uint8_t)m;
    drive(50);
  }
  CHECK(channels[1].out_mv <= DAC_MAX_MV);
  masterClock = MasterClock{};
  suite_ok("storage: round-trip");
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// regression suites (review batch 1)
// ---------------------------------------------------------------------------

// Autosave writes SLOT_ACTIVE once per burst of edits, ~3 s after the last
// one, and never again while nothing changes.
static void test_patch_autosave() {
  const uint32_t off = store_slot_offset(SLOT_ACTIVE);
  uint32_t t = 10000000u;                      // ms
  mark_dirty();
  patch_autosave_tick(t);                      // sees the edit: starts the quiet timer
  CHECK(state_dirty());
  patch_autosave_tick(t += 2000);              // still within the 3 s pause
  CHECK(state_dirty());
  patch_autosave_tick(t += 1100);              // quiet for 3.1 s -> save
  CHECK(!state_dirty());
  StoreBlob saved;
  storage_port_read(off, &saved, sizeof(saved));
  CHECK(blob_verify(saved));

  // Clean state: later ticks must not touch storage again.
  EEPROM.data[off] ^= 0xA5;                    // sentinel a rewrite would undo
  const uint8_t sentinel = EEPROM.data[off];
  for (int i = 0; i < 10; i++) patch_autosave_tick(t += 1000);
  CHECK(EEPROM.data[off] == sentinel);

  // Continuous editing (an edit every 100 ms for 10 s) defers the save...
  for (int i = 0; i < 100; i++) { mark_dirty(); patch_autosave_tick(t += 100); }
  CHECK(state_dirty());
  CHECK(EEPROM.data[off] == sentinel);
  // ...to one save once the knob stops.
  patch_autosave_tick(t += 3100);
  CHECK(!state_dirty());
  CHECK(EEPROM.data[off] != sentinel);
  suite_ok("patch: autosave debounce");
}

// Patch use cases copy engine state only inside a balanced engine lock.
static void test_patch_engine_lock() {
  patch_save(SLOT_BANK1);
  int before = mock_irq_mask_count;
  CHECK(patch_load(SLOT_BANK1));
  CHECK(mock_irq_mask_count == before + 1);
  CHECK(mock_irq_masked == 0);
  before = mock_irq_mask_count;
  patch_save(SLOT_BANK2);
  CHECK(mock_irq_mask_count == before + 1 && mock_irq_masked == 0);
  before = mock_irq_mask_count;
  patch_defaults();
  CHECK(mock_irq_mask_count == before + 1 && mock_irq_masked == 0);
  // An invalid slot is rejected before taking the lock.
  EEPROM.data[store_slot_offset(SLOT_BANK2)] ^= 0xFF;
  before = mock_irq_mask_count;
  CHECK(!patch_load(SLOT_BANK2));
  CHECK(mock_irq_mask_count == before);
  suite_ok("patch: engine lock");
}

static void ext_clock_start(uint8_t ppqn) {
  masterClock = MasterClock{};
  masterClock.source = 1;
  masterClock.ext_ppqn = ppqn;
  masterClock.reset_phase();
  mock_time_us = 1000000;
}

// Glitch edges are rejected before they count; a timeout restarts measurement.
static void test_clock_external_robust() {
  const uint32_t QT = (uint32_t)TICKS_PER_QUARTER * 256;

  // A bounce 50us after a real edge must not advance the clock.
  ext_clock_start(1);
  masterClock.pulse_external();
  mock_time_us += 50;
  masterClock.pulse_external();                // glitch
  masterClock.tick();
  CHECK(masterClock.take_delta() == QT);       // one quarter, not two

  // ...nor become the reference for the next interval.
  ext_clock_start(1);
  for (int i = 0; i < 4; i++) { masterClock.pulse_external(); mock_time_us += 500000u; }
  masterClock.pulse_external();
  mock_time_us += 60;
  masterClock.pulse_external();                // glitch
  mock_time_us += 500000u - 60;
  masterClock.pulse_external();
  masterClock.tick();
  CHECK(masterClock.measured_period_us() == 500000u);

  // Stop the clock long enough to time out, then resume at a new tempo: the
  // gap must not be averaged in, so the first interval already reads true.
  ext_clock_start(24);
  const uint32_t p120 = 60000000u / (120u * 24u);
  for (int i = 0; i < 20; i++) {
    mock_time_us += p120;
    masterClock.pulse_external();
    masterClock.tick();
  }
  mock_time_us += 2000000u;                    // 2 s silence
  masterClock.tick();
  CHECK(masterClock.measured_bpm_cent() == 0);
  const uint32_t p90 = 60000000u / (90u * 24u);
  masterClock.pulse_external();                // restart edge
  mock_time_us += p90;
  masterClock.pulse_external();
  masterClock.tick();
  CHECK(masterClock.measured_bpm_cent() >= 8950 && masterClock.measured_bpm_cent() <= 9050);
  suite_ok("clock: external glitch/restart");
}

// PULSE gates under external clock never latch high once the clock stops.
static void test_channel_ext_gate_stop() {
  ext_clock_start(1);
  channels_init(16);
  Channel &c = channels[0];
  setup_pulse(c, 16, 128);                     // 1/4, 50% duty
  channels_reset();
  auto run_ms = [&](int ms) {
    for (int i = 0; i < ms; i++) {
      mock_time_us += 1000;
      masterClock.tick();
      const EngineEnv env = { USER_SCALES, masterClock.measured_bpm_cent(), 1 };
      channels_update(masterClock.take_delta(), 0, 0, env);
    }
  };
  int highs = 0;
  for (int b = 0; b < 8; b++) {                // 120 BPM, 1 PPQN
    masterClock.pulse_external();
    for (int i = 0; i < 500; i++) { run_ms(1); if (b == 7 && c.gate_state) highs++; }
  }
  CHECK(highs >= 245 && highs <= 255);         // ~50% of a 500 ms beat
  masterClock.pulse_external();
  run_ms(5);
  CHECK(c.gate_state && c.out_mv == 5000);     // last pulse still gates
  run_ms(5000);                                // clock stopped, estimator timed out
  CHECK(masterClock.measured_bpm_cent() == 0);
  CHECK(!c.gate_state && c.out_mv == 0);

  // No tempo yet: a pulse opens the fixed fallback trigger, then closes.
  ext_clock_start(1);
  channels_reset();
  masterClock.pulse_external();
  run_ms(1);
  CHECK(c.gate_state);
  run_ms(EXT_GATE_FALLBACK_MS + 1);
  CHECK(!c.gate_state && c.out_mv == 0);
  masterClock = MasterClock{};
  suite_ok("channel: external gate stops");
}

// External clock phase is interpolated between pulses and snapped on each.
static void test_clock_external_interp() {
  const uint32_t QT = (uint32_t)TICKS_PER_QUARTER * 256;
  auto tick_ms = [](int ms, uint32_t *sum) {
    for (int i = 0; i < ms; i++) {
      mock_time_us += 1000;
      masterClock.tick();
      uint32_t d = masterClock.take_delta();
      if (sum) *sum += d;
    }
  };

  // Steady 120 BPM, 1 PPQN: phase moves between pulses, and the running total
  // at every pulse tick is exactly n quarters.
  ext_clock_start(1);
  uint32_t total = 0;
  bool locked = true, moved_between = true;
  for (int b = 0; b < 8; b++) {
    masterClock.pulse_external();
    masterClock.tick();
    total += masterClock.take_delta();
    if (total != (uint32_t)(b + 1) * QT) locked = false;
    uint32_t before = total;
    tick_ms(499, &total);
    mock_time_us += 1000;                      // the pulse tick's own millisecond
    // From the 2nd pulse a tempo is known: ~a full quarter is interpolated.
    if (b >= 1 && total - before < QT * 9 / 10) moved_between = false;
  }
  CHECK(locked);
  CHECK(moved_between);                        // ~a full quarter interpolated

  // A late pulse: interpolation holds just short of the next pulse position.
  ext_clock_start(1);
  for (int b = 0; b < 4; b++) { masterClock.pulse_external(); masterClock.tick();
                                masterClock.take_delta(); tick_ms(499, nullptr);
                                mock_time_us += 1000; }
  masterClock.pulse_external(); masterClock.tick(); masterClock.take_delta();
  uint32_t ahead = 0;
  tick_ms(900, &ahead);                        // next pulse is 400 ms late
  CHECK(ahead == QT - 1);                      // parked right before the beat
  masterClock.pulse_external(); masterClock.tick();
  CHECK(masterClock.take_delta() == 1);        // snap completes the quarter

  // Engine: an LFO under 1 PPQN now moves continuously, and a 1/16 pulse
  // channel gives four gates per beat.
  ext_clock_start(1);
  channels_init(16);
  Channel &lfo = channels[0];
  lfo.p.mode = MODE_LFO; lfo.p.wave = LFO_TRI; lfo.p.ratio = 16;
  lfo.p.cv_min = 0; lfo.p.cv_max = 5000; lfo.p.slew = 0;
  Channel &pul = channels[1];
  setup_pulse(pul, 10, 128);                   // 1/16, 50% duty
  channels_reset();
  int distinct = 0, rises = 0;
  uint16_t last = 0xFFFF;
  uint8_t prev_gate = 0;
  for (int b = 0; b < 6; b++) {
    masterClock.pulse_external();
    for (int i = 0; i < 500; i++) {
      mock_time_us += 1000;
      masterClock.tick();
      const EngineEnv env = { USER_SCALES, masterClock.measured_bpm_cent(), 1 };
      channels_update(masterClock.take_delta(), 0, 0, env);
      if (b == 5) {
        if (lfo.out_mv != last) { distinct++; last = lfo.out_mv; }
        if (pul.gate_state && !prev_gate) rises++;
      }
      prev_gate = pul.gate_state;
    }
  }
  CHECK(distinct > 200);
  CHECK(rises == 4);
  masterClock = MasterClock{};
  suite_ok("clock: external interpolation");
}

// ---------------------------------------------------------------------------
// Regression tests for the 2026-11-19 code review. Each suite pins the
// CORRECT behaviour, so it fails until the matching fix lands.
// ---------------------------------------------------------------------------

// Bug 1: rwalk steps only on firing events and holds in between (wiki
// Modes.md: "on each firing event it steps up or down ... and holds").
static void test_bug_rwalk_holds() {
  channels_init(DEFAULT_RATIO);
  Channel &c = channels[0];
  c.p.mode = MODE_RWALK; c.p.ratio = 16; c.p.width = 255;   // big steps: drift is obvious
  c.p.cv_min = 0; c.p.cv_max = 5000; c.p.slew = 0; c.p.prob = 100; c.p.eucl_en = 0;

  // Clock stopped (delta 0): no events, so the value must not move at all.
  const int16_t wv0 = c.wv;
  drive(1000, 0);
  CHECK(c.wv == wv0);

  // Clock running: at most one step per event (2 events in 512 ticks).
  int changes = 0;
  int16_t prev = c.wv;
  for (int t = 0; t < 512; t++) {
    drive(1);
    if (c.wv != prev) changes++;
    prev = c.wv;
  }
  CHECK(changes <= 2);
  suite_ok("bug: rwalk holds between events");
}

// Bug 2: after a reset the sequencer outputs step 1 for the first cycle (t=0
// is the step-1 slot, as the existing step-sequence suite assumes), not the
// leftover sh_val = 1 from channels_reset().
static void test_bug_step_first_cycle() {
  channels_init(DEFAULT_RATIO);
  Channel &c = channels[0];
  c.p.mode = MODE_STEP; c.p.ratio = 16; c.p.step_len = 4;
  c.p.steps[0] = 200; c.p.steps[1] = 10; c.p.steps[2] = 20; c.p.steps[3] = 30;
  c.p.cv_min = 0; c.p.cv_max = 5000; c.p.slew = 0; c.p.prob = 100; c.p.eucl_en = 0;
  channels_reset();
  drive(1);
  CHECK(c.sh_val == 200);
  drive(100);                            // still inside the first cycle
  CHECK(c.sh_val == 200);
  CHECK(c.out_mv > 3000);                // 200/255 of 5 V, not ~0 mV
  drive(156);                            // t=257: first event -> step 2
  CHECK(c.sh_val == 10);
  suite_ok("bug: step plays step 1 in the first cycle");
}

// Bug 3: ratch emits exactly `ratchets` pulses per cycle for every count, and
// the gate is low on the cycle's last tick (no stray slot from 256 % n).
static void test_bug_ratchet_counts() {
  int wrong_edges = 0, high_at_end = 0;
  for (uint8_t n = 1; n <= 16; n++) {
    channels_init(DEFAULT_RATIO);
    Channel &c = channels[0];
    c.p.mode = MODE_RATCHET; c.p.ratio = 16; c.p.ratchets = n; c.p.width = 64;
    c.p.delay = 0; c.p.cv_min = 0; c.p.cv_max = 5000; c.p.slew = 0;
    c.p.eucl_en = 0; c.p.prob = 100;
    // delta=96: one cycle per 256 ticks, first event at t=256. Measure 3 cycles.
    int rising = 0; uint8_t prev = 0;
    for (uint32_t t = 0; t < 1024; t++) {
      drive(1);
      if (t >= 256) {
        if (c.gate_state && !prev) rising++;
        if (t % 256 == 254 && c.gate_state) high_at_end++;   // last tick of a cycle
      }
      prev = c.gate_state;
    }
    if (rising != 3 * n) { wrong_edges++; printf("  ratchet n=%u: %d pulses in 3 cycles, want %d\n", n, rising, 3 * n); }
  }
  CHECK(wrong_edges == 0);
  CHECK(high_at_end == 0);
  suite_ok("bug: ratchet pulse count for every n");
}

// Bug 4: a channel set to `off` has no parameters. Tapping the RIGHT push
// there must not divide by zero (traps on x86, silently moves the cursor on
// ARM) and turning the RIGHT encoder must not edit the hidden `ratio`.
static void ui_test_reset(uint8_t page) {
  hal_input_init();                      // encoders at rest, buttons released
  ui_nav = UiNav{};
  ui_nav.page = page;
}

static void test_bug_ui_off_channel() {
  channels_init(DEFAULT_RATIO);
  channels[0].p.mode = MODE_OFF;
  channels[0].p.ratio = 16;

  // RIGHT push tap. Run in a child: on x86 the % 0 kills the process.
  fflush(stdout);
  pid_t pid = fork();
  if (pid == 0) {
    ui_test_reset(1);
    btn_enc_r_sw.s = 0x3F;               // next poll reads 0x7F = just released
    ui_service();
    _exit(ui_nav.param_idx == 0 ? 0 : 1);
  }
  int status = 0;
  waitpid(pid, &status, 0);
  CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);

  // RIGHT encoder: one clockwise detent (the controller negates enc_r).
  ui_test_reset(1);
  enc_r.acc = -ENC_CLICK_STEPS;
  ui_service();
  CHECK(channels[0].p.ratio == 16);

  ui_nav = UiNav{};
  suite_ok("bug: UI on an `off` channel");
}

// Bug 5: a TR1 pulse landing between tick()'s time read and its read of the
// last-pulse timestamp must not look like a stopped clock.
static void pulse_now() {
  mock_time_us += 3;
  masterClock.pulse_external();
}

static void test_bug_ext_timeout_race() {
  ext_clock_start(24);
  const uint32_t p120 = 60000000u / (120u * 24u);
  for (int i = 0; i < 20; i++) {
    mock_time_us += p120;
    masterClock.pulse_external();
    masterClock.tick();
    masterClock.take_delta();
  }
  CHECK(masterClock.measured_bpm_cent() != 0);
  mock_time_us += 5000;                  // mid-period, interpolating
  mock_now_hook = pulse_now;             // pulse ISR fires inside tick()'s time read
  masterClock.tick();
  mock_now_hook = nullptr;
  CHECK(masterClock.measured_bpm_cent() != 0);   // tempo not dropped
  masterClock.take_delta();
  suite_ok("bug: external timeout race");
}

// Bug 6: SYS "defaults" restores the factory clock, swing included.
static void test_bug_defaults_swing() {
  masterClock.swing = 40;
  patch_defaults();
  CHECK(masterClock.swing == 0);
  masterClock.swing = 0;
  suite_ok("bug: defaults resets swing");
}

// Bug 7: ui_draw renders ONE page per frame, even if the ISR changes page
// while it draws (snapshot the nav state once). Simulated by flipping from
// OUT1 to the master page right after the first text is printed.
static std::string render_frame(uint8_t page, void (*hook)()) {
  ui_nav = UiNav{};
  ui_nav.page = page;
  Adafruit_GFX &g = oled_gfx();
  g.text.clear();
  g.print_hook = hook;
  ui_draw();
  g.print_hook = nullptr;
  return g.text;
}

static void flip_to_master() { ui_nav.page = 0; }

static void test_bug_draw_consistent_page() {
  channels_init(DEFAULT_RATIO);
  masterClock = MasterClock{};
  const std::string out1 = render_frame(1, nullptr);
  const std::string master = render_frame(0, nullptr);
  CHECK(out1 != master);
  const std::string torn = render_frame(1, flip_to_master);
  CHECK(torn == out1 || torn == master);
  if (torn != out1 && torn != master) printf("  torn frame: %s\n", torn.c_str());
  ui_nav = UiNav{};
  suite_ok("bug: draw uses one page per frame");
}

// Bug 8: `run` = stop also halts the external clock (pulses and interpolation).
static void test_bug_ext_stop() {
  ext_clock_start(24);
  const uint32_t p120 = 60000000u / (120u * 24u);
  for (int i = 0; i < 20; i++) {
    mock_time_us += p120;
    masterClock.pulse_external();
    masterClock.tick();
    masterClock.take_delta();
  }
  masterClock.running = false;
  uint32_t moved = 0;
  for (int i = 0; i < 4; i++) {
    for (int k = 0; k < 20; k++) {       // interpolation ticks between pulses
      mock_time_us += 1000;
      masterClock.tick();
      moved += masterClock.take_delta();
    }
    mock_time_us += p120 - 20000;
    masterClock.pulse_external();
    masterClock.tick();
    moved += masterClock.take_delta();
  }
  CHECK(moved == 0);
  masterClock.running = true;
  suite_ok("bug: stop halts external clock");
}

// Bug 9: `lag` is a time constant. The menu shows v*8 ms and the wiki documents
// "off ... ~2 s (8...2040 ms)": a bigger value must be SLOWER, and a step must
// reach ~63% (one time constant) after about v*8 ms. The filter used slew/256
// as the per-tick coefficient, so 1 was the slowest (~256 ms) and 255 ~instant.
static void test_bug_lag_time_constant() {
  static const uint8_t lags[] = { 1, 8, 32, 125, 255 };
  uint32_t prev_t = 0;
  for (uint8_t lag : lags) {
    channels_init(DEFAULT_RATIO);
    Channel &k = channels[0];
    k.p.mode = MODE_STEP; k.p.cv_min = 0; k.p.cv_max = 10000; k.p.slew = lag;
    for (int i = 0; i < MAX_STEPS; i++) k.p.steps[i] = 255;
    k.sh_val = 255;                      // delta 0: hold a 0 -> 10 V step
    k.filt_x256 = 0;
    uint32_t t = 0;
    while (t < 20000 && k.out_mv < 6321) { drive(1, 0); t++; }
    char shown[16];
    menu_fmt_val(0, P_SLEW, MENU_CHANNEL, shown, sizeof(shown));
    const uint32_t want = (uint32_t)lag * 8;
    const bool ok = t * 10 >= want * 8 && t * 10 <= want * 12 + 20;   // ±20% (+2 ms)
    if (!ok) printf("  lag %u (menu \"%s\"): 63%% after %u ms, want ~%u ms\n", lag, shown, t, want);
    CHECK(ok);
    CHECK(t > prev_t);                   // bigger lag is strictly slower
    prev_t = t;
  }
  suite_ok("bug: lag matches its displayed time constant");
}

// Bug 10: `delay` is a ratch shape param (menu + wiki) but the ratchet gate
// was timed from the undelayed phase, so delay had no effect. A delay of d
// (256ths of the cycle) must shift the whole burst train d/256 of a cycle late.
static void test_bug_ratchet_delay() {
  static uint8_t trace[2][1024];
  static const uint8_t delays[2] = { 0, 32 };
  for (uint8_t k = 0; k < 2; k++) {
    channels_init(DEFAULT_RATIO);
    Channel &c = channels[0];
    c.p.mode = MODE_RATCHET; c.p.ratio = 16; c.p.ratchets = 4; c.p.width = 64;
    c.p.delay = delays[k]; c.p.cv_min = 0; c.p.cv_max = 5000; c.p.slew = 0;
    c.p.eucl_en = 0; c.p.prob = 100;
    for (uint32_t t = 0; t < 1024; t++) { drive(1); trace[k][t] = c.gate_state; }
  }
  // delta=96: one cycle per 256 ticks, so delay 32 = 32 ticks late (half of
  // a 64-tick sub-slot, so an ignored delay cannot line up by accident).
  int mismatch = 0;
  for (uint32_t t = 512; t < 1024; t++) if (trace[1][t] != trace[0][t - 32]) mismatch++;
  if (mismatch) printf("  ratchet delay 32: %d ticks differ from the 32-tick-shifted trace\n", mismatch);
  CHECK(mismatch == 0);
  suite_ok("bug: ratchet honours delay");
}

// Bug 11: an `off` channel kept accumulating phase without wrapping it to its
// cycle. After ~24 h at 120 BPM the uint32 overflowed and, since 2^32 is not a
// multiple of the cycle, the channel came back permanently out of phase with
// a twin that had been running all along. Big deltas reach the wrap quickly.
static void test_bug_off_phase_wrap() {
  channels_init(DEFAULT_RATIO);
  Channel &off = channels[0], &on = channels[1];
  off.p.mode = MODE_OFF;   off.p.ratio = 16;
  on.p.mode  = MODE_PULSE; on.p.ratio  = 16;
  channels_reset();
  drive(300, 0x01000001u);               // 300 * ~2^24 > 2^32: crosses the wrap
  off.p.mode = MODE_PULSE;
  drive(1, 96);
  CHECK(off.pos_x256 == on.pos_x256);    // still phase-locked to its twin
  CHECK(off.event_count <= 1);           // no bogus event burst on re-enable
  suite_ok("bug: off channel stays phase-locked");
}

// Bug 12: XOP_HOLD froze the output while the source was LOW, the inverse of
// the wiki ("freeze while the source is high"). Logic XOPs also used a 5 V
// source threshold while this channel's own "high" was 1 V, so AND/XOR never
// fired with the default 3 V gate level.
static void test_bug_crossop_gate_levels() {
  channels_init(DEFAULT_RATIO);
  Channel &a = channels[0], &b = channels[1];
  setup_pulse(a, 16, 255); a.p.cv_min = 0; a.p.cv_max = 3000;
  setup_pulse(b, 16, 255); b.p.cv_min = 0; b.p.cv_max = 3000;
  b.p.xop = XOP_AND; b.p.xsrc = 2;
  int mismatch = 0, highs = 0;
  for (uint32_t t = 0; t < 512; t++) {
    drive(1);
    if (t < 256) continue;
    if ((a.out_mv > 0) != (b.out_mv > 0)) mismatch++;
    if (b.out_mv > 0) highs++;
  }
  CHECK(mismatch == 0);
  CHECK(highs > 0);

  channels_init(DEFAULT_RATIO);
  setup_pulse(b, 16, 255); b.p.cv_min = 0; b.p.cv_max = 3000;
  setup_pulse(a, 16, 255); a.p.cv_min = 0; a.p.cv_max = 3000;
  b.p.xop = XOP_XOR; b.p.xsrc = 2;
  highs = 0;
  for (uint32_t t = 0; t < 512; t++) {
    drive(1);
    if (t >= 256 && b.out_mv > 0) highs++;
  }
  CHECK(highs == 0);           // both gates are high together -> XOR is low

  channels_init(DEFAULT_RATIO);
  setup_pulse(a, 16, 255); a.p.cv_min = 0; a.p.cv_max = 3000;
  setup_pulse(b, 16, 1);   b.p.cv_min = 0; b.p.cv_max = 3000;
  b.p.xop = XOP_OR; b.p.xsrc = 2;
  highs = 0;
  for (uint32_t t = 0; t < 512; t++) {
    drive(1);
    if (t >= 256 && b.out_mv > 0) highs++;
  }
  CHECK(highs > 200);          // source high keeps OR high even when own gate is low

  channels_init(DEFAULT_RATIO);
  Channel &s = channels[0], &h = channels[1];
  setup_pulse(s, 16, 64); s.p.cv_min = 0; s.p.cv_max = 10000;
  h.p.mode = MODE_LFO; h.p.ratio = 16; h.p.wave = LFO_SAW;
  h.p.cv_min = 0; h.p.cv_max = 10000; h.p.slew = 0;
  h.p.xop = XOP_HOLD; h.p.xsrc = 2;
  channels_reset();
  int high_ticks = 0, low_ticks = 0, moved_high = 0, moved_low = 0;
  uint16_t prev = h.out_mv;
  for (uint32_t t = 0; t < 512; t++) {
    drive(1);
    if (t < 256) { prev = h.out_mv; continue; }
    const bool same = h.out_mv == prev;
    if (s.gate_state) {
      high_ticks++;
      if (!same) moved_high++;
    } else {
      low_ticks++;
      if (!same) moved_low++;
    }
    prev = h.out_mv;
  }
  CHECK(high_ticks > 0);
  CHECK(moved_high == 0);          // frozen while the source is high
  CHECK(moved_low == low_ticks);   // free-running while the source is low
  suite_ok("bug: cross-op hold polarity / gate thresholds");
}

// Bug 13: a user scale whose lowest note is above position 0 made MODE_NOTE
// emit note 0, which is off-scale, for the first part of every ramp.
static void test_bug_note_scale_bottom() {
  const uint16_t saved = USER_SCALES[0];
  USER_SCALES[0] = 0x800;               // only semitone 11
  channels_init(DEFAULT_RATIO);
  Channel &c = channels[0];
  c.p.mode = MODE_NOTE; c.p.ratio = 16;
  c.p.scale = SCALE_COUNT - USER_SCALE_COUNT;   // first user slot
  c.p.cv_min = 0; c.p.cv_max = 2490; c.p.slew = 0;
  channels_reset();
  bool off_scale = false;
  int seen11 = 0;
  for (uint32_t t = 0; t < 256; t++) {
    drive(1);
    int k = grid_semi(c.out_mv);
    if (k < 0 || !((0x800 >> (k % 12)) & 1)) off_scale = true;
    if (k >= 0 && (k % 12) == 11) seen11++;
  }
  CHECK(!off_scale);
  CHECK(seen11 > 0);
  USER_SCALES[0] = saved;
  suite_ok("bug: note quantiser bottom of sparse scale");
}

// Bug 14: the external-clock timeout did `(int32_t)(avg * 4.0f)`. With a very
// slow clock avg*4 exceeds INT32_MAX, so the cast is undefined behaviour.
static void test_bug_clock_large_gap() {
  ext_clock_start(1);
  for (int i = 0; i < 5; i++) {
    mock_time_us += 600000000u;          // 600 s between edges, avg*4 > INT32_MAX
    masterClock.pulse_external();
  }
  masterClock.tick();                    // consume the five pulses
  masterClock.tick();                    // now evaluate the timeout gap
  CHECK(masterClock.measured_period_us() == 600000000u);
  masterClock = MasterClock{};
  mock_time_us = 1000000;
  suite_ok("bug: large external clock timeout");
}

// Bug 15: two built-in scale masks held the wrong notes. Iwato is
// 1 b2 4 b5 b7 = {0,1,5,6,10}; blues major is 1 2 b3 3 5 6 = {0,2,3,4,7,9}.
static int scale_index(const char *name) {
  for (int i = 0; i < SCALE_COUNT; i++)
    if (strcmp(SCALE_NAMES[i], name) == 0) return i;
  return -1;
}

static void test_bug_scale_masks() {
  const int iwato = scale_index("iwato"), blsmaj = scale_index("blsmaj");
  CHECK(iwato >= 0 && blsmaj >= 0);
  if (iwato >= 0) CHECK(scale_mask((uint8_t)iwato, USER_SCALES) == 0x463);
  if (blsmaj >= 0) CHECK(scale_mask((uint8_t)blsmaj, USER_SCALES) == 0x29D);
  suite_ok("bug: iwato / blues major scale masks");
}

// Bug 16: editing swing in the middle of a quarter permanently shifted the
// grid. The warp assumed the current quarter had been swung with the NEW
// amount from its start, so that quarter emitted more or less than QT and
// every later boundary stayed off by the difference (19 % of a quarter for
// 0 -> 40 at the half-way point).
static void test_bug_swing_change_mid_beat() {
  const uint32_t QT = (uint32_t)TICKS_PER_QUARTER * 256;
  masterClock = MasterClock{};
  masterClock.bpm_cent = 12000;          // 500 ticks = exactly one quarter
  masterClock.source = 0; masterClock.running = true; masterClock.swing = 0;
  masterClock.reset_phase();

  uint32_t total = 0;
  run_clock(250, &total);
  masterClock.swing = 40;                // switched on half-way through
  run_clock(250, &total);
  CHECK(total == QT);                    // quarter boundary still on time

  // Several edits inside one quarter, ending with swing off again.
  run_clock(100, &total); masterClock.swing = 63;
  run_clock(150, &total); masterClock.swing = 10;
  run_clock(100, &total); masterClock.swing = 0;
  run_clock(150, &total);
  CHECK(total == 2 * QT);

  // ... and the grid stays locked afterwards.
  masterClock.swing = 33;
  run_clock(1500, &total);
  CHECK(total == 5 * QT);

  // No phase jump: dropping hard swing early in the first eighth must not
  // dump the warp difference (~2400 units) into a single tick.
  masterClock.reset_phase();
  masterClock.swing = 63;
  total = 0;
  run_clock(100, &total);
  masterClock.swing = 0;
  uint32_t max_step = 0;
  for (int i = 0; i < 400; i++) {
    masterClock.tick();
    const uint32_t d = masterClock.take_delta();
    if (d > max_step) max_step = d;
    total += d;
  }
  CHECK(total == QT);
  CHECK(max_step <= 100);                // straight is ~49 units/tick
  masterClock = MasterClock{};
  suite_ok("bug: swing edit mid-quarter keeps the grid");
}

// Bug 17: holding the RIGHT push resets every channel to phase 0, but the
// master clock kept its position inside the swung quarter. With swing on,
// the channels then restarted mid-quarter and the swing landed on the wrong
// eighths. After the reset, the clock must swing exactly like a fresh start.
static void test_bug_reset_realigns_swing() {
  masterClock = MasterClock{};
  masterClock.bpm_cent = 12000; masterClock.source = 0;
  masterClock.running = true; masterClock.swing = 63;
  masterClock.reset_phase();
  uint32_t ref = 0;
  run_clock(250, &ref);                  // fresh start: first half of the raw quarter

  masterClock.reset_phase();
  run_clock(250, nullptr);               // now mid-quarter
  channels_init(DEFAULT_RATIO);
  channels[0].pos_x256 = 12345;
  ui_test_reset(1);
  mock_pins_low = 1ull << PIN_ENC_R_SW;  // hold the RIGHT push
  for (int i = 0; i < 700 && channels[0].pos_x256 != 0; i++) ui_service();
  CHECK(channels[0].pos_x256 == 0);      // long press fired
  mock_pins_low = 0;
  for (int i = 0; i < 20; i++) ui_service();

  uint32_t after = 0;
  run_clock(250, &after);
  CHECK(after == ref);                   // first (late) eighth, like a fresh start
  ui_nav = UiNav{};
  masterClock = MasterClock{};
  suite_ok("bug: channel reset realigns the swing quarter");
}

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
  test_patch_autosave();
  test_patch_engine_lock();
  test_clock_external_robust();
  test_channel_ext_gate_stop();
  test_clock_external_interp();
  test_bug_rwalk_holds();
  test_bug_step_first_cycle();
  test_bug_ratchet_counts();
  test_bug_ui_off_channel();
  test_bug_ext_timeout_race();
  test_bug_defaults_swing();
  test_bug_draw_consistent_page();
  test_bug_ext_stop();
  test_bug_lag_time_constant();
  test_bug_ratchet_delay();
  test_bug_off_phase_wrap();
  test_bug_crossop_gate_levels();
  test_bug_note_scale_bottom();
  test_bug_clock_large_gap();
  test_bug_scale_masks();
  test_bug_swing_change_mid_beat();
  test_bug_reset_realigns_swing();
  printf("%d checks, %d failures\n", g_checks, g_failed);
  return g_failed ? 1 : 0;
}
