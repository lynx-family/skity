// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/text/scaler_context_container.hpp"

#include <cmath>
#include <cstring>
#include <limits>
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
  if (count == 0) {
    return;
  }

  DEBUG_CHECK(count == 1);
  if (count != 1) {
    return;
  }

  // The legacy API cannot return an owner. Keep its single-glyph view on the
  // requesting thread so another raster thread cannot replace or free it.
  thread_local OwnedGlyphImage image;
  image = PrepareOwnedImage(glyph_ids[0], paint);
  results[0] = image.Get();
}

OwnedGlyphImage ScalerContextContainer::PrepareOwnedImage(
    PackedGlyphID glyph_id, const Paint &paint) SKITY_EXCLUDES(mutex_) {
  std::lock_guard<std::mutex> lock(mutex_);
  OwnedGlyphImage result;
  result.glyph_.emplace(*Glyph(glyph_id));
  auto &image = result.glyph_->image_;
  // Metrics and paths may be cached, but a generated image must never be
  // published through the shared GlyphData (including on backend failure).
  image = {};
  const StrokeDesc stroke_desc{paint.GetStyle() != Paint::kFill_Style,
                               paint.GetStrokeWidth(), paint.GetStrokeCap(),
                               paint.GetStrokeJoin(), paint.GetStrokeMiter()};
  PrepareImage(glyph_id, &*result.glyph_, stroke_desc);
  if (image.need_free) {
    result.pixels_.reset(image.buffer);
  }
  image.need_free = false;

  // Validate before float-to-integer conversion or copying borrowed pixels.
  // Empty glyphs may still carry a malloc(0) allocation; the owner releases it.
  if (!std::isfinite(image.width) || !std::isfinite(image.height) ||
      image.width < 0.f || image.height < 0.f ||
      std::floor(image.width) != image.width ||
      std::floor(image.height) != image.height ||
      static_cast<double>(image.width) > std::numeric_limits<uint32_t>::max() ||
      static_cast<double>(image.height) >
          std::numeric_limits<uint32_t>::max()) {
    image = {};
    return result;
  }
  if (!image.buffer || image.width == 0.f || image.height == 0.f) {
    image.buffer = nullptr;
    return result;
  }
  const size_t width = static_cast<size_t>(image.width);
  const size_t height = static_cast<size_t>(image.height);
  size_t bytes_per_pixel = 0;
  switch (image.format) {
    case BitmapFormat::kGray8:
      bytes_per_pixel = 1;
      break;
    case BitmapFormat::kBGRA8:
    case BitmapFormat::kRGBA8:
      bytes_per_pixel = 4;
      break;
    case BitmapFormat::kUnknown:
      break;
  }
  if (bytes_per_pixel == 0 ||
      width > std::numeric_limits<size_t>::max() / bytes_per_pixel) {
    image = {};
    return result;
  }
  const size_t row_bytes = image.RowBytes();
  if (row_bytes < width * bytes_per_pixel ||
      row_bytes > std::numeric_limits<size_t>::max() / height) {
    image = {};
    return result;
  }
  if (!result.pixels_) {
    // CoreText and FreeType color rendering can return reusable backend
    // storage. Copy it while the same lock still protects its contents.
    result.pixels_.reset(
        static_cast<uint8_t *>(std::malloc(height * row_bytes)));
    if (!result.pixels_) {
      image = {};
      return result;
    }
    const size_t pixel_row_bytes = width * bytes_per_pixel;
    if (row_bytes == pixel_row_bytes) {
      std::memcpy(result.pixels_.get(), image.buffer, height * row_bytes);
    } else {
      // A source view need not include padding after its last row. Initialize
      // only destination padding, avoiding a second write over every pixel.
      for (size_t y = 0; y < height; ++y) {
        auto *dst = result.pixels_.get() + y * row_bytes;
        std::memcpy(dst, image.buffer + y * row_bytes, pixel_row_bytes);
        std::memset(dst + pixel_row_bytes, 0, row_bytes - pixel_row_bytes);
      }
    }
    image.buffer = result.pixels_.get();
  }
  return result;
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
  // PrepareOwnedImage adopts or copies the result before releasing mutex_.
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
