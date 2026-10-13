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
//   active low); CV1/CV2 = A5/A4 on the T4 (see below), unipolar 0-10V, inverted.
// ---------------------------------------------------------------------------

#define IS_O_C 1

#if NUM_OUTPUTS > 4
#error "O_C hardware has a quad DAC: NUM_OUTPUTS must be 1..4"
#endif
#define DAC_MAX_MV        10000
#define DAC_FULL_SCALE_CV 10000
#define CV_BIPOLAR        0
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

// Teensy 4.0 in O_C hardware: the jack-to-ADC mapping is NOT the LC's A0..A3.
// Phazerville OC_ADC.cpp (Init_Teensy4_builtin_ADC): the CV jacks land on
// A5(CV1) A4(CV2) A6(CV3) A3(CV4). CV1 is A5 here, NOT A3 (that is CV4!).
#define PIN_CV_IN1 A5     // CV1
#define PIN_CV_IN2 A4     // CV2

// Measured on this board with the self-test (OUT1 patched into CV1):
//   jack 0V -> ADC raw ~4095, 5V -> ~2220, 10V -> ~170.
// The CV input stage inverts and is UNIPOLAR 0..10V (not the O_C bipolar
// input): firmware flips the raw ADC (4095 - raw) in main.cpp and CV_BIPOLAR=0
// makes modulation read 0..255 = 0..10V.
#define PNC_CV_INVERT 1

#define DISPLAY_KIND "OC-SPI SH1106"

// Boot splash: play the "Pam's Newer Crime" chase animation (~3 s) on the OLED
// at power-on, before the control timer starts. Build with -DPNC_SPLASH=0 to skip.
#ifndef PNC_SPLASH
#define PNC_SPLASH 1
#endif

// Idle screensaver: after this many ms with no encoder/button activity the
// OLED shows an abstract wave-interference animation; any input wakes it.
// Set to 0 to disable.
#ifndef PNC_SA_TIMEOUT_MS
#define PNC_SA_TIMEOUT_MS 60000
#endif

// Opt-in CV output bring-up test: build with -DPNC_DAC_SELFTEST=1 to sweep each
// output 0->5->10->0 V at boot (before the engine takes the bus) and log the
// exact SPI bytes. Off by default; normal firmware is unchanged.
#ifndef PNC_DAC_SELFTEST
#define PNC_DAC_SELFTEST 0
#endif

// Opt-in runtime CV telemetry: build with -DPNC_LOG_CV=1 to print the CV-input
// ADC values alongside the four commanded output voltages every 250 ms. Patch
// an output into CV1 and watch cv1 track out1 to prove the DAC path is live.
#ifndef PNC_LOG_CV
#define PNC_LOG_CV 0
#endif
