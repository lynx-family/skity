// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/hw_dynamic_rect_draw.hpp"

#include "src/logging.hpp"

namespace skity {

HWDynamicRectDraw::HWDynamicRectDraw(const Matrix& transform, const Rect& rect,
                                     const Paint& paint)
    : HWDynamicDraw(transform),
      paint_(paint),
      first_instance_{{rect.Left(), rect.Top(), rect.Right(), rect.Bottom()},
                      {transform.GetScaleX(), transform.GetScaleY(),
                       transform.GetTranslateX(), transform.GetTranslateY()},
                      paint.GetFillColor()} {}

bool HWDynamicRectDraw::OnMergeIfPossible(HWDraw* draw) {
  const auto& other = *static_cast<const HWDynamicRectDraw*>(draw);
  if (!CanMergeSolidPaints(paint_, other.paint_)) {
    return false;
  }

  DEBUG_CHECK(other.additional_instances_.empty());
  additional_instances_.push_back(other.first_instance_);
  return true;
}

void HWDynamicRectDraw::OnGenerateDrawStep(ArrayList<HWDrawStep*, 2>& steps,
                                           HWDrawContext* context) {
  auto* geometry = context->arena_allocator->Make<WGSLRectGeometry>(
      first_instance_, additional_instances_);
  steps.emplace_back(MakeShapeColorStep(context, geometry, paint_));
}

}  // namespace skity
