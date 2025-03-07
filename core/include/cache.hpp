#pragma once

#include <cassert>
#include <filesystem>

#include <lru.hpp>
#include <large_page_provider.hpp>
#include <cache_config.hpp>

namespace cache {

class Cache {
public:
    explicit Cache(std::filesystem::path dir_path = "./data")
        : tiny_lfu_(), provider_(dir_path, tiny_lfu_)
#if USE_LRU_FLAG
        , lru_(static_cast<size_t>(LRU_SIZE))
#endif
        {}

    bool Get(Key key) {
#if USE_LRU_FLAG
        if (lru_.Get(key)) return true;
#endif

        auto* maybe_large_page = provider_.Get</*CalledOnUpdate=*/false>(key);

        if (maybe_large_page == nullptr) return false;

        return maybe_large_page->Get(key);
    }

    void Update(Key key) {
#if USE_LRU_FLAG
        auto lru_evicted = lru_.Update(key);
        if (!lru_evicted) return;

        key = *lru_evicted;
#endif

        auto* maybe_large_page = provider_.Get</*CalledOnUpdate=*/true>(key);

        if (maybe_large_page == nullptr) return;

        maybe_large_page->Update(key);
    }

    void Store() const { provider_.Store(); }

private:
    TTinyLFU tiny_lfu_{};
    LargePageProvider provider_;

#if USE_LRU_FLAG
    LRU<uint32_t> lru_;
#endif
};

}  // namespace cache
