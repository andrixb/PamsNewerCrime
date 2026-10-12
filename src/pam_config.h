#pragma once
#include <Arduino.h>

#ifndef NUM_OUTPUTS
#define NUM_OUTPUTS 4
#endif
#if NUM_OUTPUTS < 1 || NUM_OUTPUTS > 8
#error "NUM_OUTPUTS must be 1..8"
#endif

#define TICKS_PER_QUARTER 96
#define CONTROL_RATE_HZ   1000
#define MAX_STEPS         16
#define EUCLID_MAX_LEN    32
#define BPM_MIN           1000
#define BPM_MAX           33000
#define NUM_BANKS         2

// ---------------------------------------------------------------------------
// Hardware profile: genuine O_C board (14HP original / uO_c 8HP micro),
// Teensy 4.0 / 4.1 drop-in.
//   SH1106 OLED on SPI (CS=8 DC=6 RST=7); DAC8565 quad on the same SPI
//   (CS=10 RST=9), 0-10 V outputs; left encoder A=22 B=21 push=23, right encoder
//   A=16 B=15 push=14, top button = 5, bottom button = 4; clock = TR1 (pin 0,
//   active low); CV1/CV2 = A3/A4, bipolar. Pin map verified against Phazerville.
// ---------------------------------------------------------------------------

#define IS_O_C 1

#if NUM_OUTPUTS > 4
#error "O_C hardware has a quad DAC: NUM_OUTPUTS must be 1..4"
#endif
#define DAC_MAX_MV        10000
#define DAC_FULL_SCALE_CV 10000
#define CV_BIPOLAR        1
#define TR_ACTIVE_LOW     1

#define PIN_DAC_CS        10
#define PIN_DAC_RST       9
#define PIN_OLED_CS       8
#define PIN_OLED_DC       6
#define PIN_OLED_RST      7

#define PIN_ENC_A 22
#define PIN_ENC_B 21
#define PIN_ENC_SW 23
#define PIN_ENC_R_A 16
#define PIN_ENC_R_B 15
#define PIN_ENC_R_SW 14
#define PIN_BTN_B2 5      // O_C top button
#define PIN_BTN_B1 4      // O_C bottom button
#define PIN_CK_IN 0       // TR1

#define PIN_CV_IN1 A3     // CV1
#define PIN_CV_IN2 A4     // CV2

#define DISPLAY_KIND "OC-SPI SH1106"
