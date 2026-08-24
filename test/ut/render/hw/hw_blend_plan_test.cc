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

TEST(HWBlendPlan, ResolvesLegacyFixedFunctionFormulas) {
  struct TestCase {
    BlendMode mode;
    GPUBlendFactor src_factor;
    GPUBlendFactor dst_factor;
  };
  constexpr TestCase kCases[] = {
      {BlendMode::kClear, GPUBlendFactor::kZero, GPUBlendFactor::kZero},
      {BlendMode::kSrc, GPUBlendFactor::kOne, GPUBlendFactor::kZero},
      {BlendMode::kDst, GPUBlendFactor::kZero, GPUBlendFactor::kOne},
      {BlendMode::kSrcOver, GPUBlendFactor::kOne,
       GPUBlendFactor::kOneMinusSrcAlpha},
      {BlendMode::kDstOver, GPUBlendFactor::kOneMinusDstAlpha,
       GPUBlendFactor::kOne},
      {BlendMode::kSrcIn, GPUBlendFactor::kDstAlpha, GPUBlendFactor::kZero},
      {BlendMode::kDstIn, GPUBlendFactor::kZero, GPUBlendFactor::kSrcAlpha},
      {BlendMode::kSrcOut, GPUBlendFactor::kOneMinusDstAlpha,
       GPUBlendFactor::kZero},
      {BlendMode::kDstOut, GPUBlendFactor::kZero,
       GPUBlendFactor::kOneMinusSrcAlpha},
      {BlendMode::kSrcATop, GPUBlendFactor::kDstAlpha,
       GPUBlendFactor::kOneMinusSrcAlpha},
      {BlendMode::kDstATop, GPUBlendFactor::kOneMinusDstAlpha,
       GPUBlendFactor::kSrcAlpha},
      {BlendMode::kXor, GPUBlendFactor::kOneMinusDstAlpha,
       GPUBlendFactor::kOneMinusSrcAlpha},
      {BlendMode::kPlus, GPUBlendFactor::kOne, GPUBlendFactor::kOne},
      {BlendMode::kModulate, GPUBlendFactor::kOne, GPUBlendFactor::kZero},
      {BlendMode::kScreen, GPUBlendFactor::kOne, GPUBlendFactor::kZero},
  };

  GPUCaps caps = {};
  for (const auto& test : kCases) {
    auto formula = skity::ResolveHWBlendFormula({test.mode}, caps,
                                                /*shader_side_blending=*/false);
    EXPECT_EQ(formula.src_factor, test.src_factor);
    EXPECT_EQ(formula.dst_factor, test.dst_factor);
    EXPECT_EQ(formula.operation, GPUBlendOperation::kAdd);
  }
}

TEST(HWBlendPlan, UsesNativeAdvancedOperationOnlyOutsideShader) {
  GPUCaps caps = {};
  caps.supports_native_advanced_blend = true;
  auto plan = skity::HWBlendPlan{BlendMode::kOverlay};

  auto native =
      skity::ResolveHWBlendFormula(plan, caps, /*shader_side_blending=*/false);
  EXPECT_EQ(native.src_factor, GPUBlendFactor::kOne);
  EXPECT_EQ(native.dst_factor, GPUBlendFactor::kOneMinusSrcAlpha);
  EXPECT_EQ(native.operation, GPUBlendOperation::kOverlay);

  auto programmable =
      skity::ResolveHWBlendFormula(plan, caps, /*shader_side_blending=*/true);
  EXPECT_EQ(programmable.src_factor, GPUBlendFactor::kOne);
  EXPECT_EQ(programmable.dst_factor, GPUBlendFactor::kZero);
  EXPECT_EQ(programmable.operation, GPUBlendOperation::kAdd);

  auto modulate = skity::ResolveHWBlendFormula({BlendMode::kModulate}, caps,
                                               /*shader_side_blending=*/false);
  EXPECT_EQ(modulate.src_factor, GPUBlendFactor::kOne);
  EXPECT_EQ(modulate.dst_factor, GPUBlendFactor::kZero);
  EXPECT_EQ(modulate.operation, GPUBlendOperation::kAdd);
}

TEST(HWBlendPlan, CarriesResolvedDestinationReadStrategy) {
  GPUCaps caps = {};
  caps.supports_framebuffer_fetch = true;

  auto plan = skity::ResolveHWBlendPlan(
      BlendMode::kOverlay, /*has_fragment_mask=*/false, caps,
      /*supports_texture_copy_dst_read=*/true);
  EXPECT_EQ(plan.blend_mode, BlendMode::kOverlay);
  EXPECT_EQ(plan.dst_read_strategy, skity::DstReadStrategy::kFramebufferFetch);
}

TEST(HWBlendPlan, FragmentMaskUsesProgrammableDestinationRead) {
  GPUCaps caps = {};
  caps.supports_framebuffer_fetch = true;

  auto framebuffer_fetch = skity::ResolveHWBlendPlan(
      BlendMode::kSrc, /*has_fragment_mask=*/true, caps,
      /*supports_texture_copy_dst_read=*/true);
  EXPECT_EQ(framebuffer_fetch.dst_read_strategy,
            skity::DstReadStrategy::kFramebufferFetch);
  EXPECT_TRUE(framebuffer_fetch.SupportsFragmentMask());

  auto plus = skity::ResolveHWBlendPlan(
      BlendMode::kPlus, /*has_fragment_mask=*/true, caps,
      /*supports_texture_copy_dst_read=*/true);
  EXPECT_EQ(plus.dst_read_strategy, skity::DstReadStrategy::kFramebufferFetch);
  EXPECT_TRUE(plus.SupportsFragmentMask());

  auto unmasked = skity::ResolveHWBlendPlan(
      BlendMode::kSrc, /*has_fragment_mask=*/false, caps,
      /*supports_texture_copy_dst_read=*/true);
  EXPECT_EQ(unmasked.dst_read_strategy, skity::DstReadStrategy::kNonRequired);

  caps.supports_framebuffer_fetch = false;
  auto texture_copy = skity::ResolveHWBlendPlan(
      BlendMode::kSrc, /*has_fragment_mask=*/true, caps,
      /*supports_texture_copy_dst_read=*/true);
  EXPECT_EQ(texture_copy.dst_read_strategy,
            skity::DstReadStrategy::kTextureCopy);
  EXPECT_TRUE(texture_copy.SupportsFragmentMask());

  auto legacy = skity::ResolveHWBlendPlan(
      BlendMode::kSrc, /*has_fragment_mask=*/true, caps,
      /*supports_texture_copy_dst_read=*/false);
  EXPECT_EQ(legacy.dst_read_strategy, skity::DstReadStrategy::kNonRequired);
  EXPECT_FALSE(legacy.SupportsFragmentMask());
}

TEST(HWBlendPlan, FragmentMaskKeepsCompatibleLegacyFormulas) {
  constexpr BlendMode kModes[] = {
      BlendMode::kDst,    BlendMode::kSrcOver, BlendMode::kDstOver,
      BlendMode::kDstOut, BlendMode::kSrcATop, BlendMode::kXor,
  };
  GPUCaps caps = {};
  caps.supports_framebuffer_fetch = true;

  for (auto mode : kModes) {
    auto plan =
        skity::ResolveHWBlendPlan(mode, /*has_fragment_mask=*/true, caps,
                                  /*supports_texture_copy_dst_read=*/true);
    EXPECT_EQ(plan.dst_read_strategy, skity::DstReadStrategy::kNonRequired);
    EXPECT_TRUE(plan.SupportsFragmentMask());
  }
}

TEST(HWBlendPlan, ShaderSideBlendReplacesDestination) {
  GPUCaps caps = {};
  auto formula = skity::ResolveHWBlendFormula({BlendMode::kSrcOver}, caps,
                                              /*shader_side_blending=*/true);
  EXPECT_EQ(formula.src_factor, GPUBlendFactor::kOne);
  EXPECT_EQ(formula.dst_factor, GPUBlendFactor::kZero);
  EXPECT_EQ(formula.operation, GPUBlendOperation::kAdd);
}

}  // namespace
