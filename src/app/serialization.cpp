#include "app/serialization.h"
#include "app/clock.h"
#include <string.h>

#define STORE_MAGIC   0x504B5731u
#define STORE_VERSION 10u

static_assert(sizeof(StoreBlob) <= STORE_SLOT_STRIDE, "blob too big");

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

void capture_state(StoreBlob &blob) {
  memset(&blob, 0, sizeof(blob));
  blob.magic = STORE_MAGIC;
  blob.version = STORE_VERSION;
  blob.master.bpm_cent = masterClock.bpm_cent;
  blob.master.source = masterClock.source;
  blob.master.ext_ppqn = masterClock.ext_ppqn;
  blob.master.running = masterClock.running ? 1 : 0;
  blob.master.swing = masterClock.swing;
  blob.master.default_ratio = DEFAULT_RATIO;
  for (uint8_t u = 0; u < USER_SCALE_COUNT; u++) blob.master.user_scales[u] = USER_SCALES[u];
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) blob.ch[i] = channels[i].p;
  blob.crc = crc32((const uint8_t *)&blob.master, PAYLOAD_LEN);
}

void restore_state(const StoreBlob &blob) {
  masterClock.bpm_cent = blob.master.bpm_cent;
  masterClock.source = blob.master.source;
  masterClock.ext_ppqn = blob.master.ext_ppqn;
  masterClock.running = blob.master.running;
  masterClock.swing = constrain((int)blob.master.swing, 0, 63);
  if (blob.master.default_ratio < RATIO_COUNT) DEFAULT_RATIO = blob.master.default_ratio;
  for (uint8_t u = 0; u < USER_SCALE_COUNT; u++) USER_SCALES[u] = blob.master.user_scales[u];
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
    channels[i].p = blob.ch[i];
    channel_rebuild_euclid(channels[i]);
  }
  masterClock.reset_phase();
  channels_reset();
}

bool blob_verify(const StoreBlob &blob) {
  if (blob.magic != STORE_MAGIC || blob.version != STORE_VERSION) return false;
  return crc32((const uint8_t *)&blob.master, PAYLOAD_LEN) == blob.crc;
}
