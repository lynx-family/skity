// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_TEXT_SCALER_CONTEXT_CONTAINER_HPP
#define SRC_TEXT_SCALER_CONTEXT_CONTAINER_HPP

#include <cstdlib>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <utility>

#include "src/text/packed_glyph_id.hpp"
#include "src/text/scaler_context.hpp"
#include "src/utils/thread_annotations.hpp"

namespace skity {

// A request-local glyph and its pixels, independent of the scaler cache and
// backend scratch storage. The bitmap view never transfers ownership to
// callers.
class OwnedGlyphImage {
 public:
  OwnedGlyphImage() = default;
  OwnedGlyphImage(OwnedGlyphImage&& other) noexcept
      : glyph_(std::move(other.glyph_)), pixels_(std::move(other.pixels_)) {
    other.glyph_.reset();
  }
  OwnedGlyphImage& operator=(OwnedGlyphImage&& other) noexcept {
    if (this != &other) {
      // Reconstruct instead of assigning GlyphData: Path has a move
      // constructor but its assignment operator copies the path vectors.
      glyph_.reset();
      if (other.glyph_) {
        glyph_.emplace(std::move(*other.glyph_));
        other.glyph_.reset();
      }
      pixels_ = std::move(other.pixels_);
    }
    return *this;
  }

  const GlyphData* Get() const { return glyph_ ? &*glyph_ : nullptr; }
  const GlyphBitmapData& Image() const { return glyph_->Image(); }

 private:
  struct FreePixels {
    void operator()(uint8_t* pixels) const { std::free(pixels); }
  };

  // Store the request-local metadata inline, avoiding one allocation per
  // image request. Cached paths still need an independent snapshot.
  std::optional<GlyphData> glyph_;
  std::unique_ptr<uint8_t, FreePixels> pixels_;

  friend class ScalerContextContainer;
};

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
  OwnedGlyphImage PrepareOwnedImage(PackedGlyphID glyph_id, const Paint& paint)
      SKITY_EXCLUDES(mutex_);
  // Compatibility view, valid until the next PrepareImages call on this
  // thread. Rendering code should retain an OwnedGlyphImage instead.
  void PrepareImages(const GlyphID* glyph_ids, uint32_t count,
                     const GlyphData* results[], const Paint& paint)
      SKITY_EXCLUDES(mutex_);
  void PrepareImages(const PackedGlyphID* glyph_ids, uint32_t count,
                     const GlyphData* results[], const Paint& paint)
      SKITY_EXCLUDES(mutex_);

  void PrepareImageInfos(const GlyphID* glyph_ids, uint32_t count,
                         const GlyphData* results[], const Paint& paint)
      SKITY_EXCLUDES(mutex_);

  uint16_t GetFixedSize() { return scaler_context_->GetFixedSize(); }

 private:
  GlyphData* Glyph(PackedGlyphID id) SKITY_REQUIRES(mutex_);
  GlyphData* AddGlyph(PackedGlyphID id, std::unique_ptr<GlyphData> glyph)
      SKITY_REQUIRES(mutex_);
  void PrepareImage(PackedGlyphID id, GlyphData* glyph,
                    const StrokeDesc& stroke_desc) SKITY_REQUIRES(mutex_);
  void PrepareImageInfo(PackedGlyphID id, GlyphData* glyph,
                        const StrokeDesc& stroke_desc) SKITY_REQUIRES(mutex_);
  void PreparePath(GlyphData* glyph) SKITY_REQUIRES(mutex_);
  enum PathDetail { kMetricsOnly, kMetricsAndPath };
  void InternalPrepare(const GlyphID* glyph_ids, uint32_t count,
                       PathDetail path_detail, const GlyphData* results[])
      SKITY_REQUIRES(mutex_);

 private:
  std::unique_ptr<ScalerContext> scaler_context_;
  const FontMetrics font_metrics_;
  mutable std::mutex mutex_;
  std::unordered_map<PackedGlyphID, std::unique_ptr<GlyphData>,
                     PackedGlyphID::Hash>
      glyph_data_map_ SKITY_GUARDED_BY(mutex_);
  //  std::vector<GlyphData*> glyph_data_for_index SKITY_GUARDED_BY(mutex_);
  // so we don't grow our arrays a lot
  static constexpr size_t kMinGlyphCount = 8;
  static constexpr size_t kMinGlyphImageSize = 16 /* height */ * 8 /* width */;
  static constexpr size_t kMinAllocAmount = kMinGlyphImageSize * kMinGlyphCount;
};

}  // namespace skity

#endif  // SRC_TEXT_SCALER_CONTEXT_CONTAINER_HPP
