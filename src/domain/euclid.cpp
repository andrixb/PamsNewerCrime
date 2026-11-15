#include "domain/euclid.h"

// Bjorklund in bucket form: `ones` buckets of "1" are consumed pairwise into
// `total-ones` buckets of "0" each round until at most one remainder bucket is
// left; merged + remaining buckets, concatenated, give the most-even pattern
// (max pulse gap - min pulse gap <= 1, exactly `ones` pulses in `total` slots).
//
// Allocation-free: this runs from the 1 kHz control ISR (menu edits rebuild the
// euclid mask), so no heap (ADR-0007). Each bucket is a bit string packed into
// a uint64_t (bit k = slot k) plus its length; patterns are capped at
// EUCLID_ALGO_MAX slots, any slots past that are written as rests.
static const int EUCLID_ALGO_MAX = 64;

struct Bucket {
  uint64_t bits;
  uint8_t len;
};

void euclid(int ones, int total, uint8_t *out) {
  if (total <= 0) return;
  if (ones <= 0) {
    for (int i = 0; i < total; i++) out[i] = 0;
    return;
  }
  if (ones >= total) {
    for (int i = 0; i < total; i++) out[i] = 1;
    return;
  }
  const int n_slots = total < EUCLID_ALGO_MAX ? total : EUCLID_ALGO_MAX;
  if (ones > n_slots) ones = n_slots;

  Bucket a[EUCLID_ALGO_MAX], b[EUCLID_ALGO_MAX], merged[EUCLID_ALGO_MAX];
  int na = ones, nb = n_slots - ones;
  for (int i = 0; i < na; i++) a[i] = { 1u, 1 };
  for (int i = 0; i < nb; i++) b[i] = { 0u, 1 };

  while (nb > 1) {
    const int n = na < nb ? na : nb;
    for (int i = 0; i < n; i++)
      merged[i] = { a[i].bits | (b[i].bits << a[i].len),
                    (uint8_t)(a[i].len + b[i].len) };
    // Leftover buckets (from whichever side was longer) become the new b.
    if (na > n) {
      for (int i = n; i < na; i++) b[i - n] = a[i];
      nb = na - n;
    } else {
      for (int i = n; i < nb; i++) b[i - n] = b[i];
      nb = nb - n;
    }
    for (int i = 0; i < n; i++) a[i] = merged[i];
    na = n;
  }

  int pos = 0;
  for (int i = 0; i < na; i++)
    for (int k = 0; k < a[i].len && pos < total; k++)
      out[pos++] = (uint8_t)((a[i].bits >> k) & 1u);
  for (int i = 0; i < nb; i++)
    for (int k = 0; k < b[i].len && pos < total; k++)
      out[pos++] = (uint8_t)((b[i].bits >> k) & 1u);
  while (pos < total) out[pos++] = 0;
}
