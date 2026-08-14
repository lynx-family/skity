// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DST_READ_STRATEGY_HPP
#define SRC_RENDER_HW_DST_READ_STRATEGY_HPP

namespace skity {

enum class DstReadStrategy {
  kNonRequired,
  kFramebufferFetch,
  kTextureCopy,
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DST_READ_STRATEGY_HPP
