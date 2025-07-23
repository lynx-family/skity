// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/text/scaler_context_container.hpp"

#include <skity/geometry/stroke.hpp>
#include <skity/text/glyph.hpp>

#include "src/logging.hpp"

namespace skity {

static FontMetrics GenerateMetrics(ScalerContext *context) {
  FontMetrics font_metrics;
  context->GetFontMetrics(&font_metrics);
  return font_metrics;
}

ScalerContextContainer::ScalerContextContainer(
    std::unique_ptr<ScalerContext> scaler_context)
    : scaler_context_(std::move(scaler_context)),
      font_metrics_(GenerateMetrics(scaler_context_.get())) {}

ScalerContextContainer::~ScalerContextContainer() SKITY_EXCLUDES(mutex_) {
  std::lock_guard<std::mutex> lock(mutex_);
  glyph_data_map_.clear();
}

void ScalerContextContainer::Metrics(const GlyphID *glyph_ids, uint32_t count,
                                     const GlyphData *results[])
    SKITY_EXCLUDES(mutex_) {
  std::lock_guard<std::mutex> lock(mutex_);
  this->InternalPrepare(glyph_ids, count, kMetricsOnly, results);
}

void ScalerContextContainer::PreparePaths(const GlyphID *glyph_ids,
                                          uint32_t count,
                                          const GlyphData *results[])
    SKITY_EXCLUDES(mutex_) {
  std::lock_guard<std::mutex> lock(mutex_);
  this->InternalPrepare(glyph_ids, count, kMetricsAndPath, results);
}

void ScalerContextContainer::PrepareImages(const GlyphID *glyph_ids,
                                           uint32_t count,
                                           const GlyphData *results[],
                                           const Paint &paint)
    SKITY_EXCLUDES(mutex_) {
  std::lock_guard<std::mutex> lock(mutex_);
  const GlyphData **cursor = results;
  for (uint32_t idx = 0; idx < count; ++idx) {
    auto glyph_id = glyph_ids[idx];
    auto *glyph_data = this->Glyph(glyph_id);
    StrokeDesc stroke_desc{paint.GetStyle() != Paint::kFill_Style,
                           paint.GetStrokeWidth(), paint.GetStrokeCap(),
                           paint.GetStrokeJoin(), paint.GetStrokeMiter()};
    this->PrepareImage(glyph_data, stroke_desc);
    *cursor++ = glyph_data;
  }
}

void ScalerContextContainer::PrepareImageInfos(const GlyphID *glyph_ids,
                                               uint32_t count,
                                               const GlyphData *results[],
                                               const Paint &paint)
    SKITY_EXCLUDES(mutex_) {
  std::lock_guard<std::mutex> lock(mutex_);
  const GlyphData **cursor = results;
  for (uint32_t idx = 0; idx < count; ++idx) {
    auto glyph_id = glyph_ids[idx];
    auto *glyph_data = this->Glyph(glyph_id);
    if (glyph_data->image_.origin_x == 0 && glyph_data->image_.origin_y == 0) {
      StrokeDesc stroke_desc{paint.GetStyle() != Paint::kFill_Style,
                             paint.GetStrokeWidth(), paint.GetStrokeCap(),
                             paint.GetStrokeJoin(), paint.GetStrokeMiter()};
      this->PrepareImageInfo(glyph_data, stroke_desc);
    }
    *cursor++ = glyph_data;
  }
}

GlyphData *ScalerContextContainer::Glyph(GlyphID id) SKITY_REQUIRES(mutex_) {
  auto it = glyph_data_map_.find(id);
  if (it != glyph_data_map_.end()) {
    DEBUG_CHECK(!it->second->NeedFree());
    return it->second.get();
  }
  auto glyph_data = std::make_unique<GlyphData>(id);
  scaler_context_->MakeGlyph(glyph_data.get());
  // handle stroke metrics
  ScalerContextDesc desc = scaler_context_->GetDesc();
  if (desc.stroke_width > 0) {
    Paint paint;
    paint.SetStyle(Paint::kStroke_Style);
    paint.SetStrokeWidth(desc.stroke_width);
    paint.SetStrokeCap(desc.cap);
    paint.SetStrokeJoin(desc.join);
    paint.SetStrokeMiter(desc.miter_limit);
    PreparePath(glyph_data.get());
    Path quad_path;
    Path fill_path;
    Stroke stroke(paint);
    stroke.QuadPath(glyph_data->GetPath(), &quad_path);
    stroke.StrokePath(quad_path, &fill_path);

    Rect bound = fill_path.GetBounds();
    glyph_data->hori_bearing_x_ = bound.Left();
    glyph_data->hori_bearing_y_ = -bound.Top();
    glyph_data->width_ = bound.Width();
    glyph_data->height_ = bound.Height();
  }

  return this->AddGlyph(std::move(glyph_data));
}

GlyphData *ScalerContextContainer::AddGlyph(std::unique_ptr<GlyphData> glyph)
    SKITY_REQUIRES(mutex_) {
  GlyphData *raw_pointer = glyph.get();
  glyph_data_map_[glyph->Id()] = std::move(glyph);
  return raw_pointer;
}

void ScalerContextContainer::PrepareImage(GlyphData *glyph,
                                          const StrokeDesc &stroke_desc)
    SKITY_REQUIRES(mutex_) {
  // Freetype is reuse bitmap memory, so can not just save memory pointer in
  // this function. And there's no need to cache the memory in this function.
  // Because:
  // If this is in GPU backend, this is only called once, the outline
  // bitmap is uploaded into GPU memory, so no need to save this memory. If this
  // is in CPU backend, only Color Emoji need to load bitmap, and this is copied
  // into CPU canvas context immediately.
  scaler_context_->GetImage(glyph, stroke_desc);
}

void ScalerContextContainer::PrepareImageInfo(GlyphData *glyph,
                                              const StrokeDesc &stroke_desc)
    SKITY_REQUIRES(mutex_) {
  scaler_context_->GetImageInfo(glyph, stroke_desc);
}

void ScalerContextContainer::PreparePath(GlyphData *glyph)
    SKITY_REQUIRES(mutex_) {
  if (glyph->GetPath().IsEmpty()) {
    scaler_context_->GetPath(glyph);
  }
}
void ScalerContextContainer::InternalPrepare(
    const GlyphID *glyph_ids, uint32_t count,
    ScalerContextContainer::PathDetail path_detail, const GlyphData *results[])
    SKITY_REQUIRES(mutex_) {
  for (uint32_t idx = 0; idx < count; ++idx) {
    auto glyph_id = glyph_ids[idx];
    auto *glyph_data = this->Glyph(glyph_id);
    if (path_detail == kMetricsAndPath) {
      this->PreparePath(glyph_data);
    }
    results[idx] = glyph_data;
  }
}
}  // namespace skity
