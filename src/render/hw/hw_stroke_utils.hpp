// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_HW_STROKE_UTILS_HPP
#define SRC_RENDER_HW_HW_STROKE_UTILS_HPP

#include <algorithm>
#include <cmath>
#include <skity/geometry/matrix.hpp>

namespace skity {

inline float ComputeMaxAxisScale(const Matrix& matrix) {
  const float sx = std::sqrt(matrix.GetScaleX() * matrix.GetScaleX() +
                             matrix.GetSkewY() * matrix.GetSkewY());
  const float sy = std::sqrt(matrix.GetSkewX() * matrix.GetSkewX() +
                             matrix.GetScaleY() * matrix.GetScaleY());
  return std::max(sx, sy);
}

inline float ComputeDeviceMinStrokeRadius(const Matrix& matrix,
                                          float stroke_width) {
  constexpr float kMinDeviceStrokeWidth = 0.5f;

  if (matrix.HasPersp()) {
    // Perspective scale varies by local position after the perspective divide.
    // This helper has no path bounds to sample, so leave perspective strokes at
    // the requested local width instead of applying an inaccurate min-device
    // clamp.
    return std::max(0.f, stroke_width) * 0.5f;
  }

  const float max_scale = ComputeMaxAxisScale(matrix);
  const float min_local_stroke_width = max_scale > 0.f
                                           ? kMinDeviceStrokeWidth / max_scale
                                           : kMinDeviceStrokeWidth;

  return std::max(stroke_width, min_local_stroke_width) * 0.5f;
}

}  // namespace skity

#endif  // SRC_RENDER_HW_HW_STROKE_UTILS_HPP
