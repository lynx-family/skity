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

namespace {

void DrawImageTileModes(skity::FilterMode filter, const char* filename) {
  skity::Bitmap bitmap(8, 8, skity::AlphaType::kPremul_AlphaType,
                       skity::ColorType::kRGBA);
  const skity::Color colors[] = {skity::Color_RED, skity::Color_GREEN,
                                 skity::Color_BLUE, skity::Color_YELLOW};
  for (uint32_t y = 0; y < 8; ++y) {
    for (uint32_t x = 0; x < 8; ++x) {
      bitmap.SetPixel(x, y, colors[(y / 4) * 2 + x / 4]);
    }
  }
  auto image = skity::Image::MakeImage(bitmap.GetPixmap());
  const skity::TileMode modes[] = {
      skity::TileMode::kClamp, skity::TileMode::kRepeat,
      skity::TileMode::kMirror, skity::TileMode::kDecal};
  skity::SamplingOptions sampling;
  sampling.filter = filter;

  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400, 400));
  auto canvas = recorder.GetRecordingCanvas();
  skity::Paint paint;
  paint.SetColor(skity::Color_WHITE);
  canvas->DrawRect(skity::Rect::MakeWH(400, 400), paint);
  // Columns select X mode; rows select Y mode. Each panel spans [-1, 2]
  // on both texture axes, covering negative UVs and both image boundaries.
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      canvas->Save();
      canvas->Translate(x * 100.f + 34.f, y * 100.f + 34.f);
      canvas->Scale(4.f, 4.f);
      paint.SetShader(
          skity::Shader::MakeShader(image, sampling, modes[x], modes[y]));
      canvas->DrawRect(skity::Rect::MakeXYWH(-8.f, -8.f, 24.f, 24.f), paint);
      canvas->Restore();
    }
  }
  auto dl = recorder.FinishRecording();
  auto path = std::string(kGoldenTestDir) + filename;
  EXPECT_TRUE(
      skity::testing::CompareGoldenTexture(dl.get(), 400, 400, path.c_str()));
}

}  // namespace

TEST(ImageGolden, ShaderTileModesNearest) {
  DrawImageTileModes(skity::FilterMode::kNearest,
                     "image_tile_modes_nearest.png");
}

TEST(ImageGolden, ShaderTileModesLinear) {
  DrawImageTileModes(skity::FilterMode::kLinear, "image_tile_modes_linear.png");
}
