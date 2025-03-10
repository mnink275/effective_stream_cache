#include <gtest/gtest.h>

#include <bloom_filter.hpp>

#include <random>
#include <fstream>

namespace cache::test {

static constexpr size_t kCapacity = 1000;

TEST(BloomFilter, AddAndTest) {
  BloomFilter<kCapacity> bf;

  for (size_t i = 0; i < kCapacity; ++i) {
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
  BloomFilter<kCapacity> bf;

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
  BloomFilter<kCapacity> bf;
  BloomFilter<kCapacity> bf_copy;

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
