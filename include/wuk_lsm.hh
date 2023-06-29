#pragma once
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <map>
#include <vector>

template <int threshold0 = 1 << 10, int maxbin = 1 << 4> class WuKLSM {
public:
  void del(const int64_t &key);
  void update(const int64_t &key, const int64_t &value);
  void insert(const int64_t &key, const int64_t &value);
  int get(const int64_t &key, int64_t &value) const;
  WuKLSM() : time_step(0) {}

private:
  int time_step;
  std::vector<int> file_size;
  std::map<int64_t, std::pair<int64_t, int>> level0;
  std::vector<std::vector<int>> level;
  void down(int n);
};

#include "wuk_lsm/del.hh"
#include "wuk_lsm/down.hh"
#include "wuk_lsm/get.hh"
#include "wuk_lsm/insert.hh"
#include "wuk_lsm/update.hh"