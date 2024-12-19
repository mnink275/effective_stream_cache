#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <list>
#include <optional>
#include <unordered_map>

class LRU {
public:
    using Key = uint32_t;
    using It = typename std::list<Key>::iterator;

    explicit LRU(size_t size) : size_(size) {}

    std::optional<Key> UpdateAndEvict(Key key) {
        std::optional<LRU::Key> evicted_key;

        if (const auto it = key_to_node_.find(key); it != key_to_node_.end()) {
            keys_.splice(keys_.begin(), keys_, it->second);
        } else if (keys_.size() == size_) {
            evicted_key = keys_.back();

            keys_.back() = key;
            keys_.splice(keys_.begin(), keys_, std::prev(keys_.end()));

            auto node = key_to_node_.extract(*evicted_key);
            node.key() = key;
            key_to_node_.insert(std::move(node));

            assert(evicted_key);
        } else {
            keys_.push_front(key);
            key_to_node_[key] = keys_.begin();
        }

        assert(key_to_node_[key] == keys_.begin());

        return evicted_key;
    }

    bool Get(Key key) {
        const auto it = key_to_node_.find(key);
        if (it == key_to_node_.end()) return false;

        keys_.splice(keys_.begin(), keys_, it->second);

        assert(key_to_node_[key] == keys_.begin());

        return true;
    }

private:
    const size_t size_;
    std::list<Key> keys_;
    std::unordered_map<Key, It> key_to_node_;
};
