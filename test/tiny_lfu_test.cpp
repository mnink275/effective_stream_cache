#include <gtest/gtest.h>

#include "../tiny_lfu.hpp"

namespace ink::test {

TEST(TinyLFU, Basics) {
  TinyLFU<int64_t, /*kSize=*/10, /*kCounterLimit=*/20'000> tiny_lfu(
    [](int64_t key) { return static_cast<size_t>(key); },
    [](int64_t key) { return static_cast<size_t>(2 * key); }
  );

  EXPECT_EQ(tiny_lfu.Estimate(1), 0);
  tiny_lfu.Add(1);
  EXPECT_EQ(tiny_lfu.Estimate(1), 0);
  tiny_lfu.Add(1);
  EXPECT_EQ(tiny_lfu.Estimate(1), 1);

  EXPECT_EQ(tiny_lfu.Estimate(2), 0);
  tiny_lfu.Add(2);
  EXPECT_EQ(tiny_lfu.Estimate(2), 1);
  tiny_lfu.Add(2);
  EXPECT_EQ(tiny_lfu.Estimate(2), 1);
  tiny_lfu.Add(2);
  EXPECT_EQ(tiny_lfu.Estimate(2), 2);

  EXPECT_EQ(tiny_lfu.Estimate(1), 1);
  tiny_lfu.Add(1);
  EXPECT_EQ(tiny_lfu.Estimate(1), 2);
}

TEST(TinyLFU, OneHashFunc) {
  TinyLFU<int64_t, /*kSize=*/10, /*kCounterLimit=*/20'000> tiny_lfu(
    [](int64_t key) { return static_cast<size_t>(key); }
  );

  EXPECT_EQ(tiny_lfu.Estimate(5), 0);
  tiny_lfu.Add(5);
  EXPECT_EQ(tiny_lfu.Estimate(5), 1);
  for (size_t i = 0; i < 10; ++i) {
    tiny_lfu.Add(5);
    EXPECT_EQ(tiny_lfu.Estimate(5), i + 2);
  }

  for (size_t i = 0; i < 10; ++i) {
    if (i == 5) continue;
    EXPECT_EQ(tiny_lfu.Estimate(i), 0);
  }
}

TEST(TinyLFU, CounterLimit) {
  constexpr size_t kCounterLimit = 10;
  TinyLFU<int64_t, /*kSize=*/10, /*kCounterLimit=*/kCounterLimit> tiny_lfu(
    [](int64_t key) { return static_cast<size_t>(key); }
  );

  for (size_t i = 1; i <= kCounterLimit; ++i) {
    tiny_lfu.Add(5);
    EXPECT_EQ(tiny_lfu.Estimate(5), i);
  }

  tiny_lfu.Add(5);
  EXPECT_EQ(tiny_lfu.Estimate(5), kCounterLimit / 2);
}

TEST(TinyLFU, HashFuncs) {
  TinyLFU<int64_t, /*kSize=*/10'000, /*kCounterLimit=*/20'000> tiny_lfu(
    [](int64_t key) { return static_cast<size_t>(key) * 2654435761 % 2^32; },
    [](int64_t key) { return static_cast<size_t>(key); },
    [](int64_t key) { return static_cast<size_t>(key) ^ ((key << 17) | (key >> 16)); }
  );

  EXPECT_EQ(tiny_lfu.Estimate(1), 0);
  tiny_lfu.Add(1);
  EXPECT_EQ(tiny_lfu.Estimate(1), 0);
  tiny_lfu.Add(1);
  EXPECT_EQ(tiny_lfu.Estimate(1), 0);
  tiny_lfu.Add(1);
  EXPECT_EQ(tiny_lfu.Estimate(1), 1);
}

}  // namespace ink::test
