// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/hw_blend_plan.hpp"

#include "src/logging.hpp"
#include "src/render/hw/native_blend.hpp"

namespace skity {
namespace {

HWBlendPlan MakePlan(BlendMode blend_mode, HWBlendOutput primary_output,
                     HWBlendOutput secondary_output, GPUBlendFactor src_factor,
                     GPUBlendFactor dst_factor, GPUBlendOperation operation,
                     DstReadStrategy dst_read_strategy) {
  return {blend_mode,
          {primary_output, secondary_output, src_factor, dst_factor, operation},
          dst_read_strategy};
}

std::optional<HWBlendPlan> MakeProgrammablePlan(
    BlendMode blend_mode, const GPUCaps& caps,
    bool supports_texture_copy_dst_read) {
  if (caps.supports_framebuffer_fetch) {
    return MakePlan(blend_mode, HWBlendOutput::kSource, HWBlendOutput::kNone,
                    GPUBlendFactor::kOne, GPUBlendFactor::kZero,
                    GPUBlendOperation::kAdd,
                    DstReadStrategy::kFramebufferFetch);
  }
  if (supports_texture_copy_dst_read) {
    return MakePlan(blend_mode, HWBlendOutput::kSource, HWBlendOutput::kNone,
                    GPUBlendFactor::kOne, GPUBlendFactor::kZero,
                    GPUBlendOperation::kAdd, DstReadStrategy::kTextureCopy);
  }
  return std::nullopt;
}

HWBlendPlan MakeDualSourcePlan(BlendMode blend_mode, GPUBlendFactor src_factor,
                               HWBlendOutput secondary_output) {
  return MakePlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                  secondary_output, src_factor,
                  GPUBlendFactor::kOneMinusSrc1Alpha, GPUBlendOperation::kAdd,
                  DstReadStrategy::kNonRequired);
}

std::optional<HWBlendPlan> ResolveOpaqueCoveragePlan(BlendMode blend_mode) {
  switch (blend_mode) {
    case BlendMode::kSrc:
      return MakePlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                      HWBlendOutput::kNone, GPUBlendFactor::kOne,
                      GPUBlendFactor::kOneMinusSrcAlpha,
                      GPUBlendOperation::kAdd, DstReadStrategy::kNonRequired);
    case BlendMode::kSrcIn:
      return MakePlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                      HWBlendOutput::kNone, GPUBlendFactor::kDstAlpha,
                      GPUBlendFactor::kOneMinusSrcAlpha,
                      GPUBlendOperation::kAdd, DstReadStrategy::kNonRequired);
    case BlendMode::kSrcOut:
      return MakePlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                      HWBlendOutput::kNone, GPUBlendFactor::kOneMinusDstAlpha,
                      GPUBlendFactor::kOneMinusSrcAlpha,
                      GPUBlendOperation::kAdd, DstReadStrategy::kNonRequired);
    case BlendMode::kDstATop:
      return MakePlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                      HWBlendOutput::kNone, GPUBlendFactor::kOneMinusDstAlpha,
                      GPUBlendFactor::kOne, GPUBlendOperation::kAdd,
                      DstReadStrategy::kNonRequired);
    case BlendMode::kDstIn:
      return MakePlan(blend_mode, HWBlendOutput::kNone, HWBlendOutput::kNone,
                      GPUBlendFactor::kZero, GPUBlendFactor::kOne,
                      GPUBlendOperation::kAdd, DstReadStrategy::kNonRequired);
    case BlendMode::kDstOut:
      return MakePlan(blend_mode, HWBlendOutput::kCoverage,
                      HWBlendOutput::kNone, GPUBlendFactor::kDst,
                      GPUBlendFactor::kOne, GPUBlendOperation::kReverseSubtract,
                      DstReadStrategy::kNonRequired);
    default:
      return std::nullopt;
  }
}

std::optional<HWBlendPlan> ResolveCoveragePlan(
    BlendMode blend_mode, bool source_is_opaque, const GPUCaps& caps,
    bool supports_texture_copy_dst_read) {
  if (source_is_opaque) {
    if (auto plan = ResolveOpaqueCoveragePlan(blend_mode)) {
      return plan;
    }
  }

  switch (blend_mode) {
    case BlendMode::kClear:
      return MakePlan(blend_mode, HWBlendOutput::kCoverage,
                      HWBlendOutput::kNone, GPUBlendFactor::kDst,
                      GPUBlendFactor::kOne, GPUBlendOperation::kReverseSubtract,
                      DstReadStrategy::kNonRequired);
    case BlendMode::kSrc:
      if (caps.supports_dual_source_blending) {
        return MakeDualSourcePlan(blend_mode, GPUBlendFactor::kOne,
                                  HWBlendOutput::kCoverage);
      }
      break;
    case BlendMode::kDst:
      return MakePlan(blend_mode, HWBlendOutput::kNone, HWBlendOutput::kNone,
                      GPUBlendFactor::kZero, GPUBlendFactor::kOne,
                      GPUBlendOperation::kAdd, DstReadStrategy::kNonRequired);
    case BlendMode::kSrcOver:
      return MakePlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                      HWBlendOutput::kNone, GPUBlendFactor::kOne,
                      GPUBlendFactor::kOneMinusSrcAlpha,
                      GPUBlendOperation::kAdd, DstReadStrategy::kNonRequired);
    case BlendMode::kDstOver:
      return MakePlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                      HWBlendOutput::kNone, GPUBlendFactor::kOneMinusDstAlpha,
                      GPUBlendFactor::kOne, GPUBlendOperation::kAdd,
                      DstReadStrategy::kNonRequired);
    case BlendMode::kSrcIn:
      if (caps.supports_dual_source_blending) {
        return MakeDualSourcePlan(blend_mode, GPUBlendFactor::kDstAlpha,
                                  HWBlendOutput::kCoverage);
      }
      break;
    case BlendMode::kDstIn:
      return MakePlan(
          blend_mode, HWBlendOutput::kOneMinusSourceAlphaTimesCoverage,
          HWBlendOutput::kNone, GPUBlendFactor::kDst, GPUBlendFactor::kOne,
          GPUBlendOperation::kReverseSubtract, DstReadStrategy::kNonRequired);
    case BlendMode::kSrcOut:
      if (caps.supports_dual_source_blending) {
        return MakeDualSourcePlan(blend_mode, GPUBlendFactor::kOneMinusDstAlpha,
                                  HWBlendOutput::kCoverage);
      }
      break;
    case BlendMode::kDstOut:
      return MakePlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                      HWBlendOutput::kNone, GPUBlendFactor::kZero,
                      GPUBlendFactor::kOneMinusSrcAlpha,
                      GPUBlendOperation::kAdd, DstReadStrategy::kNonRequired);
    case BlendMode::kSrcATop:
      return MakePlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                      HWBlendOutput::kNone, GPUBlendFactor::kDstAlpha,
                      GPUBlendFactor::kOneMinusSrcAlpha,
                      GPUBlendOperation::kAdd, DstReadStrategy::kNonRequired);
    case BlendMode::kDstATop:
      if (caps.supports_dual_source_blending) {
        return MakeDualSourcePlan(
            blend_mode, GPUBlendFactor::kOneMinusDstAlpha,
            HWBlendOutput::kOneMinusSourceAlphaTimesCoverage);
      }
      break;
    case BlendMode::kXor:
      return MakePlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                      HWBlendOutput::kNone, GPUBlendFactor::kOneMinusDstAlpha,
                      GPUBlendFactor::kOneMinusSrcAlpha,
                      GPUBlendOperation::kAdd, DstReadStrategy::kNonRequired);
    case BlendMode::kModulate:
      return MakePlan(blend_mode, HWBlendOutput::kOneMinusSourceTimesCoverage,
                      HWBlendOutput::kNone, GPUBlendFactor::kDst,
                      GPUBlendFactor::kOne, GPUBlendOperation::kReverseSubtract,
                      DstReadStrategy::kNonRequired);
    case BlendMode::kScreen:
      return MakePlan(blend_mode, HWBlendOutput::kSourceTimesCoverage,
                      HWBlendOutput::kNone, GPUBlendFactor::kOne,
                      GPUBlendFactor::kOneMinusSrc, GPUBlendOperation::kAdd,
                      DstReadStrategy::kNonRequired);
    case BlendMode::kPlus:
    default:
      break;
  }

  return MakeProgrammablePlan(blend_mode, caps, supports_texture_copy_dst_read);
}

HWBlendPlan ResolveRegularPlan(BlendMode blend_mode, const GPUCaps& caps,
                               bool supports_texture_copy_dst_read) {
  switch (blend_mode) {
    case BlendMode::kClear:
      return MakePlan(blend_mode, HWBlendOutput::kNone, HWBlendOutput::kNone,
                      GPUBlendFactor::kZero, GPUBlendFactor::kZero,
                      GPUBlendOperation::kAdd, DstReadStrategy::kNonRequired);
    case BlendMode::kSrc:
      return MakePlan(blend_mode, HWBlendOutput::kSource, HWBlendOutput::kNone,
                      GPUBlendFactor::kOne, GPUBlendFactor::kZero,
                      GPUBlendOperation::kAdd, DstReadStrategy::kNonRequired);
    case BlendMode::kDst:
      return MakePlan(blend_mode, HWBlendOutput::kNone, HWBlendOutput::kNone,
                      GPUBlendFactor::kZero, GPUBlendFactor::kOne,
                      GPUBlendOperation::kAdd, DstReadStrategy::kNonRequired);
    case BlendMode::kSrcOver:
      return MakePlan(blend_mode, HWBlendOutput::kSource, HWBlendOutput::kNone,
                      GPUBlendFactor::kOne, GPUBlendFactor::kOneMinusSrcAlpha,
                      GPUBlendOperation::kAdd, DstReadStrategy::kNonRequired);
    case BlendMode::kDstOver:
      return MakePlan(blend_mode, HWBlendOutput::kSource, HWBlendOutput::kNone,
                      GPUBlendFactor::kOneMinusDstAlpha, GPUBlendFactor::kOne,
                      GPUBlendOperation::kAdd, DstReadStrategy::kNonRequired);
    case BlendMode::kSrcIn:
      return MakePlan(blend_mode, HWBlendOutput::kSource, HWBlendOutput::kNone,
                      GPUBlendFactor::kDstAlpha, GPUBlendFactor::kZero,
                      GPUBlendOperation::kAdd, DstReadStrategy::kNonRequired);
    case BlendMode::kDstIn:
      return MakePlan(blend_mode, HWBlendOutput::kSource, HWBlendOutput::kNone,
                      GPUBlendFactor::kZero, GPUBlendFactor::kSrcAlpha,
                      GPUBlendOperation::kAdd, DstReadStrategy::kNonRequired);
    case BlendMode::kSrcOut:
      return MakePlan(blend_mode, HWBlendOutput::kSource, HWBlendOutput::kNone,
                      GPUBlendFactor::kOneMinusDstAlpha, GPUBlendFactor::kZero,
                      GPUBlendOperation::kAdd, DstReadStrategy::kNonRequired);
    case BlendMode::kDstOut:
      return MakePlan(blend_mode, HWBlendOutput::kSource, HWBlendOutput::kNone,
                      GPUBlendFactor::kZero, GPUBlendFactor::kOneMinusSrcAlpha,
                      GPUBlendOperation::kAdd, DstReadStrategy::kNonRequired);
    case BlendMode::kSrcATop:
      return MakePlan(blend_mode, HWBlendOutput::kSource, HWBlendOutput::kNone,
                      GPUBlendFactor::kDstAlpha,
                      GPUBlendFactor::kOneMinusSrcAlpha,
                      GPUBlendOperation::kAdd, DstReadStrategy::kNonRequired);
    case BlendMode::kDstATop:
      return MakePlan(blend_mode, HWBlendOutput::kSource, HWBlendOutput::kNone,
                      GPUBlendFactor::kOneMinusDstAlpha,
                      GPUBlendFactor::kSrcAlpha, GPUBlendOperation::kAdd,
                      DstReadStrategy::kNonRequired);
    case BlendMode::kXor:
      return MakePlan(blend_mode, HWBlendOutput::kSource, HWBlendOutput::kNone,
                      GPUBlendFactor::kOneMinusDstAlpha,
                      GPUBlendFactor::kOneMinusSrcAlpha,
                      GPUBlendOperation::kAdd, DstReadStrategy::kNonRequired);
    case BlendMode::kPlus:
      return MakePlan(blend_mode, HWBlendOutput::kSource, HWBlendOutput::kNone,
                      GPUBlendFactor::kOne, GPUBlendFactor::kOne,
                      GPUBlendOperation::kAdd, DstReadStrategy::kNonRequired);
    case BlendMode::kModulate:
      return MakePlan(blend_mode, HWBlendOutput::kSource, HWBlendOutput::kNone,
                      GPUBlendFactor::kZero, GPUBlendFactor::kSrc,
                      GPUBlendOperation::kAdd, DstReadStrategy::kNonRequired);
    case BlendMode::kScreen:
      return MakePlan(blend_mode, HWBlendOutput::kSource, HWBlendOutput::kNone,
                      GPUBlendFactor::kOne, GPUBlendFactor::kOneMinusSrc,
                      GPUBlendOperation::kAdd, DstReadStrategy::kNonRequired);
    default:
      break;
  }

  auto strategy =
      ResolveDstReadStrategy(blend_mode, caps, supports_texture_copy_dst_read);
  if (strategy == DstReadStrategy::kNativeBlend) {
    auto operation = ToNativeBlendOp(blend_mode);
    DEBUG_CHECK(operation.has_value());
    return MakePlan(blend_mode, HWBlendOutput::kSource, HWBlendOutput::kNone,
                    GPUBlendFactor::kOne, GPUBlendFactor::kOneMinusSrcAlpha,
                    *operation, strategy);
  }
  if (strategy == DstReadStrategy::kFramebufferFetch ||
      strategy == DstReadStrategy::kTextureCopy) {
    return MakePlan(blend_mode, HWBlendOutput::kSource, HWBlendOutput::kNone,
                    GPUBlendFactor::kOne, GPUBlendFactor::kZero,
                    GPUBlendOperation::kAdd, strategy);
  }

  // Preserve the legacy source-replacement fallback when the backend cannot
  // read destination color for an advanced blend mode.
  return MakePlan(blend_mode, HWBlendOutput::kSource, HWBlendOutput::kNone,
                  GPUBlendFactor::kOne, GPUBlendFactor::kZero,
                  GPUBlendOperation::kAdd, DstReadStrategy::kNonRequired);
}

}  // namespace

HWBlendPlan ResolveCoefficientBlendPlan(BlendMode blend_mode) {
  DEBUG_CHECK(blend_mode <= BlendMode::kLastCoeffMode);
  return ResolveRegularPlan(blend_mode, GPUCaps{},
                            /*supports_texture_copy_dst_read=*/false);
}

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
