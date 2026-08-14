// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/hw_dynamic_text_draw.hpp"

#include "src/logging.hpp"
#include "src/render/hw/draw/step/color_step.hpp"
#include "src/render/hw/draw/wgx_utils.hpp"

namespace skity {

void HWDynamicTextDraw::OnGenerateDrawStep(ArrayList<HWDrawStep*, 2>& steps,
                                           HWDrawContext* context) {
  if (geometry_ == nullptr || fragment_ == nullptr) {
    return;
  }

  ConfigureFragmentBlending(context, GetBlendPlan(), fragment_);
  steps.emplace_back(context->arena_allocator->Make<ColorStep>(
      geometry_, fragment_, CoverageType::kNone));
}

Matrix HWDynamicTextDraw::CalcTransform(const Matrix& canvas_transform,
                                        const Matrix& text_transform) {
  // Only support linear text transforms for now
  if (canvas_transform.GetScaleX() == text_transform.GetScaleX() &&
      canvas_transform.GetScaleY() == text_transform.GetScaleY() &&
      canvas_transform.GetSkewX() == text_transform.GetSkewX() &&
      canvas_transform.GetSkewY() == text_transform.GetSkewY()) {
    const Vec2 origin{0, 0};
    Vec2 dst_canvas{0, 0};
    Vec2 dst_text{0, 0};
    canvas_transform.MapPoints(&dst_canvas, &origin, 1);
    text_transform.MapPoints(&dst_text, &origin, 1);
    return Matrix::Translate(dst_text.x - dst_canvas.x,
                             dst_text.y - dst_canvas.y);
  }

  // TODO(jingle) Add debug check to detect general transforms
  DEBUG_CHECK(false);
  return Matrix{};
}

bool HWDynamicTextDraw::OnMergeIfPossible(HWDraw* draw) {
  if (!HWDynamicDraw::OnMergeIfPossible(draw)) {
    return false;
  }
  auto other = static_cast<HWDynamicTextDraw*>(draw);
  // One merged draw uploads one common-slot transform for all glyph instances.
  if (GetTransform() != other->GetTransform()) {
    return false;
  }
  if (!geometry_->CanMerge(other->geometry_) ||
      !fragment_->CanMerge(other->fragment_)) {
    return false;
  }
  geometry_->Merge(other->geometry_);
  fragment_->Merge(other->fragment_);
  return true;
}

void HWDynamicSdfTextDraw::OnGenerateDrawStep(ArrayList<HWDrawStep*, 2>& steps,
                                              HWDrawContext* context) {
  if (geometry_ == nullptr || fragment_ == nullptr) {
    return;
  }

  ConfigureFragmentBlending(context, GetBlendPlan(), fragment_);
  steps.emplace_back(context->arena_allocator->Make<ColorStep>(
      geometry_, fragment_, CoverageType::kNone));
}

Matrix HWDynamicSdfTextDraw::CalcTransform(const Matrix& transform,
                                           const float scale) {
  return transform * Matrix::Scale(1.f / scale, 1.f / scale);
}

}  // namespace skity
