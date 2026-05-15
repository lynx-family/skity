// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/hw_dynamic_rrect_draw.hpp"

#include "src/effect/pixmap_shader.hpp"
#include "src/graphic/blend_mode_priv.hpp"
#include "src/render/hw/draw/geometry/wgsl_rrect_geometry.hpp"
#include "src/render/hw/draw/step/color_step.hpp"
#include "src/render/hw/draw/wgx_filter.hpp"
#include "src/render/hw/draw/wgx_programmable_blending.hpp"
#include "src/render/hw/draw/wgx_utils.hpp"

namespace skity {

HWDynamicRRectDraw::HWDynamicRRectDraw(Matrix transform, RRect rrect,
                                       Paint paint)
    : HWDynamicDraw(transform, paint.GetBlendMode()) {
  batch_group_.emplace_back(BatchGroup<RRect>{
      std::move(rrect),
      std::move(paint),
      std::move(transform),
  });
}

bool HWDynamicRRectDraw::OnMergeIfPossible(HWDraw* draw) {
  if (!HWDynamicDraw::OnMergeIfPossible(draw)) {
    return false;
  }
  auto rrect_draw = static_cast<HWDynamicRRectDraw*>(draw);
  auto& rrect_group = rrect_draw->batch_group_;
  auto& merge_paint = rrect_group.front().paint;
  auto& paint = batch_group_.front().paint;

  if (paint.GetShader() != nullptr || merge_paint.GetShader() != nullptr) {
    return false;
  }

  if (paint.GetColorFilter() != merge_paint.GetColorFilter()) {
    return false;
  }

  for (size_t i = 0; i < rrect_group.size(); i++) {
    batch_group_.emplace_back(BatchGroup<RRect>{
        std::move(rrect_group[i].item),
        std::move(rrect_group[i].paint),
        std::move(rrect_group[i].transform),
    });
  }

  return true;
}

void HWDynamicRRectDraw::OnGenerateDrawStep(ArrayList<HWDrawStep*, 2>& steps,
                                            HWDrawContext* context) {
  auto arena_allocator = context->arena_allocator;
  auto paint = batch_group_.front().paint;
  auto geom = arena_allocator->Make<WGSLRRectGeometry>(batch_group_);
  auto frag = GenShadingFragment(
      context, paint, paint.GetStyle() == Paint::kStroke_Style, false);

  if (paint.GetColorFilter()) {
    frag->SetFilter(WGXFilterFragment::Make(paint.GetColorFilter().get()));
  }

  if (IsAdvancedBlendMode(paint.GetBlendMode())) {
    frag->SetProgrammableBlending(WGXProgrammableBlending::Make(
        paint.GetBlendMode(), GetDstReadStrategy()));
  }

  steps.emplace_back(context->arena_allocator->Make<ColorStep>(
      std::move(geom), std::move(frag), CoverageType::kNone));
}

}  // namespace skity
