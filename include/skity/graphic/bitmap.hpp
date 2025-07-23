// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_GRAPHIC_BITMAP_HPP
#define INCLUDE_SKITY_GRAPHIC_BITMAP_HPP

#include <cstdint>
#include <memory>
#include <skity/graphic/blend_mode.hpp>
#include <skity/graphic/color.hpp>
#include <skity/graphic/color_type.hpp>
#include <skity/io/pixmap.hpp>
#include <skity/macros.hpp>

namespace skity {

/**
 * @class Bitmap
 * Describtes a two-dimensional raster pixel array.
 *
 * Bitmap can be drawn using Canvas with software raster or set pixel directory
 * by calling **Bitmap::setPixel**
 */
class SKITY_API Bitmap {
 public:
  Bitmap();
  Bitmap(uint32_t width, uint32_t height,
         AlphaType alpha_type = AlphaType::kUnpremul_AlphaType,
         ColorType color_type = ColorType::kRGBA);
  explicit Bitmap(std::shared_ptr<Pixmap> pixmap, bool read_only = true);

  Bitmap(Bitmap const&) = delete;
  Bitmap& operator=(Bitmap const&) = delete;

  ~Bitmap() = default;

  Color GetPixel(uint32_t x, uint32_t y);

  void SetPixel(uint32_t x, uint32_t y, Color color);

  void SetPixel(uint32_t x, uint32_t y, Color4f color);

  uint32_t Width() const { return pixmap_->Width(); }
  uint32_t Height() const { return pixmap_->Height(); }
  uint32_t RowBytes() const { return pixmap_->RowBytes(); }

  uint8_t* GetPixelAddr() const {
    return reinterpret_cast<uint8_t*>(pixmap_->WritableAddr());
  }

  std::shared_ptr<Pixmap> const& GetPixmap() const { return pixmap_; }

  /*
   * Deprecated, use SetColorInfo instead
   */
  bool SetAlphaType(AlphaType alpha_type);

  /*
   * Deprecated, use SetColorInfo instead
   */
  bool SetColorType(ColorType type);

  bool SetColorInfo(AlphaType alpha_type, ColorType color_type);

  AlphaType GetAlphaType() const { return pixmap_->GetAlphaType(); }

  ColorType GetColorType() const { return pixmap_->GetColorType(); }

 private:
  std::shared_ptr<Pixmap> pixmap_;
  bool read_only_ = false;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_GRAPHIC_BITMAP_HPP
