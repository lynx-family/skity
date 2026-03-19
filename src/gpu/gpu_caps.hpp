// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_GPU_CAPS_HPP
#define SRC_GPU_GPU_CAPS_HPP

#include <cstdint>

namespace skity {

struct GPUCaps {
  bool supports_framebuffer_fetch = false;
};

}  // namespace skity

#endif  // SRC_GPU_GPU_CAPS_HPP
