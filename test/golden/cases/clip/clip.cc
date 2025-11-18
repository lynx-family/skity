// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <filesystem>
#include <skity/recorder/picture_recorder.hpp>

#include "common/golden_test_check.hpp"

static const char* kGoldenTestImageDir = CASE_DIR;

namespace {

skity::Path MakeStarPath() {
  skity::Path path;
  path.MoveTo(199, 34);
  path.LineTo(253, 143);
  path.LineTo(374, 160);
  path.LineTo(287, 244);
  path.LineTo(307, 365);
  path.LineTo(199, 309);
  path.LineTo(97, 365);
  path.LineTo(112, 245);
  path.LineTo(26, 161);
  path.LineTo(146, 143);
  path.Close();

  return path;
}

}  // namespace

TEST(ClipGolden, ClipRect) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));
  auto canvas = recorder.GetRecordingCanvas();
  canvas->Save();

  skity::Path path = MakeStarPath();

  skity::Paint paint;
  paint.SetColor(skity::Color_GREEN);
  paint.SetStyle(skity::Paint::Style::kStroke_Style);

  canvas->DrawPath(path, paint);

  skity::Rect clip_bounds = skity::Rect::MakeXYWH(20.f, 20.f, 175.f, 375.f);

  paint.SetColor(skity::Color_RED);
  canvas->DrawRect(clip_bounds, paint);

  canvas->ClipRect(clip_bounds);

  paint.SetColor(skity::Color_BLUE);
  paint.SetStyle(skity::Paint::Style::kFill_Style);

  canvas->DrawPath(path, paint);

  std::filesystem::path golden_path = kGoldenTestImageDir;
  golden_path.append("clip_rect.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 400.f, 400.f,
                                                   golden_path.c_str()));
}

TEST(ClipGolden, ClipPath) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));
  auto canvas = recorder.GetRecordingCanvas();

  auto path = MakeStarPath();

  skity::Paint paint;
  paint.SetColor(skity::Color_GREEN);
  paint.SetStyle(skity::Paint::Style::kStroke_Style);
  paint.SetStrokeWidth(1.f);

  canvas->DrawPath(path, paint);

  skity::Path clip_path;

  clip_path.MoveTo(10.f, 10.f);
  clip_path.QuadTo(300.f, 10.f, 150.f, 150.f);
  clip_path.QuadTo(10.f, 300.f, 300.f, 300.f);
  clip_path.Close();
  paint.SetColor(skity::Color_RED);

  canvas->DrawPath(clip_path, paint);

  canvas->ClipPath(clip_path);

  paint.SetColor(skity::Color_BLUE);
  paint.SetStyle(skity::Paint::Style::kFill_Style);
  canvas->DrawPath(path, paint);

  std::filesystem::path golden_path = kGoldenTestImageDir;
  golden_path.append("clip_path.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 400.f, 400.f,
                                                   golden_path.c_str()));
}

TEST(ClipGolden, ClipPathDifference) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));
  auto canvas = recorder.GetRecordingCanvas();

  auto path = MakeStarPath();

  skity::Paint paint;
  paint.SetColor(skity::Color_GREEN);
  paint.SetStyle(skity::Paint::Style::kStroke_Style);
  paint.SetStrokeWidth(1.f);

  canvas->DrawPath(path, paint);

  skity::Path clip_path;

  clip_path.MoveTo(10.f, 10.f);
  clip_path.QuadTo(300.f, 10.f, 150.f, 150.f);
  clip_path.QuadTo(10.f, 300.f, 300.f, 300.f);
  clip_path.Close();
  paint.SetColor(skity::Color_RED);

  canvas->DrawPath(clip_path, paint);

  canvas->ClipPath(clip_path, skity::Canvas::ClipOp::kDifference);

  paint.SetColor(skity::Color_BLUE);
  paint.SetStyle(skity::Paint::Style::kFill_Style);
  canvas->DrawPath(path, paint);

  std::filesystem::path golden_path = kGoldenTestImageDir;
  golden_path.append("clip_path_difference.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 400.f, 400.f,
      skity::testing::PathList{.cpu_tess_path = golden_path.c_str(),
                               .gpu_tess_path = golden_path.c_str()}));
}
