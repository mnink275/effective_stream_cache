#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <vector>
#include <limits>

template <class T, size_t kSize, size_t KCounterLimit>
class TinyLFU final {
 public:
  using HashFunc = size_t(*)(T key);
  // TODO: we need no more than log2(kCounterLimit) bits
  // Set CounterType type in constexpr context?
  using CounterType = uint8_t;

  static constexpr auto kCounterLimit = KCounterLimit;

  template <class ...Funcs>
  TinyLFU(Funcs&&... funcs)
    : counters_(),
      hash_funcs_({funcs...}), global_counter_(0) {}

  void Add(T key) {
    if (counters_[GetMinCountIdx(key)] < std::numeric_limits<CounterType>::max()) {
      ++counters_[GetMinCountIdx(key)];
    }

    if (++global_counter_ > kCounterLimit) {
      DivideCountersBy2();
    }
  }

  size_t Estimate(T key) {
    if (key == std::numeric_limits<T>::max()) return 0;

    return counters_[GetMinCountIdx(key)];
  }

  void Load(std::ifstream& file) {
    Clear();

    file.read(reinterpret_cast<char*>(&global_counter_), sizeof(global_counter_));
    for (auto& counter : counters_) {
      file.read(reinterpret_cast<char*>(&counter), sizeof(CounterType));
    }
  }

  void Store(std::ofstream& file) const {
    auto glob_counter = global_counter_;
    file.write(reinterpret_cast<char*>(&glob_counter), sizeof(global_counter_));
    for (auto counter : counters_) {
      file.write(reinterpret_cast<char*>(&counter), sizeof(CounterType));
    }
  }

  void Clear() {
    counters_.fill(0);
    global_counter_ = 0;
  }

  bool operator==(const TinyLFU& other) const {
    return counters_ == other.counters_ && global_counter_ == other.global_counter_;
  }

 private:
  size_t GetMinCountIdx(T key) {
    size_t min_count_idx = hash_funcs_[0](key) % kSize;
    for (auto& hash_func : hash_funcs_) {
      const auto idx = hash_func(key) % kSize;
      if (counters_[idx] < counters_[min_count_idx]) {
        min_count_idx = idx;
      }
    }

    return min_count_idx;
  }

  void DivideCountersBy2() {
    for (auto& counter : counters_) {
      counter >>= 1;
    }
  }

  std::array<CounterType, kSize> counters_;
  std::vector<HashFunc> hash_funcs_; // TODO: reduce memory usage

  size_t global_counter_;
};
