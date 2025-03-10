#include <benchmark/benchmark.h>

#include <bloom_filter.hpp>

#include <random>

static constexpr size_t kCapacity = 1024;

static void BloomFilter_Add(benchmark::State& state) {
  cache::BloomFilter<kCapacity> bf;
  std::mt19937 rng;
  for (auto _ : state) {
    bf.Add(rng());
  }
}
BENCHMARK(BloomFilter_Add);

static void BloomFilter_Test(benchmark::State& state) {
  cache::BloomFilter<kCapacity> bf;
  std::mt19937 rng;
  for (auto _ : state) {
    bf.Test(rng());
  }
}
BENCHMARK(BloomFilter_Test);

/*
Run on (16 X 4949.96 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x8)
  L1 Instruction 32 KiB (x8)
  L2 Unified 1024 KiB (x8)
  L3 Unified 16384 KiB (x1)
Load Average: 2.18, 1.66, 1.47
-----------------------------------------------------------
Benchmark                 Time             CPU   Iterations
-----------------------------------------------------------
BloomFilter_Add        1.75 ns         1.75 ns    392168096
BloomFilter_Test       1.82 ns         1.81 ns    387862054
*/
