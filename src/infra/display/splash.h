#pragma once
// Infra: boot splash animation ("Pam's Newer Crime" heist chase). Runs from
// ui_init(), before the control timer is armed, so delay()/SPI here are safe.
// Moved verbatim from infra/ui.cpp (Phase 4).
#include <Arduino.h>
#include "pam_config.h"

#ifndef PNC_SPLASH
#define PNC_SPLASH 1
#endif

// Text setup + (optionally) the ~3 s chase. Leaves the text color selected.
void splash_boot();
