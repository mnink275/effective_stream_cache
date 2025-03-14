#pragma once

#include <filesystem>
#include <set>

#include <large_page.hpp>
#include <cache_config.hpp>

namespace cache {

class LargePageProvider {
    struct Storage {
        explicit Storage(TTinyLFU& tiny_lfu) : large_pages_(utils::MakeArray<LOADED_PAGE_NUMBER>(LargePage{tiny_lfu})) {}

        std::array<LargePage, LOADED_PAGE_NUMBER> large_pages_;
    };

public:
    LargePageProvider(std::filesystem::path dir_path, TTinyLFU& tiny_lfu) : dir_path_(std::move(dir_path)), storage_(std::make_unique<Storage>(tiny_lfu)) {
        static_assert(LOADED_PAGE_NUMBER <= LARGE_PAGE_NUMBER);
        static_assert(LARGE_PAGE_SHIFT + SMALL_PAGE_SHIFT + SMALL_PAGE_SIZE_SHIFT <= 8 * sizeof(Key));
        if (!std::filesystem::exists(dir_path_)) std::filesystem::create_directory(dir_path_);
        LoadHeader();

        size_t j = 0;

        for (auto [_, i] : loaded_pages_) {
            // TODO: сделать ленивую загрузку
            page_infos[i].ptr = &(storage_->large_pages_[j++]);
            LoadPage(i);
        }
    }

    template <bool CalledOnUpdate>
    LargePage* Get(Key key) {
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
        }
        page_infos[i].frequency += 1;

        if (loaded_pages_.begin()->first + FREQUENCY_THRESHOLD < page_infos[i].frequency) {
            const size_t worse = loaded_pages_.begin()->second;
            StorePage(worse);

            page_infos[i].ptr = page_infos[worse].ptr;
            page_infos[worse].ptr = nullptr;
            loaded_pages_.erase(loaded_pages_.begin());
            LoadPage(i);
            loaded_pages_.emplace(page_infos[i].frequency, i);

            return page_infos[i].ptr;
        }
#if ENABLE_STATISTICS_FLAG
        if (CalledOnUpdate) dropped_keys_++;
#endif

        return nullptr;
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
        if constexpr (ENABLE_STATISTICS_FLAG) PrintStatistics();
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
                utils::BinaryRead(file, &page_infos[i].frequency, sizeof(page_infos[i].frequency));
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
            utils::BinaryWrite(file, &page.frequency, sizeof(page.frequency));
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

}  // namespace cache
