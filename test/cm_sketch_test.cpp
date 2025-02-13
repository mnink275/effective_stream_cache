#include <gtest/gtest.h>

#include <unordered_map>

#include "../cm_sketch.hpp"

namespace cache::test {

TEST(CountMinSketch, SizeOf) {
  constexpr size_t NUM_COUNTERS = 16;
  static_assert(sizeof(CountMinSketch<NUM_COUNTERS>) == 2 * NUM_COUNTERS + 16 + 4);
}

TEST(CountMinSketch, Construction) {
  CountMinSketch<5> sketch1;
  EXPECT_EQ(sketch1.GetMask(), 7);

  CountMinSketch<32> sketch2;
  EXPECT_EQ(sketch2.GetMask(), 31);
}

TEST(CountMinSketch, Add) {
  CountMinSketch<16> sketch;
  const auto to_be_multiple_times_added = 1;
  for (size_t i = 0; i < 4; i++) {
    sketch.Add(to_be_multiple_times_added);
    EXPECT_EQ(sketch.Estimate(to_be_multiple_times_added), i + 1);
  }
}

TEST(CountMinSketch, Estimate) {
  CountMinSketch<16> sketch;
  sketch.Add(1);
  sketch.Add(1);
  EXPECT_EQ(sketch.Estimate(1), 2);
  EXPECT_EQ(sketch.Estimate(0), 0);
}

TEST(CountMinSketch, AddEstimateLarge) {
  const auto sample_size = 1000;
  CountMinSketch<sample_size> sketch;

  const auto four_bit_counter_limit = 15;
  for (size_t i = 1; i <= four_bit_counter_limit; ++i) {
    sketch.Add(5);
    EXPECT_EQ(sketch.Estimate(5), i);
  }
  const auto less_than_sample_size = sample_size / 10;
  for (size_t i = four_bit_counter_limit; i <= less_than_sample_size; ++i) {
    sketch.Add(5);
    EXPECT_EQ(sketch.Estimate(5), four_bit_counter_limit);
  }
}

TEST(CountMinSketch, Reset) {
  CountMinSketch<16> sketch;

  for (size_t i = 0; i < 4; i++) {
    sketch.Add(1);
  }

  sketch.Reset();
  EXPECT_EQ(sketch.Estimate(1), 2);
}

TEST(CountMinSketch, Clear) {
  CountMinSketch<16> sketch;
  for (size_t i = 0; i < 16; i++) {
    sketch.Add(i);
  }

  sketch.Clear();

  for (size_t i = 0; i < 16; i++) {
    EXPECT_EQ(sketch.Estimate(i), 0);
  }
}

TEST(CountMinSketch, SerializeDeserialize) {
  CountMinSketch<1000> sketch;

  std::random_device rd;
  const auto seed = rd();
  std::cout << "Seed: " << seed << std::endl;

  std::mt19937 gen(seed);

  std::unordered_map<uint32_t, size_t> keys_freqs;
  for (size_t i = 0; i < 10000; ++i) {
    const auto key = gen();
    ++keys_freqs[key];

    sketch.Add(key);
  }

  {
    std::ofstream file("/tmp/sketch.bin", std::ios::binary);
    sketch.Store(file);
  }
  {
    sketch.Clear();
    std::ifstream file("/tmp/sketch.bin", std::ios::binary);
    sketch.Load(file);
  }

  for (auto [key, count] : keys_freqs) {
    EXPECT_GE(sketch.Estimate(key), count);
  }
}

}  // namespace cache::test
