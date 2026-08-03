// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <skity/skity.hpp>
#include <string>

#include "common/golden_test_check.hpp"

constexpr const char* kGoldenTestDir = CASE_DIR;

namespace {

// An 8x8 red/blue checkerboard with hard edges: upscaling it exercises
// bicubic ringing/overshoot, which is what distinguishes cubic from bilinear.
std::shared_ptr<skity::Image> MakeCheckerImage() {
  constexpr uint32_t kSize = 8;
  skity::Bitmap bmp(kSize, kSize, skity::AlphaType::kPremul_AlphaType,
                    skity::ColorType::kRGBA);
  for (uint32_t y = 0; y < kSize; y++) {
    for (uint32_t x = 0; x < kSize; x++) {
      bool red = ((x / 2) + (y / 2)) % 2 == 0;
      bmp.SetPixel(x, y, red ? skity::Color_RED : skity::Color_BLUE);
    }
  }
  return skity::Image::MakeImage(bmp.GetPixmap());
}

}  // namespace

TEST(ImageGolden, DrawImageRect_Cubic_CatmullRom) {
  auto image = MakeCheckerImage();

  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200, 200));
  auto canvas = recorder.GetRecordingCanvas();

  skity::SamplingOptions sampling;
  sampling.cubic = {0.f, 0.5f};  // Catmull-Rom

  canvas->DrawImageRect(image, skity::Rect::MakeWH(8.f, 8.f),
                        skity::Rect::MakeWH(100.f, 100.f), sampling);

  auto dl = recorder.FinishRecording();
  std::string path =
      std::string(kGoldenTestDir) + "image_cubic_catmull_rom.png";
  EXPECT_TRUE(
      skity::testing::CompareGoldenTexture(dl.get(), 200, 200, path.c_str()));
}

TEST(ImageGolden, DrawImageRect_Cubic_Mitchell) {
  auto image = MakeCheckerImage();

  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200, 200));
  auto canvas = recorder.GetRecordingCanvas();

  // Mitchell placed next to Catmull-Rom for visual comparison of presets.
  skity::SamplingOptions sampling;
  sampling.cubic = {1.f / 3.f, 1.f / 3.f};

  canvas->Save();
  canvas->Translate(100.f, 0.f);
  canvas->DrawImageRect(image, skity::Rect::MakeWH(8.f, 8.f),
                        skity::Rect::MakeWH(100.f, 100.f), sampling);
  canvas->Restore();

  auto dl = recorder.FinishRecording();
  std::string path = std::string(kGoldenTestDir) + "image_cubic_mitchell.png";
  EXPECT_TRUE(
      skity::testing::CompareGoldenTexture(dl.get(), 200, 200, path.c_str()));
}
