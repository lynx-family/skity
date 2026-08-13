// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/text/scaler_context_container.hpp"

#include <skity/geometry/stroke.hpp>
#include <skity/text/glyph.hpp>

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
  if (count == 0) {
    return;
  }

  DEBUG_CHECK(count == 1);
  if (count != 1) {
    return;
  }

  const PackedGlyphID packed_id(glyph_ids[0]);
  this->PrepareImages(&packed_id, 1, results, paint);
}

void ScalerContextContainer::PrepareImages(const PackedGlyphID *glyph_ids,
                                           uint32_t count,
                                           const GlyphData *results[],
                                           const Paint &paint)
    SKITY_EXCLUDES(mutex_) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (count == 0) {
    return;
  }

  // Platform scaler contexts rasterize and expose one glyph at a time. The
  // caller must consume or copy the returned bitmap before the next request.
  DEBUG_CHECK(count == 1);
  if (count != 1) {
    return;
  }

  StrokeDesc stroke_desc{paint.GetStyle() != Paint::kFill_Style,
                         paint.GetStrokeWidth(), paint.GetStrokeCap(),
                         paint.GetStrokeJoin(), paint.GetStrokeMiter()};
  const PackedGlyphID packed_id = glyph_ids[0];
  auto *glyph_data = this->Glyph(packed_id);
  this->PrepareImage(packed_id, glyph_data, stroke_desc);
  results[0] = glyph_data;
}

void ScalerContextContainer::PrepareImageInfos(const GlyphID *glyph_ids,
                                               uint32_t count,
                                               const GlyphData *results[],
                                               const Paint &paint)
    SKITY_EXCLUDES(mutex_) {
  std::lock_guard<std::mutex> lock(mutex_);
  const GlyphData **cursor = results;
  for (uint32_t idx = 0; idx < count; ++idx) {
    const PackedGlyphID packed_id(glyph_ids[idx]);
    auto *glyph_data = this->Glyph(packed_id);
    if (glyph_data->image_.origin_x == 0 && glyph_data->image_.origin_y == 0) {
      StrokeDesc stroke_desc{paint.GetStyle() != Paint::kFill_Style,
                             paint.GetStrokeWidth(), paint.GetStrokeCap(),
                             paint.GetStrokeJoin(), paint.GetStrokeMiter()};
      this->PrepareImageInfo(packed_id, glyph_data, stroke_desc);
    }
    *cursor++ = glyph_data;
  }
}

GlyphData *ScalerContextContainer::Glyph(PackedGlyphID id)
    SKITY_REQUIRES(mutex_) {
  auto it = glyph_data_map_.find(id);
  if (it != glyph_data_map_.end()) {
    return it->second.get();
  }
  auto glyph_data = std::make_unique<GlyphData>(id.GetGlyphID());
  scaler_context_->MakeGlyph(glyph_data.get());
  return this->AddGlyph(id, std::move(glyph_data));
}

GlyphData *ScalerContextContainer::AddGlyph(PackedGlyphID id,
                                            std::unique_ptr<GlyphData> glyph)
    SKITY_REQUIRES(mutex_) {
  GlyphData *raw_pointer = glyph.get();
  glyph_data_map_[id] = std::move(glyph);
  return raw_pointer;
}

void ScalerContextContainer::PrepareImage(PackedGlyphID id, GlyphData *glyph,
                                          const StrokeDesc &stroke_desc)
    SKITY_REQUIRES(mutex_) {
  // A backend may return caller-owned pixels or borrowed scratch storage. The
  // single-glyph caller consumes the result before requesting another image.
  scaler_context_->GetImage(id, glyph, stroke_desc);
}

void ScalerContextContainer::PrepareImageInfo(PackedGlyphID id,
                                              GlyphData *glyph,
                                              const StrokeDesc &stroke_desc)
    SKITY_REQUIRES(mutex_) {
  scaler_context_->GetImageInfo(id, glyph, stroke_desc);
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
    const PackedGlyphID packed_id(glyph_ids[idx]);
    auto *glyph_data = this->Glyph(packed_id);
    if (path_detail == kMetricsAndPath) {
      this->PreparePath(glyph_data);
    }
    results[idx] = glyph_data;
  }
}
}  // namespace skity
