#include "infra/hal_input.h"

const int8_t PhzEncoder::QD[16] = { 0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0 };

PhzEncoder enc_l, enc_r;
PhzButton btn_enc_sw, btn_b2, btn_enc_r_sw, btn_b1;

static uint16_t cv1_f = 2048, cv2_f = 2048;

void hal_input_init() {
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_R_A, INPUT_PULLUP);
  pinMode(PIN_ENC_R_B, INPUT_PULLUP);
  enc_l.init(PIN_ENC_A, PIN_ENC_B);
  enc_r.init(PIN_ENC_R_A, PIN_ENC_R_B);

  btn_enc_sw.init(PIN_ENC_SW);
  btn_b2.init(PIN_BTN_B2);
  btn_enc_r_sw.init(PIN_ENC_R_SW);
  btn_b1.init(PIN_BTN_B1);
}

void hal_input_poll() {
  enc_l.poll(PIN_ENC_A, PIN_ENC_B);
  enc_r.poll(PIN_ENC_R_A, PIN_ENC_R_B);
  btn_enc_sw.poll(PIN_ENC_SW);
  btn_enc_r_sw.poll(PIN_ENC_R_SW);
  btn_b2.poll(PIN_BTN_B2);
  btn_b1.poll(PIN_BTN_B1);
}

void hal_input_sample_cv() {
  uint16_t r1 = analogRead(PIN_CV_IN1);
  uint16_t r2 = analogRead(PIN_CV_IN2);
#if PNC_CV_INVERT
  // This board's CV input stage inverts (measured: 0V -> ~4095, 10V -> ~170).
  r1 = 4095 - r1;
  r2 = 4095 - r2;
#endif
  cv1_f = cv1_f - (cv1_f >> 2) + (r1 >> 2);
  cv2_f = cv2_f - (cv2_f >> 2) + (r2 >> 2);
}

uint8_t hal_input_cv1() { return cv1_f >> 4; }
uint8_t hal_input_cv2() { return cv2_f >> 4; }
uint16_t hal_input_cv1_raw() { return cv1_f; }
uint16_t hal_input_cv2_raw() { return cv2_f; }
