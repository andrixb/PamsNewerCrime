#include "ui.h"
#include "clock.h"
#include "storage.h"
#include <Adafruit_GFX.h>

// ---------------------------------------------------------------------------
// O_C panel: SH1106 128x64 on SPI (shared bus with the DAC8565).
// Own framebuffer + per-page diff flush; noInterrupts() around each page so
// the 1 kHz DAC writes can never interleave on the shared SPI lines.
// Column addressing matches Phazerville: pages start at column 2 (SH1106
// 132-column RAM offset), page-addressing mode.
// ---------------------------------------------------------------------------
#include <SPI.h>
class PncSpiOled : public Adafruit_GFX {
public:
  PncSpiOled() : Adafruit_GFX(128, 64) {
    memset(fb, 0, sizeof(fb));
    memset(last, 0xFF, sizeof(last));
  }
  void begin() {
    pinMode(PIN_OLED_CS, OUTPUT);  digitalWrite(PIN_OLED_CS, HIGH);
    pinMode(PIN_OLED_DC, OUTPUT);  digitalWrite(PIN_OLED_DC, HIGH);
    pinMode(PIN_OLED_RST, OUTPUT);
    digitalWrite(PIN_OLED_RST, HIGH); delay(1);
    digitalWrite(PIN_OLED_RST, LOW);  delay(10);
    digitalWrite(PIN_OLED_RST, HIGH); delay(20);
    static const uint8_t init_seq[] = {
      0xAE,             // display off
      0xD5, 0x80,       // clock divide / osc freq
      0xA8, 0x3F,       // multiplex 1/64
      0xD3, 0x00,       // display offset 0
      0x40,             // start line 0
      0x8D, 0x14,       // charge pump
      0x20, 0x00,       // page addressing mode
      0xA1, 0xC8,       // segment remap / scan dir
      0xDA, 0x12,       // com pins config
      0x81, 0xCF,       // contrast
      0xD9, 0xF1,       // pre-charge period
      0xDB, 0x40,       // vcomh deselect
      0x2E,             // no scroll
      0xA4,             // output RAM->panel
      0xA6,             // non-inverted
      0xAF,             // display on
    };
    digitalWrite(PIN_OLED_CS, LOW);
    xfer(false, init_seq, sizeof(init_seq));
    digitalWrite(PIN_OLED_CS, HIGH);
  }
  void drawPixel(int16_t x, int16_t y, uint16_t c) override {
    if (x < 0 || y < 0 || x >= 128 || y >= 64) return;
    uint8_t *p = &fb[(y >> 3) * 128 + x];
    if (c) *p |=  (uint8_t)(1u << (y & 7));
    else   *p &= (uint8_t)~(1u << (y & 7));
  }
  void flush() {
    for (uint8_t pg = 0; pg < 8; pg++) {
      uint8_t *p = &fb[pg * 128];
      uint8_t *q = &last[pg * 128];
      if (memcmp(p, q, 128) == 0) continue;
      memcpy(q, p, 128);
      const uint8_t seq[3] = { 0x10, 0x02, (uint8_t)(0xB0 | pg) };
      noInterrupts();
      digitalWrite(PIN_OLED_CS, LOW);
      xfer(false, seq, 3);          // command phase (DC low), CS held
      xfer(true, p, 128);           // data phase (DC high)
      digitalWrite(PIN_OLED_CS, HIGH);
      interrupts();
      delayMicroseconds(2);
    }
  }
private:
  uint8_t fb[1024];
  uint8_t last[1024];
  void xfer(bool data, const uint8_t *b, size_t n) {
    digitalWrite(PIN_OLED_DC, data ? HIGH : LOW);
    for (size_t i = 0; i < n; i++) SPI.transfer(b[i]);
  }
};
static PncSpiOled oled_spi;
static Adafruit_GFX *gfx = &oled_spi;
#define PNC_BLACK 0
#define PNC_WHITE 1
static bool display_ok = false;

extern Channel channels[NUM_OUTPUTS];

static const char *MODE_NAMES[] = { "off", "pulse", "LFO", "ramp", "step", "S&H", "note", "env", "logic", "woggle", "rwalk" };
static const char *WAVE_NAMES[] = { "sin", "tri", "saw", "sqr" };
static const char *CVP_NAMES[]  = { "--", "range", "width", "phase", "delay", "prob", "ratio" };
static const char *SCALE_NAMES[] = { "chro", "major", "minor", "dorian", "phryg", "lydian", "mixolyd", "harm", "penta", "pentmn", "blues", "whole" };
static const char *LOGIC_NAMES[] = { "--", "and", "or", "xor", "nand", "nor" };
static const char *SRC_NAMES[2] = { "int", "ext" };
#define SRC_COUNT 2

static const char *SYS_ACTIONS[] = { "save bank A", "save bank B", "load bank A", "load bank B", "defaults" };
#define SYS_ACTION_COUNT (sizeof(SYS_ACTIONS) / sizeof(SYS_ACTIONS[0]))

#define P_RATIO 0
#define P_MODE 1
#define P_WAVE 2
#define P_WIDTH 3
#define P_SKEW 4
#define P_CVMIN 5
#define P_CVMAX 6
#define P_INV 7
#define P_PHASE 8
#define P_DELAY 9
#define P_PROB 10
#define P_EUEN 11
#define P_EULEN 12
#define P_EUFILL 13
#define P_EUROT 14
#define P_SLEN 15
#define P_STEP0 16
#define P_CVS1 32
#define P_CVSC1 33
#define P_CVS2 34
#define P_CVSC2 35
#define P_SLEW 36
#define P_SCALE 37
#define P_ENV_A 38
#define P_ENV_D 39
#define P_LGOP 40
#define P_LGSRC 41
#define P_IN1S 42
#define P_IN2S 43
#define CH_PARAM_COUNT 44

#define MASTER_PARAM_COUNT 4

static uint8_t page = 0;
static uint8_t param_idx = 0;

// ---------------------------------------------------------------------------
// App menu (Phazerville-style). Each channel's "app" is its mode; assigning an
// app makes only that app's parameters browsable, instead of the flat 44-item
// list. Each table below is a list of raw P_* indices the mode actually uses,
// followed by a shared "modulation" block (CV-in destinations + sources + lag)
// that applies to every live mode. 0xFF terminates each list.
// ---------------------------------------------------------------------------
static const uint8_t MOD_BLOCK[] = { P_CVMIN, P_CVMAX, P_INV, P_CVS1, P_CVSC1, P_CVS2, P_CVSC2, P_IN1S, P_IN2S, P_SLEW, 0xFF };

static const uint8_t* app_params(uint8_t mode, uint8_t *out_count) {
  static const uint8_t A_OFF[]   = { 0xFF };
  static const uint8_t A_PULSE[] = { P_RATIO, P_WIDTH, P_DELAY, P_PHASE, P_PROB,
                                     P_EUEN, P_EULEN, P_EUFILL, P_EUROT, 0xFF };
  static const uint8_t A_LFO[]   = { P_RATIO, P_WAVE, P_SKEW, P_PHASE, P_PROB, 0xFF };
  static const uint8_t A_RAMP[]  = { P_RATIO, P_SKEW, P_PHASE, P_PROB, 0xFF };
  static const uint8_t A_STEP[]  = { P_RATIO, P_SLEN, P_STEP0, 0xFF };
  static const uint8_t A_SH[]    = { P_RATIO, P_PHASE, P_PROB, 0xFF };
  static const uint8_t A_NOTE[]  = { P_RATIO, P_SCALE, P_PHASE, P_PROB, 0xFF };
  static const uint8_t A_ENV[]   = { P_RATIO, P_ENV_A, P_ENV_D, P_PROB, 0xFF };
  static const uint8_t A_LOGIC[] = { P_RATIO, P_LGOP, P_LGSRC, P_WIDTH, P_DELAY, P_PHASE, P_PROB,
                                     P_EUEN, P_EULEN, P_EUFILL, P_EUROT, 0xFF };
  static const uint8_t A_WG[]    = { P_RATIO, P_WIDTH, P_SKEW, P_PHASE, 0xFF };
  static const uint8_t A_RW[]    = { P_RATIO, P_WIDTH, P_PROB, 0xFF };

  const uint8_t *hdr;
  switch (mode) {
    case MODE_PULSE: hdr = A_PULSE; break;
    case MODE_LFO:   hdr = A_LFO;   break;
    case MODE_RAMP:  hdr = A_RAMP;  break;
    case MODE_STEP:  hdr = A_STEP;  break;
    case MODE_SH:    hdr = A_SH;    break;
    case MODE_NOTE:  hdr = A_NOTE;  break;
    case MODE_ENV:   hdr = A_ENV;   break;
    case MODE_LOGIC: hdr = A_LOGIC; break;
    case MODE_WGGLE: hdr = A_WG;    break;
    case MODE_RWALK: hdr = A_RW;    break;
    default:         hdr = A_OFF;   break;
  }
  uint8_t n = 0;
  while (hdr[n] != 0xFF) n++;
  if (mode != MODE_OFF) {
    uint8_t m = 0;
    while (MOD_BLOCK[m] != 0xFF) m++;
    n += m;
  }
  if (out_count) *out_count = n;
  return hdr;
}

// Flattened param list for the current channel page (app header + mod block +
// the 16 steps, since A_STEP only lists P_STEP0 as the anchor). Indices are
// relative to the app's own view; step views expand P_STEP0 into P_STEP0..15.
static uint8_t app_param_at(uint8_t mode, uint8_t pos, uint8_t slen) {
  uint8_t count;
  const uint8_t *hdr = app_params(mode, &count);
  uint8_t modn = 0;
  while (MOD_BLOCK[modn] != 0xFF) modn++;
  uint8_t hdrlen = (mode == MODE_OFF) ? count : (uint8_t)(count - modn);

  // Steps mode: header up to P_STEP0, then step_len steps, then mod block.
  if (mode == MODE_STEP) {
    uint8_t base = 2;  // ratio, seq len  (P_SLEN anchors steps)
    uint8_t steps = slen ? slen : 1;
    if (pos < base) return hdr[pos];
    if (pos < base + steps) return P_STEP0 + (pos - base);
    return MOD_BLOCK[pos - base - steps];
  }
  if (pos < hdrlen) return hdr[pos];
  return MOD_BLOCK[pos - hdrlen];
}

static uint8_t chan_param_count(uint8_t mode, uint8_t slen) {
  if (mode == MODE_OFF) return 0;
  uint8_t count;
  app_params(mode, &count);
  uint8_t modn = 0;
  while (MOD_BLOCK[modn] != 0xFF) modn++;
  uint8_t hdrlen = (uint8_t)(count - modn);
  if (mode == MODE_STEP) return 2 + (slen ? slen : 1) + modn;
  return hdrlen + modn;
}


// ---------------------------------------------------------------------------
// Encoder + button decoding. Buttons use the Phazerville 7-bit rolling shift
// (UI::Button<> in ui_button.h): 0x80 = just pressed, 0x7f = just released,
// 0x00 = held — debounced for free at the 1 kHz tick.
//
// Encoder decoding: full quadrature accumulation, not single-edge keying.
// The Phazerville read() fires on one channel's falling edge while the other
// is low. On a detented knob whose click doesn't land on that exact quadrature
// state, that makes one click register as 0 or 2 values (the "can't stick"
// jitter). Instead we track the 2-bit A/B state each tick and sum the net
// transition (-1/0/+1). Bounce and half-positions cancel out (forward step
// followed by back step = 0), so a value only moves once the knob has actually
// turned a detent. ENC_CLICK_STEPS = quadrature steps per detent: 4 for knobs
// that click once per full cycle, 2 if a click is a half cycle. Adjust if every
// detent moves 2 values (raise it) or only every other detent moves (lower it).
#ifndef ENC_CLICK_STEPS
#define ENC_CLICK_STEPS 4
#endif
static const int8_t QD[16] = { 0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0 };

static uint32_t ui_ticks = 0;

struct PhzEncoder {
  uint8_t prev = 0xFF;   // last 2-bit (A<<1|B) state
  int16_t acc = 0;       // accumulated quadrature steps
  void init(uint8_t pin_a, uint8_t pin_b) {
    prev = (digitalRead(pin_a) ? 2 : 0) | (digitalRead(pin_b) ? 1 : 0);
    acc = 0;
  }
  void poll(uint8_t pin_a, uint8_t pin_b) {
    uint8_t cur = (digitalRead(pin_a) ? 2 : 0) | (digitalRead(pin_b) ? 1 : 0);
    if (cur != prev) {
      acc += QD[(prev << 2) | cur];
      prev = cur;
    }
  }
  // Emit whole detents, keep the remainder so partial turns carry over.
  int8_t read() {
    int8_t steps = acc / ENC_CLICK_STEPS;
    acc -= steps * ENC_CLICK_STEPS;
    if (steps > 8) steps = 8;
    if (steps < -8) steps = -8;
    return steps;
  }
};

struct PhzButton {
  uint8_t s = 0xFF;
  void init(uint8_t pin) { pinMode(pin, INPUT_PULLUP); s = 0xFF; }
  void poll(uint8_t pin) { s = (s << 1) | (digitalRead(pin) ? 1 : 0); }
  bool pressed()      const { return s == 0x00; }  // held, debounced
  bool just_pressed() const { return s == 0x80; }  // first clean press sample
  bool released()     const { return s == 0x7F; }  // first clean release sample
};


static PhzEncoder enc_l, enc_r;
static PhzButton btn_enc_sw, btn_b2, btn_enc_r_sw, btn_b1;
static uint32_t enc_sw_down_tick = 0;
static uint32_t enc_r_sw_down_tick = 0;
static bool enc_r_sw_handled = false;
static bool enc_l_sw_handled = false;
#define kLongPressTicks 500u

// App-menu state. When app_menu is true, the encoders + push drive the mode
// picker instead of normal param/value browsing. app_sel is the highlighted mode.
static bool app_menu = false;
static uint8_t app_sel = 0;

static uint8_t ch_of_page() { return page - 1; }
static bool is_master() { return page == 0; }
static bool is_sys() { return page == NUM_OUTPUTS + 1; }
static bool is_chan() { return !is_master() && !is_sys(); }

// Raw P_* index for a browsable position on the current channel page.
static uint8_t cur_slen() { return channels[ch_of_page()].p.step_len; }
static uint8_t raw_idx(uint8_t pos) { return app_param_at(channels[ch_of_page()].p.mode, pos, cur_slen()); }

static uint8_t param_count() {
  if (is_master()) return MASTER_PARAM_COUNT;
  if (is_sys()) return SYS_ACTION_COUNT;
  return chan_param_count(channels[ch_of_page()].p.mode, cur_slen());
}

static int16_t get_val(uint8_t ch, uint8_t idx) {
  if (is_master()) {
    switch (idx) {
      case 0: return masterClock.bpm_cent;
      case 1: return masterClock.source;
      case 2: return masterClock.ext_ppqn;
      default: return masterClock.running;
    }
  }
  ChannelParams &p = channels[ch].p;
  if (idx >= P_STEP0 && idx < P_STEP0 + MAX_STEPS) return p.steps[idx - P_STEP0];
  switch (idx) {
    case P_RATIO: return p.ratio;
    case P_MODE: return p.mode;
    case P_WAVE: return p.wave;
    case P_WIDTH: return p.width;
    case P_SKEW: return p.skew;
    case P_CVMIN: return p.cv_min;
    case P_CVMAX: return p.cv_max;
    case P_INV: return p.invert;
    case P_PHASE: return p.phase;
    case P_DELAY: return p.delay;
    case P_PROB: return p.prob;
    case P_EUEN: return p.eucl_en;
    case P_EULEN: return p.eucl_len;
    case P_EUFILL: return p.eucl_fill;
    case P_EUROT: return p.eucl_rot;
    case P_SLEN: return p.step_len;
    case P_CVS1: return p.cv_src1;
    case P_CVSC1: return p.cv_scale1;
    case P_CVS2: return p.cv_src2;
    case P_SLEW: return p.slew;
    case P_SCALE: return p.scale;
    case P_ENV_A: return p.env_a;
    case P_ENV_D: return p.env_d;
    case P_LGOP: return p.logic_op;
    case P_LGSRC: return p.logic_src;
    case P_IN1S: return p.cv_in1;
    case P_IN2S: return p.cv_in2;
    default: return p.cv_scale2;
  }
}

static void set_val(uint8_t ch, uint8_t idx, int32_t v) {
  if (is_master()) {
    switch (idx) {
      case 0: masterClock.bpm_cent = constrain(v, BPM_MIN, BPM_MAX); break;
      case 1: {
        uint8_t s = (uint8_t)(((v % SRC_COUNT) + SRC_COUNT) % SRC_COUNT);
        if (s != masterClock.source) { masterClock.source = s; masterClock.reset_phase(); }
        break;
      }
      case 2: masterClock.ext_ppqn = constrain(v, 1, 48); break;
      default: masterClock.running = v ? 1 : 0; break;
    }
    mark_dirty();
    return;
  }
  ChannelParams &p = channels[ch].p;
  bool touch_euclid = false;
  if (idx >= P_STEP0 && idx < P_STEP0 + MAX_STEPS) {
    p.steps[idx - P_STEP0] = constrain(v, 0, 255);
  } else switch (idx) {
    case P_RATIO: p.ratio = constrain(v, 0, RATIO_COUNT - 1); break;
    case P_MODE: p.mode = constrain(v, 0, MODE_LAST - 1); break;
    case P_WAVE: p.wave = constrain(v, 0, LFO_LAST - 1); break;
    case P_WIDTH: p.width = constrain(v, 1, 255); break;
    case P_SKEW: p.skew = constrain(v, 0, 255); break;
    case P_CVMIN: p.cv_min = constrain(v, 0, DAC_MAX_MV); if (p.cv_max < p.cv_min) p.cv_max = p.cv_min; break;
    case P_CVMAX: p.cv_max = constrain(v, 0, DAC_MAX_MV); if (p.cv_min > p.cv_max) p.cv_min = p.cv_max; break;
    case P_INV: p.invert = v ? 1 : 0; break;
    case P_PHASE: p.phase = constrain(v, 0, 255); break;
    case P_DELAY: p.delay = constrain(v, 0, 255); break;
    case P_PROB: p.prob = constrain(v, 0, 100); break;
    case P_EUEN: p.eucl_en = v ? 1 : 0; touch_euclid = true; break;
    case P_EULEN: p.eucl_len = constrain(v, 1, EUCLID_MAX_LEN);
                  if (p.eucl_fill > p.eucl_len) p.eucl_fill = p.eucl_len;
                  touch_euclid = true; break;
    case P_EUFILL: p.eucl_fill = constrain(v, 0, p.eucl_len); touch_euclid = true; break;
    case P_EUROT: p.eucl_rot = constrain(v, 0, p.eucl_len ? p.eucl_len - 1 : 0); break;
    case P_SLEN: p.step_len = constrain(v, 1, MAX_STEPS); break;
    case P_CVS1: p.cv_src1 = constrain(v, 0, CVP_LAST - 1); break;
    case P_CVSC1: p.cv_scale1 = constrain(v, -128, 127); break;
    case P_CVS2: p.cv_src2 = constrain(v, 0, CVP_LAST - 1); break;
    case P_SLEW: p.slew = constrain(v, 0, 255); break;
    case P_SCALE: p.scale = constrain(v, 0, SCALE_COUNT - 1); break;
    case P_ENV_A: p.env_a = constrain(v, 0, 255); break;
    case P_ENV_D: p.env_d = constrain(v, 0, 255); break;
    case P_LGOP: p.logic_op = constrain(v, 0, LOGIC_LAST - 1); break;
    case P_LGSRC: p.logic_src = constrain(v, 0, NUM_OUTPUTS); break;
    case P_IN1S: p.cv_in1 = constrain(v, 0, 1 + NUM_OUTPUTS); break;
    case P_IN2S: p.cv_in2 = constrain(v, 0, 1 + NUM_OUTPUTS); break;
    default: p.cv_scale2 = constrain(v, -128, 127); break;
  }
  if (touch_euclid) channel_rebuild_euclid(channels[ch]);
  mark_dirty();
}

static const char *label_of(uint8_t ch, uint8_t idx) {
  static char buf[14];
  if (is_master()) {
    switch (idx) {
      case 0: return "BPM";
      case 1: return "clock";
      case 2: return "ext ppqn";
      default: return "run";
    }
  }
  if (is_sys()) return "";
  if (idx >= P_STEP0 && idx < P_STEP0 + MAX_STEPS) {
    snprintf(buf, sizeof(buf), "step %d", idx - P_STEP0 + 1);
    return buf;
  }
  switch (idx) {
    case P_RATIO: return "ratio";
    case P_MODE: return "mode";
    case P_WAVE: return "wave";
    case P_WIDTH: return "width";
    case P_SKEW: return "skew";
    case P_CVMIN: return "cv min";
    case P_CVMAX: return "cv max";
    case P_INV: return "invert";
    case P_PHASE: return "phase";
    case P_DELAY: return "delay";
    case P_PROB: return "prob";
    case P_EUEN: return "euclid";
    case P_EULEN: return "eu length";
    case P_EUFILL: return "eu fill";
    case P_EUROT: return "eu rot";
    case P_SLEN: return "seq len";
    case P_CVS1: return "cv in 1";
    case P_CVSC1: return "cv1 scale";
    case P_CVS2: return "cv in 2";
    case P_SLEW: return "lag";
    case P_SCALE: return "scale";
    case P_ENV_A: return "attack";
    case P_ENV_D: return "decay";
    case P_LGOP: return "logic";
    case P_LGSRC: return "lg src";
    case P_IN1S: return "cv1 from";
    case P_IN2S: return "cv2 from";
    default: return "cv2 scale";
  }
}

static volatile int8_t pending_action = -1;
static void do_action(uint8_t idx);

void ui_run_pending() {
  if (pending_action < 0) return;
  int8_t a = pending_action;
  pending_action = -1;
  do_action((uint8_t)a);
}

static void fmt_val(uint8_t ch, uint8_t idx, char *buf, size_t n) {
  int16_t v = get_val(ch, idx);
  if (is_master()) {
    switch (idx) {
      case 0: snprintf(buf, n, "%d.%02d", v / 100, v % 100); break;
      case 1: snprintf(buf, n, "%s", SRC_NAMES[v % SRC_COUNT]); break;
      case 2: snprintf(buf, n, "%d", v); break;
      default: snprintf(buf, n, v ? "run" : "stop"); break;
    }
    return;
  }
  ChannelParams &p = channels[ch].p;
  switch (idx) {
    case P_RATIO: snprintf(buf, n, "%s", RATIO_TABLE[p.ratio].label); break;
    case P_MODE: snprintf(buf, n, "%s", MODE_NAMES[p.mode]); break;
    case P_WAVE: snprintf(buf, n, "%s", WAVE_NAMES[p.wave]); break;
    case P_WIDTH: case P_SKEW: case P_PHASE: case P_DELAY: snprintf(buf, n, "%d", v); break;
    case P_CVMIN: case P_CVMAX: snprintf(buf, n, "%dmV", v); break;
    case P_PROB: snprintf(buf, n, "%d%%", v); break;
    case P_INV: case P_EUEN: snprintf(buf, n, v ? "on" : "off"); break;
    case P_CVS1: case P_CVS2: snprintf(buf, n, "%s", CVP_NAMES[v]); break;
    case P_CVSC1: case P_CVSC2: snprintf(buf, n, "%d", v); break;
    case P_SLEW: snprintf(buf, n, v ? "%dms" : "off", v * 8); break;
    case P_SCALE: snprintf(buf, n, "%s", SCALE_NAMES[v]); break;
    case P_ENV_A: case P_ENV_D: snprintf(buf, n, "%dms", v * 32); break;
    case P_LGOP: snprintf(buf, n, "%s", LOGIC_NAMES[v]); break;
    case P_LGSRC: if (v) snprintf(buf, n, "out%d", v); else snprintf(buf, n, "--"); break;
    case P_IN1S: case P_IN2S:
      if (v == 0) snprintf(buf, n, "CV1");
      else if (v == 1) snprintf(buf, n, "CV2");
      else snprintf(buf, n, "out%d", v - 1);
      break;
    default: snprintf(buf, n, "%d", v); break;
  }
}

static void do_action(uint8_t idx) {
  StoreBlob blob;
  switch (idx) {
    case 0: capture_state(blob); storage_save(SLOT_BANK1, blob); break;
    case 1: capture_state(blob); storage_save(SLOT_BANK2, blob); break;
    case 2: if (storage_load(SLOT_BANK1, blob)) restore_state(blob); break;
    case 3: if (storage_load(SLOT_BANK2, blob)) restore_state(blob); break;
    default:
      masterClock.bpm_cent = 12000;
      masterClock.source = 0;
      masterClock.ext_ppqn = 24;
      masterClock.running = true;
      channels_init();
      break;
  }
  mark_dirty();
}

static void oled_flush() { oled_spi.flush(); }

void ui_init() {
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_R_A, INPUT_PULLUP);
  pinMode(PIN_ENC_R_B, INPUT_PULLUP);
  enc_l.init(PIN_ENC_A, PIN_ENC_B);
  enc_r.init(PIN_ENC_R_A, PIN_ENC_R_B);

  btn_enc_sw.init(PIN_ENC_SW);
  btn_b2.init(PIN_BTN_B2);
  btn_enc_r_sw.init(PIN_ENC_R_SW);
  btn_b1.init(PIN_BTN_B1);
  param_idx = 0;

  // dac_init() already ran SPI.begin(); panel init needs no address
  oled_spi.begin();
  display_ok = true;
  Serial.printf("pnc: boot, oled %s\n", DISPLAY_KIND);
  if (!display_ok) return;  // boot without display rather than hanging in setup()
  gfx->setTextSize(1);
  gfx->cp437(true);
}

static uint8_t browsable_raw() { return is_chan() ? raw_idx(param_idx) : param_idx; }

static void adjust(int8_t d) {
  if (is_sys() || app_menu) return;
  uint8_t ch = is_master() ? 0 : ch_of_page();
  uint8_t raw = browsable_raw();
  int32_t v = get_val(ch, raw);
  int32_t step = 1;
  if (is_master() && raw == 0) step = digitalRead(PIN_ENC_R_SW) == LOW ? 25 : 100;
  else if (is_chan() && (raw == P_CVMIN || raw == P_CVMAX))
    step = digitalRead(PIN_ENC_R_SW) == LOW ? 16 : 128;
  set_val(ch, raw, v + d * step);
}

static void page_next() {
  page = (page + 1) % (NUM_OUTPUTS + 2);
  param_idx = 0;
}
static void page_prev() {
  page = (page + (NUM_OUTPUTS + 2) - 1) % (NUM_OUTPUTS + 2);
  param_idx = 0;
}

// RIGHT encoder turn moves the selection; rolling past the end of a page's
// params wraps into the next page (and the start wraps back to the previous).
static void select_rel(int8_t d) {
  if (is_sys()) {
    int8_t n = (int8_t)param_idx + d;
    if (n < 0) n = 0;
    if (n >= (int8_t)SYS_ACTION_COUNT) n = SYS_ACTION_COUNT - 1;
    param_idx = n;
    return;
  }
  int8_t n = (int8_t)param_idx + d;
  if (n < 0) { page_prev(); param_idx = param_count() - 1; }
  else if (n >= (int8_t)param_count()) { page_next(); param_idx = 0; }
  else param_idx = n;
}

// Poll all encoder + button pins once per 1 kHz tick (Phazerville UI::Poll model).
//
// O_C dual-knob scheme:
//   RIGHT turn = edit selected value (hold RIGHT push = fine steps, live in adjust())
//   LEFT  turn = select param / wrap into pages
//   RIGHT push: tap = next param; hold = reset all channel phases
//   LEFT  push: tap = start/stop master clock; hold = open the APP menu
//   APP menu open: LEFT/RIGHT turn scroll modes, RIGHT push = assign+close,
//                  LEFT push or either panel button = cancel
//   TOP button = next page, BOTTOM button = prev page
void ui_service() {
  ui_ticks++;

  enc_l.poll(PIN_ENC_A, PIN_ENC_B);
  enc_r.poll(PIN_ENC_R_A, PIN_ENC_R_B);
  btn_enc_sw.poll(PIN_ENC_SW);
  btn_enc_r_sw.poll(PIN_ENC_R_SW);
  btn_b2.poll(PIN_BTN_B2);
  btn_b1.poll(PIN_BTN_B1);

  if (app_menu) {
    // Both encoders scroll the mode list; wrap around.
    int8_t d = enc_l.read();
    d += enc_r.read();
    while (d > 0) { app_sel = (app_sel + 1) % MODE_LAST; d--; }
    while (d < 0) { app_sel = (app_sel + MODE_LAST - 1) % MODE_LAST; d++; }
    // Assign: RIGHT push tap.
    if (btn_enc_r_sw.released()) {
      channels[ch_of_page()].p.mode = app_sel;
      mark_dirty();
      app_menu = false;
      param_idx = 0;
    }
    // Cancel: LEFT push tap, or either panel button.
    if (btn_enc_sw.released() || btn_b2.released() || btn_b1.released()) app_menu = false;
    return;
  }

  // RIGHT encoder = edit the selected value. Its A/B phase is wired opposite to
  // the left knob, so negate it: clockwise increases, counter-clockwise decreases.
  int8_t dr = (int8_t)-enc_r.read();
  while (dr > 0) { adjust(1); dr--; }
  while (dr < 0) { adjust(-1); dr++; }

  // LEFT encoder = select param / wrap into pages.
  int8_t dl = enc_l.read();
  while (dl > 0) { select_rel(1); dl--; }
  while (dl < 0) { select_rel(-1); dl++; }

  // RIGHT push: fine step is read live in adjust(); tap = next param; hold = reset.
  if (btn_enc_r_sw.just_pressed()) { enc_sw_down_tick = ui_ticks; enc_r_sw_handled = false; }
  else if (btn_enc_r_sw.pressed() && !enc_r_sw_handled
           && ui_ticks - enc_sw_down_tick == kLongPressTicks) {
    channels_reset(); enc_r_sw_handled = true;
  } else if (btn_enc_r_sw.released() && !enc_r_sw_handled) {
    if (is_sys()) pending_action = param_idx;
    else if (is_chan()) param_idx = (param_idx + 1) % param_count();
  }

  // LEFT push: tap = run/stop; hold-release = open the app menu (channel pages).
  // Opening on release (not during the hold) means this same press cannot also
  // trip the menu's "released = cancel" check, which would flash it closed.
  if (btn_enc_sw.just_pressed()) { enc_r_sw_down_tick = ui_ticks; enc_l_sw_handled = false; }
  else if (btn_enc_sw.released()) {
    if (enc_l_sw_handled) { /* consumed by opening */ }
    else if (ui_ticks - enc_r_sw_down_tick >= kLongPressTicks && is_chan()) {
      app_sel = channels[ch_of_page()].p.mode;
      app_menu = true;
      enc_l_sw_handled = true;
    } else if (is_sys()) pending_action = param_idx;
    else { masterClock.running = !masterClock.running; mark_dirty(); }
  }

  // Two buttons: top = next page, bottom = prev page.
  if (btn_b2.released()) page_next();
  if (btn_b1.released()) page_prev();

  // step_len edits can shrink the list; keep the cursor in range.
  if (is_chan() && param_idx >= param_count()) param_idx = param_count() ? param_count() - 1 : 0;
}

void ui_draw() {
  if (!display_ok) return;
  gfx->fillScreen(PNC_BLACK);
  gfx->setTextColor(PNC_WHITE);
  uint8_t ch = is_master() ? 0 : ch_of_page();
  char buf[20];

  if (is_master()) {
    gfx->setCursor(0, 0);
    gfx->printf("M %d.%02d %s", masterClock.bpm_cent / 100, masterClock.bpm_cent % 100,
                masterClock.source ? "EXT" : "INT");
    if (masterClock.source && masterClock.measured_bpm_cent()) {
      gfx->setCursor(80, 0);
      gfx->printf("%d.%02d", masterClock.measured_bpm_cent() / 100, masterClock.measured_bpm_cent() % 100);
    }
  } else if (is_sys()) {
    gfx->setCursor(0, 0);
    gfx->printf("SYS");
  } else {
    gfx->setCursor(0, 0);
    gfx->printf("OUT%d %s", ch + 1, MODE_NAMES[channels[ch].p.mode]);
    if (app_menu) {
      // GFX glyphs are 8px tall, so rows must step by 8 or they overlap. 11 modes
      // don't fit, so keep a 6-row window centred on the highlighted entry.
      gfx->setCursor(96, 0);
      gfx->printf("[%d]", ch + 1);
      const int16_t y0 = 12, dy = 8;
      const uint8_t rows = 6;                 // 6 rows: y 12..52, above the bottom edge
      uint8_t top = 0;
      if (app_sel + 1 >= rows) {
        top = (app_sel >= MODE_LAST - 1) ? (MODE_LAST - rows) : (app_sel - rows + 1);
      }
      for (uint8_t r = 0; r < rows; r++) {
        uint8_t m = top + r;
        if (m >= MODE_LAST) break;
        int16_t y = y0 + r * dy;
        if (m == app_sel) { gfx->fillRect(0, y, 128, 8, PNC_WHITE); gfx->setTextColor(PNC_BLACK); }
        gfx->setCursor(2, y);
        gfx->printf("%s", MODE_NAMES[m]);
        if (m == app_sel) gfx->setTextColor(PNC_WHITE);
      }
      oled_flush();
      return;
    }
  }

  if (is_sys()) {
    for (uint8_t i = 0; i < SYS_ACTION_COUNT; i++) {
      int16_t y = 12 + i * 10;
      if (i == param_idx) { gfx->fillRect(0, y - 1, 128, 10, PNC_WHITE); gfx->setTextColor(PNC_BLACK); }
      gfx->setCursor(0, y);
      gfx->printf("%s", SYS_ACTIONS[i]);
      if (i == param_idx) gfx->setTextColor(PNC_WHITE);
    }
  } else {
    // Master and channel pages share one 3-row scrolling window (y 12/26/40),
    // clear of the voltage bars at y52. Master indices are raw 0..3; channel
    // positions map through the app's parameter table.
    uint8_t cnt = param_count();
    const uint8_t rows = 3;
    uint8_t top = 0;
    if (cnt > rows) {
      if (param_idx >= rows - 1) top = param_idx >= cnt - 1 ? cnt - rows : param_idx - 1;
    }
    for (uint8_t r = 0; r < rows; r++) {
      uint8_t i_pos = top + r;
      if (i_pos >= cnt) break;
      uint8_t raw = is_master() ? i_pos : raw_idx(i_pos);
      int16_t y = 12 + r * 14;
      if (i_pos == param_idx) { gfx->fillRect(0, y - 1, 128, 10, PNC_WHITE); gfx->setTextColor(PNC_BLACK); }
      gfx->setCursor(0, y);
      gfx->printf("%-9s", label_of(ch, raw));
      fmt_val(ch, raw, buf, sizeof(buf));
      gfx->printf("%s", buf);
      if (i_pos == param_idx) gfx->setTextColor(PNC_WHITE);
    }
  }

  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
    uint16_t mv = channels[i].out_mv;
    uint8_t x = i * (128 / NUM_OUTPUTS);
    uint8_t w = (128 / NUM_OUTPUTS) - 3;
    uint8_t h = mv * 10 / DAC_MAX_MV;
    gfx->drawRect(x, 52, w, 10, PNC_WHITE);
    if (h) gfx->fillRect(x + 1, 61 - h, w - 2, h, PNC_WHITE);
    if (channels[i].gate_state) gfx->fillRect(x, 63, w, 1, PNC_WHITE);
  }
  oled_flush();
}

uint8_t ui_current_page() { return page; }

bool ui_display_found() { return display_ok; }
uint8_t ui_current_channel() { return is_master() ? 0xFF : ch_of_page(); }
