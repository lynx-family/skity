// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_HW_RESOURCE_CACHE_HPP
#define SRC_RENDER_HW_HW_RESOURCE_CACHE_HPP

#include <cstddef>
#include <list>
#include <map>
#include <memory>

namespace skity {

template <typename K, typename V>
class HWResource {
 public:
  virtual ~HWResource() = default;
  virtual const K& GetKey() const = 0;
  virtual V GetValue() const = 0;
  virtual size_t GetBytes() const = 0;
};

template <typename K, typename V>
class HWResourceAllocator {
 public:
  virtual ~HWResourceAllocator() = default;
  virtual std::shared_ptr<HWResource<K, V>> AllocateResource(const K& key) = 0;
};

const size_t kDefaultMaxBytes = 0;

template <typename K, typename V, typename Compare>
class HWResourceCache {
 public:
  class Pool {
   public:
    explicit Pool(HWResourceCache<K, V, Compare>* cache) : cache_(cache) {}
    void PutResource(std::shared_ptr<HWResource<K, V>> resource) {
      resources_.push_back(resource);
    }

    ~Pool() {
      for (auto resource : resources_) {
        cache_->StoreResource(resource);
      }
    }

   private:
    std::vector<std::shared_ptr<HWResource<K, V>>> resources_;
    HWResourceCache<K, V, Compare>* cache_;
  };

  HWResourceCache(std::unique_ptr<HWResourceAllocator<K, V>> allocator,
                  size_t max_bytes = kDefaultMaxBytes)
      : allocator_(std::move(allocator)), max_bytes_(max_bytes) {}

  std::shared_ptr<HWResource<K, V>> ObtainResource(K key,
                                                   Pool* pool = nullptr) {
    auto range = purgeable_map_.equal_range(key);
    if (range.first != range.second) {
      auto list_it = range.first->second;
      auto resource = *list_it;
      purgeable_list_.erase(list_it);
      purgeable_map_.erase(range.first);
      purgeable_bytes_ -= resource->GetBytes();
      if (pool) {
        pool->PutResource(resource);
      }
      return resource;
    }

    std::shared_ptr<HWResource<K, V>> resource =
        allocator_->AllocateResource(key);
    total_resource_bytes_ += resource->GetBytes();
    if (pool) {
      pool->PutResource(resource);
    }
    return resource;
  }

  void StoreResource(std::shared_ptr<HWResource<K, V>> resource) {
    purgeable_list_.push_front(resource);
    purgeable_map_.insert({resource->GetKey(), purgeable_list_.begin()});
    purgeable_bytes_ += resource->GetBytes();
  }

  void PurgeAsNeeded() {
    while (total_resource_bytes_ > max_bytes_ && !purgeable_list_.empty()) {
      std::shared_ptr<HWResource<K, V>> resource = purgeable_list_.back();
      const K& key = resource->GetKey();
      auto range = purgeable_map_.equal_range(key);
      for (auto it = range.first; it != range.second; it++) {
        std::shared_ptr<HWResource<K, V>> r = *(it->second);
        if (resource.get() == r.get()) {
          purgeable_map_.erase(it);
          break;
        }
      }
      purgeable_list_.pop_back();
      total_resource_bytes_ -= resource->GetBytes();
      purgeable_bytes_ -= resource->GetBytes();
    }
  }

  void SetMaxBytes(size_t max_bytes) {
    max_bytes_ = max_bytes;
    PurgeAsNeeded();
  }

  size_t GetTotalResourceBytes() const { return total_resource_bytes_; }
  size_t GetPurgableBytes() const { return purgeable_bytes_; }
  size_t GetMaxbytes() const { return max_bytes_; }

 private:
  size_t total_resource_bytes_ = 0;
  size_t purgeable_bytes_ = 0;
  std::unique_ptr<HWResourceAllocator<K, V>> allocator_;
  size_t max_bytes_;

  std::list<std::shared_ptr<HWResource<K, V>>> purgeable_list_;
  std::multimap<K, decltype(purgeable_list_.begin()), Compare> purgeable_map_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_HW_RESOURCE_CACHE_HPP
