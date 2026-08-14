// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_WGX_BLEND_HPP
#define SRC_RENDER_HW_DRAW_WGX_BLEND_HPP

#include <skity/graphic/blend_mode.hpp>
#include <string>
#include <string_view>

namespace skity {

inline std::string BuildPorterDuffBlendExpression(BlendMode mode,
                                                  std::string_view src,
                                                  std::string_view dst,
                                                  bool clamp_plus) {
  const std::string s{src};
  const std::string d{dst};
  switch (mode) {
    case BlendMode::kClear:
      return "vec4<f32>(0.0, 0.0, 0.0, 0.0)";
    case BlendMode::kSrc:
      return s;
    case BlendMode::kDst:
      return d;
    case BlendMode::kSrcOver:
      return s + " + " + d + " * (1.0 - " + s + ".a)";
    case BlendMode::kDstOver:
      return d + " + " + s + " * (1.0 - " + d + ".a)";
    case BlendMode::kSrcIn:
      return s + " * " + d + ".a";
    case BlendMode::kDstIn:
      return d + " * " + s + ".a";
    case BlendMode::kSrcOut:
      return s + " * (1.0 - " + d + ".a)";
    case BlendMode::kDstOut:
      return d + " * (1.0 - " + s + ".a)";
    case BlendMode::kSrcATop:
      return s + " * " + d + ".a + " + d + " * (1.0 - " + s + ".a)";
    case BlendMode::kDstATop:
      return s + ".a * " + d + " + " + s + " * (1.0 - " + d + ".a)";
    case BlendMode::kXor:
      return s + " * (1.0 - " + d + ".a) + " + d + " * (1.0 - " + s + ".a)";
    case BlendMode::kPlus: {
      auto sum = s + " + " + d;
      return clamp_plus ? "min(" + sum + ", vec4<f32>(1.0))" : sum;
    }
    case BlendMode::kModulate:
      return s + " * " + d;
    case BlendMode::kScreen:
      return s + " + " + d + " - " + s + " * " + d;
    default:
      return {};
  }
}

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_WGX_BLEND_HPP
