// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/text/transformed_mask_glyph_run.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

#include "src/render/hw/draw/fragment/wgsl_text_fragment.hpp"
#include "src/render/hw/draw/geometry/wgsl_text_geometry.hpp"
#include "src/render/hw/draw/hw_dynamic_text_draw.hpp"
#include "src/render/hw/draw/wgx_filter.hpp"
#include "src/render/hw/draw/wgx_utils.hpp"
#include "src/tracing.hpp"

namespace skity {
namespace transformed_mask {

namespace {

constexpr float kCreationScaleBucketsPerOctave = 4.f;
constexpr float kMinimumCreationScale = 1.f / 65536.f;
constexpr uint32_t kMaximumScaleFitAttempts = 8;
constexpr float kPerspectiveWEpsilon = 1.f / 65536.f;
constexpr float kConservativeBounds = 1E9F;

struct GlyphRegionWithIndex {
  uint32_t index;
  GlyphRegion region;
};

struct GlyphRegionGroup {
  uint32_t group_index;
  GlyphFormat glyph_format;
  std::vector<GlyphRegionWithIndex> glyph_regions;
};

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

GlyphFormat ResolveGlyphFormat(AtlasFormat atlas_format,
                               const GlyphData* glyph_info) {
  if (atlas_format == AtlasFormat::A8) {
    return GlyphFormat::A8;
  }
  if (glyph_info != nullptr && glyph_info->GetFormat().has_value() &&
      *glyph_info->GetFormat() == GlyphFormat::BGRA32) {
    return GlyphFormat::BGRA32;
  }
  return GlyphFormat::RGBA32;
}

class TransformedMaskGlyphRun final : public GlyphRun {
 public:
  TransformedMaskGlyphRun(uint32_t count, const GlyphID* glyphs,
                          const Point& origin, const float* position_x,
                          const float* position_y, const Font& font,
                          float context_scale, const Matrix& creation_matrix,
                          const Paint& paint, bool is_stroke,
                          std::vector<GlyphRegionWithIndex> glyph_locs,
                          uint32_t group_index, Atlas* atlas,
                          GlyphFormat glyph_format)
      : count_(count),
        glyphs_(glyphs, glyphs + count),
        origin_(origin),
        position_x_(position_x, position_x + count),
        position_y_(position_y, position_y + count),
        font_(font),
        context_scale_(context_scale),
        creation_matrix_(creation_matrix),
        paint_(paint),
        is_stroke_(is_stroke),
        glyph_locs_(std::move(glyph_locs)),
        group_index_(group_index),
        atlas_(atlas),
        glyph_format_(glyph_format) {}

  GlyphDrawList Draw(Matrix transform, ArenaAllocator* arena_allocator,
                     float canvas_scale, bool use_linear_text_filter,
                     bool split_overlapping_glyphs) override;

  bool HasFragmentMask() const override {
    return glyph_format_ == GlyphFormat::A8;
  }

  bool IsSourceOpaque() const override {
    return glyph_format_ == GlyphFormat::A8 && IsPaintSourceOpaque(paint_);
  }

  bool IsStroke() override { return is_stroke_; }

 private:
  ArrayList<GlyphRect, 16> Raster(ArenaAllocator* arena_allocator);

  uint32_t count_;
  std::vector<GlyphID> glyphs_;
  Point origin_;
  std::vector<float> position_x_;
  std::vector<float> position_y_;
  Font font_;
  float context_scale_;
  Matrix creation_matrix_;
  Paint paint_;
  bool is_stroke_;
  std::vector<GlyphRegionWithIndex> glyph_locs_;
  uint32_t group_index_;
  Atlas* atlas_;
  GlyphFormat glyph_format_;
};

ArrayList<GlyphRect, 16> TransformedMaskGlyphRun::Raster(
    ArenaAllocator* arena_allocator) {
  ArrayList<GlyphRect, 16> glyph_rects;
  glyph_rects.SetArenaAllocator(arena_allocator);
  if (glyph_locs_.empty() || context_scale_ <= 0.f) {
    return glyph_rects;
  }

  std::vector<const GlyphData*> glyph_info(count_);
  font_.LoadGlyphBitmapInfo(glyphs_.data(), count_, glyph_info.data(), paint_,
                            context_scale_, creation_matrix_);

  for (const auto& glyph_loc : glyph_locs_) {
    const GlyphData* info = glyph_info[glyph_loc.index];
    if (info == nullptr) {
      continue;
    }

    Vec2 uv_lt =
        atlas_->CalculateUV(glyph_loc.region.index_in_group,
                            glyph_loc.region.loc.x, glyph_loc.region.loc.y);
    Vec2 uv_rb =
        atlas_->CalculateUV(glyph_loc.region.index_in_group,
                            glyph_loc.region.loc.x + glyph_loc.region.loc.z,
                            glyph_loc.region.loc.y + glyph_loc.region.loc.w);

    const Vec2 run_position{position_x_[glyph_loc.index],
                            position_y_[glyph_loc.index]};
    Vec2 creation_position{};
    creation_matrix_.MapPoints(&creation_position, &run_position, 1);
    if (!IsFinite(creation_position)) {
      continue;
    }

    const GlyphBitmapData& image = info->Image();
    float left = creation_position.x + image.origin_x;
    float top = creation_position.y - image.origin_y;
    float width = static_cast<float>(glyph_loc.region.loc.z) / context_scale_;
    float height = static_cast<float>(glyph_loc.region.loc.w) / context_scale_;
    if (!std::isfinite(left) || !std::isfinite(top) || !std::isfinite(width) ||
        !std::isfinite(height) || width <= 0.f || height <= 0.f) {
      continue;
    }

    glyph_rects.emplace_back(Vec4{left, top, left + width, top + height}, uv_lt,
                             uv_rb);
  }

  return glyph_rects;
}

GlyphDrawList TransformedMaskGlyphRun::Draw(Matrix transform,
                                            ArenaAllocator* arena_allocator,
                                            float canvas_scale,
                                            bool use_linear_text_filter,
                                            bool split_overlapping_glyphs) {
  SKITY_TRACE_EVENT(TransformedMaskGlyphRun_Draw);
  (void)canvas_scale;
  (void)use_linear_text_filter;
  GlyphDrawList draws;
  draws.SetArenaAllocator(arena_allocator);
  if (!transform.IsFinite()) {
    return draws;
  }

  ArrayList<GlyphRect, 16> glyph_rects = Raster(arena_allocator);
  if (glyph_rects.empty()) {
    return draws;
  }

  Matrix position_matrix = transform * Matrix::Translate(origin_.x, origin_.y);
  Matrix view_difference;
  // Atlas quads are stored in creation space (C * local). The vertex shader
  // applies V = P * inverse(C), replacing C with the full perspective position
  // matrix P. Surface density remains in MVP, so the final relation is
  // MVP(D) * V * C * local = D * P * local.
  if (!ComputeViewDifference(position_matrix, creation_matrix_,
                             &view_difference)) {
    return draws;
  }

  auto batches =
      BuildGlyphRectBatches(std::move(glyph_rects), Matrix{}, arena_allocator,
                            split_overlapping_glyphs);

  atlas_->UploadAtlas(group_index_);
  auto gpu_texture = atlas_->GetGPUTexture(group_index_);
  auto gpu_sampler =
      atlas_->GetGPUSampler(group_index_, GPUFilterMode::kLinear);

  for (auto& batch : batches) {
    HWWGSLGeometry* geometry = nullptr;
    if (atlas_->GetFormat() == AtlasFormat::A8 && paint_.GetShader()) {
      geometry = arena_allocator->Make<WGSLTextGradientGeometry>(
          Matrix(), std::move(batch.glyph_rects),
          paint_.GetShader()->GetLocalMatrix(), position_matrix);
    } else {
      Vector color =
          is_stroke_ ? paint_.GetStrokeColor() : paint_.GetFillColor();
      Paint paint_copy = paint_;
      paint_copy.SetFillColor(color);
      paint_copy.SetStrokeColor(color);
      geometry = arena_allocator->Make<WGSLTextSolidColorGeometry>(
          Matrix(), std::move(batch.glyph_rects), paint_copy);
    }

    HWWGSLFragment* fragment = nullptr;
    if (atlas_->GetFormat() == AtlasFormat::A8) {
      if (paint_.GetShader() && paint_.GetShader()->AsGradient(nullptr) !=
                                    Shader::GradientType::kNone) {
        Shader::GradientInfo info{};
        auto type = paint_.GetShader()->AsGradient(&info);
        fragment = arena_allocator->Make<WGSLGradientTextFragment>(
            gpu_texture, gpu_sampler, info, type, paint_.GetAlphaF());
      } else {
        fragment = arena_allocator->Make<WGSLColorTextFragment>(gpu_texture,
                                                                gpu_sampler);
      }
    } else {
      fragment = arena_allocator->Make<WGSLColorEmojiFragment>(
          gpu_texture, gpu_sampler, glyph_format_ == GlyphFormat::BGRA32,
          paint_.GetAlphaF());
    }

    if (paint_.GetColorFilter()) {
      fragment->SetFilter(
          WGXFilterFragment::Make(paint_.GetColorFilter().get()));
    }

    auto* draw = arena_allocator->Make<HWDynamicTextDraw>(view_difference,
                                                          geometry, fragment);
    draws.emplace_back(
        GlyphDraw{draw, MapBounds(view_difference, batch.bounds)});
  }
  return draws;
}

GlyphRegionGroup* GetOrAppendContiguousGroup(
    uint32_t group_index, GlyphFormat glyph_format,
    std::vector<GlyphRegionGroup>* groups) {
  if (groups->empty() || groups->back().group_index != group_index ||
      groups->back().glyph_format != glyph_format) {
    groups->push_back({group_index, glyph_format, {}});
  }
  return &groups->back();
}

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

GlyphRunList MakeGlyphRunList(uint32_t count, const GlyphID* glyphs,
                              const Point& origin, const float* position_x,
                              const float* position_y, const Font& font,
                              const Paint& paint, AtlasFormat format,
                              float context_scale, const Matrix& transform,
                              bool is_stroke, AtlasManager* atlas_manager,
                              ArenaAllocator* arena_allocator) {
  GlyphRunList run_list;
  run_list.SetArenaAllocator(arena_allocator);
  if (count == 0 || glyphs == nullptr || position_x == nullptr ||
      position_y == nullptr || atlas_manager == nullptr ||
      arena_allocator == nullptr || !transform.IsFinite()) {
    return run_list;
  }

  Atlas* atlas = atlas_manager->GetAtlas(format);
  Matrix position_matrix = transform * Matrix::Translate(origin.x, origin.y);
  float creation_scale =
      ComputeCreationScale(count, glyphs, position_x, position_y, font, paint,
                           context_scale, position_matrix, atlas->GetConfig());
  if (creation_scale <= 0.f) {
    return run_list;
  }
  Matrix creation_matrix = Matrix::Scale(creation_scale, creation_scale);

  std::vector<const GlyphData*> glyph_info(count);
  font.LoadGlyphMetrics(glyphs, count, glyph_info.data(), paint);

  uint32_t max_per_atlas = atlas->GetConfig().max_num_bitmap_per_atlas;
  std::vector<GlyphRegionGroup> groups;
  for (uint32_t index = 0; index < count; ++index) {
    if (glyph_info[index] == nullptr) {
      continue;
    }
    GlyphRegion glyph_region =
        atlas->GetGlyphRegion(font, glyph_info[index]->Id(), paint, false,
                              context_scale, creation_matrix);
    if (glyph_region.loc.z <= 0 || glyph_region.loc.w <= 0) {
      continue;
    }

    uint32_t group_index = glyph_region.index_in_group / max_per_atlas;
    glyph_region.index_in_group %= max_per_atlas;
    GlyphFormat glyph_format = ResolveGlyphFormat(format, glyph_info[index]);
    GetOrAppendContiguousGroup(group_index, glyph_format, &groups)
        ->glyph_regions.push_back({index, glyph_region});
  }

  for (auto& group : groups) {
    run_list.push_back(arena_allocator->Make<TransformedMaskGlyphRun>(
        count, glyphs, origin, position_x, position_y, font, context_scale,
        creation_matrix, paint, is_stroke, std::move(group.glyph_regions),
        group.group_index, atlas, group.glyph_format));
  }
  return run_list;
}

}  // namespace transformed_mask
}  // namespace skity
