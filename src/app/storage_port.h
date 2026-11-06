#pragma once
// App: storage transport port. Declared here, implemented by the infra
// adapter (infra/hal_storage.cpp, bound to EEPROM). The app depends only on
// this port — app -> infra includes stay at zero; infra depends on app.
#include <stddef.h>
#include <stdint.h>

void storage_port_read(uint32_t offset, void *dst, size_t n);
void storage_port_write(uint32_t offset, const void *src, size_t n);

// Engine critical section for the patch use cases. Load/save/defaults run in
// the main loop while the 1 kHz control ISR reads and writes the same engine
// state; capture/restore happen between these calls so the ISR never sees a
// half-copied patch. Infra masks interrupts: hold it only for in-memory
// copies, never around storage I/O. Not reentrant.
void engine_lock();
void engine_unlock();
