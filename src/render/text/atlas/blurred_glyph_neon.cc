// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/text/atlas/blurred_glyph.hpp"

#ifdef SKITY_GLYPH_BLUR_NEON
#include <arm_neon.h>

#include <algorithm>
#include <cstring>

namespace skity {
namespace {

// FMULX gives the same rounded product as FMUL for finite coverage and
// weights. The explicit instruction keeps a compiler with FP contraction
// enabled from fusing the multiply and add across an A8 rounding boundary.
float32x4_t Accumulate(float32x4_t sum, float32x4_t value, float32x4_t weight) {
  return vaddq_f32(sum, vmulxq_f32(value, weight));
}

uint8x8_t RoundToA8(float32x4_t low, float32x4_t high) {
  // Coverage is nonnegative. FCVTAU rounds halfway away from zero, like round.
  return vmovn_u16(vcombine_u16(vmovn_u32(vcvtaq_u32_f32(low)),
                                vmovn_u32(vcvtaq_u32_f32(high))));
}

}  // namespace

std::vector<uint8_t> BlurGlyphMask(const GlyphBitmapData& source,
                                   const std::vector<float>& weights_x,
                                   const std::vector<float>& weights_y) {
  const int width = static_cast<int>(source.width);
  const int height = static_cast<int>(source.height);
  const int taps_x = static_cast<int>(weights_x.size());
  const int taps_y = static_cast<int>(weights_y.size());
  const int output_width = width + taps_x - 1;
  const int output_height = height + taps_y - 1;

  // Zero padding makes the last four-lane input and sixteen-lane intermediate
  // loads safe. Source bitmap rows and the returned A8 rows keep their layout.
  const int alpha_width = (width + 3) & ~3;
  const int stride = (alpha_width + taps_x - 1 + 15) & ~15;
  std::vector<float> alpha(alpha_width);
  std::vector<float> horizontal(stride * height);
  const bool gray = source.format == BitmapFormat::kGray8;
  for (int y = 0; y < height; ++y) {
    const auto* src = source.buffer + y * source.RowBytes();
    for (int x = 0; x < width; ++x) {
      alpha[x] = gray ? src[x] : src[4 * x + 3];
    }
    for (int k = 0; k < taps_x; ++k) {
      auto* dst = horizontal.data() + y * stride + k;
      const auto weight = vdupq_n_f32(weights_x[k]);
      for (int x = 0; x < alpha_width; x += 4) {
        auto sum =
            Accumulate(vld1q_f32(dst + x), vld1q_f32(alpha.data() + x), weight);
        vst1q_f32(dst + x, sum);
      }
    }
  }

  std::vector<uint8_t> pixels(output_width * output_height);
  for (int y = 0; y < output_height; ++y) {
    const int first = std::max(0, y - (taps_y - 1));
    const int last = std::min(y, height - 1);
    for (int x = 0; x < output_width; x += 16) {
      // Keep sixteen output pixels in registers across all contributing rows.
      auto sum0 = vdupq_n_f32(0.f);
      auto sum1 = sum0, sum2 = sum0, sum3 = sum0;
      for (int sy = first; sy <= last; ++sy) {
        const auto* src = horizontal.data() + sy * stride + x;
        const auto weight = vdupq_n_f32(weights_y[y - sy]);
        sum0 = Accumulate(sum0, vld1q_f32(src), weight);
        sum1 = Accumulate(sum1, vld1q_f32(src + 4), weight);
        sum2 = Accumulate(sum2, vld1q_f32(src + 8), weight);
        sum3 = Accumulate(sum3, vld1q_f32(src + 12), weight);
      }
      const auto bytes =
          vcombine_u8(RoundToA8(sum0, sum1), RoundToA8(sum2, sum3));
      auto* dst = pixels.data() + y * output_width + x;
      if (x + 16 <= output_width) {
        vst1q_u8(dst, bytes);
      } else {
        uint8_t tail[16];
        vst1q_u8(tail, bytes);
        std::memcpy(dst, tail, output_width - x);
      }
    }
  }
  return pixels;
}

}  // namespace skity
#endif  // SKITY_GLYPH_BLUR_NEON
