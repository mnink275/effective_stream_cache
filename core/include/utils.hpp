#pragma once

#include <cassert>
#include <cstdint>
#include <chrono>
#include <array>
#include <cstring>
#include <fstream>

namespace utils {

template <std::size_t ... Is, class T>
std::array<T, sizeof...(Is)> MakeArrayImpl(T&& initer, std::index_sequence<Is...> /*unused*/) {
    return {((void)Is, std::forward<T>(initer))...};
}

template <std::size_t N, class T>
std::array<T, N> MakeArray(T&& initer) {
    return MakeArrayImpl(std::forward<T>(initer), std::make_index_sequence<N>());
}

inline void BinaryWrite(std::ofstream& out, const void* data, size_t size) {
    out.write(reinterpret_cast<const char*>(data), size);
}

inline void BinaryWrite(char* buffer, const void* data, size_t size) noexcept {
    std::memcpy(buffer, data, size);
}

template <class T, size_t N>
requires (std::is_trivial_v<T>)
inline void StoreArrayToBuffer(char* buffer, const std::array<T, N>& data) noexcept {
    for (size_t i = 0; i < N; ++i) {
        BinaryWrite(buffer + i * sizeof(T), &data[i], sizeof(T));
    }
}

inline void BinaryRead(std::ifstream& in, void* data, size_t size) {
    in.read(reinterpret_cast<char*>(data), size);
}

inline void BinaryRead(const char* buffer, void* data, size_t size) noexcept {
    std::memcpy(data, buffer, size);
}

template <class T, size_t N>
requires (std::is_trivial_v<T>)
inline void LoadArrayFromBuffer(const char* buffer, std::array<T, N>& data) noexcept {
    for (size_t i = 0; i < N; ++i) {
        BinaryRead(buffer + i * sizeof(T), &data[i], sizeof(T));
    }
}

inline uint32_t Now() {
    static const auto kTimeSinceEpoch = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto seconds_since_epoch = std::chrono::steady_clock::now().time_since_epoch().count();
    assert(seconds_since_epoch >= 0);
    assert(seconds_since_epoch > kTimeSinceEpoch);
    return seconds_since_epoch - kTimeSinceEpoch;
}

}  // namespace utils
