// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_UTILS_UNIQUE_ID_HPP
#define SRC_UTILS_UNIQUE_ID_HPP

#include <cstddef>
#include <functional>

namespace skity {

struct UniqueID {
  size_t id;

  UniqueID();

  constexpr bool operator==(const UniqueID& other) const {
    return id == other.id;
  }

  struct Hash {
    std::size_t operator()(const UniqueID& rhs) const {
      return std::hash<size_t>()(rhs.id);
    }
  };

  struct Equal {
    bool operator()(const UniqueID& lhs, const UniqueID& rhs) const {
      return lhs.id == rhs.id;
    }
  };
};

}  // namespace skity

#endif  // SRC_UTILS_UNIQUE_ID_HPP
