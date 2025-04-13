#include <gtest/gtest.h>

#include <lru.hpp>

#include <chrono>

namespace cache::test {

using namespace std::chrono_literals;

TEST(LRU, BasicsNoTTL) {
  LRU<uint32_t> lru{3};

  const auto now = utils::Now();
  const auto far_future = now + 3600;

  EXPECT_FALSE(lru.Update(1, far_future).has_value());
  EXPECT_FALSE(lru.Update(2, far_future).has_value());
  EXPECT_FALSE(lru.Update(3, far_future).has_value());

  EXPECT_TRUE(lru.Get(1, now));
  EXPECT_TRUE(lru.Get(2, now));
  EXPECT_TRUE(lru.Get(3, now));
}

TEST(LRU, BasicWithTTL) {
  LRU<uint32_t> lru{3};

  const auto now = utils::Now();
  const auto future = now + 3600;

  EXPECT_FALSE(lru.Update(1, future).has_value());
  EXPECT_FALSE(lru.Update(2, future).has_value());
  EXPECT_FALSE(lru.Update(3, future).has_value());

  EXPECT_TRUE(lru.Get(1, now));
  EXPECT_TRUE(lru.Get(2, now));
  EXPECT_TRUE(lru.Get(3, now));

  EXPECT_FALSE(lru.Get(1, future + 60));
  EXPECT_FALSE(lru.Get(2, future + 60));
  EXPECT_FALSE(lru.Get(3, future + 60));
}

TEST(LRU, EvictionNoTTL) {
  LRU<uint32_t> lru{3};

  const auto now = utils::Now();
  const auto far_future = now + 3600;

  EXPECT_FALSE(lru.Update(1, far_future).has_value());
  EXPECT_FALSE(lru.Update(2, far_future).has_value());
  EXPECT_FALSE(lru.Update(3, far_future).has_value());

  EXPECT_TRUE(lru.Get(1, now));
  EXPECT_TRUE(lru.Get(1, now));
  EXPECT_TRUE(lru.Get(2, now));

  std::optional<uint32_t> evicted{};
  EXPECT_TRUE(evicted = lru.Update(4, far_future));
  EXPECT_EQ(*evicted, 3);  // 3 is evicted
  EXPECT_FALSE(lru.Get(3, now));

  EXPECT_TRUE(lru.Get(1, now));  // rest are still there
  EXPECT_TRUE(lru.Get(2, now));
}

TEST(LRU, MaxSizeGreaterThanZeroNoTTL) {
  LRU<uint32_t> lru{0};

  const auto now = utils::Now();
  const auto far_future = now + 3600;

  EXPECT_FALSE(lru.Update(1, far_future).has_value());
  EXPECT_TRUE(lru.Update(2, far_future).has_value());

  EXPECT_FALSE(lru.Get(1, now));
  EXPECT_TRUE(lru.Get(2, now));
}

}  // namespace cache::test
