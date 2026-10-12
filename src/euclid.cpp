#include "euclid.h"
#include <vector>
#include <string>

// Bjorklund in bucket form: `ones` buckets of "1" are consumed pairwise into
// `total-ones` buckets of "0" each round until at most one remainder bucket is
// left; merged + remaining buckets, concatenated, give the most-even pattern
// (max pulse gap - min pulse gap <= 1, exactly `ones` pulses in `total` slots).
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
  std::vector<std::string> a(ones, "1");
  std::vector<std::string> b(total - ones, "0");
  while (b.size() > 1) {
    size_t n = a.size() < b.size() ? a.size() : b.size();
    std::vector<std::string> merged;
    merged.reserve(n);
    for (size_t i = 0; i < n; i++) merged.push_back(a[i] + b[i]);
    if (a.size() > n)
      b.assign(a.begin() + n, a.end());
    else
      b.assign(b.begin() + n, b.end());
    a.swap(merged);
  }
  std::string r;
  for (size_t i = 0; i < a.size(); i++) r += a[i];
  for (size_t i = 0; i < b.size(); i++) r += b[i];
  for (int i = 0; i < total; i++)
    out[i] = (i < (int)r.size() && r[i] == '1') ? 1 : 0;
}
