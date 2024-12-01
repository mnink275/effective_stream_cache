#include "cache.hpp"

#include <thread>

int main() {
    // директория ./data - пустая
    {
        cache::Cache cache;

        assert(cache.Get(0) == false);
        cache.Update(0);
        assert(cache.Get(0) == true);
        assert(cache.Get(0) == true);

        for (size_t i = 1; i < cache::SMALL_PAGE_SIZE; ++i) {
            assert(cache.Get(i) == false);
            cache.Update(i);
            assert(cache.Get(i) == true);
        }

        // страница заполнена
        assert(cache.Get(256) == false);
        cache.Update(256);
        assert(cache.Get(256) == false);

        for (size_t i = 0; i < cache::KEY_PERIOD; ++i) {
            assert(cache.Get(0) == true);
        }

        // вытесняем ключ
        assert(cache.Get(256) == false);
        cache.Update(256);
        assert(cache.Get(256) == true);

        // вытесняем большую страницу
        assert(cache.Get(1 << 31) == false);

        {
            using namespace std::chrono_literals;  // смотрим, что страница 1 выгрузилась в папку ./data
            std::this_thread::sleep_for(1s);
        }

        cache.Update(1 << 31);
        assert(cache.Get(1 << 31) == true);

        cache.Store();
    }

    {
        cache::Cache cache;

        for (size_t i = 0; i < cache::SMALL_PAGE_SIZE - 1; ++i) {
            assert(cache.Get(i) == true);
        }
        assert(cache.Get(255) == false);
        assert(cache.Get(256) == true);

        assert(cache.Get(1 << 31) == true);
        assert(cache.Get((1 << 31) + 1) == false);

        cache.Store();
    }
}
