#pragma once
// Infra: idle screensaver. After PNC_SA_TIMEOUT_MS without encoder/button
// input the panel shows a drifting two-source wave-interference field. Wake
// input is consumed (the turn that wakes never nudges a value).
// Moved verbatim from infra/ui.cpp (Phase 4).
#include <Arduino.h>
#include "pam_config.h"

void saver_note_boot();   // arm the idle timer (call from ui_init)
// Per-tick gate for ui_service: observes the already-polled input devices and
// manages active/wake-hold/timeout state. `busy` is true while a scale/ratio
// editor or the app menu owns the encoders (the saver never enters then).
// Returns true when the tick's input was consumed and the controller must do
// nothing more this tick.
bool saver_consume(bool busy);
// For ui_draw: true while the interference field owns the panel.
bool saver_showing();
void saver_draw();
