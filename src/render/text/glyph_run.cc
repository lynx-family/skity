// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/text/glyph_run.hpp"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <optional>
#include <skity/geometry/matrix.hpp>
#include <skity/graphic/paint.hpp>

#include "src/render/hw/draw/geometry/wgsl_text_geometry.hpp"
#include "src/render/hw/draw/hw_dynamic_text_draw.hpp"
#include "src/render/hw/draw/wgx_utils.hpp"
#include "src/render/hw/hw_path_raster.hpp"
#include "src/render/hw/hw_stage_buffer.hpp"
#include "src/render/text/glyph_position.hpp"
#include "src/render/text/text_render_control.hpp"
#include "src/render/text/transformed_mask_glyph_run.hpp"
#include "src/tracing.hpp"
#include "src/utils/arena_allocator.hpp"

namespace skity {

struct GlyphRegionWithIndex {
  uint32_t index;
  GlyphRegion region;
  Vec2 position{};
};

class DirectGlyphRun : public GlyphRun {
 public:
  DirectGlyphRun(const Point& origin, const Paint& paint, const bool is_stroke,
                 std::vector<GlyphRegionWithIndex> glyph_locs,
                 uint32_t group_index, Atlas* atlas, GlyphFormat glyph_format)
      : origin_(origin),
        paint_(paint),
        is_stroke_(is_stroke),
        glyph_locs_(std::move(glyph_locs)),
        group_index_(group_index),
        atlas_(atlas),
        glyph_format_(glyph_format) {}

  ~DirectGlyphRun() override = default;

  ArrayList<GlyphRect, 16> Raster(float canvas_scale,
                                  ArenaAllocator* arena_allocator);

  GlyphDrawList Draw(Matrix transform, const Matrix& glyph_to_layer,
                     ArenaAllocator* arena_allocator, float canvas_scale,
                     bool use_linear_text_filter,
                     bool split_overlapping_glyphs) override;

  bool HasCoverageMask() const override {
    return glyph_format_ == GlyphFormat::A8;
  }

  bool IsSourceOpaque() const override {
    return HasCoverageMask() && IsPaintSourceOpaque(paint_);
  }

  static GlyphRunList SubRunListByTexture(
      const uint32_t count, const GlyphID* glyphs, const Point& origin,
      const float* position_x, const float* position_y, const Font& font,
      const Paint& paint, AtlasFormat format, float context_scale,
      const Matrix& transform, const bool is_stroke,
      AtlasManager* atlas_manager, ArenaAllocator* arena_allocator);

 private:
  const Point origin_;
  const Paint paint_;
  const bool is_stroke_;
  std::vector<GlyphRegionWithIndex> glyph_locs_;
  uint32_t group_index_;
  Atlas* atlas_;
  GlyphFormat glyph_format_;
};

ArrayList<GlyphRect, 16> DirectGlyphRun::Raster(
    float canvas_scale, ArenaAllocator* arena_allocator) {
  ArrayList<GlyphRect, 16> glyph_rects;
  glyph_rects.SetArenaAllocator(arena_allocator);
  if (glyph_locs_.empty()) {
    return glyph_rects;
  }

  for (uint32_t k = 0; k < glyph_locs_.size(); k++) {
    Vec2 uv_lt = atlas_->CalculateUV(glyph_locs_[k].region.index_in_group,
                                     glyph_locs_[k].region.loc.x,
                                     glyph_locs_[k].region.loc.y);
    Vec2 uv_rb = atlas_->CalculateUV(
        glyph_locs_[k].region.index_in_group,
        glyph_locs_[k].region.loc.x + glyph_locs_[k].region.loc.z,
        glyph_locs_[k].region.loc.y + glyph_locs_[k].region.loc.w);

    auto origin_x = glyph_locs_[k].region.origin_x;
    auto origin_y = glyph_locs_[k].region.origin_y;

    float rx = glyph_locs_[k].position.x + origin_x;
    float ry = glyph_locs_[k].position.y - origin_y;
    float rw = (uv_rb.x - uv_lt.x) / canvas_scale;
    float rh = (uv_rb.y - uv_lt.y) / canvas_scale;

    if (rh == 0) {
      continue;
    }

    Vec4 bounds = {rx, ry, rx + rw, ry + rh};
    glyph_rects.emplace_back(bounds, uv_lt, uv_rb);
  }

  return glyph_rects;
}

GlyphDrawList DirectGlyphRun::Draw(Matrix transform,
                                   const Matrix& glyph_to_layer,
                                   ArenaAllocator* arena_allocator,
                                   float canvas_scale,
                                   bool use_linear_text_filter,
                                   bool split_overlapping_glyphs) {
  SKITY_TRACE_EVENT(DirectGlyphRun_Draw);
  ArrayList<GlyphRect, 16> glyph_rects = Raster(canvas_scale, arena_allocator);

  atlas_->UploadAtlas(group_index_);
  auto gpu_texture = atlas_->GetGPUTexture(group_index_);

  Matrix text_transform = transform * Matrix::Translate(origin_.x, origin_.y);
  Matrix final_transform =
      HWDynamicTextDraw::CalcTransform(transform, text_transform);
  const Matrix glyph_rect_to_layer = glyph_to_layer * final_transform;
  auto batches =
      BuildGlyphRectBatches(std::move(glyph_rects), glyph_rect_to_layer,
                            arena_allocator, split_overlapping_glyphs);
  auto gpu_sampler = atlas_->GetGPUSampler(
      group_index_, use_linear_text_filter ? GPUFilterMode::kLinear
                                           : GPUFilterMode::kNearest);
  const auto effect = atlas_->GetFormat() == AtlasFormat::A8
                          ? TextAtlasEffect::kA8Coverage
                          : (glyph_format_ == GlyphFormat::BGRA32
                                 ? TextAtlasEffect::kColorSwizzleRB
                                 : TextAtlasEffect::kColor);
  const Vector color =
      is_stroke_ ? paint_.GetStrokeColor() : paint_.GetFillColor();
  Paint paint_copy = paint_;
  paint_copy.SetFillColor(color);
  paint_copy.SetStrokeColor(color);
  std::optional<Matrix> device_to_local;
  if (effect == TextAtlasEffect::kA8Coverage && paint_.GetShader()) {
    Matrix inverse;
    (glyph_to_layer * transform).Invert(&inverse);
    device_to_local = inverse;
  }

  GlyphDrawList draws;
  draws.SetArenaAllocator(arena_allocator);
  for (auto& batch : batches) {
    auto* draw = arena_allocator->Make<HWDynamicTextDraw>(
        glyph_to_layer, final_transform, std::move(batch.glyph_rects),
        paint_copy, gpu_texture, gpu_sampler, effect, is_stroke_,
        device_to_local);
    draws.push_back({draw, paint_copy.ComputeFastBounds(batch.bounds)});
  }
  return draws;
}

GlyphRunList DirectGlyphRun::SubRunListByTexture(
    const uint32_t count, const GlyphID* glyphs, const Point& origin,
    const float* position_x, const float* position_y, const Font& font,
    const Paint& paint, AtlasFormat format, float context_scale,
    const Matrix& transform, const bool is_stroke, AtlasManager* atlas_manager,
    ArenaAllocator* arena_allocator) {
  GlyphRunList run_list;
  run_list.SetArenaAllocator(arena_allocator);
  std::vector<GlyphRegionWithIndex> glyph_regions;
  uint32_t max_index = 0;

  std::vector<const GlyphData*> glyph_info(count);
  Paint metrics_paint;
  if (is_stroke) {
    metrics_paint.SetStyle(Paint::kStroke_Style);
    metrics_paint.SetStrokeWidth(paint.GetStrokeWidth());
    metrics_paint.SetStrokeCap(paint.GetStrokeCap());
    metrics_paint.SetStrokeJoin(paint.GetStrokeJoin());
    metrics_paint.SetStrokeMiter(paint.GetStrokeMiter());
  } else {
    metrics_paint.SetStyle(Paint::kFill_Style);
  }
  font.LoadGlyphMetrics(glyphs, count, glyph_info.data(), metrics_paint);
  GlyphFormat glyph_format = *glyph_info[0]->GetFormat();

  Vec2 device_origin{0.f, 0.f};
  Vec2 device_zero{0.f, 0.f};
  const Vec2 local_origin{origin.x, origin.y};
  const Vec2 local_zero{0.f, 0.f};
  transform.MapPoints(&device_origin, &local_origin, 1);
  transform.MapPoints(&device_zero, &local_zero, 1);
  const Vec2 origin_offset = device_origin - device_zero;

  GlyphPositionRoundingSpec rounding_spec;
  rounding_spec.is_subpixel = font.IsSubpixel();
  rounding_spec.axis_alignment =
      ComputeAxisAlignmentForHorizontalText(font.IsBaselineSnap(), transform);

  Atlas* atlas = atlas_manager->GetAtlas(format);
  uint32_t k = 0;
  while (k < count) {
    auto info = *(glyph_info[k]);

    const Vec2 run_pos{position_x[k] + origin.x, position_y[k] + origin.y};
    Vec2 device_run_pos{0.f, 0.f};
    transform.MapPoints(&device_run_pos, &run_pos, 1);
    const QuantizedGlyphPosition glyph_position =
        QuantizeGlyphPosition(device_run_pos, context_scale, rounding_spec);

    GlyphRegion glyph_region =
        atlas->GetGlyphRegion(font,
                              PackedGlyphID(info.Id(), glyph_position.x_phase,
                                            glyph_position.y_phase),
                              paint, false, context_scale, transform);
    if (glyph_region.loc.z == 0 || glyph_region.loc.w == 0) {
      k++;
      continue;
    }

    if (max_index < glyph_region.index_in_group) {
      max_index = glyph_region.index_in_group;
    }

    glyph_regions.push_back(
        {k, glyph_region, glyph_position.position - origin_offset});
    k++;
  }

  uint32_t draw_count =
      max_index / atlas->GetConfig().max_num_bitmap_per_atlas + 1;
  if (draw_count == 1) {
    if (!glyph_regions.empty()) {
      run_list.push_back(arena_allocator->Make<DirectGlyphRun>(
          origin, paint, is_stroke, std::move(glyph_regions),
          static_cast<uint32_t>(0), atlas, glyph_format));
    }
  } else {
    std::vector<std::vector<GlyphRegionWithIndex>> glyph_region_groups(
        draw_count);

    k = 0;
    while (k < glyph_regions.size()) {
      uint32_t group_index = glyph_regions[k].region.index_in_group /
                             atlas->GetConfig().max_num_bitmap_per_atlas;
      GlyphRegion region = glyph_regions[k].region;
      region.index_in_group %= atlas->GetConfig().max_num_bitmap_per_atlas;
      glyph_region_groups[group_index].push_back(
          {glyph_regions[k].index, region, glyph_regions[k].position});
      k++;
    }

    for (uint32_t group_index = 0; group_index < glyph_region_groups.size();
         group_index++) {
      run_list.push_back(arena_allocator->Make<DirectGlyphRun>(
          origin, paint, is_stroke, std::move(glyph_region_groups[group_index]),
          group_index, atlas, glyph_format));
    }
  }

  return run_list;
}

class SDFGlyphRun : public GlyphRun {
 public:
  static GlyphRunList SubRunListByTexture(
      const uint32_t count, const GlyphID* glyphs, const Point& origin,
      const float* position_x, const float* position_y, const Font& font,
      const Paint& paint, float context_scale, const Matrix& transform,
      AtlasManager* atlas_manager, ArenaAllocator* arena_allocator);

  SDFGlyphRun(const uint32_t count, const GlyphID* glyphs, const Point& origin,
              const float* position_x, const float* position_y,
              const Font& font, const Paint& paint,
              std::vector<GlyphRegionWithIndex> glyph_locs,
              uint32_t group_index, Atlas* atlas)
      : count_(count),
        glyphs_(glyphs, glyphs + count),
        origin_(origin),
        position_x_(position_x, position_x + count),
        position_y_(position_y, position_y + count),
        font_(font),
        paint_(paint),
        glyph_locs_(std::move(glyph_locs)),
        group_index_(group_index),
        atlas_(atlas) {}

  ~SDFGlyphRun() override = default;

  ArrayList<GlyphRect, 16> Raster(float canvas_scale,
                                  ArenaAllocator* arena_allocator);

  GlyphDrawList Draw(Matrix transform, const Matrix& glyph_to_layer,
                     ArenaAllocator* arena_allocator, float canvas_scale,
                     bool enable_text_linear_filter,
                     bool split_overlapping_glyphs) override;

  bool HasCoverageMask() const override { return true; }

  bool IsSourceOpaque() const override { return IsPaintSourceOpaque(paint_); }

 private:
  uint32_t count_;
  std::vector<GlyphID> glyphs_;
  const Point origin_;
  std::vector<float> position_x_;
  std::vector<float> position_y_;
  const Font font_;
  const Paint paint_;
  std::vector<GlyphRegionWithIndex> glyph_locs_;
  uint32_t group_index_;
  Atlas* atlas_;
};

ArrayList<GlyphRect, 16> SDFGlyphRun::Raster(float canvas_scale,
                                             ArenaAllocator* arena_allocator) {
  float max_height = 0.f;
  int16_t max_bearing_y = 0.f;

  std::vector<const GlyphData*> glyph_info(count_);
  font_.LoadGlyphMetrics(glyphs_.data(), count_, glyph_info.data(), paint_);

  ArrayList<GlyphRect, 16> glyph_rects;
  glyph_rects.SetArenaAllocator(arena_allocator);
  for (uint32_t k = 0; k < glyph_locs_.size(); k++) {
    auto info = *(glyph_info[glyph_locs_[k].index]);

    Vec2 uv_lt = atlas_->CalculateUV(glyph_locs_[k].region.index_in_group,
                                     glyph_locs_[k].region.loc.x,
                                     glyph_locs_[k].region.loc.y);
    Vec2 uv_rb = atlas_->CalculateUV(
        glyph_locs_[k].region.index_in_group,
        glyph_locs_[k].region.loc.x + glyph_locs_[k].region.loc.z,
        glyph_locs_[k].region.loc.y + glyph_locs_[k].region.loc.w);
    // DirectGlyph rendering use image origin point calculate the vertex
    // position.
    // SDF not needs to do the same thing in the future.
    float rx = position_x_[glyph_locs_[k].index] + info.GetHoriBearingX();
    float ry = position_y_[glyph_locs_[k].index] - info.GetHoriBearingY();
    float rw = (uv_rb.x - uv_lt.x) * glyph_locs_[k].region.scale / canvas_scale;
    float rh = (uv_rb.y - uv_lt.y) * glyph_locs_[k].region.scale / canvas_scale;

    if (font_.GetFixedSize() != 0.f) {
      float scale = font_.GetSize() * canvas_scale / font_.GetFixedSize();
      rw *= scale;
      rh *= scale;
    }

    max_height = std::max(rh, max_height);
    max_bearing_y = std::fmax(max_bearing_y, info.GetHoriBearingY());
    if (rh == 0) {
      continue;
    }

    Vec4 bounds = {rx, ry, rx + rw, ry + rh};
    glyph_rects.emplace_back(bounds, uv_lt, uv_rb);
  }

  return glyph_rects;
}

GlyphDrawList SDFGlyphRun::Draw(Matrix transform, const Matrix& glyph_to_layer,
                                ArenaAllocator* arena_allocator,
                                float canvas_scale, bool,
                                bool split_overlapping_glyphs) {
  SKITY_TRACE_EVENT(SDFGlyphRun_Draw);

  ArrayList<GlyphRect, 16> glyph_rects = Raster(canvas_scale, arena_allocator);
  atlas_->UploadAtlas(group_index_);
  auto gpu_texture = atlas_->GetGPUTexture(group_index_);
  Matrix text_transform = transform * Matrix::Translate(origin_.x, origin_.y);
  Matrix final_transform =
      HWDynamicTextDraw::CalcSDFTransform(text_transform, 1.0f);
  const Matrix glyph_rect_to_layer = glyph_to_layer * final_transform;
  auto batches =
      BuildGlyphRectBatches(std::move(glyph_rects), glyph_rect_to_layer,
                            arena_allocator, split_overlapping_glyphs);

  auto gpu_sampler =
      atlas_->GetGPUSampler(group_index_, GPUFilterMode::kLinear);
  std::optional<Matrix> device_to_local;
  if (paint_.GetShader()) {
    Matrix inverse;
    (glyph_to_layer * transform).Invert(&inverse);
    device_to_local = inverse;
  }

  GlyphDrawList draws;
  draws.SetArenaAllocator(arena_allocator);
  for (auto& batch : batches) {
    auto* draw = arena_allocator->Make<HWDynamicTextDraw>(
        glyph_to_layer, final_transform, std::move(batch.glyph_rects), paint_,
        gpu_texture, gpu_sampler, TextAtlasEffect::kSDFCoverage, false,
        device_to_local);
    draws.push_back({draw, paint_.ComputeFastBounds(batch.bounds)});
  }
  return draws;
}

GlyphRunList SDFGlyphRun::SubRunListByTexture(
    const uint32_t count, const GlyphID* glyphs, const Point& origin,
    const float* position_x, const float* position_y, const Font& font,
    const Paint& paint, float context_scale, const Matrix& transform,
    AtlasManager* atlas_manager, ArenaAllocator* arena_allocator) {
  GlyphRunList run_list;
  std::vector<GlyphRegionWithIndex> glyph_regions;
  uint32_t max_index = 0;

  std::vector<const GlyphData*> glyph_info(count);
  font.LoadGlyphMetrics(glyphs, count, glyph_info.data(), paint);
  AtlasFormat format = AtlasFormat::A8;
  Atlas* atlas = atlas_manager->GetAtlas(format);
  uint32_t k = 0;
  while (k < count) {
    auto info = *(glyph_info[k]);
    GlyphRegion glyph_region = atlas->GetGlyphRegion(
        font, PackedGlyphID(info.Id()), paint, true, context_scale, transform);
    if (glyph_region.loc.z == 0 || glyph_region.loc.w == 0) {
      k++;
      continue;
    }

    if (max_index < glyph_region.index_in_group) {
      max_index = glyph_region.index_in_group;
    }

    glyph_regions.push_back({k, glyph_region});
    k++;
  }

  uint32_t draw_count =
      max_index / atlas->GetConfig().max_num_bitmap_per_atlas + 1;
  if (draw_count == 1) {
    if (!glyph_regions.empty()) {
      run_list.push_back(arena_allocator->Make<SDFGlyphRun>(
          count, glyphs, origin, position_x, position_y, font, paint,
          std::move(glyph_regions), 0U, atlas));
    }
  } else {
    std::vector<std::vector<GlyphRegionWithIndex>> glyph_region_groups(
        draw_count);

    k = 0;
    while (k < glyph_regions.size()) {
      uint32_t group_index = glyph_regions[k].region.index_in_group /
                             atlas->GetConfig().max_num_bitmap_per_atlas;
      glyph_region_groups[group_index].push_back(
          {glyph_regions[k].index,
           {glyph_regions[k].region.index_in_group %
                atlas->GetConfig().max_num_bitmap_per_atlas,
            glyph_regions[k].region.loc, glyph_regions[k].region.scale}});
      k++;
    }

    for (uint32_t group_index = 0; group_index < glyph_region_groups.size();
         group_index++) {
      run_list.push_back(arena_allocator->Make<SDFGlyphRun>(
          count, glyphs, origin, position_x, position_y, font, paint,
          std::move(glyph_region_groups[group_index]), group_index, atlas));
    }
  }

  return run_list;
}

class PathGlyphRun : public GlyphRun {
 public:
  PathGlyphRun(const Path& path, const float position_x, const float position_y,
               const Paint& paint, DrawPathFunc draw_path_func)
      : path_(path),
        position_x_(position_x),
        position_y_(position_y),
        paint_(paint),
        draw_path_func_(std::move(draw_path_func)) {}

  ~PathGlyphRun() override = default;

  GlyphDrawList Draw(Matrix transform, const Matrix& glyph_to_layer,
                     ArenaAllocator* arena_allocator, float canvas_scale,
                     bool enable_text_linear_filter,
                     bool split_overlapping_glyphs) override;

  bool HasCoverageMask() const override { return false; }

  bool IsSourceOpaque() const override { return false; }

 private:
  const Path path_;
  const float position_x_;
  const float position_y_;
  const Paint& paint_;
  DrawPathFunc draw_path_func_;
};

GlyphDrawList PathGlyphRun::Draw(Matrix, const Matrix&,
                                 ArenaAllocator* arena_allocator, float, bool,
                                 bool) {
  SKITY_TRACE_EVENT(PathGlyphRun_Draw);
  Matrix glyph_transform = Matrix::Translate(position_x_, position_y_);
  Path path = path_.CopyWithMatrix(glyph_transform);

  // Consider to extract isolated path renderer.
  draw_path_func_(path, paint_);
  GlyphDrawList draws;
  draws.SetArenaAllocator(arena_allocator);
  return draws;
}

GlyphRun::~GlyphRun() = default;

GlyphRunList GlyphRun::Make(const uint32_t count, const GlyphID* glyphs,
                            const Point& origin, const float* position_x,
                            const float* position_y, const Font& font,
                            const Paint& paint, float context_scale,
                            const Matrix& transform,
                            AtlasManager* atlas_manager,
                            ArenaAllocator* arena_allocator,
                            DrawPathFunc draw_path_func) {
  std::vector<const GlyphData*> glyph_info(count);
  Paint metrics_paint;
  metrics_paint.SetStyle(Paint::kFill_Style);
  font.LoadGlyphMetrics(glyphs, count, glyph_info.data(), metrics_paint);

  uint32_t a8_count = 0;
  for (uint32_t index = 0; index < count; index++) {
    if (glyph_info[index]->GetFormat() == GlyphFormat::A8) {
      a8_count++;
    }
  }

  if (a8_count == count) {
    return MakeInternal(count, glyphs, origin, position_x, position_y, font,
                        paint, AtlasFormat::A8, context_scale, transform,
                        atlas_manager, arena_allocator, draw_path_func);
  }

  if (a8_count == 0) {
    return MakeInternal(count, glyphs, origin, position_x, position_y, font,
                        paint, AtlasFormat::RGBA32, context_scale, transform,
                        atlas_manager, arena_allocator, draw_path_func);
  }

  const uint32_t rgba_count = count - a8_count;
  std::vector<GlyphID> A8_glyph(a8_count);
  std::vector<float> A8_position_x(a8_count);
  std::vector<float> A8_position_y(a8_count);
  std::vector<GlyphID> RGBA_glyph(rgba_count);
  std::vector<float> RGBA_position_x(rgba_count);
  std::vector<float> RGBA_position_y(rgba_count);

  uint32_t a8_index = 0;
  uint32_t rgba_index = 0;
  for (uint32_t index = 0; index < count; index++) {
    if (glyph_info[index]->GetFormat() == GlyphFormat::A8) {
      A8_glyph[a8_index] = glyphs[index];
      A8_position_x[a8_index] = position_x[index];
      A8_position_y[a8_index] = position_y[index];
      a8_index++;
    } else {
      RGBA_glyph[rgba_index] = glyphs[index];
      RGBA_position_x[rgba_index] = position_x[index];
      RGBA_position_y[rgba_index] = position_y[index];
      rgba_index++;
    }
  }

  GlyphRunList result;
  result.SetArenaAllocator(arena_allocator);

  GlyphRunList A8_list = MakeInternal(
      a8_count, A8_glyph.data(), origin, A8_position_x.data(),
      A8_position_y.data(), font, paint, AtlasFormat::A8, context_scale,
      transform, atlas_manager, arena_allocator, draw_path_func);
  for (auto& item : A8_list) {
    result.push_back(item);
  }

  GlyphRunList RGBA_list = MakeInternal(
      rgba_count, RGBA_glyph.data(), origin, RGBA_position_x.data(),
      RGBA_position_y.data(), font, paint, AtlasFormat::RGBA32, context_scale,
      transform, atlas_manager, arena_allocator, draw_path_func);
  for (auto& item : RGBA_list) {
    result.push_back(item);
  }

  return result;
}

GlyphRunList GlyphRun::MakeInternal(
    const uint32_t count, const GlyphID* glyphs, const Point& origin,
    const float* position_x, const float* position_y, const Font& font,
    const Paint& paint, AtlasFormat format, float context_scale,
    const Matrix& transform, AtlasManager* atlas_manager,
    ArenaAllocator* arena_allocator, DrawPathFunc draw_path_func) {
  SKITY_TRACE_EVENT(GlyphRun_MakeInternal);
  TextRenderControl control{true};
  GlyphRunList run_list;
  run_list.SetArenaAllocator(arena_allocator);

  float sx = Vec2{transform.GetScaleX(), transform.GetSkewY()}.Length();
  float sy = Vec2{transform.GetSkewX(), transform.GetScaleY()}.Length();
  float maximun_text_scale =
      std::abs(glm::max(sx * context_scale, sy * context_scale));
  if (format == AtlasFormat::RGBA32 &&
      atlas_manager->IsLargeEmojiAtlasEnabled() && !transform.HasPersp()) {
    constexpr float kMaxLargeEmojiTextSize = 1024.f;
    const float effective_text_size = font.GetSize() * maximun_text_scale;
    if (effective_text_size >= paint.GetFontThreshold() &&
        effective_text_size < kMaxLargeEmojiTextSize) {
      Atlas* atlas = atlas_manager->GetAtlas(format);
      if (atlas->GetConfig().is_large_emoji_atlas) {
        Paint working_paint = paint;
        working_paint.SetStyle(Paint::kFill_Style);
        return DirectGlyphRun::SubRunListByTexture(
            count, glyphs, origin, position_x, position_y, font, working_paint,
            format, context_scale, transform, false, atlas_manager,
            arena_allocator);
      }
    }
  }

  if (control.CanUseDirect(font.GetSize() * maximun_text_scale, transform,
                           paint, font.GetTypeface())) {
    // texture
    Paint working_paint = paint;
    if (format == AtlasFormat::RGBA32) {
      working_paint.SetStyle(Paint::kFill_Style);
      GlyphRunList sub_run_list = DirectGlyphRun::SubRunListByTexture(
          count, glyphs, origin, position_x, position_y, font, working_paint,
          format, context_scale, transform, false, atlas_manager,
          arena_allocator);
      for (auto& sub_run : sub_run_list) {
        run_list.push_back(sub_run);
      }
    } else {
      if (paint.GetStyle() != Paint::kStroke_Style) {
        Paint working_paint = paint;
        working_paint.SetStyle(paint.GetStyle() == Paint::kStrokeThenFill_Style
                                   ? Paint::kStroke_Style
                                   : Paint::kFill_Style);
        GlyphRunList sub_run_list = DirectGlyphRun::SubRunListByTexture(
            count, glyphs, origin, position_x, position_y, font, working_paint,
            format, context_scale, transform,
            paint.GetStyle() == Paint::kStrokeThenFill_Style, atlas_manager,
            arena_allocator);
        for (auto& sub_run : sub_run_list) {
          run_list.push_back(sub_run);
        }
      }
      if (paint.GetStyle() != Paint::kFill_Style) {
        Paint working_paint = paint;
        working_paint.SetStyle(paint.GetStyle() == Paint::kStrokeThenFill_Style
                                   ? Paint::kFill_Style
                                   : Paint::kStroke_Style);
        GlyphRunList sub_run_list = DirectGlyphRun::SubRunListByTexture(
            count, glyphs, origin, position_x, position_y, font, working_paint,
            format, context_scale, transform,
            paint.GetStyle() != Paint::kStrokeThenFill_Style, atlas_manager,
            arena_allocator);
        for (auto& sub_run : sub_run_list) {
          run_list.push_back(sub_run);
        }
      }
    }
  } else if (transform.HasPersp()) {
    if (format == AtlasFormat::RGBA32) {
      Paint working_paint = paint;
      working_paint.SetStyle(Paint::kFill_Style);
      run_list = transformed_mask::MakeGlyphRunList(
          count, glyphs, origin, position_x, position_y, font, working_paint,
          format, context_scale, transform, false, atlas_manager,
          arena_allocator);
    } else {
      std::vector<const GlyphData*> glyph_data(count);
      font.LoadGlyphPath(glyphs, count, glyph_data.data());

      std::vector<GlyphID> mask_glyphs;
      std::vector<float> mask_position_x;
      std::vector<float> mask_position_y;
      mask_glyphs.reserve(count);
      mask_position_x.reserve(count);
      mask_position_y.reserve(count);

      auto append_mask_runs = [&](const Paint& working_paint) {
        GlyphRunList mask_runs = transformed_mask::MakeGlyphRunList(
            static_cast<uint32_t>(mask_glyphs.size()), mask_glyphs.data(),
            origin, mask_position_x.data(), mask_position_y.data(), font,
            working_paint, format, context_scale, transform, false,
            atlas_manager, arena_allocator);
        for (auto* mask_run : mask_runs) {
          run_list.push_back(mask_run);
        }
      };

      auto flush_mask_runs = [&]() {
        if (mask_glyphs.empty()) {
          return;
        }
        // Bitmap-only glyphs have no outline to stroke. Match the bitmap text
        // fallback by drawing a single fill mask for every paint style.
        Paint working_paint = transformed_mask::MakeBitmapOnlyFillPaint(paint);
        append_mask_runs(working_paint);
        mask_glyphs.clear();
        mask_position_x.clear();
        mask_position_y.clear();
      };

      for (uint32_t index = 0; index < count; ++index) {
        if (glyph_data[index] != nullptr &&
            !glyph_data[index]->GetPath().IsEmpty()) {
          // Preserve paint order when outline and bitmap-only glyphs overlap.
          flush_mask_runs();
          Path path = glyph_data[index]->GetPath().CopyWithMatrix(
              Matrix::Translate(origin.x, origin.y));
          run_list.push_back(arena_allocator->Make<PathGlyphRun>(
              path, position_x[index], position_y[index], paint,
              draw_path_func));
        } else {
          // Bitmap-only glyphs (for example bitmap emoji in an A8 font) have
          // no outline to feed the path renderer. Keep them in local text
          // coordinates and let the transformed-mask path rasterize them.
          mask_glyphs.push_back(glyphs[index]);
          mask_position_x.push_back(position_x[index]);
          mask_position_y.push_back(position_y[index]);
        }
      }
      flush_mask_runs();
    }
  } else if (control.CanUseSDF(maximun_text_scale, paint,  // NOLINT
                               font.GetTypeface())) {
    // sdf
    run_list = SDFGlyphRun::SubRunListByTexture(
        count, glyphs, origin, position_x, position_y, font, paint,
        context_scale, transform, atlas_manager, arena_allocator);
  } else {  // NOLINT
    // path
    std::vector<const GlyphData*> glyph_data(count);
    font.LoadGlyphPath(glyphs, count, glyph_data.data());
    for (uint32_t k = 0; k < count; k++) {
      Path path = glyph_data[k]->GetPath();
      if (path.IsEmpty()) {
        // maybe is empty white space
        continue;
      }
      path = path.CopyWithMatrix(Matrix::Translate(origin.x, origin.y));
      run_list.push_back(arena_allocator->Make<PathGlyphRun>(
          path, position_x[k], position_y[k], paint, draw_path_func));
    }
  }

  return run_list;
}

}  // namespace skity
