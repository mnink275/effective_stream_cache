#include <gtest/gtest.h>

#include <cache.hpp>

#include <chrono>

namespace cache::test {

using namespace std::chrono_literals;

TEST(SmallPageTLFU, BasicsNoTTL) {
  TTinyLFU tiny_lfu;
  SmallPageAdvanced small_page{tiny_lfu};

  const auto now = utils::Now();
  const auto far_future = now + 3600;
  for (size_t i = 0; i < SMALL_PAGE_SIZE; ++i) {
    EXPECT_FALSE(small_page.Get(i, now));
    EXPECT_TRUE(small_page.Update(i, far_future));
    EXPECT_TRUE(small_page.Get(i, now));
  }
}

TEST(SmallPageTLFU, BasicsWithTTL) {
  TTinyLFU tiny_lfu;
  SmallPageAdvanced small_page{tiny_lfu};

  const auto now = utils::Now();
  const auto future = now + 3600;
  for (size_t i = 0; i < SMALL_PAGE_SIZE; ++i) {
    EXPECT_FALSE(small_page.Get(i, now));
    EXPECT_TRUE(small_page.Update(i, future));
    EXPECT_TRUE(small_page.Get(i, now));
  }

  for (size_t i = 0; i < SMALL_PAGE_SIZE; ++i) {
    EXPECT_FALSE(small_page.Get(i, future + 2 * 3600));
  }

  for (size_t i = 0; i < SMALL_PAGE_SIZE; ++i) {
    EXPECT_TRUE(small_page.Update(i, future + 2 * 3600));
  }
}

}  // namespace cache::test
