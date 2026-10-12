#include <Arduino.h>
#include "pam_config.h"
#include "clock.h"
#include "channel.h"
#include "storage.h"
#include "ui.h"
#include "dac.h"

static IntervalTimer ctrl_timer;
static volatile uint32_t tick_count = 0;
static uint16_t cv1_f = 2048, cv2_f = 2048;
static uint32_t last_save_ms = 0;
static uint32_t last_ui_ms = 0;

static void ck_isr() {
  masterClock.pulse_external();
}

static void control_tick() {
  tick_count++;
  if ((tick_count % 8) == 0) {
    uint16_t r1 = analogRead(PIN_CV_IN1);
    uint16_t r2 = analogRead(PIN_CV_IN2);
    cv1_f = cv1_f - (cv1_f >> 2) + (r1 >> 2);
    cv2_f = cv2_f - (cv2_f >> 2) + (r2 >> 2);
    ui_service();
  }
  masterClock.tick();
  uint32_t delta = masterClock.take_delta();
  channels_update(delta, cv1_f >> 4, cv2_f >> 4);
}

static void autosave() {
  uint32_t now = millis();
  if (state_dirty() && now - last_save_ms > 3000) {
    last_save_ms = now;
    StoreBlob blob;
    capture_state(blob);
    storage_save(SLOT_ACTIVE, blob);
  }
}

static void midi_poll() {
  static int8_t expected_data = 0;  // -1 = inside sysex
  while (MIDI_SERIAL.available()) {
    uint8_t b = MIDI_SERIAL.read();
    if (b >= 0xF8) {  // real-time, can interleave anywhere
      switch (b) {
        case 0xF8: masterClock.pulse_midi(); break;
        case 0xFA: masterClock.source = 2; masterClock.running = true; masterClock.reset_phase(); break;
        case 0xFB: masterClock.running = true; break;
        case 0xFC: masterClock.running = false; break;
      }
      continue;
    }
    if (b >= 0x80) {
      expected_data = (b == 0xF0) ? -1 : ((b >= 0xC0 && b < 0xE0) ? 1 : 2);
      continue;
    }
    if (expected_data < 0) { if (b == 0xF7) expected_data = 0; continue; }
    if (expected_data > 0) expected_data--;
  }
}

void setup() {
  Serial.begin(115200);
  MIDI_SERIAL.begin(31250);
  analogReadResolution(12);
  dac_init();
  storage_init();
  ui_init();
  masterClock.begin();
  pinMode(PIN_CK_IN, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(PIN_CK_IN), ck_isr, RISING);
  ctrl_timer.begin(control_tick, (uint32_t)(1000000 / CONTROL_RATE_HZ));
  ctrl_timer.priority(16);
}

void loop() {
  midi_poll();
  ui_run_pending();
  uint32_t now = millis();
  if (now - last_ui_ms > 33) {
    last_ui_ms = now;
    ui_draw();
    autosave();
  }
}
