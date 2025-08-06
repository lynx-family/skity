// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <skity/effect/color_filter.hpp>

#include "src/effect/color_filter_base.hpp"
#include "src/graphic/color_priv.hpp"

TEST(BlendFilterTest, Creation) {
  auto filter =
      skity::ColorFilters::Blend(skity::Color_WHITE, skity::BlendMode::kDst);

  // no needs to create filter if BlendMode is kDst
  EXPECT_EQ(filter, nullptr);

  filter =
      skity::ColorFilters::Blend(skity::Color_WHITE, skity::BlendMode::kDstIn);

  // no needs to create filter if BlendMode is kDstIn and color alpha is 255
  EXPECT_EQ(filter, nullptr);

  filter = skity::ColorFilters::Blend(skity::Color_TRANSPARENT,
                                      skity::BlendMode::kDstOut);

  // no needs to create filter if BlendMode is kDstOut and color alpha is 0
  EXPECT_EQ(filter, nullptr);

  filter = skity::ColorFilters::Blend(skity::ColorSetARGB(127, 255, 0, 0),
                                      skity::BlendMode::kSrcOver);

  // needs to create filter if BlendMode is kDstOut and color alpha is not 0
  EXPECT_NE(filter, nullptr);

  auto bf = static_cast<skity::ColorFilterBase*>(filter.get());

  EXPECT_EQ(bf->GetType(), skity::ColorFilterType::kBlend);

  auto nf = static_cast<skity::BlendColorFilter*>(bf);

  EXPECT_EQ(nf->GetColor(), skity::ColorSetARGB(127, 255, 0, 0));
  EXPECT_EQ(nf->GetBlendMode(), skity::BlendMode::kSrcOver);
}

TEST(MatrixFilterTest, Creation) {
  auto filter = skity::ColorFilters::Matrix(nullptr);

  // nullptr matrix will cause creation failed
  EXPECT_EQ(filter, nullptr);

  constexpr float identity_matrix[20] = {1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f,
                                         0.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f,
                                         0.f, 0.f, 0.f, 0.f, 1.f, 0.f};

  filter = skity::ColorFilters::Matrix(identity_matrix);

  // no needs to create filter if matrix is identity matrix
  EXPECT_EQ(filter, nullptr);

  constexpr float color_matrix[20] = {
      0, 1, 0, 0, 0,  //
      0, 0, 1, 0, 0,  //
      1, 0, 0, 0, 0,  //
      0, 0, 0, 1, 0,  //
  };

  filter = skity::ColorFilters::Matrix(color_matrix);

  auto bf = static_cast<skity::ColorFilterBase*>(filter.get());

  EXPECT_EQ(bf->GetType(), skity::ColorFilterType::kMatrix);

  auto matrix = skity::Matrix{
      color_matrix[0], color_matrix[5], color_matrix[10], color_matrix[15],  //
      color_matrix[1], color_matrix[6], color_matrix[11], color_matrix[16],  //
      color_matrix[2], color_matrix[7], color_matrix[12], color_matrix[17],  //
      color_matrix[3], color_matrix[8], color_matrix[13], color_matrix[18],  //
  };

  auto vec = skity::Vec4{
      color_matrix[4],
      color_matrix[9],
      color_matrix[14],
      color_matrix[19],
  };

  auto nf = static_cast<skity::MatrixColorFilter*>(bf);

  auto [m, v] = nf->GetMatrix();

  EXPECT_EQ(m, matrix);
  EXPECT_EQ(v, vec);
}

TEST(MatrixFilterTest, Apply) {
  constexpr float color_matrix[20] = {
      0, 1, 0, 0, 0,  //
      0, 0, 1, 0, 0,  //
      1, 0, 0, 0, 0,  //
      0, 0, 0, 1, 0,  //
  };

  auto filter = skity::ColorFilters::Matrix(color_matrix);

  auto nf = static_cast<skity::MatrixColorFilter*>(filter.get());

  auto [m, v] = nf->GetMatrix();

  auto color_4fv = skity::Vec4{1.f, 0.f, 0.f, 1.f};

  auto dst = filter->FilterColor(skity::Color4fToColor(color_4fv));

  auto expect_4fv = m * color_4fv + v;

  auto expect_c = skity::Color4fToColor(expect_4fv);

  EXPECT_EQ(dst, expect_c);
}

TEST(LinearToSRGBGammaTest, Apply) {
  auto filter = skity::ColorFilters::LinearToSRGBGamma();

  auto bf = static_cast<skity::ColorFilterBase*>(filter.get());

  EXPECT_EQ(bf->GetType(), skity::ColorFilterType::kLinearToSRGBGamma);

  auto dst = filter->FilterColor(
      skity::ColorToPMColor(skity::ColorSetARGB(127, 127, 0, 0)));

  auto expect = skity::ColorSetARGB(127, 187, 0, 0);

  EXPECT_EQ(dst, skity::ColorToPMColor(expect));
}

TEST(SRGBToLinearGammaTest, Apply) {
  auto filter = skity::ColorFilters::SRGBToLinearGamma();

  auto bf = static_cast<skity::ColorFilterBase*>(filter.get());

  EXPECT_EQ(bf->GetType(), skity::ColorFilterType::kSRGBToLinearGamma);

  auto dst = filter->FilterColor(
      skity::ColorToPMColor(skity::ColorSetARGB(127, 187, 0, 0)));

  auto expect = skity::ColorSetARGB(127, 126, 0, 0);

  EXPECT_EQ(dst, skity::ColorToPMColor(expect));
}

TEST(ComposeFilterTest, Creation) {
  auto filter = skity::ColorFilters::Compose(nullptr, nullptr);

  EXPECT_EQ(filter, nullptr);

  {
    auto filter1 = skity::ColorFilters::SRGBToLinearGamma();

    filter = skity::ColorFilters::Compose(filter1, nullptr);

    EXPECT_EQ(filter, filter1);

    auto filter2 = skity::ColorFilters::Compose(nullptr, filter1);

    EXPECT_EQ(filter2, filter1);
  }

  auto filter1 = skity::ColorFilters::SRGBToLinearGamma();
  auto filter2 = skity::ColorFilters::LinearToSRGBGamma();

  filter = skity::ColorFilters::Compose(filter1, filter2);

  EXPECT_NE(filter, nullptr);

  auto bf = static_cast<skity::ColorFilterBase*>(filter.get());

  EXPECT_EQ(bf->GetType(), skity::ColorFilterType::kCompose);

  auto compose_filter = static_cast<skity::ComposeColorFilter*>(bf);

  auto filters = compose_filter->GetFilters();

  EXPECT_EQ(filters.size(), 2);

  EXPECT_EQ(filters[0], filter2.get());
  EXPECT_EQ(filters[1], filter1.get());
}
