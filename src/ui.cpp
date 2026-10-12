#include "ui.h"
#include "clock.h"
#include "storage.h"
#include <Adafruit_GFX.h>

#if IS_O_C
// ---------------------------------------------------------------------------
// Genuine O_C panel: SH1106 128x64 on SPI (shared bus with the DAC8565).
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
static uint8_t display_addr = 0;
#define PNC_BLACK 0
#define PNC_WHITE 1
#else
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_SH110X.h>
static Adafruit_SSD1306 oled_ssd(128, 64, &Wire, DISPLAY_RESET);
static Adafruit_SH110X  oled_sh(128, 64, &Wire, DISPLAY_RESET);
static Adafruit_GFX *gfx = nullptr;
static uint8_t display_addr = 0;
// 0 = SSD1306, 2 = SH1106
static uint8_t oled_mode = 0;
#define PNC_BLACK 0
#define PNC_WHITE 1
#endif
static bool display_ok = false;

extern Channel channels[NUM_OUTPUTS];

static const char *MODE_NAMES[] = { "off", "pulse", "LFO", "ramp", "step", "S&H", "note", "env", "logic", "woggle", "rwalk" };
static const char *WAVE_NAMES[] = { "sin", "tri", "saw", "sqr" };
static const char *CVP_NAMES[]  = { "--", "range", "width", "phase", "delay", "prob", "ratio" };
static const char *SCALE_NAMES[] = { "chro", "major", "minor", "dorian", "phryg", "lydian", "mixolyd", "harm", "penta", "pentmn", "blues", "whole" };
static const char *LOGIC_NAMES[] = { "--", "and", "or", "xor", "nand", "nor" };
static const char *SRC_NAMES[3] = { "int", "ext", "midi" };
#define SRC_COUNT (HAS_MIDI_IN ? 3 : 2)

static const char *SYS_ACTIONS[] = { "save bank A", "save bank B", "load bank A", "load bank B", "defaults"
#if !IS_O_C
  , "oled: ssd1306", "oled: sh1106"
#endif
};
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

static uint8_t enc_state = 0;
static uint32_t b2_down_ms = 0;
static bool b2_was_down = false;

static uint8_t ch_of_page() { return page - 1; }
static bool is_master() { return page == 0; }
static bool is_sys() { return page == NUM_OUTPUTS + 1; }
static uint8_t param_count() { return is_master() ? MASTER_PARAM_COUNT : (is_sys() ? SYS_ACTION_COUNT : CH_PARAM_COUNT); }

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
#if !IS_O_C
static void oled_pick_mode(uint8_t mode);
#endif

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
#if !IS_O_C
    case 5: oled_pick_mode(0); return;   // no autosave mark for display choice
    case 6: oled_pick_mode(2); return;
#endif
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

#if IS_O_C
static void oled_flush() { oled_spi.flush(); }
#else
static void i2c_bus_recovery() {
  // I2C spec "bus clear": a slave left mid-transfer holds SDA low forever
  // (e.g. if the Teensy resets during a write). Clock SCL until SDA is
  // released, then issue a STOP so the slave aborts cleanly.
  pinMode(PIN_I2C_SCL, OUTPUT);
  digitalWrite(PIN_I2C_SCL, HIGH);
  pinMode(PIN_I2C_SDA, INPUT);
  for (uint8_t i = 0; i < 18; i++) {
    if (digitalRead(PIN_I2C_SDA) == HIGH) break;
    delayMicroseconds(6);
    digitalWrite(PIN_I2C_SCL, LOW);
    delayMicroseconds(6);
    digitalWrite(PIN_I2C_SCL, HIGH);
  }
  pinMode(PIN_I2C_SDA, OUTPUT);
  digitalWrite(PIN_I2C_SDA, LOW);
  delayMicroseconds(6);
  digitalWrite(PIN_I2C_SCL, HIGH);
  delayMicroseconds(6);
  digitalWrite(PIN_I2C_SDA, HIGH);
  delayMicroseconds(6);
  pinMode(PIN_I2C_SDA, INPUT);
  pinMode(PIN_I2C_SCL, INPUT);
}

static void oled_flush() {
  if (oled_mode == 2) oled_sh.display();
  else oled_ssd.display();
}

static bool oled_setup_mode(uint8_t mode, uint8_t addr) {
  bool ok;
  if (mode == 2) {
    ok = oled_sh.begin(addr, true);
    gfx = &oled_sh;
  } else {
    ok = oled_ssd.begin(SSD1306_SWITCHCAPVCC, addr);
    gfx = &oled_ssd;
  }
  if (ok) oled_mode = mode;
  return ok;
}

static uint8_t oled_preferred_mode() {
  uint8_t m = oled_mode_load();
  return (m == 2) ? 2 : 0;
}

// chosen from the SYS page: persist and re-init immediately
static void oled_pick_mode(uint8_t mode) {
  oled_mode_save(mode);
  if (display_addr && oled_setup_mode(mode, display_addr)) {
    gfx->fillScreen(PNC_BLACK);
    oled_flush();
  }
}

// First boot (no saved choice): flash panel with 1=SSD1306, 2=SH1106;
// user picks the one that flashed in the SYS page, choice is remembered.
static void oled_probe(uint8_t addr) {
  Serial.println("pnc: probe -> count white flashes: 1=SSD1306 2=SH1106");
  for (uint8_t m = 0; m <= 2; m += 2) {
    oled_setup_mode(m, addr);
    gfx->fillScreen(PNC_BLACK);
    oled_flush();
    for (uint8_t f = 0; f <= m / 2; f++) {
      gfx->fillScreen(PNC_WHITE); oled_flush(); delay(150);
      gfx->fillScreen(PNC_BLACK); oled_flush(); delay(150);
    }
    delay(600);
  }
  oled_setup_mode(0, addr);
}
#endif

void ui_init() {
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);
  pinMode(PIN_BTN_B2, INPUT_PULLUP);
  enc_state = (digitalRead(PIN_ENC_B) ? 1 : 0) | (digitalRead(PIN_ENC_A) ? 2 : 0);
  param_idx = 0;

#if IS_O_C
  // dac_init() already ran SPI.begin(); panel init needs no address
  oled_spi.begin();
  display_ok = true;
  Serial.printf("pnc: boot, oled %s\n", DISPLAY_KIND);
#else
  i2c_bus_recovery();
  Wire.begin();
  Wire.setClock(400000);
  static const uint8_t ADDRS[] = { DISPLAY_ADDRESS, 0x3D };
  for (uint8_t i = 0; i < 2 && !display_addr; i++) {
    Wire.beginTransmission(ADDRS[i]);
    if (Wire.endTransmission() == 0) display_addr = ADDRS[i];
  }
  uint8_t pref = oled_preferred_mode();
  bool first_boot = (oled_mode_load() == 0xFF);
  display_ok = display_addr && oled_setup_mode(pref, display_addr);
  Serial.printf("pnc: boot, oled %s @0x%02X mode %u\n",
                display_ok ? "init" : "FAIL", display_addr, pref);
  if (!display_addr) {
    Serial.print("pnc: i2c scan:");
    for (uint8_t a = 8; a < 120; a++) {
      Wire.beginTransmission(a);
      if (Wire.endTransmission() == 0) Serial.printf(" 0x%02X", a);
    }
    Serial.println();
  }
  if (display_ok && first_boot) {
    oled_probe(display_addr);
    oled_setup_mode(pref, display_addr);
  }
#endif
  if (!display_ok) return;  // boot without display rather than hanging in setup()
  gfx->setTextSize(1);
  gfx->cp437(true);
}

static void adjust(int8_t d) {
  if (is_sys()) return;
  uint8_t ch = is_master() ? 0 : ch_of_page();
  int32_t v = get_val(ch, param_idx);
  int32_t step = 1;
  if (is_master() && param_idx == 0) step = digitalRead(PIN_ENC_SW) == LOW ? 25 : 100;
  else if (!is_master() && (param_idx == P_CVMIN || param_idx == P_CVMAX))
    step = digitalRead(PIN_ENC_SW) == LOW ? 16 : 128;
  set_val(ch, param_idx, v + d * step);
}

void ui_service() {
  uint32_t now = millis();

  uint8_t a = digitalRead(PIN_ENC_A) ? 1 : 0;
  uint8_t b = digitalRead(PIN_ENC_B) ? 1 : 0;
  uint8_t st = b | (a << 1);
  static const int8_t LOOKUP[16] = { 0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0 };
  // settle filter: accept a new quadrature state only after it is stable
  // for 2 ms, so contact bounce and mid-flight samples can't fake a step
  static uint8_t enc_raw = 0xFF;
  static uint32_t enc_raw_ms = 0;
  if (st != enc_raw) {
    enc_raw = st;
    enc_raw_ms = now;
  } else if (st != enc_state && (uint32_t)(now - enc_raw_ms) >= 2) {
    int8_t d = LOOKUP[(enc_state << 2) | st];
    if (d) adjust(d);
    enc_state = st;
  }

  static uint32_t sw_down_ms = 0;
  static uint32_t sw_up_ms = 0;
  static bool sw_handled = false;
  if (digitalRead(PIN_ENC_SW) == LOW) {
    if (!sw_down_ms && (uint32_t)(now - sw_up_ms) > 30) { sw_down_ms = now; sw_handled = false; }
    else if (!sw_handled && sw_down_ms && now - sw_down_ms > 500) {
      page = (page + 1) % (NUM_OUTPUTS + 2);
      param_idx = 0;
      sw_handled = true;
    }
  } else if (sw_down_ms) {
    if (!sw_handled) {
      if (is_sys()) pending_action = param_idx;
      else param_idx = (param_idx + 1) % param_count();
    }
    sw_down_ms = 0;
    sw_up_ms = now;
  }

  bool b2 = digitalRead(PIN_BTN_B2) == LOW;
  static uint32_t b2_up_ms = 0;
  if (b2 && !b2_was_down && (uint32_t)(now - b2_up_ms) > 30) { b2_down_ms = now; b2_was_down = true; }
  else if (!b2 && b2_was_down) b2_up_ms = now;
  if (b2) {
    if (now - b2_down_ms > 600 && b2_was_down) {
      masterClock.running = !masterClock.running;
      mark_dirty();
      b2_was_down = false;
    }
  } else if (b2_was_down) {
    if (now - b2_down_ms < 600) {
      if (!masterClock.running) masterClock.running = true;
      else channels_reset();
    }
    b2_was_down = false;
  }
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
                masterClock.source == 2 ? "MIDI" : (masterClock.source ? "EXT" : "INT"));
    if (masterClock.source && masterClock.measured_bpm_cent()) {
      gfx->setCursor(80, 0);
      gfx->printf("%d.%02d", masterClock.measured_bpm_cent() / 100, masterClock.measured_bpm_cent() % 100);
    }
  } else if (is_sys()) {
    gfx->setCursor(0, 0);
    gfx->printf("SYS");
  } else {
    gfx->setCursor(0, 0);
    gfx->printf("OUT%d %d.%02d %s", ch + 1, masterClock.bpm_cent / 100, masterClock.bpm_cent % 100,
                masterClock.running ? ">" : "||");
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
    uint8_t cnt = param_count();
    const uint8_t rows = 3;
    uint8_t top = 0;
    if (cnt > rows) {
      if (param_idx >= rows - 1) top = param_idx >= cnt - 1 ? cnt - rows : param_idx - 1;
    }
    for (uint8_t r = 0; r < rows; r++) {
      uint8_t i = top + r;
      if (i >= cnt) break;
      int16_t y = 12 + r * 14;
      if (i == param_idx) { gfx->fillRect(0, y - 1, 128, 10, PNC_WHITE); gfx->setTextColor(PNC_BLACK); }
      gfx->setCursor(0, y);
      gfx->printf("%-9s", label_of(ch, i));
      fmt_val(ch, i, buf, sizeof(buf));
      gfx->printf("%s", buf);
      if (i == param_idx) gfx->setTextColor(PNC_WHITE);
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
uint8_t ui_display_addr() { return display_addr; }
uint8_t ui_current_channel() { return is_master() ? 0xFF : ch_of_page(); }
