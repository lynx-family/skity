// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_SHADING_INFO_HPP
#define SRC_RENDER_HW_SHADING_INFO_HPP

#include <skity/effect/shader.hpp>
#include <skity/graphic/paint.hpp>

namespace skity {
enum class ShadingType {
  kSolidColor = 0,
  kLinearGradient = 1,
  kRadialGradient = 2,
  kConicalGradient = 3,
  kSweepGradient = 4,
  kUnknown,
};

struct ShadingInfo {
  static ShadingInfo Make(const Paint paint, Vector color) {
    ShadingInfo shading_info{};
    shading_info.type = ShadingType::kUnknown;
    shading_info.color = color;
    if (paint.GetShader()) {
      Shader::GradientType gradient_type =
          paint.GetShader()->AsGradient(&shading_info.gradient_info);
      switch (gradient_type) {
        case Shader::GradientType::kLinear:
          shading_info.type = ShadingType::kLinearGradient;
          break;
        case Shader::GradientType::kRadial:
          shading_info.type = ShadingType::kRadialGradient;
          break;
        case Shader::GradientType::kConical:
          shading_info.type = ShadingType::kConicalGradient;
          break;
        case Shader::GradientType::kSweep:
          shading_info.type = ShadingType::kSweepGradient;
          break;
        // TODO(zhangzhijian): support image shader
        case Shader::GradientType::kNone:
        default:
          break;
      }
    }
    if (shading_info.type == ShadingType::kUnknown) {
      shading_info.type = ShadingType::kSolidColor;
    }
    return shading_info;
  }

  ShadingType type;
  Vector color;
  Shader::GradientInfo gradient_info{};
};
}  // namespace skity

#endif  // SRC_RENDER_HW_SHADING_INFO_HPP
