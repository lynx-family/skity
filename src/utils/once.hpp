// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_UTILS_ONCE_HPP
#define SRC_UTILS_ONCE_HPP

#include <mutex>
#include <utility>

namespace skity {

class Once {
 public:
  template <typename Fn>
  void operator()(Fn&& fn) {
    std::call_once(flag_, std::forward<Fn>(fn));
  }

 private:
  std::once_flag flag_;
};

}  // namespace skity

#endif  // SRC_UTILS_ONCE_HPP
