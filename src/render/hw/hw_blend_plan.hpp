// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_HW_BLEND_PLAN_HPP
#define SRC_RENDER_HW_HW_BLEND_PLAN_HPP

#include <cstdint>
#include <optional>
#include <skity/graphic/blend_mode.hpp>

#include "src/gpu/gpu_caps.hpp"
#include "src/gpu/gpu_render_pipeline.hpp"
#include "src/render/hw/dst_read_strategy.hpp"

namespace skity {

// A fragment shader output consumed by the fixed-function blend equation.
// Analytical coverage cannot always be folded into the source color; some
// Porter-Duff modes need a coverage-only or complement-of-source value.
// S is the premultiplied source color and C is the fragment coverage.
enum class HWBlendOutput : uint8_t {
  kNone,                              // vec4(0)
  kSource,                            // S
  kCoverage,                          // vec4(C)
  kSourceTimesCoverage,               // S * C
  kOneMinusSourceAlphaTimesCoverage,  // vec4((1 - S.a) * C)
  kOneMinusSourceTimesCoverage,       // (vec4(1) - S) * C
};

struct HWBlendFormula {
  HWBlendOutput primary_output = HWBlendOutput::kSource;
  HWBlendOutput secondary_output = HWBlendOutput::kNone;
  GPUBlendFactor src_factor = GPUBlendFactor::kOne;
  GPUBlendFactor dst_factor = GPUBlendFactor::kOneMinusSrcAlpha;
  GPUBlendOperation operation = GPUBlendOperation::kAdd;

  bool operator==(const HWBlendFormula& other) const {
    return primary_output == other.primary_output &&
           secondary_output == other.secondary_output &&
           src_factor == other.src_factor && dst_factor == other.dst_factor &&
           operation == other.operation;
  }

  bool operator!=(const HWBlendFormula& other) const {
    return !(*this == other);
  }
};

struct HWBlendPlan {
  BlendMode blend_mode = BlendMode::kSrcOver;
  HWBlendFormula formula = {};
  DstReadStrategy dst_read_strategy = DstReadStrategy::kNonRequired;

  bool operator==(const HWBlendPlan& other) const {
    return blend_mode == other.blend_mode && formula == other.formula &&
           dst_read_strategy == other.dst_read_strategy;
  }

  bool operator!=(const HWBlendPlan& other) const { return !(*this == other); }
};

// Resolves an unmasked coefficient blend mode. These formulas do not depend on
// source opacity or device capabilities.
HWBlendPlan ResolveCoefficientBlendPlan(BlendMode blend_mode);

// Resolves the complete shader output, fixed-function equation, and optional
// destination-read route. A null result is possible only for a fragment-mask
// draw whose correct coverage-aware blend cannot be expressed by the device.
std::optional<HWBlendPlan> ResolveHWBlendPlan(
    BlendMode blend_mode, bool has_fragment_mask, bool source_is_opaque,
    const GPUCaps& caps, bool supports_texture_copy_dst_read);

}  // namespace skity

#endif  // SRC_RENDER_HW_HW_BLEND_PLAN_HPP
