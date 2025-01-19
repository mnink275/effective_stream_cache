#include <gtest/gtest.h>

#include "../cache.hpp"

#include <random>
#include <fstream>

namespace cache::test {

TEST(SmallPage, Simple) {
  SmallPage page;

  EXPECT_FALSE(page.Get(42));
  page.Update(42);
  EXPECT_TRUE(page.Get(42));

  page.Clear();
  EXPECT_FALSE(page.Get(42));
}

TEST(SmallPage, Frequency) {
  SmallPage page;

  for (size_t i = 0; i < SMALL_PAGE_SIZE; ++i) {
    EXPECT_FALSE(page.Get(i));
  }

  std::random_device rd;
  const auto seed = rd();
  std::cout << "Seed: " << seed << std::endl;

  std::mt19937 gen(seed);
  std::uniform_int_distribution<Key> dis(0, std::numeric_limits<Key>::max() - 1);

  const size_t kTimes = 100;
  for (size_t i = 0; i < kTimes * SMALL_PAGE_SIZE; ++i) {
    auto key = dis(gen);
    if (!page.Get(key)) page.Update(key);

    // maintain key frequency
    if (!page.Get(42)) page.Update(42);
  }
  EXPECT_TRUE(page.Get(42));

  page.Clear();
  for (size_t i = 0; i < SMALL_PAGE_SIZE; ++i) {
    EXPECT_FALSE(page.Get(i));
  }
}

TEST(SmallPage, SerializeDeserialize) {
  SmallPage page;
  SmallPage page_copy;

  std::random_device rd;
  const auto seed = rd();
  std::cout << "Seed: " << seed << std::endl;

  std::mt19937 gen(seed);
  std::uniform_int_distribution<Key> dis(0, std::numeric_limits<Key>::max() - 1);

  const size_t kTimes = 100;
  for (size_t i = 0; i < kTimes * SMALL_PAGE_SIZE; ++i) {
    auto key = dis(gen);
    if (!page.Get(key)) page.Update(key);
    if (!page_copy.Get(key)) page_copy.Update(key);
  }

  {
    std::ofstream file("/tmp/small_page.bin", std::ios::binary);
    page.Store(file);
  }
  {
    page.Clear();
    std::ifstream file("/tmp/small_page.bin", std::ios::binary);
    page.Load(file);
  }

  for (size_t i = 0; i < kTimes * SMALL_PAGE_SIZE; ++i) {
    // both false or both true
    EXPECT_FALSE(page.Get(i) ^ page_copy.Get(i));
  }
}

}  // namespace cache::test
