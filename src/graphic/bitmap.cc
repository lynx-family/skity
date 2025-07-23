// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <algorithm>
#include <skity/graphic/bitmap.hpp>
#include <skity/graphic/color_type.hpp>
#include <skity/io/data.hpp>

#include "src/graphic/blend_mode_priv.hpp"
#include "src/graphic/color_priv.hpp"

namespace skity {

Bitmap::Bitmap() { pixmap_ = std::make_shared<Pixmap>(); }

Bitmap::Bitmap(uint32_t width, uint32_t height, AlphaType alpha_type,
               ColorType color_type) {
  pixmap_ = std::make_shared<Pixmap>(width, height, alpha_type, color_type);
}

Bitmap::Bitmap(std::shared_ptr<Pixmap> pixmap, bool read_only)
    : pixmap_(std::move(pixmap)), read_only_(read_only) {}

Color Bitmap::GetPixel(uint32_t x, uint32_t y) {
  if (const uint8_t* color_start = pixmap_->Addr8(x, y)) {
    switch (GetColorType()) {
      case ColorType::kRGBA:
        return ColorSetARGB(color_start[3], color_start[0], color_start[1],
                            color_start[2]);
      case ColorType::kBGRA:
        return ColorSetARGB(color_start[3], color_start[2], color_start[1],
                            color_start[0]);
      case ColorType::kRGB565: {
        const uint16_t* color_start_16 =
            reinterpret_cast<const uint16_t*>(color_start);
        uint8_t r = color_start_16[0] >> 11 << 3;
        uint8_t g = (color_start_16[0] & 0x7E0) >> 3;
        uint8_t b = (color_start_16[0] & 0x1F) << 3;
        return ColorSetARGB(0xFF, r, g, b);
      }
      case ColorType::kA8:
        return ColorSetARGB(color_start[0], 0, 0, 0);
      case ColorType::kUnknown:
        return 0;
    }
  }

  return 0;
}

void Bitmap::SetPixel(uint32_t x, uint32_t y, Color color) {
  if (read_only_) {
    return;
  }

  if (uint8_t* color_start = pixmap_->WritableAddr8(x, y)) {
    switch (GetColorType()) {
      case ColorType::kRGBA: {
        color_start[0] = ColorGetR(color);
        color_start[1] = ColorGetG(color);
        color_start[2] = ColorGetB(color);
        color_start[3] = ColorGetA(color);
      } break;
      case ColorType::kBGRA:
        color_start[0] = ColorGetB(color);
        color_start[1] = ColorGetG(color);
        color_start[2] = ColorGetR(color);
        color_start[3] = ColorGetA(color);
        break;
      case ColorType::kRGB565: {
        uint16_t r = ColorGetR(color) >> 3;
        uint16_t g = ColorGetG(color) >> 2;
        uint16_t b = ColorGetB(color) >> 3;
        uint16_t* color_start_16 = reinterpret_cast<uint16_t*>(color_start);
        color_start_16[0] = r << 11 | g << 5 | b;
      } break;
      case ColorType::kA8:
        color_start[0] = ColorGetA(color);
        break;
      case ColorType::kUnknown:
        break;
    }
  }
}

void Bitmap::SetPixel(uint32_t x, uint32_t y, Color4f color) {
  this->SetPixel(x, y, Color4fToColor(color));
}

bool Bitmap::SetAlphaType(AlphaType alpha_type) { return false; }

bool Bitmap::SetColorType(ColorType type) { return false; }

bool Bitmap::SetColorInfo(AlphaType alpha_type, ColorType color_type) {
  return pixmap_->SetColorInfo(alpha_type, color_type);
}

}  // namespace skity
