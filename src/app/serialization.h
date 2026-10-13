#pragma once
// App: persisted patch format owner. StoreBlob layout, magic/version/CRC,
// slot geometry, and the live-state <-> blob mapping.
//
// Format-stable: the on-device layout must not change; any modification has
// to break the golden test in test/test_main.cpp first.
#include <Arduino.h>
#include "pam_config.h"
#include "app/channel.h"

struct MasterPersist {
  uint16_t bpm_cent;
  uint8_t source;
  uint8_t ext_ppqn;
  uint8_t running;
  uint8_t swing;
  uint8_t default_ratio;
  uint16_t user_scales[USER_SCALE_COUNT];
};

struct StoreBlob {
  uint32_t magic;
  uint32_t version;
  uint32_t crc;
  MasterPersist master;
  ChannelParams ch[NUM_OUTPUTS];
};

enum StoreSlot { SLOT_ACTIVE = 0, SLOT_BANK1 = 1, SLOT_BANK2 = 2 };

// EEPROM slot geometry (byte-identical to the original storage module).
static const uint32_t STORE_SLOT_STRIDE = 512;
inline uint32_t store_slot_offset(StoreSlot slot) {
  return (uint32_t)slot * STORE_SLOT_STRIDE;
}

void capture_state(StoreBlob &blob);
void restore_state(const StoreBlob &blob);
// In-memory verification: magic + version + CRC over the payload.
bool blob_verify(const StoreBlob &blob);
