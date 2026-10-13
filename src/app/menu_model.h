#pragma once
// App: channel/clock menu model — parameter IDs, per-mode group tables, and
// the get/set/label/format vocabulary the UI browses. Operates on the engine
// globals (channels[], masterClock) so it is unit-testable on the host;
// performs no hardware I/O. Moved verbatim from infra/ui.cpp (Phase 4),
// except get/set/label/format now take an explicit MenuPage instead of
// reading the controller's page state.
#include <Arduino.h>
#include <stddef.h>
#include "pam_config.h"
#include "app/channel.h"
#include "app/clock.h"
#include "app/patch.h"

#define P_RATIO 0
#define P_MODE 1
#define P_WAVE 2
#define P_WIDTH 3
#define P_SKEW 4
#define P_CVMIN 5
#define P_CVMAX 6
#define P_INV 7
#define P_PHASE 8
#define P_DELAY 9
#define P_PROB 10
#define P_EUEN 11
#define P_EULEN 12
#define P_EUFILL 13
#define P_EUROT 14
#define P_SLEN 15
#define P_STEP0 16
#define P_CVS1 32
#define P_CVSC1 33
#define P_CVS2 34
#define P_CVSC2 35
#define P_SLEW 36
#define P_SCALE 37
#define P_ENV_A 38
#define P_ENV_D 39
#define P_LGOP 40
#define P_LGSRC 41
#define P_IN1S 42
#define P_IN2S 43
#define P_DIVA 44
#define P_DIVB 45
#define P_QSRC 46
#define P_XOP 47
#define P_XSRC 48
#define P_FLEX 49
#define P_FLEXA 50
#define P_RCH 51
#define P_EULOOP 52
#define P_SPROB0 53   // +16 per-step probabilities
#define P_GQ 69
#define P_SMOOTH 70
#define P_BURST 71
#define CH_PARAM_COUNT 72

#define MASTER_PARAM_COUNT 5

// Display vocabularies (const data, owned here).
extern const char *MODE_NAMES[];
extern const char *WAVE_NAMES[];
extern const char *CVP_NAMES[];
extern const char *SCALE_NAMES[];
extern const char *LOGIC_NAMES[];
extern const char *XOP_NAMES[];
extern const char *FLEX_NAMES[];
extern const char *SRC_NAMES[];
extern const char *SYS_ACTIONS[];
#define SRC_COUNT 2
uint8_t sys_action_count();

// App model. Each channel's "app" is its mode. To avoid an endless flat list,
// an app's parameters are split into labelled GROUPS, browsed one after another
// (the group name shows in the page header):
//   shape  - params unique to this app (wave, width, env, logic, steps...)
//   clock  - ratio, phase, prob
//   euclid - euclid en/len/fill/rot   (only for gate-based apps)
//   range  - cv min, cv max, invert, lag
//   mod    - the two CV-in modulation chains
// The browse cursor is a single flat index over the app's concatenated groups.
// Indices are raw P_* values; 0xFF terminates each array.
enum { G_SHAPE = 0, G_CLOCK, G_EUCLID, G_FLEX, G_XOP, G_RANGE, G_MOD, G_END = 0xFF };

const uint8_t *mode_groups(uint8_t mode);
const uint8_t *shape_arr(uint8_t mode);
uint8_t arr_len(const uint8_t *a);
// Number of items in one group. Steps mode expands P_STEP0 into step_len slots.
uint8_t group_count(uint8_t code, uint8_t mode, uint8_t slen);
const char *group_name(uint8_t code, uint8_t mode);
// Resolve position `local` within group `code` to a raw P_* index.
uint8_t group_raw(uint8_t code, uint8_t local, uint8_t mode, uint8_t slen);
// Total browsable params for a mode (sum of its groups).
uint8_t chan_param_count(uint8_t mode, uint8_t slen);
// Flat index -> raw P_* index.
uint8_t app_param_at(uint8_t mode, uint8_t pos, uint8_t slen);
// Flat index -> its group's display name (for the header).
const char *app_param_group(uint8_t mode, uint8_t pos, uint8_t slen);

enum MenuPage : uint8_t { MENU_MASTER, MENU_CHANNEL };

int32_t menu_get_val(uint8_t ch, uint8_t idx, MenuPage pg);
void menu_set_val(uint8_t ch, uint8_t idx, int32_t v, MenuPage pg);
const char *menu_label_of(uint8_t ch, uint8_t idx, MenuPage pg);
void menu_fmt_val(uint8_t ch, uint8_t idx, MenuPage pg, char *buf, size_t n);
