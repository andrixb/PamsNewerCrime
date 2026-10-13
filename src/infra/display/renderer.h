#pragma once
// Infra: OLED page renderer. Owns the right-column scope history, samples it
// for ui_service, and draws every page. Reads engine/controller state, never
// mutates it. Split out of infra/ui.cpp (Phase 4).
#include <Arduino.h>
#include "pam_config.h"

#define SCOPE_DECIMATE 8   // ms per history row -> ~430 ms window

// Scope sampler for the 1 kHz tick (call when ui_ticks % SCOPE_DECIMATE == 0).
void renderer_scope_push();
