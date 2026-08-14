// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_NATIVE_BLEND_HPP
#define SRC_RENDER_HW_NATIVE_BLEND_HPP

#include <optional>
#include <skity/graphic/blend_mode.hpp>

#include "src/gpu/gpu_render_pipeline.hpp"

namespace skity {

// Maps a BlendMode to its fixed-function advanced blend operation, used when
// the hardware exposes GL_KHR_blend_equation_advanced /
// VK_EXT_blend_operation_advanced.
//
// Coefficient modes (<= kLastCoeffMode) never reach this function; they use
// standard fixed-function blending. Callers are expected to gate with
// IsAdvancedBlendMode first.
constexpr std::optional<GPUBlendOperation> ToNativeBlendOp(BlendMode mode) {
  switch (mode) {
    case BlendMode::kOverlay:
      return GPUBlendOperation::kOverlay;
    case BlendMode::kDarken:
      return GPUBlendOperation::kDarken;
    case BlendMode::kLighten:
      return GPUBlendOperation::kLighten;
    case BlendMode::kColorDodge:
      return GPUBlendOperation::kColorDodge;
    case BlendMode::kColorBurn:
      return GPUBlendOperation::kColorBurn;
    case BlendMode::kHardLight:
      return GPUBlendOperation::kHardLight;
    case BlendMode::kSoftLight:
      return GPUBlendOperation::kSoftLight;
    case BlendMode::kDifference:
      return GPUBlendOperation::kDifference;
    case BlendMode::kExclusion:
      return GPUBlendOperation::kExclusion;
    case BlendMode::kMultiply:
      return GPUBlendOperation::kMultiply;
    case BlendMode::kHue:
      return GPUBlendOperation::kHslHue;
    case BlendMode::kSaturation:
      return GPUBlendOperation::kHslSaturation;
    case BlendMode::kColor:
      return GPUBlendOperation::kHslColor;
    case BlendMode::kLuminosity:
      return GPUBlendOperation::kHslLuminosity;
    default:
      // Coefficient modes stay on the standard fixed-function path.
      return std::nullopt;
  }
}

}  // namespace skity

#endif  // SRC_RENDER_HW_NATIVE_BLEND_HPP
