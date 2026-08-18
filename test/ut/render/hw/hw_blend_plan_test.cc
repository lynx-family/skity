// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/hw_blend_plan.hpp"

#include <gtest/gtest.h>

namespace {

using skity::BlendMode;
using skity::GPUBlendFactor;
using skity::GPUBlendOperation;
using skity::GPUCaps;
using skity::HWBlendOutput;
using skity::HWBlendStrategy;

TEST(HWBlendPlan, CoverageUsesFixedFunctionForSupportedModes) {
  constexpr BlendMode kModes[] = {
      BlendMode::kClear,    BlendMode::kDst,    BlendMode::kSrcOver,
      BlendMode::kDstOver,  BlendMode::kDstIn,  BlendMode::kDstOut,
      BlendMode::kSrcATop,  BlendMode::kXor,    BlendMode::kPlus,
      BlendMode::kModulate, BlendMode::kScreen,
  };

  for (auto mode : kModes) {
    auto plan = skity::ResolveFixedFunctionBlendPlan(
        mode, /*has_fragment_mask=*/true, /*source_is_opaque=*/false);
    ASSERT_TRUE(plan.has_value()) << static_cast<int>(mode);
    EXPECT_EQ(plan->blend_mode, mode);
  }
}

TEST(HWBlendPlan, CoverageDefersModesNeedingSourceAlpha) {
  constexpr BlendMode kModes[] = {BlendMode::kSrc, BlendMode::kSrcIn,
                                  BlendMode::kSrcOut, BlendMode::kDstATop};
  for (auto mode : kModes) {
    EXPECT_FALSE(skity::ResolveFixedFunctionBlendPlan(
        mode, /*has_fragment_mask=*/true, /*source_is_opaque=*/false));
  }
}

TEST(HWBlendPlan, OpaqueCoverageRemovesSourceAlphaDependency) {
  auto src = skity::ResolveFixedFunctionBlendPlan(
      BlendMode::kSrc, /*has_fragment_mask=*/true, /*source_is_opaque=*/true);
  ASSERT_TRUE(src.has_value());
  EXPECT_EQ(src->formula.primary_output, HWBlendOutput::kSourceTimesCoverage);
  EXPECT_EQ(src->formula.src_factor, GPUBlendFactor::kOne);
  EXPECT_EQ(src->formula.dst_factor, GPUBlendFactor::kOneMinusSrcAlpha);

  auto dst_out = skity::ResolveFixedFunctionBlendPlan(
      BlendMode::kDstOut, /*has_fragment_mask=*/true,
      /*source_is_opaque=*/true);
  ASSERT_TRUE(dst_out.has_value());
  EXPECT_EQ(dst_out->formula.primary_output, HWBlendOutput::kCoverage);
  EXPECT_EQ(dst_out->formula.src_factor, GPUBlendFactor::kDst);
  EXPECT_EQ(dst_out->formula.dst_factor, GPUBlendFactor::kOne);
  EXPECT_EQ(dst_out->formula.operation, GPUBlendOperation::kReverseSubtract);
}

TEST(HWBlendPlan, UnmaskedModesKeepOrdinaryFixedFunctionEquations) {
  auto src_over = skity::ResolveFixedFunctionBlendPlan(
      BlendMode::kSrcOver, /*has_fragment_mask=*/false,
      /*source_is_opaque=*/false);
  ASSERT_TRUE(src_over.has_value());
  EXPECT_EQ(src_over->formula.primary_output, HWBlendOutput::kSource);
  EXPECT_EQ(src_over->formula.src_factor, GPUBlendFactor::kOne);
  EXPECT_EQ(src_over->formula.dst_factor, GPUBlendFactor::kOneMinusSrcAlpha);

  auto modulate = skity::ResolveFixedFunctionBlendPlan(
      BlendMode::kModulate, /*has_fragment_mask=*/false,
      /*source_is_opaque=*/false);
  ASSERT_TRUE(modulate.has_value());
  EXPECT_EQ(modulate->formula.primary_output, HWBlendOutput::kSource);
  EXPECT_EQ(modulate->formula.src_factor, GPUBlendFactor::kZero);
  EXPECT_EQ(modulate->formula.dst_factor, GPUBlendFactor::kSrc);

  auto screen = skity::ResolveFixedFunctionBlendPlan(
      BlendMode::kScreen, /*has_fragment_mask=*/false,
      /*source_is_opaque=*/false);
  ASSERT_TRUE(screen.has_value());
  EXPECT_EQ(screen->formula.primary_output, HWBlendOutput::kSource);
  EXPECT_EQ(screen->formula.src_factor, GPUBlendFactor::kOne);
  EXPECT_EQ(screen->formula.dst_factor, GPUBlendFactor::kOneMinusSrc);
}

TEST(HWBlendPlan, CompletePlanKeepsOrdinaryFixedFunctionRoute) {
  GPUCaps caps = {};
  caps.supports_framebuffer_fetch = true;

  auto plan = skity::ResolveHWBlendPlan(
      BlendMode::kModulate, /*has_fragment_mask=*/false,
      /*use_coverage_aware_blending=*/false,
      /*source_is_opaque=*/false, caps,
      /*supports_texture_copy_dst_read=*/true);

  EXPECT_EQ(plan.strategy, HWBlendStrategy::kFixedFunction);
  EXPECT_EQ(plan.formula.primary_output, HWBlendOutput::kSource);
  EXPECT_EQ(plan.formula.src_factor, GPUBlendFactor::kZero);
  EXPECT_EQ(plan.formula.dst_factor, GPUBlendFactor::kSrc);
  EXPECT_EQ(plan.dst_read_strategy, skity::DstReadStrategy::kNonRequired);

  auto masked =
      skity::ResolveHWBlendPlan(BlendMode::kSrcOver, /*has_fragment_mask=*/true,
                                /*use_coverage_aware_blending=*/true,
                                /*source_is_opaque=*/false, caps,
                                /*supports_texture_copy_dst_read=*/true);
  EXPECT_EQ(masked.strategy, HWBlendStrategy::kFixedFunction);
  EXPECT_EQ(masked.formula.primary_output, HWBlendOutput::kSourceTimesCoverage);
  EXPECT_EQ(masked.dst_read_strategy, skity::DstReadStrategy::kNonRequired);
}

TEST(HWBlendPlan, CompletePlanOnlyUsesCoverageAwareFormulaWhenRequested) {
  GPUCaps caps = {};

  auto legacy =
      skity::ResolveHWBlendPlan(BlendMode::kSrc, /*has_fragment_mask=*/true,
                                /*use_coverage_aware_blending=*/false,
                                /*source_is_opaque=*/true, caps,
                                /*supports_texture_copy_dst_read=*/true);
  EXPECT_EQ(legacy.formula.primary_output, HWBlendOutput::kSourceTimesCoverage);
  EXPECT_EQ(legacy.formula.src_factor, GPUBlendFactor::kOne);
  EXPECT_EQ(legacy.formula.dst_factor, GPUBlendFactor::kZero);

  auto coverage_aware =
      skity::ResolveHWBlendPlan(BlendMode::kSrc, /*has_fragment_mask=*/true,
                                /*use_coverage_aware_blending=*/true,
                                /*source_is_opaque=*/true, caps,
                                /*supports_texture_copy_dst_read=*/true);
  EXPECT_EQ(coverage_aware.formula.primary_output,
            HWBlendOutput::kSourceTimesCoverage);
  EXPECT_EQ(coverage_aware.formula.src_factor, GPUBlendFactor::kOne);
  EXPECT_EQ(coverage_aware.formula.dst_factor,
            GPUBlendFactor::kOneMinusSrcAlpha);
}

TEST(HWBlendPlan, CompletePlanPreservesMaskedAdvancedRoute) {
  GPUCaps caps = {};
  caps.supports_framebuffer_fetch = true;

  auto unmasked = skity::ResolveHWBlendPlan(
      BlendMode::kOverlay, /*has_fragment_mask=*/false,
      /*use_coverage_aware_blending=*/false,
      /*source_is_opaque=*/false, caps,
      /*supports_texture_copy_dst_read=*/true);
  EXPECT_EQ(unmasked.strategy, HWBlendStrategy::kProgrammable);
  EXPECT_EQ(unmasked.formula.primary_output, HWBlendOutput::kSource);
  EXPECT_EQ(unmasked.formula.src_factor, GPUBlendFactor::kOne);
  EXPECT_EQ(unmasked.formula.dst_factor, GPUBlendFactor::kZero);
  EXPECT_EQ(unmasked.dst_read_strategy,
            skity::DstReadStrategy::kFramebufferFetch);

  auto masked =
      skity::ResolveHWBlendPlan(BlendMode::kOverlay, /*has_fragment_mask=*/true,
                                /*use_coverage_aware_blending=*/false,
                                /*source_is_opaque=*/false, caps,
                                /*supports_texture_copy_dst_read=*/true);
  EXPECT_EQ(masked.strategy, HWBlendStrategy::kProgrammable);
  EXPECT_EQ(masked.formula.primary_output, HWBlendOutput::kSourceTimesCoverage);
  EXPECT_EQ(masked.dst_read_strategy,
            skity::DstReadStrategy::kFramebufferFetch);
}

TEST(HWBlendPlan, CompletePlanCarriesNativeAndTextureCopyState) {
  GPUCaps native_caps = {};
  native_caps.supports_native_advanced_blend = true;
  native_caps.supports_native_advanced_blend_coherent = true;

  auto native = skity::ResolveHWBlendPlan(
      BlendMode::kOverlay, /*has_fragment_mask=*/false,
      /*use_coverage_aware_blending=*/false,
      /*source_is_opaque=*/false, native_caps,
      /*supports_texture_copy_dst_read=*/true);
  EXPECT_EQ(native.strategy, HWBlendStrategy::kFixedFunction);
  EXPECT_EQ(native.formula.operation, GPUBlendOperation::kOverlay);
  EXPECT_EQ(native.dst_read_strategy, skity::DstReadStrategy::kNativeBlend);

  GPUCaps texture_copy_caps = {};
  auto texture_copy = skity::ResolveHWBlendPlan(
      BlendMode::kOverlay, /*has_fragment_mask=*/false,
      /*use_coverage_aware_blending=*/false,
      /*source_is_opaque=*/false, texture_copy_caps,
      /*supports_texture_copy_dst_read=*/true);
  EXPECT_EQ(texture_copy.strategy, HWBlendStrategy::kProgrammable);
  EXPECT_EQ(texture_copy.dst_read_strategy,
            skity::DstReadStrategy::kTextureCopy);
}

TEST(HWBlendPlan, CompletePlanPreservesLegacyFallback) {
  GPUCaps caps = {};

  auto advanced = skity::ResolveHWBlendPlan(
      BlendMode::kOverlay, /*has_fragment_mask=*/false,
      /*use_coverage_aware_blending=*/false,
      /*source_is_opaque=*/false, caps,
      /*supports_texture_copy_dst_read=*/false);
  EXPECT_EQ(advanced.strategy, HWBlendStrategy::kFixedFunction);
  EXPECT_EQ(advanced.formula.primary_output, HWBlendOutput::kSource);
  EXPECT_EQ(advanced.formula.src_factor, GPUBlendFactor::kOne);
  EXPECT_EQ(advanced.formula.dst_factor, GPUBlendFactor::kZero);

  auto masked =
      skity::ResolveHWBlendPlan(BlendMode::kSrcIn, /*has_fragment_mask=*/true,
                                /*use_coverage_aware_blending=*/false,
                                /*source_is_opaque=*/false, caps,
                                /*supports_texture_copy_dst_read=*/false);
  EXPECT_EQ(masked.strategy, HWBlendStrategy::kFixedFunction);
  EXPECT_EQ(masked.formula.primary_output, HWBlendOutput::kSourceTimesCoverage);
  EXPECT_EQ(masked.formula.src_factor, GPUBlendFactor::kDstAlpha);
  EXPECT_EQ(masked.formula.dst_factor, GPUBlendFactor::kZero);
}

}  // namespace
