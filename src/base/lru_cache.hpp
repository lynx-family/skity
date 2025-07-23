// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_BASE_LRU_CACHE_HPP
#define SRC_BASE_LRU_CACHE_HPP

#include <cstddef>
#include <list>
#include <unordered_map>

#include "src/logging.hpp"

namespace skity {

template <typename K, typename V>
class LRUCache {
 private:
  struct Entry {
    Entry(const K& p_key, V&& p_value)
        : key(p_key), value(std::move(p_value)) {}

    K key;
    V value;
  };

 public:
  struct Hash {
    std::size_t operator()(const K& key) const { return key.hash(); }
  };

  struct Equal {
    bool operator()(const K& lhs, const K& rhs) const { return lhs == rhs; }
  };

 public:
  explicit LRUCache(size_t max_count) : max_count_(max_count) {}

  bool exsit(const K& key) const {
    return cache_map_.find(key) != cache_map_.end();
  }

  V* find(const K& key) {
    auto it = cache_map_.find(key);
    if (it == cache_map_.end()) {
      return nullptr;
    }
    Entry* entry = it->second;
    if (entry != cache_list_.front()) {
      cache_list_.remove(entry);
      cache_list_.push_front(entry);
    }
    return &entry->value;
  }

  V* insert(const K& key, V value) {
    Entry* entry = new Entry(key, std::move(value));
    cache_map_.emplace(key, entry);
    cache_list_.push_front(entry);
    while (cache_map_.size() > max_count_) {
      this->remove(cache_list_.back()->key);
    }
    return &entry->value;
  }

 private:
  void remove(const K& key) {
    auto it = cache_map_.find(key);
    if (it == cache_map_.end()) {
      // error
      DEBUG_CHECK(false);
      return;
    }
    Entry* entry = it->second;
    if (key != entry->key) {
      // error
      DEBUG_CHECK(false);
      return;
    }
    cache_map_.erase(key);
    cache_list_.remove(entry);
    delete entry;
  }

  size_t max_count_;
  std::list<Entry*> cache_list_;
  std::unordered_map<K, Entry*, Hash, Equal> cache_map_;

  LRUCache(const LRUCache&) = delete;
  LRUCache& operator=(const LRUCache&) = delete;
};

}  // namespace skity

#endif  // SRC_BASE_LRU_CACHE_HPP
