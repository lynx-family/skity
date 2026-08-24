// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/hw_blend_plan.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>

#include "src/graphic/blend_mode_priv.hpp"

namespace {

using skity::BlendMode;
using skity::DstReadStrategy;
using skity::GPUBlendFactor;
using skity::GPUBlendOperation;
using skity::GPUCaps;
using skity::HWBlendFormula;
using skity::HWBlendOutput;
using skity::HWBlendPlan;

using RGBA = std::array<float, 4>;

RGBA ToRGBA(skity::PMColor color) {
  constexpr float kScale = 1.0f / 255.0f;
  return {ColorGetR(color) * kScale, ColorGetG(color) * kScale,
          ColorGetB(color) * kScale, ColorGetA(color) * kScale};
}

RGBA EvaluateOutput(HWBlendOutput output, const RGBA& source, float coverage) {
  RGBA result = {};
  for (size_t channel = 0; channel < result.size(); channel++) {
    switch (output) {
      case HWBlendOutput::kNone:
        break;
      case HWBlendOutput::kSource:
        result[channel] = source[channel];
        break;
      case HWBlendOutput::kCoverage:
        result[channel] = coverage;
        break;
      case HWBlendOutput::kSourceTimesCoverage:
        result[channel] = source[channel] * coverage;
        break;
      case HWBlendOutput::kOneMinusSourceAlphaTimesCoverage:
        result[channel] = (1.0f - source[3]) * coverage;
        break;
      case HWBlendOutput::kOneMinusSourceTimesCoverage:
        result[channel] = (1.0f - source[channel]) * coverage;
        break;
    }
  }
  return result;
}

float EvaluateFactor(GPUBlendFactor factor, const RGBA& source,
                     const RGBA& source1, const RGBA& destination,
                     size_t channel) {
  switch (factor) {
    case GPUBlendFactor::kZero:
      return 0.0f;
    case GPUBlendFactor::kOne:
      return 1.0f;
    case GPUBlendFactor::kSrc:
      return source[channel];
    case GPUBlendFactor::kOneMinusSrc:
      return 1.0f - source[channel];
    case GPUBlendFactor::kSrcAlpha:
      return source[3];
    case GPUBlendFactor::kOneMinusSrcAlpha:
      return 1.0f - source[3];
    case GPUBlendFactor::kDst:
      return destination[channel];
    case GPUBlendFactor::kOneMinusDst:
      return 1.0f - destination[channel];
    case GPUBlendFactor::kDstAlpha:
      return destination[3];
    case GPUBlendFactor::kOneMinusDstAlpha:
      return 1.0f - destination[3];
    case GPUBlendFactor::kSrcAlphaSaturated:
      return channel == 3 ? 1.0f : std::min(source[3], 1.0f - destination[3]);
    case GPUBlendFactor::kSrc1:
      return source1[channel];
    case GPUBlendFactor::kOneMinusSrc1:
      return 1.0f - source1[channel];
    case GPUBlendFactor::kSrc1Alpha:
      return source1[3];
    case GPUBlendFactor::kOneMinusSrc1Alpha:
      return 1.0f - source1[3];
  }
  return 0.0f;
}

RGBA EvaluateFormula(const HWBlendFormula& formula, const RGBA& source,
                     const RGBA& destination, float coverage) {
  auto primary = EvaluateOutput(formula.primary_output, source, coverage);
  auto secondary = EvaluateOutput(formula.secondary_output, source, coverage);
  RGBA result = {};
  for (size_t channel = 0; channel < result.size(); channel++) {
    float src_term =
        primary[channel] * EvaluateFactor(formula.src_factor, primary,
                                          secondary, destination, channel);
    float dst_term =
        destination[channel] * EvaluateFactor(formula.dst_factor, primary,
                                              secondary, destination, channel);
    if (formula.operation == GPUBlendOperation::kAdd) {
      result[channel] = src_term + dst_term;
    } else {
      EXPECT_EQ(formula.operation, GPUBlendOperation::kReverseSubtract);
      result[channel] = dst_term - src_term;
    }
    result[channel] = std::clamp(result[channel], 0.0f, 1.0f);
  }
  return result;
}

void ExpectCoverageFormulaMatchesReference(BlendMode mode,
                                           skity::PMColor source_color,
                                           skity::PMColor destination_color,
                                           bool source_is_opaque,
                                           const GPUCaps& caps) {
  auto plan = skity::ResolveHWBlendPlan(
      mode, /*has_fragment_mask=*/true, source_is_opaque, caps,
      /*supports_texture_copy_dst_read=*/false);
  ASSERT_TRUE(plan.has_value());
  ASSERT_EQ(plan->dst_read_strategy, DstReadStrategy::kNonRequired);

  auto source = ToRGBA(source_color);
  auto destination = ToRGBA(destination_color);
  auto blended =
      ToRGBA(skity::PorterDuffBlend(source_color, destination_color, mode));
  constexpr float kCoverages[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
  constexpr float kTolerance = 2.0f / 255.0f;
  for (float coverage : kCoverages) {
    auto actual = EvaluateFormula(plan->formula, source, destination, coverage);
    for (size_t channel = 0; channel < actual.size(); channel++) {
      float expected = destination[channel] * (1.0f - coverage) +
                       blended[channel] * coverage;
      EXPECT_NEAR(actual[channel], expected, kTolerance)
          << "mode=" << static_cast<int>(mode) << ", coverage=" << coverage
          << ", channel=" << channel;
    }
  }
}

HWBlendPlan Resolve(BlendMode mode, bool has_fragment_mask,
                    bool source_is_opaque, const GPUCaps& caps = {},
                    bool supports_texture_copy = false) {
  auto plan = skity::ResolveHWBlendPlan(
      mode, has_fragment_mask, source_is_opaque, caps, supports_texture_copy);
  EXPECT_TRUE(plan.has_value());
  return plan.value_or(HWBlendPlan{});
}

void ExpectFormula(const HWBlendPlan& plan, HWBlendOutput primary,
                   GPUBlendFactor src, GPUBlendFactor dst,
                   GPUBlendOperation operation = GPUBlendOperation::kAdd,
                   HWBlendOutput secondary = HWBlendOutput::kNone) {
  EXPECT_EQ(plan.formula.primary_output, primary);
  EXPECT_EQ(plan.formula.src_factor, src);
  EXPECT_EQ(plan.formula.dst_factor, dst);
  EXPECT_EQ(plan.formula.operation, operation);
  EXPECT_EQ(plan.formula.secondary_output, secondary);
}

TEST(HWBlendPlan, ResolvesRegularFixedFunctionFormulas) {
  struct TestCase {
    BlendMode mode;
    HWBlendOutput output;
    GPUBlendFactor src_factor;
    GPUBlendFactor dst_factor;
  };
  constexpr TestCase kCases[] = {
      {BlendMode::kClear, HWBlendOutput::kNone, GPUBlendFactor::kZero,
       GPUBlendFactor::kZero},
      {BlendMode::kSrc, HWBlendOutput::kSource, GPUBlendFactor::kOne,
       GPUBlendFactor::kZero},
      {BlendMode::kDst, HWBlendOutput::kNone, GPUBlendFactor::kZero,
       GPUBlendFactor::kOne},
      {BlendMode::kSrcOver, HWBlendOutput::kSource, GPUBlendFactor::kOne,
       GPUBlendFactor::kOneMinusSrcAlpha},
      {BlendMode::kDstOver, HWBlendOutput::kSource,
       GPUBlendFactor::kOneMinusDstAlpha, GPUBlendFactor::kOne},
      {BlendMode::kSrcIn, HWBlendOutput::kSource, GPUBlendFactor::kDstAlpha,
       GPUBlendFactor::kZero},
      {BlendMode::kDstIn, HWBlendOutput::kSource, GPUBlendFactor::kZero,
       GPUBlendFactor::kSrcAlpha},
      {BlendMode::kSrcOut, HWBlendOutput::kSource,
       GPUBlendFactor::kOneMinusDstAlpha, GPUBlendFactor::kZero},
      {BlendMode::kDstOut, HWBlendOutput::kSource, GPUBlendFactor::kZero,
       GPUBlendFactor::kOneMinusSrcAlpha},
      {BlendMode::kSrcATop, HWBlendOutput::kSource, GPUBlendFactor::kDstAlpha,
       GPUBlendFactor::kOneMinusSrcAlpha},
      {BlendMode::kDstATop, HWBlendOutput::kSource,
       GPUBlendFactor::kOneMinusDstAlpha, GPUBlendFactor::kSrcAlpha},
      {BlendMode::kXor, HWBlendOutput::kSource,
       GPUBlendFactor::kOneMinusDstAlpha, GPUBlendFactor::kOneMinusSrcAlpha},
      {BlendMode::kPlus, HWBlendOutput::kSource, GPUBlendFactor::kOne,
       GPUBlendFactor::kOne},
      {BlendMode::kModulate, HWBlendOutput::kSource, GPUBlendFactor::kZero,
       GPUBlendFactor::kSrc},
      {BlendMode::kScreen, HWBlendOutput::kSource, GPUBlendFactor::kOne,
       GPUBlendFactor::kOneMinusSrc},
  };

  for (const auto& test : kCases) {
    auto plan = skity::ResolveCoefficientBlendPlan(test.mode);
    ExpectFormula(plan, test.output, test.src_factor, test.dst_factor);
    EXPECT_EQ(plan.dst_read_strategy, DstReadStrategy::kNonRequired);
  }
}

TEST(HWBlendPlan, ResolvesCoverageFixedFunctionFormulas) {
  struct TestCase {
    BlendMode mode;
    HWBlendOutput output;
    GPUBlendFactor src_factor;
    GPUBlendFactor dst_factor;
    GPUBlendOperation operation;
  };
  constexpr TestCase kCases[] = {
      {BlendMode::kClear, HWBlendOutput::kCoverage, GPUBlendFactor::kDst,
       GPUBlendFactor::kOne, GPUBlendOperation::kReverseSubtract},
      {BlendMode::kDst, HWBlendOutput::kNone, GPUBlendFactor::kZero,
       GPUBlendFactor::kOne, GPUBlendOperation::kAdd},
      {BlendMode::kSrcOver, HWBlendOutput::kSourceTimesCoverage,
       GPUBlendFactor::kOne, GPUBlendFactor::kOneMinusSrcAlpha,
       GPUBlendOperation::kAdd},
      {BlendMode::kDstOver, HWBlendOutput::kSourceTimesCoverage,
       GPUBlendFactor::kOneMinusDstAlpha, GPUBlendFactor::kOne,
       GPUBlendOperation::kAdd},
      {BlendMode::kDstIn, HWBlendOutput::kOneMinusSourceAlphaTimesCoverage,
       GPUBlendFactor::kDst, GPUBlendFactor::kOne,
       GPUBlendOperation::kReverseSubtract},
      {BlendMode::kDstOut, HWBlendOutput::kSourceTimesCoverage,
       GPUBlendFactor::kZero, GPUBlendFactor::kOneMinusSrcAlpha,
       GPUBlendOperation::kAdd},
      {BlendMode::kSrcATop, HWBlendOutput::kSourceTimesCoverage,
       GPUBlendFactor::kDstAlpha, GPUBlendFactor::kOneMinusSrcAlpha,
       GPUBlendOperation::kAdd},
      {BlendMode::kXor, HWBlendOutput::kSourceTimesCoverage,
       GPUBlendFactor::kOneMinusDstAlpha, GPUBlendFactor::kOneMinusSrcAlpha,
       GPUBlendOperation::kAdd},
      {BlendMode::kModulate, HWBlendOutput::kOneMinusSourceTimesCoverage,
       GPUBlendFactor::kDst, GPUBlendFactor::kOne,
       GPUBlendOperation::kReverseSubtract},
      {BlendMode::kScreen, HWBlendOutput::kSourceTimesCoverage,
       GPUBlendFactor::kOne, GPUBlendFactor::kOneMinusSrc,
       GPUBlendOperation::kAdd},
  };

  for (const auto& test : kCases) {
    auto plan = Resolve(test.mode, /*has_fragment_mask=*/true,
                        /*source_is_opaque=*/false);
    ExpectFormula(plan, test.output, test.src_factor, test.dst_factor,
                  test.operation);
    EXPECT_EQ(plan.dst_read_strategy, DstReadStrategy::kNonRequired);
  }
}

TEST(HWBlendPlan, UsesOpaqueSourceCoverageFormulas) {
  struct TestCase {
    BlendMode mode;
    HWBlendOutput output;
    GPUBlendFactor src_factor;
    GPUBlendFactor dst_factor;
    GPUBlendOperation operation;
  };
  constexpr TestCase kCases[] = {
      {BlendMode::kSrc, HWBlendOutput::kSourceTimesCoverage,
       GPUBlendFactor::kOne, GPUBlendFactor::kOneMinusSrcAlpha,
       GPUBlendOperation::kAdd},
      {BlendMode::kSrcIn, HWBlendOutput::kSourceTimesCoverage,
       GPUBlendFactor::kDstAlpha, GPUBlendFactor::kOneMinusSrcAlpha,
       GPUBlendOperation::kAdd},
      {BlendMode::kSrcOut, HWBlendOutput::kSourceTimesCoverage,
       GPUBlendFactor::kOneMinusDstAlpha, GPUBlendFactor::kOneMinusSrcAlpha,
       GPUBlendOperation::kAdd},
      {BlendMode::kDstATop, HWBlendOutput::kSourceTimesCoverage,
       GPUBlendFactor::kOneMinusDstAlpha, GPUBlendFactor::kOne,
       GPUBlendOperation::kAdd},
      {BlendMode::kDstIn, HWBlendOutput::kNone, GPUBlendFactor::kZero,
       GPUBlendFactor::kOne, GPUBlendOperation::kAdd},
      {BlendMode::kDstOut, HWBlendOutput::kCoverage, GPUBlendFactor::kDst,
       GPUBlendFactor::kOne, GPUBlendOperation::kReverseSubtract},
  };

  for (const auto& test : kCases) {
    auto plan = Resolve(test.mode, /*has_fragment_mask=*/true,
                        /*source_is_opaque=*/true);
    ExpectFormula(plan, test.output, test.src_factor, test.dst_factor,
                  test.operation);
    EXPECT_EQ(plan.dst_read_strategy, DstReadStrategy::kNonRequired);
  }
}

TEST(HWBlendPlan, UsesDualSourceForTranslucentCoverage) {
  struct TestCase {
    BlendMode mode;
    GPUBlendFactor src_factor;
    HWBlendOutput secondary_output;
  };
  constexpr TestCase kCases[] = {
      {BlendMode::kSrc, GPUBlendFactor::kOne, HWBlendOutput::kCoverage},
      {BlendMode::kSrcIn, GPUBlendFactor::kDstAlpha, HWBlendOutput::kCoverage},
      {BlendMode::kSrcOut, GPUBlendFactor::kOneMinusDstAlpha,
       HWBlendOutput::kCoverage},
      {BlendMode::kDstATop, GPUBlendFactor::kOneMinusDstAlpha,
       HWBlendOutput::kOneMinusSourceAlphaTimesCoverage},
  };

  GPUCaps caps = {};
  caps.supports_dual_source_blending = true;
  for (const auto& test : kCases) {
    auto plan = Resolve(test.mode, /*has_fragment_mask=*/true,
                        /*source_is_opaque=*/false, caps);
    ExpectFormula(plan, HWBlendOutput::kSourceTimesCoverage, test.src_factor,
                  GPUBlendFactor::kOneMinusSrc1Alpha, GPUBlendOperation::kAdd,
                  test.secondary_output);
    EXPECT_EQ(plan.dst_read_strategy, DstReadStrategy::kNonRequired);
  }
}

TEST(HWBlendPlan, CoverageFormulasMatchPorterDuffReference) {
  constexpr BlendMode kSingleSourceModes[] = {
      BlendMode::kClear,   BlendMode::kDst,   BlendMode::kSrcOver,
      BlendMode::kDstOver, BlendMode::kDstIn, BlendMode::kDstOut,
      BlendMode::kSrcATop, BlendMode::kXor,   BlendMode::kModulate,
      BlendMode::kScreen,
  };
  constexpr BlendMode kOpaqueSourceModes[] = {
      BlendMode::kSrc,     BlendMode::kSrcIn, BlendMode::kSrcOut,
      BlendMode::kDstATop, BlendMode::kDstIn, BlendMode::kDstOut,
  };
  constexpr BlendMode kDualSourceModes[] = {
      BlendMode::kSrc,
      BlendMode::kSrcIn,
      BlendMode::kSrcOut,
      BlendMode::kDstATop,
  };
  constexpr skity::PMColor kSource = skity::ColorSetARGB(160, 120, 40, 80);
  constexpr skity::PMColor kOpaqueSource =
      skity::ColorSetARGB(255, 190, 70, 130);
  constexpr skity::PMColor kDestination =
      skity::ColorSetARGB(192, 40, 150, 100);

  GPUCaps caps = {};
  for (auto mode : kSingleSourceModes) {
    ExpectCoverageFormulaMatchesReference(mode, kSource, kDestination,
                                          /*source_is_opaque=*/false, caps);
  }
  for (auto mode : kOpaqueSourceModes) {
    ExpectCoverageFormulaMatchesReference(mode, kOpaqueSource, kDestination,
                                          /*source_is_opaque=*/true, caps);
  }

  caps.supports_dual_source_blending = true;
  for (auto mode : kDualSourceModes) {
    ExpectCoverageFormulaMatchesReference(mode, kSource, kDestination,
                                          /*source_is_opaque=*/false, caps);
  }
}

TEST(HWBlendPlan, CoverageFallbackPrefersFramebufferFetchThenTextureCopy) {
  GPUCaps caps = {};
  caps.supports_framebuffer_fetch = true;
  auto framebuffer_fetch =
      Resolve(BlendMode::kSrc, /*has_fragment_mask=*/true,
              /*source_is_opaque=*/false, caps, /*supports_texture_copy=*/true);
  EXPECT_EQ(framebuffer_fetch.dst_read_strategy,
            DstReadStrategy::kFramebufferFetch);
  ExpectFormula(framebuffer_fetch, HWBlendOutput::kSource, GPUBlendFactor::kOne,
                GPUBlendFactor::kZero);

  caps.supports_framebuffer_fetch = false;
  auto texture_copy = Resolve(BlendMode::kSrc,
                              /*has_fragment_mask=*/true,
                              /*source_is_opaque=*/false, caps,
                              /*supports_texture_copy=*/true);
  EXPECT_EQ(texture_copy.dst_read_strategy, DstReadStrategy::kTextureCopy);

  auto unsupported =
      skity::ResolveHWBlendPlan(BlendMode::kSrc, /*has_fragment_mask=*/true,
                                /*source_is_opaque=*/false, caps,
                                /*supports_texture_copy_dst_read=*/false);
  EXPECT_FALSE(unsupported.has_value());
}

TEST(HWBlendPlan, PlusWithCoverageAlwaysUsesProgrammableBlending) {
  GPUCaps caps = {};
  caps.supports_dual_source_blending = true;
  caps.supports_framebuffer_fetch = true;
  auto plus = Resolve(BlendMode::kPlus, /*has_fragment_mask=*/true,
                      /*source_is_opaque=*/true, caps,
                      /*supports_texture_copy=*/true);
  EXPECT_EQ(plus.dst_read_strategy, DstReadStrategy::kFramebufferFetch);
  EXPECT_EQ(plus.formula.primary_output, HWBlendOutput::kSource);

  caps.supports_framebuffer_fetch = false;
  auto unsupported =
      skity::ResolveHWBlendPlan(BlendMode::kPlus, /*has_fragment_mask=*/true,
                                /*source_is_opaque=*/true, caps,
                                /*supports_texture_copy_dst_read=*/false);
  EXPECT_FALSE(unsupported.has_value());
}

TEST(HWBlendPlan, FragmentMaskDoesNotUseNativeAdvancedBlend) {
  GPUCaps caps = {};
  caps.supports_native_advanced_blend = true;
  caps.supports_native_advanced_blend_coherent = true;

  auto unmasked = Resolve(BlendMode::kOverlay,
                          /*has_fragment_mask=*/false,
                          /*source_is_opaque=*/false, caps);
  EXPECT_EQ(unmasked.dst_read_strategy, DstReadStrategy::kNativeBlend);
  EXPECT_EQ(unmasked.formula.operation, GPUBlendOperation::kOverlay);

  auto masked =
      skity::ResolveHWBlendPlan(BlendMode::kOverlay, /*has_fragment_mask=*/true,
                                /*source_is_opaque=*/false, caps,
                                /*supports_texture_copy_dst_read=*/false);
  EXPECT_FALSE(masked.has_value());

  caps.supports_framebuffer_fetch = true;
  auto programmable = Resolve(BlendMode::kOverlay,
                              /*has_fragment_mask=*/true,
                              /*source_is_opaque=*/false, caps);
  EXPECT_EQ(programmable.dst_read_strategy, DstReadStrategy::kFramebufferFetch);
  EXPECT_EQ(programmable.formula.operation, GPUBlendOperation::kAdd);
}

}  // namespace
