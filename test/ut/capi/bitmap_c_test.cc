// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>
#include <skity/c/skity_bitmap.h>

#include <cstring>

namespace {

skity_color_space_info LinearInfo() {
  skity_color_space_info info{};
  info.kind = SKITY_COLOR_SPACE_SRGB_LINEAR;
  return info;
}

skity_color_space_info Gamma22Info() {
  skity_color_space_info info{};
  info.kind = SKITY_COLOR_SPACE_PARAMETRIC;
  info.transfer_fn[0] = 2.2f;  // g; the rest stays zero (pure power)
  info.transfer_fn[1] = 1.f;
  float identity_gamut[9] = {
      1.f, 0.f, 0.f,  //
      0.f, 1.f, 0.f,  //
      0.f, 0.f, 1.f,
  };
  std::memcpy(info.gamut_xyz, identity_gamut, sizeof(identity_gamut));
  return info;
}

}  // namespace

TEST(CapiPixmapColorSpace, NamedKindsUseCanonicalValues) {
  skity_pixmap pixmap = skity_pixmap_create_alloc(
      2, 2, SKITY_ALPHA_TYPE_UNPREMUL, SKITY_COLOR_TYPE_RGBA);
  ASSERT_NE(pixmap, nullptr);

  // Default color space is sRGB.
  skity_color_space_info out{};
  ASSERT_EQ(skity_pixmap_get_color_space(pixmap, &out), SKITY_SUCCESS);
  EXPECT_EQ(out.kind, SKITY_COLOR_SPACE_SRGB);
  EXPECT_GT(out.gamut_xyz[0], 0.f);

  // Named kinds ignore the arrays and store canonical values.
  skity_color_space_info info{};
  info.kind = SKITY_COLOR_SPACE_DISPLAY_P3;
  info.transfer_fn[0] = 123.f;  // garbage must not leak through
  ASSERT_EQ(skity_pixmap_set_color_space(pixmap, &info), SKITY_SUCCESS);
  ASSERT_EQ(skity_pixmap_get_color_space(pixmap, &out), SKITY_SUCCESS);
  EXPECT_EQ(out.kind, SKITY_COLOR_SPACE_DISPLAY_P3);
  EXPECT_NEAR(out.transfer_fn[0], 2.4f, 1e-5f);

  info.kind = SKITY_COLOR_SPACE_SRGB_LINEAR;
  ASSERT_EQ(skity_pixmap_set_color_space(pixmap, &info), SKITY_SUCCESS);
  ASSERT_EQ(skity_pixmap_get_color_space(pixmap, &out), SKITY_SUCCESS);
  EXPECT_EQ(out.kind, SKITY_COLOR_SPACE_SRGB_LINEAR);
  EXPECT_EQ(out.transfer_fn[0], 1.f);

  info.kind = SKITY_COLOR_SPACE_ADOBE_RGB;
  ASSERT_EQ(skity_pixmap_set_color_space(pixmap, &info), SKITY_SUCCESS);
  ASSERT_EQ(skity_pixmap_get_color_space(pixmap, &out), SKITY_SUCCESS);
  EXPECT_EQ(out.kind, SKITY_COLOR_SPACE_ADOBE_RGB);
  // Adobe RGB carries the pure gamma 2.2 curve.
  EXPECT_NEAR(out.transfer_fn[0], 2.2f, 1e-5f);
  EXPECT_EQ(out.transfer_fn[1], 1.f);

  skity_pixmap_destroy(pixmap);
}

TEST(CapiPixmapColorSpace, ParametricRoundTripAndValidation) {
  skity_pixmap pixmap = skity_pixmap_create_alloc(
      2, 2, SKITY_ALPHA_TYPE_UNPREMUL, SKITY_COLOR_TYPE_RGBA);
  ASSERT_NE(pixmap, nullptr);

  skity_color_space_info gamma22 = Gamma22Info();
  ASSERT_EQ(skity_pixmap_set_color_space(pixmap, &gamma22), SKITY_SUCCESS);
  skity_color_space_info out{};
  ASSERT_EQ(skity_pixmap_get_color_space(pixmap, &out), SKITY_SUCCESS);
  EXPECT_EQ(out.kind, SKITY_COLOR_SPACE_PARAMETRIC);
  EXPECT_EQ(out.transfer_fn[0], 2.2f);
  EXPECT_EQ(out.gamut_xyz[0], 1.f);
  EXPECT_EQ(out.gamut_xyz[4], 1.f);

  // Invalid parametric descriptions are rejected and change nothing.
  skity_color_space_info bad = Gamma22Info();
  bad.transfer_fn[0] = -2.2f;  // negative gamma
  EXPECT_EQ(skity_pixmap_set_color_space(pixmap, &bad),
            SKITY_ERROR_INVALID_ARGUMENT);
  ASSERT_EQ(skity_pixmap_get_color_space(pixmap, &out), SKITY_SUCCESS);
  EXPECT_EQ(out.transfer_fn[0], 2.2f);

  bad = Gamma22Info();
  float singular_gamut[9] = {
      1.f, 0.f, 0.f,  //
      1.f, 0.f, 0.f,  //
      0.f, 0.f, 1.f,
  };
  std::memcpy(bad.gamut_xyz, singular_gamut, sizeof(singular_gamut));
  EXPECT_EQ(skity_pixmap_set_color_space(pixmap, &bad),
            SKITY_ERROR_INVALID_ARGUMENT);

  // Null arguments.
  EXPECT_EQ(skity_pixmap_set_color_space(pixmap, nullptr),
            SKITY_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(skity_pixmap_get_color_space(pixmap, nullptr),
            SKITY_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(skity_pixmap_set_color_space(nullptr, &gamma22),
            SKITY_ERROR_INVALID_HANDLE);
  EXPECT_EQ(skity_pixmap_get_color_space(nullptr, &out),
            SKITY_ERROR_INVALID_HANDLE);

  skity_pixmap_destroy(pixmap);
}

TEST(CapiPixmapColorSpace, SetColorInfoResetsToSRGB) {
  skity_pixmap pixmap = skity_pixmap_create_alloc(
      2, 2, SKITY_ALPHA_TYPE_UNPREMUL, SKITY_COLOR_TYPE_RGBA);
  ASSERT_NE(pixmap, nullptr);

  skity_color_space_info linear = LinearInfo();
  ASSERT_EQ(skity_pixmap_set_color_space(pixmap, &linear), SKITY_SUCCESS);
  // The two-parameter color info update resets the space to sRGB, matching
  // the C++ Pixmap semantics.
  ASSERT_EQ(skity_pixmap_set_color_info(pixmap, SKITY_ALPHA_TYPE_UNPREMUL,
                                        SKITY_COLOR_TYPE_RGBA),
            SKITY_SUCCESS);
  skity_color_space_info out{};
  ASSERT_EQ(skity_pixmap_get_color_space(pixmap, &out), SKITY_SUCCESS);
  EXPECT_EQ(out.kind, SKITY_COLOR_SPACE_SRGB);

  skity_pixmap_destroy(pixmap);
}

TEST(CapiColorSpaceMath, ChromaticityGamutRoundTrip) {
  // Display P3 primaries in their native D65 white point.
  const float p3[8] = {0.3127f, 0.3290f, 0.680f, 0.320f,
                       0.265f,  0.690f,  0.150f, 0.060f};
  float gamut[9];
  ASSERT_EQ(skity_color_space_gamut_from_chromaticities(p3, gamut),
            SKITY_SUCCESS);
  float back[8];
  ASSERT_EQ(skity_color_space_chromaticities_from_gamut(gamut, back),
            SKITY_SUCCESS);
  // The un-adaptation towards D65 undoes the Bradford step, so the native
  // primaries come back (exact for a D65-native space).
  for (int i = 0; i < 8; i++) {
    EXPECT_NEAR(back[i], p3[i], 3e-5f);
  }

  // Unphysical primaries and null arguments are rejected.
  const float bad[8] = {0.3127f, 0.3290f, 0.90f,  0.95f,
                        0.265f,  0.690f,  0.150f, 0.060f};
  float unused[9];
  EXPECT_EQ(skity_color_space_gamut_from_chromaticities(bad, unused),
            SKITY_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(skity_color_space_gamut_from_chromaticities(nullptr, unused),
            SKITY_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(skity_color_space_chromaticities_from_gamut(gamut, nullptr),
            SKITY_ERROR_INVALID_ARGUMENT);
}

TEST(CapiColorSpaceInfo, InitFillsCanonicalValues) {
  // The parametric kind has no canonical values and must be rejected.
  skity_color_space_info info{};
  EXPECT_EQ(skity_color_space_info_init(&info, SKITY_COLOR_SPACE_PARAMETRIC),
            SKITY_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(skity_color_space_info_init(nullptr, SKITY_COLOR_SPACE_SRGB),
            SKITY_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(skity_color_space_info_init(
                &info, static_cast<skity_color_space_kind>(99)),
            SKITY_ERROR_INVALID_ARGUMENT);

  ASSERT_EQ(skity_color_space_info_init(&info, SKITY_COLOR_SPACE_DISPLAY_P3),
            SKITY_SUCCESS);
  EXPECT_EQ(info.kind, SKITY_COLOR_SPACE_DISPLAY_P3);
  // sRGB transfer curve plus the P3 gamut (D50-adapted; red and green
  // primaries are more saturated than the sRGB ones, whose diagonal is
  // 0.436 / 0.717).
  EXPECT_NEAR(info.transfer_fn[0], 2.4f, 1e-5f);
  EXPECT_GT(info.gamut_xyz[0], 0.5f);
  EXPECT_GT(info.gamut_xyz[4], 0.6f);
  EXPECT_LT(info.gamut_xyz[4], 0.75f);

  // The canonical values match what a pixmap reports for the same kind.
  skity_pixmap pixmap = skity_pixmap_create_alloc(
      2, 2, SKITY_ALPHA_TYPE_UNPREMUL, SKITY_COLOR_TYPE_RGBA);
  ASSERT_NE(pixmap, nullptr);
  ASSERT_EQ(skity_pixmap_set_color_space(pixmap, &info), SKITY_SUCCESS);
  skity_color_space_info out{};
  ASSERT_EQ(skity_pixmap_get_color_space(pixmap, &out), SKITY_SUCCESS);
  EXPECT_EQ(out.kind, info.kind);
  EXPECT_EQ(0, std::memcmp(out.transfer_fn, info.transfer_fn,
                           sizeof(info.transfer_fn)));
  EXPECT_EQ(0,
            std::memcmp(out.gamut_xyz, info.gamut_xyz, sizeof(info.gamut_xyz)));
  skity_pixmap_destroy(pixmap);

  ASSERT_EQ(skity_color_space_info_init(&info, SKITY_COLOR_SPACE_SRGB_LINEAR),
            SKITY_SUCCESS);
  EXPECT_NEAR(info.transfer_fn[0], 1.f, 1e-6f);
  ASSERT_EQ(skity_color_space_info_init(&info, SKITY_COLOR_SPACE_ADOBE_RGB),
            SKITY_SUCCESS);
  EXPECT_NEAR(info.transfer_fn[0], 2.2f, 1e-5f);
}

TEST(CapiColorSpaceInfo, InitParametricValidates) {
  const float tf[7] = {2.2f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f};
  const float identity_gamut[9] = {
      1.f, 0.f, 0.f,  //
      0.f, 1.f, 0.f,  //
      0.f, 0.f, 1.f,
  };
  skity_color_space_info info{};
  ASSERT_EQ(skity_color_space_info_init_parametric(&info, tf, identity_gamut),
            SKITY_SUCCESS);
  EXPECT_EQ(info.kind, SKITY_COLOR_SPACE_PARAMETRIC);
  EXPECT_EQ(info.transfer_fn[0], 2.2f);
  EXPECT_EQ(info.gamut_xyz[4], 1.f);

  // Invalid parameters and null pointers are rejected.
  const float bad_tf[7] = {-2.2f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f};
  EXPECT_EQ(
      skity_color_space_info_init_parametric(&info, bad_tf, identity_gamut),
      SKITY_ERROR_INVALID_ARGUMENT);
  const float singular_gamut[9] = {
      1.f, 0.f, 0.f,  //
      1.f, 0.f, 0.f,  //
      0.f, 0.f, 1.f,
  };
  EXPECT_EQ(skity_color_space_info_init_parametric(&info, tf, singular_gamut),
            SKITY_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(skity_color_space_info_init_parametric(nullptr, tf, identity_gamut),
            SKITY_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(
      skity_color_space_info_init_parametric(&info, nullptr, identity_gamut),
      SKITY_ERROR_INVALID_ARGUMENT);
}

TEST(CapiColorSpaceConvert, ConvertsAndPassesAlphaThrough) {
  // NULL on both sides is the sRGB identity.
  const skity_color4f gray{0.25f, 0.5f, 0.75f, 0.5f};
  skity_color4f out{};
  ASSERT_EQ(skity_color_space_convert(nullptr, &gray, nullptr, &out),
            SKITY_SUCCESS);
  EXPECT_FLOAT_EQ(out.e[0], 0.25f);
  EXPECT_FLOAT_EQ(out.e[1], 0.5f);
  EXPECT_FLOAT_EQ(out.e[2], 0.75f);
  EXPECT_FLOAT_EQ(out.e[3], 0.5f);

  // P3 red converts outside the sRGB gamut (extended values preserved),
  // alpha passes through untouched.
  skity_color_space_info p3{};
  ASSERT_EQ(skity_color_space_info_init(&p3, SKITY_COLOR_SPACE_DISPLAY_P3),
            SKITY_SUCCESS);
  const skity_color4f p3_red{1.f, 0.f, 0.f, 0.5f};
  ASSERT_EQ(skity_color_space_convert(&p3, &p3_red, nullptr, &out),
            SKITY_SUCCESS);
  EXPECT_GT(out.e[0], 1.f);
  EXPECT_LT(out.e[1], 0.f);
  EXPECT_LT(out.e[2], 0.f);
  EXPECT_FLOAT_EQ(out.e[3], 0.5f);

  // Converting back lands on the original color.
  skity_color4f back{};
  ASSERT_EQ(skity_color_space_convert(nullptr, &out, &p3, &back),
            SKITY_SUCCESS);
  EXPECT_NEAR(back.e[0], 1.f, 1e-3f);
  EXPECT_NEAR(back.e[1], 0.f, 1e-3f);
  EXPECT_NEAR(back.e[2], 0.f, 1e-3f);
  EXPECT_FLOAT_EQ(back.e[3], 0.5f);

  EXPECT_EQ(skity_color_space_convert(&p3, nullptr, nullptr, &out),
            SKITY_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(skity_color_space_convert(&p3, &gray, nullptr, nullptr),
            SKITY_ERROR_INVALID_ARGUMENT);
}
