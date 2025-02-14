#include <unistd.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <unordered_set>
#include <thread>

#include "cache.hpp"

namespace utils {

int64_t PrintRSS() {
    // https://stackoverflow.com/questions/669438/how-to-get-memory-usage-at-runtime-using-c
    std::ifstream file("/proc/self/statm");
    if (!file.is_open()) {
        throw std::runtime_error("Can't open /proc/self/statm");
    }

    int tSize = 0;
    int resident = 0;
    int share = 0;
    file >> tSize >> resident >> share;

    int64_t page_size_kb = sysconf(_SC_PAGE_SIZE) / 1024; // in case x86-64 is configured to use 2MB pages
    double rss = resident * page_size_kb;
    return static_cast<int64_t>(rss); // in KB
}

}  // namespace utils

std::vector<uint32_t> unique(std::vector<uint32_t> arr) {
    std::sort(arr.begin(), arr.end());
    arr.resize(std::unique(arr.begin(), arr.end()) - arr.begin());
    return arr;
}

std::vector<uint32_t> LoadFromFile(const std::string& filename) {
    std::ifstream input(filename);
    if (!input.is_open()) {
        throw std::runtime_error(std::string{"Filed to open file: "} + filename);
    }

    std::vector<uint32_t> keys;
    keys.reserve(1'000'000);

    for (uint32_t key; input >> key;) {
        keys.emplace_back(key);
    }

    std::cout << "Total keys count: " << keys.size() << std::endl;
    std::cout << "Unique keys count: " << unique(keys).size() << std::endl;

    return keys;
}

struct BenchmarkResult {
    static constexpr auto CPU_WORKING_CLOCK_GHz = 4.0;

    size_t hitCount;
    size_t totalCount;
    std::chrono::nanoseconds benchmarkTime;
    std::chrono::nanoseconds updatesTime;
    int64_t RSS;

    void Print() const {
        std::cout << "RSS: " << RSS / 1024.0 << " MB" << std::endl;
        std::cout << "Hit ratio: " << (100 * static_cast<double>(hitCount) / totalCount)  << " %" << std::endl;

        const auto opAverageTime = std::chrono::duration_cast<std::chrono::nanoseconds>(benchmarkTime).count() / totalCount;
        std::cout << "Operation average time: "
                  << opAverageTime << std::endl;

        const auto getAverageTime = std::chrono::duration_cast<std::chrono::nanoseconds>(benchmarkTime - updatesTime).count() / totalCount;
        std::cout << "Get average time: "
                  << getAverageTime << std::endl;
    }

    BenchmarkResult& operator+=(const BenchmarkResult& other) {
        hitCount += other.hitCount;
        totalCount += other.totalCount;
        benchmarkTime += other.benchmarkTime;
        updatesTime += other.updatesTime;
        RSS += other.RSS;
        return *this;
    }
};

template <class TCache, typename... CacheArgs>
BenchmarkResult RunBenchmark(const auto& keys, TCache& cache) {
    using namespace std::chrono_literals;
    const auto beforeBenchmarkRSS =  utils::PrintRSS();

    size_t hitCount = 0;
    size_t totalCount = 0;

    auto updates_time = 0ns;
    auto start = std::chrono::high_resolution_clock::now();
    for (auto key : keys) {
        if (cache.Get(key)) {
            hitCount++;
        } else {
            auto start_update = std::chrono::high_resolution_clock::now();
            cache.Update(key);
            updates_time += std::chrono::high_resolution_clock::now() - start_update;
        }

        totalCount++;
    }

    const auto benchmarkTime = std::chrono::high_resolution_clock::now() - start;

    return BenchmarkResult{hitCount, totalCount, benchmarkTime, updates_time, utils::PrintRSS() - beforeBenchmarkRSS};
}


int main() {
    using namespace std::chrono_literals;
    using namespace cache;

#define PAGE_BASED_CACHE true

#if PAGE_BASED_CACHE
    std::cout << "Cache size: " << CACHE_SIZE << '\n';
    std::cout << "Configuration: "
        << LARGE_PAGE_SHIFT << ' '
        << SMALL_PAGE_SHIFT << ' '
        << SMALL_PAGE_SIZE_SHIFT << ' '
        << "(" << LOADED_PAGE_NUMBER << " loaded)"
    << '\n';

    if (USE_TINY_LFU_FLAG) std::cout << "TinyLFU " << "(" << TLFU_SIZE << ", " << SAMPLE_SIZE << ")\n";
    else std::cout << "Keys Sample Size: " << SAMPLE_SIZE << '\n';
    if (USE_LRU) std::cout << "LRU " << (USE_LRU ? LRU_MULTIPLIER * 100 : 0) << "%" << std::endl;
    if (USE_BF) std::cout << "Bloom filter " << (USE_BF ? "ON" : "OFF") << std::endl;
    if (USE_SIMD) std::cout << "SIMD " << (USE_SIMD ? "ON" : "OFF") << std::endl;
#endif

    const size_t kBatchSize = 500'000'000;
    std::vector<uint32_t> benchmark_keys;
    benchmark_keys.reserve(kBatchSize);

    BenchmarkResult total_result{0, 0, 0ns, 0ns, 0};

    std::ifstream input("dataset/Financial1.txt");
    if (!input.is_open()) {
        throw std::runtime_error("Can't open file");
    }

    const auto beforeCacheInitRSS = utils::PrintRSS();

#if PAGE_BASED_CACHE
    using TCache = Cache;
    TCache cache;
#else
    using TCache = LRU;
    // const size_t CACHE_SIZE = 131584;
    // const size_t CACHE_SIZE = 524288;
    const size_t CACHE_SIZE = 5'000'000;
    TCache cache{CACHE_SIZE};
#endif

    total_result.RSS += utils::PrintRSS() - beforeCacheInitRSS;

    for (uint32_t key{}; input >> key;) {
        benchmark_keys.emplace_back(key);
        if (benchmark_keys.size() == kBatchSize) {
            auto result = RunBenchmark<TCache>(benchmark_keys, cache);

            total_result += result;

            benchmark_keys.clear();
            std::cout << "Batch handled" << std::endl;
        }
    }
    if (!benchmark_keys.empty()) {
        auto result = RunBenchmark<TCache>(benchmark_keys, cache);
        total_result += result;
        std::cout << "Batch handled" << std::endl;
    }

    total_result.Print();
}
