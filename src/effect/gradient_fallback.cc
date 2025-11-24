/*
 * Copyright 2022 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/effect/gradient_fallback.hpp"

namespace skity {

namespace {

static constexpr float kFallbackThreshold = 1.f / (1 << 15);

template <typename T>
static constexpr const T& Pin(const T& x, const T& lo, const T& hi) {
  return std::max(lo, std::min(x, hi));
}

static Color4f AverageGradientColor(const Color4f colors[], const float pos[],
                                    int colorCount) {
  // The gradient is a piecewise linear interpolation between colors. For a
  // given interval, the integral between the two endpoints is 0.5 * (ci + cj)
  // * (pj - pi), which provides that intervals average color. The overall
  // average color is thus the sum of each piece. The thing to keep in mind is
  // that the provided gradient definition may implicitly use p=0 and p=1.
  Vec4 blend(0.0f);
  for (int i = 0; i < colorCount - 1; ++i) {
    // Calculate the average color for the interval between pos(i) and
    // pos(i+1)
    auto c0 = colors[i];
    auto c1 = colors[i + 1];

    // when pos == null, there are colorCount uniformly distributed stops,
    // going from 0 to 1, so pos[i + 1] - pos[i] = 1/(colorCount-1)
    float w;
    if (pos) {
      // Match position fixing in SkGradientShader's constructor, clamping
      // positions outside [0, 1] and forcing the sequence to be monotonic
      float p0 = Pin(pos[i], 0.f, 1.f);
      float p1 = Pin(pos[i + 1], p0, 1.f);
      w = p1 - p0;

      // And account for any implicit intervals at the start or end of the
      // positions
      if (i == 0) {
        if (p0 > 0.0f) {
          // The first color is fixed between p = 0 to pos[0], so 0.5*(ci +
          // cj)*(pj - pi) becomes 0.5*(c + c)*(pj - 0) = c * pj
          auto c = colors[0];
          blend += p0 * c;
        }
      }
      if (i == colorCount - 2) {
        if (p1 < 1.f) {
          // The last color is fixed between pos[n-1] to p = 1, so 0.5*(ci +
          // cj)*(pj - pi) becomes 0.5*(c + c)*(1 - pi) = c * (1 - pi)
          auto c = colors[colorCount - 1];
          blend += (1.f - p1) * c;
        }
      }
    } else {
      w = 1.f / (colorCount - 1);
    }

    blend += 0.5f * w * (c1 + c0);
  }

  return blend;
}
}  // namespace

bool NeedsFallbackToSolidColor(Shader::GradientType type,
                               const Shader::GradientInfo& info,
                               Color4f& fallback_color) {
  bool needs_fallback = false;
  switch (type) {
    case Shader::GradientType::kLinear:
      needs_fallback = FloatNearlyZero((info.point[0] - info.point[1]).Length(),
                                       kFallbackThreshold);
      break;
    case Shader::GradientType::kRadial:
      needs_fallback = FloatNearlyZero(info.radius[0], kFallbackThreshold);
      break;
    default:
      break;
  }

  if (needs_fallback) {
    switch (info.tile_mode) {
      case TileMode::kClamp:
        fallback_color = info.colors[info.color_count - 1];
        break;
      case TileMode::kRepeat:
        fallback_color = AverageGradientColor(
            info.colors.data(), info.color_offsets.data(), info.color_count);
        break;
      case TileMode::kMirror:
        fallback_color = AverageGradientColor(
            info.colors.data(), info.color_offsets.data(), info.color_count);
        break;
      case TileMode::kDecal:
        fallback_color = Colors::kTransparent;
        break;
    }
  }

  return needs_fallback;
}
}  // namespace skity
