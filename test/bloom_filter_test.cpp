#include <gtest/gtest.h>

#include <bloom_filter.hpp>

#include <random>
#include <fstream>

namespace cache::test {

TEST(BloomFilter, AddAndTest) {
  const size_t capacity = 1000;
  BloomFilter bf{capacity, 0.01};

  for (size_t i = 0; i < capacity; ++i) {
    EXPECT_FALSE(bf.Test(i));
  }

  bf.Add(1);
  bf.Add(2);
  bf.Add(3);
  bf.Add(5);

  EXPECT_TRUE(bf.Test(1));
  EXPECT_TRUE(bf.Test(2));
  EXPECT_TRUE(bf.Test(3));
  EXPECT_TRUE(bf.Test(5));
}

TEST(BloomFilter, FillAndClear) {
  BloomFilter bf{1000, 0.01};

  const size_t num_keys = 42;

  for (size_t i = 0; i < num_keys; ++i) {
    bf.Add(i);
    EXPECT_TRUE(bf.Test(i));
  }
  
  bf.Clear();

  for (size_t i = 0; i < num_keys; ++i) {
    EXPECT_FALSE(bf.Test(i));
  }
}

TEST(BloomFilter, SerializeDeserialize) {
  BloomFilter bf{1000, 0.01};
  BloomFilter bf_copy{1000, 0.01};

  EXPECT_EQ(bf, bf_copy);

  std::random_device rd;
  const auto seed = rd();
  std::cout << "Seed: " << seed << std::endl;

  std::mt19937 gen(seed);

  for (size_t i = 0; i < 100; ++i) {
    auto key = gen();
    bf.Add(key);
    bf_copy.Add(key);
  }

  {
    std::ofstream file("/tmp/bloom_filter.bin", std::ios::binary);
    bf.Store(file);
  }
  {
    bf.Clear();
    std::ifstream file("/tmp/bloom_filter.bin", std::ios::binary);
    bf.Load(file);
  }

  EXPECT_EQ(bf, bf_copy);
}

}  // namespace cache::test
