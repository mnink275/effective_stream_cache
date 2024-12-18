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

    if (key_to_node_.contains(key)) {
      keys_.erase(key_to_node_[key]);
    } else if (keys_.size() == size_) {
      evicted_key = keys_.back();
      keys_.pop_back();
      assert(evicted_key);
      key_to_node_.erase(*evicted_key);
    }

    keys_.push_front(key);
    key_to_node_[key] = keys_.begin();

    return evicted_key;
  }

  bool Get(Key key) {
    if (!key_to_node_.contains(key)) return false;

    // TODO: reduce allocations from 2 to 1
    keys_.erase(key_to_node_[key]);
    keys_.push_front(key);
    key_to_node_[key] = keys_.begin();

    return true;
  }

 private:
  size_t size_;
  std::list<Key> keys_;
  std::unordered_map<Key, It> key_to_node_;
};
