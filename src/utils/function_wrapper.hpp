// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_UTILS_FUNCTION_WRAPPER_HPP
#define SRC_UTILS_FUNCTION_WRAPPER_HPP

#include <utility>

namespace skity {

template <typename T, T* P>
struct FunctionWrapper {
  template <typename... Args>
  auto operator()(Args&&... args) const
      -> decltype(P(std::forward<Args>(args)...)) {
    return P(std::forward<Args>(args)...);
  }
};

}  // namespace skity

#endif  // SRC_UTILS_FUNCTION_WRAPPER_HPP
