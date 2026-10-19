// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/hw_dynamic_text_draw.hpp"

#include <iterator>
#include <utility>

#include "src/logging.hpp"
#include "src/render/hw/draw/step/color_step.hpp"
#include "src/render/hw/draw/wgx_utils.hpp"

namespace skity {

HWDynamicTextDraw::HWDynamicTextDraw(Matrix draw_transform,
                                     const Matrix& glyph_transform,
                                     ArrayList<GlyphRect, 16> glyph_rects,
                                     Paint paint,
                                     TextGeometryData::BatchedTexture textures,
                                     std::shared_ptr<GPUSampler> sampler,
                                     TextAtlasEffect effect, bool is_stroke,
                                     std::optional<Matrix> layer_to_local)
    : HWDynamicDraw(std::move(draw_transform)),
      geometry_data_{{},
                     std::move(textures),
                     std::move(sampler),
                     effect,
                     std::move(layer_to_local)},
      shader_local_matrix_(
          paint.GetShader() ? paint.GetShader()->GetLocalMatrix() : Matrix{}),
      is_stroke_(is_stroke) {
  geometry_data_.glyph_rects.reserve(glyph_rects.size());
  const bool transform_glyphs = !glyph_transform.IsIdentity();
  for (auto& glyph_rect : glyph_rects) {
    if (transform_glyphs) {
      const Rect rect = glyph_transform.MapRect(glyph_rect.GetBounds());
      glyph_rect.vertex_coord = {rect.Left(), rect.Top(), rect.Right(),
                                 rect.Bottom()};
    }
    geometry_data_.glyph_rects.push_back(
        {std::move(glyph_rect), paint, glyph_transform});
  }
}

void HWDynamicTextDraw::OnGenerateDrawStep(ArrayList<HWDrawStep*, 2>& steps,
                                           HWDrawContext* context) {
  if (geometry_data_.glyph_rects.empty()) {
    return;
  }

  auto* arena = context->arena_allocator;
  Paint paint = geometry_data_.glyph_rects.front().paint;
  const bool color_atlas =
      geometry_data_.effect == TextAtlasEffect::kColor ||
      geometry_data_.effect == TextAtlasEffect::kColorSwizzleRB;
  if (color_atlas) {
    // Color glyphs provide their own source color, ignoring the paint shader.
    paint.SetShader(nullptr);
  }
  auto* fragment =
      GenShadingFragment(context, paint, is_stroke_,
                         /*has_color=*/false, &shader_local_matrix_);
  ConfigureShadingFragment(context, paint, GetBlendPlan(), fragment);

  auto* geometry = arena->Make<WGSLTextGeometry>(geometry_data_);
  steps.emplace_back(
      arena->Make<ColorStep>(geometry, fragment, CoverageType::kNone));
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
  auto& data = geometry_data_;
  auto& other_data = other->geometry_data_;
  // One merged draw uploads one common-slot transform for all glyph instances.
  if (GetTransform() != other->GetTransform() ||
      data.effect != other_data.effect || data.sampler != other_data.sampler ||
      data.textures != other_data.textures ||
      data.layer_to_local != other_data.layer_to_local) {
    return false;
  }

  const Paint& paint = data.glyph_rects.front().paint;
  const Paint& other_paint = other_data.glyph_rects.front().paint;
  const bool color_atlas = data.effect == TextAtlasEffect::kColor ||
                           data.effect == TextAtlasEffect::kColorSwizzleRB;
  if ((!color_atlas &&
       (paint.GetShader() != nullptr || other_paint.GetShader() != nullptr)) ||
      paint.GetColorFilter() != other_paint.GetColorFilter()) {
    return false;
  }

  data.glyph_rects.insert(
      data.glyph_rects.end(),
      std::make_move_iterator(other_data.glyph_rects.begin()),
      std::make_move_iterator(other_data.glyph_rects.end()));
  return true;
}

Matrix HWDynamicTextDraw::CalcSDFTransform(const Matrix& transform,
                                           const float scale) {
  return transform * Matrix::Scale(1.f / scale, 1.f / scale);
}

}  // namespace skity
