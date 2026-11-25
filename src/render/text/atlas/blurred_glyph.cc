// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/text/atlas/blurred_glyph.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "src/render/text/atlas/atlas_manager.hpp"

namespace skity {
namespace {

// Match the finite, normalized kernel used by the hardware mask filter. The
// glyph has transparent coverage outside its source rectangle.
std::vector<float> MakeBlurWeights(float radius, int support) {
  const float sigma = radius * 0.57735f + 0.5f;
  std::vector<float> weights(2 * support + 1);
  float sum = 0.f;
  for (int k = -support; k <= support; ++k) {
    const float w = std::exp(-0.5f * k * k / (sigma * sigma));
    weights[k + support] = w;
    sum += w;
  }
  for (auto& w : weights) {
    w /= sum;
  }
  return weights;
}

}  // namespace

#if !defined(SKITY_GLYPH_BLUR_NEON) && !defined(SKITY_GLYPH_BLUR_X64)
std::vector<uint8_t> BlurGlyphMask(const GlyphBitmapData& source,
                                   const std::vector<float>& weights_x,
                                   const std::vector<float>& weights_y) {
  const int width = static_cast<int>(source.width);
  const int height = static_cast<int>(source.height);
  const int taps_x = static_cast<int>(weights_x.size());
  const int taps_y = static_cast<int>(weights_y.size());
  const int output_width = width + taps_x - 1;
  const int output_height = height + taps_y - 1;
  std::vector<float> horizontal(output_width * height);
  std::vector<float> alpha(width);
  const bool gray = source.format == BitmapFormat::kGray8;
  for (int y = 0; y < height; ++y) {
    const auto* src = source.buffer + y * source.RowBytes();
    for (int x = 0; x < width; ++x) {
      alpha[x] = gray ? src[x] : src[4 * x + 3];
    }
    for (int k = 0; k < taps_x; ++k) {
      auto* dst = horizontal.data() + y * output_width + k;
      for (int x = 0; x < width; ++x) {
        dst[x] += alpha[x] * weights_x[k];
      }
    }
  }

  std::vector<uint8_t> pixels(output_width * output_height);
  std::vector<float> row(output_width);
  for (int y = 0; y < output_height; ++y) {
    std::fill(row.begin(), row.end(), 0.f);
    for (int sy = std::max(0, y - taps_y + 1); sy <= std::min(y, height - 1);
         ++sy) {
      const auto* src = horizontal.data() + sy * output_width;
      const float weight = weights_y[y - sy];
      for (int x = 0; x < output_width; ++x) {
        row[x] += src[x] * weight;
      }
    }
    for (int x = 0; x < output_width; ++x) {
      pixels[y * output_width + x] = static_cast<uint8_t>(std::round(row[x]));
    }
  }
  return pixels;
}
#endif

AtlasGlyphResult Atlas::GenerateBlurredGlyphRegion(
    const GlyphKey& key, const GlyphBitmapData& source) {
  // PrepareOwnedImage validated the source dimensions, buffer and stride.
  const int max_side = atlas_config_.max_bitmap_size - Atlas_Padding - 2;
  if (!std::isfinite(key.blur_radius.x) || !std::isfinite(key.blur_radius.y) ||
      key.blur_radius.x <= 0 || key.blur_radius.y <= 0 ||
      key.blur_radius.x >= max_side || key.blur_radius.y >= max_side) {
    return {AtlasGlyphStatus::kInvalidImage};
  }
  const int rx = std::max(1, static_cast<int>(key.blur_radius.x));
  const int ry = std::max(1, static_cast<int>(key.blur_radius.y));
  if (source.width > max_side - 2 * rx || source.height > max_side - 2 * ry) {
    const auto saturated_extent = [](float extent, int padding) {
      return static_cast<uint32_t>(
          std::min(static_cast<double>(std::numeric_limits<uint32_t>::max()),
                   static_cast<double>(extent) + 2 * padding));
    };
    return {AtlasGlyphStatus::kTooLarge,
            {},
            saturated_extent(source.width, rx),
            saturated_extent(source.height, ry)};
  }
  auto pixels = BlurGlyphMask(source, MakeBlurWeights(key.blur_radius.x, rx),
                              MakeBlurWeights(key.blur_radius.y, ry));

  GlyphBitmapData mask{};
  mask.buffer = pixels.data();
  mask.width = static_cast<int>(source.width) + 2 * rx;
  mask.height = static_cast<int>(source.height) + 2 * ry;
  mask.format = BitmapFormat::kGray8;
  mask.origin_x = source.origin_x - rx / key.scaler_context_desc.context_scale;
  mask.origin_y = source.origin_y + ry / key.scaler_context_desc.context_scale;
  return GenerateGlyphRegionInternal(key, mask);
}

}  // namespace skity
