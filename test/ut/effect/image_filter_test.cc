// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <skity/effect/color_filter.hpp>
#include <skity/effect/image_filter.hpp>

using namespace skity;

namespace {
inline float ConvertSigmaToRadius(float sigma) {
  constexpr static float kBlueSigmaScale = 0.57735f;
  return sigma > 0.5f ? (sigma - 0.5f) / kBlueSigmaScale : 0.0f;
}
}  // namespace

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
