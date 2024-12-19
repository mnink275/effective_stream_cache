#include <unistd.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <random>

#include "cache_mod.hpp"

// std::vector<uint32_t> ReadKeys(std::filesystem::path file_path) {
//   assert(std::filesystem::exists(file_path));

//   std::ifstream file(file_path);

//   std::vector<uint32_t> res;
//   uint32_t key;
//   while (file >> key) {
//     res.push_back(key);
//   }

//   return res;
// }

// void Test2() {
//   const auto keys = ReadKeys("Financial1.txt");

//   cache::Cache cache;
//   size_t hit = 0;
//   for (uint32_t key : keys) {
//     if (cache.Get(key)) {
//       ++hit;
//     } else {
//       cache.Update(key);
//     }
//   }

//   std::cout << float(hit) / keys.size() << '\n';
// }

// void Test3() {
//   const auto keys = ReadKeys("Financial1.txt");

//   cache::Cache cache;

//   const auto start = std::chrono::steady_clock::now();
//   for (uint32_t key : keys) {
//     if (cache.Get(key)) {
//     } else {
//       cache.Update(key);
//     }
//   }

//   const auto end = std::chrono::steady_clock::now();

//   std::cout << "Cache size: "
//             << sizeof(cache) +
//                    cache::LOADED_PAGE_NUMBER * sizeof(cache::LargePage)
//             << " bytes" << '\n';
//   std::cout << "Working time: "
//             << std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
//             << '\n';
//   std::cout << "Keys number " << keys.size() << '\n';
//   std::cout << "Time per key "
//             << std::chrono::duration_cast<std::chrono::nanoseconds>(end -
//                                                                     start) /
//                    keys.size()
//             << '\n';
// }

namespace utils {

struct Options {
    size_t CacheSize;
    double Mean;
    double Stddev;
    size_t ThreadCount;
    std::optional<size_t> MakeStaleProbability;
};

Options GetOptions(int argc, char** argv) {
    Options options{
        .CacheSize = 0,
        .Mean = 0.0,
        .Stddev = 0.0,
        .ThreadCount = 1,
        .MakeStaleProbability = std::nullopt
    };

    for (int i = 1; i < argc; ++i) {
        auto arg = std::string_view(argv[i]);
        if (arg == "--cache-size" || arg == "-s") {
            options.CacheSize = std::stoull(argv[++i]);
        } else if (arg == "--mean" || arg == "-m") {
            options.Mean = std::stod(argv[++i]);
        } else if (arg == "--stddev" || arg == "-d") {
            options.Stddev = std::stod(argv[++i]);
        } else if (arg == "--thread-count" || arg == "-t") {
            options.ThreadCount = std::stoull(argv[++i]);
        } else if (arg == "--stale-probability" || arg == "-p") {
            options.MakeStaleProbability = std::stoull(argv[++i]);
            if (options.MakeStaleProbability > 100) throw std::runtime_error("Stale probability must be in range [0, 100]");
        } else {
            throw std::runtime_error("Unknown option: " + std::string(argv[i]));
        }
    }

    if (options.CacheSize == 0) {
        throw std::runtime_error("Cache size must be more than 0");
    }

    if (options.Stddev == 0.0) {
        options.Stddev = options.CacheSize;
    }

    return options;
}

// int64_t PrintRSS() {
//     // https://stackoverflow.com/questions/669438/how-to-get-memory-usage-at-runtime-using-c
//     std::ifstream file("/proc/self/statm");
//     if (!file.is_open()) {
//         throw std::runtime_error("Can't open /proc/self/statm");
//     }

//     int tSize = 0;
//     int resident = 0;
//     int share = 0;
//     file >> tSize >> resident >> share;

//     int64_t page_size_kb = sysconf(_SC_PAGE_SIZE) / 1024; // in case x86-64 is configured to use 2MB pages
//     double rss = resident * page_size_kb;
//     return static_cast<int64_t>(rss) / 1024; // in MB
// }

}  // namespace utils

template <class T>
class IKeysGenerator {
public:
    virtual ~IKeysGenerator() = default;
    virtual T NextKey() = 0;
    size_t UniqueCount() const {
        return Freqs.size();
    }

protected:
    T NextKeyImpl(const auto& genKey) {
        return static_cast<T>(genKey);
    }

private:
    std::map<int64_t, size_t> Freqs;
};

template <class T>
class NormalDistributionKeyGenerator final : public IKeysGenerator<T> {
public:
    NormalDistributionKeyGenerator(
        double mean,
        double stddev,
        size_t seed = std::random_device{}())
        : Generator(seed),
          Distribution(mean, stddev) {};

    T NextKey() override {
        auto genKey = std::round(Distribution(Generator));
        return this->NextKeyImpl(genKey);
    }

private:
    std::mt19937 Generator;
    std::normal_distribution<double> Distribution;
};

class UniformStaleItemDecider final {
public:
    UniformStaleItemDecider(size_t seed)
        : Generator(seed),
          Distribution(1, 100) {};

    size_t NextKey() {
        return Distribution(Generator);
    }

private:
    std::mt19937 Generator;
    std::uniform_int_distribution<size_t> Distribution;
};

template <class T, class TCache>
class CacheFixture {
public:
    CacheFixture(size_t /* cacheSize */, auto& keys, size_t seed, double mean, double stddev, std::optional<size_t> staleProbability)
        : Cache(),
          KeysGenerator(mean, stddev, seed),
          MakeStaleProbability(staleProbability),
          StaleItemDecider(seed) {
        // make keys unique
        keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
        for (auto key : keys) {
            Cache.Update(T{key});
        }
        // Cache.Store();

        // std::cout << "Cache fullness: " << keys.size() << " / " << cache::CACHE_SIZE << " = " << 100 * keys.size() / cache::CACHE_SIZE << " %" << std::endl;
    }

    bool Get(auto&& key) {
        return Cache.Get(key);
    }

    void Update(auto&& key) {
        Cache.Update(key);
    }

    bool Get() {
        auto key = KeysGenerator.NextKey();
        // if (!MakeStaleProbability) {
        //     auto value = Cache.Get(T{key});
        //     return;
        // }
        return Cache.Get(T{key});

        // bool make_stale = (StaleItemDecider.NextKey() < MakeStaleProbability);
        // // Cache.Prune(now);
        // if (make_stale) {
        //     static const auto LARGE_DURATION = std::chrono::hours(1);

        //     T new_key = key;
        //     auto res = Cache.Get(T{key}, now + LARGE_DURATION);
        //     assert(!res);
        //     Cache.Update(new_key.Release());
        // } else {
        //     auto value = Cache.Get(T{key}, now);
        //     Y_DO_NOT_OPTIMIZE_AWAY(value);
        // }
    }

private:
    TCache Cache;

    NormalDistributionKeyGenerator<T> KeysGenerator;
    std::optional<size_t> MakeStaleProbability;
    UniformStaleItemDecider StaleItemDecider;
};

std::vector<int64_t> LoadFromFile(const std::string& filename) {
    std::ifstream input(filename);
    if (!input.is_open()) {
        throw std::runtime_error(std::string{"Filed to open file: "} + filename);
    }

    std::set<int64_t> unique;

    std::vector<int64_t> keys;
    keys.reserve(1'000'000);
    while (!input.eof()) {
        int64_t key;
        input >> key;
        unique.insert(key);
        keys.emplace_back(key);
    }

    // std::cout << "Total keys count: " << keys.size() << std::endl;
    std::cout << "Unique keys count: " << unique.size() << std::endl;

    return keys;
}


template <class T, class TCache>
void RunGetBenchmark(auto& keys, utils::Options options, size_t seed = std::random_device{}()) {
    using namespace std::chrono_literals;
    // const auto beforeBenchmarkRSS =  utils::PrintRSS();
    CacheFixture<T, TCache> cacheFixture{options.CacheSize, keys, seed, options.Mean, options.Stddev, options.MakeStaleProbability};


    auto benchmark_keys = LoadFromFile("WebSearch1.txt");

    size_t hitCount = 0;
    size_t totalCount = 0;
    auto start = std::chrono::high_resolution_clock::now();
    // const auto periodDuration = 100ms;
    // auto period_start = start;
    // size_t hot_key_eviction_count = 0;
    for (auto& key : benchmark_keys) {
        if (cacheFixture.Get(key)) {
            hitCount++;
        } else {
            // if (key == 6112) hot_key_eviction_count++;
            cacheFixture.Update(key);
        }

        totalCount++;

        // if (std::chrono::high_resolution_clock::now() - period_start > periodDuration) {
        //     period_start = std::chrono::high_resolution_clock::now();
        //     std::cout << "Hit ratio: " << static_cast<double>(hitCount) / totalCount << std::endl;
        // }
    }

    // std::cout << "Hot key eviction count: " << hot_key_eviction_count << std::endl;

    const auto benchmarkTime = std::chrono::high_resolution_clock::now() - start;

    // const auto afterBenchmarkRSS = utils::PrintRSS();
    // std::cout << "RSS before: " << beforeBenchmarkRSS << " MB" << std::endl;
    // std::cout << "RSS after: " << afterBenchmarkRSS << " MB" << std::endl;
    // std::cout << "RSS: " << afterBenchmarkRSS - beforeBenchmarkRSS << " MB" << std::endl;

    // std::cout << "Data seed: " << seed << std::endl;
    // std::cout << "Benchmark duration: " << std::chrono::duration_cast<std::chrono::milliseconds>(benchmarkTime).count() << " ms" << std::endl;

    // std::cout << "Total count: " << totalCount << std::endl;
    // std::cout << "Hit count: " << hitCount << std::endl;
    std::cout << "Hit ratio: " << static_cast<double>(hitCount) / totalCount << std::endl;

    const auto averageTime = options.ThreadCount * std::chrono::duration_cast<std::chrono::nanoseconds>(benchmarkTime).count() / totalCount;
    static const auto CPU_WORKING_CLOCK_GHz = 4.0;
    std::cout << "Get() average time: "
              << averageTime
              << " ns (" << static_cast<uint64_t>(averageTime * CPU_WORKING_CLOCK_GHz) << " ticks)" << std::endl;

    std::cout << std::endl;
}


int main(int argc, char** argv) {
  using namespace std::chrono_literals;
  using TCache = cache::Cache;

  auto options = utils::GetOptions(argc, argv);

  std::vector<int64_t> intCacheInitData;

//   std::cout << "Cache size: " << cache::CACHE_SIZE << '\n';
  std::cout << "Configuration: " << cache::LARGE_PAGE_SHIFT << ' ' << cache::SMALL_PAGE_SHIFT << ' ' << cache::SMALL_PAGE_SIZE_SHIFT << '\n';
  const size_t intSeed =
      std::random_device{}();  // seed for both benchmarks must be the same
  RunGetBenchmark<int64_t, TCache>(intCacheInitData,
                                   options, intSeed);
}
