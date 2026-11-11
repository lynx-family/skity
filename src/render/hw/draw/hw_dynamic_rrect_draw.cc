// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/hw_dynamic_rrect_draw.hpp"

#include "src/logging.hpp"
#include "src/render/hw/draw/geometry/wgsl_rrect_geometry.hpp"

namespace skity {

HWDynamicRRectDraw::HWDynamicRRectDraw(const Matrix& transform,
                                       const RRect& rrect, const Paint& paint)
    : HWDynamicDraw(transform) {
  batch_group_.push_back({rrect, paint, transform});
}

bool HWDynamicRRectDraw::OnMergeIfPossible(HWDraw* draw) {
  const auto& other = *static_cast<const HWDynamicRRectDraw*>(draw);
  if (!CanMergeSolidPaints(batch_group_.front().paint,
                           other.batch_group_.front().paint)) {
    return false;
  }

  DEBUG_CHECK(other.batch_group_.size() == 1);
  batch_group_.push_back(other.batch_group_.front());
  return true;
}

void HWDynamicRRectDraw::OnGenerateDrawStep(ArrayList<HWDrawStep*, 2>& steps,
                                            HWDrawContext* context) {
  auto* geometry =
      context->arena_allocator->Make<WGSLRRectGeometry>(batch_group_);
  steps.emplace_back(
      MakeShapeColorStep(context, geometry, batch_group_.front().paint));
}

}  // namespace skity
