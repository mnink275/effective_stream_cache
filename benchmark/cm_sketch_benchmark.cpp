#include <benchmark/benchmark.h>

#include "../cm_sketch.hpp"

#include <random>

static constexpr size_t kNumCounters = 1024;

static void CMS_Add(benchmark::State& state) {
  cache::CountMinSketch<kNumCounters> sketch;
  std::mt19937 rng;
  for (auto _ : state) {
    sketch.Add(rng());
  }
}
BENCHMARK(CMS_Add);

static void CMS_Estimate(benchmark::State& state) {
  cache::CountMinSketch<kNumCounters> sketch;
  std::mt19937 rng;
  for (size_t i = 0; i < kNumCounters; i++) {
    sketch.Add(rng());
  }

  for (auto _ : state) {
    sketch.Estimate(rng());
  }
}
BENCHMARK(CMS_Estimate);

/*
2025-03-05T18:49:54+03:00
Running ./build_release/benchmark/cache_benchmark
Run on (16 X 3374.88 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x8)
  L1 Instruction 32 KiB (x8)
  L2 Unified 1024 KiB (x8)
  L3 Unified 16384 KiB (x1)
Load Average: 1.53, 1.55, 1.75
-------------------------------------------------------
Benchmark             Time             CPU   Iterations
-------------------------------------------------------
CMS_Add            4.49 ns         4.49 ns    131088315
CMS_Estimate       1.74 ns         1.74 ns    379967233
*/
