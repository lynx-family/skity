// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/hw_dynamic_text_draw.hpp"

#include <iterator>
#include <utility>

#include "src/logging.hpp"
#include "src/render/hw/draw/fragment/wgsl_solid_vertex_color.hpp"
#include "src/render/hw/draw/step/color_step.hpp"
#include "src/render/hw/draw/wgx_utils.hpp"

namespace skity {

HWDynamicTextDraw::HWDynamicTextDraw(Matrix draw_transform,
                                     const Matrix& glyph_transform,
                                     ArrayList<GlyphRect, 16> glyph_rects,
                                     Paint paint,
                                     WGSLTextGeometry::BatchedTexture textures,
                                     std::shared_ptr<GPUSampler> sampler,
                                     TextAtlasEffect effect, bool is_stroke,
                                     std::optional<Matrix> device_to_local)
    : HWDynamicDraw(std::move(draw_transform)),
      textures_(std::move(textures)),
      sampler_(std::move(sampler)),
      effect_(effect),
      is_stroke_(is_stroke),
      device_to_local_(std::move(device_to_local)) {
  glyph_rects_.reserve(glyph_rects.size());
  for (auto& glyph_rect : glyph_rects) {
    const auto rect = glyph_transform.MapRect(
        {glyph_rect.vertex_coord.x, glyph_rect.vertex_coord.y,
         glyph_rect.vertex_coord.z, glyph_rect.vertex_coord.w});
    glyph_rect.vertex_coord = {rect.Left(), rect.Top(), rect.Right(),
                               rect.Bottom()};
    glyph_rects_.push_back({std::move(glyph_rect), paint, glyph_transform});
  }
}

void HWDynamicTextDraw::OnGenerateDrawStep(ArrayList<HWDrawStep*, 2>& steps,
                                           HWDrawContext* context) {
  if (glyph_rects_.empty()) {
    return;
  }

  auto* arena = context->arena_allocator;
  const Paint paint = glyph_rects_.front().paint;
  const bool color_atlas = effect_ == TextAtlasEffect::kColor ||
                           effect_ == TextAtlasEffect::kColorSwizzleRB;
  HWWGSLFragment* fragment =
      color_atlas
          ? static_cast<HWWGSLFragment*>(arena->Make<WGSLSolidVertexColor>())
          : GenShadingFragment(context, paint, is_stroke_, false);
  ConfigureShadingFragment(context, paint, GetBlendPlan(), fragment);

  auto* geometry = arena->Make<WGSLTextGeometry>(
      std::move(glyph_rects_), std::move(textures_), std::move(sampler_),
      effect_, std::move(device_to_local_));
  steps.emplace_back(
      arena->Make<ColorStep>(geometry, fragment, CoverageType::kNone));
}

Matrix HWDynamicTextDraw::CalcTransform(const Matrix& canvas_transform,
                                        const Matrix& text_transform) {
  // Only support linear text transforms for now.
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

  DEBUG_CHECK(false);
  return Matrix{};
}

Matrix HWDynamicTextDraw::CalcSDFTransform(const Matrix& transform,
                                           float scale) {
  return transform * Matrix::Scale(1.f / scale, 1.f / scale);
}

bool HWDynamicTextDraw::OnMergeIfPossible(HWDraw* draw) {
  if (!HWDynamicDraw::OnMergeIfPossible(draw)) {
    return false;
  }
  auto* other = static_cast<HWDynamicTextDraw*>(draw);
  if (GetTransform() != other->GetTransform() || effect_ != other->effect_ ||
      is_stroke_ != other->is_stroke_ || sampler_ != other->sampler_ ||
      textures_ != other->textures_ ||
      device_to_local_ != other->device_to_local_) {
    return false;
  }

  const Paint& paint = glyph_rects_.front().paint;
  const Paint& other_paint = other->glyph_rects_.front().paint;
  const bool color_atlas = effect_ == TextAtlasEffect::kColor ||
                           effect_ == TextAtlasEffect::kColorSwizzleRB;
  if ((!color_atlas &&
       (paint.GetShader() != nullptr || other_paint.GetShader() != nullptr)) ||
      paint.GetColorFilter() != other_paint.GetColorFilter()) {
    return false;
  }

  glyph_rects_.insert(glyph_rects_.end(),
                      std::make_move_iterator(other->glyph_rects_.begin()),
                      std::make_move_iterator(other->glyph_rects_.end()));
  return true;
}

}  // namespace skity
