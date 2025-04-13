#include <benchmark/benchmark.h>

#include <small_page.hpp>

#include <iostream>
#include <random>
#include <unordered_set>

namespace {

struct Records {
  alignas(32) std::array<cache::Key, cache::SMALL_PAGE_SIZE> data{};
};

std::pair<Records, uint32_t> getInitRecordsAndKeyToBeFind() {
  static auto data_and_key = [&]() {
    const auto kSeed = std::random_device{}();
    std::mt19937 gen{kSeed};
    std::uniform_int_distribution<uint32_t> dist(0, cache::INVALID_HASH - 1);

    std::unordered_set<cache::Key> exists;
    Records records{};
    for (size_t i = 0; i < cache::SMALL_PAGE_SIZE; ++i) {
      auto key = dist(gen);

      records.data[i] = key;
      exists.insert(key);
    }

    uint32_t key = 0;
    while (exists.contains(key)) {
      key = dist(gen);
    }

    std::cout << "Seed: " << kSeed << '\n';
    std::cout << "Key to be found: " << key << '\n';

    return std::pair<Records, uint32_t>{records, key};
  }();

  return {data_and_key.first, data_and_key.second};
}

}  // namespace

static void SmallPage_SimpleFind(benchmark::State& state) {
  const auto [records, key] = getInitRecordsAndKeyToBeFind();
  for (auto _ : state) {
    auto res = cache::FindKeyIdx(key, records.data);
    benchmark::DoNotOptimize(res);
  }
}
BENCHMARK(SmallPage_SimpleFind);

static void SmallPage_SIMD_8(benchmark::State& state) {
  const auto [records, key] = getInitRecordsAndKeyToBeFind();
  for (auto _ : state) {
    auto res = cache::FindKeyIdxSIMD8(key, records.data);
    benchmark::DoNotOptimize(res);
  }
}
BENCHMARK(SmallPage_SIMD_8);

static void SmallPage_SIMD_16(benchmark::State& state) {
  const auto [records, key] = getInitRecordsAndKeyToBeFind();
  for (auto _ : state) {
    auto res = cache::FindKeyIdxSIMD16(key, records.data);
    benchmark::DoNotOptimize(res);
  }
}
BENCHMARK(SmallPage_SIMD_16);

/*
DOUBLE SWAP:

2025-04-04T10:25:44+03:00
Running ./build_release/benchmark/cache_benchmark
Run on (16 X 4209.86 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x8)
  L1 Instruction 32 KiB (x8)
  L2 Unified 1024 KiB (x8)
  L3 Unified 16384 KiB (x1)
Load Average: 2.27, 2.37, 1.81
Seed: 666530122
Key to be found: 0
---------------------------------------------------------------
Benchmark                     Time             CPU   Iterations
---------------------------------------------------------------
SmallPage_SimpleFind        151 ns          151 ns      4627770
SmallPage_SIMD_8           53.2 ns         53.2 ns     13030907
SmallPage_SIMD_16          23.5 ns         23.5 ns     29738834

SINGLE SWAP (removed, see /dev/Meetings/04.04.25/small_page_single_swap.hpp):

2025-04-04T09:47:23+03:00
Running ./build_release/benchmark/cache_benchmark
Run on (16 X 3894.15 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x8)
  L1 Instruction 32 KiB (x8)
  L2 Unified 1024 KiB (x8)
  L3 Unified 16384 KiB (x1)
Load Average: 2.40, 2.28, 1.86
Seed: 911297508
Key to be found: 0
---------------------------------------------------------------
Benchmark                     Time             CPU   Iterations
---------------------------------------------------------------
SmallPage_SimpleFind        130 ns          130 ns      4701407
SmallPage_SIMD_8            273 ns          273 ns      2518062
SmallPage_SIMD_16           274 ns          274 ns      2518156
*/
