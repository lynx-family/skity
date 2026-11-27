// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <skity/effect/color_filter.hpp>
#include <skity/effect/image_filter.hpp>
#include <skity/graphic/color_space.hpp>
#include <skity/graphic/paint.hpp>

#include "src/effect/image_filter_base.hpp"
#include "src/render/hw/draw/wgx_utils.hpp"

using namespace skity;

TEST(ImageFilter, ComputeFastBounds) {
  Rect src = Rect::MakeLTRB(100, 50, 200, 100);
  auto image_filter = ImageFilters::Blur(3, 4);
  Rect dst = image_filter->ComputeFastBounds(src);
  ASSERT_EQ(dst, Rect::MakeLTRB(100 - ConvertSigmaToRadius(3),
                                50 - ConvertSigmaToRadius(4),
                                200 + ConvertSigmaToRadius(3),
                                100 + ConvertSigmaToRadius(4)));

  image_filter = ImageFilters::DropShadow(5, 10, 3, 4, Color_RED, nullptr);
  dst = image_filter->ComputeFastBounds(src);
  ASSERT_EQ(dst, Rect::MakeLTRB(100, 50, 205 + ConvertSigmaToRadius(3),
                                110 + ConvertSigmaToRadius(4)));

  image_filter = ImageFilters::MatrixTransform(Matrix::Translate(-100, -100) *
                                               Matrix::Scale(2, 2));
  dst = image_filter->ComputeFastBounds(src);
  ASSERT_EQ(dst, Rect::MakeLTRB(100, 0, 300, 100));

  image_filter = ImageFilters::ColorFilter(ColorFilters::LinearToSRGBGamma());
  dst = image_filter->ComputeFastBounds(src);
  ASSERT_EQ(dst, src);

  image_filter = ImageFilters::Compose(
      ImageFilters::MatrixTransform(Matrix::Translate(-100, -100)),
      ImageFilters::MatrixTransform(Matrix::Scale(2, 2)));
  dst = image_filter->ComputeFastBounds(src);
  ASSERT_EQ(dst, Rect::MakeLTRB(100, 0, 300, 100));
}

TEST(ImageFilterOpacityTest, IsConservativelyTranslucent) {
  Paint paint;
  paint.SetImageFilter(ImageFilters::Blur(2.f, 2.f));
  EXPECT_FALSE(IsPaintSourceOpaque(paint));
}

namespace {

const DropShadowImageFilter* AsDropShadow(
    const std::shared_ptr<ImageFilter>& f) {
  return static_cast<const DropShadowImageFilter*>(As_IFB(f.get()));
}

}  // namespace

TEST(ImageFilterColorSpace, DropShadowRecordsDeclaredSpace) {
  const Color4f p3_red{1.f, 0.f, 0.f, 1.f};
  auto declared = ImageFilters::DropShadow(
      4, 4, 3, 3, p3_red, nullptr, DropShadowOptions{ColorSpace::DisplayP3()});
  ASSERT_NE(declared, nullptr);
  auto* shadow = AsDropShadow(declared);
  EXPECT_EQ(shadow->GetColorSpace(), ColorSpace::DisplayP3());
  EXPECT_EQ(shadow->GetColor4f().r, 1.f);
  EXPECT_EQ(shadow->GetColor4f().a, 1.f);

  // Default options keep the sRGB convention: undeclared space.
  auto defaults = ImageFilters::DropShadow(4, 4, 3, 3, p3_red, nullptr);
  ASSERT_NE(defaults, nullptr);
  shadow = AsDropShadow(defaults);
  EXPECT_EQ(shadow->GetColorSpace(), nullptr);

  // The legacy factory keeps the sRGB convention too: undeclared space and
  // the color still reachable through the Color accessor.
  auto legacy = ImageFilters::DropShadow(4, 4, 3, 3, Color_RED, nullptr);
  ASSERT_NE(legacy, nullptr);
  shadow = AsDropShadow(legacy);
  EXPECT_EQ(shadow->GetColorSpace(), nullptr);
  EXPECT_EQ(shadow->GetColor(), Color_RED);
}

TEST(ImageFilterColorSpace, DropShadowLocalMatrixPreservesDeclaredSpace) {
  const Color4f p3_red{1.f, 0.f, 0.f, 1.f};
  auto filter = ImageFilters::DropShadow(
      4, 4, 3, 3, p3_red, nullptr, DropShadowOptions{ColorSpace::DisplayP3()});
  auto rebuilt = ImageFilters::LocalMatrix(filter, Matrix::Scale(2, 2));
  ASSERT_NE(rebuilt, nullptr);
  auto* shadow = AsDropShadow(rebuilt);
  EXPECT_EQ(shadow->GetColorSpace(), ColorSpace::DisplayP3());
  EXPECT_EQ(shadow->GetColor4f().r, 1.f);
  // The rebuild still scales geometry: offsets double.
  EXPECT_FLOAT_EQ(shadow->GetOffsetX(), 8.f);

  // A legacy filter rebuilds through the same path: the color survives the
  // Color4f round trip byte-for-byte and the space stays undeclared.
  auto legacy_rebuilt = ImageFilters::LocalMatrix(
      ImageFilters::DropShadow(4, 4, 3, 3, Color_RED, nullptr),
      Matrix::Scale(2, 2));
  ASSERT_NE(legacy_rebuilt, nullptr);
  shadow = AsDropShadow(legacy_rebuilt);
  EXPECT_EQ(shadow->GetColorSpace(), nullptr);
  EXPECT_EQ(shadow->GetColor4f().r, 1.f);
  EXPECT_EQ(shadow->GetColor(), Color_RED);
}

TEST(ImageFilter, LocalMatrixScalesBlurRadiiAlongEachAxis) {
  auto matrix = Matrix::Scale(-2, 3);
  for (auto input :
       {ImageFilters::Blur(2, 4),
        ImageFilters::DropShadow(3, 2, 2, 4, Color_RED, nullptr)}) {
    auto rebuilt = ImageFilters::LocalMatrix(input, matrix);
    ASSERT_TRUE(rebuilt);
    EXPECT_FLOAT_EQ(As_IFB(rebuilt.get())->GetRadiusX(),
                    2 * As_IFB(input.get())->GetRadiusX());
    EXPECT_FLOAT_EQ(As_IFB(rebuilt.get())->GetRadiusY(),
                    3 * As_IFB(input.get())->GetRadiusY());
  }
}
