#include <gtest/gtest.h>

#include <cache.hpp>

#include <chrono>

namespace cache::test {

using namespace std::chrono_literals;

TEST(SmallPageTLFU, BasicsNoTTL) {
  TTinyLFU tiny_lfu;
  SmallPageAdvanced small_page{tiny_lfu};

  const auto now = std::chrono::steady_clock::now();
  const auto far_future = now + 1h;
  for (size_t i = 0; i < SMALL_PAGE_SIZE; ++i) {
    EXPECT_FALSE(small_page.Get(i, now));
    EXPECT_TRUE(small_page.Update(i, far_future));
    EXPECT_TRUE(small_page.Get(i, now));
  }
}

TEST(SmallPageTLFU, BasicsWithTTL) {
  TTinyLFU tiny_lfu;
  SmallPageAdvanced small_page{tiny_lfu};

  const auto now = std::chrono::steady_clock::now();
  const auto future = now + 1h;
  for (size_t i = 0; i < SMALL_PAGE_SIZE; ++i) {
    EXPECT_FALSE(small_page.Get(i, now));
    EXPECT_TRUE(small_page.Update(i, future));
    EXPECT_TRUE(small_page.Get(i, now));
  }

  for (size_t i = 0; i < SMALL_PAGE_SIZE; ++i) {
    EXPECT_FALSE(small_page.Get(i, future + 2h));
  }

  for (size_t i = 0; i < SMALL_PAGE_SIZE; ++i) {
    EXPECT_TRUE(small_page.Update(i, future + 2h));
  }
}

}  // namespace cache::test
