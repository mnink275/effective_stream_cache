#pragma once

#include <array>

#include <small_page.hpp>
#include <cache_config.hpp>
#include <utils.hpp>

#include <immintrin.h>

namespace cache {

inline size_t LargePageIndex(Key key) noexcept {
    return key >> (8ull * sizeof(Key) - LARGE_PAGE_SHIFT);
}

class LargePage {
public:
    explicit LargePage(TTinyLFU& tiny_lfu)
        : small_pages_(utils::MakeArray<SMALL_PAGE_NUMBER>(SmallPage{tiny_lfu})) {}

    void Clear() noexcept {
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

    bool Get(Key key) noexcept { return small_pages_[SmallPageIndex(key)].Get(key); }

    void Update(Key key) noexcept {
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

}  // namespace cache
