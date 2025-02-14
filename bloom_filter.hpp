#pragma once

#include <cmath>
#include <fstream>
#include <vector>
#include <cstdint>

#include "utils.hpp"

class BloomFilter final {
public:
    BloomFilter(int32_t capacity, double falsePositiveRate)
        : num_bits(std::max(
            1024U,
            utils::NextPowerOf2(static_cast<uint32_t>(capacity * -std::log(falsePositiveRate) / (std::log(2.0) * std::log(2.0)))))
        ),
        num_hash_func_(std::max(2U, static_cast<uint32_t>(0.7 * num_bits / capacity))),
        data_((num_bits + 63) / 64, 0) {}

    bool Add(uint64_t key) {
        const auto h1 = static_cast<uint32_t>(key);
        const auto h2 = static_cast<uint32_t>(key >> 32);
        bool was_added = true;
        for (uint32_t i = 0; i < num_hash_func_; i++) {
            const auto bit = (h1 + (i * h2)) & (num_bits - 1);
            const auto mask = 1ULL << (bit % 64);
            auto& block = data_[bit / 64];
            const auto prev_bit_value = (block & mask) >> (bit % 64);
            block |= mask;
            was_added &= prev_bit_value;
        }

        return was_added;
    }

    bool Test(uint64_t key) {
        const auto h1 = static_cast<uint32_t>(key);
        const auto h2 = static_cast<uint32_t>(key >> 32);
        bool was_added = true;
        for (uint32_t i = 0; i < num_hash_func_; i++) {
            const auto bit = (h1 + (i * h2)) & (num_bits - 1);
            const auto mask = 1ULL << (bit % 64);
            const auto block = data_[bit / 64];
            was_added &= (block & mask) >> (bit % 64);
        }

        return was_added;
    }

    void Clear() {
        std::fill(data_.begin(), data_.end(), 0);
    }

    void Load(std::ifstream& file) {
        file.read(reinterpret_cast<char*>(&num_bits), sizeof(num_bits));
        file.read(reinterpret_cast<char*>(&num_hash_func_), sizeof(num_hash_func_));
        for (auto& byte : data_) {
            file.read(reinterpret_cast<char*>(&byte), sizeof(byte));
        }
    }

    void Store(std::ofstream& file) const {
        file.write(reinterpret_cast<const char*>(&num_bits), sizeof(num_bits));
        file.write(reinterpret_cast<const char*>(&num_hash_func_), sizeof(num_hash_func_));
        for (auto byte : data_) {
            file.write(reinterpret_cast<const char*>(&byte), sizeof(byte));
        }
    }

    bool operator==(const BloomFilter& other) const {
        return num_bits == other.num_bits
            && num_hash_func_ == other.num_hash_func_
            && data_ == other.data_;
    }

private:
    uint32_t num_bits; // size of bit vector in bits
    uint32_t num_hash_func_; // distinct hash functions needed
    std::vector<uint64_t> data_;
};
