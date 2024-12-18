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
#include "crc32.hpp"

namespace cache {

/*
TODO:

Сериализировать страницы раз в какой-то период

*/

using Key = uint32_t;
inline const size_t INVALID_HASH = std::numeric_limits<Key>::max();

inline const size_t LARGE_PAGE_SHIFT = 1;
inline const size_t SMALL_PAGE_SHIFT = 9;
inline const size_t SMALL_PAGE_SIZE_SHIFT = 9;
inline constexpr bool USE_LRU = true;

inline const size_t LARGE_PAGE_NUMBER = 1 << LARGE_PAGE_SHIFT;

inline size_t LargePageIndex(Key key) { return key >> (8ull * sizeof(Key) - LARGE_PAGE_SHIFT); }
// inline size_t LargePageIndex(Key key) { return key % LARGE_PAGE_NUMBER; }

inline const size_t SMALL_PAGE_NUMBER = (1 << SMALL_PAGE_SHIFT) + 1;

// inline size_t SmallPageIndex(Key key) {
//     key &= (1ull << (8ull * sizeof(Key) - LARGE_PAGE_SHIFT)) - 1ull;
//     return key >> (8ull * sizeof(Key) - LARGE_PAGE_SHIFT - SMALL_PAGE_SHIFT);
// }

inline size_t SmallPageIndex(Key key) {
    key &= (1ull << (8ull * sizeof(Key) - LARGE_PAGE_SHIFT)) - 1ull;
    return key % SMALL_PAGE_NUMBER;
}

inline const size_t SMALL_PAGE_SIZE = (1 << SMALL_PAGE_SIZE_SHIFT) + 1;  // количество записей на странице

// inline const size_t LOADED_PAGE_NUMBER = std::max(LARGE_PAGE_NUMBER / 2, 1ul);
inline const size_t LOADED_PAGE_NUMBER = 1;

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

struct Record {
    Record() {
        Clear();
    }

    void Clear() {
        key = INVALID_HASH;   // TODO: придумать что-то
    }

    void Load(std::ifstream& file) {
        ReadFromFile(file, key);
    }

    void Store(std::ofstream& file) const {
        WriteToFile(file, key);
    }

    Key key;
};

class SmallPage {
public:
    void Clear() {
        for (auto& r : records_) {
            r.Clear();
        }
    }

    void Load(std::ifstream& file) {
        // ReadFromFile(file, time_);
        tiny_lfu_.Load(file);
        for (auto& r : records_) {
            r.Load(file);
        }
    }

    void Store(std::ofstream& file) const {
        // WriteToFile(file, time_);
        tiny_lfu_.Store(file);
        for (const auto& r : records_) {
            r.Store(file);
        }
    }

    bool Get(Key key) {
        // if (time_ == KEY_PERIOD) {
        //     DivFrequency();
        //     time_ = 0;
        // }

        if (!bloom_filter_[key % SMALL_PAGE_SIZE]) {
            return false;
        }

        for (size_t i = 0; i < records_.size(); ++i) {
            if (records_[i].key == key) {
                tiny_lfu_.Add(key);
                Raise(i);
                return true;
            }
        }
        return false;
    }

    void Update(Key key) {
        // key не содержится в records_
        // assert(FindIdxOf(key) == INVALID_KEY);

        if (records_.back().key == INVALID_HASH) {
            records_.back().key = key;
            tiny_lfu_.Add(key);
            Raise(records_.size() - 1);
            return;
        }

        auto victim = records_.back().key;

        auto est_victim = tiny_lfu_.Estimate(victim);
        auto est_key = tiny_lfu_.Estimate(key);
        if (est_victim < est_key) {
            records_.back().key = key;
            tiny_lfu_.Add(key);

            Raise(records_.size() - 1);
        } else {
            // bloom_filter_[victim % SMALL_PAGE_SIZE] = false;
        }

        bloom_filter_[key % SMALL_PAGE_SIZE] = true;
    }

private:
    void Raise(size_t i) {  // поднимает запись i в соответствии с частотой
        while (i && tiny_lfu_.Estimate(records_[i - 1].key) < tiny_lfu_.Estimate(records_[i].key)) {
            std::swap(records_[i - 1], records_[i]);
            --i;
        }
    }

    size_t FindIdxOf(Key key) {
        for (size_t i = 0; i < records_.size(); ++i) {
            if (records_[i].key == key) {
                return i;
            }
        }
        return INVALID_HASH;
    }

    std::array<Record, SMALL_PAGE_SIZE> records_{};

    TinyLFU<Key, SMALL_PAGE_SIZE / 10, KEY_PERIOD> tiny_lfu_{
        [](Key key) { return static_cast<size_t>(key) * 2654435761 % 2^32; },
        // [](Key key) { return static_cast<size_t>(key); },
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

    std::bitset<SMALL_PAGE_SIZE> bloom_filter_;
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
        // small_page_freq_stat_[small_idx]++;
        small_pages_[small_idx].Update(key);
    }

    ~LargePage() {
        // std::vector<std::pair<size_t, size_t>> sorted;
        // for (size_t i = 0; i < SMALL_PAGE_NUMBER; ++i) {
        //     if (small_page_freq_stat_[i] == 0) continue;
        //     sorted.emplace_back(small_page_freq_stat_[i], i);
        // }

        // std::sort(sorted.begin(), sorted.end(), std::greater<>());
        // for (auto [count, idx] : sorted) {
        //     std::cout << idx << ": " << count << '\n';
        // }
        // std::cout << "Used pages number: " << sorted.size() << '\n';
        // std::cout << '\n';
    }

private:
    std::array<SmallPage, SMALL_PAGE_NUMBER> small_pages_;
    // std::array<size_t, SMALL_PAGE_NUMBER> small_page_freq_stat_{};
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
        // calls_++;
    }

    void Store() const { provider_.Store(); }

    ~Cache() {
        // std::cout << calls_ << '\n';
    }

private:
    LargePageProvider provider_;
    LRU lru_;
    // size_t calls_{0};
};

}  // namespace cache
