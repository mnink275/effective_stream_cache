#pragma once

#include <array>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <algorithm>

namespace cache {

/*
TODO:

Сериализировать страницы раз в какой-то период

*/

using Key = uint32_t;
inline const size_t INVALID_KEY = std::numeric_limits<Key>::max();

inline const size_t LARGE_PAGE_SHIFT = 4;
inline const size_t SMALL_PAGE_SHIFT = 8;
inline const size_t SMALL_PAGE_SIZE_SHIFT = 8;

inline const size_t LARGE_PAGE_NUMBER = 1 << LARGE_PAGE_SHIFT;

inline size_t LargePageIndex(Key key) { return key >> (8ull * sizeof(Key) - LARGE_PAGE_SHIFT); }
// inline size_t LargePageIndex(Key key) { return key % LARGE_PAGE_NUMBER; }

inline const size_t SMALL_PAGE_NUMBER = 1 << SMALL_PAGE_SHIFT;

// inline size_t SmallPageIndex(Key key) {
//     key &= (1ull << (8ull * sizeof(Key) - LARGE_PAGE_SHIFT)) - 1ull;
//     return key >> (8ull * sizeof(Key) - LARGE_PAGE_SHIFT - SMALL_PAGE_SHIFT);
// }
inline size_t SmallPageIndex(Key key) {
    key &= (1ull << (8ull * sizeof(Key) - LARGE_PAGE_SHIFT)) - 1ull;
    return key % SMALL_PAGE_NUMBER;
}

inline const size_t SMALL_PAGE_SIZE = 1 << SMALL_PAGE_SIZE_SHIFT;  // количество записей на странице

inline const size_t LOADED_PAGE_NUMBER = LARGE_PAGE_NUMBER;

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
        frequency = 0;
        key = INVALID_KEY;   // TODO: придумать что-то
    }

    void Load(std::ifstream& file) {
        ReadFromFile(file, frequency);
        ReadFromFile(file, key);
    }

    void Store(std::ofstream& file) const {
        WriteToFile(file, frequency);
        WriteToFile(file, key);
    }

    size_t frequency;
    Key key;
};

class SmallPage {
public:
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
        if (time_ == KEY_PERIOD) {
            DivFrequency();
            time_ = 0;
        }

        ++time_;

        for (size_t i = 0; i < records_.size(); ++i) {
            if (records_[i].key == key) {
                Raise(i);
                return true;
            }
        }
        return false;
    }

    // key не содержится в records_
    void Update(Key key) {
        if (time_ == KEY_PERIOD) {
            DivFrequency();
            time_ = 0;
        }

        if (records_.back().frequency == 0) {
            records_.back().frequency = 1;
            records_.back().key = std::move(key);
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
    size_t time_{0};
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

    void Update(Key key) { small_pages_[SmallPageIndex(key)].Update(key); }

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
    Cache(std::filesystem::path dir_path = "./data") : provider_(dir_path) {}

    bool Get(Key key) {
        auto maybe_large_page = provider_.Get(key);

        if (!maybe_large_page.has_value()) return false;

        return maybe_large_page.value()->Get(key);
    }

    void Update(Key key) {
        auto maybe_large_page = provider_.Get(key);

        if (!maybe_large_page.has_value()) return;

        return maybe_large_page.value()->Update(key);
    }

    void Store() const { provider_.Store(); }

private:
    LargePageProvider provider_;
};

}  // namespace cache
