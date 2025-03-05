#include <gtest/gtest.h>

#include <lru.hpp>

namespace cache::test {

TEST(LRU, Basics) {
  LRU<uint32_t> lru{3};

  EXPECT_FALSE(lru.Update(1).has_value());
  EXPECT_FALSE(lru.Update(2).has_value());
  EXPECT_FALSE(lru.Update(3).has_value());

  EXPECT_TRUE(lru.Get(1));
  EXPECT_TRUE(lru.Get(2));
  EXPECT_TRUE(lru.Get(3));
}

TEST(LRU, Eviction) {
  LRU<uint32_t> lru{3};

  EXPECT_FALSE(lru.Update(1).has_value());
  EXPECT_FALSE(lru.Update(2).has_value());
  EXPECT_FALSE(lru.Update(3).has_value());

  EXPECT_TRUE(lru.Get(1));
  EXPECT_TRUE(lru.Get(1));
  EXPECT_TRUE(lru.Get(2));

  std::optional<uint32_t> evicted{};
  EXPECT_TRUE(evicted = lru.Update(4));
  EXPECT_EQ(*evicted, 3); // 3 is evicted
  EXPECT_FALSE(lru.Get(3));

  EXPECT_TRUE(lru.Get(1)); // rest are still there
  EXPECT_TRUE(lru.Get(2));
}

TEST(LRU, MaxSizeGreaterThanZero) {
  LRU<uint32_t> lru{0};

  EXPECT_FALSE(lru.Update(1).has_value());
  EXPECT_TRUE(lru.Update(2).has_value());

  EXPECT_FALSE(lru.Get(1));
  EXPECT_TRUE(lru.Get(2));
}

}  // namespace cache::test
