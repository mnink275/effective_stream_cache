#pragma once

#include <array>
#include <bitset>
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
#include "tiny_lfu.hpp"
#include "bloom_filter.hpp"


#include <immintrin.h>

namespace cache {

/*
TODO:

Сериализировать страницы раз в какой-то период

*/

using Key = uint32_t;
inline const size_t INVALID_HASH = std::numeric_limits<Key>::max();

inline const size_t LARGE_PAGE_SHIFT = 13;
inline const size_t SMALL_PAGE_SHIFT = 8;
inline const size_t SMALL_PAGE_SIZE_SHIFT = 8;
inline constexpr bool USE_LRU = true;
inline constexpr bool USE_BF = true;
inline constexpr bool USE_SIMD = true;

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

// inline const size_t LOADED_PAGE_NUMBER = LARGE_PAGE_NUMBER / 2;
inline const size_t LOADED_PAGE_NUMBER = 75;

inline const size_t LARGE_PAGE_PERIOD = 10000;  // время, через которое частоты больших страниц /= 2
inline const size_t KEY_PERIOD = 2000;  // время, через которое частоты ключей /= 2

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


class SmallPage {
public:
    SmallPage() {
        records_.fill(INVALID_HASH);
    }

    void Clear() {
        for (auto& r : records_) {
            r = INVALID_HASH;
        }
    }

    void Load(std::ifstream& file) {
        // ReadFromFile(file, time_);
        tiny_lfu_.Load(file);
        bloom_filter_.Load(file);
        for (auto& r : records_) {
            ReadFromFile(file, r);
        }
    }

    void Store(std::ofstream& file) const {
        // WriteToFile(file, time_);
        tiny_lfu_.Store(file);
        bloom_filter_.Store(file);
        for (const auto& r : records_) {
            WriteToFile(file, r);
        }
    }

    bool Get(Key key) {
        // if (time_ == KEY_PERIOD) {
        //     DivFrequency();
        //     time_ = 0;
        // }

        if (USE_BF && !bloom_filter_.Test(key)) {
            return false;
        }


        if (USE_SIMD) {
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
        } else {
            for (size_t i = 0; i < records_.size(); ++i) {
                if (records_[i] == key) {
                    tiny_lfu_.Add(key);
                    Raise(i);
                    return true;
                }
            }
        }

        return false;
    }

    void Update(Key key) {
        // key не содержится в records_
        // assert(FindIdxOf(key) == INVALID_KEY);

        if (records_.back() == INVALID_HASH) {
            records_.back() = key;
            tiny_lfu_.Add(key);
            if (USE_BF) bloom_filter_.Add(key);

            Raise(records_.size() - 1);
            return;
        }

        auto victim = records_.back();

        auto est_victim = tiny_lfu_.Estimate(victim);
        auto est_key = tiny_lfu_.Estimate(key);
        if (est_victim < est_key) {
            records_.back() = key;
            tiny_lfu_.Add(key);
            if (USE_BF) bloom_filter_.Add(key);

            Raise(records_.size() - 1);
        }
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

    alignas(32) std::array<uint32_t, SMALL_PAGE_SIZE> records_{};

    TinyLFU<Key, SMALL_PAGE_SIZE / 10, KEY_PERIOD> tiny_lfu_{
        [](Key key) { return static_cast<size_t>(key) * 2654435761 % 2^32; },
        [](Key i32key) {
            uint32_t key = i32key;
            key += ~(key<<15);
            key ^=  (key>>10);
            key +=  (key<<3);
            key ^=  (key>>6);
            key += ~(key<<11);
            key ^=  (key>>16);
            return static_cast<size_t>(key); }
    };

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
};

class LargePage {
public:
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

private:
    std::array<SmallPage, SMALL_PAGE_NUMBER> small_pages_;
};

class LargePageProvider {
    using Storage = std::array<LargePage, LOADED_PAGE_NUMBER>;

public:
    LargePageProvider(std::filesystem::path dir_path) : dir_path_(std::move(dir_path)), storage_(new Storage) {
        static_assert(LOADED_PAGE_NUMBER <= LARGE_PAGE_NUMBER);
        static_assert(LARGE_PAGE_SHIFT + SMALL_PAGE_SHIFT + SMALL_PAGE_SIZE_SHIFT <= 8 * sizeof(Key));
        assert(std::filesystem::exists(dir_path));
        LoadHeader();

        size_t j = 0;

        for (auto [_, i] : loaded_pages_) {
            // TODO: сделать ленивую загрузку
            page_infos[i].ptr = &((*storage_)[j++]);
            LoadPage(i);
        }
    }

    std::optional<LargePage*> Get(Key key) {
        if (time_ == LARGE_PAGE_PERIOD) {
            DivFrequency();
            time_ = 0;
        }

        ++time_;

        size_t i = LargePageIndex(key);
        if (page_infos[i].ptr) {
            loaded_pages_.erase({page_infos[i].frequency, i});
            page_infos[i].frequency += 1;
            loaded_pages_.emplace(page_infos[i].frequency, i);
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
        }

        assert(false);
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

    ~LargePageProvider() { delete storage_; }

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

    Storage* const storage_;
    std::array<LargePageInfo, LARGE_PAGE_NUMBER> page_infos;
    std::set<std::pair<size_t, size_t>> loaded_pages_;  // <частота, индекс большой страницы>
    size_t time_{0};
};

class Cache {
public:
    Cache(std::filesystem::path dir_path = "./data") : provider_(dir_path), lru_(static_cast<size_t>(CACHE_SIZE * 0.05)) {}

    bool Get(Key key) {
        if (USE_LRU) {
            if (lru_.Get(key)) return true;
        }

        auto maybe_large_page = provider_.Get(key);

        if (!maybe_large_page.has_value()) return false;

        return maybe_large_page.value()->Get(key);
    }

    void Update(Key key) {
        if (USE_LRU) {
            auto lru_evicted = lru_.UpdateAndEvict(key);
            if (!lru_evicted) return;

            key = *lru_evicted;
        }

        auto maybe_large_page = provider_.Get(key);

        assert(maybe_large_page.has_value());
        if (!maybe_large_page.has_value()) return;


        maybe_large_page.value()->Update(key);
    }

    void Store() const { provider_.Store(); }

private:
    LargePageProvider provider_;
    LRU lru_;
};

}  // namespace cache
