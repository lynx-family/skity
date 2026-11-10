// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <cmath>
#include <cstring>
#include <memory>
#include <skity/graphic/color_space.hpp>
#include <skity/graphic/image.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/io/pixmap.hpp>

#include "gtest/gtest.h"

namespace {

// Decodes an sRGB signal value with the reference piecewise formula.
float SRGBSignalToLinear(float x) {
  if (x <= 0.04045f) {
    return x / 12.92f;
  }
  return std::pow((x + 0.055f) / 1.055f, 2.4f);
}

}  // namespace

TEST(ColorSpace, DefaultsAreSRGB) {
  // The struct itself still defaults to sRGB bytes; a null shared_ptr means
  // "undeclared", which is also sRGB.
  skity::ColorSpace cs{};
  EXPECT_TRUE(cs.IsSRGB());
  EXPECT_FALSE(cs.IsLinear());
  EXPECT_EQ(cs.kind, skity::ColorSpace::Kind::kSRGB);
  EXPECT_EQ(cs, *skity::ColorSpace::SRGB());
  EXPECT_TRUE(skity::ColorSpaceIsSRGB(nullptr));
}

TEST(ColorSpace, NamedFactories) {
  auto linear = skity::ColorSpace::SRGBLinear();
  EXPECT_TRUE(linear->IsLinear());
  EXPECT_FALSE(linear->IsSRGB());
  EXPECT_EQ(linear->kind, skity::ColorSpace::Kind::kSRGBLinear);

  auto p3 = skity::ColorSpace::DisplayP3();
  // Display P3 shares the sRGB transfer curve but has a wider gamut.
  EXPECT_FALSE(p3->IsSRGB());
  EXPECT_FALSE(p3->IsLinear());
  EXPECT_EQ(p3->kind, skity::ColorSpace::Kind::kDisplayP3);

  // Named factories are interned singletons: same pointer on every call.
  EXPECT_EQ(skity::ColorSpace::DisplayP3(), p3);
  EXPECT_NE(skity::ColorSpace::SRGB(), linear);
  EXPECT_NE(skity::ColorSpace::SRGB(), p3);
}

TEST(ColorSpace, DisplayP3GamutMatchesPrimariesAndWhitePoint) {
  auto p3 = skity::ColorSpace::DisplayP3();
  // Each row of an RGB->XYZ matrix sums to the white point (D65 for P3).
  for (int row = 0; row < 3; row++) {
    float sum = p3->gamut_xyz[row * 3] + p3->gamut_xyz[row * 3 + 1] +
                p3->gamut_xyz[row * 3 + 2];
    float d65 = row == 0 ? 0.950456f : (row == 1 ? 1.f : 1.089058f);
    EXPECT_NEAR(sum, d65, 1e-4f);
  }
  // The red primary column converts back to xy (0.680, 0.320).
  float x = p3->gamut_xyz[0], y = p3->gamut_xyz[3], z = p3->gamut_xyz[6];
  float sum = x + y + z;
  EXPECT_NEAR(x / sum, 0.680f, 1e-4f);
  EXPECT_NEAR(y / sum, 0.320f, 1e-4f);
}

TEST(ColorSpace, ConvertSameSpaceIsIdentity) {
  skity::Color4f color{0.25f, 0.5f, 0.75f, 0.6f};
  for (auto space :
       {std::shared_ptr<skity::ColorSpace>{}, skity::ColorSpace::SRGBLinear(),
        skity::ColorSpace::DisplayP3()}) {
    auto out = skity::ColorSpace::Convert(space, color, space);
    EXPECT_EQ(out.r, color.r);
    EXPECT_EQ(out.g, color.g);
    EXPECT_EQ(out.b, color.b);
    EXPECT_EQ(out.a, color.a);
  }
}

TEST(ColorSpace, ConvertSRGBToLinear) {
  auto srgb = skity::ColorSpace::SRGB();
  auto linear = skity::ColorSpace::SRGBLinear();

  // 0.5 gray: sRGB 0.5 encodes the linear value 0.21404114.
  skity::Color4f half{0.5f, 0.5f, 0.5f, 1.f};
  auto out = skity::ColorSpace::Convert(srgb, half, linear);
  EXPECT_NEAR(out.r, 0.21404114f, 1e-5f);
  EXPECT_NEAR(out.g, 0.21404114f, 1e-5f);
  EXPECT_NEAR(out.b, 0.21404114f, 1e-5f);
  EXPECT_EQ(out.a, 1.f);

  // A null source space is the sRGB working space.
  auto null_src = skity::ColorSpace::Convert(nullptr, half, linear);
  EXPECT_NEAR(null_src.r, out.r, 1e-7f);

  // The transfer curve matches the reference piecewise formula pointwise.
  for (int i = 0; i <= 255; i++) {
    float signal = i / 255.f;
    auto c = skity::ColorSpace::Convert(
        srgb, skity::Color4f{signal, signal, signal, 1.f}, linear);
    EXPECT_NEAR(c.r, SRGBSignalToLinear(signal), 1e-5f);
  }
}

TEST(ColorSpace, ConvertLinearToSRGB) {
  auto srgb = skity::ColorSpace::SRGB();
  auto linear = skity::ColorSpace::SRGBLinear();

  // Linear 0.5 encodes to sRGB 0.73535698 (the inverse curve). A null
  // destination is the sRGB working space.
  skity::Color4f half{0.5f, 0.5f, 0.5f, 1.f};
  auto out = skity::ColorSpace::Convert(linear, half, nullptr);
  EXPECT_NEAR(out.r, 0.73535698f, 1e-4f);

  // Round trip both ways.
  for (int i = 0; i <= 255; i++) {
    float signal = i / 255.f;
    skity::Color4f c{signal, signal, signal, 0.4f};
    auto round = skity::ColorSpace::Convert(
        linear, skity::ColorSpace::Convert(srgb, c, linear), srgb);
    EXPECT_NEAR(round.r, signal, 2e-3f);
    EXPECT_EQ(round.a, 0.4f);
  }
}

TEST(ColorSpace, ConvertAlphaIsUntouched) {
  auto srgb = skity::ColorSpace::SRGB();
  auto linear = skity::ColorSpace::SRGBLinear();
  skity::Color4f color{0.2f, 0.4f, 0.8f, 0.35f};
  auto out = skity::ColorSpace::Convert(srgb, color, linear);
  EXPECT_EQ(out.a, 0.35f);
  // The curve is applied per channel regardless of alpha (unpremul domain).
  EXPECT_NEAR(out.g, SRGBSignalToLinear(0.4f), 1e-5f);
}

TEST(ColorSpace, ConvertSRGBAndDisplayP3RoundTrip) {
  auto srgb = skity::ColorSpace::SRGB();
  auto p3 = skity::ColorSpace::DisplayP3();

  for (int i = 0; i <= 255; i += 5) {
    float signal = i / 255.f;
    // Both spaces share the sRGB transfer curve, so only the gamut matrices
    // are exercised here.
    skity::Color4f color{signal, 1.f - signal, 0.25f, 1.f};
    auto in_p3 = skity::ColorSpace::Convert(srgb, color, p3);
    auto round = skity::ColorSpace::Convert(p3, in_p3, srgb);
    EXPECT_NEAR(round.r, signal, 1e-4f);
    EXPECT_NEAR(round.g, 1.f - signal, 1e-4f);
    EXPECT_NEAR(round.b, 0.25f, 1e-4f);
  }
}

TEST(ColorSpace, ConvertOutOfGamutIsNotClamped) {
  auto srgb = skity::ColorSpace::SRGB();
  auto p3 = skity::ColorSpace::DisplayP3();
  // The P3 red primary is outside the sRGB gamut.
  auto out =
      skity::ColorSpace::Convert(p3, skity::Color4f{1.f, 0.f, 0.f, 1.f}, srgb);
  EXPECT_GT(out.r, 1.f);
  EXPECT_LT(out.g, 0.f);
  EXPECT_LT(out.b, 0.f);
}

TEST(ColorSpace, MakeRGBValidation) {
  float identity_gamut[9] = {
      1.f, 0.f, 0.f,  //
      0.f, 1.f, 0.f,  //
      0.f, 0.f, 1.f,
  };

  // A plain gamma 2.2 curve is valid (pure power, d == 0).
  float gamma22[7] = {2.2f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f};
  auto valid = skity::ColorSpace::MakeRGB(gamma22, identity_gamut);
  ASSERT_NE(valid, nullptr);
  EXPECT_EQ(valid->kind, skity::ColorSpace::Kind::kParametric);
  EXPECT_FALSE(valid->IsLinear());
  EXPECT_FALSE(valid->IsSRGB());

  // Round trips through a parametric space.
  auto srgb = skity::ColorSpace::SRGB();
  skity::Color4f color{0.3f, 0.6f, 0.9f, 1.f};
  auto round = skity::ColorSpace::Convert(
      valid, skity::ColorSpace::Convert(srgb, color, valid), srgb);
  EXPECT_NEAR(round.r, color.r, 1e-4f);

  // Invalid parameter sets are rejected with a null result.
  float bad_gamma[7] = {-2.2f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f};
  EXPECT_EQ(skity::ColorSpace::MakeRGB(bad_gamma, identity_gamut), nullptr);
  float bad_linear_segment[7] = {2.2f, 1.f, 0.f, 0.f, 0.5f, 0.f, 0.f};
  EXPECT_EQ(skity::ColorSpace::MakeRGB(bad_linear_segment, identity_gamut),
            nullptr);
  float singular_gamut[9] = {
      1.f, 0.f, 0.f,  //
      1.f, 0.f, 0.f,  //
      0.f, 0.f, 1.f,
  };
  EXPECT_EQ(skity::ColorSpace::MakeRGB(gamma22, singular_gamut), nullptr);
  EXPECT_EQ(skity::ColorSpace::MakeRGB(nullptr, identity_gamut), nullptr);
  EXPECT_EQ(skity::ColorSpace::MakeRGB(gamma22, nullptr), nullptr);
}

TEST(ColorSpace, EqualityIsNumericAndIgnoresKind) {
  auto srgb = skity::ColorSpace::SRGB();
  float srgb_gamut[9];
  float srgb_tf[7];
  std::memcpy(srgb_gamut, srgb->gamut_xyz, sizeof(srgb_gamut));
  std::memcpy(srgb_tf, srgb->transfer_fn, sizeof(srgb_tf));

  auto parametric = skity::ColorSpace::MakeRGB(srgb_tf, srgb_gamut);
  ASSERT_NE(parametric, nullptr);
  EXPECT_EQ(parametric->kind, skity::ColorSpace::Kind::kParametric);
  // Numerically identical spaces are equal even when built differently, and
  // the null-tolerant comparison folds null into the sRGB working space.
  EXPECT_NE(parametric, srgb);
  EXPECT_TRUE(*parametric == *srgb);
  EXPECT_TRUE(skity::ColorSpaceEquals(parametric, srgb));
  EXPECT_TRUE(skity::ColorSpaceEquals(nullptr, srgb));
  EXPECT_TRUE(skity::ColorSpaceEquals(nullptr, parametric));
  EXPECT_FALSE(
      skity::ColorSpaceEquals(nullptr, skity::ColorSpace::DisplayP3()));
  EXPECT_TRUE(parametric->IsSRGB());
}

TEST(ColorSpace, ConvertColorsBulk) {
  auto srgb = skity::ColorSpace::SRGB();
  auto linear = skity::ColorSpace::SRGBLinear();
  skity::Color4f colors[3] = {
      {0.f, 0.f, 0.f, 1.f}, {0.5f, 0.5f, 0.5f, 1.f}, {1.f, 1.f, 1.f, 1.f}};
  skity::ColorSpace::ConvertColors(srgb, colors, 3, linear);
  EXPECT_NEAR(colors[0].r, 0.f, 1e-6f);
  EXPECT_NEAR(colors[1].r, 0.21404114f, 1e-5f);
  EXPECT_NEAR(colors[2].r, 1.f, 1e-6f);
}

TEST(ColorSpace, PixmapColorSpaceMetadata) {
  skity::Pixmap pixmap(4, 4);
  // No space declared: the pixmap starts out null (= sRGB).
  EXPECT_EQ(pixmap.GetColorSpace(), nullptr);

  auto p3 = skity::ColorSpace::DisplayP3();
  EXPECT_TRUE(pixmap.SetColorInfo(skity::kUnpremul_AlphaType,
                                  skity::ColorType::kRGBA, p3));
  EXPECT_EQ(pixmap.GetColorSpace(), p3);

  // The two parameter variant keeps working and resets the space back to
  // undeclared (sRGB).
  EXPECT_TRUE(
      pixmap.SetColorInfo(skity::kUnpremul_AlphaType, skity::ColorType::kRGBA));
  EXPECT_EQ(pixmap.GetColorSpace(), nullptr);
  EXPECT_TRUE(skity::ColorSpaceIsSRGB(pixmap.GetColorSpace()));

  // Invalid alpha/color type combinations are still rejected.
  EXPECT_FALSE(pixmap.SetColorInfo(skity::kUnknown_AlphaType,
                                   skity::ColorType::kRGBA, p3));
}

TEST(ColorSpace, PixmapImageCarriesColorSpace) {
  auto pixmap = std::make_shared<skity::Pixmap>(2, 2);
  ASSERT_TRUE(pixmap->SetColorInfo(skity::kUnpremul_AlphaType,
                                   skity::ColorType::kRGBA,
                                   skity::ColorSpace::SRGBLinear()));
  auto image = skity::Image::MakeImage(pixmap);
  EXPECT_TRUE(image->GetColorSpace()->IsLinear());

  auto plain = skity::Image::MakeImage(std::make_shared<skity::Pixmap>(2, 2));
  EXPECT_EQ(plain->GetColorSpace(), nullptr);
  EXPECT_TRUE(skity::ColorSpaceIsSRGB(plain->GetColorSpace()));
}

TEST(ColorSpace, PaintSetColor4fConvertsToWorkingSpace) {
  skity::Paint paint;
  paint.SetColor4f(skity::Color4f{0.5f, 0.5f, 0.5f, 1.f},
                   skity::ColorSpace::SRGBLinear());
  // Linear 0.5 gray drawn as working-space sRGB is 0.73536.
  auto color = paint.GetColor4f();
  EXPECT_NEAR(color.r, 0.73535698f, 1e-4f);
  EXPECT_NEAR(color.g, 0.73535698f, 1e-4f);
  EXPECT_NEAR(color.b, 0.73535698f, 1e-4f);
  EXPECT_NEAR(color.a, 1.f, 1e-6f);

  // The default (null) space leaves the color untouched.
  paint.SetColor4f(skity::Color4f{0.5f, 0.25f, 0.125f, 1.f});
  color = paint.GetColor4f();
  EXPECT_EQ(color.r, 0.5f);
  EXPECT_EQ(color.g, 0.25f);
  EXPECT_EQ(color.b, 0.125f);

  // An sRGB color declared through a P3 tag converts through the gamut.
  paint.SetColor4f(skity::Color4f{0.5f, 0.25f, 0.125f, 1.f},
                   skity::ColorSpace::DisplayP3());
  color = paint.GetColor4f();
  auto expected = skity::ColorSpace::Convert(
      skity::ColorSpace::DisplayP3(), skity::Color4f{0.5f, 0.25f, 0.125f, 1.f},
      nullptr);
  EXPECT_NEAR(color.r, expected.r, 1e-6f);
  EXPECT_NEAR(color.g, expected.g, 1e-6f);
  EXPECT_NEAR(color.b, expected.b, 1e-6f);
}
