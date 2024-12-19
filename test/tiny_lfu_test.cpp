#include <gtest/gtest.h>

#include "../tiny_lfu.hpp"

namespace ink::test {

TEST(TinyLFU, Basics) {
  using TLFU = TinyLFU<int64_t, /*kSize=*/10, /*kCounterLimit=*/20'000>;
  TLFU tiny_lfu(std::vector<TLFU::HashFunc>{
    [](int64_t key) { return static_cast<size_t>(key); },
    [](int64_t key) { return static_cast<size_t>(2 * key); }}
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
  using TLFU = TinyLFU<int64_t, /*kSize=*/10, /*kCounterLimit=*/20'000>;
  TLFU tiny_lfu(std::vector<TLFU::HashFunc>{
    [](int64_t key) { return static_cast<size_t>(key); }}
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
  using TLFU = TinyLFU<int64_t, /*kSize=*/100, /*kCounterLimit=*/kCounterLimit>;
  TLFU tiny_lfu(std::vector<TLFU::HashFunc>{
    [](int64_t key) { return static_cast<size_t>(key); }}
  );

  for (size_t i = 1; i <= kCounterLimit; ++i) {
    tiny_lfu.Add(5);
    EXPECT_EQ(tiny_lfu.Estimate(5), i);
  }

  tiny_lfu.Add(5);
  EXPECT_EQ(tiny_lfu.Estimate(5), kCounterLimit / 2);
}

TEST(TinyLFU, HashFuncs) {
  using TLFU = TinyLFU<int64_t, /*kSize=*/10'000, /*kCounterLimit=*/20'000>;
  TLFU tiny_lfu(std::vector<TLFU::HashFunc>{
    [](int64_t key) { return static_cast<size_t>(key) * 2654435761 % 2^32; },
    [](int64_t key) { return static_cast<size_t>(key); },
    [](int64_t key) { return static_cast<size_t>(key) ^ ((key << 17) | (key >> 16)); }}
  );

  EXPECT_EQ(tiny_lfu.Estimate(1), 0);
  tiny_lfu.Add(1);
  EXPECT_EQ(tiny_lfu.Estimate(1), 0);
  tiny_lfu.Add(1);
  EXPECT_EQ(tiny_lfu.Estimate(1), 0);
  tiny_lfu.Add(1);
  EXPECT_EQ(tiny_lfu.Estimate(1), 1);
}

TEST(TinyLFU, SerializeDeserialize) {
  using TLFU = TinyLFU<int64_t, /*kSize=*/100, /*kCounterLimit=*/1000>;
  TLFU tiny_lfu(std::vector<TLFU::HashFunc>{
    [](int64_t key) { return static_cast<size_t>(key); }}
  );
  using TLFU = TinyLFU<int64_t, /*kSize=*/100, /*kCounterLimit=*/1000>;
  TLFU tiny_lfu_copy(std::vector<TLFU::HashFunc>{
    [](int64_t key) { return static_cast<size_t>(key); }
});

  for (size_t i = 0; i < 50; ++i) {
    tiny_lfu.Add(i);
    tiny_lfu_copy.Add(i);
  }

  {
    std::ofstream file("/tmp/tiny_lfu.bin", std::ios::binary);
    tiny_lfu.Store(file);
  }

  {
    tiny_lfu.Clear();
    std::ifstream file("/tmp/tiny_lfu.bin", std::ios::binary);
    tiny_lfu.Load(file);
  }
  EXPECT_EQ(tiny_lfu, tiny_lfu_copy);
}

}  // namespace ink::test
