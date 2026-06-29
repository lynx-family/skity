// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/dst_read_strategy.hpp"

#include "src/graphic/blend_mode_priv.hpp"
#include "src/render/hw/native_blend.hpp"

namespace skity {

DstReadStrategy ResolveDstReadStrategy(BlendMode blend_mode,
                                       const GPUCaps& caps) {
  if (!IsAdvancedBlendMode(blend_mode)) {
    return DstReadStrategy::kNonRequired;
  }

  const bool has_native = caps.supports_native_advanced_blend &&
                          ToNativeBlendOp(blend_mode).has_value();

  // 1. Coherent native is strictly optimal: no flush, no shader math, no pass
  //    split. Highest priority everywhere.
  if (has_native && caps.supports_native_advanced_blend_coherent) {
    return DstReadStrategy::kNativeBlend;
  }

  // 2. framebuffer_fetch is tile-friendly. On tile-based GPUs it beats
  //    non-coherent native (whose per-draw barrier forces a tile flush), so it
  //    is the main path on mobile. kModulate (has_native == false) lands here.
  if (caps.supports_framebuffer_fetch) {
    return DstReadStrategy::kFramebufferFetch;
  }

  // 3. Non-coherent native is still cheap on desktop immediate-mode renderers
  //    (the barrier is not a tile flush there), so prefer it over texture_copy
  //    where framebuffer_fetch is unavailable (typical desktop GL).
  if (has_native) {
    return DstReadStrategy::kNativeBlend;
  }

  return DstReadStrategy::kTextureCopy;
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
