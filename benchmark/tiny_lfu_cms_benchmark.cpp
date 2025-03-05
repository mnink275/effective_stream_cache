#include <benchmark/benchmark.h>

#include "../tiny_lfu_cms.hpp"

#include <random>
#include <cstdint>

static constexpr size_t kNumCounters = 1024;
using TLFU =cache::TinyLFU<
      uint32_t,
      /*SampleSize=*/10 * kNumCounters,
      /*NumCounters=*/kNumCounters,
      /*UseDoorKeeper=*/false
>;
using TLFUDoorKeeper =cache::TinyLFU<
      uint32_t,
      /*SampleSize=*/10 * kNumCounters,
      /*NumCounters=*/kNumCounters,
      /*UseDoorKeeper=*/true
>;

static void TLFU_Add(benchmark::State& state) {
  TLFU tiny_lfu;
  std::mt19937 rng;
  for (auto _ : state) {
    tiny_lfu.Add(rng());
  }
}
BENCHMARK(TLFU_Add);

static void TLFU_Estimate(benchmark::State& state) {
  TLFU tiny_lfu;
  std::mt19937 rng;
  for (size_t i = 0; i < kNumCounters; i++) {
    tiny_lfu.Add(rng());
  }

  for (auto _ : state) {
    tiny_lfu.Estimate(rng());
  }
}
BENCHMARK(TLFU_Estimate);

static void TLFUDoorKeeper_Add(benchmark::State& state) {
  TLFUDoorKeeper tiny_lfu;
  std::mt19937 rng;
  for (auto _ : state) {
    tiny_lfu.Add(rng());
  }
}
BENCHMARK(TLFUDoorKeeper_Add);

static void TLFUDoorKeeper_Estimate(benchmark::State& state) {
  TLFUDoorKeeper tiny_lfu;
  std::mt19937 rng;
  for (size_t i = 0; i < kNumCounters; i++) {
    tiny_lfu.Add(rng());
  }

  for (auto _ : state) {
    tiny_lfu.Estimate(rng());
  }
}
BENCHMARK(TLFUDoorKeeper_Estimate);

/*
2025-03-05T19:13:38+03:00
Running ./build_release/benchmark/cache_benchmark
Run on (16 X 4485.69 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x8)
  L1 Instruction 32 KiB (x8)
  L2 Unified 1024 KiB (x8)
  L3 Unified 16384 KiB (x1)
Load Average: 2.19, 1.92, 1.65
------------------------------------------------------------------
Benchmark                        Time             CPU   Iterations
------------------------------------------------------------------
TLFU_Add                      7.19 ns         7.19 ns     88385271
TLFU_Estimate                 1.83 ns         1.83 ns    379588387
TLFUDoorKeeper_Add            6.01 ns         6.01 ns    105109425
TLFUDoorKeeper_Estimate       1.80 ns         1.80 ns    377758077
*/
