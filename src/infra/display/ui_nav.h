#pragma once
// Infra: shared UI navigation state. Owned and mutated by the controller
// (ui_controller.cpp); read by the renderer. One struct keeps the
// cross-module reads explicit. Split out of infra/ui.cpp (Phase 4).
#include <Arduino.h>
#include "pam_config.h"

struct UiNav {
  uint8_t page = 0;        // 0 = master, 1..NUM_OUTPUTS = channels, NUM_OUTPUTS+1 = sys
  uint8_t param_idx = 0;   // cursor within the page
  bool app_menu = false;   // mode picker open (encoders + push drive it)
  uint8_t app_sel = 0;     // highlighted mode in the picker
  bool scale_edit = false; // user-scale editor open
  uint8_t scale_slot = 0;  // USER_SCALES index
  uint8_t scale_note = 0;  // 0..11 cursor
  bool ratio_edit = false; // default-ratio editor open
  uint8_t ratio_sel = 0;   // highlighted ratio
};

extern UiNav ui_nav;

// Page queries on a given nav state. The renderer runs in the main loop while
// the 1 kHz ISR mutates ui_nav, so it takes one snapshot per frame and asks
// these about the snapshot; the controller (ISR) uses the live forms below.
inline uint8_t ch_of_page(const UiNav &n) { return n.page - 1; }
inline bool is_master(const UiNav &n) { return n.page == 0; }
inline bool is_sys(const UiNav &n) { return n.page == NUM_OUTPUTS + 1; }
inline bool is_chan(const UiNav &n) { return !is_master(n) && !is_sys(n); }

inline uint8_t ch_of_page() { return ch_of_page(ui_nav); }
inline bool is_master() { return is_master(ui_nav); }
inline bool is_sys() { return is_sys(ui_nav); }
inline bool is_chan() { return is_chan(ui_nav); }
