#include <gtest/gtest.h>

#include <bloom_filter_simple.hpp>

#include <fstream>
#include <random>

namespace cache::test {

using Key = uint32_t;

TEST(BloomFilterSimple, AddAndTest) {
  BloomFilter<Key, /*kSize=*/1000> bf{
      [](Key key) { return static_cast<size_t>(key); },
      [](Key key) { return static_cast<size_t>(2 * key); }};

  EXPECT_FALSE(bf.Test(1));
  bf.Add(1);
  EXPECT_TRUE(bf.Test(1));

  EXPECT_FALSE(bf.Test(2));
  bf.Add(2);
  EXPECT_TRUE(bf.Test(2));
}

TEST(BloomFilterSimple, AddAndTestCollision) {
  BloomFilter<Key, /*kSize=*/1000> bf(
      [](Key key) { return static_cast<size_t>(key); },
      [](Key key) { return static_cast<size_t>(2 * key); });

  EXPECT_FALSE(bf.Test(1));
  EXPECT_FALSE(bf.Test(4));
  bf.Add(1);  // marks cells 1 and 2
  bf.Add(4);  // marks cells 4 and 8

  EXPECT_TRUE(bf.Test(2));  // collision at 2 and 4
}

TEST(BloomFilterSimple, LoadFactor) {
  BloomFilter<Key, /*kSize=*/10> bf(
      [](Key key) { return static_cast<size_t>(key); },
      [](Key key) { return static_cast<size_t>(key + 5); });

  EXPECT_EQ(bf.LoadFactor(), 0.0);

  for (size_t i = 10; i < 15; ++i) {
    EXPECT_TRUE(bf.LoadFactor() < 1.0);
    bf.Add(i);
  }

  EXPECT_DOUBLE_EQ(bf.LoadFactor(), 1.0);
}

TEST(BloomFilterSimple, FillAndClear) {
  BloomFilter<Key, /*kSize=*/10> bf(
      [](Key key) { return static_cast<size_t>(key); });

  EXPECT_EQ(bf.LoadFactor(), 0.0);

  for (size_t i = 0; i < 10; ++i) {
    EXPECT_TRUE(bf.LoadFactor() < 1.0);
    bf.Add(i);
  }

  EXPECT_DOUBLE_EQ(bf.LoadFactor(), 1.0);
  for (size_t i = 0; i < 10; ++i) {
    EXPECT_TRUE(bf.Test(i));
  }

  bf.Clear();
  EXPECT_EQ(bf.LoadFactor(), 0.0);

  for (size_t i = 0; i < 10; ++i) {
    EXPECT_FALSE(bf.Test(i));
  }
}

TEST(BloomFilterSimple, Mod) {
  BloomFilter<Key, /*kSize=*/10> bf(
      [](Key key) { return static_cast<size_t>(key); });

  EXPECT_FALSE(bf.Test(0));
  bf.Add(420);
  EXPECT_TRUE(bf.Test(0));

  EXPECT_FALSE(bf.Test(42));
  bf.Add(42);
  EXPECT_TRUE(bf.Test(2));
}

TEST(BloomFilterSimple, SerializeDeserialize) {
  BloomFilter<Key, /*kSize=*/1000> bf(
      [](Key key) { return static_cast<size_t>(key) * 2654435761 % 2 ^ 32; },
      [](Key key) {
        key += ~(key << 15);
        key ^= (key >> 10);
        key += (key << 3);
        key ^= (key >> 6);
        key += ~(key << 11);
        key ^= (key >> 16);
        return static_cast<size_t>(key);
      });

  BloomFilter<Key, /*kSize=*/1000> bf_copy(
      [](Key key) { return static_cast<size_t>(key) * 2654435761 % 2 ^ 32; },
      [](Key key) {
        key += ~(key << 15);
        key ^= (key >> 10);
        key += (key << 3);
        key ^= (key >> 6);
        key += ~(key << 11);
        key ^= (key >> 16);
        return static_cast<size_t>(key);
      });

  std::random_device rd;
  const auto seed = rd();
  std::cout << "Seed: " << seed << std::endl;

  std::mt19937 gen(seed);
  std::uniform_int_distribution<Key> dis(0, 1000 * 1000);

  for (size_t i = 0; i < 100; ++i) {
    auto key = dis(gen);
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
