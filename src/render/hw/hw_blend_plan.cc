// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/hw_blend_plan.hpp"

#include "src/logging.hpp"
#include "src/render/hw/native_blend.hpp"

namespace skity {
namespace {

HWBlendPlan MakePlan(
    BlendMode blend_mode, HWBlendOutput output, GPUBlendFactor src_factor,
    GPUBlendFactor dst_factor,
    GPUBlendOperation operation = GPUBlendOperation::kAdd,
    DstReadStrategy dst_read_strategy = DstReadStrategy::kNonRequired) {
  return {blend_mode,
          HWBlendStrategy::kFixedFunction,
          {output, src_factor, dst_factor, operation},
          dst_read_strategy};
}

HWBlendPlan MakeProgrammablePlan(BlendMode blend_mode, HWBlendOutput output,
                                 DstReadStrategy dst_read_strategy) {
  return {blend_mode,
          HWBlendStrategy::kProgrammable,
          {output, GPUBlendFactor::kOne, GPUBlendFactor::kZero,
           GPUBlendOperation::kAdd},
          dst_read_strategy};
}

std::optional<HWBlendPlan> ResolveOpaqueCoveragePlan(BlendMode blend_mode) {
  // For an opaque source, these modes can be represented with one fixed
  // function source and the analytical coverage folded into its output.
  switch (blend_mode) {
    case BlendMode::kSrc:
      return MakePlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                      GPUBlendFactor::kOne, GPUBlendFactor::kOneMinusSrcAlpha);
    case BlendMode::kSrcIn:
      return MakePlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                      GPUBlendFactor::kDstAlpha,
                      GPUBlendFactor::kOneMinusSrcAlpha);
    case BlendMode::kSrcOut:
      return MakePlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                      GPUBlendFactor::kOneMinusDstAlpha,
                      GPUBlendFactor::kOneMinusSrcAlpha);
    case BlendMode::kDstATop:
      return MakePlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                      GPUBlendFactor::kOneMinusDstAlpha, GPUBlendFactor::kOne);
    case BlendMode::kDstIn:
      return MakePlan(blend_mode, HWBlendOutput::kNone, GPUBlendFactor::kZero,
                      GPUBlendFactor::kOne);
    case BlendMode::kDstOut:
      return MakePlan(blend_mode, HWBlendOutput::kCoverage,
                      GPUBlendFactor::kDst, GPUBlendFactor::kOne,
                      GPUBlendOperation::kReverseSubtract);
    default:
      return std::nullopt;
  }
}

std::optional<HWBlendPlan> ResolveCoveragePlan(BlendMode blend_mode,
                                               bool source_is_opaque) {
  if (source_is_opaque) {
    if (auto plan = ResolveOpaqueCoveragePlan(blend_mode)) {
      return plan;
    }
  }

  switch (blend_mode) {
    case BlendMode::kClear:
      return MakePlan(blend_mode, HWBlendOutput::kCoverage,
                      GPUBlendFactor::kDst, GPUBlendFactor::kOne,
                      GPUBlendOperation::kReverseSubtract);
    case BlendMode::kDst:
      return MakePlan(blend_mode, HWBlendOutput::kNone, GPUBlendFactor::kZero,
                      GPUBlendFactor::kOne);
    case BlendMode::kSrcOver:
      return MakePlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                      GPUBlendFactor::kOne, GPUBlendFactor::kOneMinusSrcAlpha);
    case BlendMode::kDstOver:
      return MakePlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                      GPUBlendFactor::kOneMinusDstAlpha, GPUBlendFactor::kOne);
    case BlendMode::kDstIn:
      return MakePlan(blend_mode,
                      HWBlendOutput::kOneMinusSourceAlphaTimesCoverage,
                      GPUBlendFactor::kDst, GPUBlendFactor::kOne,
                      GPUBlendOperation::kReverseSubtract);
    case BlendMode::kDstOut:
      return MakePlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                      GPUBlendFactor::kZero, GPUBlendFactor::kOneMinusSrcAlpha);
    case BlendMode::kSrcATop:
      return MakePlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                      GPUBlendFactor::kDstAlpha,
                      GPUBlendFactor::kOneMinusSrcAlpha);
    case BlendMode::kXor:
      return MakePlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                      GPUBlendFactor::kOneMinusDstAlpha,
                      GPUBlendFactor::kOneMinusSrcAlpha);
    case BlendMode::kPlus:
      return MakePlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                      GPUBlendFactor::kOne, GPUBlendFactor::kOne);
    case BlendMode::kModulate:
      return MakePlan(blend_mode, HWBlendOutput::kOneMinusSourceTimesCoverage,
                      GPUBlendFactor::kDst, GPUBlendFactor::kOne,
                      GPUBlendOperation::kReverseSubtract);
    case BlendMode::kScreen:
      return MakePlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                      GPUBlendFactor::kOne, GPUBlendFactor::kOneMinusSrc);
    // Without an opaque source, these modes require a second source output or
    // a destination read. They are deliberately deferred to later commits.
    case BlendMode::kSrc:
    case BlendMode::kSrcIn:
    case BlendMode::kSrcOut:
    case BlendMode::kDstATop:
      return std::nullopt;
    default:
      return std::nullopt;
  }
}

std::optional<HWBlendPlan> ResolveRegularPlan(BlendMode blend_mode) {
  switch (blend_mode) {
    case BlendMode::kClear:
      return MakePlan(blend_mode, HWBlendOutput::kNone, GPUBlendFactor::kZero,
                      GPUBlendFactor::kZero);
    case BlendMode::kSrc:
      return MakePlan(blend_mode, HWBlendOutput::kSource, GPUBlendFactor::kOne,
                      GPUBlendFactor::kZero);
    case BlendMode::kDst:
      return MakePlan(blend_mode, HWBlendOutput::kNone, GPUBlendFactor::kZero,
                      GPUBlendFactor::kOne);
    case BlendMode::kSrcOver:
      return MakePlan(blend_mode, HWBlendOutput::kSource, GPUBlendFactor::kOne,
                      GPUBlendFactor::kOneMinusSrcAlpha);
    case BlendMode::kDstOver:
      return MakePlan(blend_mode, HWBlendOutput::kSource,
                      GPUBlendFactor::kOneMinusDstAlpha, GPUBlendFactor::kOne);
    case BlendMode::kSrcIn:
      return MakePlan(blend_mode, HWBlendOutput::kSource,
                      GPUBlendFactor::kDstAlpha, GPUBlendFactor::kZero);
    case BlendMode::kDstIn:
      return MakePlan(blend_mode, HWBlendOutput::kSource, GPUBlendFactor::kZero,
                      GPUBlendFactor::kSrcAlpha);
    case BlendMode::kSrcOut:
      return MakePlan(blend_mode, HWBlendOutput::kSource,
                      GPUBlendFactor::kOneMinusDstAlpha, GPUBlendFactor::kZero);
    case BlendMode::kDstOut:
      return MakePlan(blend_mode, HWBlendOutput::kSource, GPUBlendFactor::kZero,
                      GPUBlendFactor::kOneMinusSrcAlpha);
    case BlendMode::kSrcATop:
      return MakePlan(blend_mode, HWBlendOutput::kSource,
                      GPUBlendFactor::kDstAlpha,
                      GPUBlendFactor::kOneMinusSrcAlpha);
    case BlendMode::kDstATop:
      return MakePlan(blend_mode, HWBlendOutput::kSource,
                      GPUBlendFactor::kOneMinusDstAlpha,
                      GPUBlendFactor::kSrcAlpha);
    case BlendMode::kXor:
      return MakePlan(blend_mode, HWBlendOutput::kSource,
                      GPUBlendFactor::kOneMinusDstAlpha,
                      GPUBlendFactor::kOneMinusSrcAlpha);
    case BlendMode::kPlus:
      return MakePlan(blend_mode, HWBlendOutput::kSource, GPUBlendFactor::kOne,
                      GPUBlendFactor::kOne);
    case BlendMode::kModulate:
      return MakePlan(blend_mode, HWBlendOutput::kSource, GPUBlendFactor::kZero,
                      GPUBlendFactor::kSrc);
    case BlendMode::kScreen:
      return MakePlan(blend_mode, HWBlendOutput::kSource, GPUBlendFactor::kOne,
                      GPUBlendFactor::kOneMinusSrc);
    default:
      return std::nullopt;
  }
}

HWBlendPlan ResolveLegacyPlan(BlendMode blend_mode, bool has_fragment_mask) {
  if (auto plan = ResolveRegularPlan(blend_mode)) {
    if (has_fragment_mask &&
        plan->formula.primary_output == HWBlendOutput::kSource) {
      plan->formula.primary_output = HWBlendOutput::kSourceTimesCoverage;
    }
    return plan.value();
  }

  // Before HWBlendPlan, an advanced mode without a destination-read route was
  // still drawn with source replacement. Preserve that behavior here.
  return MakePlan(blend_mode,
                  has_fragment_mask ? HWBlendOutput::kSourceTimesCoverage
                                    : HWBlendOutput::kSource,
                  GPUBlendFactor::kOne, GPUBlendFactor::kZero);
}

}  // namespace

std::optional<HWBlendPlan> ResolveFixedFunctionBlendPlan(
    BlendMode blend_mode, bool has_fragment_mask, bool source_is_opaque) {
  if (!has_fragment_mask) {
    return ResolveRegularPlan(blend_mode);
  }
  return ResolveCoveragePlan(blend_mode, source_is_opaque);
}

HWBlendPlan ResolveHWBlendPlan(BlendMode blend_mode, bool has_fragment_mask,
                               bool use_coverage_aware_blending,
                               bool source_is_opaque, const GPUCaps& caps,
                               bool supports_texture_copy_dst_read) {
  if (!has_fragment_mask || use_coverage_aware_blending) {
    if (auto plan = ResolveFixedFunctionBlendPlan(blend_mode, has_fragment_mask,
                                                  source_is_opaque)) {
      return plan.value();
    }
  }

  auto strategy =
      ResolveDstReadStrategy(blend_mode, caps, supports_texture_copy_dst_read);
  if (strategy == DstReadStrategy::kNonRequired) {
    return ResolveLegacyPlan(blend_mode, has_fragment_mask);
  }

  auto output = has_fragment_mask ? HWBlendOutput::kSourceTimesCoverage
                                  : HWBlendOutput::kSource;
  if (strategy == DstReadStrategy::kNativeBlend) {
    auto operation = ToNativeBlendOp(blend_mode);
    DEBUG_CHECK(operation.has_value());
    return MakePlan(blend_mode, output, GPUBlendFactor::kOne,
                    GPUBlendFactor::kOneMinusSrcAlpha, *operation, strategy);
  }

  return MakeProgrammablePlan(blend_mode, output, strategy);
}

}  // namespace skity
