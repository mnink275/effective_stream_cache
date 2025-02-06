#include <gtest/gtest.h>

#include "../cm_sketch.hpp"

namespace cache::test {

TEST(CountMinSketch, SizeOf) {
  static_assert(sizeof(CountMinSketch<16>) == 2 * 16 + 32 + 8);
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

}  // namespace cache::test
