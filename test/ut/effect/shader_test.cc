// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <skity/effect/color_filter.hpp>
#include <skity/effect/shader.hpp>
#include <skity/graphic/image.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/io/pixmap.hpp>

#include "src/render/hw/draw/wgx_utils.hpp"

namespace skity {
namespace {

constexpr Point kGradientPoints[] = {{0.f, 0.f, 0.f, 1.f},
                                     {10.f, 10.f, 0.f, 1.f}};
constexpr Vec4 kOpaqueColors[] = {Colors::kRed, Colors::kBlue};

TEST(ShaderOpacityTest, GradientRequiresOpaqueStopsAndNonDecalTiling) {
  Paint paint;
  paint.SetShader(
      Shader::MakeLinear(kGradientPoints, kOpaqueColors, nullptr, 2));
  EXPECT_TRUE(IsPaintSourceOpaque(paint));

  Vec4 translucent_colors[] = {Colors::kRed, {0.f, 0.f, 1.f, 0.5f}};
  paint.SetShader(
      Shader::MakeLinear(kGradientPoints, translucent_colors, nullptr, 2));
  EXPECT_FALSE(IsPaintSourceOpaque(paint));

  paint.SetShader(Shader::MakeLinear(kGradientPoints, kOpaqueColors, nullptr, 2,
                                     TileMode::kDecal));
  EXPECT_FALSE(IsPaintSourceOpaque(paint));
}

TEST(ShaderOpacityTest, ConicalGradientIsConservativelyTranslucent) {
  Paint paint;
  paint.SetShader(Shader::MakeTwoPointConical(kGradientPoints[0], 1.f,
                                              kGradientPoints[1], 4.f,
                                              kOpaqueColors, nullptr, 2));

  ASSERT_NE(paint.GetShader(), nullptr);
  EXPECT_FALSE(IsPaintSourceOpaque(paint));
}

TEST(ShaderOpacityTest, ImageRequiresStableOpaqueContentAndNonDecalTiling) {
  Paint paint;
  auto opaque_pixmap = std::make_shared<Pixmap>(
      1, 1, AlphaType::kOpaque_AlphaType, ColorType::kRGBA);
  auto opaque_image = Image::MakeImage(opaque_pixmap);

  paint.SetShader(Shader::MakeShader(opaque_image));
  EXPECT_TRUE(IsPaintSourceOpaque(paint));

  paint.SetShader(Shader::MakeShader(opaque_image, SamplingOptions{},
                                     TileMode::kDecal, TileMode::kClamp));
  EXPECT_FALSE(IsPaintSourceOpaque(paint));

  auto translucent_pixmap = std::make_shared<Pixmap>(
      1, 1, AlphaType::kPremul_AlphaType, ColorType::kRGBA);
  paint.SetShader(Shader::MakeShader(Image::MakeImage(translucent_pixmap)));
  EXPECT_FALSE(IsPaintSourceOpaque(paint));

  auto deferred_image = Image::MakeDeferredTextureImage(
      TextureFormat::kRGBA, 1, 1, AlphaType::kOpaque_AlphaType);
  paint.SetShader(Shader::MakeShader(deferred_image));
  EXPECT_FALSE(IsPaintSourceOpaque(paint));
}

TEST(PaintSourceOpacityTest, IncludesPaintAlphaAndColorFilter) {
  Paint paint;
  EXPECT_TRUE(IsPaintSourceOpaque(paint));

  paint.SetAlphaF(0.5f);
  EXPECT_FALSE(IsPaintSourceOpaque(paint));
  paint.SetAlphaF(1.f);

  paint.SetColorFilter(ColorFilters::Blend(Color_RED, BlendMode::kSrcATop));
  EXPECT_TRUE(IsPaintSourceOpaque(paint));

  constexpr float alpha_preserving_matrix[20] = {
      0.f, 1.f, 0.f, 0.f, 0.f,  //
      1.f, 0.f, 0.f, 0.f, 0.f,  //
      0.f, 0.f, 1.f, 0.f, 0.f,  //
      0.f, 0.f, 0.f, 1.f, 0.f,  //
  };
  paint.SetColorFilter(ColorFilters::Matrix(alpha_preserving_matrix));
  EXPECT_TRUE(IsPaintSourceOpaque(paint));

  constexpr float alpha_reducing_matrix[20] = {
      1.f, 0.f, 0.f, 0.f,  0.f,  //
      0.f, 1.f, 0.f, 0.f,  0.f,  //
      0.f, 0.f, 1.f, 0.f,  0.f,  //
      0.f, 0.f, 0.f, 0.5f, 0.f,  //
  };
  paint.SetColorFilter(ColorFilters::Matrix(alpha_reducing_matrix));
  EXPECT_FALSE(IsPaintSourceOpaque(paint));

  paint.SetColorFilter(
      ColorFilters::Compose(ColorFilters::SRGBToLinearGamma(),
                            ColorFilters::Matrix(alpha_preserving_matrix)));
  EXPECT_TRUE(IsPaintSourceOpaque(paint));

  paint.SetColorFilter(
      ColorFilters::Compose(ColorFilters::SRGBToLinearGamma(),
                            ColorFilters::Matrix(alpha_reducing_matrix)));
  EXPECT_FALSE(IsPaintSourceOpaque(paint));
}

}  // namespace
}  // namespace skity
