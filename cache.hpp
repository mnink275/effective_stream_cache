#pragma once

#include <array>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <algorithm>

#include "lru.hpp"
#include "tiny_lfu_cms.hpp"
#include "bloom_filter.hpp"


#include <immintrin.h>

namespace cache {

/*
TODO:

Сериализировать страницы раз в какой-то период

*/

using Key = uint32_t;
inline const size_t INVALID_HASH = std::numeric_limits<Key>::max();

inline constexpr size_t LARGE_PAGE_SHIFT = 13;
inline constexpr size_t SMALL_PAGE_SHIFT = 8;
inline constexpr size_t SMALL_PAGE_SIZE_SHIFT = 10;

#define USE_LRU_FLAG true
inline constexpr double LRU_SIZE = 50'000;

inline constexpr size_t TLFU_SIZE = 1000;
inline constexpr size_t SAMPLE_SIZE = TLFU_SIZE * 10;

using TTinyLFU = TinyLFU<Key, SAMPLE_SIZE, TLFU_SIZE, false>;

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

inline size_t LargePageIndex(Key key) { return key >> (8ull * sizeof(Key) - LARGE_PAGE_SHIFT); }

inline const size_t SMALL_PAGE_NUMBER = (1 << SMALL_PAGE_SHIFT) + 1;

// inline size_t SmallPageIndex(Key key) {
//     key &= (1ull << (8ull * sizeof(Key) - LARGE_PAGE_SHIFT)) - 1ull;
//     return key >> (8ull * sizeof(Key) - LARGE_PAGE_SHIFT - SMALL_PAGE_SHIFT);
// }

inline size_t SmallPageIndex(Key key) {
    key &= (1ull << (8ull * sizeof(Key) - LARGE_PAGE_SHIFT)) - 1ull;
    return key % SMALL_PAGE_NUMBER;
}

inline const size_t SMALL_PAGE_SIZE = (1 << SMALL_PAGE_SIZE_SHIFT);  // количество записей на странице

inline const size_t LOADED_PAGE_NUMBER = 20;

inline const size_t LARGE_PAGE_PERIOD = 2'000;  // время, через которое частоты больших страниц /= 2

inline const size_t FREQUENCY_THRESHOLD = 370;

inline const size_t CACHE_SIZE = LOADED_PAGE_NUMBER * SMALL_PAGE_NUMBER * SMALL_PAGE_SIZE;

template <typename T>
void ReadFromFile(std::ifstream& file, T& value) {
    file.read((char*)&value, sizeof(T));
}

template <typename T>
void WriteToFile(std::ofstream& file, const T& value) {
    file.write((char*)&value, sizeof(T));
}

class SmallPageBasic {
public:
    struct Record {
        void Clear() {
            frequency = 0;
            key = INVALID_HASH;  // TODO: придумать что-то
        }

        void Load(std::ifstream& file) {
            ReadFromFile(file, frequency);
            ReadFromFile(file, key);
        }

        void Store(std::ofstream& file) const {
            WriteToFile(file, frequency);
            WriteToFile(file, key);
        }

        Key frequency{0};
        Key key{INVALID_HASH};
    };

    void Clear() {
        time_ = 0;
        for (auto& r : records_) {
            r.Clear();
        }
    }

    void Load(std::ifstream& file) {
        ReadFromFile(file, time_);
        for (auto& r : records_) {
            r.Load(file);
        }
    }

    void Store(std::ofstream& file) const {
        WriteToFile(file, time_);
        for (const auto& r : records_) {
            r.Store(file);
        }
    }

    bool Get(Key key) {
        if (time_ == SAMPLE_SIZE) {
            DivFrequency();
            time_ = 0;
        }

        ++time_;

        for (size_t i = 0; i < records_.size(); ++i) {
            if (records_[i].key == key) {
                ++records_[i].frequency;
                Raise(i);
                return true;
            }
        }
        return false;
    }

    // key не содержится в records_
    void Update(Key key) {
        if (time_ == SAMPLE_SIZE) {
            DivFrequency();
            time_ = 0;
        }

        if (records_.back().frequency == 0) {
            records_.back().frequency = 1;
            records_.back().key = key;
            ++time_;

            Raise(records_.size() - 1);
        }
    }

private:
    void DivFrequency() {  // делит все частоты на 2, TODO: эту операцию можно сделать отложенной
        for (auto& r : records_) {
            r.frequency >>= 1;
        }
    }

    void Raise(size_t i) {  // поднимает запись i в соответствии с частотой
        while (i && records_[i - 1].frequency < records_[i].frequency) {
            std::swap(records_[i - 1], records_[i]);
            --i;
        }
    }

    std::array<Record, SMALL_PAGE_SIZE> records_;
    Key time_{0};
};

class SmallPageAdvanced {
public:
    explicit SmallPageAdvanced(TTinyLFU& tiny_lfu) : tiny_lfu_(tiny_lfu) {
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

    void Clear() {
        for (auto& r : records_) {
            r = INVALID_HASH;
        }
    }

    void Load(std::ifstream& file) {
        // ReadFromFile(file, time_);
        tiny_lfu_.Load(file);

#if USE_BF_FLAG
        bloom_filter_.Load(file);
#endif

        for (auto& r : records_) {
            ReadFromFile(file, r);
        }
    }

    void Store(std::ofstream& file) const {
        // WriteToFile(file, time_);
        tiny_lfu_.Store(file);

#if USE_BF_FLAG
        bloom_filter_.Store(file);
#endif

        for (const auto& r : records_) {
            WriteToFile(file, r);
        }
    }

    bool Get(Key key) {
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

    void Update(Key key) {
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

private:
    void Raise(size_t i) {  // поднимает запись i в соответствии с частотой
        while (i && tiny_lfu_.Estimate(records_[i - 1]) < tiny_lfu_.Estimate(records_[i])) {
            std::swap(records_[i - 1], records_[i]);
            --i;
        }
    }

    size_t FindIdxOf(Key key) {
        for (size_t i = 0; i < records_.size(); ++i) {
            if (records_[i] == key) {
                return i;
            }
        }
        return INVALID_HASH;
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

#if USE_TINY_LFU_FLAG
using SmallPage = SmallPageAdvanced;
#else
using SmallPage = SmallPageSimple;
#endif

class LargePage {
public:
    explicit LargePage(TTinyLFU& tiny_lfu)
        : small_pages_(utils::MakeArray<SMALL_PAGE_NUMBER>(SmallPage{tiny_lfu})) {}

    void Clear() {
        for (auto& page : small_pages_) {
            page.Clear();
        }
    }

    void Load(std::ifstream& file) {
        for (auto& page : small_pages_) {
            page.Load(file);
        }
    }

    void Store(std::ofstream& file) const {
        for (const auto& page : small_pages_) {
            page.Store(file);
        }
    }

    bool Get(Key key) { return small_pages_[SmallPageIndex(key)].Get(key); }

    void Update(Key key) {
        auto small_idx = SmallPageIndex(key);
        small_pages_[small_idx].Update(key);
    }

#if ENABLE_STATISTICS
    std::vector<double> GetSmallPagesFillFactors() {
        std::vector<double> res;
        res.reserve(small_pages_.size());
        for (const auto& page : small_pages_) {
            res.push_back(page.GetFillFactor());
        }
        return res;
    }

    uint64_t GetNumEvictionsHighFreq() const {
        uint64_t res = 0;
        for (const auto& page : small_pages_) {
            res += page.GetNumEvictionsHighFreq();
        }
        return res;
    }

    uint64_t GetNumDroppedKeysLowFreq() const {
        uint64_t res = 0;
        for (const auto& page : small_pages_) {
            res += page.GetNumDroppedKeysLowFreq();
        }
        return res;
    }
#endif

private:
    std::array<SmallPage, SMALL_PAGE_NUMBER> small_pages_;
};

class LargePageProvider {
    struct Storage {
        explicit Storage(TTinyLFU& tiny_lfu) : large_pages_(utils::MakeArray<LOADED_PAGE_NUMBER>(LargePage{tiny_lfu})) {}

        std::array<LargePage, LOADED_PAGE_NUMBER> large_pages_;
    };

public:
    LargePageProvider(std::filesystem::path dir_path, TTinyLFU& tiny_lfu) : dir_path_(std::move(dir_path)), storage_(std::make_unique<Storage>(tiny_lfu)) {
        static_assert(LOADED_PAGE_NUMBER <= LARGE_PAGE_NUMBER);
        static_assert(LARGE_PAGE_SHIFT + SMALL_PAGE_SHIFT + SMALL_PAGE_SIZE_SHIFT <= 8 * sizeof(Key));
        assert(std::filesystem::exists(dir_path));
        LoadHeader();

        size_t j = 0;

        for (auto [_, i] : loaded_pages_) {
            // TODO: сделать ленивую загрузку
            page_infos[i].ptr = &(storage_->large_pages_[j++]);
            LoadPage(i);
        }
    }

    template <bool CalledOnUpdate>
    std::optional<LargePage*> Get(Key key) {
        if (time_ == LARGE_PAGE_PERIOD) {
            DivFrequency();
            time_ = 0;
        }

        ++time_;

        size_t i = LargePageIndex(key);
        if (page_infos[i].ptr) {
            auto node = loaded_pages_.extract({page_infos[i].frequency, i});
            page_infos[i].frequency += 1;
            node.value().first = page_infos[i].frequency;
            loaded_pages_.insert(std::move(node));
            return page_infos[i].ptr;
        } else {
            page_infos[i].frequency += 1;

            if (loaded_pages_.begin()->first + FREQUENCY_THRESHOLD < page_infos[i].frequency) {
                size_t worse = loaded_pages_.begin()->second;

                StorePage(worse);

                page_infos[i].ptr = page_infos[worse].ptr;
                page_infos[worse].ptr = nullptr;

                loaded_pages_.erase(loaded_pages_.begin());

                LoadPage(i);

                loaded_pages_.emplace(page_infos[i].frequency, i);
                return page_infos[i].ptr;
            }
#if ENABLE_STATISTICS
            if (CalledOnUpdate) dropped_keys_++;
#endif
        }

        return std::nullopt;
    }

    void Store() const {
        StoreHeader();
        for (size_t i = 0; i < page_infos.size(); ++i) {
            if (page_infos[i].ptr) {
                StorePage(i);
            }
        }
    }

#if ENABLE_STATISTICS_FLAG
    size_t large_page_loads_{0};
    uint64_t dropped_keys_{0};
#endif

    ~LargePageProvider() {
        if constexpr (ENABLE_STATISTICS) PrintStatistics();
    }

private:
    const std::string dir_path_;

    struct LargePageInfo {
        size_t frequency{0};  // всегда < 2*period, можно оптимизировать размер
        LargePage* ptr{nullptr};
    };

    std::filesystem::path GetFilePath(size_t i) const {
        return dir_path_ / std::filesystem::path("page" + std::to_string(i) + ".bin");
    }

    std::filesystem::path GetHeaderPath() const { return dir_path_ / std::filesystem::path("header.bin"); }

    void LoadHeader() {
        const std::filesystem::path file_path = GetHeaderPath();
        loaded_pages_.clear();
        if (std::filesystem::exists(file_path)) {
            std::ifstream file(file_path, std::ios_base::binary);
            for (size_t i = 0; i < page_infos.size(); ++i) {
                ReadFromFile(file, page_infos[i].frequency);
                loaded_pages_.emplace(page_infos[i].frequency, i);
            }
            while (LOADED_PAGE_NUMBER < loaded_pages_.size()) {
                loaded_pages_.erase(loaded_pages_.begin());
            }
        } else {
            while (loaded_pages_.size() < LOADED_PAGE_NUMBER) {
                loaded_pages_.emplace(0, loaded_pages_.size());
            }
        }
    }

    void StoreHeader() const {
        std::ofstream file(GetHeaderPath(), std::ios_base::binary | std::ios_base::trunc);
        for (const auto& page : page_infos) {
            WriteToFile(file, page.frequency);
        }
    }

    void LoadPage(size_t i) {
        assert(page_infos[i].ptr != nullptr);
#if ENABLE_STATISTICS_FLAG
        large_page_loads_++;
#endif

        const std::filesystem::path file_path = GetFilePath(i);
        if (std::filesystem::exists(file_path)) {
            std::ifstream file(file_path, std::ios_base::binary);
            (*page_infos[i].ptr).Load(file);  // TODO: убрать копирование, при большом размере stack-overflow
        } else {
            (*page_infos[i].ptr).Clear();
        }
    }

    void StorePage(size_t i) const {
        assert(page_infos[i].ptr != nullptr);

        std::ofstream file(GetFilePath(i), std::ios_base::binary | std::ios_base::trunc);

        page_infos[i].ptr->Store(file);
    }

    void DivFrequency() {  // делит все частоты на 2
        // TODO: эту операцию можно сделать отложенной и избавиться от loaded_pages_, с помощью дерева отрезков
        loaded_pages_.clear();
        for (size_t i = 0; i < page_infos.size(); ++i) {
            page_infos[i].frequency >>= 1;
            if (page_infos[i].ptr) {
                loaded_pages_.emplace(page_infos[i].frequency, i);
            }
        }
    }

    void PrintStatistics() const {
#if ENABLE_STATISTICS_FLAG
        std::cout << '\n';
        std::cout << "Кол-во свопов больших страниц (RAM <-> диск): " << large_page_loads_ << std::endl;
        std::cout << "Кол-во отброшенных ключей при Update (если соотв. LargePage не загружена в RAM): " << dropped_keys_ << std::endl;

        uint64_t evictions_high_freq = 0;
        for (size_t i = 0; i < LOADED_PAGE_NUMBER; ++i) {
            evictions_high_freq += storage_->large_pages_[i].GetNumEvictionsHighFreq();
        }
        std::cout << "Кол-во вытесненных ключей из страницы (при переполнении): " << evictions_high_freq << std::endl;

        uint64_t dropped_keys_low_freq = 0;
        for (size_t i = 0; i < LOADED_PAGE_NUMBER; ++i) {
            dropped_keys_low_freq += storage_->large_pages_[i].GetNumDroppedKeysLowFreq();
        }
        std::cout << "Кол-во отброшенных ключей при Update (из-за низкой частоты): " << dropped_keys_low_freq << std::endl;

        std::vector<double> fill_factors;
        const size_t SMALL_PAGE_NUM_OVERALL = LOADED_PAGE_NUMBER * SMALL_PAGE_NUMBER;
        fill_factors.reserve(SMALL_PAGE_NUM_OVERALL);
        for (size_t i = 0; i < LOADED_PAGE_NUMBER; ++i) {
            auto factors = storage_->large_pages_[i].GetSmallPagesFillFactors();
            fill_factors.insert(fill_factors.end(), factors.begin(), factors.end());
        }

        const double kSample = 0.1;
        std::vector<size_t> freqs(10, 0);
        for (auto freq : fill_factors) {
            const auto sample_idx = std::min(9ul, static_cast<size_t>(freq / kSample));
            freqs[sample_idx]++;
        }
        std::cout << "Распределение страниц по лоад-фактору после бенчмарка:" << std::endl;
        for (size_t idx = freqs.size(); idx --> 0; ) {
            if (freqs[idx] == 0) continue;
            std::cout << std::fixed << std::setprecision(1) << "[" << idx * kSample << "-" << (idx + 1) * kSample << "): ";
            std::cout << std::fixed  << std::setprecision(2) << 100.0 * freqs[idx] / SMALL_PAGE_NUM_OVERALL << "%" << std::endl;
        }
#endif
    }

    std::unique_ptr<Storage> storage_;
    std::array<LargePageInfo, LARGE_PAGE_NUMBER> page_infos;
    std::set<std::pair<size_t, size_t>> loaded_pages_;  // <частота, индекс большой страницы>
    size_t time_{0};
};

class Cache {
public:
    explicit Cache(std::filesystem::path dir_path = "./data")
        : tiny_lfu_(), provider_(dir_path, tiny_lfu_)
#if USE_LRU_FLAG
        , lru_(static_cast<size_t>(LRU_SIZE))
#endif
        {}

    bool Get(Key key) {
#if USE_LRU_FLAG
        if (lru_.Get(key)) return true;
#endif

        auto maybe_large_page = provider_.Get</*CalledOnUpdate=*/false>(key);

        if (!maybe_large_page.has_value()) return false;

        return maybe_large_page.value()->Get(key);
    }

    void Update(Key key) {
#if USE_LRU_FLAG
        auto lru_evicted = lru_.Update(key);
        if (!lru_evicted) return;

        key = *lru_evicted;
#endif

        auto maybe_large_page = provider_.Get</*CalledOnUpdate=*/true>(key);

        if (!maybe_large_page.has_value()) return;


        maybe_large_page.value()->Update(key);
    }

    void Store() const { provider_.Store(); }

private:
    TTinyLFU tiny_lfu_{};
    LargePageProvider provider_;

#if USE_LRU_FLAG
    LRU<uint32_t> lru_;
#endif
};

}  // namespace cache
