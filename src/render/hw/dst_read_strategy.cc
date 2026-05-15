// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/dst_read_strategy.hpp"

#include "src/graphic/blend_mode_priv.hpp"

namespace skity {

DstReadStrategy ResolveDstReadStrategy(BlendMode blend_mode,
                                       const GPUCaps& caps) {
  if (!IsAdvancedBlendMode(blend_mode)) {
    return DstReadStrategy::kNonRequired;
  }

  return caps.supports_framebuffer_fetch ? DstReadStrategy::kFramebufferFetch
                                         : DstReadStrategy::kTextureCopy;
}

DstReadStrategy ResolveDstReadStrategy(BlendMode blend_mode,
                                       const GPUCaps& caps,
                                       bool supports_texture_copy_dst_read) {
  auto strategy = ResolveDstReadStrategy(blend_mode, caps);
  if (strategy == DstReadStrategy::kTextureCopy &&
      !supports_texture_copy_dst_read) {
    return DstReadStrategy::kNonRequired;
  }

  return strategy;
}

}  // namespace skity
