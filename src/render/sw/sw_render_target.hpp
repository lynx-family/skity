// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_SW_SW_RENDER_TARGET_HPP
#define SRC_RENDER_SW_SW_RENDER_TARGET_HPP

#include <skity/graphic/bitmap.hpp>
#include <skity/graphic/blend_mode.hpp>
#include <skity/macros.hpp>

namespace skity {

class SWRenderTarget {
 public:
  explicit SWRenderTarget(Bitmap* bitmap)
      : bitmap_(bitmap),
        pixel_addr_(bitmap_ != nullptr ? bitmap->GetPixelAddr() : nullptr) {}

  // blend pixel with premultiplied color
  void BlendPixel(uint32_t x, uint32_t y, Color color,
                  BlendMode blend = BlendMode::kDefault);

  void BlendPixel(uint32_t x, uint32_t y, Color4f color,
                  BlendMode blend = BlendMode::kDefault);

  void BlendPixelH(uint32_t x, uint32_t y, PMColor* pm_colors, uint32_t len,
                   BlendMode blend = BlendMode::kDefault);

  void BlendPixelH(uint32_t x, uint32_t y, PMColor pm_color, uint32_t len,
                   BlendMode blend = BlendMode::kDefault);

 private:
  bool FastBlend(uint32_t x, uint32_t y, Color color, BlendMode blend);

#ifdef SKITY_ARM_NEON
  void BlendPixelNeon(uint32_t x, uint32_t y, PMColor* pm_colors, uint32_t len,
                      BlendMode blend);

  void BlendPixelNeon(uint32_t x, uint32_t y, PMColor pm_color, uint32_t len,
                      BlendMode blend);

#endif
  Bitmap* bitmap_;
  uint8_t* pixel_addr_;
};

}  // namespace skity

#endif  // SRC_RENDER_SW_SW_RENDER_TARGET_HPP
