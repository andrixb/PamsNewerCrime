#pragma once
// App: storage transport port. Declared here, implemented by the infra
// adapter (infra/hal_storage.cpp, bound to EEPROM). The app depends only on
// this port — app -> infra includes stay at zero; infra depends on app.
#include <stddef.h>
#include <stdint.h>

void storage_port_read(uint32_t offset, void *dst, size_t n);
void storage_port_write(uint32_t offset, const void *src, size_t n);
