// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_UTILS_ARRAY_LIST_HPP
#define SRC_UTILS_ARRAY_LIST_HPP

// TODO(zhangzhijian) Replace 'DEBUG_ASSERT' with 'DEBUG_DEBUG' if possible
#ifdef NDEBUG
#define DEBUG_ASSERT(condition)
#else
#include <cassert>
#define DEBUG_ASSERT(condition) assert(condition)
#endif

/// You can enable this macro if you need to view the data in the ArrayList
/// when debugging.
// #define SKITY_DEBUG_ARRAY_LIST

#ifndef SKITY_DEBUG_ARRAY_LIST

#include <utility>
#include <vector>

#include "src/utils/arena_allocator.hpp"

namespace skity {

template <typename T, size_t N>
class ArrayList {
 public:
  struct Node {
    Node* prev = nullptr;
    Node* next = nullptr;
    size_t offset = 0;
    alignas(T) uint8_t storage[sizeof(T) * N];
  };

  class Iterator {
   public:
    Iterator(Node* Node, size_t offset, size_t index)
        : node_(Node), offset_(offset), index_(index) {}

    bool operator==(const Iterator& other) const {
      return index_ == other.index_;
    }

    bool operator!=(const Iterator& other) const { return !(*this == other); }

    T& operator*() const {
      return *reinterpret_cast<T*>(node_->storage + offset_ * sizeof(T));
    }

    Iterator& operator++() {
      if (offset_ == N - 1) {
        node_ = node_->next;
        offset_ = 0;
      } else {
        offset_++;
      }
      index_++;
      return *this;
    }

   private:
    Node* node_;
    size_t offset_;
    size_t index_;
  };

  ArrayList() = default;
  ArrayList(const ArrayList& rhs) = delete;
  ArrayList& operator=(const ArrayList& rhs) = delete;

  ArrayList(ArrayList&& rhs)
      : header_(rhs.header_),
        tail_(rhs.tail_),
        count_(rhs.count_),
        arena_allocator_(rhs.arena_allocator_) {
    rhs.header_ = nullptr;
    rhs.tail_ = nullptr;
    rhs.count_ = 0;
    rhs.arena_allocator_ = nullptr;
  }

  ArrayList& operator=(ArrayList&& rhs) {
    header_ = rhs.header_;
    tail_ = rhs.tail_;
    count_ = rhs.count_;
    arena_allocator_ = rhs.arena_allocator_;
    rhs.tail_ = nullptr;
    rhs.header_ = nullptr;
    rhs.count_ = 0;
    rhs.arena_allocator_ = nullptr;
    return *this;
  }

  ~ArrayList() { reset(); }

  void push_back(const T& value) {
    new (AllocateElement()) T(value);
    count_++;
  }

  void push_back(T&& value) {
    new (AllocateElement()) T(std::move(value));
    count_++;
  }

  template <class... Args>
  void emplace_back(Args&&... args) {
    new (AllocateElement()) T(std::forward<Args>(args)...);
    count_++;
  }

  void pop_back() {
    DEBUG_ASSERT(count_ > 0);
    DEBUG_ASSERT(tail_->offset > 0);
    tail_->offset--;
    if constexpr (!std::is_trivially_destructible<T>::value) {
      T* o = ToPointer(tail_, tail_->offset);
      o->~T();
    }
    count_--;

    if (tail_ != nullptr && tail_->offset == 0) {
      auto prev = tail_->prev;
      DeleteNode(tail_);
      tail_ = prev;
    }

    if (tail_ == nullptr) {
      DEBUG_ASSERT(count_ == 0);
      header_ = nullptr;
    }

    DEBUG_ASSERT(tail_ == nullptr || tail_->offset > 0);
  }

  T& operator[](size_t pos) {
    size_t node_index = pos / N;
    size_t offset = pos % N;
    Node* curr = header_;
    for (size_t i = 0; i < node_index; i++) {
      curr = curr->next;
    }
    T* o = ToPointer(curr, offset);
    return *o;
  }

  constexpr size_t size() const { return count_; }

  constexpr bool empty() const { return count_ == 0; }

  constexpr Iterator begin() const { return Iterator(header_, 0, 0); }

  constexpr Iterator end() const {
    return Iterator(tail_, tail_ ? tail_->offset : 0, count_);
  }

  const T& front() const {
    DEBUG_ASSERT(count_ > 0);
    return *ToPointer(header_, 0);
  }

  const T& back() const {
    DEBUG_ASSERT(count_ > 0);
    DEBUG_ASSERT(tail_->offset > 0);
    return *ToPointer(tail_, tail_->offset - 1);
  }

  T& front() {
    DEBUG_ASSERT(count_ > 0);
    return *ToPointer(header_, 0);
  }

  T& back() {
    DEBUG_ASSERT(count_ > 0);
    DEBUG_ASSERT(tail_->offset > 0);
    return *ToPointer(tail_, tail_->offset - 1);
  }

  void clear() { reset(); }

  void reset() {
    if (count_ == 0) {
      DEBUG_ASSERT(header_ == nullptr);
      DEBUG_ASSERT(tail_ == nullptr);
      return;
    }

    if constexpr (!std::is_trivially_destructible<T>::value) {
      while (count_) {
        pop_back();
      }
    } else {
      while (tail_ != nullptr) {
        Node* prev = tail_->prev;
        DeleteNode(tail_);
        tail_ = prev;
      }

      header_ = nullptr;
      count_ = 0;
    }

    DEBUG_ASSERT(header_ == nullptr);
    DEBUG_ASSERT(tail_ == nullptr);
    DEBUG_ASSERT(count_ == 0);
  }

  ArrayList<T, N> Clone() {
    ArrayList<T, N> result;
    result.arena_allocator_ = arena_allocator_;
    Iterator iterator = begin();
    while (iterator != end()) {
      result.push_back(*iterator);
      ++iterator;
    }
    return result;
  }

  void SetArenaAllocator(ArenaAllocator* arena_allocator) {
    DEBUG_ASSERT(arena_allocator != nullptr);
    DEBUG_ASSERT(count_ == 0);
    arena_allocator_ = arena_allocator;
  }

  Node* GetHeader() const { return header_; }

  Node* GetTail() const { return tail_; }

 private:
  constexpr T* ToPointer(Node* node, size_t offset) {
    return reinterpret_cast<T*>(node->storage + offset * sizeof(T));
  }

  Node* AllocateNode() {
    if (arena_allocator_) {
      return arena_allocator_->Make<Node>();
    }
    return new Node();
  }

  void DeleteNode(Node* node) {
    if (arena_allocator_) {
      return;
    }
    delete node;
  }

  void* AllocateElement() {
    if (tail_ == nullptr) {
      DEBUG_ASSERT(header_ == nullptr);
      DEBUG_ASSERT(count_ == 0);
      header_ = AllocateNode();
      tail_ = header_;
    } else {
      DEBUG_ASSERT(header_ != nullptr);
      DEBUG_ASSERT(tail_ != nullptr);
      DEBUG_ASSERT(tail_->offset <= N);
      DEBUG_ASSERT(count_ > 0);

      if (tail_->offset == N) {
        auto node = AllocateNode();
        tail_->next = node;
        node->prev = tail_;
        tail_ = node;
      }
    }

    DEBUG_ASSERT(header_ != nullptr);
    DEBUG_ASSERT(tail_ != nullptr);
    DEBUG_ASSERT(tail_->offset < N);

    auto result = tail_->storage + tail_->offset * sizeof(T);
    tail_->offset++;
    return result;
  }

  Node* header_ = nullptr;
  Node* tail_ = nullptr;
  size_t count_ = 0;
  ArenaAllocator* arena_allocator_ = nullptr;
};

}  // namespace skity

#else
#include "src/utils/array_list_debug.hpp"
#endif  // SKITY_DEBUG_ARRAY_LIST

#endif  // SRC_UTILS_ARRAY_LIST_HPP
