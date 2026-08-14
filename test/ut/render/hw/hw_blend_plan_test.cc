// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/hw_blend_plan.hpp"

#include <gtest/gtest.h>

#include "src/render/hw/hw_draw.hpp"
#include "test/ut/render/hw/coverage_blend_test_utils.hpp"

namespace {

using skity::testing::FloatColor;

FloatColor Subtract(FloatColor lhs, FloatColor rhs) {
  return {lhs[0] - rhs[0], lhs[1] - rhs[1], lhs[2] - rhs[2], lhs[3] - rhs[3]};
}

FloatColor OneMinus(FloatColor color) {
  return {1.f - color[0], 1.f - color[1], 1.f - color[2], 1.f - color[3]};
}

FloatColor EvaluateOutput(skity::HWBlendOutput output, FloatColor src,
                          float coverage) {
  switch (output) {
    case skity::HWBlendOutput::kNone:
      return {};
    case skity::HWBlendOutput::kCoverage:
      return {coverage, coverage, coverage, coverage};
    case skity::HWBlendOutput::kSourceTimesCoverage:
      return skity::testing::Multiply(src, coverage);
    case skity::HWBlendOutput::kOneMinusSourceAlphaTimesCoverage:
      return {coverage * (1.f - src[3]), coverage * (1.f - src[3]),
              coverage * (1.f - src[3]), coverage * (1.f - src[3])};
    case skity::HWBlendOutput::kOneMinusSourceTimesCoverage:
      return skity::testing::Multiply(OneMinus(src), coverage);
  }
  return {};
}

FloatColor EvaluateFactor(skity::GPUBlendFactor factor, FloatColor src,
                          FloatColor dst, FloatColor src1) {
  switch (factor) {
    case skity::GPUBlendFactor::kZero:
      return {};
    case skity::GPUBlendFactor::kOne:
      return {1.f, 1.f, 1.f, 1.f};
    case skity::GPUBlendFactor::kSrc:
      return src;
    case skity::GPUBlendFactor::kOneMinusSrc:
      return OneMinus(src);
    case skity::GPUBlendFactor::kSrcAlpha:
      return {src[3], src[3], src[3], src[3]};
    case skity::GPUBlendFactor::kOneMinusSrcAlpha:
      return {1.f - src[3], 1.f - src[3], 1.f - src[3], 1.f - src[3]};
    case skity::GPUBlendFactor::kDst:
      return dst;
    case skity::GPUBlendFactor::kOneMinusDst:
      return OneMinus(dst);
    case skity::GPUBlendFactor::kDstAlpha:
      return {dst[3], dst[3], dst[3], dst[3]};
    case skity::GPUBlendFactor::kOneMinusDstAlpha:
      return {1.f - dst[3], 1.f - dst[3], 1.f - dst[3], 1.f - dst[3]};
    case skity::GPUBlendFactor::kSrcAlphaSaturated: {
      float value = std::min(src[3], 1.f - dst[3]);
      return {value, value, value, 1.f};
    }
    case skity::GPUBlendFactor::kSrc1:
      return src1;
    case skity::GPUBlendFactor::kOneMinusSrc1:
      return OneMinus(src1);
    case skity::GPUBlendFactor::kSrc1Alpha:
      return {src1[3], src1[3], src1[3], src1[3]};
    case skity::GPUBlendFactor::kOneMinusSrc1Alpha:
      return {1.f - src1[3], 1.f - src1[3], 1.f - src1[3], 1.f - src1[3]};
  }
  return {};
}

FloatColor SimulateHardwareBlend(const skity::HWBlendPlan& plan, FloatColor src,
                                 FloatColor dst, float coverage) {
  auto primary = EvaluateOutput(plan.formula.primary_output, src, coverage);
  auto secondary = EvaluateOutput(plan.formula.secondary_output, src, coverage);
  auto src_term = skity::testing::Multiply(
      primary,
      EvaluateFactor(plan.formula.src_factor, primary, dst, secondary));
  auto dst_term = skity::testing::Multiply(
      dst, EvaluateFactor(plan.formula.dst_factor, primary, dst, secondary));
  FloatColor result;
  if (plan.formula.operation == skity::GPUBlendOperation::kReverseSubtract) {
    result = Subtract(dst_term, src_term);
  } else {
    result = skity::testing::Add(src_term, dst_term);
  }
  for (auto& component : result) {
    component = std::clamp(component, 0.f, 1.f);
  }
  return result;
}

void ExpectColorNear(FloatColor actual, FloatColor expected) {
  for (size_t i = 0; i < actual.size(); ++i) {
    EXPECT_NEAR(actual[i], expected[i], 1e-5f) << "component " << i;
  }
}

constexpr std::array<FloatColor, 6> kColors = {{
    {0.f, 0.f, 0.f, 0.f},
    {0.5f, 0.1f, 0.3f, 0.5f},
    {0.05f, 0.4f, 0.2f, 0.5f},
    {0.12f, 0.24f, 0.36f, 0.6f},
    {0.16f, 0.08f, 0.04f, 0.8f},
    {1.f, 0.25f, 0.75f, 1.f},
}};

constexpr std::array<float, 7> kCoverages = {0.f,   1.f / 256.f,   0.25f, 0.5f,
                                             0.75f, 255.f / 256.f, 1.f};

void ExpectFormulaMatchesReference(const skity::HWBlendPlan& plan,
                                   skity::BlendMode mode, float coverage) {
  for (size_t src_index = 0; src_index < kColors.size(); ++src_index) {
    for (size_t dst_index = 0; dst_index < kColors.size(); ++dst_index) {
      SCOPED_TRACE(::testing::Message()
                   << "mode=" << skity::BlendMode_Name(mode)
                   << " src=" << src_index << " dst=" << dst_index
                   << " coverage=" << coverage);
      ExpectColorNear(
          SimulateHardwareBlend(plan, kColors[src_index], kColors[dst_index],
                                coverage),
          skity::testing::ApplyCoverage(kColors[src_index], kColors[dst_index],
                                        mode, coverage));
    }
  }
}

constexpr FloatColor kOpaqueSource = {0.2f, 0.4f, 0.7f, 1.f};
constexpr FloatColor kDestination = {0.16f, 0.08f, 0.04f, 0.8f};

class MergeableDraw final : public skity::HWDraw {
 public:
  MergeableDraw() : HWDraw(skity::Matrix{}) {}

  void Draw(skity::GPURenderPass*, skity::GPUCommandBuffer*) override {}

  skity::HWDrawType GetDrawType() const override {
    return skity::HWDrawType::kRRect;
  }

 private:
  skity::HWDrawState OnPrepare(skity::HWDrawContext*) override {
    return skity::kDrawStateNone;
  }

  void OnGenerateCommand(skity::HWDrawContext*, skity::HWDrawState) override {}

  bool OnMergeIfPossible(skity::HWDraw*) override { return true; }
};

}  // namespace

TEST(HWBlendPlan, FixedCoverageFormulasMatchReference) {
  skity::GPUCaps caps;

  for (int value = static_cast<int>(skity::BlendMode::kClear);
       value <= static_cast<int>(skity::BlendMode::kLastCoeffMode); ++value) {
    auto mode = static_cast<skity::BlendMode>(value);
    auto plan =
        skity::ResolveHWBlendPlan(mode, true, /*source_is_opaque=*/false, caps,
                                  /*supports_texture_copy_dst_read=*/true);
    ASSERT_TRUE(plan.has_value()) << skity::BlendMode_Name(mode);
    if (plan->strategy != skity::HWBlendStrategy::kFixedFunction) {
      continue;
    }

    for (float coverage : kCoverages) {
      ExpectFormulaMatchesReference(*plan, mode, coverage);
    }
  }
}

TEST(HWBlendPlan, SecondaryOutputModesUseProgrammableFallback) {
  skity::GPUCaps caps;
  constexpr std::array<skity::BlendMode, 4> kModes = {
      skity::BlendMode::kSrc, skity::BlendMode::kSrcIn,
      skity::BlendMode::kSrcOut, skity::BlendMode::kDstATop};

  for (auto mode : kModes) {
    auto plan =
        skity::ResolveHWBlendPlan(mode, true, /*source_is_opaque=*/false, caps,
                                  /*supports_texture_copy_dst_read=*/true);
    ASSERT_TRUE(plan.has_value());
    EXPECT_EQ(plan->strategy, skity::HWBlendStrategy::kProgrammable);
    EXPECT_EQ(plan->dst_read_strategy, skity::DstReadStrategy::kTextureCopy);
  }
}

TEST(HWBlendPlan, DualSourceCoverageFormulasMatchReference) {
  skity::GPUCaps caps;
  caps.supports_dual_source_blending = true;
  constexpr std::array<skity::BlendMode, 4> kModes = {
      skity::BlendMode::kSrc, skity::BlendMode::kSrcIn,
      skity::BlendMode::kSrcOut, skity::BlendMode::kDstATop};

  for (auto mode : kModes) {
    auto plan =
        skity::ResolveHWBlendPlan(mode, true, /*source_is_opaque=*/false, caps,
                                  /*supports_texture_copy_dst_read=*/false);
    ASSERT_TRUE(plan.has_value());
    EXPECT_EQ(plan->strategy, skity::HWBlendStrategy::kDualSource);
    EXPECT_EQ(plan->dst_read_strategy, skity::DstReadStrategy::kNonRequired);
    EXPECT_NE(plan->formula.secondary_output, skity::HWBlendOutput::kNone);

    for (float coverage : kCoverages) {
      ExpectFormulaMatchesReference(*plan, mode, coverage);
    }
  }
}

TEST(HWBlendPlan, OpaqueSourceCoverageUsesFixedFunctionFormulas) {
  constexpr std::array<skity::BlendMode, 6> kModes = {
      skity::BlendMode::kSrc,    skity::BlendMode::kSrcIn,
      skity::BlendMode::kSrcOut, skity::BlendMode::kDstATop,
      skity::BlendMode::kDstIn,  skity::BlendMode::kDstOut};
  for (bool supports_dual_source : {false, true}) {
    skity::GPUCaps caps;
    caps.supports_dual_source_blending = supports_dual_source;
    for (auto mode : kModes) {
      auto plan =
          skity::ResolveHWBlendPlan(mode, true, /*source_is_opaque=*/true, caps,
                                    /*supports_texture_copy_dst_read=*/false);
      ASSERT_TRUE(plan.has_value()) << skity::BlendMode_Name(mode);
      EXPECT_EQ(plan->strategy, skity::HWBlendStrategy::kFixedFunction);
      EXPECT_EQ(plan->dst_read_strategy, skity::DstReadStrategy::kNonRequired);
      EXPECT_EQ(plan->formula.secondary_output, skity::HWBlendOutput::kNone);

      for (float coverage : kCoverages) {
        ExpectColorNear(
            SimulateHardwareBlend(*plan, kOpaqueSource, kDestination, coverage),
            skity::testing::ApplyCoverage(kOpaqueSource, kDestination, mode,
                                          coverage));
      }
    }
  }
}

TEST(HWBlendPlan, UnsupportedDestinationReadReturnsNullopt) {
  skity::GPUCaps caps;
  EXPECT_FALSE(skity::ResolveHWBlendPlan(
      skity::BlendMode::kSrc, true, /*source_is_opaque=*/false, caps,
      /*supports_texture_copy_dst_read=*/false));
}

TEST(HWBlendPlan, FramebufferFetchIsPreferredForProgrammableFallback) {
  skity::GPUCaps caps;
  caps.supports_framebuffer_fetch = true;
  auto plan =
      skity::ResolveHWBlendPlan(skity::BlendMode::kSrc, true,
                                /*source_is_opaque=*/false, caps,
                                /*supports_texture_copy_dst_read=*/true);
  ASSERT_TRUE(plan.has_value());
  EXPECT_EQ(plan->dst_read_strategy, skity::DstReadStrategy::kFramebufferFetch);
}

TEST(HWBlendPlan, RegularCoefficientModesKeepExistingBlendState) {
  skity::GPUCaps caps;
  auto src =
      skity::ResolveHWBlendPlan(skity::BlendMode::kSrc, false,
                                /*source_is_opaque=*/false, caps,
                                /*supports_texture_copy_dst_read=*/false);
  ASSERT_TRUE(src.has_value());
  EXPECT_EQ(src->formula.src_factor, skity::GPUBlendFactor::kOne);
  EXPECT_EQ(src->formula.dst_factor, skity::GPUBlendFactor::kZero);

  auto src_over =
      skity::ResolveHWBlendPlan(skity::BlendMode::kSrcOver, false,
                                /*source_is_opaque=*/false, caps,
                                /*supports_texture_copy_dst_read=*/false);
  ASSERT_TRUE(src_over.has_value());
  EXPECT_EQ(src_over->formula.src_factor, skity::GPUBlendFactor::kOne);
  EXPECT_EQ(src_over->formula.dst_factor,
            skity::GPUBlendFactor::kOneMinusSrcAlpha);
}

TEST(HWBlendPlan, RegularCoefficientFormulasMatchReference) {
  skity::GPUCaps caps;
  for (int value = static_cast<int>(skity::BlendMode::kClear);
       value <= static_cast<int>(skity::BlendMode::kLastCoeffMode); ++value) {
    auto mode = static_cast<skity::BlendMode>(value);
    auto plan =
        skity::ResolveHWBlendPlan(mode, false, /*source_is_opaque=*/false, caps,
                                  /*supports_texture_copy_dst_read=*/false);
    ASSERT_TRUE(plan.has_value()) << skity::BlendMode_Name(mode);
    ASSERT_EQ(plan->strategy, skity::HWBlendStrategy::kFixedFunction);
    ExpectFormulaMatchesReference(*plan, mode, 1.f);
  }
}

TEST(HWBlendPlan, AdvancedBlendKeepsExistingPriority) {
  skity::GPUCaps caps;
  caps.supports_native_advanced_blend = true;
  caps.supports_native_advanced_blend_coherent = true;
  caps.supports_framebuffer_fetch = true;
  auto coherent =
      skity::ResolveHWBlendPlan(skity::BlendMode::kMultiply, true,
                                /*source_is_opaque=*/false, caps,
                                /*supports_texture_copy_dst_read=*/true);
  ASSERT_TRUE(coherent.has_value());
  EXPECT_EQ(coherent->strategy, skity::HWBlendStrategy::kFixedFunction);
  EXPECT_EQ(coherent->dst_read_strategy, skity::DstReadStrategy::kNonRequired);

  caps.supports_native_advanced_blend_coherent = false;
  auto fetch =
      skity::ResolveHWBlendPlan(skity::BlendMode::kMultiply, true,
                                /*source_is_opaque=*/false, caps,
                                /*supports_texture_copy_dst_read=*/true);
  ASSERT_TRUE(fetch.has_value());
  EXPECT_EQ(fetch->strategy, skity::HWBlendStrategy::kProgrammable);
  EXPECT_EQ(fetch->dst_read_strategy,
            skity::DstReadStrategy::kFramebufferFetch);
}

TEST(HWBlendPlan, DrawMergeRequiresIdenticalResolvedPlans) {
  skity::GPUCaps caps;
  auto src_over =
      skity::ResolveHWBlendPlan(skity::BlendMode::kSrcOver, false,
                                /*source_is_opaque=*/false, caps,
                                /*supports_texture_copy_dst_read=*/false);
  auto dst =
      skity::ResolveHWBlendPlan(skity::BlendMode::kDst, false,
                                /*source_is_opaque=*/false, caps,
                                /*supports_texture_copy_dst_read=*/false);
  ASSERT_TRUE(src_over.has_value());
  ASSERT_TRUE(dst.has_value());

  MergeableDraw first;
  MergeableDraw same_plan;
  first.SetBlendPlan(src_over.value());
  same_plan.SetBlendPlan(src_over.value());
  EXPECT_TRUE(first.MergeIfPossible(&same_plan));

  MergeableDraw different_plan;
  different_plan.SetBlendPlan(dst.value());
  EXPECT_FALSE(first.MergeIfPossible(&different_plan));

  MergeableDraw unresolved;
  EXPECT_FALSE(first.MergeIfPossible(&unresolved));
}
