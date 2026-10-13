#include "app/patch.h"
#include "app/clock.h"
#include "app/storage_port.h"

static bool dirty = false;
bool state_dirty() { return dirty; }
void mark_dirty() { dirty = true; }

void patch_save(StoreSlot slot) {
  StoreBlob blob;
  capture_state(blob);
  storage_port_write(store_slot_offset(slot), &blob, sizeof(blob));
}

bool patch_load(StoreSlot slot) {
  StoreBlob blob;
  storage_port_read(store_slot_offset(slot), &blob, sizeof(blob));
  if (!blob_verify(blob)) return false;
  restore_state(blob);
  return true;
}

void patch_init() {
  channels_init(DEFAULT_RATIO);
  patch_load(SLOT_ACTIVE);   // ignored: empty/corrupt slot keeps the defaults
}

void patch_defaults() {
  masterClock.bpm_cent = 12000;
  masterClock.source = 0;
  masterClock.ext_ppqn = 24;
  masterClock.running = true;
  channels_init(DEFAULT_RATIO);
}

void patch_autosave_tick(uint32_t now_ms) {
  static uint32_t last_save_ms = 0;
  if (state_dirty() && now_ms - last_save_ms > 3000) {
    last_save_ms = now_ms;
    patch_save(SLOT_ACTIVE);
  }
}
