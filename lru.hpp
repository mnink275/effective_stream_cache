#pragma once

#include <boost/intrusive/link_mode.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/list_hook.hpp>

#include <boost/intrusive/unordered_set.hpp>
#include <boost/intrusive/unordered_set_hook.hpp>

// Based on: https://github.com/apolukhin/apolukhin.github.io/blob/master/presentations/C%2B%2B%20Faster.cpp

namespace cache {

namespace details {

using LinkMode = boost::intrusive::link_mode<
#ifdef NDEBUG
    boost::intrusive::normal_link
#else
    boost::intrusive::safe_link
#endif
>;

using ListBaseHook = boost::intrusive::list_base_hook<LinkMode>;
using UnorderedSetBaseHook = boost::intrusive::unordered_set_base_hook<LinkMode>;

template <class Key>
class Node final : public ListBaseHook, public UnorderedSetBaseHook {
 public:
  explicit Node(Key key) : key_(std::move(key)) {}

  const Key& Get() const noexcept { return key_; }
  void Set(Key key) { key_ = std::move(key); }

 private:
  Key key_;
};

template <class SomeKey>
const SomeKey& GetKey(const Node<SomeKey>& node) noexcept {
  return node.Get();
}

template <class T>
const T& GetKey(const T& key) noexcept {
  return key;
}

}  // namespace details

template <class Key, class Hash = std::hash<Key>,
          class Equal = std::equal_to<Key>>
class LRU final {
 public:
  explicit LRU(size_t max_size)
      : buckets_(max_size ? max_size : 1),
        map_(BucketTraits(buckets_.data(), buckets_.size())) {}

  LRU(LRU&& lru) = delete;
  LRU(const LRU& lru) = delete;

  LRU& operator=(LRU&& lru) = delete;
  LRU& operator=(const LRU& lru) = delete;

  ~LRU() {
    while (!list_.empty()) {
      ExtractNode(list_.begin());
    }
  }

  std::optional<Key> Update(Key key) {
    std::optional<Key> evicted_key;
    auto it = map_.find(key, map_.hash_function(), map_.key_eq());
    if (it != map_.end()) {
      list_.splice(list_.end(), list_, list_.iterator_to(*it));
      return std::nullopt;
    }

    if (map_.size() == buckets_.size()) {
      auto node = ExtractNode(list_.begin());
      evicted_key = node->Get();
      node->Set(key);
      InsertNode(std::move(node));
    } else {
      auto node = std::make_unique<LruNode>(Key(key));
      InsertNode(std::move(node));
    }

    return evicted_key;
  }

  bool Get(Key key) {
    auto it = map_.find(key, map_.hash_function(), map_.key_eq());
    if (it == map_.end()) return false;

    list_.splice(list_.end(), list_, list_.iterator_to(*it));
    return true;
  }

 private:
  using LruNode = details::Node<Key>;
  using List = boost::intrusive::list<
      LruNode,
      boost::intrusive::constant_time_size<false>>;

  std::unique_ptr<LruNode> ExtractNode(typename List::iterator it) noexcept {
    std::unique_ptr<LruNode> ret(&*it);
    map_.erase(map_.iterator_to(*it));
    list_.erase(it);
    return ret;
  }

  void InsertNode(std::unique_ptr<LruNode>&& node) noexcept {
    if (!node) return;

    map_.insert(*node);
    list_.insert(list_.end(), *node);

    node.release();
  }

  struct LruNodeHash : Hash {
    template <class NodeOrKey>
    auto operator()(const NodeOrKey& x) const {
      return Hash::operator()(details::GetKey(x));
    }
  };

  struct LruNodeEqual : Equal {
    template <class NodeOrKey1, class NodeOrKey2>
    auto operator()(const NodeOrKey1& x, const NodeOrKey2& y) const {
      return Equal::operator()(details::GetKey(x), details::GetKey(y));
    }
  };

  using Map = boost::intrusive::unordered_set<
      LruNode, boost::intrusive::constant_time_size<true>,
      boost::intrusive::hash<LruNodeHash>,
      boost::intrusive::equal<LruNodeEqual>>;
  using BucketTraits = typename Map::bucket_traits;
  using BucketType = typename Map::bucket_type;

 private:
  std::vector<BucketType> buckets_;
  Map map_;
  List list_;
};

}  // namespace cache
