#pragma once
#include "wuk_lsm.hh"
template <int threshold0, int maxbin>
void WuKLSM<threshold0, maxbin>::update(const int64_t &key,
                                        const int64_t &value) {
  level0.emplace(key, std::pair<int64_t, int>{value, 0});
  if (level0.size() > threshold0) {
    down(0);
    level0.clear();
  }
}