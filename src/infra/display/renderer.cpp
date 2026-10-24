#include "infra/ui.h"
#include "infra/display/renderer.h"
#include "infra/display/ui_nav.h"
#include "infra/display/ui_controller.h"
#include "infra/display/saver.h"
#include "infra/display/oled.h"
#include "app/menu_model.h"
#include "app/clock.h"
#include "app/channel.h"
#include <string.h>

// Left text column ends and the right scope column begins here. All text,
// selection highlights and group labels stay strictly left of METR_X.
#define METR_X 88

// Scrolling waveform scope drawn in the right column (see ui_draw). The engine
// samples every output at 1 kHz / SCOPE_DECIMATE and shifts the history up, so
// time flows bottom-up: newest sample at the column floor, amplitude mapped
// across the column width. Purely live data - no peak-hold, nothing to latch.
#define SCOPE_ROWS 54            // interior rows of the scope box (y 9..62)
static uint8_t scope_hist[NUM_OUTPUTS][SCOPE_ROWS];  // bits 0..6 = level 0..127, bit 7 = gate
static uint8_t scope_head = 0;                        // index of the newest sample

void renderer_scope_push() {
  scope_head = (uint8_t)((scope_head + 1) % SCOPE_ROWS);
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
    int32_t lo = channels[i].p.cv_min, hi = channels[i].p.cv_max;
    int32_t span = hi - lo;
    int32_t lv = 0;
    if (span > 0) {
      lv = ((int32_t)channels[i].out_mv - lo) * 127 / span;
      if (lv < 0) lv = 0;
      if (lv > 127) lv = 127;
    }
    scope_hist[i][scope_head] = (uint8_t)lv | (channels[i].gate_state ? 0x80 : 0);
  }
}

void ui_draw() {
  if (!display_present()) return;
  if (saver_showing()) { saver_draw(); return; }
  Adafruit_GFX &gfx = oled_gfx();
  gfx.fillScreen(PNC_BLACK);
  gfx.setTextColor(PNC_WHITE);
  uint8_t ch = is_master() ? 0 : ch_of_page();
  MenuPage pg = is_master() ? MENU_MASTER : MENU_CHANNEL;
  char buf[20];

  if (ui_nav.scale_edit) {
    // 12 note cells across the screen; filled = in scale, box = cursor.
    gfx.printf("SCALE USER%d", ui_nav.scale_slot + 1);
    const int16_t cw = 10, cy = 24;
    for (uint8_t n = 0; n < 12; n++) {
      int16_t x = 2 + n * cw;
      bool on = (USER_SCALES[ui_nav.scale_slot] >> n) & 1;
      if (on) gfx.fillRect(x, cy, cw - 2, 20, PNC_WHITE);
      else gfx.drawRect(x, cy, cw - 2, 20, PNC_WHITE);
      if (n == ui_nav.scale_note) gfx.drawRect(x - 1, cy - 1, cw, 22, PNC_WHITE);
    }
    gfx.setCursor(2, 50);
    gfx.printf("L:note R:toggle push:done");
    oled_flush();
    return;
  }

  if (ui_nav.ratio_edit) {
    // 6-row scrolling window over the ratio table; "*" marks the stored default.
    gfx.setCursor(0, 0);
    gfx.printf("DEFAULT RATIO");
    const int16_t y0 = 12, dy = 8, rows = 6;
    int16_t top = 0;
    if (ui_nav.ratio_sel + 1 >= rows)
      top = (ui_nav.ratio_sel >= RATIO_COUNT - 1) ? (RATIO_COUNT - rows) : (ui_nav.ratio_sel - rows + 1);
    for (int16_t r = 0; r < rows; r++) {
      int16_t m = top + r;
      if (m >= RATIO_COUNT) break;
      int16_t y = y0 + r * dy;
      if (m == ui_nav.ratio_sel) { gfx.fillRect(0, y, 128, 8, PNC_WHITE); gfx.setTextColor(PNC_BLACK); }
      gfx.setCursor(2, y);
      gfx.printf("%s", RATIO_TABLE[m].label);
      if (m == DEFAULT_RATIO) { gfx.setCursor(96, y); gfx.printf("*"); }
      if (m == ui_nav.ratio_sel) gfx.setTextColor(PNC_WHITE);
    }
    oled_flush();
    return;
  }

  if (is_master()) {
    gfx.setCursor(0, 0);
    if (masterClock.source) {
      uint16_t mbpm = masterClock.measured_bpm_cent();
      if (mbpm)
        gfx.printf("M %d.%02d EXT", mbpm / 100, mbpm % 100);
      else
        gfx.printf("M --.-- EXT");
    } else {
      gfx.printf("M %d.%02d INT", masterClock.bpm_cent / 100,
                  masterClock.bpm_cent % 100);
    }
  } else if (is_sys()) {
    gfx.setCursor(0, 0);
    gfx.printf("SYS");
  } else {
    gfx.setCursor(0, 0);
    uint8_t gm = channels[ch].p.mode;
    if (ui_nav.app_menu) {
      gfx.printf("PICK APP OUT%d", ch + 1);
    } else {
      // Single header label: the current group name, which defaults to the mode
      // name on shape pages. Truncated so "OUT%d " + label can never reach the
      // meter column (each glyph is 6px wide, meters start at METR_X).
      const char *grp = app_param_group(gm, ui_nav.param_idx, channels[ch].p.step_len);
      int16_t avail = (METR_X - 1) / 6 - 5;   // chars left after "OUT%d "
      if (avail < 0) avail = 0;
      gfx.printf("OUT%d %.*s", ch + 1, avail, grp);
    }
  }

  if (ui_nav.app_menu && !is_master() && !is_sys()) {
    // 8px rows (GFX glyph height); 6-row scrolling window over 11 modes.
    const int16_t y0 = 12, dy = 8;
    const uint8_t rows = 6;
    uint8_t top = 0;
    if (ui_nav.app_sel + 1 >= rows)
      top = (ui_nav.app_sel >= MODE_LAST - 1) ? (MODE_LAST - rows) : (ui_nav.app_sel - rows + 1);
    for (uint8_t r = 0; r < rows; r++) {
      uint8_t m = top + r;
      if (m >= MODE_LAST) break;
      int16_t y = y0 + r * dy;
      if (m == ui_nav.app_sel) { gfx.fillRect(0, y, 128, 8, PNC_WHITE); gfx.setTextColor(PNC_BLACK); }
      gfx.setCursor(2, y);
      gfx.printf("%s", MODE_NAMES[m]);
      if (m == channels[ch].p.mode) {
        gfx.setCursor(96, y);
        gfx.printf("*");   // the output's current app
      }
      if (m == ui_nav.app_sel) gfx.setTextColor(PNC_WHITE);
    }
    oled_flush();
    return;
  }

  if (is_sys()) {
    // 6-row scrolling window (y 12..62): the last item (defaults, scale edits)
    // would otherwise be cut off by the bottom of the 64px panel. Keep the
    // cursor inside the window by scrolling top when it passes the edges.
    const uint8_t rows = 6;
    const uint8_t n_actions = sys_action_count();
    uint8_t top = 0;
    if (ui_nav.param_idx + 1 >= rows)
      top = (ui_nav.param_idx >= n_actions - 1) ? (n_actions - rows) : (ui_nav.param_idx - rows + 1);
    for (uint8_t r = 0; r < rows; r++) {
      uint8_t i = top + r;
      if (i >= n_actions) break;
      int16_t y = 12 + r * 8;
      if (i == ui_nav.param_idx) { gfx.fillRect(0, y, 128, 8, PNC_WHITE); gfx.setTextColor(PNC_BLACK); }
      gfx.setCursor(0, y);
      gfx.printf("%s", SYS_ACTIONS[i]);
      if (i == ui_nav.param_idx) gfx.setTextColor(PNC_WHITE);
    }
    oled_flush();
    return;   // no output meter on the SYS page
  }

  // Master and channel pages: a 3-row scrolling list on the LEFT (x < METR_X),
  // a self-contained output meter on the RIGHT (x >= METR_X). The two zones never
  // overlap: text/highlights are clipped to the left column, and the meter box
  // plus its gate strip live entirely inside the right column's rows 9..62.
  const uint8_t text_w = METR_X - 4;   // selectable text strip width (4px gutter before meters)
  uint8_t cnt = ctl_param_count();
  const uint8_t rows = 3;
  uint8_t top = 0;
  if (cnt > rows) {
    if (ui_nav.param_idx >= rows - 1) top = ui_nav.param_idx >= cnt - 1 ? cnt - rows : ui_nav.param_idx - 1;
  }
  for (uint8_t r = 0; r < rows; r++) {
    uint8_t i_pos = top + r;
    if (i_pos >= cnt) break;
    uint8_t raw = is_master() ? i_pos : ctl_raw_idx(i_pos);
    int16_t y = 12 + r * 14;
    if (i_pos == ui_nav.param_idx) { gfx.fillRect(0, y - 1, text_w, 12, PNC_WHITE); gfx.setTextColor(PNC_BLACK); }
    menu_fmt_val(ch, raw, pg, buf, sizeof(buf));
    // Value right-aligned to the column edge; label left, truncated to whatever
    // room remains (>=1 char gap). The two can never overlap or hit the meter.
    int16_t vlen = (int16_t)strlen(buf);
    int16_t vx = (int16_t)text_w - 6 * vlen;
    if (vx < 0) vx = 0;
    int16_t lmax = (vx - 6) / 6;         // label chars that fit left of the value
    if (lmax < 0) lmax = 0;
    const char *lab = menu_label_of(ch, raw, pg);
    int16_t l = 0;
    while (lab[l] && l < lmax) l++;
    gfx.setCursor(0, y);
    for (int16_t k = 0; k < l; k++) gfx.write(lab[k]);
    gfx.setCursor(vx, y);
    gfx.printf("%s", buf);
    if (i_pos == ui_nav.param_idx) gfx.setTextColor(PNC_WHITE);
  }

  // Right column: scrolling waveform scope. Time flows bottom-up (newest row
  // at the box floor), amplitude maps cv_min..cv_max across the column width,
  // so pulse shape, tempo and level are all readable at a glance. It draws the
  // live signal, so it can never freeze or stick at the top like a meter did.
  const int16_t mx = (int16_t)METR_X;
  const int16_t box_top = 8, box_btm = 63;
  gfx.drawRect(mx, box_top, 128 - mx, box_btm - box_top + 1, PNC_WHITE);
  const int16_t rx1 = mx + (128 - mx) - 2;   // rightmost usable trace column
  if (is_master()) {
    // Overview: one thin band per output, newest sample at each band's bottom,
    // and one black separator row left free between bands (bands never draw it).
    const int16_t bh = (box_btm - box_top - 1) / NUM_OUTPUTS;
    const int16_t rx0 = mx + 1;
    for (uint8_t c = 0; c < NUM_OUTPUTS; c++) {
      int16_t prev = -1;
      for (int16_t r = 0; r < bh - 1; r++) {
        uint8_t s = scope_hist[c][(scope_head + SCOPE_ROWS - (bh - 1) + r) % SCOPE_ROWS];
        int16_t y = box_top + 1 + c * bh + r;
        int16_t x = rx1 - (int16_t)(s & 0x7F) * (rx1 - rx0) / 127;
        if (prev >= 0) gfx.drawLine(prev, y - 1, x, y, PNC_WHITE);
        gfx.drawPixel(x, y, PNC_WHITE);
        prev = x;
      }
    }
  } else {
    // Channel page: full-height trace of this output + a 1px gate-history
    // strip on the left, so gate rhythm reads independently of the CV shape.
    uint8_t c = ch_of_page();
    const int16_t gx = mx + 1;
    const int16_t rx0 = mx + 3;
    int16_t prev = -1;
    for (int16_t r = 0; r < SCOPE_ROWS; r++) {
      uint8_t s = scope_hist[c][(scope_head + 1 + r) % SCOPE_ROWS];
      int16_t y = box_top + 1 + r;
      if (s & 0x80) gfx.drawPixel(gx, y, PNC_WHITE);
      int16_t x = rx1 - (int16_t)(s & 0x7F) * (rx1 - rx0) / 127;
      if (prev >= 0) gfx.drawLine(prev, y - 1, x, y, PNC_WHITE);
      gfx.drawPixel(x, y, PNC_WHITE);
      prev = x;
    }
  }
  oled_flush();
}
