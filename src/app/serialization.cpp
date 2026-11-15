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

// A blob can pass CRC yet hold out-of-range fields (e.g. written by another
// build with the same STORE_VERSION). Clamp every field the engine indexes or
// divides by into the ranges menu_set_val() allows, so a load can never read
// past RATIO_TABLE & co. Values channels_init() itself produces stay legal,
// so valid patches round-trip byte-identically.
static void sanitize_params(ChannelParams &p) {
  if (p.ratio >= RATIO_COUNT) p.ratio = RATIO_COUNT - 1;
  if (p.mode >= MODE_LAST) p.mode = MODE_OFF;
  if (p.wave >= LFO_LAST) p.wave = LFO_SINE;
  p.cv_min = constrain(p.cv_min, 0, DAC_MAX_MV);
  p.cv_max = constrain(p.cv_max, 0, DAC_MAX_MV);
  if (p.cv_max < p.cv_min) p.cv_max = p.cv_min;
  if (p.prob > 100) p.prob = 100;
  p.invert = p.invert ? 1 : 0;
  p.eucl_en = p.eucl_en ? 1 : 0;
  p.eucl_len = constrain(p.eucl_len, 1, EUCLID_MAX_LEN);
  if (p.eucl_fill > p.eucl_len) p.eucl_fill = p.eucl_len;
  if (p.eucl_rot >= p.eucl_len) p.eucl_rot = p.eucl_len - 1;
  if (p.eucl_loop > EUCLID_MAX_LEN) p.eucl_loop = EUCLID_MAX_LEN;
  p.step_len = constrain(p.step_len, 1, MAX_STEPS);
  for (uint8_t s = 0; s < MAX_STEPS; s++)
    if (p.step_prob[s] > 100) p.step_prob[s] = 100;
  if (p.cv_src1 >= CVP_LAST) p.cv_src1 = CVP_NONE;
  if (p.cv_src2 >= CVP_LAST) p.cv_src2 = CVP_NONE;
  if (p.cv_in1 > 1 + NUM_OUTPUTS) p.cv_in1 = 0;
  if (p.cv_in2 > 1 + NUM_OUTPUTS) p.cv_in2 = 0;
  if (p.scale >= SCALE_COUNT) p.scale = 0;
  if (p.logic_op >= LOGIC_LAST) p.logic_op = LOGIC_OFF;
  if (p.logic_src > NUM_OUTPUTS) p.logic_src = 0;
  p.div_a = constrain(p.div_a, -32, 32);
  p.div_b = constrain(p.div_b, -32, 32);
  if (p.quant_src > 1 + NUM_OUTPUTS) p.quant_src = 0;
  if (p.ratchets > 16) p.ratchets = 16;          // 0 is legal (= 1)
  if (p.xop >= XOP_LAST) p.xop = XOP_NONE;
  if (p.xsrc > 1 + NUM_OUTPUTS) p.xsrc = 0;
  if (p.flex >= FLEX_LAST) p.flex = FLEX_NONE;
  p.gquant = p.gquant ? 1 : 0;
}

void restore_state(const StoreBlob &blob) {
  masterClock.bpm_cent = constrain(blob.master.bpm_cent, BPM_MIN, BPM_MAX);
  masterClock.source = blob.master.source ? 1 : 0;
  masterClock.ext_ppqn = constrain(blob.master.ext_ppqn, 1, 48);
  masterClock.running = blob.master.running;
  masterClock.swing = constrain((int)blob.master.swing, 0, 63);
  if (blob.master.default_ratio < RATIO_COUNT) DEFAULT_RATIO = blob.master.default_ratio;
  for (uint8_t u = 0; u < USER_SCALE_COUNT; u++) USER_SCALES[u] = blob.master.user_scales[u];
  for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
    channels[i].p = blob.ch[i];
    sanitize_params(channels[i].p);
    channel_rebuild_euclid(channels[i]);
  }
  masterClock.reset_phase();
  channels_reset();
}

bool blob_verify(const StoreBlob &blob) {
  if (blob.magic != STORE_MAGIC || blob.version != STORE_VERSION) return false;
  return crc32((const uint8_t *)&blob.master, PAYLOAD_LEN) == blob.crc;
}
