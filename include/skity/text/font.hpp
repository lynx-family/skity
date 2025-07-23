// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_TEXT_FONT_HPP
#define INCLUDE_SKITY_TEXT_FONT_HPP

#include <skity/geometry/rect.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/macros.hpp>
#include <skity/text/font_metrics.hpp>
#include <skity/text/typeface.hpp>

namespace skity {

class Paint;

template <typename T>
static constexpr bool ToBool(const T& x) {
  return static_cast<bool>(x);
}

class SKITY_API Font {
 public:
  enum class FontHinting {
    kNone,    //!< glyph outlines unchanged
    kSlight,  //!< minimal modification to improve constrast
    kNormal,  //!< glyph outlines modified to improve constrast
    kFull,    //!< modifies glyph outlines for maximum constrast
  };

  /** Whether edge pixels draw opaque or with partial transparency.
   */
  enum class Edging {
    kAlias,              //!< no transparent pixels on glyph edges
    kAntiAlias,          //!< may have transparent pixels on glyph edges
    kSubpixelAntiAlias,  //!< glyph positioned in pixel using transparency
  };

  Font();

  Font(Typeface* typeface, float size);

  explicit Font(Typeface* typeface);

  Font(Typeface* typeface, float size, float scaleX, float skewX);

  bool IsForceAutoHinting() const {
    return ToBool(flags_ & kForceAutoHinting_PrivFlag);
  }
  void SetForceAutoHinting(bool forceAutoHinting);

  bool IsEmbeddedBitmaps() const {
    return ToBool(flags_ & kEmbeddedBitmaps_PrivFlag);
  }
  void SetEmbeddedBitmaps(bool embeddedBitmaps);

  bool IsSubpixel() const { return ToBool(flags_ & kSubpixel_PrivFlag); }
  void SetSubpixel(bool subpixel);

  bool IsLinearMetrics() const {
    return ToBool(flags_ & kLinearMetrics_PrivFlag);
  }
  void SetLinearMetrics(bool linearMetrics);

  bool IsEmbolden() const { return ToBool(flags_ & kEmbolden_PrivFlag); }
  void SetEmbolden(bool embolden);

  bool IsBaselineSnap() const {
    return ToBool(flags_ & kBaselineSnap_PrivFlag);
  }
  void SetBaselineSnap(bool baselineSnap);

  Edging GetEdging() const { return (Edging)edging_; }
  void SetEdging(Edging edging) { edging_ = (uint8_t)edging; }

  FontHinting GetHinting() const { return (FontHinting)hinting_; }
  void SetHinting(FontHinting hintingLevel) {
    hinting_ = (uint8_t)hintingLevel;
  }

  float GetSize() const { return size_; }
  void SetSize(float textSize);

  float GetScaleX() const { return scale_x_; }
  void SetScaleX(float scaleX) { scale_x_ = scaleX; }

  float GetSkewX() const { return skew_x_; }
  void SetSkewX(float skewX) { skew_x_ = skewX; }

  Font MakeWithSize(float size) const;

  void SetTypeface(Typeface* tf) { typeface_ = tf; }
  Typeface* GetTypeface() const { return typeface_; }
  Typeface* GetTypefaceOrDefault() const;

  void GetWidths(const GlyphID glyphs[], int count, float widths[],
                 Rect bounds[]) const {
    this->GetWidthsBounds(glyphs, count, widths, bounds, Paint());
  }

  void GetWidths(const GlyphID glyphs[], int count, float widths[]) const {
    this->GetWidthsBounds(glyphs, count, widths, nullptr, Paint());
  }

  void GetWidthsBounds(const GlyphID glyphs[], int count, float widths[],
                       Rect bounds[], const Paint& paint) const;

  void GetMetrics(FontMetrics* metrics) const;

  //  SkScalar getSpacing() const { return this->getSpacing(); }

  void LoadGlyphMetrics(const GlyphID glyphs[], uint32_t count,
                        const GlyphData* glyph_data[],
                        const Paint& paint = Paint()) const;
  void LoadGlyphPath(const GlyphID* glyphs, uint32_t count,
                     const GlyphData* glyph_data[]) const;
  void LoadGlyphBitmap(const GlyphID* glyphs, uint32_t count,
                       const GlyphData* glyph_data[], const Paint& paint,
                       float context_scale, const Matrix& transform) const;
  void LoadGlyphBitmapInfo(const GlyphID* glyphs, uint32_t count,
                           const GlyphData* glyph_data[], const Paint& paint,
                           float context_scale, const Matrix& transform) const;
  uint16_t GetFixedSize() const;

 private:
  mutable Typeface* typeface_;
  float size_;
  float scale_x_;
  float skew_x_;

  uint8_t flags_{};
  uint8_t edging_{};
  uint8_t hinting_{};

  enum PrivFlags {
    kForceAutoHinting_PrivFlag = 1 << 0,
    kEmbeddedBitmaps_PrivFlag = 1 << 1,
    kSubpixel_PrivFlag = 1 << 2,
    kLinearMetrics_PrivFlag = 1 << 3,
    kEmbolden_PrivFlag = 1 << 4,
    kBaselineSnap_PrivFlag = 1 << 5,
  };

  static constexpr unsigned kAllFlags =
      kForceAutoHinting_PrivFlag | kEmbeddedBitmaps_PrivFlag |
      kSubpixel_PrivFlag | kLinearMetrics_PrivFlag | kEmbolden_PrivFlag |
      kBaselineSnap_PrivFlag;
};
}  // namespace skity

#endif  // INCLUDE_SKITY_TEXT_FONT_HPP
