// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/graphic/bitmap.hpp>

#include "gtest/gtest.h"
#include "src/graphic/bitmap_sampler.hpp"

TEST(Bitmap, DefaultConstructor) {
  skity::Bitmap bmp;
  EXPECT_EQ(bmp.Width(), 0u);
  EXPECT_EQ(bmp.Height(), 0u);
  EXPECT_EQ(bmp.GetColorType(), skity::ColorType::kUnknown);
}

TEST(Bitmap, ParamConstructor) {
  skity::Bitmap bmp1(100, 50, skity::AlphaType::kUnpremul_AlphaType,
                     skity::ColorType::kRGBA);
  EXPECT_EQ(bmp1.Width(), 100u);
  EXPECT_EQ(bmp1.Height(), 50u);
  EXPECT_EQ(bmp1.GetColorType(), skity::ColorType::kRGBA);
  EXPECT_NE(bmp1.GetPixelAddr(), nullptr);

  skity::Bitmap bmp2(10, 10, skity::AlphaType::kUnpremul_AlphaType,
                     skity::ColorType::kBGRA);
  EXPECT_EQ(bmp2.Width(), 10u);
  EXPECT_EQ(bmp2.Height(), 10u);
  EXPECT_EQ(bmp2.GetColorType(), skity::ColorType::kBGRA);
}

TEST(Bitmap, CreateInvalid) {
  skity::Bitmap bmp0(0, 0, skity::AlphaType::kUnpremul_AlphaType,
                     skity::ColorType::kRGBA);
  EXPECT_EQ(bmp0.Width(), 0u);
  EXPECT_EQ(bmp0.Height(), 0u);
}

TEST(Bitmap, RowBytes) {
  skity::Bitmap bmp(16, 16, skity::AlphaType::kUnpremul_AlphaType,
                    skity::ColorType::kRGBA);
  // 16 pixels * 4 bytes/pixel (RGBA) = 64
  EXPECT_EQ(bmp.RowBytes(), 64u);
}

TEST(Bitmap, GetAlphaType) {
  skity::Bitmap bmp(1, 1, skity::AlphaType::kPremul_AlphaType,
                    skity::ColorType::kRGBA);
  EXPECT_EQ(bmp.GetAlphaType(), skity::AlphaType::kPremul_AlphaType);
}

TEST(Bitmap, PixelReadWriteSingle) {
  skity::Bitmap bmp(20, 30, skity::AlphaType::kUnpremul_AlphaType,
                    skity::ColorType::kRGBA);
  bmp.SetPixel(10, 20, skity::Color_RED);
  skity::Color c = bmp.GetPixel(10, 20);
  EXPECT_EQ(c, skity::Color_RED);
}

TEST(Bitmap, PixelReadWriteBatch) {
  skity::Bitmap bmp(8, 8, skity::AlphaType::kUnpremul_AlphaType,
                    skity::ColorType::kRGBA);
  // Set one row to green
  for (uint32_t x = 0; x < bmp.Width(); ++x) {
    bmp.SetPixel(x, 2, skity::Color_GREEN);
  }
  // Check that row is green
  for (uint32_t x = 0; x < bmp.Width(); ++x) {
    EXPECT_EQ(bmp.GetPixel(x, 2), skity::Color_GREEN);
  }
  // Fill all pixels with blue
  for (uint32_t y = 0; y < bmp.Height(); ++y)
    for (uint32_t x = 0; x < bmp.Width(); ++x)
      bmp.SetPixel(x, y, skity::Color_BLUE);
  // Check all pixels are blue
  for (uint32_t y = 0; y < bmp.Height(); ++y)
    for (uint32_t x = 0; x < bmp.Width(); ++x)
      EXPECT_EQ(bmp.GetPixel(x, y), skity::Color_BLUE);
}

TEST(Bitmap, PixelReadWriteBoundary) {
  skity::Bitmap bmp(5, 5, skity::AlphaType::kUnpremul_AlphaType,
                    skity::ColorType::kRGBA);
  bmp.SetPixel(0, 0, skity::Color_YELLOW);
  bmp.SetPixel(bmp.Width() - 1, 0, skity::Color_CYAN);
  bmp.SetPixel(0, bmp.Height() - 1, skity::Color_MAGENTA);
  bmp.SetPixel(bmp.Width() - 1, bmp.Height() - 1, skity::Color_WHITE);

  EXPECT_EQ(bmp.GetPixel(0, 0), skity::Color_YELLOW);
  EXPECT_EQ(bmp.GetPixel(bmp.Width() - 1, 0), skity::Color_CYAN);
  EXPECT_EQ(bmp.GetPixel(0, bmp.Height() - 1), skity::Color_MAGENTA);
  EXPECT_EQ(bmp.GetPixel(bmp.Width() - 1, bmp.Height() - 1),
            skity::Color_WHITE);
}

TEST(Bitmap, FormatConvert) {
  skity::Bitmap bmp(8, 8, skity::AlphaType::kUnpremul_AlphaType,
                    skity::ColorType::kRGBA);
  bmp.SetPixel(0, 0, skity::Color_RED);
  bmp.SetColorInfo(skity::AlphaType::kUnpremul_AlphaType,
                   skity::ColorType::kBGRA);
  EXPECT_EQ(bmp.GetColorType(), skity::ColorType::kBGRA);
  bmp.SetColorInfo(skity::AlphaType::kUnpremul_AlphaType,
                   skity::ColorType::kA8);
  EXPECT_EQ(bmp.GetColorType(), skity::ColorType::kA8);
  EXPECT_EQ(bmp.Width(), 8u);
  EXPECT_EQ(bmp.Height(), 8u);
}

TEST(Bitmap, OutOfBoundsIgnored) {
  skity::Bitmap bmp(4, 4, skity::AlphaType::kUnpremul_AlphaType,
                    skity::ColorType::kRGBA);
  bmp.SetPixel(1, 1, skity::Color_RED);
  // should be ignored
  bmp.SetPixel(100, 100, skity::Color_GREEN);
  EXPECT_EQ(bmp.GetPixel(1, 1), skity::Color_RED);
}

TEST(Bitmap, PixmapConstructor) {
  auto pixmap = std::make_shared<skity::Pixmap>(
      10, 20, skity::AlphaType::kPremul_AlphaType, skity::ColorType::kRGBA);
  skity::Bitmap bmp{pixmap, false};
  EXPECT_EQ(bmp.Width(), 10u);
  EXPECT_EQ(bmp.Height(), 20u);
  EXPECT_EQ(bmp.GetColorType(), skity::ColorType::kRGBA);
  EXPECT_EQ(bmp.GetAlphaType(), skity::AlphaType::kPremul_AlphaType);
  EXPECT_EQ(bmp.GetPixmap(), pixmap);
}

TEST(Bitmap, SetPixelColor4f) {
  skity::Bitmap bmp(5, 5, skity::AlphaType::kUnpremul_AlphaType,
                    skity::ColorType::kRGBA);
  skity::Color4f color{0.f, 1.f, 0.f, 1.f};  // Green
  bmp.SetPixel(2, 3, color);
  EXPECT_EQ(bmp.GetPixel(2, 3), skity::Color_GREEN);
}

/*
The following is the test for bitmap_sampler.
*/
TEST(BitmapSampler, NearestNeighborSample) {
  skity::Bitmap bmp(2, 2);
  bmp.SetPixel(0, 0, skity::Color_RED);
  bmp.SetPixel(1, 0, skity::Color_GREEN);
  bmp.SetPixel(0, 1, skity::Color_BLUE);
  bmp.SetPixel(1, 1, skity::Color_YELLOW);

  skity::SamplingOptions sampling(skity::FilterMode::kNearest,
                                  skity::MipmapMode::kNone);
  skity::BitmapSampler sampler(bmp, sampling, skity::TileMode::kClamp,
                               skity::TileMode::kClamp);

  // Sample near the center of each pixel
  EXPECT_EQ(sampler.GetColor({0.25f, 0.25f}), skity::Color_RED);
  EXPECT_EQ(sampler.GetColor({0.75f, 0.25f}), skity::Color_GREEN);
  EXPECT_EQ(sampler.GetColor({0.25f, 0.75f}), skity::Color_BLUE);
  EXPECT_EQ(sampler.GetColor({0.75f, 0.75f}), skity::Color_YELLOW);
}

TEST(BitmapSampler, LinearSample) {
  skity::Bitmap bmp(2, 2);
  bmp.SetPixel(0, 0, skity::ColorSetARGB(255, 255, 0, 0));    // Red
  bmp.SetPixel(1, 0, skity::ColorSetARGB(255, 0, 255, 0));    // Green
  bmp.SetPixel(0, 1, skity::ColorSetARGB(255, 0, 0, 255));    // Blue
  bmp.SetPixel(1, 1, skity::ColorSetARGB(255, 255, 255, 0));  // Yellow

  skity::SamplingOptions sampling(skity::FilterMode::kLinear,
                                  skity::MipmapMode::kNone);
  skity::BitmapSampler sampler(bmp, sampling, skity::TileMode::kClamp,
                               skity::TileMode::kClamp);

  // Sample at the exact center, should be an average of all 4 colors
  skity::Color center_color = sampler.GetColor({0.5f, 0.5f});
  // Expected: R = (255+0+0+255)/4 = 127.5
  //           G = (0+255+0+255)/4 = 127.5
  //           B = (0+0+255+0)/4 = 63.75
  // The skity implementation appears to truncate float color components to int,
  // instead of rounding. So 127.5 becomes 127.
  EXPECT_EQ(ColorGetR(center_color), 127);
  EXPECT_EQ(ColorGetG(center_color), 127);
  EXPECT_EQ(ColorGetB(center_color), 63);
}

TEST(BitmapSampler, TileModeClamp) {
  skity::Bitmap bmp(1, 1);
  bmp.SetPixel(0, 0, skity::Color_MAGENTA);

  skity::SamplingOptions sampling(skity::FilterMode::kNearest,
                                  skity::MipmapMode::kNone);
  skity::BitmapSampler sampler(bmp, sampling, skity::TileMode::kClamp,
                               skity::TileMode::kClamp);

  EXPECT_EQ(sampler.GetColor({-1.f, -1.f}), skity::Color_MAGENTA);
  EXPECT_EQ(sampler.GetColor({0.5f, 0.5f}), skity::Color_MAGENTA);
  EXPECT_EQ(sampler.GetColor({2.f, 2.f}), skity::Color_MAGENTA);
}

TEST(BitmapSampler, TileModeRepeat) {
  skity::Bitmap bmp(2, 2);
  bmp.SetPixel(0, 0, skity::Color_RED);
  bmp.SetPixel(1, 0, skity::Color_GREEN);
  bmp.SetPixel(0, 1, skity::Color_BLUE);
  bmp.SetPixel(1, 1, skity::Color_YELLOW);

  skity::SamplingOptions sampling(skity::FilterMode::kNearest,
                                  skity::MipmapMode::kNone);
  skity::BitmapSampler sampler(bmp, sampling, skity::TileMode::kRepeat,
                               skity::TileMode::kRepeat);

  // Should wrap around
  EXPECT_EQ(sampler.GetColor({1.25f, 1.25f}), skity::Color_RED);
  EXPECT_EQ(sampler.GetColor({-0.25f, -0.75f}), skity::Color_GREEN);
}

TEST(BitmapSampler, TileModeMirror) {
  skity::Bitmap bmp(2, 1);
  bmp.SetPixel(0, 0, skity::Color_RED);
  bmp.SetPixel(1, 0, skity::Color_GREEN);

  skity::SamplingOptions sampling(skity::FilterMode::kNearest,
                                  skity::MipmapMode::kNone);
  skity::BitmapSampler sampler(bmp, sampling, skity::TileMode::kMirror,
                               skity::TileMode::kMirror);

  EXPECT_EQ(sampler.GetColor({0.25f, 0.f}), skity::Color_RED);
  EXPECT_EQ(sampler.GetColor({0.75f, 0.f}), skity::Color_GREEN);
  // Mirroring
  EXPECT_EQ(sampler.GetColor({1.25f, 0.f}), skity::Color_GREEN);
  EXPECT_EQ(sampler.GetColor({1.75f, 0.f}), skity::Color_RED);
  EXPECT_EQ(sampler.GetColor({2.25f, 0.f}), skity::Color_RED);
}
