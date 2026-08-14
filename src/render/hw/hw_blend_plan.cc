// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/hw_blend_plan.hpp"

#include "src/graphic/blend_mode_priv.hpp"
#include "src/render/hw/native_blend.hpp"

namespace skity {
namespace {

HWBlendPlan MakeFixedPlan(
    BlendMode blend_mode, HWBlendOutput output, GPUBlendFactor src_factor,
    GPUBlendFactor dst_factor,
    GPUBlendOperation operation = GPUBlendOperation::kAdd,
    DstReadStrategy dst_read_strategy = DstReadStrategy::kNonRequired) {
  return {blend_mode,
          HWBlendStrategy::kFixedFunction,
          {output, src_factor, dst_factor, operation},
          dst_read_strategy};
}

std::optional<HWBlendPlan> MakeProgrammablePlan(
    BlendMode blend_mode, const GPUCaps& caps,
    bool supports_texture_copy_dst_read) {
  DstReadStrategy strategy;
  if (caps.supports_framebuffer_fetch) {
    strategy = DstReadStrategy::kFramebufferFetch;
  } else if (supports_texture_copy_dst_read) {
    strategy = DstReadStrategy::kTextureCopy;
  } else {
    return std::nullopt;
  }

  return HWBlendPlan{blend_mode,
                     HWBlendStrategy::kProgrammable,
                     {HWBlendOutput::kSourceTimesCoverage, GPUBlendFactor::kOne,
                      GPUBlendFactor::kZero, GPUBlendOperation::kAdd},
                     strategy};
}

HWBlendPlan MakeDualSourcePlan(BlendMode blend_mode,
                               HWBlendOutput secondary_output,
                               GPUBlendFactor src_factor,
                               GPUBlendFactor dst_factor) {
  HWBlendFormula formula = {HWBlendOutput::kSourceTimesCoverage, src_factor,
                            dst_factor, GPUBlendOperation::kAdd};
  formula.secondary_output = secondary_output;
  return {blend_mode, HWBlendStrategy::kDualSource, formula,
          DstReadStrategy::kNonRequired};
}

std::optional<HWBlendPlan> ResolveAdvancedPlan(
    BlendMode blend_mode, const GPUCaps& caps,
    bool supports_texture_copy_dst_read) {
  auto native_operation = caps.supports_native_advanced_blend
                              ? ToNativeBlendOp(blend_mode)
                              : std::nullopt;
  if (native_operation.has_value() &&
      caps.supports_native_advanced_blend_coherent) {
    return MakeFixedPlan(
        blend_mode, HWBlendOutput::kSourceTimesCoverage, GPUBlendFactor::kOne,
        GPUBlendFactor::kOneMinusSrcAlpha, native_operation.value());
  }

  if (caps.supports_framebuffer_fetch) {
    return MakeProgrammablePlan(blend_mode, caps,
                                supports_texture_copy_dst_read);
  }

  if (native_operation.has_value()) {
    return MakeFixedPlan(
        blend_mode, HWBlendOutput::kSourceTimesCoverage, GPUBlendFactor::kOne,
        GPUBlendFactor::kOneMinusSrcAlpha, native_operation.value());
  }

  return MakeProgrammablePlan(blend_mode, caps, supports_texture_copy_dst_read);
}

std::optional<HWBlendPlan> ResolveOpaqueSourceCoveragePlan(
    BlendMode blend_mode) {
  // With source alpha fixed at one, these coverage equations no longer need a
  // secondary output or a destination read.
  switch (blend_mode) {
    case BlendMode::kSrc:
      return MakeFixedPlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                           GPUBlendFactor::kOne,
                           GPUBlendFactor::kOneMinusSrcAlpha);
    case BlendMode::kSrcIn:
      return MakeFixedPlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                           GPUBlendFactor::kDstAlpha,
                           GPUBlendFactor::kOneMinusSrcAlpha);
    case BlendMode::kSrcOut:
      return MakeFixedPlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                           GPUBlendFactor::kOneMinusDstAlpha,
                           GPUBlendFactor::kOneMinusSrcAlpha);
    case BlendMode::kDstATop:
      return MakeFixedPlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                           GPUBlendFactor::kOneMinusDstAlpha,
                           GPUBlendFactor::kOne);
    case BlendMode::kDstIn:
      return MakeFixedPlan(blend_mode, HWBlendOutput::kNone,
                           GPUBlendFactor::kZero, GPUBlendFactor::kOne);
    case BlendMode::kDstOut:
      return MakeFixedPlan(blend_mode, HWBlendOutput::kCoverage,
                           GPUBlendFactor::kDst, GPUBlendFactor::kOne,
                           GPUBlendOperation::kReverseSubtract);
    default:
      return std::nullopt;
  }
}

std::optional<HWBlendPlan> ResolveCoveragePlan(
    BlendMode blend_mode, bool source_is_opaque, const GPUCaps& caps,
    bool supports_texture_copy_dst_read) {
  if (source_is_opaque) {
    auto opaque_plan = ResolveOpaqueSourceCoveragePlan(blend_mode);
    if (opaque_plan.has_value()) {
      return opaque_plan;
    }
  }

  switch (blend_mode) {
    case BlendMode::kClear:
      return MakeFixedPlan(blend_mode, HWBlendOutput::kCoverage,
                           GPUBlendFactor::kDst, GPUBlendFactor::kOne,
                           GPUBlendOperation::kReverseSubtract);
    case BlendMode::kSrc:
      if (caps.supports_dual_source_blending) {
        return MakeDualSourcePlan(blend_mode, HWBlendOutput::kCoverage,
                                  GPUBlendFactor::kOne,
                                  GPUBlendFactor::kOneMinusSrc1Alpha);
      }
      return MakeProgrammablePlan(blend_mode, caps,
                                  supports_texture_copy_dst_read);
    case BlendMode::kSrcIn:
      if (caps.supports_dual_source_blending) {
        return MakeDualSourcePlan(blend_mode, HWBlendOutput::kCoverage,
                                  GPUBlendFactor::kDstAlpha,
                                  GPUBlendFactor::kOneMinusSrc1Alpha);
      }
      return MakeProgrammablePlan(blend_mode, caps,
                                  supports_texture_copy_dst_read);
    case BlendMode::kSrcOut:
      if (caps.supports_dual_source_blending) {
        return MakeDualSourcePlan(blend_mode, HWBlendOutput::kCoverage,
                                  GPUBlendFactor::kOneMinusDstAlpha,
                                  GPUBlendFactor::kOneMinusSrc1Alpha);
      }
      return MakeProgrammablePlan(blend_mode, caps,
                                  supports_texture_copy_dst_read);
    case BlendMode::kDstATop:
      if (caps.supports_dual_source_blending) {
        return MakeDualSourcePlan(
            blend_mode, HWBlendOutput::kOneMinusSourceAlphaTimesCoverage,
            GPUBlendFactor::kOneMinusDstAlpha, GPUBlendFactor::kOneMinusSrc1);
      }
      return MakeProgrammablePlan(blend_mode, caps,
                                  supports_texture_copy_dst_read);
    case BlendMode::kDst:
      return MakeFixedPlan(blend_mode, HWBlendOutput::kNone,
                           GPUBlendFactor::kZero, GPUBlendFactor::kOne);
    case BlendMode::kSrcOver:
      return MakeFixedPlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                           GPUBlendFactor::kOne,
                           GPUBlendFactor::kOneMinusSrcAlpha);
    case BlendMode::kDstOver:
      return MakeFixedPlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                           GPUBlendFactor::kOneMinusDstAlpha,
                           GPUBlendFactor::kOne);
    case BlendMode::kDstIn:
      return MakeFixedPlan(blend_mode,
                           HWBlendOutput::kOneMinusSourceAlphaTimesCoverage,
                           GPUBlendFactor::kDst, GPUBlendFactor::kOne,
                           GPUBlendOperation::kReverseSubtract);
    case BlendMode::kDstOut:
      return MakeFixedPlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                           GPUBlendFactor::kZero,
                           GPUBlendFactor::kOneMinusSrcAlpha);
    case BlendMode::kSrcATop:
      return MakeFixedPlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                           GPUBlendFactor::kDstAlpha,
                           GPUBlendFactor::kOneMinusSrcAlpha);
    case BlendMode::kXor:
      return MakeFixedPlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                           GPUBlendFactor::kOneMinusDstAlpha,
                           GPUBlendFactor::kOneMinusSrcAlpha);
    case BlendMode::kPlus:
      return MakeFixedPlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                           GPUBlendFactor::kOne, GPUBlendFactor::kOne);
    case BlendMode::kModulate:
      return MakeFixedPlan(blend_mode,
                           HWBlendOutput::kOneMinusSourceTimesCoverage,
                           GPUBlendFactor::kDst, GPUBlendFactor::kOne,
                           GPUBlendOperation::kReverseSubtract);
    case BlendMode::kScreen:
      return MakeFixedPlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                           GPUBlendFactor::kOne, GPUBlendFactor::kOneMinusSrc);
    default:
      return ResolveAdvancedPlan(blend_mode, caps,
                                 supports_texture_copy_dst_read);
  }
}

std::optional<HWBlendPlan> ResolveRegularPlan(
    BlendMode blend_mode, const GPUCaps& caps,
    bool supports_texture_copy_dst_read) {
  if (IsAdvancedBlendMode(blend_mode)) {
    return ResolveAdvancedPlan(blend_mode, caps,
                               supports_texture_copy_dst_read);
  }

  switch (blend_mode) {
    case BlendMode::kClear:
      return MakeFixedPlan(blend_mode, HWBlendOutput::kNone,
                           GPUBlendFactor::kZero, GPUBlendFactor::kZero);
    case BlendMode::kSrc:
      return MakeFixedPlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                           GPUBlendFactor::kOne, GPUBlendFactor::kZero);
    case BlendMode::kDst:
      return MakeFixedPlan(blend_mode, HWBlendOutput::kNone,
                           GPUBlendFactor::kZero, GPUBlendFactor::kOne);
    case BlendMode::kSrcOver:
      return MakeFixedPlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                           GPUBlendFactor::kOne,
                           GPUBlendFactor::kOneMinusSrcAlpha);
    case BlendMode::kDstOver:
      return MakeFixedPlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                           GPUBlendFactor::kOneMinusDstAlpha,
                           GPUBlendFactor::kOne);
    case BlendMode::kSrcIn:
      return MakeFixedPlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                           GPUBlendFactor::kDstAlpha, GPUBlendFactor::kZero);
    case BlendMode::kDstIn:
      return MakeFixedPlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                           GPUBlendFactor::kZero, GPUBlendFactor::kSrcAlpha);
    case BlendMode::kSrcOut:
      return MakeFixedPlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                           GPUBlendFactor::kOneMinusDstAlpha,
                           GPUBlendFactor::kZero);
    case BlendMode::kDstOut:
      return MakeFixedPlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                           GPUBlendFactor::kZero,
                           GPUBlendFactor::kOneMinusSrcAlpha);
    case BlendMode::kSrcATop:
      return MakeFixedPlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                           GPUBlendFactor::kDstAlpha,
                           GPUBlendFactor::kOneMinusSrcAlpha);
    case BlendMode::kDstATop:
      return MakeFixedPlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                           GPUBlendFactor::kOneMinusDstAlpha,
                           GPUBlendFactor::kSrcAlpha);
    case BlendMode::kXor:
      return MakeFixedPlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                           GPUBlendFactor::kOneMinusDstAlpha,
                           GPUBlendFactor::kOneMinusSrcAlpha);
    case BlendMode::kPlus:
      return MakeFixedPlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                           GPUBlendFactor::kOne, GPUBlendFactor::kOne);
    case BlendMode::kModulate:
      return MakeFixedPlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                           GPUBlendFactor::kZero, GPUBlendFactor::kSrc);
    case BlendMode::kScreen:
      return MakeFixedPlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                           GPUBlendFactor::kOne, GPUBlendFactor::kOneMinusSrc);
    default:
      return std::nullopt;
  }
}

}  // namespace

std::optional<HWBlendPlan> ResolveHWBlendPlan(
    BlendMode blend_mode, bool has_fragment_mask, bool source_is_opaque,
    const GPUCaps& caps, bool supports_texture_copy_dst_read) {
  if (has_fragment_mask) {
    return ResolveCoveragePlan(blend_mode, source_is_opaque, caps,
                               supports_texture_copy_dst_read);
  }
  return ResolveRegularPlan(blend_mode, caps, supports_texture_copy_dst_read);
}

}  // namespace skity
