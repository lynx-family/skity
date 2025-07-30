// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/text/glyph_run.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <skity/graphic/paint.hpp>

#include "src/render/hw/draw/fragment/wgsl_text_fragment.hpp"
#include "src/render/hw/draw/geometry/wgsl_text_geometry.hpp"
#include "src/render/hw/draw/hw_dynamic_text_draw.hpp"
#include "src/render/hw/draw/wgx_filter.hpp"
#include "src/render/hw/hw_path_raster.hpp"
#include "src/render/hw/hw_stage_buffer.hpp"
#include "src/render/text/text_render_control.hpp"
#include "src/tracing.hpp"
#include "src/utils/arena_allocator.hpp"

namespace skity {

struct GlyphRegionWithIndex {
  uint32_t index;
  GlyphRegion region;
};

class DirectGlyphRun : public GlyphRun {
 public:
  DirectGlyphRun(const uint32_t count, const GlyphID* glyphs,
                 const Point& origin, const float* position_x,
                 const float* position_y, const Font& font, float context_scale,
                 const Matrix& transform, const Paint& paint,
                 const bool is_stroke,
                 std::vector<GlyphRegionWithIndex> glyph_locs,
                 uint32_t group_index, Atlas* atlas, GlyphFormat glyph_format)
      : count_(count),
        glyphs_(glyphs),
        origin_(origin),
        position_x_(position_x),
        position_y_(position_y),
        font_(font),
        context_scale_(context_scale),
        transform_(transform),
        paint_(paint),
        is_stroke_(is_stroke),
        glyph_locs_(std::move(glyph_locs)),
        group_index_(group_index),
        atlas_(atlas),
        glyph_format_(glyph_format) {}

  ~DirectGlyphRun() override = default;

  ArrayList<GlyphRect, 16> Raster(float canvas_scale,
                                  ArenaAllocator* arena_allocator);

  HWDraw* Draw(Matrix transform, ArenaAllocator* arena_allocator,
               float canvas_scale, bool use_linear_text_filter) override;

  Rect GetBounds() override { return bounds_; }

  bool IsStroke() override { return is_stroke_; }

  static GlyphRunList SubRunListByTexture(
      const uint32_t count, const GlyphID* glyphs, const Point& origin,
      const float* position_x, const float* position_y, const Font& font,
      const Paint& paint, float context_scale, const Matrix& transform,
      const bool is_stroke, AtlasManager* atlas_manager,
      ArenaAllocator* arena_allocator);

 private:
  uint32_t count_;
  const GlyphID* glyphs_;
  const Point origin_;
  const float* position_x_;
  const float* position_y_;
  const Font font_;
  float context_scale_;
  Matrix transform_;
  const Paint paint_;
  const bool is_stroke_;
  std::vector<GlyphRegionWithIndex> glyph_locs_;
  uint32_t group_index_;
  Atlas* atlas_;
  GlyphFormat glyph_format_;

  Rect bounds_;
};

ArrayList<GlyphRect, 16> DirectGlyphRun::Raster(
    float canvas_scale, ArenaAllocator* arena_allocator) {
  float max_height = 0.f;
  int16_t max_bearing_y = 0.f;

  std::vector<const GlyphData*> glyph_info(count_);
  Paint metrics_paint;
  if (is_stroke_) {
    metrics_paint.SetStyle(Paint::kStroke_Style);
    metrics_paint.SetStrokeWidth(paint_.GetStrokeWidth());
    metrics_paint.SetStrokeCap(paint_.GetStrokeCap());
    metrics_paint.SetStrokeJoin(paint_.GetStrokeJoin());
    metrics_paint.SetStrokeMiter(paint_.GetStrokeMiter());
  } else {
    metrics_paint.SetStyle(Paint::kFill_Style);
  }
  font_.LoadGlyphMetrics(glyphs_, count_, glyph_info.data(), metrics_paint);
  font_.LoadGlyphBitmapInfo(glyphs_, count_, glyph_info.data(), metrics_paint,
                            context_scale_, transform_);

  ArrayList<GlyphRect, 16> glyph_rects;
  glyph_rects.SetArenaAllocator(arena_allocator);
  bounds_ = Rect::MakeEmpty();
  for (uint32_t k = 0; k < glyph_locs_.size(); k++) {
    auto info = *(glyph_info[glyph_locs_[k].index]);

    Vec2 uv_lt = atlas_->CalculateUV(glyph_locs_[k].region.index_in_group,
                                     glyph_locs_[k].region.loc.x,
                                     glyph_locs_[k].region.loc.y);
    Vec2 uv_rb = atlas_->CalculateUV(
        glyph_locs_[k].region.index_in_group,
        glyph_locs_[k].region.loc.x + glyph_locs_[k].region.loc.z,
        glyph_locs_[k].region.loc.y + glyph_locs_[k].region.loc.w);

    auto origin_x = info.Image().origin_x;
    auto origin_y = info.Image().origin_y;

    const Vec2 run_pos{position_x_[glyph_locs_[k].index],
                       position_y_[glyph_locs_[k].index]};
    Vec2 device_run_pos{0, 0};
    transform_.MapPoints(&device_run_pos, &run_pos, 1);

    float rx = device_run_pos.x + origin_x;
    float ry = device_run_pos.y - origin_y;
    float rw = (uv_rb.x - uv_lt.x) / canvas_scale;
    float rh = (uv_rb.y - uv_lt.y) / canvas_scale;

    // if (font_.GetFixedSize() != 0.f) {
    //   float scale = font_.GetSize() * canvas_scale / font_.GetFixedSize();
    //   rw *= scale;
    //   rh *= scale;
    // }

    max_height = std::max(rh, max_height);
    max_bearing_y = std::fmax(max_bearing_y, info.GetHoriBearingY());
    if (rh == 0) {
      continue;
    }

    Vec4 bounds = {rx, ry, rx + rw, ry + rh};
    bounds_.Join(Rect::MakeXYWH(rx, ry, rw, rh));

    glyph_rects.emplace_back(bounds, uv_lt, uv_rb);
  }

  return glyph_rects;
}

HWDraw* DirectGlyphRun::Draw(Matrix transform, ArenaAllocator* arena_allocator,
                             float canvas_scale, bool use_linear_text_filter) {
  SKITY_TRACE_EVENT(DirectGlyphRun_Draw);
  ArrayList<GlyphRect, 16> glyph_rects = Raster(canvas_scale, arena_allocator);

  Vector color = is_stroke_ ? paint_.GetStrokeColor() : paint_.GetFillColor();

  atlas_->UploadAtlas(group_index_);
  auto gpu_texture = atlas_->GetGPUTexture(group_index_);

  HWWGSLGeometry* geometry;
  if (paint_.GetShader()) {
    geometry = arena_allocator->Make<WGSLTextGradientGeometry>(
        std::move(glyph_rects), paint_.GetShader()->GetLocalMatrix(),
        transform);
  } else {
    geometry = arena_allocator->Make<WGSLTextSolidColorGeometry>(
        std::move(glyph_rects));
  }

  HWWGSLFragment* fragment;

  auto gpu_sampler = atlas_->GetGPUSampler(
      group_index_, use_linear_text_filter ? GPUFilterMode::kLinear
                                           : GPUFilterMode::kNearest);
  if (atlas_->GetFormat() == AtlasFormat::A8) {
    if (paint_.GetShader() && paint_.GetShader()->AsGradient(nullptr) !=
                                  Shader::GradientType::kNone) {
      // Text does not have image shader for now
      Shader::GradientInfo info{};
      auto type = paint_.GetShader()->AsGradient(&info);

      fragment = arena_allocator->Make<WGSLGradientTextFragment>(
          std::move(gpu_texture), std::move(gpu_sampler), info, type,
          paint_.GetAlphaF());
    } else {
      fragment = arena_allocator->Make<WGSLColorTextFragment>(
          std::move(gpu_texture), std::move(gpu_sampler), color);
    }
  } else {
    fragment = arena_allocator->Make<WGSLColorEmojiFragment>(
        std::move(gpu_texture), std::move(gpu_sampler),
        glyph_format_ == GlyphFormat::BGRA32, paint_.GetAlphaF());
  }

  if (paint_.GetColorFilter()) {
    fragment->SetFilter(WGXFilterFragment::Make(paint_.GetColorFilter().get()));
  }

  Matrix text_transform = transform * Matrix::Translate(origin_.x, origin_.y);
  HWDynamicTextDraw* text_draw = arena_allocator->Make<HWDynamicTextDraw>(
      transform, text_transform, paint_.GetBlendMode(), geometry, fragment);
  bounds_ = text_draw->GetTransform().MapRect(bounds_);
  return text_draw;
}

GlyphRunList DirectGlyphRun::SubRunListByTexture(
    const uint32_t count, const GlyphID* glyphs, const Point& origin,
    const float* position_x, const float* position_y, const Font& font,
    const Paint& paint, float context_scale, const Matrix& transform,
    const bool is_stroke, AtlasManager* atlas_manager,
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
  GlyphFormat format =
      font.GetTypeface()
          ? (font.GetTypeface()->ContainsColorTable() ? GlyphFormat::BGRA32
                                                      : GlyphFormat::A8)
          : GlyphFormat::A8;
  if (count > 0 && glyph_info[0]->GetFormat().has_value()) {
    format = *glyph_info[0]->GetFormat();
  }
  Atlas* atlas = atlas_manager->GetAtlas(FromGlyphFormat(format));
  uint32_t k = 0;
  while (k < count) {
    auto info = *(glyph_info[k]);
    GlyphRegion glyph_region = atlas->GetGlyphRegion(
        font, info.Id(), paint, false, context_scale, transform);
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
      run_list.push_back(arena_allocator->Make<DirectGlyphRun>(
          count, glyphs, origin, position_x, position_y, font, context_scale,
          transform, paint, is_stroke, std::move(glyph_regions),
          static_cast<uint32_t>(0), atlas, format));
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
            glyph_regions[k].region.loc, 1.0f}});
      k++;
    }

    for (uint32_t group_index = 0; group_index < glyph_region_groups.size();
         group_index++) {
      run_list.push_back(arena_allocator->Make<DirectGlyphRun>(
          count, glyphs, origin, position_x, position_y, font, context_scale,
          transform, paint, is_stroke,
          std::move(glyph_region_groups[group_index]), group_index, atlas,
          format));
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
        glyphs_(glyphs),
        origin_(origin),
        position_x_(position_x),
        position_y_(position_y),
        font_(font),
        paint_(paint),
        glyph_locs_(std::move(glyph_locs)),
        group_index_(group_index),
        atlas_(atlas) {}

  ~SDFGlyphRun() override = default;

  ArrayList<GlyphRect, 16> Raster(float canvas_scale,
                                  ArenaAllocator* arena_allocator);

  HWDraw* Draw(Matrix transform, ArenaAllocator* arena_allocator,
               float canvas_scale, bool) override;

  Rect GetBounds() override { return bounds_; }

  bool IsStroke() override { return false; }

 private:
  uint32_t count_;
  const GlyphID* glyphs_;
  const Point origin_;
  const float* position_x_;
  const float* position_y_;
  const Font font_;
  const Paint paint_;
  std::vector<GlyphRegionWithIndex> glyph_locs_;
  uint32_t group_index_;
  Atlas* atlas_;

  Rect bounds_;
};

ArrayList<GlyphRect, 16> SDFGlyphRun::Raster(float canvas_scale,
                                             ArenaAllocator* arena_allocator) {
  float max_height = 0.f;
  int16_t max_bearing_y = 0.f;

  std::vector<const GlyphData*> glyph_info(count_);
  font_.LoadGlyphMetrics(glyphs_, count_, glyph_info.data(), paint_);

  ArrayList<GlyphRect, 16> glyph_rects;
  glyph_rects.SetArenaAllocator(arena_allocator);
  bounds_ = Rect::MakeEmpty();
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
    bounds_.Join(Rect::MakeXYWH(rx, ry, rw, rh));

    glyph_rects.emplace_back(bounds, uv_lt, uv_rb);
  }

  return glyph_rects;
}

HWDraw* SDFGlyphRun::Draw(Matrix transform, ArenaAllocator* arena_allocator,
                          float canvas_scale, bool) {
  SKITY_TRACE_EVENT(SDFGlyphRun_Draw);

  ArrayList<GlyphRect, 16> glyph_rects = Raster(canvas_scale, arena_allocator);
  atlas_->UploadAtlas(group_index_);
  auto gpu_texture = atlas_->GetGPUTexture(group_index_);
  Vector color = paint_.GetFillColor();

  WGSLTextSolidColorGeometry* geometry =
      arena_allocator->Make<WGSLTextSolidColorGeometry>(std::move(glyph_rects));

  auto gpu_sampler =
      atlas_->GetGPUSampler(group_index_, GPUFilterMode::kLinear);
  HWWGSLFragment* fragment = arena_allocator->Make<WGSLSdfColorTextFragment>(
      std::move(gpu_texture), std::move(gpu_sampler), color);

  if (paint_.GetColorFilter()) {
    fragment->SetFilter(WGXFilterFragment::Make(paint_.GetColorFilter().get()));
  }

  Matrix text_transform = transform * Matrix::Translate(origin_.x, origin_.y);
  // need to apply sdf scale to draw other than glyph
  HWDynamicSdfTextDraw* text_draw = arena_allocator->Make<HWDynamicSdfTextDraw>(
      text_transform, 1.f, paint_.GetBlendMode(), geometry, fragment);
  bounds_ = text_draw->GetTransform().MapRect(bounds_);
  return text_draw;
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
        font, info.Id(), paint, true, context_scale, transform);
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

  HWDraw* Draw(Matrix transform, ArenaAllocator* arena_allocator,
               float canvas_scale, bool) override;

  Rect GetBounds() override { return path_.GetBounds(); }

  bool IsStroke() override { return false; }

 private:
  const Path path_;
  const float position_x_;
  const float position_y_;
  const Paint& paint_;
  DrawPathFunc draw_path_func_;
};

HWDraw* PathGlyphRun::Draw(Matrix transform, ArenaAllocator* arena_allocator,
                           float canvas_scale, bool) {
  SKITY_TRACE_EVENT(PathGlyphRun_Draw);
  Matrix glyph_transform = Matrix::Translate(position_x_, position_y_);
  Path path = path_.CopyWithMatrix(glyph_transform);

  // Consider to extract isolated path renderer.
  draw_path_func_(path, paint_);
  return nullptr;
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
  SKITY_TRACE_EVENT(GlyphRun_Make);
  TextRenderControl control{true};
  GlyphRunList run_list;
  run_list.SetArenaAllocator(arena_allocator);

  float sx = Vec2{transform.GetScaleX(), transform.GetSkewY()}.Length();
  float sy = Vec2{transform.GetSkewX(), transform.GetScaleY()}.Length();
  float maximun_text_scale =
      std::abs(glm::max(sx * context_scale, sy * context_scale));
  if (control.CanUseDirect(font.GetSize() * maximun_text_scale, transform,
                           paint, font.GetTypeface())) {
    // texture
    Paint working_paint = paint;
    if (font.GetTypeface()->ContainsColorTable()) {
      working_paint.SetStyle(Paint::kFill_Style);
      GlyphRunList sub_run_list = DirectGlyphRun::SubRunListByTexture(
          count, glyphs, origin, position_x, position_y, font, working_paint,
          context_scale, transform, false, atlas_manager, arena_allocator);
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
            context_scale, transform,
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
            context_scale, transform,
            paint.GetStyle() != Paint::kStrokeThenFill_Style, atlas_manager,
            arena_allocator);
        for (auto& sub_run : sub_run_list) {
          run_list.push_back(sub_run);
        }
      }
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
