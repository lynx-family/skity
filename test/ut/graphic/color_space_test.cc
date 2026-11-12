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
#include <type_traits>

#include "gtest/gtest.h"
#include "test/ut/graphic/color_space_test_util.hpp"

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

TEST(ColorSpace, NamedSpacesAreImmutable) {
  // The named factories hand out interned singletons shared by every
  // holder, so the handle type must make the object unwritable: a caller
  // must not be able to redefine, say, Display P3 for everyone else.
  static_assert(std::is_same_v<decltype(skity::ColorSpace::SRGB()),
                               std::shared_ptr<const skity::ColorSpace>>,
                "named factories must return immutable spaces");
  static_assert(
      std::is_same_v<decltype(skity::ColorSpace::MakeRGB(nullptr, nullptr)),
                     std::shared_ptr<const skity::ColorSpace>>,
      "custom spaces are handed out immutable too");
  auto p3 = skity::ColorSpace::DisplayP3();
  static_assert(!std::is_assignable_v<decltype(p3->transfer_fn[0]), float>,
                "transfer function parameters must be read-only");
  static_assert(!std::is_assignable_v<decltype(p3->gamut_xyz[0]), float>,
                "gamut matrix parameters must be read-only");
  static_assert(
      !std::is_assignable_v<decltype(p3->kind), skity::ColorSpace::Kind>,
      "kind must be read-only");
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

TEST(ColorSpace, DisplayP3GamutIsD50AdaptedFromItsPrimaries) {
  namespace util = color_space_test_util;
  auto p3 = skity::ColorSpace::DisplayP3();
  // Gamut matrices live in the D50 connection space, so RGB white maps to
  // the D50 white point, not to the P3 native D65 white.
  for (int row = 0; row < 3; row++) {
    float sum = p3->gamut_xyz[row * 3] + p3->gamut_xyz[row * 3 + 1] +
                p3->gamut_xyz[row * 3 + 2];
    EXPECT_NEAR(sum, static_cast<float>(util::kD50[row]), 1e-5f);
  }
  // The matrix matches an independent derivation from the published
  // chromaticities plus a Bradford adaptation to D50.
  auto reference = util::D50Gamut(util::kDisplayP3Chromaticities);
  for (int i = 0; i < 9; i++) {
    EXPECT_NEAR(p3->gamut_xyz[i], static_cast<float>(reference[i]), 1e-6f);
  }
  // Undoing the adaptation recovers the published primaries and white.
  auto native = util::NativeChromaticities(p3->gamut_xyz);
  EXPECT_NEAR(native.wx, 0.3127, 1e-5);
  EXPECT_NEAR(native.wy, 0.3290, 1e-5);
  EXPECT_NEAR(native.rx, 0.680, 1e-5);
  EXPECT_NEAR(native.ry, 0.320, 1e-5);
  EXPECT_NEAR(native.gx, 0.265, 1e-5);
  EXPECT_NEAR(native.gy, 0.690, 1e-5);
  EXPECT_NEAR(native.bx, 0.150, 1e-5);
  EXPECT_NEAR(native.by, 0.060, 1e-5);
}

TEST(ColorSpace, SRGBGamutIsD50AdaptedFromItsPrimaries) {
  namespace util = color_space_test_util;
  // The default sRGB constants come from the standard ICC sRGB profile,
  // whose colorants are already D50-adapted. Verifying that claim is what
  // makes the D50 convention trustworthy: the same adaptation that builds
  // Display P3 above must recover the published sRGB primaries here.
  skity::ColorSpace srgb{};
  for (int row = 0; row < 3; row++) {
    float sum = srgb.gamut_xyz[row * 3] + srgb.gamut_xyz[row * 3 + 1] +
                srgb.gamut_xyz[row * 3 + 2];
    EXPECT_NEAR(sum, static_cast<float>(util::kD50[row]), 2e-4f);
  }
  auto reference = util::D50Gamut(util::kSRGBChromaticities);
  for (int i = 0; i < 9; i++) {
    // The 16-bit fixed constants round the reference values slightly.
    EXPECT_NEAR(srgb.gamut_xyz[i], static_cast<float>(reference[i]), 2e-4f);
  }
  auto native = util::NativeChromaticities(srgb.gamut_xyz);
  EXPECT_NEAR(native.wx, 0.3127, 1e-4);
  EXPECT_NEAR(native.wy, 0.3290, 1e-4);
  EXPECT_NEAR(native.rx, 0.640, 1e-4);
  EXPECT_NEAR(native.ry, 0.330, 1e-4);
  EXPECT_NEAR(native.gx, 0.300, 1e-4);
  EXPECT_NEAR(native.gy, 0.600, 1e-4);
  EXPECT_NEAR(native.bx, 0.150, 1e-4);
  EXPECT_NEAR(native.by, 0.060, 1e-4);
}

TEST(ColorSpace, CrossGamutNeutralsStayNeutral) {
  // sRGB and Display P3 share the transfer curve and the native white point,
  // so neutral colors are the same values in both spaces. A reference-white
  // mix-up between the gamut matrices shows up here as a color cast on every
  // gray, which round-trip tests built on the same conversion cannot see.
  auto srgb = skity::ColorSpace::SRGB();
  auto p3 = skity::ColorSpace::DisplayP3();
  for (float v : {0.f, 0.25f, 0.5f, 0.75f, 1.f}) {
    skity::Color4f gray{v, v, v, 1.f};
    auto to_srgb = skity::ColorSpace::Convert(p3, gray, srgb);
    EXPECT_NEAR(to_srgb.r, v, 2e-4f);
    EXPECT_NEAR(to_srgb.g, v, 2e-4f);
    EXPECT_NEAR(to_srgb.b, v, 2e-4f);
    auto to_p3 = skity::ColorSpace::Convert(srgb, gray, p3);
    EXPECT_NEAR(to_p3.r, v, 2e-4f);
    EXPECT_NEAR(to_p3.g, v, 2e-4f);
    EXPECT_NEAR(to_p3.b, v, 2e-4f);
  }
}

TEST(ColorSpace, LinearP3ToSRGBMatchesPublishedMatrix) {
  // Linear transfer functions on both sides leave the pure gamut matrices,
  // so the P3 primaries convert through the published linear P3 -> sRGB
  // matrix. The residual (under 2e-4) comes from the ICC 16-bit rounding of
  // the sRGB constants.
  namespace util = color_space_test_util;
  const float linear_tf[7] = {1.f, 1.f, 0.f, 1.f, 0.f, 0.f, 0.f};
  auto linear_p3 = skity::ColorSpace::MakeRGB(
      linear_tf, skity::ColorSpace::DisplayP3()->gamut_xyz);
  ASSERT_NE(linear_p3, nullptr);
  const float kPublishedP3ToSRGB[9] = {
      1.2249402f,  -0.2249402f, 0.f,  //
      -0.042057f,  1.042057f,   0.f,  //
      -0.0196376f, -0.078636f,  1.0982736f,
  };
  const skity::Color4f primaries[3] = {
      {1.f, 0.f, 0.f, 1.f}, {0.f, 1.f, 0.f, 1.f}, {0.f, 0.f, 1.f, 1.f}};
  for (int i = 0; i < 3; i++) {
    auto out = skity::ColorSpace::Convert(linear_p3, primaries[i],
                                          skity::ColorSpace::SRGBLinear());
    for (int row = 0; row < 3; row++) {
      float expected = kPublishedP3ToSRGB[row * 3 + i];
      float actual = row == 0 ? out.r : (row == 1 ? out.g : out.b);
      EXPECT_NEAR(actual, expected, 2.5e-4f);
    }
  }
}

TEST(ColorSpace, ConvertSameSpaceIsIdentity) {
  skity::Color4f color{0.25f, 0.5f, 0.75f, 0.6f};
  for (auto space :
       {std::shared_ptr<const skity::ColorSpace>{},
        skity::ColorSpace::SRGBLinear(), skity::ColorSpace::DisplayP3()}) {
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

TEST(ColorSpace, PureGammaPreservesExtendedNegatives) {
  // Pure power curves (c == f == 0) must not collapse negative components
  // to zero: the P3 red primary leaves the sRGB + gamma 2.2 gamut negative,
  // and collapsing there breaks the round trip (it used to come back as
  // (1.0033, 0.2227, 0.1556) instead of (1, 0, 0)).
  float gamma22[7] = {2.2f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f};
  auto gamma_space =
      skity::ColorSpace::MakeRGB(gamma22, skity::ColorSpace::SRGB()->gamut_xyz);
  ASSERT_NE(gamma_space, nullptr);
  auto p3 = skity::ColorSpace::DisplayP3();

  auto to_gamma = skity::ColorSpace::Convert(
      p3, skity::Color4f{1.f, 0.f, 0.f, 1.f}, gamma_space);
  auto back = skity::ColorSpace::Convert(gamma_space, to_gamma, p3);
  EXPECT_NEAR(back.r, 1.f, 1e-4f);
  EXPECT_NEAR(back.g, 0.f, 1e-4f);
  EXPECT_NEAR(back.b, 0.f, 1e-4f);

  // Direct negative linear round trip: -0.1 must survive the gamma encode
  // and decode with its sign.
  auto encoded = skity::ColorSpace::Convert(
      skity::ColorSpace::SRGBLinear(), skity::Color4f{-0.1f, 0.f, 0.f, 1.f},
      gamma_space);
  EXPECT_LT(encoded.r, 0.f);
  auto decoded = skity::ColorSpace::Convert(gamma_space, encoded,
                                            skity::ColorSpace::SRGBLinear());
  EXPECT_NEAR(decoded.r, -0.1f, 1e-4f);
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
