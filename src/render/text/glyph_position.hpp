// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_TEXT_GLYPH_POSITION_HPP
#define SRC_RENDER_TEXT_GLYPH_POSITION_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <skity/geometry/matrix.hpp>

namespace skity {

// Matches Skia's SkAxisAlignment: the named component keeps subpixel phases,
// while the other component is rounded to an integer device pixel.
enum class GlyphAxisAlignment : uint8_t {
  kNone,
  kX,
  kY,
};

constexpr uint32_t kGlyphSubpixelPositionBits = 2;
constexpr uint32_t kGlyphSubpixelPhaseCount = 1u << kGlyphSubpixelPositionBits;
constexpr float kGlyphSubpixelRoundingBias =
    1.f / static_cast<float>(kGlyphSubpixelPhaseCount * 2u);

struct GlyphPositionRoundingSpec {
  bool is_subpixel = false;
  GlyphAxisAlignment axis_alignment = GlyphAxisAlignment::kNone;

  bool UseSubpixelX() const {
    return is_subpixel && axis_alignment != GlyphAxisAlignment::kY;
  }

  bool UseSubpixelY() const {
    return is_subpixel && axis_alignment != GlyphAxisAlignment::kX;
  }
};

inline GlyphAxisAlignment ComputeAxisAlignmentForHorizontalText(
    bool baseline_snap, const Matrix& transform) {
  if (!baseline_snap) {
    return GlyphAxisAlignment::kNone;
  }
  if (transform.GetSkewY() == 0.f) {
    return GlyphAxisAlignment::kX;
  }
  if (transform.GetScaleX() == 0.f) {
    return GlyphAxisAlignment::kY;
  }
  return GlyphAxisAlignment::kNone;
}

struct QuantizedGlyphPosition {
  Vec2 position{};
  uint8_t x_phase = 0;
  uint8_t y_phase = 0;
};

struct QuantizedGlyphAxisPosition {
  float position = 0.f;
  uint8_t phase = 0;
};

inline QuantizedGlyphAxisPosition QuantizeGlyphAxisPosition(float position,
                                                            float scale,
                                                            bool use_subpixel) {
  const float physical_position = position * scale;
  const float rounding = use_subpixel ? kGlyphSubpixelRoundingBias : 0.5f;
  const float biased_position = physical_position + rounding;
  const float integer_position = std::floor(biased_position);

  uint8_t phase = 0;
  if (use_subpixel) {
    const float fractional_position = biased_position - integer_position;
    const uint32_t phase_value = std::min(
        static_cast<uint32_t>(fractional_position *
                              static_cast<float>(kGlyphSubpixelPhaseCount)),
        kGlyphSubpixelPhaseCount - 1u);
    phase = static_cast<uint8_t>(phase_value);
  }

  const float quantized_physical_position =
      integer_position +
      static_cast<float>(phase) / static_cast<float>(kGlyphSubpixelPhaseCount);
  return {quantized_physical_position / scale, phase};
}

inline QuantizedGlyphPosition QuantizeGlyphPosition(
    const Vec2& position, float scale,
    const GlyphPositionRoundingSpec& rounding_spec) {
  const QuantizedGlyphAxisPosition x = QuantizeGlyphAxisPosition(
      position.x, scale, rounding_spec.UseSubpixelX());
  const QuantizedGlyphAxisPosition y = QuantizeGlyphAxisPosition(
      position.y, scale, rounding_spec.UseSubpixelY());
  return {{x.position, y.position}, x.phase, y.phase};
}

inline float GlyphSubpixelPhase(uint8_t phase) {
  return static_cast<float>(phase) /
         static_cast<float>(kGlyphSubpixelPhaseCount);
}

inline uint8_t FlipGlyphSubpixelPhase(uint8_t phase) {
  return static_cast<uint8_t>((kGlyphSubpixelPhaseCount - phase) &
                              (kGlyphSubpixelPhaseCount - 1u));
}

inline float AlignRasterPointAtOrAbove(float minimum_point, float scale,
                                       uint8_t phase) {
  const float minimum_pixel = minimum_point * scale;
  const float pixel_phase = GlyphSubpixelPhase(phase);
  return (std::ceil(minimum_pixel - pixel_phase) + pixel_phase) / scale;
}

}  // namespace skity

#endif  // SRC_RENDER_TEXT_GLYPH_POSITION_HPP
