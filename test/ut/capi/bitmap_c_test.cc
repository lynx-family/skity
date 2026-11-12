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
