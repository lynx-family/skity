// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <filesystem>
#include <skity/recorder/picture_recorder.hpp>
#include <skity/skity.hpp>
#include <string>

#include "common/golden_test_check.hpp"
#include "skity/geometry/rect.hpp"

static const char* kGoldenTestImageSimpleDir = CASE_DIR "simple_images/";
static const char* kGoldenTestImageCPUTessDir = CASE_DIR "cpu_tess_images/";
static const char* kGoldenTestImageGPUTessDir = CASE_DIR "gpu_tess_images/";

namespace {

struct PathListContext {
  PathListContext(std::string name)
      : expected_image_cpu_tess_path(kGoldenTestImageCPUTessDir),
        expected_image_gpu_tess_path(kGoldenTestImageGPUTessDir),
        expected_image_simple_path(kGoldenTestImageSimpleDir) {
    expected_image_cpu_tess_path.append(name);
    expected_image_gpu_tess_path.append(name);
    expected_image_simple_path.append(name);
  }

  skity::testing::PathList ToPathList() const {
    return {
        .cpu_tess_path = expected_image_cpu_tess_path.c_str(),
        .gpu_tess_path = expected_image_gpu_tess_path.c_str(),
        .simple_shape_path = expected_image_simple_path.c_str(),
    };
  }

  std::filesystem::path expected_image_cpu_tess_path;
  std::filesystem::path expected_image_gpu_tess_path;
  std::filesystem::path expected_image_simple_path;
};

}  // namespace

TEST(SimpleShapeGolden, DrawFilledRect) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));

  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint paint;
  paint.SetColor(skity::Color_GREEN);

  canvas->Save();
  canvas->Translate(50.f, 50.f);
  canvas->DrawRect(skity::Rect::MakeWH(50, 50), paint);

  canvas->Translate(100.3f, 50.f);
  canvas->DrawRect(skity::Rect::MakeWH(50, 50), paint);
  canvas->Restore();

  PathListContext context("draw_filled_rect.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 400, 400,
                                                   context.ToPathList()));
}

TEST(SimpleShapeGolden, DrawStrokeRect) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));

  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint paint;
  paint.SetColor(skity::Color_GREEN);
  paint.SetStrokeWidth(1);
  paint.SetStyle(skity::Paint::kStroke_Style);
  canvas->Save();
  canvas->Translate(3.f, 50.f);
  canvas->DrawRect(skity::Rect::MakeWH(50, 50), paint);

  canvas->Translate(100, 0);
  paint.SetStrokeWidth(20);
  canvas->DrawRect(skity::Rect::MakeWH(50, 50), paint);

  canvas->Translate(100, 0);
  paint.SetStrokeWidth(49);
  canvas->DrawRect(skity::Rect::MakeWH(50, 50), paint);

  canvas->Translate(120, 0);
  paint.SetStrokeWidth(50);
  canvas->DrawRect(skity::Rect::MakeWH(50, 50), paint);
  canvas->Restore();
  auto dl = recorder.FinishRecording();
  PathListContext context("draw_stroke_rect.png");
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 400, 400,
                                                   context.ToPathList()));
}

TEST(SimpleShapeGolden, DrawStrokeRectWithJoins) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));

  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint paint;
  paint.SetColor(skity::Color_GREEN);
  paint.SetStrokeWidth(20);
  paint.SetStyle(skity::Paint::kStroke_Style);
  canvas->Save();
  canvas->Translate(30.f, 50.f);
  paint.SetStrokeJoin(skity::Paint::kBevel_Join);
  canvas->DrawRect(skity::Rect::MakeWH(100, 100), paint);

  canvas->Translate(130, 0);
  paint.SetStrokeJoin(skity::Paint::kRound_Join);
  canvas->DrawRect(skity::Rect::MakeWH(100, 100), paint);

  canvas->Translate(130, 0);
  paint.SetStrokeJoin(skity::Paint::kMiter_Join);
  canvas->DrawRect(skity::Rect::MakeWH(100, 100), paint);
  canvas->Restore();

  auto dl = recorder.FinishRecording();
  PathListContext context("draw_stroke_rect_with_joins.png");
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 400, 400,
                                                   context.ToPathList()));
}

TEST(SimpleShapeGolden, DrawFilledRRect) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));

  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint paint;
  paint.SetColor(skity::Color_GREEN);
  canvas->Save();

  canvas->Translate(3.f, 50.f);
  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 150), 20, 20),
      paint);

  canvas->Translate(110, 0);
  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 150), 20, 30),
      paint);

  canvas->Translate(110, 0);

  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 150), 50, 75),
      paint);

  canvas->Restore();

  auto dl = recorder.FinishRecording();
  PathListContext context("draw_filled_rrect.png");
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 400, 400,
                                                   context.ToPathList()));
}

TEST(SimpleShapeGolden, DrawStrokeRRect) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));

  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint paint;
  paint.SetColor(skity::Color_GREEN);
  paint.SetStrokeWidth(10);
  paint.SetStyle(skity::Paint::kStroke_Style);

  canvas->Save();

  canvas->Translate(3.f, 50.f);
  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 150), 20, 20),
      paint);

  canvas->Translate(130, 0);
  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 150), 20, 30),
      paint);

  canvas->Translate(130, 0);

  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 150), 50, 75),
      paint);

  canvas->Restore();

  auto dl = recorder.FinishRecording();
  PathListContext context("draw_stroke_rrect.png");
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 400, 400,
                                                   context.ToPathList()));
}

TEST(SimpleShapeGolden, DrawStrokeRRect2) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(600.f, 400.f));

  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint paint;
  paint.SetColor(skity::Color_GREEN);

  paint.SetStyle(skity::Paint::kStroke_Style);

  canvas->Save();

  canvas->Translate(30.f, 50.f);
  paint.SetStrokeWidth(10);
  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 150), 20, 20),
      paint);

  canvas->Translate(130, 0);
  paint.SetStrokeWidth(20);
  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 150), 20, 20),
      paint);

  canvas->Translate(150, 0);
  paint.SetStrokeWidth(40);
  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 150), 20, 20),
      paint);

  canvas->Restore();
  auto dl = recorder.FinishRecording();
  PathListContext context("draw_stroke_rrect2.png");
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 600, 400,
                                                   context.ToPathList()));
}

TEST(SimpleShapeGolden, DrawStrokeRRectWithRotate) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));

  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint paint;
  paint.SetColor(skity::Color_GREEN);
  paint.SetStrokeWidth(10);
  paint.SetStyle(skity::Paint::kStroke_Style);

  canvas->Save();
  canvas->Rotate(30);
  canvas->Translate(120, -30);

  canvas->Translate(3.f, 50.f);
  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 150), 20, 20),
      paint);

  canvas->Translate(130, 0);
  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 150), 20, 30),
      paint);

  canvas->Translate(130, 0);

  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 150), 50, 75),
      paint);

  canvas->Restore();
  auto dl = recorder.FinishRecording();
  PathListContext context("draw_stroke_rrect_with_rotate.png");

  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 400, 400,
                                                   context.ToPathList()));
}

TEST(SimpleShapeGolden, DrawStrokeRRectWithSkew) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(600.f, 400.f));

  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint paint;
  paint.SetColor(skity::Color_GREEN);
  paint.SetStrokeWidth(10);
  paint.SetStyle(skity::Paint::kStroke_Style);

  canvas->Save();
  canvas->Skew(-0.5, 0);

  canvas->Translate(160.f, 50.f);
  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 150), 20, 20),
      paint);

  canvas->Translate(130, 0);
  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 150), 20, 30),
      paint);

  canvas->Translate(130, 0);

  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 150), 50, 75),
      paint);

  canvas->Restore();

  auto dl = recorder.FinishRecording();
  PathListContext context("draw_stroke_rrect_with_skew.png");
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 600, 400,
                                                   context.ToPathList()));
}

TEST(SimpleShapeGolden, DrawStrokeRRectWithScale) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));

  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint paint;
  paint.SetColor(skity::Color_GREEN);
  paint.SetStrokeWidth(1);
  paint.SetStyle(skity::Paint::kStroke_Style);

  canvas->Save();
  canvas->Scale(10, 10);

  canvas->Translate(0.3f, 5.f);
  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 10, 15), 2, 2),
      paint);

  canvas->Translate(13, 0);
  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 10, 15), 2, 3),
      paint);

  canvas->Translate(13, 0);

  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 10, 15), 5, 7.5),
      paint);

  canvas->Restore();

  auto dl = recorder.FinishRecording();
  PathListContext context("draw_stroke_rrect_with_scale.png");

  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 400, 400,
                                                   context.ToPathList()));
}
