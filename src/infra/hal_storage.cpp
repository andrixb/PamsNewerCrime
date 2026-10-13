#include "app/storage_port.h"
#include <EEPROM.h>

// Infra adapter: the app-declared storage port, bound to EEPROM. No format,
// slot, or blob knowledge — the app layer (app/serialization.h for offsets
// and format, app/patch.cpp for use) owns everything above raw bytes.

void storage_port_read(uint32_t offset, void *dst, size_t n) {
  uint8_t *d = (uint8_t *)dst;
  for (size_t i = 0; i < n; i++) EEPROM.get(offset + i, d[i]);
}

void storage_port_write(uint32_t offset, const void *src, size_t n) {
  const uint8_t *s = (const uint8_t *)src;
  for (size_t i = 0; i < n; i++) EEPROM.put(offset + i, s[i]);
}
