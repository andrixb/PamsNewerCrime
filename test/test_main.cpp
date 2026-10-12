// Native unit tests for Pam's Newer Crime.
//
// Builds the REAL production translation units (euclid.cpp, clock.cpp,
// channel.cpp, storage.cpp) on the host against small Arduino/EEPROM/DAC
// mocks, and exercises their public behaviour. No production code is
// duplicated here: every assertion runs through the shipped engine.
//
//   make test
//
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>

#include "mock/EEPROM.h"

#include "channel.h"
#include "clock.h"
#include "euclid.h"
#include "storage.h"

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
// channel engine
// ---------------------------------------------------------------------------
static void drive(uint32_t ticks, uint32_t delta = 96, uint8_t cv1 = 128, uint8_t cv2 = 128) {
  for (uint32_t t = 0; t < ticks; t++) channels_update(delta, cv1, cv2);
}

// Configure channel 0 from init defaults. delta=96 -> one 1/4 cycle per 256 ticks.
static void setup_pulse(Channel &c, uint8_t ratio, uint8_t width) {
  c.p.mode = MODE_PULSE; c.p.ratio = ratio; c.p.width = width; c.p.delay = 0;
  c.p.cv_min = 0; c.p.cv_max = 5000; c.p.prob = 100; c.p.eucl_en = 0; c.p.slew = 0;
}

static void test_channel_pulse() {
  channels_init();
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
  channels_init();
  Channel &c = channels[0];
  c.p.mode = MODE_DIV; c.p.div_a = 4; c.p.div_b = 0; c.p.width = 64; c.p.slew = 0;
  c.p.cv_min = 0; c.p.cv_max = 5000;
  drive(3072);                          // 12 quarters
  CHECK(c.event_count == 3);            // quarter /4 -> one event per 4 quarters

  channels_init();
  c.p.mode = MODE_DIV; c.p.div_a = -4; c.p.div_b = 0; c.p.width = 64; c.p.slew = 0;
  drive(1024);                          // 4 quarters
  CHECK(c.event_count == 16);           // x4 -> 16 events

  // Stages compose: /3 then x4 -> net 4/3 events per quarter (period = 3/4 quarter).
  channels_init();
  c.p.mode = MODE_DIV; c.p.div_a = 3; c.p.div_b = -4; c.p.width = 64; c.p.slew = 0;
  c.event_count = 0;
  drive(3072);                          // 12 quarters -> 16 events
  CHECK(c.event_count == 16);

  // Extreme multiply clamps safely (x32 * x32 -> /1024 of a quarter).
  channels_init();
  c.p.mode = MODE_DIV; c.p.div_a = -32; c.p.div_b = -32; c.p.width = 64; c.p.slew = 0;
  drive(1024);
  CHECK(c.event_count == 1024u * 96u / 24u);   // total units / period (period clamps to 24)
  suite_ok("channel: clock div/mult");
}

static void test_channel_note() {
  channels_init();
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
  channels_init();
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
  channels_init();
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
  channels_init();
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
  channels_init();
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

  channels_init();
  b.p.mode = MODE_PULSE; setup_pulse(b, 16, 64);
  b.p.xop = XOP_MASK; b.p.xsrc = 0;           // mask by unpatched CV1 (0V)
  int on_low = 0;
  drive(1024);
  for (uint32_t t = 0; t < 256; t++) {        // CV1 stays 0 throughout
    channels_update(96, 0, 0);
    if (b.out_mv > 0) on_low++;
  }
  CHECK(on_low == 0);          // masked dead while source is low
  suite_ok("channel: cross-ops");
}

static void test_channel_quant_app() {
  channels_init();
  Channel &c = channels[1];
  c.p.mode = MODE_QUANT; c.p.quant_src = 0;   // CV1 jack
  c.p.scale = 1;                              // major
  c.p.cv_min = 0; c.p.cv_max = 2490; c.p.slew = 0;

  static const bool in_major[12] = {1,0,1,0,1,1,0,1,0,1,0,1};
  int prev_mv = -1;
  bool off_grid = false, nonmono = false;
  for (int s = 0; s <= 255; s++) {
    channels_update(0, (uint8_t)s, 0);
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
  channels_init();
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
    channels_init();
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
// storage
// ---------------------------------------------------------------------------
static void test_storage() {
  storage_init();

  masterClock.bpm_cent = 13456;
  masterClock.swing = 20;
  USER_SCALES[1] = 0x123;
  channels[0].p.mode = MODE_STEP;
  channels[0].p.ratio = 3;
  channels[0].p.steps[5] = 77;
  channels[0].p.div_a = -8;

  StoreBlob blob;
  capture_state(blob);
  storage_save(SLOT_BANK1, blob);

  masterClock.bpm_cent = 9999;
  masterClock.swing = 0;
  USER_SCALES[1] = 0;
  channels[0].p.mode = MODE_OFF;
  channels[0].p.ratio = 9;
  channels[0].p.steps[5] = 0;
  channels[0].p.div_a = 0;

  StoreBlob loaded;
  CHECK(storage_load(SLOT_BANK1, loaded));
  restore_state(loaded);
  CHECK(masterClock.bpm_cent == 13456);
  CHECK(masterClock.swing == 20);
  CHECK(USER_SCALES[1] == 0x123);
  CHECK(channels[0].p.mode == MODE_STEP);
  CHECK(channels[0].p.ratio == 3);
  CHECK(channels[0].p.steps[5] == 77);
  CHECK(channels[0].p.div_a == -8);

  // Corrupted payload must be rejected by CRC. (SLOT_BANK1 lives at byte 512.)
  EEPROM.data[512 + 40] ^= 0xAA;
  CHECK(!storage_load(SLOT_BANK1, loaded));
  EEPROM.data[512 + 40] ^= 0xAA;   // restore
  CHECK(storage_load(SLOT_BANK1, loaded));

  // A future version must be rejected outright.
  blob.version += 1;
  storage_save(SLOT_BANK2, blob);
  CHECK(!storage_load(SLOT_BANK2, loaded));

  suite_ok("storage");
}

// ---------------------------------------------------------------------------
int main() {
  test_euclid();
  test_clock();
  test_channel_pulse();
  test_channel_div();
  test_channel_note();
  test_channel_step_prob();
  test_channel_euclid();
  test_channel_ratchet();
  test_channel_crossop();
  test_channel_quant_app();
  test_channel_lfo();
  test_storage();
  printf("%d checks, %d failures\n", g_checks, g_failed);
  return g_failed ? 1 : 0;
}
