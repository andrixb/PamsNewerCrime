#pragma once
// Infra: input state machine + nav actions. Owns UiNav; interprets the
// hal_input devices into app use-case calls (menu_model get/set, patch
// save/load). Split out of infra/ui.cpp (Phase 4).
#include <Arduino.h>
#include "pam_config.h"
#include "infra/display/ui_nav.h"

// Browse helpers over the current page, for the renderer:
// total params on this page / raw P_* index at a flat position.
// The UiNav overloads work on a snapshot (renderer); the plain forms on the
// live ui_nav (controller).
uint8_t ctl_param_count(const UiNav &n);
uint8_t ctl_raw_idx(const UiNav &n, uint8_t pos);
uint8_t ctl_param_count();
uint8_t ctl_raw_idx(uint8_t pos);
