#pragma once
#include "wuk_lsm.hh"
template <int threshold0, int maxbin>
int WuKLSM<threshold0, maxbin>::get(const int64_t &key, int64_t &value) const {
#if DEBUG
  std::fprintf(stderr, "Line %d\n", __LINE__);
#endif
  auto it = level0.lower_bound(key);
  if (it != level0.end() && it->first == key) {
    if (!it->second.second) {
      value = it->second.first;
    }
    return it->second.second;
  }
#if DEBUG
  std::fprintf(stderr, "Line %d\n", __LINE__);
#endif
  std::vector<std::pair<int64_t, std::pair<int64_t, int>>> tmp;
  for (int n = 0; n < level.size(); ++n)
    for (int i = level[n].size() - 1; i >= 0; --i) {
#if DEBUG
      std::fprintf(stderr, "Line %d\n", __LINE__);
#endif
      int it = level[n][i];
      char filename[99];
      sprintf(filename, "level-%d.bin", it);
      tmp.resize(tmp.size() + file_size[it]);
      FILE *f = std::fopen(filename, "rb");
      std::fread(tmp.data(),
                 sizeof(std::pair<int64_t, std::pair<int64_t, int>>),
                 file_size[it], f);
      std::fclose(f);
#if DEBUG
      std::fprintf(stderr, "Line %d: ", __LINE__);
      for (auto it : tmp) {
        std::fprintf(stderr, "(%lld,%lld,%d) ", it.first, it.second.first,
                     it.second.second);
      }
      std::fprintf(stderr, "\n");
#endif
      auto jt =
          std::lower_bound(tmp.begin(), tmp.end(),
                           std::pair<int64_t, std::pair<int64_t, int>>(
                               key, std::pair<int64_t, int>(-(1LL << 60), 0)));

      if (jt != tmp.end() && jt->first == key) {
        if (!jt->second.second)
          value = jt->second.first;
        return jt->second.second;
      }
    }
  return 1;
}