#pragma once

#include <cache_config.hpp>
#include <utils.hpp>

namespace cache {

inline size_t SmallPageIndex(Key key) noexcept {
    key &= (1ull << (8ull * sizeof(Key) - LARGE_PAGE_SHIFT)) - 1ull;
    return key % SMALL_PAGE_NUMBER;
}

// inline size_t SmallPageIndex(Key key) noexcept {
//     key &= (1ull << (8ull * sizeof(Key) - LARGE_PAGE_SHIFT)) - 1ull;
//     return key >> (8ull * sizeof(Key) - LARGE_PAGE_SHIFT - SMALL_PAGE_SHIFT);
// }

class SmallPageBasic {
public:
    struct Record {
        void Clear() noexcept {
            frequency = 0;
            key = INVALID_HASH;
        }

        void Load(std::ifstream& file) {
            utils::BinaryRead(file, &frequency, sizeof(frequency));
            utils::BinaryRead(file, &key, sizeof(key));
        }

        void Store(std::ofstream& file) const {
            utils::BinaryWrite(file, &frequency, sizeof(frequency));
            utils::BinaryWrite(file, &key, sizeof(key));
        }

        Key frequency{0};
        Key key{INVALID_HASH};
    };

    void Clear() noexcept {
        time_ = 0;
        for (auto& r : records_) {
            r.Clear();
        }
    }

    void Load(std::ifstream& file) {
        utils::BinaryRead(file, &time_, sizeof(time_));
        for (auto& r : records_) {
            r.Load(file);
        }
    }

    void Store(std::ofstream& file) const {
        utils::BinaryWrite(file, &time_, sizeof(time_));
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
    void Update(Key key) noexcept {
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
    void DivFrequency() noexcept {  // делит все частоты на 2, TODO: эту операцию можно сделать отложенной
        for (auto& r : records_) {
            r.frequency >>= 1;
        }
    }

    void Raise(size_t i) noexcept {  // поднимает запись i в соответствии с частотой
        while (i > 0 && records_[i - 1].frequency < records_[i].frequency) {
            std::swap(records_[i - 1], records_[i]);
            --i;
        }
    }

    std::array<Record, SMALL_PAGE_SIZE> records_;
    Key time_{0};
};


}  // namespace cache
