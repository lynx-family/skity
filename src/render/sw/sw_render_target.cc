// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/sw/sw_render_target.hpp"

#include "src/graphic/blend_mode_priv.hpp"
#include "src/graphic/color_priv.hpp"

namespace skity {

void SWRenderTarget::BlendPixel(uint32_t x, uint32_t y, Color src,
                                BlendMode blend) {
  if (!pixel_addr_) {
    return;
  }

  if (x >= bitmap_->Width() || y >= bitmap_->Height()) {
    return;
  }

  if (FastBlend(x, y, src, blend)) {
    return;
  }

  PMColor dst_pm = bitmap_->GetAlphaType() == AlphaType::kPremul_AlphaType
                       ? bitmap_->GetPixel(x, y)
                       : ColorToPMColor(bitmap_->GetPixel(x, y));

  PMColor r_pm = PorterDuffBlend(src, dst_pm, blend);
  bitmap_->SetPixel(x, y,
                    bitmap_->GetAlphaType() == AlphaType::kPremul_AlphaType
                        ? r_pm
                        : PMColorToColor(r_pm));
}

void SWRenderTarget::BlendPixel(uint32_t x, uint32_t y, Color4f color,
                                BlendMode blend) {
  this->BlendPixel(x, y, Color4fToColor(color), blend);
}

void SWRenderTarget::BlendPixelH(uint32_t x, uint32_t y, PMColor* pm_colors,
                                 uint32_t len, BlendMode blend) {
#ifdef SKITY_ARM_NEON
  if (bitmap_->GetAlphaType() == AlphaType::kPremul_AlphaType &&
      blend != BlendMode::kSoftLight) {
    BlendPixelNeon(x, y, pm_colors, len, blend);
    return;
  }
#endif

  for (uint32_t i = 0; i < len; i++) {
    BlendPixel(x + i, y, pm_colors[i], blend);
  }
}

void SWRenderTarget::BlendPixelH(uint32_t x, uint32_t y, PMColor pm_color,
                                 uint32_t len, BlendMode blend) {
#ifdef SKITY_ARM_NEON
  if (bitmap_->GetAlphaType() == AlphaType::kPremul_AlphaType &&
      blend != BlendMode::kSoftLight) {
    BlendPixelNeon(x, y, pm_color, len, blend);
    return;
  }
#endif

  for (uint32_t i = 0; i < len; i++) {
    BlendPixel(x + i, y, pm_color, blend);
  }
}

#ifdef SKITY_ARM_NEON
void SWRenderTarget::BlendPixelNeon(uint32_t x, uint32_t y, PMColor* pm_colors,
                                    uint32_t len, BlendMode blend) {
  auto dst = pixel_addr_ + y * bitmap_->RowBytes() + x * 4;

  ProterDuffBlendNeon(pm_colors, reinterpret_cast<uint32_t*>(dst), len, blend);
}

void SWRenderTarget::BlendPixelNeon(uint32_t x, uint32_t y, PMColor pm_color,
                                    uint32_t len, BlendMode blend) {
  auto dst = pixel_addr_ + y * bitmap_->RowBytes() + x * 4;

  ProterDuffBlendNeon(pm_color, reinterpret_cast<uint32_t*>(dst), len, blend);
}
#endif

bool SWRenderTarget::FastBlend(uint32_t x, uint32_t y, Color color,
                               BlendMode blend) {
  // TODO(tangruiwen): Handle other blend mode
  size_t base_index = y * bitmap_->RowBytes() + x * 4;

  if (blend == BlendMode::kClear) {
    pixel_addr_[base_index] = 0;
    pixel_addr_[base_index + 1] = 0;
    pixel_addr_[base_index + 2] = 0;
    pixel_addr_[base_index + 3] = 0;

    return true;
  } else if (blend == BlendMode::kSrc) {
    bitmap_->SetPixel(x, y, color);
    return true;
  } else if (blend == BlendMode::kDst) {
    return true;
  } else if (blend == BlendMode::kSrcOver) {
    auto a = ColorGetA(color);
    if (a == 0) {
      return true;
    } else if (a == 255) {
      bitmap_->SetPixel(x, y, color);
      return true;
    }
  } else if (blend == BlendMode::kDstOver &&
             pixel_addr_[base_index + 3] == 255) {
    return true;
  } else if (blend == BlendMode::kDstIn) {
    auto a = ColorGetA(color);
    if (a == 255) {
      return true;
    } else if (a == 0) {
      pixel_addr_[base_index] = 0;
      pixel_addr_[base_index + 1] = 0;
      pixel_addr_[base_index + 2] = 0;
      pixel_addr_[base_index + 3] = 0;
      return true;
    }
  } else if (blend == BlendMode::kDstOut) {
    auto a = ColorGetA(color);
    if (a == 0) {
      return true;
    } else if (a == 255) {
      pixel_addr_[base_index] = 0;
      pixel_addr_[base_index + 1] = 0;
      pixel_addr_[base_index + 2] = 0;
      pixel_addr_[base_index + 3] = 0;
      return true;
    }
  }

  return false;
}

}  // namespace skity
