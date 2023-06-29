#pragma once
#include "wuk_lsm.hh"
template <int threshold0, int maxbin>
void WuKLSM<threshold0, maxbin>::insert(const int64_t &key,
                                        const int64_t &value) {
  update(key, value);
}