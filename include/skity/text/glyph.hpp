// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_TEXT_GLYPH_HPP
#define INCLUDE_SKITY_TEXT_GLYPH_HPP

#include <cstdint>
#include <optional>
#include <skity/graphic/path.hpp>
#include <skity/macros.hpp>

namespace skity {
using GlyphID = uint16_t;
using Unichar = uint32_t;

enum class GlyphFormat { A8, RGBA32, BGRA32 };
enum class AtlasFormat { A8, RGBA32 };

inline AtlasFormat FromGlyphFormat(GlyphFormat glyph_format) {
  switch (glyph_format) {
    case GlyphFormat::A8:
      return AtlasFormat::A8;
    case GlyphFormat::RGBA32:
    case GlyphFormat::BGRA32:
      return AtlasFormat::RGBA32;
  }
}

enum class BitmapFormat {
  kUnknown,
  kGray8,
  kBGRA8,
  kRGBA8,
};

struct SKITY_API GlyphBitmapData {
  // origin point for rendering, This value used in canvas draw
  float origin_x = 0.f;
  float origin_y = 0.f;
  // origin point for Scaler
  float origin_x_for_raster = 0.f;
  float origin_y_for_raster = 0.f;
  float width = {};
  float height = {};
  uint8_t* buffer = {};
  BitmapFormat format = BitmapFormat::kUnknown;
  bool need_free = false;
};

class SKITY_API GlyphData {
 public:
  GlyphData() : id_(0) {}
  explicit GlyphData(GlyphID id) : id_(id) {}

 public:
  float AdvanceX() const { return advance_x_; }
  float AdvanceY() const { return advance_y_; }

  void ZeroMetrics() {
    advance_x_ = 0;
    advance_y_ = 0;
    width_ = 0;
    height_ = 0;
    y_max_ = 0.f;
    y_min_ = 0.f;
  }

  GlyphID Id() const { return id_; }

  float GetWidth() const { return width_; }
  float GetHeight() const { return height_; }
  float GetLeft() const { return GetHoriBearingX(); }
  float GetTop() const { return GetHoriBearingY(); }
  float GetHoriBearingX() const { return hori_bearing_x_; }
  float GetHoriBearingY() const { return hori_bearing_y_; }
  float GetYMin() const { return y_min_; }
  float GetYMax() const { return y_max_; }
  float FontSize() const { return font_size_; }
  float FixedSize() const { return fixed_size_; }

  const Path& GetPath() const { return path_; }
  const GlyphBitmapData& Image() const { return image_; }

  std::optional<GlyphFormat> GetFormat() const { return format_; }

  void ScaleToFontSize(float new_font_size) {
    float scale = new_font_size / font_size_;
    advance_x_ = advance_x_ * scale;
    advance_y_ = advance_y_ * scale;
    width_ = width_ * scale;
    height_ = height_ * scale;
    hori_bearing_x_ = hori_bearing_x_ * scale;
    hori_bearing_y_ = hori_bearing_y_ * scale;
    font_size_ = new_font_size;
    if (!path_.IsEmpty()) {
      path_ = path_.CopyWithScale(scale);
    }
  }

  bool IsEmpty() const { return width_ == 0; }
  bool IsColor() const { return image_.format == BitmapFormat::kBGRA8; }

  bool NeedFree() const { return image_.need_free; }

 private:
  GlyphID id_;

  // The advance for this glyph.
  float advance_x_ = 0, advance_y_ = 0;
  // The width and height of the glyph mask.
  float width_ = 0, height_ = 0;

  float y_min_ = 0.f;
  float y_max_ = 0.f;

  float hori_bearing_x_ = 0, hori_bearing_y_ = 0;

  float font_size_ = 0;
  // This is used for color emoji with fixed char size.
  // since the glyph bitmap is fixed size, the render needs to scale the texture
  // viewport to fit logical font-size
  float fixed_size_ = 0.f;

  skity::Path path_ = {};
  skity::GlyphBitmapData image_ = {};

  std::optional<GlyphFormat> format_ = std::nullopt;

  friend class TypefaceFreeType;
  friend class ScalerContextFreetype;
  friend class ScalerContextDarwin;
  friend class ScalerContextContainer;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_TEXT_GLYPH_HPP
