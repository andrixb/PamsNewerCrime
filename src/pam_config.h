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
#define DAC_FULL_SCALE_CV 4096
#define DAC_MAX_MV        4096
#define MAX_STEPS         16
#define EUCLID_MAX_LEN    32
#define BPM_MIN           1000
#define BPM_MAX           30000
#define NUM_BANKS         2

#define PIN_DAC_CS0 10
#define PIN_DAC_CS1 9
#define PIN_DAC_CS2 8
#define PIN_DAC_CS3 7

#define PIN_ENC_A 4
#define PIN_ENC_B 5
#define PIN_ENC_SW 6
#define PIN_BTN_B2 2
#define PIN_CK_IN 3

#define PIN_CV_IN1 A0
#define PIN_CV_IN2 A1

#define DISPLAY_ADDRESS 0x3C
#define DISPLAY_RESET   -1
