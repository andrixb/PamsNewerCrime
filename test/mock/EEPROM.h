#pragma once
// EEPROM mock: a RAM block with the same templated get/put surface as the
// Teensy EEPROM library. Tests can poke bytes to exercise CRC/version rejection.
#include <stdint.h>
#include <string.h>

struct EepromMock {
  static const uint32_t SIZE = 4096;
  uint8_t data[SIZE];
  EepromMock() { memset(data, 0xFF, SIZE); }
  template <typename T> void get(uint32_t addr, T &v) const {
    memcpy(&v, data + addr, sizeof(T));
  }
  template <typename T> void put(uint32_t addr, const T &v) {
    memcpy(data + addr, &v, sizeof(T));
  }
};

extern EepromMock EEPROM;
