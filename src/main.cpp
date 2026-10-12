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
  ui_service();               // 1 kHz: poll + apply encoders/buttons
  if ((tick_count % 8) == 0) {
    uint16_t r1 = analogRead(PIN_CV_IN1);
    uint16_t r2 = analogRead(PIN_CV_IN2);
    cv1_f = cv1_f - (cv1_f >> 2) + (r1 >> 2);
    cv2_f = cv2_f - (cv2_f >> 2) + (r2 >> 2);
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

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  dac_init();
  storage_init();
  ui_init();
  masterClock.begin();
#if TR_ACTIVE_LOW
  pinMode(PIN_CK_IN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_CK_IN), ck_isr, FALLING);
#else
  pinMode(PIN_CK_IN, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(PIN_CK_IN), ck_isr, RISING);
#endif
  ctrl_timer.begin(control_tick, (uint32_t)(1000000 / CONTROL_RATE_HZ));
  ctrl_timer.priority(16);
  Serial.println("pnc: setup done");
}

static uint32_t last_beat_ms = 0;

void loop() {
  uint32_t beat = millis();
  if (beat - last_beat_ms > 5000) {
    last_beat_ms = beat;
    Serial.printf("pnc: alive ticks=%u bpm=%d page=%d oled=%s\n", tick_count,
                  masterClock.bpm_cent / 100, ui_current_page(),
                  ui_display_found() ? "ok" : "MISSING");
  }
  ui_run_pending();
  uint32_t now = millis();
  if (now - last_ui_ms > 33) {
    last_ui_ms = now;
    ui_draw();
    autosave();
  }
}
