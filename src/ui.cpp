#include "ui.h"
#include "clock.h"
#include "storage.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

extern Channel channels[NUM_OUTPUTS];

static const char *MODE_NAMES[] = { "off", "pulse", "LFO", "ramp", "step", "S&H" };
static const char *WAVE_NAMES[] = { "sin", "tri", "saw", "sqr" };
static const char *CVP_NAMES[]  = { "--", "range", "width", "phase", "delay", "prob", "ratio" };
static const char *SYS_ACTIONS[] = { "save bank A", "save bank B", "load bank A", "load bank B", "defaults" };

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
#define CH_PARAM_COUNT 36

#define MASTER_PARAM_COUNT 4

static Adafruit_SSD1306 display(128, 64, &Wire, DISPLAY_RESET);

static uint8_t page = 0;
static uint8_t param_idx = 0;

static uint8_t enc_state = 0;
static uint32_t b2_down_ms = 0;
static bool b2_was_down = false;

static uint8_t ch_of_page() { return page - 1; }
static bool is_master() { return page == 0; }
static bool is_sys() { return page == NUM_OUTPUTS + 1; }
static uint8_t param_count() { return is_master() ? MASTER_PARAM_COUNT : (is_sys() ? 5 : CH_PARAM_COUNT); }

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
    default: return p.cv_scale2;
  }
}

static void set_val(uint8_t ch, uint8_t idx, int32_t v) {
  if (is_master()) {
    switch (idx) {
      case 0: masterClock.bpm_cent = constrain(v, BPM_MIN, BPM_MAX); break;
      case 1: masterClock.source = v & 1; break;
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
    default: return "cv2 scale";
  }
}

static void fmt_val(uint8_t ch, uint8_t idx, char *buf, size_t n) {
  int16_t v = get_val(ch, idx);
  if (is_master()) {
    switch (idx) {
      case 0: snprintf(buf, n, "%d.%02d", v / 100, v % 100); break;
      case 1: snprintf(buf, n, v ? "ext" : "int"); break;
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

void ui_init() {
  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_ADDRESS)) for (;;) ;
  display.clearDisplay();
  display.setTextColor(SSD1306_BLACK);
  display.fillScreen(SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.cp437(true);
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);
  pinMode(PIN_BTN_B2, INPUT_PULLUP);
  enc_state = (digitalRead(PIN_ENC_B) ? 1 : 0) | (digitalRead(PIN_ENC_A) ? 2 : 0);
  param_idx = 0;
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
  int8_t d = LOOKUP[(enc_state << 2) | st];
  if (d != 0 && st != enc_state) { enc_state = st; adjust(d); }
  else enc_state = st;

  static uint32_t sw_down_ms = 0;
  static bool sw_handled = false;
  if (digitalRead(PIN_ENC_SW) == LOW) {
    if (!sw_down_ms) { sw_down_ms = now; sw_handled = false; }
    else if (!sw_handled && now - sw_down_ms > 500) {
      page = (page + 1) % (NUM_OUTPUTS + 2);
      param_idx = 0;
      sw_handled = true;
    }
  } else if (sw_down_ms) {
    if (!sw_handled) {
      if (is_sys()) do_action(param_idx);
      else param_idx = (param_idx + 1) % param_count();
    }
    sw_down_ms = 0;
  }

  bool b2 = digitalRead(PIN_BTN_B2) == LOW;
  if (b2 && !b2_was_down) { b2_down_ms = now; b2_was_down = true; }
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
  display.fillScreen(SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  uint8_t ch = is_master() ? 0 : ch_of_page();
  char buf[20];

  if (is_master()) {
    display.setCursor(0, 0);
    display.printf("M %d.%02d %s", masterClock.bpm_cent / 100, masterClock.bpm_cent % 100,
                   masterClock.source ? "EXT" : "INT");
    if (masterClock.source && masterClock.measured_bpm_cent()) {
      display.setCursor(80, 0);
      display.printf("%d.%02d", masterClock.measured_bpm_cent() / 100, masterClock.measured_bpm_cent() % 100);
    }
  } else if (is_sys()) {
    display.setCursor(0, 0);
    display.printf("SYS");
  } else {
    display.setCursor(0, 0);
    display.printf("OUT%d %d.%02d %s", ch + 1, masterClock.bpm_cent / 100, masterClock.bpm_cent % 100,
                   masterClock.running ? ">" : "||");
  }

  if (is_sys()) {
    for (uint8_t i = 0; i < 5; i++) {
      int16_t y = 12 + i * 10;
      if (i == param_idx) { display.fillRect(0, y - 1, 128, 10, SSD1306_BLACK); display.setTextColor(SSD1306_WHITE); }
      display.setCursor(0, y);
      display.printf("%s", SYS_ACTIONS[i]);
      if (i == param_idx) display.setTextColor(SSD1306_BLACK);
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
      if (i == param_idx) { display.fillRect(0, y - 1, 128, 10, SSD1306_BLACK); display.setTextColor(SSD1306_WHITE); }
      display.setCursor(0, y);
      display.printf("%-9s", label_of(ch, i));
      fmt_val(ch, i, buf, sizeof(buf));
      display.printf("%s", buf);
      if (i == param_idx) display.setTextColor(SSD1306_BLACK);
    }
  }

  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
    uint16_t mv = channels[i].out_mv;
    uint8_t x = i * (128 / NUM_OUTPUTS);
    uint8_t w = (128 / NUM_OUTPUTS) - 3;
    uint8_t h = mv * 10 / DAC_MAX_MV;
    display.drawRect(x, 52, w, 10, SSD1306_BLACK);
    if (h) display.fillRect(x + 1, 61 - h, w - 2, h, SSD1306_BLACK);
    if (channels[i].gate_state) display.fillRect(x, 63, w, 1, SSD1306_BLACK);
  }
  display.display();
}

uint8_t ui_current_page() { return page; }
uint8_t ui_current_channel() { return is_master() ? 0xFF : ch_of_page(); }
