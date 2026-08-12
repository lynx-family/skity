// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include "test/ut/render/hw/coverage_blend_test_utils.hpp"

namespace {

using skity::testing::FloatColor;

void ExpectColorNear(FloatColor actual, FloatColor expected) {
  for (size_t i = 0; i < actual.size(); ++i) {
    EXPECT_NEAR(actual[i], expected[i], 1e-6f) << "component " << i;
  }
}

constexpr FloatColor kSource = {0.12f, 0.24f, 0.36f, 0.6f};
constexpr FloatColor kDestination = {0.16f, 0.08f, 0.04f, 0.8f};

}  // namespace

TEST(CoverageBlendReference, ZeroCoveragePreservesDestination) {
  for (int value = static_cast<int>(skity::BlendMode::kClear);
       value <= static_cast<int>(skity::BlendMode::kLastCoeffMode); ++value) {
    auto mode = static_cast<skity::BlendMode>(value);
    ExpectColorNear(
        skity::testing::ApplyCoverage(kSource, kDestination, mode, 0.f),
        kDestination);
  }
}

TEST(CoverageBlendReference, FullCoverageMatchesPorterDuffBlend) {
  for (int value = static_cast<int>(skity::BlendMode::kClear);
       value <= static_cast<int>(skity::BlendMode::kLastCoeffMode); ++value) {
    auto mode = static_cast<skity::BlendMode>(value);
    ExpectColorNear(
        skity::testing::ApplyCoverage(kSource, kDestination, mode, 1.f),
        skity::testing::ReferencePorterDuffBlend(kSource, kDestination, mode));
  }
}

TEST(CoverageBlendReference, CoverageIsAppliedAfterBlend) {
  constexpr float kCoverage = 0.25f;
  auto expected = skity::testing::Add(
      skity::testing::Multiply(kSource, kCoverage),
      skity::testing::Multiply(kDestination, 1.f - kCoverage));

  auto actual = skity::testing::ApplyCoverage(
      kSource, kDestination, skity::BlendMode::kSrc, kCoverage);
  ExpectColorNear(actual, expected);

  auto coverage_as_source_alpha = skity::testing::Multiply(kSource, kCoverage);
  EXPECT_NE(actual, coverage_as_source_alpha);
}
