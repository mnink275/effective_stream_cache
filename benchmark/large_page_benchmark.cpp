#include <benchmark/benchmark.h>

#include <cache.hpp>

static void LargePage_StoreLoad(benchmark::State& state) {
  cache::TTinyLFU tiny_lfu;
  cache::LargePage large_page{tiny_lfu};
  std::ofstream out("/tmp/large_page.bin", std::ios::binary);
  std::ifstream in("/tmp/large_page.bin", std::ios::binary);
  for (auto _ : state) {
    large_page.Store(out);
    large_page.Load(in);
  }
}
BENCHMARK(LargePage_StoreLoad);

/*
Running ./build_release/benchmark/cache_benchmark
Run on (16 X 5065.12 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x8)
  L1 Instruction 32 KiB (x8)
  L2 Unified 1024 KiB (x8)
  L3 Unified 16384 KiB (x1)
Load Average: 0.98, 1.42, 1.53
--------------------------------------------------------------
Benchmark                    Time             CPU   Iterations
--------------------------------------------------------------
LargePage_StoreLoad     649488 ns       608708 ns         1250
*/
