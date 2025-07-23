// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_BACKEND_CAST_HPP
#define SRC_GPU_BACKEND_CAST_HPP

#define SKT_BACKEND_CAST(Sub, Base)                                \
  static Sub& Cast(Base& base) { return static_cast<Sub&>(base); } \
  static const Sub& Cast(const Base& base) {                       \
    return static_cast<const Sub&>(base);                          \
  }                                                                \
  static Sub* Cast(Base* base) { return static_cast<Sub*>(base); } \
  static const Sub* Cast(const Base* base) {                       \
    return static_cast<const Sub*>(base);                          \
  }

#endif  // SRC_GPU_BACKEND_CAST_HPP
