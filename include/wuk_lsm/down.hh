#pragma once
#include "wuk_lsm.hh"
template <int threshold0, int maxbin>
void WuKLSM<threshold0, maxbin>::down(int n) {
  if (n + 1 > level.size()) {
    level.resize(n + 1);
  }
  if (n > 0 && level[n].size() < maxbin)
    return;
  std::vector<std::pair<int64_t, std::pair<int64_t, int>>> res;
#if DEBUG
  std::fprintf(stderr, "Line %d\n", __LINE__);
#endif
  if (n == 0) {
    res.assign(level0.begin(), level0.end());
  } else {
    std::vector<std::vector<std::pair<int64_t, std::pair<int64_t, int>>>> tmp;
    for (auto it : level[n - 1]) {
      char filename[99];
      sprintf(filename, "level-%d.bin", it);
      FILE *f = std::fopen(filename, "rb");
      tmp.emplace_back(file_size[it]);
      std::fread(tmp.back().data(),
                 sizeof(std::pair<int64_t, std::pair<int64_t, int>>),
                 file_size[it], f);
      std::fclose(f);
    }
    level[n - 1].clear();
    for (;;) {
      int u = tmp.size() - 1;
      while (u >= 0 && tmp[u].empty())
        --u;
      if (u < 0)
        break;
      for (int i = u - 1; i >= 0; --i) {
        if (tmp[i].empty())
          continue;
        if (tmp[u].back().first < tmp[i].back().first)
          u = i;
      }
      if (res.empty() || res.back().first > tmp[u].back().first)
        res.push_back(tmp[u].back());
      tmp[u].pop_back();
    }
    std::reverse(res.begin(), res.end());
  }
  file_size.push_back(res.size());
  level[n].push_back(time_step++);
  char filename[99];
  std::sprintf(filename, "level-%d.bin", level[n].back());
  FILE *f = std::fopen(filename, "wb");
  std::fwrite(res.data(), sizeof(std::pair<int64_t, std::pair<int64_t, int>>),
              res.size(), f);
  std::fclose(f);
  down(n + 1);
}