// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/graphic/bitmap_sampler.hpp"

#include <memory>
#include <skity/graphic/color_space.hpp>
#include <skity/io/pixmap.hpp>

#include "gtest/gtest.h"

TEST(BitmapSamplerColorSpace, DecodesNonSRGBTaggedBitmap) {
  // Store an sRGB color pre-converted into Display P3 in a tagged bitmap;
  // sampling must decode it back to (approximately) the original color.
  skity::Color4f original{0.25f, 0.5f, 0.75f, 1.f};
  auto p3_color = skity::ColorSpace::Convert(
      skity::ColorSpace::SRGB(), original, skity::ColorSpace::DisplayP3());

  auto pixmap = std::make_shared<skity::Pixmap>(1, 1);
  ASSERT_TRUE(pixmap->SetColorInfo(skity::kUnpremul_AlphaType,
                                   skity::ColorType::kRGBA,
                                   skity::ColorSpace::DisplayP3()));
  skity::Bitmap bitmap(pixmap, false);
  bitmap.SetPixel(0, 0, skity::Color4fToColor(p3_color));

  skity::BitmapSampler sampler(bitmap, skity::SamplingOptions{},
                               skity::TileMode::kClamp,
                               skity::TileMode::kClamp);
  skity::Color decoded = sampler.GetColor(skity::Vec2{0.5f, 0.5f});

  // The decode matches running the same conversion on the stored value, and
  // both land back on the original sRGB color within 8-bit roundtrip error.
  auto expected = skity::Color4fFromColor(skity::Color4fToColor(p3_color));
  expected = skity::ColorSpace::Convert(skity::ColorSpace::DisplayP3(),
                                        expected, nullptr);
  EXPECT_EQ(decoded, skity::Color4fToColor(expected));
  auto decoded_4f = skity::Color4fFromColor(decoded);
  EXPECT_NEAR(decoded_4f.r, original.r, 2.f / 255.f);
  EXPECT_NEAR(decoded_4f.g, original.g, 2.f / 255.f);
  EXPECT_NEAR(decoded_4f.b, original.b, 2.f / 255.f);
  EXPECT_NEAR(decoded_4f.a, original.a, 2.f / 255.f);
}

TEST(BitmapSamplerColorSpace, SRGBBitmapIsUnchanged) {
  // Without a color space the sampler output must stay bit-identical.
  auto pixmap = std::make_shared<skity::Pixmap>(1, 1);
  skity::Bitmap bitmap(pixmap, false);
  auto stored = skity::ColorSetARGB(255, 30, 200, 160);
  bitmap.SetPixel(0, 0, stored);

  skity::BitmapSampler sampler(bitmap, skity::SamplingOptions{},
                               skity::TileMode::kClamp,
                               skity::TileMode::kClamp);
  EXPECT_EQ(sampler.GetColor(skity::Vec2{0.5f, 0.5f}), stored);
}
