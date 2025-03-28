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
        static std::array<char, kDataSizeInBytes> buff{};
        file.read(buff.data(), kDataSizeInBytes);

        for (size_t i = 0; i < SMALL_PAGE_NUMBER; ++i) {
            small_pages_[i].Load(buff.data() + i * SmallPage::kDataSizeInBytes);
        }
    }

    void Store(std::ofstream& file) const {
        static std::array<char, kDataSizeInBytes> buff{};
        for (size_t i = 0; i < SMALL_PAGE_NUMBER; ++i) {
            small_pages_[i].Store(buff.data() + i * SmallPage::kDataSizeInBytes);
        }

        file.write(buff.data(), kDataSizeInBytes);
    }

    bool Get(Key key, uint32_t now) noexcept { return small_pages_[SmallPageIndex(key)].Get(key, now); }

    void Update(Key key, uint32_t expiration_time) noexcept {
        small_pages_[SmallPageIndex(key)].Update(key, expiration_time);
    }

#if ENABLE_STATISTICS_FLAG
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

    bool operator==(const LargePage& other) const noexcept {
        for (size_t i = 0; i < SMALL_PAGE_NUMBER; ++i) {
            if (small_pages_[i] != other.small_pages_[i]) return false;
        }
        return true;
    }

private:
    static constexpr std::size_t kDataSizeInBytes =
        SMALL_PAGE_NUMBER * SmallPage::kDataSizeInBytes;

    std::array<SmallPage, SMALL_PAGE_NUMBER> small_pages_;
};

}  // namespace cache
