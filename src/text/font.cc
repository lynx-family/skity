// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <algorithm>
#include <skity/text/font.hpp>
#include <vector>

#include "src/geometry/math.hpp"
#include "src/text/scaler_context_cache.hpp"
#include "src/text/scaler_context_desc.hpp"
#include "src/tracing.hpp"

namespace skity {

#define DEFAULT_SIZE 12.f

static inline uint32_t SetClearMask(uint32_t bits, bool cond, uint32_t mask) {
  return cond ? bits | mask : bits & ~mask;
}

static inline float ValidSize(float size) { return std::max<float>(0, size); }

Font::Font() : Font(nullptr) {}

Font::Font(std::shared_ptr<Typeface> typeface, float size)
    : Font(std::move(typeface), size, 1, 0) {}

Font::Font(std::shared_ptr<Typeface> typeface)
    : Font(std::move(typeface), DEFAULT_SIZE) {}

Font::Font(std::shared_ptr<Typeface> typeface, float size, float scaleX,
           float skewX)
    : typeface_(std::move(typeface)),
      size_(size),
      scale_x_(scaleX),
      skew_x_(skewX),
      flags_(kSubpixel_PrivFlag | kBaselineSnap_PrivFlag),
      edging_(static_cast<uint8_t>(Edging::kAntiAlias)) {}

void Font::SetForceAutoHinting(bool predicate) {
  flags_ = SetClearMask(flags_, predicate, kForceAutoHinting_PrivFlag);
}

void Font::SetEmbeddedBitmaps(bool predicate) {
  flags_ = SetClearMask(flags_, predicate, kEmbeddedBitmaps_PrivFlag);
}

void Font::SetSubpixel(bool predicate) {
  flags_ = SetClearMask(flags_, predicate, kSubpixel_PrivFlag);
}

void Font::SetLinearMetrics(bool predicate) {
  flags_ = SetClearMask(flags_, predicate, kLinearMetrics_PrivFlag);
}

void Font::SetEmbolden(bool predicate) {
  flags_ = SetClearMask(flags_, predicate, kEmbolden_PrivFlag);
}

void Font::SetBaselineSnap(bool predicate) {
  flags_ = SetClearMask(flags_, predicate, kBaselineSnap_PrivFlag);
}

void Font::SetSize(float textSize) { size_ = ValidSize(textSize); }

Font Font::MakeWithSize(float size) const {
  Font font = *this;
  font.SetSize(size);
  return font;
}

std::shared_ptr<Typeface> Font::GetTypefaceOrDefault() const {
  if (!typeface_) {
    typeface_ = Typeface::GetDefaultTypeface();
  }
  return typeface_;
}

void Font::GetWidthsBounds(const GlyphID glyphs[], int count, float widths[],
                           Rect bounds[], const Paint& paint) const {
  ScalerContextDesc desc = ScalerContextDesc::MakeCanonicalized(*this, paint);
  auto scaler_context_container =
      ScalerContextCache::GlobalScalerContextCache()->FindOrCreateScalerContext(
          desc, typeface_);
  std::vector<const GlyphData*> glyphs_data(count);
  scaler_context_container->Metrics(glyphs, count, glyphs_data.data());

  if (bounds) {
    Rect* cursor = bounds;
    for (auto glyph_data : glyphs_data) {
      cursor->SetXYWH(glyph_data->GetLeft(), -glyph_data->GetTop(),
                      glyph_data->GetWidth(), glyph_data->GetHeight());
      cursor++;
    }
  }
  if (widths) {
    float* cursor = widths;
    for (auto glyph_data : glyphs_data) {
      *cursor++ = glyph_data->AdvanceX();
    }
  }
}

void Font::GetMetrics(FontMetrics* metrics) const {
  ScalerContextDesc desc = ScalerContextDesc::MakeCanonicalized(*this, Paint());
  auto scaler_context_container =
      ScalerContextCache::GlobalScalerContextCache()->FindOrCreateScalerContext(
          desc, typeface_);

  FontMetrics storage{};
  if (nullptr == metrics) {
    metrics = &storage;
  }

  *metrics = scaler_context_container->GetFontMetrics();
}

void Font::LoadGlyphMetrics(const GlyphID* glyphs, uint32_t count,
                            const GlyphData* glyph_data[],
                            const Paint& paint) const {
  ScalerContextDesc desc = ScalerContextDesc::MakeCanonicalized(*this, paint);
  auto scaler_context_container =
      ScalerContextCache::GlobalScalerContextCache()->FindOrCreateScalerContext(
          desc, typeface_);
  scaler_context_container->Metrics(glyphs, count, glyph_data);
}

void Font::LoadGlyphPath(const GlyphID* glyphs, uint32_t count,
                         const GlyphData* glyph_data[]) const {
  ScalerContextDesc desc = ScalerContextDesc::MakeCanonicalized(*this, Paint());
  auto scaler_context_container =
      ScalerContextCache::GlobalScalerContextCache()->FindOrCreateScalerContext(
          desc, typeface_);
  scaler_context_container->PreparePaths(glyphs, count, glyph_data);
}

void Font::LoadGlyphBitmap(const GlyphID* glyphs, uint32_t count,
                           const GlyphData* glyph_data[], const Paint& paint,
                           float context_scale, const Matrix& transform) const {
  SKITY_TRACE_EVENT(Font_LoadGlyphBitmap);
  Matrix22 transform22{transform.GetScaleX(), transform.GetSkewX(),
                       transform.GetSkewY(), transform.GetScaleY()};
  ScalerContextDesc desc = ScalerContextDesc::MakeTransformed(
      *this, paint, context_scale, transform22);
  auto scaler_context_container =
      ScalerContextCache::GlobalScalerContextCache()->FindOrCreateScalerContext(
          desc, typeface_);
  scaler_context_container->PrepareImages(glyphs, count, glyph_data, paint);
}

#if defined(__APPLE__)
namespace {
bool NativePhaseDescriptor(const Font& font, const Paint& paint,
                           uint32_t phase, ScalerContextDesc* desc) {
  const auto typeface = font.GetTypeface();
  if (phase > 3 || !typeface || typeface->ContainsColorTable() ||
      paint.GetStyle() != Paint::kFill_Style || font.GetScaleX() != 1.0f ||
      font.GetSkewX() != 0.0f || font.IsEmbolden()) {
    return false;
  }
  *desc = ScalerContextDesc::MakeTransformed(font, paint, 1.0f, Matrix22{});
  desc->native_raster_phase = phase + 1;
  const Color color = Color4fToColor(paint.GetFillColor());
  const uint32_t luminance = (77 * ColorGetR(color) + 150 * ColorGetG(color) +
                              29 * ColorGetB(color) + 128) >> 8;
  desc->foreground_color = ColorSetARGB(255, luminance, luminance, luminance);
  return true;
}
}  // namespace

void Font::LoadGlyphBitmapAtPhase(const GlyphID* glyphs, uint32_t count,
                                  const GlyphData* glyph_data[],
                                  const Paint& paint,
                                  uint32_t quarter_pixel_phase) const {
  ScalerContextDesc desc;
  if (count != 1 ||
      !NativePhaseDescriptor(*this, paint, quarter_pixel_phase, &desc)) {
    LoadGlyphBitmap(glyphs, count, glyph_data, paint, 1.0f, Matrix());
    return;
  }
  auto container = ScalerContextCache::GlobalScalerContextCache()
                       ->FindOrCreateScalerContext(desc, typeface_);
  const PackedGlyphID packed_glyph_id(
      glyphs[0], static_cast<uint8_t>(quarter_pixel_phase), 0);
  container->PrepareImages(&packed_glyph_id, 1, glyph_data, paint);
}
#endif

void Font::LoadGlyphBitmapInfo(const GlyphID* glyphs, uint32_t count,
                               const GlyphData* glyph_data[],
                               const Paint& paint, float context_scale,
                               const Matrix& transform) const {
  Matrix22 transform22{transform.GetScaleX(), transform.GetSkewX(),
                       transform.GetSkewY(), transform.GetScaleY()};
  ScalerContextDesc desc = ScalerContextDesc::MakeTransformed(
      *this, paint, context_scale, transform22);
  auto scaler_context_container =
      ScalerContextCache::GlobalScalerContextCache()->FindOrCreateScalerContext(
          desc, typeface_);
  scaler_context_container->PrepareImageInfos(glyphs, count, glyph_data, paint);
}

uint16_t Font::GetFixedSize() const {
  ScalerContextDesc desc = ScalerContextDesc::MakeCanonicalized(*this, Paint());
  auto scaler_context_container =
      ScalerContextCache::GlobalScalerContextCache()->FindOrCreateScalerContext(
          desc, typeface_);
  return scaler_context_container->GetFixedSize();
}
}  // namespace skity
