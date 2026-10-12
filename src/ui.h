#pragma once
#include <Arduino.h>
#include "pam_config.h"
#include "channel.h"

void ui_init();
void ui_service();
void ui_draw();
uint8_t ui_current_page();
uint8_t ui_current_channel();
