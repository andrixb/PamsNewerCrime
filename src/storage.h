#pragma once
#include <Arduino.h>
#include "pam_config.h"
#include "channel.h"

struct MasterPersist {
  uint16_t bpm_cent;
  uint8_t source;
  uint8_t ext_ppqn;
  uint8_t running;
  uint8_t reserved[3];
};

struct StoreBlob {
  uint32_t magic;
  uint32_t version;
  uint32_t crc;
  MasterPersist master;
  ChannelParams ch[NUM_OUTPUTS];
};

enum StoreSlot { SLOT_ACTIVE = 0, SLOT_BANK1 = 1, SLOT_BANK2 = 2 };

void storage_init();
bool storage_load(StoreSlot slot, StoreBlob &blob);
void storage_save(StoreSlot slot, const StoreBlob &blob);
void capture_state(StoreBlob &blob);
void restore_state(const StoreBlob &blob);
bool state_dirty();
void mark_dirty();
uint8_t oled_mode_load();      // 0 = SSD1306, 2 = SH1106, 0xFF = unknown
void oled_mode_save(uint8_t mode);
