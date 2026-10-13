#pragma once
// Infra: O_C panel input devices (encoders, buttons) plus CV input sampling.
// Polled at 1 kHz from the control tick; the controller
// (infra/display/ui_controller) interprets the device state. Moved verbatim
// from infra/ui.cpp and src/main.cpp (Phase 4).
#include <Arduino.h>
#include "pam_config.h"

// Encoder + button decoding. Buttons use the Phazerville 7-bit rolling shift
// (UI::Button<> in ui_button.h): 0x80 = just pressed, 0x7f = just released,
// 0x00 = held — debounced for free at the 1 kHz tick.
//
// Encoder decoding: full quadrature accumulation, not single-edge keying.
// The Phazerville read() fires on one channel's falling edge while the other
// is low. On a detented knob whose click doesn't land on that exact quadrature
// state, that makes one click register as 0 or 2 values (the "can't stick"
// jitter). Instead we track the 2-bit A/B state each tick and sum the net
// transition (-1/0/+1). Bounce and half-positions cancel out (forward step
// followed by back step = 0), so a value only moves once the knob has actually
// turned a detent. ENC_CLICK_STEPS = quadrature steps per detent: 4 for knobs
// that click once per full cycle, 2 if a click is a half cycle. Adjust if every
// detent moves 2 values (raise it) or only every other detent moves (lower it).
#ifndef ENC_CLICK_STEPS
#define ENC_CLICK_STEPS 4
#endif

struct PhzEncoder {
  uint8_t prev = 0xFF;   // last 2-bit (A<<1|B) state
  int16_t acc = 0;       // accumulated quadrature steps
  bool moved = false;    // any quadrature transition this tick (screensaver idle)
  void init(uint8_t pin_a, uint8_t pin_b) {
    prev = (digitalRead(pin_a) ? 2 : 0) | (digitalRead(pin_b) ? 1 : 0);
    acc = 0;
  }
  void poll(uint8_t pin_a, uint8_t pin_b) {
    uint8_t cur = (digitalRead(pin_a) ? 2 : 0) | (digitalRead(pin_b) ? 1 : 0);
    moved = cur != prev;
    if (moved) {
      acc += QD[(prev << 2) | cur];
      prev = cur;
    }
  }
  // Emit whole detents, keep the remainder so partial turns carry over.
  int8_t read() {
    int8_t steps = acc / ENC_CLICK_STEPS;
    acc -= steps * ENC_CLICK_STEPS;
    if (steps > 8) steps = 8;
    if (steps < -8) steps = -8;
    return steps;
  }
 private:
  static const int8_t QD[16];
};

struct PhzButton {
  uint8_t s = 0xFF;
  void init(uint8_t pin) { pinMode(pin, INPUT_PULLUP); s = 0xFF; }
  void poll(uint8_t pin) { s = (s << 1) | (digitalRead(pin) ? 1 : 0); }
  bool pressed()      const { return s == 0x00; }  // held, debounced
  bool just_pressed() const { return s == 0x80; }  // first clean press sample
  bool released()     const { return s == 0x7F; }  // first clean release sample
};

extern PhzEncoder enc_l, enc_r;
extern PhzButton btn_enc_sw, btn_b2, btn_enc_r_sw, btn_b1;

void hal_input_init();    // pin modes + device init (was ui_init's head)
void hal_input_poll();    // poll all six pins (1 kHz tick)
void hal_input_sample_cv();  // ADC read + filter (every 8th tick, was main.cpp)
uint8_t hal_input_cv1();     // filtered 8-bit values for the engine
uint8_t hal_input_cv2();
uint16_t hal_input_cv1_raw();  // filtered 12-bit values (CV bring-up telemetry)
uint16_t hal_input_cv2_raw();
