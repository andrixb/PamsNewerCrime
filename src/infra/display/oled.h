#pragma once
// Infra: SH1106 128x64 OLED driver on SPI (shared bus with the DAC8565).
// Own framebuffer + per-page diff flush; each page is one SPI transaction
// with the control-timer IRQ masked (SPI.usingInterrupt), so the 1 kHz DAC
// writes can never interleave on the shared SPI lines while TR1 stays live.
// Column addressing matches Phazerville: pages start at column 2 (SH1106
// 132-column RAM offset), page-addressing mode.
// Moved verbatim from infra/ui.cpp (Phase 4).
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "pam_config.h"

#define PNC_BLACK 0
#define PNC_WHITE 1

void oled_begin();
void oled_flush();
Adafruit_GFX &oled_gfx();
bool display_present();
