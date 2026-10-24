#include "infra/ui.h"
#include "infra/display/ui_controller.h"
#include "infra/display/ui_nav.h"
#include "infra/display/renderer.h"
#include "infra/display/saver.h"
#include "infra/display/splash.h"
#include "infra/display/oled.h"
#include "infra/hal_input.h"
#include "app/menu_model.h"
#include "app/clock.h"
#include "app/channel.h"
#include "app/patch.h"

UiNav ui_nav;

static uint32_t ui_ticks = 0;

static uint32_t enc_sw_down_tick = 0;
static uint32_t enc_r_sw_down_tick = 0;
static bool enc_r_sw_handled = false;
static bool enc_l_sw_handled = false;
#define kLongPressTicks 500u

// Raw P_* index for a browsable position on the current channel page.
static uint8_t ctl_cur_slen() { return channels[ch_of_page()].p.step_len; }

uint8_t ctl_param_count() {
  if (is_master()) return MASTER_PARAM_COUNT;
  if (is_sys()) return sys_action_count();
  return chan_param_count(channels[ch_of_page()].p.mode, ctl_cur_slen());
}

uint8_t ctl_raw_idx(uint8_t pos) {
  return app_param_at(channels[ch_of_page()].p.mode, pos, ctl_cur_slen());
}

static uint8_t browsable_raw() { return is_chan() ? ctl_raw_idx(ui_nav.param_idx) : ui_nav.param_idx; }

static void adjust(int8_t d) {
  if (is_sys() || ui_nav.app_menu) return;
  MenuPage pg = is_master() ? MENU_MASTER : MENU_CHANNEL;
  uint8_t ch = is_master() ? 0 : ch_of_page();
  uint8_t raw = browsable_raw();
  int32_t v = menu_get_val(ch, raw, pg);
  int32_t step = 1;
  if (is_master() && raw == 0) step = digitalRead(PIN_ENC_R_SW) == LOW ? 25 : 100;
  else if (is_chan() && (raw == P_CVMIN || raw == P_CVMAX))
    step = digitalRead(PIN_ENC_R_SW) == LOW ? 16 : 128;
  menu_set_val(ch, raw, v + d * step, pg);
}

static void page_next() {
  ui_nav.page = (ui_nav.page + 1) % (NUM_OUTPUTS + 2);
  ui_nav.param_idx = 0;
}
static void page_prev() {
  ui_nav.page = (ui_nav.page + (NUM_OUTPUTS + 2) - 1) % (NUM_OUTPUTS + 2);
  ui_nav.param_idx = 0;
}

// RIGHT encoder turn moves the selection; rolling past the end of a page's
// params wraps into the next page (and the start wraps back to the previous).
static void select_rel(int8_t d) {
  uint8_t cnt = ctl_param_count();
  if (!cnt) { ui_nav.param_idx = 0; return; }
  int16_t n = (int16_t)ui_nav.param_idx + d;
  // Stay on the selected page: wrap within its own parameters. Page changes
  // are the panel buttons' job, not the left encoder's.
  if (n < 0) n = cnt - 1;
  else if (n >= cnt) n = 0;
  ui_nav.param_idx = (uint8_t)n;
}

static volatile int8_t pending_action = -1;
static void do_action(uint8_t idx);

void ui_run_pending() {
  if (pending_action < 0) return;
  int8_t a = pending_action;
  pending_action = -1;
  do_action((uint8_t)a);
}

static void do_action(uint8_t idx) {
  switch (idx) {
    case 0: patch_save(SLOT_BANK1); break;
    case 1: patch_save(SLOT_BANK2); break;
    case 2: patch_load(SLOT_BANK1); break;
    case 3: patch_load(SLOT_BANK2); break;
    case 4:
      patch_defaults();
      break;
    case 5:
      ui_nav.ratio_sel = DEFAULT_RATIO;
      ui_nav.ratio_edit = true;
      return;   // commit happens on exit
    case 6: case 7: case 8:
      ui_nav.scale_slot = idx - 6;
      ui_nav.scale_note = 0;
      ui_nav.scale_edit = true;
      return;   // no mark_dirty; edit commits on exit
    default: return;
  }
  mark_dirty();
}

void ui_init() {
  hal_input_init();

  // dac_init() already ran SPI.begin(); panel init needs no address
  oled_begin();
  Serial.printf("pnc: boot, oled %s\n", DISPLAY_KIND);
  splash_boot();
  saver_note_boot();
  ui_nav.param_idx = 0;
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
//   APP menu (open by holding LEFT): scroll modes with either encoder, PRESS
//   either encoder to assign, press a panel button to cancel.
void ui_service() {
  ui_ticks++;

  if (ui_ticks % SCOPE_DECIMATE == 0) renderer_scope_push();

  hal_input_poll();

  // Screensaver gate (owns the tick while active/waking/entering).
  if (saver_consume(ui_nav.scale_edit || ui_nav.ratio_edit || ui_nav.app_menu)) return;

  if (ui_nav.scale_edit) {
    // Left = move note cursor, right = toggle the note on/off in this scale.
    int8_t dl = enc_l.read();
    ui_nav.scale_note = (uint8_t)(((int16_t)ui_nav.scale_note + dl + 12) % 12);
    if (enc_r.read()) USER_SCALES[ui_nav.scale_slot] ^= (1u << ui_nav.scale_note);
    // Commit + exit on either encoder press or a panel button.
    if (btn_enc_sw.released() || btn_enc_r_sw.released() ||
        btn_b2.released() || btn_b1.released()) {
      ui_nav.scale_edit = false;
      mark_dirty();
    }
    return;
  }

  if (ui_nav.ratio_edit) {
    // Either encoder scrolls the ratio (wrapping).
    int8_t d = enc_l.read();
    d += (int8_t)-enc_r.read();
    while (d > 0) { ui_nav.ratio_sel = (uint8_t)((ui_nav.ratio_sel + 1) % RATIO_COUNT); d--; }
    while (d < 0) { ui_nav.ratio_sel = (uint8_t)((ui_nav.ratio_sel + RATIO_COUNT - 1) % RATIO_COUNT); d++; }
    // Either encoder push commits; either panel button cancels.
    if (btn_enc_sw.released() || btn_enc_r_sw.released()) {
      DEFAULT_RATIO = ui_nav.ratio_sel;
      ui_nav.ratio_edit = false;
      mark_dirty();
    } else if (btn_b2.released() || btn_b1.released()) {
      ui_nav.ratio_edit = false;
    }
    return;
  }

  if (ui_nav.app_menu) {
    // Either encoder scrolls the mode list (wrapping).
    int8_t d = enc_l.read();
    d += (int8_t)-enc_r.read();
    while (d > 0) { ui_nav.app_sel = (ui_nav.app_sel + 1) % MODE_LAST; d--; }
    while (d < 0) { ui_nav.app_sel = (ui_nav.app_sel + MODE_LAST - 1) % MODE_LAST; d++; }
    // Either encoder push ASSIGNS the highlighted app and closes (this is the
    // "select" action - a press, matching how Phazerville commits a menu pick).
    if (btn_enc_sw.released() || btn_enc_r_sw.released()) {
      channels[ch_of_page()].p.mode = ui_nav.app_sel;
      mark_dirty();
      ui_nav.app_menu = false;
      ui_nav.param_idx = 0;
      return;
    }
    // Either panel button CANCELS without changing the mode.
    if (btn_b2.released() || btn_b1.released()) { ui_nav.app_menu = false; return; }
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
  while (dl < 0) { select_rel(-1); dl--; }

  // RIGHT push: fine step is read live in adjust(); tap = next param; hold = reset.
  if (btn_enc_r_sw.just_pressed()) { enc_sw_down_tick = ui_ticks; enc_r_sw_handled = false; }
  else if (btn_enc_r_sw.pressed() && !enc_r_sw_handled
           && ui_ticks - enc_sw_down_tick == kLongPressTicks) {
    channels_reset(); enc_r_sw_handled = true;
  } else if (btn_enc_r_sw.released() && !enc_r_sw_handled) {
    if (is_sys()) pending_action = ui_nav.param_idx;
    else if (is_chan()) ui_nav.param_idx = (ui_nav.param_idx + 1) % ctl_param_count();
  }

  // LEFT push: tap = run/stop; hold-release = open the app menu (channel pages).
  // Opening on release (not during the hold) means this same press cannot also
  // trip the menu's "released = cancel" check, which would flash it closed.
  if (btn_enc_sw.just_pressed()) { enc_r_sw_down_tick = ui_ticks; enc_l_sw_handled = false; }
  else if (btn_enc_sw.released()) {
    if (enc_l_sw_handled) { /* consumed by opening */ }
    else if (ui_ticks - enc_r_sw_down_tick >= kLongPressTicks && is_chan()) {
      ui_nav.app_sel = channels[ch_of_page()].p.mode;
      ui_nav.app_menu = true;
      enc_l_sw_handled = true;
    } else if (is_sys()) pending_action = ui_nav.param_idx;
    else { masterClock.running = !masterClock.running; mark_dirty(); }
  }

  // Two panel buttons (Phazerville: B2 = left, B1 = right). Left goes to the
  // previous page, right to the next.
  if (btn_b2.released()) page_prev();
  if (btn_b1.released()) page_next();

  // step_len edits can shrink the list; keep the cursor in range.
  if (is_chan() && ui_nav.param_idx >= ctl_param_count())
    ui_nav.param_idx = ctl_param_count() ? ctl_param_count() - 1 : 0;
}

uint8_t ui_current_page() { return ui_nav.page; }

uint8_t ui_current_channel() { return is_master() ? 0xFF : ch_of_page(); }

bool ui_display_found() { return display_present(); }
