// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <filesystem>
#include <skity/recorder/picture_recorder.hpp>

#include "common/golden_test_check.hpp"

static const char* kGoldenTestImageDir = CASE_DIR;
static const char* kGoldenTestCoverageAAImageDir =
    CASE_DIR "coverage_aa_images/";

namespace {

std::filesystem::path CoverageAAGoldenPath(const char* name) {
  std::filesystem::path path(kGoldenTestCoverageAAImageDir);
  path.append(name);
  return path;
}

struct PathListContext {
  explicit PathListContext(const char* name)
      : expected_path(kGoldenTestImageDir),
        coverage_aa_path(CoverageAAGoldenPath(name)) {
    expected_path.append(name);
  }

  skity::testing::PathList ToPathList(bool include_gpu_tess = false) const {
    return {
        .cpu_tess_path = expected_path.c_str(),
        .gpu_tess_path = include_gpu_tess ? expected_path.c_str() : nullptr,
        .coverage_aa_path = coverage_aa_path.c_str(),
    };
  }

  std::filesystem::path expected_path;
  std::filesystem::path coverage_aa_path;
};

bool CompareClipGolden(skity::PictureRecorder& recorder, const char* name,
                       bool include_gpu_tess = false) {
  PathListContext context(name);
  auto display_list = recorder.FinishRecording();
  return skity::testing::CompareGoldenTexture(
      display_list.get(), 400, 400, context.ToPathList(include_gpu_tess));
}

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

skity::Path MakeCoverageAATriangle() {
  skity::Path path;
  path.MoveTo(0.f, 112.f);
  path.LineTo(64.f, 0.f);
  path.LineTo(128.f, 112.f);
  path.Close();
  return path;
}

}  // namespace

TEST(CoverageAACoordinatesGolden, ClipUsesGlobalBounds) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(220.f, 180.f));
  auto canvas = recorder.GetRecordingCanvas();
  canvas->Clear(skity::Color_WHITE);

  canvas->ClipRect(skity::Rect::MakeXYWH(48.f, 28.f, 112.f, 112.f));

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::ColorSetARGB(255, 30, 136, 229));
  paint.SetStyle(skity::Paint::Style::kFill_Style);

  canvas->Save();
  canvas->Translate(42.f, 24.f);
  canvas->Rotate(15.f, 64.f, 56.f);
  canvas->DrawPath(MakeCoverageAATriangle(), paint);
  canvas->Restore();

  auto expected_image_path =
      CoverageAAGoldenPath("coverage_aa_clip_global_bounds.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 220.f, 180.f,
      skity::testing::PathList{.coverage_aa_path =
                                   expected_image_path.c_str()}));
}

TEST(ClipGolden, ClipRect) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));
  auto canvas = recorder.GetRecordingCanvas();
  canvas->Save();

  skity::Path path = MakeStarPath();

  skity::Paint paint;
  paint.SetAntiAlias(true);
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

  EXPECT_TRUE(CompareClipGolden(recorder, "clip_rect.png"));
}

TEST(ClipGolden, ClipPath) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));
  auto canvas = recorder.GetRecordingCanvas();

  auto path = MakeStarPath();

  skity::Paint paint;
  paint.SetAntiAlias(true);
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

  EXPECT_TRUE(CompareClipGolden(recorder, "clip_path.png"));
}

TEST(ClipGolden, ClipPathDifference) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));
  auto canvas = recorder.GetRecordingCanvas();

  auto path = MakeStarPath();

  skity::Paint paint;
  paint.SetAntiAlias(true);
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

  EXPECT_TRUE(CompareClipGolden(recorder, "clip_path_difference.png", true));
}

TEST(ClipGolden, ClipRRect) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));
  auto canvas = recorder.GetRecordingCanvas();

  skity::RRect clip_rrect = skity::RRect::MakeRectXY(
      skity::Rect::MakeXYWH(20.f, 20.f, 175.f, 375.f), 40.f, 40.f);
  canvas->ClipRRect(clip_rrect);

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::Color_BLUE);
  canvas->DrawPaint(paint);

  EXPECT_TRUE(CompareClipGolden(recorder, "clip_rrect.png"));
}

TEST(ClipGolden, ClipRRectDifference) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));
  auto canvas = recorder.GetRecordingCanvas();

  skity::RRect clip_rrect = skity::RRect::MakeRectXY(
      skity::Rect::MakeXYWH(20.f, 20.f, 175.f, 375.f), 40.f, 40.f);
  canvas->ClipRRect(clip_rrect, skity::Canvas::ClipOp::kDifference);

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::Color_BLUE);
  canvas->DrawPaint(paint);

  EXPECT_TRUE(CompareClipGolden(recorder, "clip_rrect_difference.png"));
}
