// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/effect/color_filter.hpp>
#include <skity/effect/shader.hpp>

#include "gtest/gtest.h"
#include "src/graphic/color_xform.hpp"

TEST(GradientColorSpace, SetAndGetStopColorSpace) {
  const skity::Point pts[2] = {{0.f, 0.f, 0.f, 1.f}, {64.f, 0.f, 0.f, 1.f}};
  const skity::Vec4 stops[2] = {{1.f, 0.f, 0.f, 1.f}, {0.f, 0.f, 1.f, 1.f}};

  auto shader = skity::Shader::MakeLinear(pts, stops, nullptr, 2);
  // All stops share one space; the default is sRGB.
  EXPECT_TRUE(shader->GetColorSpace().IsSRGB());

  shader->SetColorSpace(skity::ColorSpace::DisplayP3());
  EXPECT_FALSE(shader->GetColorSpace().IsSRGB());

  skity::Shader::GradientInfo info{};
  shader->AsGradient(&info);
  EXPECT_FALSE(info.color_space.IsSRGB());
  // The stop values themselves stay untouched until draw time.
  EXPECT_EQ(info.colors[0].r, 1.f);
}

TEST(GradientColorSpace, BlendFilterResolvesRecordedColor) {
  // A P3-recorded red blend color applied on a P3 target stays put; on an
  // sRGB target it resolves into the sRGB encoding of the P3 red.
  auto filter = skity::ColorFilters::Blend(skity::Color4f{1.f, 0.f, 0.f, 1.f},
                                           skity::BlendMode::kSrcOver,
                                           skity::ColorSpace::DisplayP3());

  skity::PMColor src =
      skity::ColorToPMColor(skity::ColorSetARGB(255, 255, 255, 255));

  auto p3_result = filter->FilterColor(src, skity::ColorSpace::DisplayP3());
  EXPECT_EQ(ColorGetR(skity::PMColorToColor(p3_result)), 255);

  auto srgb_result = filter->FilterColor(src, skity::ColorSpace{});
  auto expected = skity::ColorSpace::DisplayP3().Convert(
      skity::Color4f{1.f, 0.f, 0.f, 1.f}, skity::ColorSpace{});
  auto clamped = skity::Color4fToColor(
      skity::Color4f{std::max(0.f, std::min(1.f, expected.r)),
                     std::max(0.f, std::min(1.f, expected.g)),
                     std::max(0.f, std::min(1.f, expected.b)), 1.f});
  // P3 red is out of the sRGB gamut; the resolved color clamps at
  // quantization exactly like the pipeline does.
  EXPECT_EQ(ColorGetR(skity::PMColorToColor(srgb_result)), ColorGetR(clamped));
  EXPECT_EQ(ColorGetG(skity::PMColorToColor(srgb_result)), ColorGetG(clamped));
  EXPECT_EQ(ColorGetB(skity::PMColorToColor(srgb_result)), ColorGetB(clamped));
}

TEST(GradientColorSpace, LegacyBlendFilterKeepsSRGBSemantics) {
  // The legacy Color overload records in sRGB; resolving to any target
  // matches converting that sRGB color directly.
  auto filter = skity::ColorFilters::Blend(
      skity::ColorSetARGB(255, 200, 100, 50), skity::BlendMode::kSrcOver);

  skity::PMColor src =
      skity::ColorToPMColor(skity::ColorSetARGB(255, 255, 255, 255));
  auto result = filter->FilterColor(src, skity::ColorSpace::DisplayP3());
  auto expected = skity::ColorSpace{}.Convert(
      skity::Color4f{200.f / 255.f, 100.f / 255.f, 50.f / 255.f, 1.f},
      skity::ColorSpace::DisplayP3());
  auto clamped = skity::Color4fToColor(
      skity::Color4f{std::max(0.f, std::min(1.f, expected.r)),
                     std::max(0.f, std::min(1.f, expected.g)),
                     std::max(0.f, std::min(1.f, expected.b)), 1.f});
  EXPECT_EQ(ColorGetR(skity::PMColorToColor(result)), ColorGetR(clamped));
  EXPECT_EQ(ColorGetG(skity::PMColorToColor(result)), ColorGetG(clamped));
  EXPECT_EQ(ColorGetB(skity::PMColorToColor(result)), ColorGetB(clamped));
}
