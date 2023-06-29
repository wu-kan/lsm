# 基于日志结构合并树的键值存储系统

## 快速开始

```bash
rm -rf Testing
cmake -G Ninja -B Testing . -DCMAKE_BUILD_TYPE=Release
cmake --build Testing
cmake --build Testing -t test
cat Testing/Testing/Temporary/LastTest.log
```

## 项目简介

本项目实现了一个简易的基于日志结构合并树的键值存储系统，以 C++Header-Only 形式编写，可直接通过 `#include "wuk_lsm.hh"` 引入使用。`test/` 目录下保存了项目的单元测试模块。

```bash
$ tree
.
├── CMakeLists.txt
├── include
│   ├── wuk_lsm
│   │   ├── del.hh
│   │   ├── down.hh
│   │   ├── get.hh
│   │   ├── insert.hh
│   │   └── update.hh
│   └── wuk_lsm.hh
├── LICENSE
├── README.md
└── test
    ├── CMakeLists.txt
    └── lsm_test.cc

3 directories, 11 files
```

### 接口设计

`wuk_lsm.hh` 中包含所需要的外部接口与内部实现。

```cpp
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
```

模板参数 `threshold0` 表示内存中的有序日志最大长度，此处设定默认值为 1024；`maxbin` 表示 LSM 树每一层最大宽度，此处设定默认值为 16，超过该值会自动执行一次合并操作。`del(key, value)`、`update(key, value)`、`insert(key, value)` 为三个键值修改接口，分别表示对一对键值的删除、更新、增加；`get(key, value)` 为查询操作，若查询成功返回 0，并将查询结果存储在 value 中，否则返回一个非零值。

常见的 LSM 实现多使用跳表维护内存中的有序数据。然而此处暂使用平衡树 `map` 代替，其与跳表均能达到理论最优的 O(logN) 的修改/查询复杂度，由于 `maxbin` 仅为 1024，性能常数差异非常小。`down(n)` 为内部接口，代表合并达到阈值的数据并写入硬盘中的第 `n` 层。为了尽可能减少调用操作系统 API（如硬件时间、硬件随机数）对存储系统的影响，此处选择了手动维护一个时间戳 `time_step` 用于日志记录，对应的写文件名保存在 level 中。同时，在写文件时还会保存写入大小 `file_size`，这样在读取日志时可减少一次文件大小查询，进一步提升顺序文件读取效率。

当然，一个潜在的问题是 `time_step` 超过 `INT_MAX` 之后会产生日志的覆盖；但实际上由计算可得 `time_step` 超过 `INT_MAX` 的时候产生的日志量已超过 40TB，而此时光 `file_size` 数组占用空间大小也已经达到 8GB！考虑到手头暂时没有这么大的存储服务器用于测试，因此没有针对该情况优化。

### 内部实现

#### insert & update

这两个操作对于 LSM 树来说是完全相同的，在内存中直接更新新的值，同时将墓碑标记设置为 0，同时触发一次 `down` 操作检查。

```cpp
template <int threshold0, int maxbin>
void WuKLSM<threshold0, maxbin>::update(const int64_t &key,
                                        const int64_t &value) {
  level0.emplace(key, std::pair<int64_t, int>{value, 0});
  if (level0.size() > threshold0) {
    down(0);
    level0.clear();
  }
}
template <int threshold0, int maxbin>
void WuKLSM<threshold0, maxbin>::insert(const int64_t &key,
                                        const int64_t &value) {
  update(key, value);
}
```

#### `del`

由于 LSM 树并不修改已经写入硬盘的数据，`del` 操作本质上和 `update` 操作也类似，只不过将墓碑标记设置为 1。

```cpp
template <int threshold0, int maxbin>
void WuKLSM<threshold0, maxbin>::del(const int64_t &key) {
  level0.emplace(key, std::pair<int64_t, int>{0, 1});
  if (level0.size() > threshold0) {
    down(0);
    level0.clear();
  }
}
```

#### get

首先在内存中查找，否则在硬盘中逐 level 自新向旧查找，每个日志使用 `fread` 顺序读一次进入内存。由于硬盘中的数据是有序的，此处同样使用 `lower_bound` 进行二分查找。

```cpp
template <int threshold0, int maxbin>
int WuKLSM<threshold0, maxbin>::get(const int64_t &key, int64_t &value) const {
  auto it = level0.lower_bound(key);
  if (it != level0.end() && it->first == key) {
    if (!it->second.second) {
      value = it->second.first;
    }
    return it->second.second;
  }
  std::vector<std::pair<int64_t, std::pair<int64_t, int>>> tmp;
  for (int n = 0; n < level.size(); ++n)
    for (int i = level[n].size() - 1; i >= 0; --i) {
      int it = level[n][i];
      char filename[99];
      sprintf(filename, "level-%d.bin", it);
      tmp.resize(tmp.size() + file_size[it]);
      FILE *f = std::fopen(filename, "rb");
      std::fread(tmp.data(),
                 sizeof(std::pair<int64_t, std::pair<int64_t, int>>),
                 file_size[it], f);
      std::fclose(f);
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
```

#### down

最后是核心的 down 操作。如果目标为内存中的有序数据，直接写入硬盘第 0 层；否则需要进行一次 n 路归并，归并时顺便删掉相同但较旧的数据。

```cpp
template <int threshold0, int maxbin>
void WuKLSM<threshold0, maxbin>::down(int n) {
  if (n + 1 > level.size()) {
    level.resize(n + 1);
  }
  if (n > 0 && level[n].size() < maxbin)
    return;
  std::vector<std::pair<int64_t, std::pair<int64_t, int>>> res;
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
```

## 性能 & 正确性测试

实验环境为自购腾讯云轻量服务器节点，2 核 2G 内存 30GB 存储，运行结果如下，进行百万级别的 KV 增删修改仅花费 0.68 秒，考虑到轻量应用服务器较为孱弱的性能，可以说取得较好效果。

```bash
Start testing: Jun 29 12:55 UTC
----------------------------------------------------------
1/2 Testing: test-1024
1/2 Test: test-1024
Command: "/workspace/lsm/Testing/test/lsm_test" "1024"
Directory: /workspace/lsm/Testing/test
"test-1024" start time: Jun 29 12:55 UTC
Output:
----------------------------------------------------------
<end of output>
Test time =   0.00 sec
----------------------------------------------------------
Test Passed.
"test-1024" end time: Jun 29 12:55 UTC
"test-1024" time elapsed: 00:00:00
----------------------------------------------------------

2/2 Testing: test-1048576
2/2 Test: test-1048576
Command: "/workspace/lsm/Testing/test/lsm_test" "1048576"
Directory: /workspace/lsm/Testing/test
"test-1048576" start time: Jun 29 12:55 UTC
Output:
----------------------------------------------------------
<end of output>
Test time =   0.68 sec
----------------------------------------------------------
Test Passed.
"test-1048576" end time: Jun 29 12:55 UTC
"test-1048576" time elapsed: 00:00:00
----------------------------------------------------------

End testing: Jun 29 12:55 UTC
```

## 总结 & 后续改进方案

在本次项目中，我实现了一个简单的基于日志结构合并树的键值存储系统。由于时间设备所限，还有如下想法可用于后续改进：

1. 增加压缩算法，使用 CPU 性能交换空间大小；
2. 此处使用了 C 语言封装的文件接口，实际上仍可能要经过多个缓冲区，可考虑调用更底层 API，甚至面向键值存储系统场景优化的文件系统；
3. 针对实际场景做针对性优化。例如，对于 del 操作较为频繁的场景，可使用布隆过滤器，尽量避免查询操作进入 worst case。
4. 针对多核甚至多节点做扩展性优化。例如，查找可由多线程同时进行，或是每个线程保存一个线程独有的子 LSM 树。
5. 移植到到异构计算设备例如 GPU，充分利用其大显存高带宽优势。
6. 设计更加灵活、自适应的合并策略，以面向不同的场景需求。
