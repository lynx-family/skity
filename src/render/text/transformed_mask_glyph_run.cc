// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/text/transformed_mask_glyph_run.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

#include "src/render/text/atlas_glyph_run.hpp"

namespace skity {
namespace transformed_mask {

namespace {

constexpr float kCreationScaleBucketsPerOctave = 4.f;
constexpr float kMinimumCreationScale = 1.f / 65536.f;
constexpr uint32_t kMaximumScaleFitAttempts = 8;
constexpr float kPerspectiveWEpsilon = 1.f / 65536.f;
constexpr float kConservativeBounds = 1E9F;

bool IsFinite(const Vec2& point) {
  return std::isfinite(point.x) && std::isfinite(point.y);
}

bool IsFinite(const Vec4& point) {
  return std::isfinite(point.x) && std::isfinite(point.y) &&
         std::isfinite(point.z) && std::isfinite(point.w);
}

float EstimateScaleAt(const Matrix& position_matrix, const Vec2& position) {
  std::array<Vec2, 3> source = {
      position,
      position + Vec2{1.f, 0.f},
      position + Vec2{0.f, 1.f},
  };
  std::array<Vec2, 3> device;
  position_matrix.MapPoints(device.data(), source.data(),
                            static_cast<int>(source.size()));
  if (!IsFinite(device[0]) || !IsFinite(device[1]) || !IsFinite(device[2])) {
    return 0.f;
  }

  float scale_x = (device[1] - device[0]).Length();
  float scale_y = (device[2] - device[0]).Length();
  float scale = std::max(scale_x, scale_y);
  return std::isfinite(scale) && scale > 0.f ? scale : 0.f;
}

float EstimatePerspectiveScale(
    const Matrix& position_matrix, uint32_t count, const float* position_x,
    const float* position_y, const std::vector<const GlyphData*>& glyph_info) {
  float scale = 1.f;
  for (uint32_t index = 0; index < count; ++index) {
    const GlyphData* info = glyph_info[index];
    if (info == nullptr) {
      continue;
    }

    float left = position_x[index] + info->GetHoriBearingX();
    float top = position_y[index] - info->GetHoriBearingY();
    float right = left + info->GetWidth();
    float bottom = top + info->GetHeight();
    std::array<Vec2, 5> samples = {
        Vec2{left, top},
        Vec2{right, top},
        Vec2{right, bottom},
        Vec2{left, bottom},
        Vec2{position_x[index], position_y[index]},
    };
    for (const Vec2& sample : samples) {
      if (IsFinite(sample)) {
        scale = std::max(scale, EstimateScaleAt(position_matrix, sample));
      }
    }
  }
  return scale;
}

float MaximumLogicalGlyphDimension(
    const Font& font, const Paint& paint,
    const std::vector<const GlyphData*>& glyph_info) {
  float dimension = std::max(font.GetSize(), 1.f);
  for (const GlyphData* info : glyph_info) {
    if (info != nullptr) {
      dimension = std::max({dimension, info->GetWidth(), info->GetHeight()});
    }
  }
  if (paint.GetStyle() == Paint::kStroke_Style) {
    dimension += std::abs(paint.GetStrokeWidth()) * 2.f;
  }
  return dimension;
}

float FitCreationScaleToBitmapInfos(uint32_t count, const GlyphID* glyphs,
                                    const Font& font, const Paint& paint,
                                    float context_scale,
                                    uint32_t maximum_dimension,
                                    float creation_scale) {
  std::vector<const GlyphData*> glyph_info(count);
  for (uint32_t attempt = 0; attempt < kMaximumScaleFitAttempts; ++attempt) {
    Matrix creation_matrix = Matrix::Scale(creation_scale, creation_scale);
    font.LoadGlyphBitmapInfo(glyphs, count, glyph_info.data(), paint,
                             context_scale, creation_matrix);

    float actual_maximum_dimension = 0.f;
    for (const GlyphData* info : glyph_info) {
      if (info != nullptr) {
        actual_maximum_dimension =
            std::max({actual_maximum_dimension, info->Image().width,
                      info->Image().height});
      }
    }

    // FreeType currently has no lightweight bitmap-info implementation. Its
    // conservative metrics fit above remains the source of truth until the
    // atlas request rasterizes the glyph.
    if (actual_maximum_dimension <= 0.f ||
        actual_maximum_dimension <= maximum_dimension) {
      return creation_scale;
    }

    float next_scale = QuantizeCreationScaleDown(
        creation_scale * static_cast<float>(maximum_dimension) /
        actual_maximum_dimension);
    if (next_scale >= creation_scale) {
      next_scale = QuantizeCreationScaleDown(
          creation_scale / std::exp2(1.f / kCreationScaleBucketsPerOctave));
    }
    if (next_scale <= 0.f || next_scale >= creation_scale) {
      return 0.f;
    }
    creation_scale = next_scale;
  }
  return 0.f;
}

float ComputeCreationScale(uint32_t count, const GlyphID* glyphs,
                           const float* position_x, const float* position_y,
                           const Font& font, const Paint& paint,
                           float context_scale, const Matrix& position_matrix,
                           const AtlasConfig& atlas_config) {
  if (count == 0 || glyphs == nullptr || position_x == nullptr ||
      position_y == nullptr || !position_matrix.IsFinite() ||
      !std::isfinite(context_scale) || context_scale <= 0.f) {
    return 0.f;
  }

  std::vector<const GlyphData*> glyph_info(count);
  font.LoadGlyphMetrics(glyphs, count, glyph_info.data(), paint);

  float creation_scale = EstimatePerspectiveScale(
      position_matrix, count, position_x, position_y, glyph_info);
  uint32_t maximum_dimension = MaximumAtlasGlyphDimension(atlas_config);
  if (maximum_dimension == 0) {
    return 0.f;
  }

  float logical_dimension =
      MaximumLogicalGlyphDimension(font, paint, glyph_info);
  creation_scale = FitCreationScaleToAtlas(creation_scale, logical_dimension,
                                           context_scale, maximum_dimension);
  if (creation_scale <= 0.f) {
    return 0.f;
  }

  return FitCreationScaleToBitmapInfos(count, glyphs, font, paint,
                                       context_scale, maximum_dimension,
                                       creation_scale);
}

class TransformedMaskGlyphRun final : public AtlasGlyphRun {
 public:
  TransformedMaskGlyphRun(Atlas& atlas, AtlasGlyphBatch batch,
                          const Paint& paint, bool is_stroke,
                          TextAtlasEffect effect, const Matrix& position_matrix,
                          const Matrix& creation_matrix)
      : AtlasGlyphRun(atlas, std::move(batch), paint, is_stroke, effect),
        position_matrix_(position_matrix),
        creation_matrix_(creation_matrix) {}

 private:
  std::optional<AtlasGlyphDrawParams> GetDrawParams(
      const GlyphDrawContext& context) const override {
    Matrix view_difference;
    // Quads contain C * local. The GPU applies P * inverse(C), replacing the
    // creation scale with the full perspective position matrix.
    if (!ComputeViewDifference(position_matrix_, creation_matrix_,
                               &view_difference)) {
      return std::nullopt;
    }
    const Matrix transform = context.raster_to_layer * view_difference;
    return AtlasGlyphDrawParams{
        transform, Matrix{}, context.raster_to_layer * position_matrix_,
        MapBounds(transform, GeometryBounds()), GPUFilterMode::kLinear};
  }

  const Matrix position_matrix_;
  const Matrix creation_matrix_;
};

}  // namespace

Paint MakeBitmapOnlyFillPaint(const Paint& paint) {
  Paint fill_paint = paint;
  if (paint.GetStyle() == Paint::kStroke_Style) {
    fill_paint.SetFillColor(paint.GetStrokeColor());
  }
  fill_paint.SetStyle(Paint::kFill_Style);
  return fill_paint;
}

float QuantizeCreationScaleDown(float scale) {
  if (!std::isfinite(scale) || scale < kMinimumCreationScale) {
    return 0.f;
  }

  float bucket = std::floor(std::log2(scale) * kCreationScaleBucketsPerOctave);
  float quantized = std::exp2(bucket / kCreationScaleBucketsPerOctave);
  return std::isfinite(quantized) && quantized >= kMinimumCreationScale
             ? quantized
             : 0.f;
}

uint32_t MaximumAtlasGlyphDimension(const AtlasConfig& atlas_config) {
  uint32_t reserved_dimension = Atlas_Padding + 2;
  return atlas_config.max_bitmap_size > reserved_dimension
             ? atlas_config.max_bitmap_size - reserved_dimension
             : 0;
}

float FitCreationScaleToAtlas(float requested_scale, float logical_dimension,
                              float context_scale, uint32_t maximum_dimension) {
  if (!std::isfinite(requested_scale) || requested_scale <= 0.f ||
      !std::isfinite(logical_dimension) || logical_dimension <= 0.f ||
      !std::isfinite(context_scale) || context_scale <= 0.f ||
      maximum_dimension == 0) {
    return 0.f;
  }

  // Two pixels are reserved for the rasterizer's AA expansion. The atlas
  // allocator's own padding is already accounted for by maximum_dimension.
  float fit_numerator =
      std::max(static_cast<float>(maximum_dimension) - 2.f, 1.f);
  float fit_scale = fit_numerator / (logical_dimension * context_scale);
  return QuantizeCreationScaleDown(std::min(requested_scale, fit_scale));
}

bool ComputeViewDifference(const Matrix& position_matrix,
                           const Matrix& creation_matrix,
                           Matrix* view_difference) {
  if (view_difference == nullptr || !position_matrix.IsFinite() ||
      !creation_matrix.IsFinite()) {
    return false;
  }
  Matrix creation_inverse;
  if (!creation_matrix.Invert(&creation_inverse)) {
    return false;
  }
  *view_difference = position_matrix * creation_inverse;
  return view_difference->IsFinite();
}

Rect MapBounds(const Matrix& transform, const Rect& bounds) {
  if (bounds.IsEmpty()) {
    return Rect::MakeEmpty();
  }

  std::array<Vec4, 4> source = {
      Vec4{bounds.Left(), bounds.Top(), 0.f, 1.f},
      Vec4{bounds.Right(), bounds.Top(), 0.f, 1.f},
      Vec4{bounds.Right(), bounds.Bottom(), 0.f, 1.f},
      Vec4{bounds.Left(), bounds.Bottom(), 0.f, 1.f},
  };
  std::array<Vec2, 4> projected;
  bool has_positive_w = false;
  bool has_negative_w = false;
  for (size_t index = 0; index < source.size(); ++index) {
    Vec4 point = transform * source[index];
    if (!IsFinite(point) || std::abs(point.w) <= kPerspectiveWEpsilon) {
      return Rect::MakeLTRB(-kConservativeBounds, -kConservativeBounds,
                            kConservativeBounds, kConservativeBounds);
    }
    has_positive_w |= point.w > 0.f;
    has_negative_w |= point.w < 0.f;
    projected[index] = Vec2{point.x / point.w, point.y / point.w};
    if (!IsFinite(projected[index])) {
      return Rect::MakeLTRB(-kConservativeBounds, -kConservativeBounds,
                            kConservativeBounds, kConservativeBounds);
    }
  }
  if (has_positive_w && has_negative_w) {
    return Rect::MakeLTRB(-kConservativeBounds, -kConservativeBounds,
                          kConservativeBounds, kConservativeBounds);
  }

  float left = projected[0].x;
  float top = projected[0].y;
  float right = projected[0].x;
  float bottom = projected[0].y;
  for (size_t index = 1; index < projected.size(); ++index) {
    left = std::min(left, projected[index].x);
    top = std::min(top, projected[index].y);
    right = std::max(right, projected[index].x);
    bottom = std::max(bottom, projected[index].y);
  }
  Rect result = Rect::MakeLTRB(left, top, right, bottom);
  return result.IsFinite()
             ? result
             : Rect::MakeLTRB(-kConservativeBounds, -kConservativeBounds,
                              kConservativeBounds, kConservativeBounds);
}

void AppendGlyphRuns(const GlyphRunInput& input, const Paint& paint,
                     AtlasFormat format, bool is_stroke,
                     const GlyphRunBuildContext& context,
                     GlyphSubRunList& runs) {
  Atlas& atlas = *context.atlas_manager.GetAtlas(format);
  const Matrix position_matrix =
      context.local_to_raster *
      Matrix::Translate(input.origin.x, input.origin.y);
  const float creation_scale = ComputeCreationScale(
      input.count, input.glyphs, input.position_x, input.position_y, input.font,
      paint, context.device_scale, position_matrix, atlas.GetConfig());
  if (creation_scale <= 0.f) {
    return;
  }
  const Matrix creation_matrix = Matrix::Scale(creation_scale, creation_scale);

  std::vector<const GlyphData*> glyph_info(input.count);
  input.font.LoadGlyphMetrics(input.glyphs, input.count, glyph_info.data(),
                              paint);
  std::vector<GlyphRegion> regions(input.count);
  std::vector<GlyphFormat> formats(input.count);
  for (uint32_t index = 0; index < input.count; ++index) {
    if (glyph_info[index] == nullptr) {
      continue;
    }
    formats[index] = ResolveAtlasGlyphFormat(format, *glyph_info[index]);
    regions[index] = atlas.GetGlyphRegion(
        input.font, PackedGlyphID(input.glyphs[index]), paint, false,
        context.device_scale, creation_matrix);
  }

  AtlasGlyphBatchBuilder batches(atlas);
  for (uint32_t index = 0; index < input.count; ++index) {
    if (glyph_info[index] == nullptr) {
      continue;
    }
    const Vec2 position{input.position_x[index], input.position_y[index]};
    Vec2 creation_position{};
    creation_matrix.MapPoints(&creation_position, &position, 1);
    const auto& region = regions[index];
    batches.Append(region, formats[index],
                   Rect::MakeXYWH(creation_position.x + region.origin_x,
                                  creation_position.y - region.origin_y,
                                  region.loc.z / context.device_scale,
                                  region.loc.w / context.device_scale));
  }

  for (auto& batch : batches.TakeBatches()) {
    const auto effect = MaskAtlasEffect(batch.format);
    runs.push_back(context.arena.Make<TransformedMaskGlyphRun>(
        atlas, std::move(batch), paint, is_stroke, effect, position_matrix,
        creation_matrix));
  }
}

}  // namespace transformed_mask
}  // namespace skity
