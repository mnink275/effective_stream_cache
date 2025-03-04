#pragma once

/*

4-bit counters CountMinSketch based on https://github.com/hypermodeinc/ristretto/blob/010b2dd22a2ef179ebbc3a916cc5c863c5837b90/sketch.go

*/

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <fstream>
#include <random>
#include <array>

#include "utils.hpp"

namespace cache {

namespace details {

constexpr size_t CM_DEPTH = 4;

template <uint32_t NumCounters>
class Row {
 public:
  constexpr static int RESET_MASK = 0x77; // 0111 0111
  constexpr static int MOD_16_MASK = 0x0F; // 0000 1111

  uint8_t Get(uint32_t value) const {
    return (data_[value / 2] >> ((value & 1) * 4)) & MOD_16_MASK;
  }

  void Add(uint32_t value) {
    uint32_t idx = value / 2; // divide by 2 because data.size() == NumCounters / 2
    uint8_t shift = (value & 1) * 4; // possible values: 0, 4
    uint8_t v = (data_[idx] >> shift) & MOD_16_MASK;
    if (v < 15) data_[idx] += (1 << shift); // add 1 to the counter
  }

  void Reset() {
    for (auto& byte : data_) {
      byte = (byte >> 1) & RESET_MASK; // reset each counter, e.g. 0011 1011 (shift)-> 0001 1101 (mask)-> 0001 0101
    }
  }

  void Clear() { data_.fill(0); }

  bool operator==(const Row& other) const { return data_ == other.data_; }

  void Load(std::ifstream& file) {
    utils::BinaryRead(file, data_.data(), data_.size());
  }

  void Store(std::ofstream& file) const {
    utils::BinaryWrite(file, data_.data(), data_.size());
  }

 private:
  std::array<uint8_t, NumCounters / 2> data_{}; // data.size() == NumCounters / 2 because each byte contains two 4-bit counters
};

}  // namespace details


template <uint32_t NumCounters>
requires(NumCounters > 0)
class CountMinSketch {
 public:
  constexpr static uint32_t kNumCounters = utils::NextPowerOf2(NumCounters);
  static_assert(kNumCounters > 1);
  constexpr static uint32_t kMask = kNumCounters - 1;

  using TRow = details::Row<kNumCounters>;

  CountMinSketch() {
    std::random_device rd;
    std::mt19937 rng(rd());
    for (size_t i = 0; i < details::CM_DEPTH; i++) {
      seeds_[i] = rng();
    }
  }

  void Add(uint32_t key) {
    for (size_t i = 0; i < details::CM_DEPTH; i++) {
      rows_[i].Add((key ^ seeds_[i]) & kMask); // `& mask` is equivalent to `% numCounters`
    }
  }

  uint8_t Estimate(uint32_t key) const {
    auto minVal = std::numeric_limits<uint8_t>::max();
    for (size_t i = 0; i < details::CM_DEPTH; i++) {
      uint8_t val = rows_[i].Get((key ^ seeds_[i]) & kMask);
      minVal = std::min(minVal, val);
    }
    return minVal;
  }

  void Reset() {
    for (auto& row : rows_) row.Reset();
  }

  void Clear() {
    for (auto& row : rows_) row.Clear();
  }

  const TRow& GetRow(size_t i) { return rows_[i]; }

  void Load(std::ifstream& file) {
    for (auto& row : rows_) {
      row.Load(file);
    }
    utils::BinaryRead(file, seeds_.data(), seeds_.size() * sizeof(seeds_[0]));
  }

  void Store(std::ofstream& file) const {
    for (auto& row : rows_) {
      row.Store(file);
    }
    utils::BinaryWrite(file, seeds_.data(), seeds_.size() * sizeof(seeds_[0]));
  }

  bool operator==(const CountMinSketch& other) const {
    return rows_ == other.rows_;
      // && seeds_ == other.seeds_ // seeds may differ
  }

 private:
  std::array<TRow, details::CM_DEPTH> rows_{}; // 2 * kNumCounters bytes
  std::array<uint32_t, details::CM_DEPTH> seeds_{}; // 16 bytes
};

}  // namespace cache
