// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/sw/sw_render_target.hpp"

#include "src/graphic/blend_mode_priv.hpp"
#include "src/graphic/color_priv.hpp"

namespace skity {

static bool IsOpaquePixel(const Pixmap& pixmap, const uint8_t* pixels,
                          uint32_t x, uint32_t y) {
  const uint8_t* row = pixels + y * pixmap.RowBytes();
  switch (pixmap.GetColorType()) {
    case ColorType::kRGBA:
    case ColorType::kBGRA:
      return row[static_cast<size_t>(x) * 4 + 3] == 255;
    case ColorType::kA8:
      return row[x] == 255;
    case ColorType::kRGB565:
      return true;
    default:
      return false;
  }
}

void SWRenderTarget::BlendPixel(uint32_t x, uint32_t y, Color src,
                                BlendMode blend, uint8_t coverage) {
  if (!pixel_addr_ || coverage == 0) {
    return;
  }

  if (x >= bitmap_->Width() || y >= bitmap_->Height()) {
    return;
  }

  if (blend == BlendMode::kDstOver &&
      IsOpaquePixel(*bitmap_->GetPixmap(), pixel_addr_, x, y)) {
    return;
  }
  if (coverage == 255 && FastBlend(x, y, src, blend)) {
    return;
  }
  const bool unpremul = bitmap_->GetAlphaType() == kUnpremul_AlphaType;
  Color dst = bitmap_->GetPixel(x, y);
  PMColor dst_pm = unpremul ? ColorToPMColor(dst) : dst;
  PMColor result =
      ApplyCoverage(PorterDuffBlend(src, dst_pm, blend), dst_pm, coverage);
  // A round trip through premultiplied color can lose straight RGB precision.
  if (result == dst_pm) {
    return;
  }
  if (unpremul && result != Color_TRANSPARENT) {
    // Erasing scales the destination alpha and preserves its straight RGB.
    if (blend == BlendMode::kClear || blend == BlendMode::kDstIn ||
        blend == BlendMode::kDstOut) {
      result = ColorSetA(dst, ColorGetA(result));
    } else {
      result = PMColorToColor(result);
    }
  }
  bitmap_->SetPixel(x, y, result);
}

void SWRenderTarget::BlendPixel(uint32_t x, uint32_t y, Color4f color,
                                BlendMode blend) {
  this->BlendPixel(x, y, Color4fToColor(color), blend);
}

void SWRenderTarget::BlendPixelH(uint32_t x, uint32_t y,
                                 const PMColor* pm_colors, uint32_t len,
                                 BlendMode blend, uint8_t coverage) {
#ifdef SKITY_ARM_NEON
  if (auto* dst = GetNeonDestination(x, y, blend)) {
    PorterDuffBlendNeon(pm_colors, dst, len, blend, bitmap_->GetColorType(),
                        coverage);
    return;
  }
#endif

  for (uint32_t i = 0; i < len; i++) {
    BlendPixel(x + i, y, pm_colors[i], blend, coverage);
  }
}

void SWRenderTarget::BlendPixelH(uint32_t x, uint32_t y, PMColor pm_color,
                                 uint32_t len, BlendMode blend,
                                 uint8_t coverage) {
#ifdef SKITY_ARM_NEON
  if (auto* dst = GetNeonDestination(x, y, blend)) {
    PorterDuffBlendNeon(pm_color, dst, len, blend, bitmap_->GetColorType(),
                        coverage);
    return;
  }
#endif

  for (uint32_t i = 0; i < len; i++) {
    BlendPixel(x + i, y, pm_color, blend, coverage);
  }
}

#ifdef SKITY_ARM_NEON
uint32_t* SWRenderTarget::GetNeonDestination(uint32_t x, uint32_t y,
                                             BlendMode blend) const {
  if (!pixel_addr_ || IsAdvancedBlendMode(blend) ||
      (bitmap_->GetColorType() != ColorType::kRGBA &&
       bitmap_->GetColorType() != ColorType::kBGRA) ||
      bitmap_->GetAlphaType() != kPremul_AlphaType) {
    return nullptr;
  }
  auto* dst = bitmap_->GetPixmap()->WritableAddr8(x, y);
  // Rows in a caller-supplied pixmap may have unaligned padding.
  if (reinterpret_cast<uintptr_t>(dst) % alignof(uint32_t) != 0) {
    return nullptr;
  }
  return reinterpret_cast<uint32_t*>(dst);
}
#endif

bool SWRenderTarget::FastBlend(uint32_t x, uint32_t y, Color color,
                               BlendMode blend) {
  const auto alpha = ColorGetA(color);
  if (blend == BlendMode::kClear ||
      (blend == BlendMode::kDstIn && alpha == 0) ||
      (blend == BlendMode::kDstOut && alpha == 255)) {
    bitmap_->SetPixel(x, y, Color_TRANSPARENT);
    return true;
  }
  if (blend == BlendMode::kSrc ||
      (blend == BlendMode::kSrcOver && alpha == 255)) {
    bitmap_->SetPixel(
        x, y,
        alpha != 255 && bitmap_->GetAlphaType() == kUnpremul_AlphaType
            ? PMColorToColor(color)
            : color);
    return true;
  }
  return blend == BlendMode::kDst ||
         (blend == BlendMode::kSrcOver && alpha == 0) ||
         (blend == BlendMode::kDstIn && alpha == 255) ||
         (blend == BlendMode::kDstOut && alpha == 0);
}

}  // namespace skity
