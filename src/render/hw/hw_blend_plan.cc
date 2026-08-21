// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/hw_blend_plan.hpp"

#include "src/graphic/blend_mode_priv.hpp"
#include "src/render/hw/native_blend.hpp"

namespace skity {
namespace {

HWBlendFormula ResolveLegacyFormula(BlendMode blend_mode) {
  switch (blend_mode) {
    case BlendMode::kClear:
      return {GPUBlendFactor::kZero, GPUBlendFactor::kZero};
    case BlendMode::kSrc:
      return {GPUBlendFactor::kOne, GPUBlendFactor::kZero};
    case BlendMode::kDst:
      return {GPUBlendFactor::kZero, GPUBlendFactor::kOne};
    case BlendMode::kSrcOver:
      return {GPUBlendFactor::kOne, GPUBlendFactor::kOneMinusSrcAlpha};
    case BlendMode::kDstOver:
      return {GPUBlendFactor::kOneMinusDstAlpha, GPUBlendFactor::kOne};
    case BlendMode::kSrcIn:
      return {GPUBlendFactor::kDstAlpha, GPUBlendFactor::kZero};
    case BlendMode::kDstIn:
      return {GPUBlendFactor::kZero, GPUBlendFactor::kSrcAlpha};
    case BlendMode::kSrcOut:
      return {GPUBlendFactor::kOneMinusDstAlpha, GPUBlendFactor::kZero};
    case BlendMode::kDstOut:
      return {GPUBlendFactor::kZero, GPUBlendFactor::kOneMinusSrcAlpha};
    case BlendMode::kSrcATop:
      return {GPUBlendFactor::kDstAlpha, GPUBlendFactor::kOneMinusSrcAlpha};
    case BlendMode::kDstATop:
      return {GPUBlendFactor::kOneMinusDstAlpha, GPUBlendFactor::kSrcAlpha};
    case BlendMode::kXor:
      return {GPUBlendFactor::kOneMinusDstAlpha,
              GPUBlendFactor::kOneMinusSrcAlpha};
    case BlendMode::kPlus:
      return {GPUBlendFactor::kOne, GPUBlendFactor::kOne};
    default:
      // Preserve the previous fallback for Modulate, Screen, and advanced
      // modes. Correct coverage-aware routes are added in later commits.
      return {GPUBlendFactor::kOne, GPUBlendFactor::kZero};
  }
}

bool LegacyFormulaHandlesFragmentMask(BlendMode blend_mode) {
  // These equations remain coverage-correct after the shader multiplies the
  // source by the fragment mask, so keep their existing fixed-function path.
  switch (blend_mode) {
    case BlendMode::kDst:
    case BlendMode::kSrcOver:
    case BlendMode::kDstOver:
    case BlendMode::kDstOut:
    case BlendMode::kSrcATop:
    case BlendMode::kXor:
      return true;
    default:
      return false;
  }
}

}  // namespace

bool HWBlendPlan::SupportsFragmentMask() const {
  return dst_read_strategy == DstReadStrategy::kFramebufferFetch ||
         dst_read_strategy == DstReadStrategy::kTextureCopy ||
         LegacyFormulaHandlesFragmentMask(blend_mode);
}

HWBlendPlan ResolveHWBlendPlan(BlendMode blend_mode, bool has_fragment_mask,
                               const GPUCaps& caps,
                               bool supports_texture_copy_dst_read) {
  auto strategy =
      ResolveDstReadStrategy(blend_mode, caps, supports_texture_copy_dst_read);
  if (has_fragment_mask && !LegacyFormulaHandlesFragmentMask(blend_mode)) {
    if (caps.supports_framebuffer_fetch) {
      strategy = DstReadStrategy::kFramebufferFetch;
    } else if (supports_texture_copy_dst_read) {
      strategy = DstReadStrategy::kTextureCopy;
    }
  }

  return {blend_mode, strategy};
}

HWBlendFormula ResolveHWBlendFormula(const HWBlendPlan& blend_plan,
                                     const GPUCaps& caps,
                                     bool shader_side_blending) {
  if (shader_side_blending) {
    return {GPUBlendFactor::kOne, GPUBlendFactor::kZero};
  }

  if (caps.supports_native_advanced_blend &&
      IsAdvancedBlendMode(blend_plan.blend_mode)) {
    if (auto operation = ToNativeBlendOp(blend_plan.blend_mode)) {
      return {GPUBlendFactor::kOne, GPUBlendFactor::kOneMinusSrcAlpha,
              *operation};
    }
  }

  return ResolveLegacyFormula(blend_plan.blend_mode);
}

}  // namespace skity
