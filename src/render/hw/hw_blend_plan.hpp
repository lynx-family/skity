// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_HW_BLEND_PLAN_HPP
#define SRC_RENDER_HW_HW_BLEND_PLAN_HPP

#include <skity/graphic/blend_mode.hpp>

#include "src/gpu/gpu_caps.hpp"
#include "src/gpu/gpu_render_pipeline.hpp"
#include "src/render/hw/dst_read_strategy.hpp"

namespace skity {

struct HWBlendFormula {
  GPUBlendFactor src_factor = GPUBlendFactor::kOne;
  GPUBlendFactor dst_factor = GPUBlendFactor::kOneMinusSrcAlpha;
  GPUBlendOperation operation = GPUBlendOperation::kAdd;
};

struct HWBlendPlan {
  BlendMode blend_mode = BlendMode::kSrcOver;
  DstReadStrategy dst_read_strategy = DstReadStrategy::kNonRequired;

  bool operator==(const HWBlendPlan& other) const {
    return blend_mode == other.blend_mode &&
           dst_read_strategy == other.dst_read_strategy;
  }

  bool operator!=(const HWBlendPlan& other) const { return !(*this == other); }

  bool SupportsFragmentMask() const;
};

// Fragment masks use shader-side blending when the existing fixed-function
// formula cannot represent partial coverage. Prefer framebuffer fetch, then a
// destination texture copy; without either route, preserve the legacy blend
// behavior.
HWBlendPlan ResolveHWBlendPlan(BlendMode blend_mode, bool has_fragment_mask,
                               const GPUCaps& caps,
                               bool supports_texture_copy_dst_read);

// Resolves the fixed-function state for the shader variant that will actually
// be used. Keeping this distinction preserves the existing text path, whose
// shader is currently created before its destination-read strategy is known.
HWBlendFormula ResolveHWBlendFormula(const HWBlendPlan& blend_plan,
                                     const GPUCaps& caps,
                                     bool shader_side_blending);

}  // namespace skity

#endif  // SRC_RENDER_HW_HW_BLEND_PLAN_HPP
