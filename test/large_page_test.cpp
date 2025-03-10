#include <gtest/gtest.h>

#include <cache.hpp>

namespace cache::test {

using Key = uint32_t;
using namespace std::chrono_literals;

TEST(LargePage, Basics) {
  TTinyLFU tiny_lfu;
  LargePage large_page{tiny_lfu};

  for (size_t i = 0; i < SMALL_PAGE_SIZE; ++i) {
    EXPECT_FALSE(large_page.Get(i));
    large_page.Update(i);
    EXPECT_TRUE(large_page.Get(i));
  }

  large_page.Clear();
  for (size_t i = 0; i < SMALL_PAGE_SIZE; ++i) {
    EXPECT_FALSE(large_page.Get(i));
  }
}

TEST(LargePage, SerializeDeserialize) {
  TTinyLFU tiny_lfu;
  LargePage large_page{tiny_lfu};

  std::random_device rd;
  const auto seed = rd();
  std::cout << "Seed: " << seed << std::endl;
  std::mt19937 gen(seed);

  const auto NUM_KEYS = 20'000;
  std::vector<uint32_t> keys;
  keys.reserve(NUM_KEYS);
  for (size_t i = 0; i < NUM_KEYS; ++i) {
    const uint32_t key = gen();
    keys.push_back(key);

    if (!large_page.Get(key)) large_page.Update(key);
  }

  tiny_lfu.Clear();
  LargePage large_page_copy{tiny_lfu};
  for (auto key : keys) {
    if (!large_page_copy.Get(key)) large_page_copy.Update(key);
  }

  {
    std::ofstream file("/tmp/large_page.bin", std::ios::binary);
    large_page.Store(file);
  }
  {
    large_page.Clear();
    std::ifstream file("/tmp/large_page.bin", std::ios::binary);
    large_page.Load(file);
  }

  EXPECT_TRUE(large_page == large_page_copy);
}

}  // namespace cache::test
