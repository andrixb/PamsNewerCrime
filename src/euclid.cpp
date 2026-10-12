#include "euclid.h"
#include <vector>
#include <string>

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
  int n1 = ones, n2 = total - ones;
  std::string s1 = "1", s2 = "0";
  while (n1 != 0 && n2 != 0) {
    if (n1 >= n2) {
      s1 += s2;
      n1 -= n2;
    } else {
      s2 += s1;
      n2 -= n1;
    }
  }
  std::string r = s1 + s2;
  for (int i = 0; i < total && i < (int)r.size(); i++)
    out[i] = r[i] == '1' ? 1 : 0;
}
