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

// A fragment shader output used by the fixed-function blend equation. Coverage
// draws cannot always output source * coverage: a few Porter-Duff modes need a
// coverage-only or complement-of-source value instead.
enum class HWBlendOutput : uint8_t {
  kNone,                              // vec4(0)
  kSource,                            // source color without coverage
  kCoverage,                          // coverage only
  kSourceTimesCoverage,               // source color * coverage
  kOneMinusSourceAlphaTimesCoverage,  // (1 - source alpha) * coverage
  kOneMinusSourceTimesCoverage,       // (1 - source color) * coverage
};

enum class HWBlendStrategy : uint8_t {
  // The GPU blend equation consumes formula directly.
  kFixedFunction,
  // The fragment shader resolves destination color before formula replaces it.
  kProgrammable,
};

struct HWBlendFormula {
  HWBlendOutput primary_output = HWBlendOutput::kSource;
  GPUBlendFactor src_factor = GPUBlendFactor::kOne;
  GPUBlendFactor dst_factor = GPUBlendFactor::kOneMinusSrcAlpha;
  GPUBlendOperation operation = GPUBlendOperation::kAdd;

  bool operator==(const HWBlendFormula& other) const {
    return primary_output == other.primary_output &&
           src_factor == other.src_factor && dst_factor == other.dst_factor &&
           operation == other.operation;
  }

  bool operator!=(const HWBlendFormula& other) const {
    return !(*this == other);
  }
};

struct HWBlendPlan {
  BlendMode blend_mode = BlendMode::kSrcOver;
  HWBlendStrategy strategy = HWBlendStrategy::kFixedFunction;
  HWBlendFormula formula = {};
  DstReadStrategy dst_read_strategy = DstReadStrategy::kNonRequired;

  bool operator==(const HWBlendPlan& other) const {
    return blend_mode == other.blend_mode && strategy == other.strategy &&
           formula == other.formula &&
           dst_read_strategy == other.dst_read_strategy;
  }

  bool operator!=(const HWBlendPlan& other) const { return !(*this == other); }
};

// Resolve the fixed-function equation for a draw whose source may be covered
// by an analytical fragment mask. A null result means that the mode needs a
// second source or a destination read and is intentionally left to a later
// blending stage. The source opacity matters because an opaque source removes
// the destination-alpha dependency from several modes.
std::optional<HWBlendPlan> ResolveFixedFunctionBlendPlan(BlendMode blend_mode,
                                                         bool has_fragment_mask,
                                                         bool source_is_opaque);

// Resolve the complete route for an existing draw. Coverage-aware formulas are
// only selected when explicitly requested by a compatible coverage producer;
// other masked draws preserve the behavior used before HWBlendPlan was
// introduced.
HWBlendPlan ResolveHWBlendPlan(BlendMode blend_mode, bool has_fragment_mask,
                               bool use_coverage_aware_blending,
                               bool source_is_opaque, const GPUCaps& caps,
                               bool supports_texture_copy_dst_read);

}  // namespace skity

#endif  // SRC_RENDER_HW_HW_BLEND_PLAN_HPP
