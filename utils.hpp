#pragma once

#include <cstdint>
#include <array>
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

inline void BinaryRead(std::ifstream& in, void* data, size_t size) {
    in.read(reinterpret_cast<char*>(data), size);
}

}  // namespace utils
