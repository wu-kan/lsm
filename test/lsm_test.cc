#include "wuk_lsm.hh"
#include <cassert>
#include <cstdio>
int main(int argc, char **argv) {
  WuKLSM<> lsm;
  assert(argc >= 2);
  int n = std::atoi(argv[1]);
  for (int i = 0; i < n; ++i)
    lsm.insert(i, i);
  for (int i = 0; i < n; i += 2)
    lsm.del(i);
  for (int i = 1; i < n; i += 2)
    lsm.update(i, i + 1);
  for (int i = 0; i < n; ++i) {
#if DEBUG
    std::fprintf(stderr, "Line %d: %d\n", __LINE__, i);
#endif
    int64_t value;
    assert(lsm.get(i, value) != i % 2);
    assert(i % 2 == 0 || i == value + 1);
  }
}