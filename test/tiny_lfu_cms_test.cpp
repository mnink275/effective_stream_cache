#include <gtest/gtest.h>

#include <tiny_lfu_cms.hpp>

namespace cache::test {

TEST(TinyLFUBasedOnCMS, Basics) {
  const size_t sample_size = 4;
  TinyLFU<uint32_t, sample_size, /*NumCounters=*/4, /*UseDoorKeeper=*/false> tiny_lfu;
  TinyLFU<uint32_t, sample_size, /*NumCounters=*/4, /*UseDoorKeeper=*/true> tiny_lfu_dk;

  for (size_t i = 1; i < sample_size; ++i) {
    tiny_lfu.Add(1);
    EXPECT_EQ(tiny_lfu.Estimate(1), i);

    tiny_lfu_dk.Add(1);
    EXPECT_EQ(tiny_lfu_dk.Estimate(1), i);
  }

  tiny_lfu.Add(1);
  EXPECT_EQ(tiny_lfu.Estimate(1), 2);

  tiny_lfu_dk.Add(1);
  // Reset happens when counter = 3 (because another +1 holds by DoorKeeper)
  // new_counter = counter / 2 = 1
  EXPECT_EQ(tiny_lfu_dk.Estimate(1), 1);
}

TEST(TinyLFUBasedOnCMS, ResetAtEvenCounter) {
  const size_t sample_size = 5;
  TinyLFU<uint32_t, sample_size, /*NumCounters=*/4, /*UseDoorKeeper=*/true> tiny_lfu_dk;

  for (size_t i = 1; i < sample_size; ++i) {
    tiny_lfu_dk.Add(1);
    EXPECT_EQ(tiny_lfu_dk.Estimate(1), i);
  }

  tiny_lfu_dk.Add(1);
  // Reset happens when counter = 4 (because another +1 holds by DoorKeeper)
  // new_counter = counter / 2 = 2
  EXPECT_EQ(tiny_lfu_dk.Estimate(1), 2);
}

TEST(TinyLFUBasedOnCMS, Reset) {
  const size_t sample_size = 1000;
  TinyLFU<uint32_t, sample_size, /*NumCounters=*/sample_size / 10, /*UseDoorKeeper=*/false> tiny_lfu;
  
  const auto four_bit_counter_limit = 15;
  for (size_t i = 1; i <= four_bit_counter_limit; ++i) {
    tiny_lfu.Add(5);
    EXPECT_EQ(tiny_lfu.Estimate(5), i);
  }
  const auto less_than_sample_size = sample_size / 10;
  for (size_t i = four_bit_counter_limit; i <= less_than_sample_size; ++i) {
    tiny_lfu.Add(5);
    EXPECT_EQ(tiny_lfu.Estimate(5), four_bit_counter_limit);
  }

  tiny_lfu.Reset();

  EXPECT_EQ(tiny_lfu.Estimate(5), four_bit_counter_limit / 2);
}

TEST(TinyLFUBasedOnCMS, Clear) {
  TinyLFU<uint32_t, /*SampleSize=*/4, /*NumCounters=*/4, /*UseDoorKeeper=*/true> tiny_lfu;

  tiny_lfu.Add(1);
  tiny_lfu.Add(5);
  tiny_lfu.Add(5);

  tiny_lfu.Clear();

  EXPECT_EQ(tiny_lfu.Estimate(1), 0);
  EXPECT_EQ(tiny_lfu.Estimate(5), 0);
}

TEST(TinyLFUBasedOnCMS, SerializeDeserialize) {
  const size_t sample_size = 1000;
  using TLFU = TinyLFU<uint32_t, /*SampleSize=*/sample_size, /*NumCounters=*/32, /*UseDoorKeeper=*/true>;
  TLFU tiny_lfu;

  std::random_device rd;
  const auto seed = rd();
  std::cout << "Seed: " << seed << std::endl;

  std::mt19937 gen(seed);

  std::unordered_map<uint32_t, size_t> keys_freqs;
  const size_t less_than_sample_size = sample_size / 10;
  for (size_t i = 0; i < less_than_sample_size; ++i) {
    const auto key = gen();
    ++keys_freqs[key];

    tiny_lfu.Add(key);
  }

  {
    std::ofstream file("/tmp/tiny_lfu_cms.bin", std::ios::binary);
    tiny_lfu.Store(file);
  }
  {
    tiny_lfu.Clear();
    std::ifstream file("/tmp/tiny_lfu_cms.bin", std::ios::binary);
    tiny_lfu.Load(file);
  }

  for (auto [key, count] : keys_freqs) {
    EXPECT_GE(tiny_lfu.Estimate(key), count);
  }
}

}  // namespace cache::test
