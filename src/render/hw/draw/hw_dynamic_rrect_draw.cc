// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/hw_dynamic_rrect_draw.hpp"

#include "src/render/hw/draw/geometry/wgsl_rrect_geometry.hpp"
#include "src/render/hw/draw/step/color_step.hpp"
#include "src/render/hw/draw/wgx_filter.hpp"
#include "src/render/hw/draw/wgx_utils.hpp"

namespace skity {

HWDynamicRRectDraw::HWDynamicRRectDraw(Matrix transform, RRect rrect,
                                       Paint paint)
    : HWDynamicDraw(transform, paint.GetBlendMode()),
      rrect_(std::move(rrect)),
      paint_(std::move(paint)) {}

void HWDynamicRRectDraw::OnGenerateDrawStep(ArrayList<HWDrawStep *, 2> &steps,
                                            HWDrawContext *context) {
  auto arena_allocator = context->arena_allocator;

  auto geom = arena_allocator->Make<WGSLRRectGeometry>(rrect_, paint_);
  auto frag = GenShadingFragment(context, paint_,
                                 paint_.GetStyle() == Paint::kStroke_Style);

  if (paint_.GetColorFilter()) {
    frag->SetFilter(WGXFilterFragment::Make(paint_.GetColorFilter().get()));
  }

  steps.emplace_back(context->arena_allocator->Make<ColorStep>(
      std::move(geom), std::move(frag), CoverageType::kNone));
}

}  // namespace skity
