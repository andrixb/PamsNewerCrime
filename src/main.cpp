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

static void IRAM_ATTR ck_isr() {
  clock.pulse_external();
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
  clock.tick();
  uint32_t delta = clock.take_delta();
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
  clock.begin();
  pinMode(PIN_CK_IN, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(PIN_CK_IN), ck_isr, RISING);
  ctrl_timer.begin(control_tick, (uint32_t)(1000000 / CONTROL_RATE_HZ));
  ctrl_timer.priority(16);
}

void loop() {
  uint32_t now = millis();
  if (now - last_ui_ms > 33) {
    last_ui_ms = now;
    ui_draw();
    autosave();
  }
}
