#pragma once

#include <bitset>
#include <vector>

namespace cache {

template <class T, size_t kSize>
class BloomFilter {
 public:
  using HashFunc = size_t(*)(T key);

  template <class ...Funcs>
  BloomFilter(Funcs&&... funcs)
    : hash_funcs_({funcs...}), bloom_filter_() {}

  void Add(T key) {
    for (const auto& hash_func : hash_funcs_) {
      bloom_filter_[hash_func(key) % kSize] = true;
    }
  }

  bool Test(T key) const {
    for (const auto& hash_func : hash_funcs_) {
      if (!bloom_filter_[hash_func(key) % kSize]) return false;
    }

    return true;
  }

 private:
  std::vector<HashFunc> hash_funcs_;
  std::bitset<kSize> bloom_filter_;
};

}  // namespace cache
