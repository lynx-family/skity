// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/graphic/bitmap_sampler.hpp"

#include "gtest/gtest.h"

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
