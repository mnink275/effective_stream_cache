#include <benchmark/benchmark.h>

#include <tiny_lfu_cms.hpp>

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
2025-03-05T21:36:45+03:00
Running ./build_release/benchmark/cache_benchmark
Run on (16 X 5065.65 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x8)
  L1 Instruction 32 KiB (x8)
  L2 Unified 1024 KiB (x8)
  L3 Unified 16384 KiB (x1)
Load Average: 1.20, 1.73, 1.88
------------------------------------------------------------------
Benchmark                        Time             CPU   Iterations
------------------------------------------------------------------
TLFU_Add                      7.06 ns         7.06 ns     86783168
TLFU_Estimate                 1.81 ns         1.81 ns    377673764
TLFUDoorKeeper_Add            5.88 ns         5.88 ns    103763591
TLFUDoorKeeper_Estimate       1.79 ns         1.79 ns    382370366
*/
