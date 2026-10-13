#pragma once
// Public UI facade: init, 1 kHz service, deferred SYS actions, 30 Hz draw and
// page/channel queries. Implemented across infra/display/* (Phase 4).
// main.cpp is the only includer.
#include <Arduino.h>

void ui_init();
void ui_service();
void ui_run_pending();
void ui_draw();
uint8_t ui_current_page();
uint8_t ui_current_channel();
bool ui_display_found();
