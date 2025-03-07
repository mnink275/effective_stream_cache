#pragma once

#include <cstdint>
#include <limits>
#include <cstddef>

#include <tiny_lfu_cms.hpp>

namespace cache {

using Key = uint32_t;

inline const size_t INVALID_HASH = std::numeric_limits<Key>::max();

inline constexpr size_t LARGE_PAGE_SHIFT = 13;
inline constexpr size_t SMALL_PAGE_SHIFT = 8;
inline constexpr size_t SMALL_PAGE_SIZE_SHIFT = 10;

#define USE_LRU_FLAG true
inline constexpr double LRU_SIZE = 50'000;

inline constexpr size_t TLFU_SIZE = 1000;
inline constexpr size_t SAMPLE_SIZE = TLFU_SIZE * 10;
inline constexpr bool USE_DOOR_KEEPER = false;
using TTinyLFU = TinyLFU<Key, SAMPLE_SIZE, TLFU_SIZE, USE_DOOR_KEEPER>;

#define USE_TINY_LFU_FLAG true // use Advanced version with TinyLFU
#define USE_ENCHANCED false // use Simple version with frequency counters 4-bit sized
#define USE_BF_FLAG false
#define USE_SIMD_FLAG true

#define ENABLE_STATISTICS_FLAG false

#if USE_BF_FLAG
inline constexpr bool USE_BF = true;
#else
inline constexpr bool USE_BF = false;
#endif

#if USE_SIMD_FLAG
inline constexpr bool USE_SIMD = true;
#else
inline constexpr bool USE_SIMD = false;
#endif

#if USE_LRU_FLAG
inline constexpr bool USE_LRU = true;
#else
inline constexpr bool USE_LRU = false;
#endif

#if ENABLE_STATISTICS_FLAG
inline constexpr bool ENABLE_STATISTICS = true;
#else
inline constexpr bool ENABLE_STATISTICS = false;
#endif

inline const size_t LARGE_PAGE_NUMBER = 1 << LARGE_PAGE_SHIFT;
inline const size_t SMALL_PAGE_NUMBER = (1 << SMALL_PAGE_SHIFT) + 1;

inline const size_t SMALL_PAGE_SIZE = (1 << SMALL_PAGE_SIZE_SHIFT);  // количество записей на странице

inline const size_t LOADED_PAGE_NUMBER = 20;

inline const size_t LARGE_PAGE_PERIOD = 2'000;  // время, через которое частоты больших страниц /= 2

inline const size_t FREQUENCY_THRESHOLD = 370;

inline const size_t CACHE_SIZE = LOADED_PAGE_NUMBER * SMALL_PAGE_NUMBER * SMALL_PAGE_SIZE;

}  // namespace cache
