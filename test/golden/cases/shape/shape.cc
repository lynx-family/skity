// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <filesystem>
#include <skity/io/parse_path.hpp>
#include <skity/recorder/picture_recorder.hpp>
#include <skity/skity.hpp>

#include "common/golden_test_check.hpp"

static const char* kGoldenTestImageDir = CASE_DIR;

TEST(ShapeGolden, StrokeMiterLimit) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 200.f));

  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint paint;
  paint.SetColor(skity::Color_RED);
  paint.SetStyle(skity::Paint::kStroke_Style);
  paint.SetStrokeWidth(20.f);
  paint.SetStrokeMiter(1.415f);

  skity::Path path;
  path.MoveTo(0, 0);
  path.LineTo(100, 0);
  path.LineTo(100, 100);
  path.LineTo(0, 100);
  path.Close();

  canvas->Save();
  canvas->Translate(50.f, 50.f);
  canvas->DrawPath(path, paint);
  canvas->Restore();

  paint.SetStrokeMiter(1.414f);

  canvas->Save();
  canvas->Translate(200.f, 50.f);
  canvas->DrawPath(path, paint);
  canvas->Restore();

  std::filesystem::path expected_image_path(kGoldenTestImageDir);
  expected_image_path.append("stroke_miter_limit.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 400, 200,
      skity::testing::PathList{.cpu_tess_path = expected_image_path.c_str(),
                               .gpu_tess_path = expected_image_path.c_str()}));
}

TEST(ShapeGolden, LargeStrokeWidth) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200.f, 100.f));
  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint paint;
  paint.SetColor(skity::Color_RED);
  paint.SetStyle(skity::Paint::kStroke_Style);
  paint.SetStrokeWidth(50.f);
  paint.SetStrokeMiter(4.f);

  canvas->Save();
  canvas->Translate(20.f, 50.f);
  canvas->DrawRect(skity::Rect::MakeWH(50.f, 0.f), paint);
  canvas->Restore();

  paint.SetStrokeJoin(skity::Paint::kRound_Join);

  canvas->Save();
  canvas->Translate(120.f, 50.f);
  canvas->DrawRect(skity::Rect::MakeWH(50.f, 0.f), paint);
  canvas->Restore();

  std::filesystem::path expected_image_path(kGoldenTestImageDir);
  expected_image_path.append("large_stroke_width.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 200, 100,
      skity::testing::PathList{.cpu_tess_path = expected_image_path.c_str(),
                               .gpu_tess_path = expected_image_path.c_str()}));
}

TEST(ShapeGolden, TinyStrokeWidth) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200.f, 100.f));
  auto canvas = recorder.GetRecordingCanvas();
  canvas->Clear(skity::Color_WHITE);
  skity::Paint paint;

  paint.SetColor(skity::Color_RED);
  paint.SetStyle(skity::Paint::kStroke_Style);
  paint.SetStrokeWidth(0.1f);
  paint.SetStrokeMiter(4.f);

  canvas->Save();
  canvas->Translate(20.f, 50.f);
  canvas->DrawRect(skity::Rect::MakeWH(50.f, 0.f), paint);
  canvas->Restore();

  paint.SetStrokeJoin(skity::Paint::kRound_Join);

  canvas->Save();
  canvas->Translate(120.f, 50.f);
  canvas->DrawRect(skity::Rect::MakeWH(50.f, 0.f), paint);
  canvas->Restore();

  std::filesystem::path expected_image_path(kGoldenTestImageDir);
  expected_image_path.append("tiny_stroke_width.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 200, 100,
      skity::testing::PathList{.cpu_tess_path = expected_image_path.c_str(),
                               .gpu_tess_path = expected_image_path.c_str()}));
}

TEST(ShapeGolden, StrokeJoinAndCap) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(500.f, 200.f));

  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint paint;
  paint.SetColor(skity::Color_RED);
  paint.SetStyle(skity::Paint::kStroke_Style);
  paint.SetStrokeWidth(20.f);
  paint.SetStrokeCap(skity::Paint::kSquare_Cap);
  paint.SetStrokeJoin(skity::Paint::kMiter_Join);

  skity::Path polyline;
  polyline.MoveTo(10, 10);
  polyline.LineTo(200, 140);
  polyline.LineTo(50, 140);
  polyline.LineTo(10, 10);

  canvas->Save();
  canvas->Translate(20.f, 20.f);
  canvas->DrawPath(polyline, paint);
  canvas->Restore();

  polyline.Close();

  canvas->Save();
  canvas->Translate(220.f, 20.f);
  canvas->DrawPath(polyline, paint);
  canvas->Restore();

  std::filesystem::path expected_image_path(kGoldenTestImageDir);
  expected_image_path.append("stroke_join_and_cap.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 500, 200,
      skity::testing::PathList{.cpu_tess_path = expected_image_path.c_str(),
                               .gpu_tess_path = expected_image_path.c_str()}));
}

TEST(ShapeGolden, PathTransformFillType) {
  skity::PictureRecorder recorder;

  recorder.BeginRecording(skity::Rect::MakeWH(400, 200));

  skity::Path path;

  path.AddCircle(100.f, 100.f, 80.f);
  path.AddCircle(100.f, 100.f, 30.f);
  path.SetFillType(skity::Path::PathFillType::kEvenOdd);

  skity::Paint paint;
  paint.SetColor(skity::Color_RED);

  skity::Matrix m{};
  m.PostTranslate(200.f, 0.f);

  recorder.GetRecordingCanvas()->DrawPath(path.CopyWithScale(0.5f), paint);
  recorder.GetRecordingCanvas()->DrawPath(path.CopyWithMatrix(m), paint);

  std::filesystem::path expected_image_path(kGoldenTestImageDir);
  expected_image_path.append("path_copy_fill_typpe.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 400, 200,
      skity::testing::PathList{.cpu_tess_path = expected_image_path.c_str(),
                               .gpu_tess_path = expected_image_path.c_str()}));
}

TEST(ShapeGolden, DrawEmptyPath) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 200.f));

  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint paint;
  paint.SetColor(skity::Color_RED);
  paint.SetStyle(skity::Paint::kStroke_Style);
  paint.SetStrokeWidth(20.f);
  paint.SetStrokeMiter(1.415f);

  skity::Path path;
  canvas->Save();
  canvas->Translate(50.f, 50.f);
  canvas->DrawPath(path, paint);
  canvas->Restore();

  canvas->Save();
  path.MoveTo(100, 100);
  canvas->DrawPath(path, paint);
  canvas->Restore();

  canvas->Save();
  path.MoveTo(200, 200);
  path.MoveTo(300, 100);
  canvas->DrawPath(path, paint);
  canvas->Restore();

  std::filesystem::path expected_image_path(kGoldenTestImageDir);
  expected_image_path.append("draw_empty_path.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 400, 200,
      skity::testing::PathList{.cpu_tess_path = expected_image_path.c_str(),
                               .gpu_tess_path = expected_image_path.c_str()}));
}

// https://dev.w3.org/SVG/tools/svgweb/samples/svg-files/check.svg
TEST(ShapeGolden, DrawCheck) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 200.f));
  auto canvas = recorder.GetRecordingCanvas();
  auto path_opt = skity::ParsePath::FromSVGString(
      R"(M30,76q6-14,13-26q6-12,14-23q8-12,13-17q3-4,6-6q1-1,5-2q8-1,12-1q1,0,1,1q0,1-1,2q-13,11-27,33q-14,21-24,44q-4,9-5,11q-1,2-9,2q-5,0-6-1q-1-1-5-6q-5-8-12-15q-3-4-3-6q0-2,4-5q3-2,6-2q3,0,8,3q5,4,10,14z)");
  ASSERT_TRUE(path_opt.has_value());
  skity::Paint paint;
  paint.SetColor(skity::Color_GREEN);
  canvas->Scale(4, 4);
  canvas->DrawColor(skity::Color_WHITE);
  canvas->DrawPath(path_opt.value(), paint);

  std::filesystem::path expected_image_path(kGoldenTestImageDir);
  expected_image_path.append("draw_check.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 400, 400,
      skity::testing::PathList{.cpu_tess_path = expected_image_path.c_str(),
                               .gpu_tess_path = expected_image_path.c_str()}));
}

