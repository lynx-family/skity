// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_UTILS_VECTOR_CACHE_HPP
#define SRC_UTILS_VECTOR_CACHE_HPP

#include <vector>

namespace skity {

template <typename T>
class VectorCache {
 public:
  struct VectorHolder {
    bool is_available = false;
    std::vector<T> vector;
  };

  std::vector<T>& ObtainVector() {
    for (VectorHolder& holder : holders_) {
      if (holder.is_available) {
        holder.is_available = false;
        holder.vector.clear();
        return holder.vector;
      }
    }

    holders_.emplace_back();
    return holders_.back().vector;
  }

  void StoreVector(std::vector<T>& vector) {
    for (VectorHolder& holder : holders_) {
      if (&holder.vector == &vector) {
        holder.is_available = true;
      }
    }
  }

 private:
  std::vector<VectorHolder> holders_;
};

}  // namespace skity

#endif  // SRC_UTILS_VECTOR_CACHE_HPP
