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
#define BPM_MAX           30000
#define NUM_BANKS         2

// ---------------------------------------------------------------------------
// Hardware profiles.
//   DIY (default)  : breadboard design from the README - MCP4822 DACs on
//                    software CS 7..10, SSD1306/SH1106 on I2C (18/19).
//   HARDWARE_O_C   : genuine O_C board (14HP original / uO_c 8HP micro),
//                    Teensy 4.0. DAC8565 (quad, 0-10V, inverted output stage)
//                    on SPI CS=10 RST=9; SH1106 OLED on the same SPI bus with
//                    CS=8 DC=6 RST=7; left encoder A=22 B=21 push=23;
//                    buttons 4/5; triggers TR1..4 = 0..3; CV1..4 = A3..A6
//                    bipolar (0V = midscale).
// Select with -DHARDWARE_O_C (see platformio env teensy40_oc).
// ---------------------------------------------------------------------------

#if defined(HARDWARE_O_C) && HARDWARE_O_C == 1

#if NUM_OUTPUTS > 4
#error "O_C hardware has a quad DAC: NUM_OUTPUTS must be 1..4"
#endif
#define DAC_MAX_MV        10000
#define DAC_FULL_SCALE_CV 10000
#define CV_BIPOLAR        1
#define TR_ACTIVE_LOW     1
#define HAS_MIDI_IN       0

#define PIN_DAC_CS        10
#define PIN_DAC_RST       9
#define PIN_OLED_CS       8
#define PIN_OLED_DC       6
#define PIN_OLED_RST      7

#define PIN_ENC_A 22
#define PIN_ENC_B 21
#define PIN_ENC_SW 23
#define PIN_BTN_B2 5      // O_C top button (pin 4 = bottom, unused)
#define PIN_CK_IN 0       // TR1

#define PIN_CV_IN1 A3     // CV1
#define PIN_CV_IN2 A4     // CV2

#define DISPLAY_KIND "OC-SPI SH1106"

#else // ---- DIY profile ----

#define DAC_MAX_MV        4096
#define DAC_FULL_SCALE_CV 4096
#define CV_BIPOLAR        0
#define TR_ACTIVE_LOW     0
#define HAS_MIDI_IN       1

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
#define PIN_I2C_SDA 18
#define PIN_I2C_SCL 19
// 1 = panel ACKs on I2C but stays blank with the SSD1306 driver: SH1106 module
#ifndef DISPLAY_SH1106
#define DISPLAY_SH1106 0
#endif
#define DISPLAY_KIND "I2C"

#endif

// MIDI clock input: Serial1 RX on pin 0 (Teensy 4.0/4.1), feed through a
// 6N138/6N139 optocoupler (or resistor divider) per the MIDI spec.
#define MIDI_SERIAL        Serial1
#define MIDI_CLOCK_PPQN    24
