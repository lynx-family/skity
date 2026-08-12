// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef TEST_UT_RENDER_HW_COVERAGE_BLEND_TEST_UTILS_HPP
#define TEST_UT_RENDER_HW_COVERAGE_BLEND_TEST_UTILS_HPP

#include <algorithm>
#include <array>
#include <skity/graphic/blend_mode.hpp>

namespace skity::testing {

using FloatColor = std::array<float, 4>;

inline FloatColor Add(FloatColor lhs, FloatColor rhs) {
  return {lhs[0] + rhs[0], lhs[1] + rhs[1], lhs[2] + rhs[2], lhs[3] + rhs[3]};
}

inline FloatColor Multiply(FloatColor color, float scale) {
  return {color[0] * scale, color[1] * scale, color[2] * scale,
          color[3] * scale};
}

inline FloatColor Multiply(FloatColor lhs, FloatColor rhs) {
  return {lhs[0] * rhs[0], lhs[1] * rhs[1], lhs[2] * rhs[2], lhs[3] * rhs[3]};
}

inline FloatColor ReferencePorterDuffBlend(FloatColor src, FloatColor dst,
                                           BlendMode mode) {
  switch (mode) {
    case BlendMode::kClear:
      return {};
    case BlendMode::kSrc:
      return src;
    case BlendMode::kDst:
      return dst;
    case BlendMode::kSrcOver:
      return Add(src, Multiply(dst, 1.f - src[3]));
    case BlendMode::kDstOver:
      return Add(dst, Multiply(src, 1.f - dst[3]));
    case BlendMode::kSrcIn:
      return Multiply(src, dst[3]);
    case BlendMode::kDstIn:
      return Multiply(dst, src[3]);
    case BlendMode::kSrcOut:
      return Multiply(src, 1.f - dst[3]);
    case BlendMode::kDstOut:
      return Multiply(dst, 1.f - src[3]);
    case BlendMode::kSrcATop:
      return Add(Multiply(src, dst[3]), Multiply(dst, 1.f - src[3]));
    case BlendMode::kDstATop:
      return Add(Multiply(dst, src[3]), Multiply(src, 1.f - dst[3]));
    case BlendMode::kXor:
      return Add(Multiply(src, 1.f - dst[3]), Multiply(dst, 1.f - src[3]));
    case BlendMode::kPlus: {
      auto result = Add(src, dst);
      for (auto& component : result) {
        component = std::min(component, 1.f);
      }
      return result;
    }
    case BlendMode::kModulate:
      return Multiply(src, dst);
    case BlendMode::kScreen:
      return Add(src, Multiply(dst, {1.f - src[0], 1.f - src[1], 1.f - src[2],
                                     1.f - src[3]}));
    default:
      return {};
  }
}

inline FloatColor ApplyCoverage(FloatColor src, FloatColor dst, BlendMode mode,
                                float coverage) {
  auto blended = mode == BlendMode::kPlus
                     ? Add(src, dst)
                     : ReferencePorterDuffBlend(src, dst, mode);
  auto result = Add(Multiply(blended, coverage), Multiply(dst, 1.f - coverage));
  if (mode == BlendMode::kPlus) {
    for (auto& component : result) {
      component = std::min(component, 1.f);
    }
  }
  return result;
}

}  // namespace skity::testing

#endif  // TEST_UT_RENDER_HW_COVERAGE_BLEND_TEST_UTILS_HPP
