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

inline uint8_t ch_of_page() { return ui_nav.page - 1; }
inline bool is_master() { return ui_nav.page == 0; }
inline bool is_sys() { return ui_nav.page == NUM_OUTPUTS + 1; }
inline bool is_chan() { return !is_master() && !is_sys(); }
