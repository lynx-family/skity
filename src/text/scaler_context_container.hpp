// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_TEXT_SCALER_CONTEXT_CONTAINER_HPP
#define SRC_TEXT_SCALER_CONTEXT_CONTAINER_HPP

#include <mutex>
#include <unordered_map>

#include "src/text/scaler_context.hpp"
#include "src/utils/thread_annotations.hpp"

namespace skity {

// It's from scaler_cache and we need to check thread safety.
class ScalerContextContainer {
 public:
  explicit ScalerContextContainer(
      std::unique_ptr<ScalerContext> scaler_context);
  virtual ~ScalerContextContainer();
  const FontMetrics& GetFontMetrics() const { return font_metrics_; }
  ScalerContext* GetScalerContext() const { return scaler_context_.get(); }

  void Metrics(const GlyphID* glyph_ids, uint32_t count,
               const GlyphData* results[]) SKITY_EXCLUDES(mutex_);
  void PreparePaths(const GlyphID* glyph_ids, uint32_t count,
                    const GlyphData* results[]) SKITY_EXCLUDES(mutex_);
  void PrepareImages(const GlyphID* glyph_ids, uint32_t count,
                     const GlyphData* results[], const Paint& paint)
      SKITY_EXCLUDES(mutex_);

  void PrepareImageInfos(const GlyphID* glyph_ids, uint32_t count,
                         const GlyphData* results[], const Paint& paint)
      SKITY_EXCLUDES(mutex_);

  uint16_t GetFixedSize() { return scaler_context_->GetFixedSize(); }

 private:
  GlyphData* Glyph(GlyphID id) SKITY_REQUIRES(mutex_);
  GlyphData* AddGlyph(std::unique_ptr<GlyphData> glyph) SKITY_REQUIRES(mutex_);
  void PrepareImage(GlyphData* glyph, const StrokeDesc& stroke_desc)
      SKITY_REQUIRES(mutex_);
  void PrepareImageInfo(GlyphData* glyph, const StrokeDesc& stroke_desc)
      SKITY_REQUIRES(mutex_);
  void PreparePath(GlyphData* glyph) SKITY_REQUIRES(mutex_);
  enum PathDetail { kMetricsOnly, kMetricsAndPath };
  void InternalPrepare(const GlyphID* glyph_ids, uint32_t count,
                       PathDetail path_detail, const GlyphData* results[])
      SKITY_REQUIRES(mutex_);

 private:
  std::unique_ptr<ScalerContext> scaler_context_;
  const FontMetrics font_metrics_;
  mutable std::mutex mutex_;
  std::unordered_map<GlyphID, std::unique_ptr<GlyphData>> glyph_data_map_
      SKITY_GUARDED_BY(mutex_);
  //  std::vector<GlyphData*> glyph_data_for_index SKITY_GUARDED_BY(mutex_);
  // so we don't grow our arrays a lot
  static constexpr size_t kMinGlyphCount = 8;
  static constexpr size_t kMinGlyphImageSize = 16 /* height */ * 8 /* width */;
  static constexpr size_t kMinAllocAmount = kMinGlyphImageSize * kMinGlyphCount;
};

}  // namespace skity

#endif  // SRC_TEXT_SCALER_CONTEXT_CONTAINER_HPP
