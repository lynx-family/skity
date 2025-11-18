// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <filesystem>
#include <skity/recorder/picture_recorder.hpp>

#include "common/golden_test_check.hpp"
#include "skity/effect/image_filter.hpp"
#include "skity/geometry/matrix.hpp"
#include "skity/graphic/color.hpp"

static const char* kGoldenTestImageDir = CASE_DIR;

TEST(SaveLayerGolden, TwoCircle) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));
  auto canvas = recorder.GetRecordingCanvas();

  canvas->Save();
  skity::Paint paint;
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
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 400.f, 400.f,
      skity::testing::PathList{.cpu_tess_path = golden_path.c_str(),
                               .gpu_tess_path = golden_path.c_str()}));
}

TEST(SaveLayerGolden, ThreeCircle) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));
  auto canvas = recorder.GetRecordingCanvas();

  canvas->Save();
  skity::Paint paint;
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
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 400.f, 400.f,
      skity::testing::PathList{.cpu_tess_path = golden_path.c_str(),
                               .gpu_tess_path = golden_path.c_str()}));
}

TEST(SaveLayerGolden, TwoCircleWithTranslate) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));
  auto canvas = recorder.GetRecordingCanvas();

  canvas->Save();
  skity::Paint paint;
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

  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 400.f, 400.f,
      skity::testing::PathList{.cpu_tess_path = golden_path.c_str(),
                               .gpu_tess_path = golden_path.c_str()}));
}
