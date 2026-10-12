#include "storage.h"
#include "clock.h"
#include <EEPROM.h>
#include <string.h>

#define STORE_MAGIC   0x504B5731u
#define STORE_VERSION 1u

static const uint32_t SLOT_STRIDE = 512;
static const uint32_t SLOT_OFFSET[3] = { 0, SLOT_STRIDE, 2 * SLOT_STRIDE };
static_assert(sizeof(StoreBlob) <= SLOT_STRIDE, "blob too big");

static uint32_t crc32(const uint8_t *data, size_t len) {
  uint32_t crc = 0xFFFFFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int k = 0; k < 8; k++)
      crc = (crc >> 1) ^ (0xEDB88320 & (-(int32_t)(crc & 1)));
  }
  return ~crc;
}

static const size_t PAYLOAD_LEN = sizeof(MasterPersist) + sizeof(ChannelParams) * NUM_OUTPUTS;

static bool dirty = false;
bool state_dirty() { return dirty; }
void mark_dirty() { dirty = true; }

void capture_state(StoreBlob &blob) {
  memset(&blob, 0, sizeof(blob));
  blob.magic = STORE_MAGIC;
  blob.version = STORE_VERSION;
  blob.master.bpm_cent = clock.bpm_cent;
  blob.master.source = clock.source;
  blob.master.ext_ppqn = clock.ext_ppqn;
  blob.master.running = clock.running ? 1 : 0;
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) blob.ch[i] = channels[i].p;
  blob.crc = crc32((const uint8_t *)&blob.master, PAYLOAD_LEN);
}

void restore_state(const StoreBlob &blob) {
  clock.bpm_cent = blob.master.bpm_cent;
  clock.source = blob.master.source;
  clock.ext_ppqn = blob.master.ext_ppqn;
  clock.running = blob.master.running;
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
    channels[i].p = blob.ch[i];
    channel_rebuild_euclid(channels[i]);
  }
  channels_reset();
}

bool storage_load(StoreSlot slot, StoreBlob &blob) {
  EEPROM.get(SLOT_OFFSET[slot], blob);
  if (blob.magic != STORE_MAGIC || blob.version != STORE_VERSION) return false;
  return crc32((const uint8_t *)&blob.master, PAYLOAD_LEN) == blob.crc;
}

void storage_save(StoreSlot slot, const StoreBlob &blob) {
  EEPROM.put(SLOT_OFFSET[slot], blob);
}

void storage_init() {
  StoreBlob blob;
  channels_init();
  if (storage_load(SLOT_ACTIVE, blob)) restore_state(blob);
}
