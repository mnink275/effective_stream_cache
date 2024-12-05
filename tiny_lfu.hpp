#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

template <class T, size_t kSize, size_t KCounterLimit>
class TinyLFU final {
 public:
  using HashFunc = size_t(*)(T key);
  // TODO: we need no more than log2(kCounterLimit) bits
  // Set CounterType type in constexpr context?
  using CounterType = uint16_t;

  static constexpr auto kCounterLimit = KCounterLimit;

  template <class ...Funcs>
  TinyLFU(Funcs&&... funcs)
    : counters_(),
      hash_funcs_({funcs...}), global_counter_(0) {}

  void Add(T key) {
    ++counters_[GetMinCountIdx(key)];

    if (++global_counter_ > kCounterLimit) {
      DivideCountersBy2();
    }
  }

  size_t Estimate(T key) {
    return counters_[GetMinCountIdx(key)];
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
