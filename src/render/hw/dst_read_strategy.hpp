// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DST_READ_STRATEGY_HPP
#define SRC_RENDER_HW_DST_READ_STRATEGY_HPP

#include <skity/graphic/blend_mode.hpp>

#include "src/gpu/gpu_caps.hpp"
#include "src/render/hw/draw/wgx_programmable_blending.hpp"

namespace skity {

DstReadStrategy ResolveDstReadStrategy(BlendMode blend_mode,
                                       const GPUCaps& caps);

DstReadStrategy ResolveDstReadStrategy(BlendMode blend_mode,
                                       const GPUCaps& caps,
                                       bool supports_texture_copy_dst_read);

}  // namespace skity

#endif  // SRC_RENDER_HW_DST_READ_STRATEGY_HPP
