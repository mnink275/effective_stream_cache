#pragma once

#include <cache_config.hpp>
#include <utils.hpp>

#include <immintrin.h>

namespace cache {

inline size_t SmallPageIndex(Key key) noexcept {
    key &= (1ull << (8ull * sizeof(Key) - LARGE_PAGE_SHIFT)) - 1ull;
    return key % SMALL_PAGE_NUMBER;
}

// inline size_t SmallPageIndex(Key key) noexcept {
//     key &= (1ull << (8ull * sizeof(Key) - LARGE_PAGE_SHIFT)) - 1ull;
//     return key >> (8ull * sizeof(Key) - LARGE_PAGE_SHIFT - SMALL_PAGE_SHIFT);
// }

class SmallPageAdvanced {
public:
    explicit SmallPageAdvanced(TTinyLFU& tiny_lfu) noexcept : tiny_lfu_(tiny_lfu) {
        records_.fill(INVALID_HASH);
    }

#if ENABLE_STATISTICS_FLAG
    double GetFillFactor() const {
        uint32_t cnt = 0;
        for (const auto& r : records_) {
            if (r != INVALID_HASH) {
                ++cnt;
            }
        }
        return cnt / static_cast<double>(records_.size());
    }

    uint64_t GetNumEvictionsHighFreq() const { return num_evictions_high_freq_; }
    uint64_t GetNumDroppedKeysLowFreq() const { return drop_keys_due_low_freq_; }
#endif

    void Clear() noexcept {
        for (auto& r : records_) {
            r = INVALID_HASH;
        }
    }

    void Load(const char* buffer) noexcept {
#if USE_BF_FLAG
        bloom_filter_.Load(file);
#endif

        for (size_t i = 0; i < records_.size(); ++i) {
            utils::BinaryRead(buffer + i * sizeof(Key), &records_[i], sizeof(Key));
        }
    }

    void Store(char* buffer) const noexcept {
#if USE_BF_FLAG
        bloom_filter_.Store(file);
#endif

        for (size_t i = 0; i < records_.size(); ++i) {
            utils::BinaryWrite(buffer + i * sizeof(Key), &records_[i], sizeof(Key));
        }
    }

    bool Get(Key key) noexcept {
#if USE_BF_FLAG
        if (!bloom_filter_.Test(key)) {
            return false;
        }
#endif

#if USE_SIMD_FLAG
        using reg = __m256i;
        const reg kSignedIntMinReg = _mm256_set1_epi32(std::numeric_limits<int32_t>::min());

        reg x = _mm256_set1_epi32(key);
        x = _mm256_subs_epi8(x, kSignedIntMinReg);

        size_t N = records_.size();
        assert(N % 8 == 0);
        for (size_t i = 0; i < N; i += 8) {
            reg y = _mm256_load_si256( (reg*) &records_[i] );
            y = _mm256_subs_epi8(y, kSignedIntMinReg);

            reg m = _mm256_cmpeq_epi32(x, y);
            if (!_mm256_testz_si256(m, m)) {
                size_t mask = _mm256_movemask_ps((__m256) m);
                size_t idx = i + __builtin_ctz(mask);

                tiny_lfu_.Add(key);
                Raise(idx);
                return true;
            }
        }
#else
        for (size_t i = 0; i < records_.size(); ++i) {
            if (records_[i] == key) {
                tiny_lfu_.Add(key);
                Raise(i);
                return true;
            }
        }

#endif
        return false;
    }

    void Update(Key key) noexcept {
        // key не содержится в records_
        // assert(FindIdxOf(key) == INVALID_KEY);

        if (records_.back() == INVALID_HASH) {
            records_.back() = key;
            tiny_lfu_.Add(key);

#if USE_BF_FLAG
            bloom_filter_.Add(key);
#endif

            Raise(records_.size() - 1);
            return;
        }

        auto victim = records_.back();

        auto est_victim = tiny_lfu_.Estimate(victim);
        auto est_key = tiny_lfu_.Estimate(key);
        if (est_victim < est_key) {
            records_.back() = key;
            tiny_lfu_.Add(key);

#if ENABLE_STATISTICS_FLAG
            num_evictions_high_freq_ += (est_victim != 0);
#endif

#if USE_BF_FLAG
            bloom_filter_.Add(key);
#endif

            Raise(records_.size() - 1);
        }
#if ENABLE_STATISTICS_FLAG
        else {
            drop_keys_due_low_freq_++;
        }
#endif
    }

    bool operator==(const SmallPageAdvanced& other) const noexcept {
        return records_ == other.records_;
    }

private:
    void Raise(size_t i) noexcept {  // поднимает запись i в соответствии с частотой
        while (i && tiny_lfu_.Estimate(records_[i - 1]) < tiny_lfu_.Estimate(records_[i])) {
            std::swap(records_[i - 1], records_[i]);
            --i;
        }
    }

    alignas(32) std::array<Key, SMALL_PAGE_SIZE> records_{};

    TTinyLFU& tiny_lfu_;

#if USE_BF_FLAG
    BloomFilter<Key, SMALL_PAGE_SIZE * 6> bloom_filter_{
        [](Key key) { return static_cast<size_t>(key) * 2654435761 % 2^32; },
        [](Key key) {
            key += ~(key << 15);
            key ^= (key >> 10);
            key += (key << 3);
            key ^= (key >> 6);
            key += ~(key << 11);
            key ^= (key >> 16);
            return static_cast<size_t>(key); },
        [](Key key) {
            int c2=0x27d4eb2d;
            key = (key ^ 61) ^ (key >> 16);
            key = key + (key << 3);
            key = key ^ (key >> 4);
            key = key * c2;
            key = key ^ (key >> 15);
            return static_cast<size_t>(key); },
        [](Key key) {
            key = (key + 0x7ed55d16) + (key << 12);
            key = (key ^ 0xc761c23c) ^ (key >> 19);
            key = (key + 0x165667b1) + (key << 5);
            key = (key + 0xd3a2646c) ^ (key << 9);
            key = (key + 0xfd7046c5) + (key << 3);
            key = (key ^ 0xb55a4f09) ^ (key >> 16);
            return static_cast<size_t>(key); }
    };
#endif

#if ENABLE_STATISTICS_FLAG
    uint64_t num_evictions_high_freq_{0};
    uint64_t drop_keys_due_low_freq_{0};
#endif
};

}  // namespace cache
