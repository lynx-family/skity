// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/graphic/color_xform.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <skity/graphic/color_space.hpp>
#include <skity/io/pixmap.hpp>

#include "gtest/gtest.h"

namespace {

skity::ColorSpace Gamma22Space() {
  // Pure power curve (d == 0) over the sRGB gamut: only the transfer
  // function differs from sRGB.
  const float tf[7] = {2.2f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f};
  return *skity::ColorSpace::MakeRGB(tf, skity::ColorSpace{}.gamut_xyz);
}

skity::ColorSpace LinearP3Space() {
  const float tf[7] = {1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f};
  skity::ColorSpace p3 = skity::ColorSpace::DisplayP3();
  return *skity::ColorSpace::MakeRGB(tf, p3.gamut_xyz);
}

uint8_t ReadPixel(const skity::Pixmap& pm, uint32_t x, uint32_t y,
                  int channel) {
  const auto* row = static_cast<const uint8_t*>(pm.Addr()) + y * pm.RowBytes();
  return row[x * 4 + channel];
}

void WritePixel(skity::Pixmap* pm, uint32_t x, uint32_t y, int channel,
                uint8_t v) {
  auto* row = static_cast<uint8_t*>(pm->WritableAddr()) + y * pm->RowBytes();
  row[x * 4 + channel] = v;
}

uint8_t Quantize8(float v) {
  return static_cast<uint8_t>(std::clamp(v * 255.f, 0.f, 255.f));
}

// Byte comparison with a +-1 tolerance for float quantization boundaries.
void ExpectBytesNear(uint8_t expected, uint8_t actual) {
  EXPECT_LE(std::abs(static_cast<int>(expected) - actual), 1);
}

}  // namespace

TEST(ColorXformSteps, SameSpaceFoldsToNoop) {
  auto steps =
      skity::ColorXformSteps::Resolve(skity::ColorSpace{}, skity::ColorSpace{});
  EXPECT_TRUE(steps.IsNoop());

  steps = skity::ColorXformSteps::Resolve(skity::ColorSpace::DisplayP3(),
                                          skity::ColorSpace::DisplayP3());
  EXPECT_TRUE(steps.IsNoop());
}

TEST(ColorXformSteps, StageFolding) {
  // sRGB -> P3: all three stages (shared curve, different gamut).
  auto steps = skity::ColorXformSteps::Resolve(skity::ColorSpace{},
                                               skity::ColorSpace::DisplayP3());
  EXPECT_TRUE(steps.decode);
  EXPECT_TRUE(steps.gamut);
  EXPECT_TRUE(steps.encode);
  EXPECT_TRUE(steps.encode_is_srgb_curve);
  EXPECT_FALSE(steps.IsNoop());

  // sRGB -> linear sRGB: decode only (same gamut, linear dst).
  steps = skity::ColorXformSteps::Resolve(skity::ColorSpace{},
                                          skity::ColorSpace::SRGBLinear());
  EXPECT_TRUE(steps.decode);
  EXPECT_FALSE(steps.gamut);
  EXPECT_FALSE(steps.encode);

  // Linear sRGB -> sRGB: encode only, with the constant sRGB curve.
  steps = skity::ColorXformSteps::Resolve(skity::ColorSpace::SRGBLinear(),
                                          skity::ColorSpace{});
  EXPECT_FALSE(steps.decode);
  EXPECT_FALSE(steps.gamut);
  EXPECT_TRUE(steps.encode);
  EXPECT_TRUE(steps.encode_is_srgb_curve);

  // Linear P3 -> linear sRGB: gamut only.
  steps = skity::ColorXformSteps::Resolve(LinearP3Space(),
                                          skity::ColorSpace::SRGBLinear());
  EXPECT_FALSE(steps.decode);
  EXPECT_TRUE(steps.gamut);
  EXPECT_FALSE(steps.encode);

  // sRGB -> gamma 2.2 (same gamut): parametric encode, not the constant
  // sRGB curve, and the gamut stage folds.
  steps = skity::ColorXformSteps::Resolve(skity::ColorSpace{}, Gamma22Space());
  EXPECT_TRUE(steps.decode);
  EXPECT_FALSE(steps.gamut);
  EXPECT_TRUE(steps.encode);
  EXPECT_FALSE(steps.encode_is_srgb_curve);
}

TEST(ColorXformSteps, ApplyReferenceValues) {
  // The sRGB decode of a 0.5 signal is 0.21404114...
  auto to_linear = skity::ColorXformSteps::Resolve(
      skity::ColorSpace{}, skity::ColorSpace::SRGBLinear());
  auto out = to_linear.Apply(skity::Color4f{0.5f, 0.5f, 0.5f, 0.25f});
  EXPECT_NEAR(out.r, 0.21404114f, 1e-6f);
  EXPECT_NEAR(out.g, 0.21404114f, 1e-6f);
  EXPECT_NEAR(out.b, 0.21404114f, 1e-6f);
  // Alpha passes through untouched.
  EXPECT_EQ(out.a, 0.25f);

  auto to_srgb = skity::ColorXformSteps::Resolve(
      skity::ColorSpace::SRGBLinear(), skity::ColorSpace{});
  out =
      to_srgb.Apply(skity::Color4f{0.21404114f, 0.21404114f, 0.21404114f, 1.f});
  EXPECT_NEAR(out.r, 0.5f, 1e-6f);
}

TEST(ColorXformSteps, ApplyDoesNotClampOutOfGamut) {
  // Display P3 red converts outside the sRGB box; clipping belongs to the
  // quantization stage, not the transform. (Exact out-of-gamut magnitudes
  // depend on the ICC-derived sRGB matrix constants, so assert signs.)
  auto steps = skity::ColorXformSteps::Resolve(skity::ColorSpace::DisplayP3(),
                                               skity::ColorSpace{});
  auto out = steps.Apply(skity::Color4f{1.f, 0.f, 0.f, 1.f});
  EXPECT_GT(out.r, 1.f);
  EXPECT_LT(out.g, 0.f);
  EXPECT_LT(out.b, 0.f);
}

TEST(ColorXformSteps, RoundTrip) {
  const skity::Color4f color{0.75f, 0.3f, 0.1f, 0.9f};
  auto to_srgb = skity::ColorXformSteps::Resolve(skity::ColorSpace::DisplayP3(),
                                                 skity::ColorSpace{});
  auto to_p3 = skity::ColorXformSteps::Resolve(skity::ColorSpace{},
                                               skity::ColorSpace::DisplayP3());
  auto round = to_p3.Apply(to_srgb.Apply(color));
  EXPECT_NEAR(round.r, color.r, 1e-5f);
  EXPECT_NEAR(round.g, color.g, 1e-5f);
  EXPECT_NEAR(round.b, color.b, 1e-5f);
  EXPECT_EQ(round.a, color.a);
}

TEST(ColorXformSteps, ApplyColorsMatchesApply) {
  skity::Color4f colors[] = {
      {0.1f, 0.2f, 0.3f, 1.f}, {0.5f, 0.5f, 0.5f, 0.5f}, {1.f, 0.f, 0.f, 0.f}};
  skity::Color4f bulk[] = {colors[0], colors[1], colors[2]};
  auto steps = skity::ColorXformSteps::Resolve(skity::ColorSpace::DisplayP3(),
                                               skity::ColorSpace{});
  steps.ApplyColors(bulk, 3);
  for (int i = 0; i < 3; i++) {
    auto one = steps.Apply(colors[i]);
    EXPECT_EQ(bulk[i].r, one.r);
    EXPECT_EQ(bulk[i].g, one.g);
    EXPECT_EQ(bulk[i].b, one.b);
    EXPECT_EQ(bulk[i].a, one.a);
  }
}

TEST(TransformPixels, NoopSameLayoutIsByteExact) {
  skity::Pixmap src(4, 2, skity::kPremul_AlphaType, skity::ColorType::kRGBA);
  skity::Pixmap dst(4, 2, skity::kPremul_AlphaType, skity::ColorType::kRGBA);
  // Arbitrary premultiplied bytes, including partial alphas.
  for (uint32_t i = 0; i < 8; i++) {
    for (int c = 0; c < 4; c++) {
      WritePixel(&src, i % 4, i / 4, c, static_cast<uint8_t>(i * 31 + c * 17));
    }
  }
  auto steps =
      skity::ColorXformSteps::Resolve(skity::ColorSpace{}, skity::ColorSpace{});
  ASSERT_TRUE(skity::TransformPixels(src, &dst, steps));
  EXPECT_EQ(0, std::memcmp(src.Addr(), dst.Addr(), 4 * 4 * 2));
}

TEST(TransformPixels, NoopSwizzlesLayout) {
  skity::Pixmap src(2, 1, skity::kUnpremul_AlphaType, skity::ColorType::kRGBA);
  skity::Pixmap dst(2, 1, skity::kUnpremul_AlphaType, skity::ColorType::kBGRA);
  WritePixel(&src, 0, 0, 0, 10);  // R
  WritePixel(&src, 0, 0, 1, 20);  // G
  WritePixel(&src, 0, 0, 2, 30);  // B
  WritePixel(&src, 0, 0, 3, 40);  // A
  auto steps =
      skity::ColorXformSteps::Resolve(skity::ColorSpace{}, skity::ColorSpace{});
  ASSERT_TRUE(skity::TransformPixels(src, &dst, steps));
  // BGRA byte order: B=30, G=20, R=10, A=40.
  EXPECT_EQ(30, ReadPixel(dst, 0, 0, 0));
  EXPECT_EQ(20, ReadPixel(dst, 0, 0, 1));
  EXPECT_EQ(10, ReadPixel(dst, 0, 0, 2));
  EXPECT_EQ(40, ReadPixel(dst, 0, 0, 3));
}

TEST(TransformPixels, NoopConvertsAlphaConvention) {
  // Unpremul -> premul with a no-op transform: 255/128/64 @ alpha 128.
  skity::Pixmap src(1, 1, skity::kUnpremul_AlphaType, skity::ColorType::kRGBA);
  skity::Pixmap dst(1, 1, skity::kPremul_AlphaType, skity::ColorType::kRGBA);
  WritePixel(&src, 0, 0, 0, 255);
  WritePixel(&src, 0, 0, 1, 128);
  WritePixel(&src, 0, 0, 2, 64);
  WritePixel(&src, 0, 0, 3, 128);
  auto steps =
      skity::ColorXformSteps::Resolve(skity::ColorSpace{}, skity::ColorSpace{});
  ASSERT_TRUE(skity::TransformPixels(src, &dst, steps));
  EXPECT_EQ(128, ReadPixel(dst, 0, 0, 0));
  EXPECT_EQ(64, ReadPixel(dst, 0, 0, 1));
  EXPECT_EQ(32, ReadPixel(dst, 0, 0, 2));
  EXPECT_EQ(128, ReadPixel(dst, 0, 0, 3));
}

TEST(TransformPixels, ConversionMatchesStepsApply) {
  // P3 -> sRGB on unpremul pixels: the buffer kernel agrees with Apply()
  // up to quantization.
  skity::Pixmap src(1, 1, skity::kUnpremul_AlphaType, skity::ColorType::kRGBA);
  skity::Pixmap dst(1, 1, skity::kUnpremul_AlphaType, skity::ColorType::kRGBA);
  WritePixel(&src, 0, 0, 0, 255);  // P3 red.
  WritePixel(&src, 0, 0, 1, 128);
  WritePixel(&src, 0, 0, 2, 0);
  WritePixel(&src, 0, 0, 3, 255);
  auto steps = skity::ColorXformSteps::Resolve(skity::ColorSpace::DisplayP3(),
                                               skity::ColorSpace{});
  ASSERT_TRUE(skity::TransformPixels(src, &dst, steps));
  auto expected = steps.Apply(skity::Color4f{1.f, 128.f / 255.f, 0.f, 1.f});
  // Out-of-gamut channels clamp at quantization.
  EXPECT_EQ(Quantize8(expected.r), ReadPixel(dst, 0, 0, 0));
  EXPECT_EQ(Quantize8(expected.g), ReadPixel(dst, 0, 0, 1));
  EXPECT_EQ(Quantize8(expected.b), ReadPixel(dst, 0, 0, 2));
  EXPECT_EQ(255, ReadPixel(dst, 0, 0, 3));
}

TEST(TransformPixels, PremulPixelsRoundTripThroughTransform) {
  // Premul P3 -> premul sRGB: divide, transform, multiply back.
  skity::Pixmap src(1, 1, skity::kPremul_AlphaType, skity::ColorType::kRGBA);
  skity::Pixmap dst(1, 1, skity::kPremul_AlphaType, skity::ColorType::kRGBA);
  WritePixel(&src, 0, 0, 0, 102);
  WritePixel(&src, 0, 0, 1, 51);
  WritePixel(&src, 0, 0, 2, 25);
  WritePixel(&src, 0, 0, 3, 128);
  auto steps = skity::ColorXformSteps::Resolve(skity::ColorSpace::DisplayP3(),
                                               skity::ColorSpace{});
  ASSERT_TRUE(skity::TransformPixels(src, &dst, steps));
  auto expected = steps.Apply(
      skity::Color4f{102.f / 128.f, 51.f / 128.f, 25.f / 128.f, 128.f / 255.f});
  ExpectBytesNear(Quantize8(expected.r * expected.a), ReadPixel(dst, 0, 0, 0));
  ExpectBytesNear(Quantize8(expected.g * expected.a), ReadPixel(dst, 0, 0, 1));
  ExpectBytesNear(Quantize8(expected.b * expected.a), ReadPixel(dst, 0, 0, 2));
  // The alpha channel carries through premul round trips byte exactly.
  EXPECT_EQ(128, ReadPixel(dst, 0, 0, 3));
}

TEST(TransformPixels, AlphaOnlyAndUnsupported) {
  skity::Pixmap a8_src(2, 1, skity::kPremul_AlphaType, skity::ColorType::kA8);
  skity::Pixmap a8_dst(2, 1, skity::kPremul_AlphaType, skity::ColorType::kA8);
  static_cast<uint8_t*>(a8_src.WritableAddr())[0] = 7;
  static_cast<uint8_t*>(a8_src.WritableAddr())[1] = 250;
  auto steps = skity::ColorXformSteps::Resolve(skity::ColorSpace::DisplayP3(),
                                               skity::ColorSpace{});
  ASSERT_TRUE(skity::TransformPixels(a8_src, &a8_dst, steps));
  EXPECT_EQ(7, static_cast<const uint8_t*>(a8_dst.Addr())[0]);
  EXPECT_EQ(250, static_cast<const uint8_t*>(a8_dst.Addr())[1]);

  // Mismatched dimensions and 16-bit formats are rejected.
  skity::Pixmap wide(2, 1, skity::kPremul_AlphaType, skity::ColorType::kRGBA);
  skity::Pixmap narrow(1, 1, skity::kPremul_AlphaType, skity::ColorType::kRGBA);
  EXPECT_FALSE(skity::TransformPixels(wide, &narrow, steps));

  skity::Pixmap rgb565(1, 1, skity::kOpaque_AlphaType,
                       skity::ColorType::kRGB565);
  skity::Pixmap rgba(1, 1, skity::kPremul_AlphaType, skity::ColorType::kRGBA);
  EXPECT_FALSE(skity::TransformPixels(rgb565, &rgba, steps));
  EXPECT_FALSE(skity::TransformPixels(rgba, &rgb565, steps));
}
