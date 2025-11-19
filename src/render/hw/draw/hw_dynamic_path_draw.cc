// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/hw_dynamic_path_draw.hpp"

#include "src/effect/pixmap_shader.hpp"
#include "src/gpu/gpu_context_impl.hpp"
#include "src/logging.hpp"
#include "src/render/hw/draw/fragment/wgsl_gradient_fragment.hpp"
#include "src/render/hw/draw/fragment/wgsl_solid_color.hpp"
#include "src/render/hw/draw/fragment/wgsl_stencil_fragment.hpp"
#include "src/render/hw/draw/fragment/wgsl_texture_fragment.hpp"
#include "src/render/hw/draw/geometry/wgsl_path_geometry.hpp"
#include "src/render/hw/draw/geometry/wgsl_tess_path_fill_geometry.hpp"
#include "src/render/hw/draw/geometry/wgsl_tess_path_stroke_geometry.hpp"
#include "src/render/hw/draw/step/color_step.hpp"
#include "src/render/hw/draw/step/stencil_step.hpp"
#include "src/render/hw/draw/wgx_filter.hpp"
#include "src/render/hw/draw/wgx_utils.hpp"

namespace skity {

HWDynamicPathDraw::HWDynamicPathDraw(Matrix transform, Path path, Paint paint,
                                     bool is_stroke, bool use_gpu_tessellation)
    : HWDynamicDraw(transform, paint.GetBlendMode()),
      path_(std::move(path)),
      paint_(std::move(paint)),
      is_stroke_(is_stroke),
      use_gpu_tessellation_(use_gpu_tessellation) {}

void HWDynamicPathDraw::OnGenerateDrawStep(ArrayList<HWDrawStep *, 2> &steps,
                                           HWDrawContext *context) {
  bool single_pass = !is_stroke_ && path_.IsConvex() && !paint_.IsAntiAlias();

  auto geom = GenGeometry(context, false);

  auto frag = GenShadingFragment(context, paint_, is_stroke_);

  if (paint_.GetColorFilter()) {
    frag->SetFilter(WGXFilterFragment::Make(paint_.GetColorFilter().get()));
  }

  CoverageType coverage = CoverageType::kNone;

  if (single_pass) {
    coverage = CoverageType::kNone;
  } else if (is_stroke_) {
    coverage = CoverageType::kNoZero;
  } else if (path_.GetFillType() == Path::PathFillType::kEvenOdd) {
    coverage = CoverageType::kEvenOdd;
  } else {
    coverage = CoverageType::kWinding;
  }

  if (!single_pass) {
    // need stencil step first
    steps.emplace_back(context->arena_allocator->Make<StencilStep>(
        GenGeometry(context, false),
        context->arena_allocator->Make<WGSLStencilFragment>(),
        coverage == CoverageType::kNoZero));
  }

  if (paint_.IsAntiAlias()) {
    auto geometry = GenGeometry(context, true);

    auto fragment = GenShadingFragment(context, paint_, is_stroke_);

    if (paint_.GetColorFilter()) {
      fragment->SetFilter(
          WGXFilterFragment::Make(paint_.GetColorFilter().get()));
    }

    steps.emplace_back(context->arena_allocator->Make<ColorAAStep>(
        std::move(geometry), std::move(fragment), coverage));
  }

  steps.emplace_back(context->arena_allocator->Make<ColorStep>(
      std::move(geom), std::move(frag), coverage));
}

HWWGSLGeometry *HWDynamicPathDraw::GenGeometry(HWDrawContext *context,
                                               bool aa) const {
  auto arena_allocator = context->arena_allocator;
  if (use_gpu_tessellation_) {
    DEBUG_CHECK(!paint_.IsAntiAlias());
    DEBUG_CHECK(!aa);
    if (is_stroke_) {
      return arena_allocator->Make<WGSLTessPathStrokeGeometry>(path_, paint_);
    } else {
      return arena_allocator->Make<WGSLTessPathFillGeometry>(path_, paint_);
    }
  } else {
    if (aa) {
      return arena_allocator->Make<WGSLPathAAGeometry>(path_, paint_);

    } else {
      return arena_allocator->Make<WGSLPathGeometry>(path_, paint_, is_stroke_);
    }
  }
}

}  // namespace skity
