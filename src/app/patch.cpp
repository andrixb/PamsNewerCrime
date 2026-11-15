#include "app/patch.h"
#include "app/clock.h"
#include "app/storage_port.h"

// Set from the 1 kHz ISR (UI edits), cleared by the main-loop autosave.
// edit_seq counts edits so the autosave can tell when editing has paused.
static volatile bool dirty = false;
static volatile uint32_t edit_seq = 0;
bool state_dirty() { return dirty; }
void mark_dirty() { edit_seq++; dirty = true; }

// Storage I/O stays outside the engine lock; only the in-memory capture /
// restore is locked (see storage_port.h).
void patch_save(StoreSlot slot) {
  StoreBlob blob;
  engine_lock();
  capture_state(blob);
  engine_unlock();
  storage_port_write(store_slot_offset(slot), &blob, sizeof(blob));
}

bool patch_load(StoreSlot slot) {
  StoreBlob blob;
  storage_port_read(store_slot_offset(slot), &blob, sizeof(blob));
  if (!blob_verify(blob)) return false;
  engine_lock();
  restore_state(blob);
  engine_unlock();
  return true;
}

void patch_init() {
  engine_lock();
  channels_init(DEFAULT_RATIO);
  engine_unlock();
  patch_load(SLOT_ACTIVE);   // ignored: empty/corrupt slot keeps the defaults
}

void patch_defaults() {
  engine_lock();
  masterClock.bpm_cent = 12000;
  masterClock.source = 0;
  masterClock.ext_ppqn = 24;
  masterClock.running = true;
  channels_init(DEFAULT_RATIO);
  engine_unlock();
}

// Saves SLOT_ACTIVE once edits have paused for AUTOSAVE_QUIET_MS (the
// documented "~3 s of no further edits"). Turning a knob continuously costs
// one save after you stop, not one every few seconds (each save can stall
// interrupts on Teensy 4's flash-emulated EEPROM, and wears it).
static const uint32_t AUTOSAVE_QUIET_MS = 3000;

void patch_autosave_tick(uint32_t now_ms) {
  static uint32_t seen_seq = 0;
  static uint32_t last_edit_ms = 0;
  if (!state_dirty()) return;
  const uint32_t seq = edit_seq;
  if (seq != seen_seq) { seen_seq = seq; last_edit_ms = now_ms; return; }
  if (now_ms - last_edit_ms < AUTOSAVE_QUIET_MS) return;
  // Clear BEFORE capturing: an edit landing mid-save re-marks dirty and is
  // picked up by a later tick instead of being lost.
  dirty = false;
  patch_save(SLOT_ACTIVE);
}
