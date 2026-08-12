// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_HW_BLEND_PLAN_HPP
#define SRC_RENDER_HW_HW_BLEND_PLAN_HPP

#include <optional>
#include <skity/graphic/blend_mode.hpp>

#include "src/gpu/gpu_caps.hpp"
#include "src/gpu/gpu_render_pipeline.hpp"
#include "src/render/hw/dst_read_strategy.hpp"

namespace skity {

enum class HWBlendStrategy {
  kFixedFunction,
  kDualSource,
  kProgrammable,
};

enum class HWBlendOutput {
  kNone,
  kCoverage,
  kSourceTimesCoverage,
  kOneMinusSourceAlphaTimesCoverage,
  kOneMinusSourceTimesCoverage,
};

struct HWBlendFormula {
  HWBlendOutput primary_output = HWBlendOutput::kSourceTimesCoverage;
  GPUBlendFactor src_factor = GPUBlendFactor::kOne;
  GPUBlendFactor dst_factor = GPUBlendFactor::kOneMinusSrcAlpha;
  GPUBlendOperation operation = GPUBlendOperation::kAdd;
  HWBlendOutput secondary_output = HWBlendOutput::kNone;

  bool operator==(const HWBlendFormula& other) const {
    return primary_output == other.primary_output &&
           src_factor == other.src_factor && dst_factor == other.dst_factor &&
           operation == other.operation &&
           secondary_output == other.secondary_output;
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
};

std::optional<HWBlendPlan> ResolveHWBlendPlan(
    BlendMode blend_mode, bool has_fragment_mask, bool source_is_opaque,
    const GPUCaps& caps, bool supports_texture_copy_dst_read);

}  // namespace skity

#endif  // SRC_RENDER_HW_HW_BLEND_PLAN_HPP
