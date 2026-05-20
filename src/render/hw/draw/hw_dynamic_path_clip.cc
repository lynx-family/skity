// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/hw_dynamic_path_clip.hpp"

#include "src/render/hw/draw/fragment/wgsl_stencil_fragment.hpp"
#include "src/render/hw/draw/geometry/wgsl_clip_geometry.hpp"
#include "src/render/hw/draw/geometry/wgsl_tess_path_fill_geometry.hpp"
#include "src/render/hw/draw/hw_wgsl_geometry.hpp"
#include "src/render/hw/draw/step/clip_step.hpp"
#include "src/render/hw/draw/step/stencil_step.hpp"

namespace skity {

HWDynamicPathClip::HWDynamicPathClip(Matrix transform, Path path,
                                     Canvas::ClipOp op, const Rect &bounds,
                                     bool use_gpu_tessellation)
    : HWDynamicDraw(transform, BlendMode::kSrcOver),
      path_(std::move(path)),
      op_(op),
      bounds_(),
      use_gpu_tessellation_(use_gpu_tessellation) {
  bounds_.AddRect(bounds);
}

void HWDynamicPathClip::OnGenerateDrawStep(ArrayList<HWDrawStep *, 2> &steps,
                                           HWDrawContext *context) {
  auto arena_allocator = context->arena_allocator;

  HWWGSLGeometry *geometry;
  if (use_gpu_tessellation_) {
    geometry = arena_allocator->Make<WGSLTessPathFillGeometry>(path_, paint_);
  } else {
    geometry = arena_allocator->Make<WGSLPathGeometry>(path_, paint_, false);
  }
  // clip always use stencil first
  steps.emplace_back(arena_allocator->Make<StencilStep>(
      geometry, arena_allocator->Make<WGSLStencilFragment>(), false));

  if (op_ == Canvas::ClipOp::kDifference) {
    steps.emplace_back(arena_allocator->Make<ClipStep>(
        geometry, arena_allocator->Make<WGSLStencilFragment>(),
        path_.GetFillType(), op_));
  } else {
    steps.emplace_back(arena_allocator->Make<ClipStep>(
        arena_allocator->Make<WGSLClipGeometry>(bounds_, paint_, false, op_),
        arena_allocator->Make<WGSLStencilFragment>(), path_.GetFillType(),
        op_));
  }
}

}  // namespace skity
