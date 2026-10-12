#pragma once
#include <Arduino.h>
#include "pam_config.h"
#include "channel.h"

void ui_init();
void ui_service();
void ui_run_pending();
void ui_draw();
uint8_t ui_current_page();
uint8_t ui_current_channel();
bool ui_display_found();
uint8_t ui_display_addr();
