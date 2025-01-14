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

template <class TCache, typename... CacheArgs>
void RunGetBenchmark(const auto& keys, CacheArgs&&... cache_args) {
    using namespace std::chrono_literals;
    const auto beforeBenchmarkRSS =  utils::PrintRSS();

    // std::cout << "Check RSS before" << std::endl;
    // std::this_thread::sleep_for(5s);

    TCache cache(std::forward<CacheArgs>(cache_args)...);

    size_t hitCount = 0;
    size_t totalCount = 0;
    auto start = std::chrono::high_resolution_clock::now();
    for (auto key : keys) {
        if (cache.Get(key)) {
            hitCount++;
        } else {
            cache.Update(key);
        }

        totalCount++;
    }

    const auto benchmarkTime = std::chrono::high_resolution_clock::now() - start;
    // std::cout << "Check RSS after" << std::endl;
    // std::this_thread::sleep_for(5s);

    const auto afterBenchmarkRSS = utils::PrintRSS();
    std::cout << "RSS before: " << beforeBenchmarkRSS << " KB" << std::endl;
    std::cout << "RSS after: " << afterBenchmarkRSS << " KB" << std::endl;
    std::cout << "RSS: " << afterBenchmarkRSS - beforeBenchmarkRSS << " KB" << std::endl;

    std::cout << "Hit ratio: " << 100 * static_cast<double>(hitCount) / totalCount << " %" << std::endl;

    const auto averageTime = std::chrono::duration_cast<std::chrono::nanoseconds>(benchmarkTime).count() / totalCount;
    static const auto CPU_WORKING_CLOCK_GHz = 4.0;
    std::cout << "Get() average time: "
              << averageTime
              << " ns (" << static_cast<uint64_t>(averageTime * CPU_WORKING_CLOCK_GHz) << " ticks)" << std::endl;

    std::cout << std::endl;
}


int main() {
  using namespace std::chrono_literals;
  using TCache = cache::Cache;

  std::cout << "Cache size: " << cache::CACHE_SIZE << '\n';
  std::cout << "Configuration: " << cache::LARGE_PAGE_SHIFT << ' ' << cache::SMALL_PAGE_SHIFT << ' ' << cache::SMALL_PAGE_SIZE_SHIFT << '\n';

//   auto benchmark_keys = LoadFromFile("dataset/Financial1.txt");
//   auto benchmark_keys = LoadFromFile("dataset/WebSearch1.txt");
//   auto benchmark_keys = LoadFromFile("dataset/WebSearch2.txt");
  auto benchmark_keys = LoadFromFile("dataset/big-dataset.txt");

  RunGetBenchmark<TCache>(benchmark_keys);
  RunGetBenchmark<LRU>(benchmark_keys, 5'000'000);
}
