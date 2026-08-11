// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_TEXT_GLYPH_RASTER_ALIGNMENT_HPP
#define SRC_RENDER_TEXT_GLYPH_RASTER_ALIGNMENT_HPP

#include <cmath>
#include <cstdint>

namespace skity {

// Bound the atlas variants while retaining subpixel positioning precision.
constexpr uint32_t kGlyphSubpixelPhaseCount = 64;

inline uint8_t QuantizeGlyphSubpixelPhase(float position, float scale) {
  const float pixel_position = position * scale;
  const float phase = pixel_position - std::floor(pixel_position);
  return static_cast<uint8_t>(
      std::floor(phase * static_cast<float>(kGlyphSubpixelPhaseCount)));
}

inline float GlyphSubpixelPhase(uint8_t phase) {
  return static_cast<float>(phase) /
         static_cast<float>(kGlyphSubpixelPhaseCount);
}

inline float AlignRasterPointAtOrAbove(float minimum_point, float scale,
                                       uint8_t phase) {
  const float minimum_pixel = minimum_point * scale;
  const float pixel_phase = GlyphSubpixelPhase(phase);
  return (std::ceil(minimum_pixel - pixel_phase) + pixel_phase) / scale;
}

}  // namespace skity

#endif  // SRC_RENDER_TEXT_GLYPH_RASTER_ALIGNMENT_HPP
