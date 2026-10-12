#pragma once
// Host-side Arduino shim for native unit tests: only the symbols the pure
// modules (euclid/clock/channel/storage) actually reference. Lives on the
// include path BEFORE any real Arduino headers, so production sources compile
// unmodified on the host.
#include <stdint.h>
#include <stddef.h>

#define LOW  0
#define HIGH 1
#define INPUT         0
#define INPUT_PULLUP  2
#define OUTPUT        1

#ifndef constrain
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#endif

inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t, uint8_t) {}
inline int  digitalRead(uint8_t) { return HIGH; }
inline void noInterrupts() {}
inline void interrupts() {}
inline void delay(uint32_t) {}
inline void delayMicroseconds(uint32_t) {}

// Controllable time source (tests advance mock_time_us between ticks).
extern uint32_t mock_time_us;
inline uint32_t micros() { return mock_time_us; }
inline uint32_t millis() { return mock_time_us / 1000u; }
