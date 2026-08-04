// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <filesystem>
#include <skity/recorder/picture_recorder.hpp>

#include "common/golden_test_check.hpp"
#include "skity/effect/image_filter.hpp"
#include "skity/geometry/camera.hpp"
#include "skity/geometry/matrix.hpp"
#include "skity/graphic/bitmap.hpp"
#include "skity/graphic/color.hpp"
#include "skity/graphic/image.hpp"

static const char* kGoldenTestImageDir = CASE_DIR;
static const char* kGoldenTestCoverageAAImageDir =
    CASE_DIR "coverage_aa_images/";

namespace {

std::filesystem::path CoverageAAGoldenPath(const char* name) {
  std::filesystem::path path(kGoldenTestCoverageAAImageDir);
  path.append(name);
  return path;
}

}  // namespace

TEST(SaveLayerGolden, TwoCircle) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));
  auto canvas = recorder.GetRecordingCanvas();

  canvas->Save();
  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::Color_GREEN);
  canvas->Scale(10, 10);
  canvas->DrawCircle(20, 20, 10, paint);

  canvas->SaveLayer(skity::Rect::MakeLTRB(0, 0, 40, 40), skity::Paint{});
  paint.SetColor(skity::Color_RED);
  canvas->DrawCircle(20, 20, 10, paint);
  canvas->Restore();
  canvas->Restore();

  std::filesystem::path golden_path = kGoldenTestImageDir;
  golden_path.append("two_circle.png");
  auto coverage_aa_path = CoverageAAGoldenPath("two_circle.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 400.f, 400.f,
      skity::testing::PathList{.cpu_tess_path = golden_path.c_str(),
                               .gpu_tess_path = golden_path.c_str(),
                               .coverage_aa_path = coverage_aa_path.c_str()}));
}

TEST(SaveLayerGolden, ThreeCircle) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));
  auto canvas = recorder.GetRecordingCanvas();

  canvas->Save();
  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::Color_GREEN);
  canvas->Scale(10.1, 10.1);
  canvas->DrawCircle(20.3, 20.3, 10, paint);

  canvas->SaveLayer(skity::Rect::MakeLTRB(10.3, 10.3, 30.3, 30.3),
                    skity::Paint{});
  paint.SetColor(skity::Color_RED);
  canvas->DrawCircle(20.3, 20.3, 10, paint);
  canvas->SaveLayer(skity::Rect::MakeLTRB(10.3, 10.3, 30.3, 30.3),
                    skity::Paint{});
  paint.SetColor(skity::Color_BLUE);
  canvas->DrawCircle(20.3, 20.3, 10, paint);

  canvas->Restore();
  canvas->Restore();
  canvas->Restore();

  std::filesystem::path golden_path = kGoldenTestImageDir;
  golden_path.append("three_circle.png");
  auto coverage_aa_path = CoverageAAGoldenPath("three_circle.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 400.f, 400.f,
      skity::testing::PathList{.cpu_tess_path = golden_path.c_str(),
                               .gpu_tess_path = golden_path.c_str(),
                               .coverage_aa_path = coverage_aa_path.c_str()}));
}

TEST(SaveLayerGolden, TwoCircleWithTranslate) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));
  auto canvas = recorder.GetRecordingCanvas();

  canvas->Save();
  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::Color_GREEN);
  canvas->Scale(10, 10);
  canvas->DrawCircle(20, 20, 10, paint);
  skity::Paint restore_paint;
  restore_paint.SetImageFilter(
      skity::ImageFilters::MatrixTransform(skity::Matrix::Translate(5, 0)));
  canvas->SaveLayer(skity::Rect::MakeLTRB(0, 0, 400, 400), restore_paint);
  paint.SetColor(skity::Color_RED);
  canvas->DrawCircle(20, 20, 10, paint);
  canvas->Restore();
  canvas->Restore();

  std::filesystem::path golden_path = kGoldenTestImageDir;
  golden_path.append("two_circle_with_translate.png");
  auto coverage_aa_path = CoverageAAGoldenPath("two_circle_with_translate.png");

  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 400.f, 400.f,
      skity::testing::PathList{.cpu_tess_path = golden_path.c_str(),
                               .gpu_tess_path = golden_path.c_str(),
                               .coverage_aa_path = coverage_aa_path.c_str()}));
}

TEST(SaveLayerGolden, PerspectiveZ0Plane) {
  constexpr uint32_t kWidth = 400;
  constexpr uint32_t kHeight = 400;
  const auto layer_bounds =
      skity::Rect::MakeLTRB(-23.f, -16.5f, 558.5f, 172.5f);

  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(kWidth, kHeight));
  auto canvas = recorder.GetRecordingCanvas();
  canvas->Clear(skity::Color_WHITE);

  constexpr float kCompositionWidth = 750.f;
  constexpr float kCompositionHeight = 1628.f;
  float scale = kHeight / kCompositionHeight;
  canvas->Translate((kWidth - kCompositionWidth * scale) * 0.5f, 0.f);
  canvas->Scale(scale, scale);
  canvas->Concat(
      skity::Camera(kCompositionWidth, kCompositionHeight).GetFixedCamera());

  auto model = skity::Matrix::Translate(565.f, 811.f) *
               skity::Matrix::Translate(0.f, 60.f) *
               skity::Matrix::Translate(-249.f, -131.f) *
               skity::Matrix::Translate(248.5f, -18.5f) *
               skity::Matrix::RotateDeg(-135.f, {1.f, 0.f, 0.f}) *
               skity::Matrix::Translate(-249.f, -172.5f);
  canvas->Concat(model);

  canvas->SaveLayer(layer_bounds, skity::Paint{});
  skity::Bitmap bitmap(8, 3);
  for (uint32_t y = 0; y < bitmap.Height(); y++) {
    for (uint32_t x = 0; x < bitmap.Width(); x++) {
      auto color = (x + y) % 2 == 0 ? skity::ColorSetARGB(255, 30, 64, 175)
                                    : skity::ColorSetARGB(255, 96, 165, 250);
      if (x == y) {
        color = skity::Color_YELLOW;
      }
      bitmap.SetPixel(x, y, color);
    }
  }
  auto image = skity::Image::MakeImage(bitmap.GetPixmap());
  canvas->DrawImage(image, layer_bounds);

  skity::Paint mask_layer_paint;
  mask_layer_paint.SetBlendMode(skity::BlendMode::kDstIn);
  canvas->SaveLayer(layer_bounds, mask_layer_paint);
  skity::Path flap;
  flap.MoveTo(55.f, -16.5f);
  flap.LineTo(480.f, -16.5f);
  flap.LineTo(558.5f, 172.5f);
  flap.LineTo(-23.f, 172.5f);
  flap.Close();
  skity::Paint mask_paint;
  mask_paint.SetColor(skity::Color_WHITE);
  canvas->DrawPath(flap, mask_paint);
  canvas->Restore();
  canvas->Restore();

  std::filesystem::path golden_path = kGoldenTestImageDir;
  golden_path.append("perspective_z0_plane.png");
  auto coverage_aa_path = CoverageAAGoldenPath("perspective_z0_plane.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), kWidth, kHeight,
      skity::testing::PathList{.cpu_tess_path = golden_path.c_str(),
                               .gpu_tess_path = golden_path.c_str(),
                               .coverage_aa_path = coverage_aa_path.c_str()}));
}
