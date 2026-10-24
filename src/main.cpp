#include <Arduino.h>
#include "pam_config.h"
#include "app/clock.h"
#include "app/channel.h"
#include "app/patch.h"
#include "infra/ui.h"
#include "infra/hal_input.h"
#include "infra/dac.h"

static IntervalTimer ctrl_timer;
static volatile uint32_t tick_count = 0;
static uint32_t last_ui_ms = 0;

static void ck_isr() {
  masterClock.pulse_external();
}

// Time port for the app clock (external-sync timeout + tempo measurement).
uint32_t clock_now_us() { return micros(); }

static void control_tick() {
  tick_count++;
  ui_service();               // 1 kHz: poll + apply encoders/buttons
  if ((tick_count % 8) == 0) hal_input_sample_cv();
  masterClock.tick();
  uint32_t delta = masterClock.take_delta();
  // App computes (no I/O); infra emits. The env carries the globals the
  // engine needs instead of it reading them itself.
  const EngineEnv env = { USER_SCALES,
                          masterClock.source ? masterClock.measured_bpm_cent()
                                             : masterClock.bpm_cent,
                          masterClock.source };
  channels_update(delta, hal_input_cv1(), hal_input_cv2(), env);
  uint16_t outs[NUM_OUTPUTS];
  uint8_t dirty = channels_take_outputs(outs);
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++)
    if (dirty & (uint8_t)(1u << i)) dac_write(i, outs[i]);
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  dac_init();
  patch_init();
  ui_init();
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
#if PNC_DAC_SELFTEST
  // Block the timer context is not yet contended: ctrl_timer runs at prio 16,
  // but the self-test just writes the DAC directly; the engine also writes it,
  // so to get a clean sweep we briefly disable the ISR by using the highest
  // priority and re-arming after.
  ctrl_timer.end();
  dac_selftest();
  ctrl_timer.begin(control_tick, (uint32_t)(1000000 / CONTROL_RATE_HZ));
  ctrl_timer.priority(16);
  Serial.println("pnc: self-test done, engine resumes");
#endif
}

static uint32_t last_beat_ms = 0;
static uint32_t last_cvlog_ms = 0;

void loop() {
#if PNC_LOG_CV
  // Periodic ADC-vs-output telemetry for CV bring-up: if an output is patched
  // into CV1, cv1 should track out1 as the engine runs.
  uint32_t cm = millis();
  if (cm - last_cvlog_ms > 250) {
    last_cvlog_ms = cm;
    Serial.printf("cv: ADC1=%4u ADC2=%4u | out(mV) %4u %4u %4u %4u | run=%d\n",
                  hal_input_cv1_raw(), hal_input_cv2_raw(),
                  channels[0].out_mv, channels[1].out_mv,
                  channels[2].out_mv, channels[3].out_mv,
                  masterClock.running);
  }
#endif
  uint32_t beat = millis();
  if (beat - last_beat_ms > 5000) {
    last_beat_ms = beat;
    Serial.printf("pnc: alive ticks=%u ", tick_count);
    if (masterClock.source) {
      uint16_t mbpm = masterClock.measured_bpm_cent();
      if (mbpm)
        Serial.printf("bpm=%d.%02d EXT", mbpm / 100, mbpm % 100);
      else
        Serial.printf("bpm=--.-- EXT");
      Serial.printf(" ext us=%u ppqn=%u", masterClock.measured_period_us(),
                    masterClock.ext_ppqn);
    } else {
      Serial.printf("bpm=%d.%02d INT", masterClock.bpm_cent / 100,
                    masterClock.bpm_cent % 100);
    }
    Serial.printf(" page=%d oled=%s\n", ui_current_page(),
                  ui_display_found() ? "ok" : "MISSING");
  }
  ui_run_pending();
  uint32_t now = millis();
  if (now - last_ui_ms > 33) {
    last_ui_ms = now;
    ui_draw();
    patch_autosave_tick(now);
  }
}
