// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_UTILS_ARRAY_LIST_DEBUG_HPP
#define SRC_UTILS_ARRAY_LIST_DEBUG_HPP

#include <utility>
#include <vector>

#include "skity/io/data.hpp"
#include "src/utils/arena_allocator.hpp"

namespace skity {

template <typename T, size_t N>
class ArrayList {
 public:
  using Iterator = typename std::vector<T>::iterator;
  using ConstIterator = typename std::vector<T>::const_iterator;

  ArrayList() { data_.reserve(N); }
  ArrayList(const ArrayList& rhs) = delete;
  ArrayList& operator=(const ArrayList& rhs) = delete;

  ArrayList(ArrayList&& rhs) : data_(std::move(rhs.data_)) {}

  ArrayList& operator=(ArrayList&& rhs) {
    data_ = rhs.data_;
    return *this;
  }

  ~ArrayList() { reset(); }

  void push_back(const T& value) { data_.push_back(value); }

  void push_back(T&& value) { data_.push_back(value); }

  template <class... Args>
  void emplace_back(Args&&... args) {
    data_.emplace_back(std::forward<Args>(args)...);
  }

  void pop_back() { data_.pop_back(); }

  T& operator[](size_t pos) { return data_[pos]; }

  constexpr size_t size() const { return data_.size(); }

  constexpr bool empty() const { return data_.empty(); }

  Iterator begin() { return data_.begin(); }

  Iterator end() { return data_.end(); }

  ConstIterator begin() const { return data_.begin(); }

  ConstIterator end() const { return data_.end(); }

  const T& front() const { return data_.front(); }

  const T& back() const { return data_.back(); }

  T& front() { return data_.front(); }

  T& back() { return data_.back(); }

  void clear() { data_.clear(); }

  void reset() { data_.clear(); }

  ArrayList<T, N> Clone() {
    ArrayList<T, N> result;
    result.data_ = data_;
    return result;
  }

  void SetArenaAllocator(ArenaAllocator* arena_allocator) {}

 private:
  std::vector<T> data_;
};

}  // namespace skity

#endif  // SRC_UTILS_ARRAY_LIST_DEBUG_HPP
