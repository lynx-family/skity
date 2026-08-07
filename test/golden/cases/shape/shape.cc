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
static const char* kGoldenTestCoverageAAImageDir =
    CASE_DIR "coverage_aa_images/";

namespace {

std::filesystem::path CoverageAAGoldenPath(const char* name) {
  std::filesystem::path path(kGoldenTestCoverageAAImageDir);
  path.append(name);
  return path;
}

struct ShapePathListContext {
  explicit ShapePathListContext(const char* name)
      : expected_path(kGoldenTestImageDir),
        coverage_aa_path(CoverageAAGoldenPath(name)) {
    expected_path.append(name);
  }

  skity::testing::PathList ToPathList() const {
    return {
        .cpu_tess_path = expected_path.c_str(),
        .gpu_tess_path = expected_path.c_str(),
        .coverage_aa_path = coverage_aa_path.c_str(),
    };
  }

  std::filesystem::path expected_path;
  std::filesystem::path coverage_aa_path;
};

bool CompareShapeGolden(skity::PictureRecorder& recorder, uint32_t width,
                        uint32_t height, const char* name) {
  ShapePathListContext context(name);
  auto display_list = recorder.FinishRecording();
  return skity::testing::CompareGoldenTexture(display_list.get(), width, height,
                                              context.ToPathList());
}

void TestCoverageAAConflationCorrectionTriangleRect(
    skity::Path::PathFillType fill_type) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(256.f, 256.f));
  auto canvas = recorder.GetRecordingCanvas();
  canvas->Clear(skity::Color_BLACK);

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::Color_YELLOW);

  constexpr float kLeft = 48.f;
  constexpr float kTop = 48.f;
  constexpr float kRight = 208.f;
  constexpr float kBottom = 208.f;

  skity::Path path;
  path.MoveTo(kLeft, kTop);
  path.LineTo(kRight, kTop);
  path.LineTo(kRight, kBottom);
  path.Close();

  path.MoveTo(kLeft, kTop);
  path.LineTo(kLeft, kBottom);
  path.LineTo(kRight, kBottom);
  path.Close();
  path.SetFillType(fill_type);

  canvas->DrawPath(path, paint);

  auto dl = recorder.FinishRecording();
  auto correction_off_path =
      CoverageAAGoldenPath("triangle_rect_conflation_off.png");
  auto correction_on_path =
      CoverageAAGoldenPath("triangle_rect_conflation_on.png");

  skity::testing::GoldenTestEnvConfig correction_off;
  correction_off.enable_coverage_aa = true;
  correction_off.sample_count = 1;
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 256, 256, correction_off_path.c_str(), correction_off));

  skity::testing::GoldenTestEnvConfig correction_on = correction_off;
  correction_on.enable_conflation_correction = true;
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 256, 256, correction_on_path.c_str(), correction_on));
}

}  // namespace

namespace {

skity::Path MakeCatHeadPath() {
  skity::Path path;

  path.MoveTo(-16.539f, -28.104f);
  path.CubicTo(-17.643f, -28.136f, -19.440f, -25.925f, -20.838f, -23.154f);
  path.CubicTo(-23.157f, -18.556f, -24.958f, -12.417f, -24.958f, -12.417f);
  path.LineTo(-25.989f, -8.666f);
  path.CubicTo(-25.989f, -8.666f, -29.171f, -3.228f, -29.669f, 2.129f);
  path.CubicTo(-30.198f, 7.815f, -29.179f, 14.140f, -23.104f, 19.063f);
  path.CubicTo(-19.060f, 22.340f, -15.698f, 24.053f, -12.331f, 24.900f);
  path.CubicTo(-10.336f, 25.402f, -5.067f, 26.521f, 0.176f, 26.520f);
  path.CubicTo(5.541f, 26.519f, 10.549f, 25.387f, 12.295f, 24.923f);
  path.CubicTo(15.501f, 24.071f, 18.384f, 23.172f, 22.896f, 19.708f);
  path.CubicTo(28.624f, 15.310f, 30.589f, 8.891f, 29.999f, 3.034f);
  path.CubicTo(29.410f, -2.811f, 26.000f, -8.681f, 26.000f, -8.681f);
  path.CubicTo(26.000f, -8.681f, 25.538f, -10.640f, 25.313f, -11.417f);
  path.CubicTo(24.568f, -13.992f, 22.830f, -19.497f, 21.003f, -23.073f);
  path.CubicTo(19.513f, -25.990f, 17.713f, -28.039f, 16.292f, -28.167f);
  path.CubicTo(14.086f, -28.365f, 7.708f, -21.062f, 7.708f, -21.062f);
  path.CubicTo(7.708f, -21.062f, 3.205f, -22.021f, -0.146f, -22.021f);
  path.CubicTo(-4.086f, -22.021f, -7.563f, -21.104f, -7.563f, -21.104f);
  path.CubicTo(-7.563f, -21.104f, -13.170f, -28.007f, -16.539f, -28.104f);
  path.Close();
  return path;
}

}  // namespace

TEST(ShapeGolden, CatHeadPath) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));
  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint background;
  background.SetColor(skity::ColorSetARGB(0xFF, 0x18, 0x18, 0x18));
  canvas->DrawPaint(background);

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kStroke_Style);
  paint.SetStrokeCap(skity::Paint::kRound_Cap);
  paint.SetStrokeJoin(skity::Paint::kMiter_Join);
  paint.SetStrokeWidth(3.5f);
  paint.SetColor(skity::ColorSetARGB(0x4D, 0xFF, 0xFF, 0xFF));

  auto cat_head = MakeCatHeadPath();

  canvas->Save();
  canvas->Translate(200.f, 200.f);
  canvas->Scale(4.f, 4.f);
  canvas->DrawPath(cat_head, paint);
  canvas->Restore();

  std::filesystem::path expected_image_path(kGoldenTestImageDir);
  std::filesystem::path expected_image_gpu_tess_path(kGoldenTestImageDir);
  auto expected_coverage_aa_path = CoverageAAGoldenPath("cat_head_path.png");
  expected_image_path.append("cat_head_path.png");
  expected_image_gpu_tess_path.append("cat_head_path_gpu_tess.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 400, 400,
      skity::testing::PathList{
          .cpu_tess_path = expected_image_path.c_str(),
          .gpu_tess_path = expected_image_gpu_tess_path.c_str(),
          .coverage_aa_path = expected_coverage_aa_path.c_str()}));
}

TEST(ShapeGolden, StrokeMiterLimit) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 200.f));

  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint paint;
  paint.SetAntiAlias(true);
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
  auto expected_coverage_aa_path =
      CoverageAAGoldenPath("stroke_miter_limit.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 400, 200,
      skity::testing::PathList{
          .cpu_tess_path = expected_image_path.c_str(),
          .gpu_tess_path = expected_image_path.c_str(),
          .coverage_aa_path = expected_coverage_aa_path.c_str()}));
}

TEST(ShapeGolden, LargeStrokeWidth) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200.f, 100.f));
  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint paint;
  paint.SetAntiAlias(true);
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
  auto expected_coverage_aa_path =
      CoverageAAGoldenPath("large_stroke_width.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 200, 100,
      skity::testing::PathList{
          .cpu_tess_path = expected_image_path.c_str(),
          .gpu_tess_path = expected_image_path.c_str(),
          .coverage_aa_path = expected_coverage_aa_path.c_str()}));
}

TEST(ShapeGolden, TinyStrokeWidth) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200.f, 100.f));
  auto canvas = recorder.GetRecordingCanvas();
  canvas->Clear(skity::Color_WHITE);
  skity::Paint paint;

  paint.SetAntiAlias(true);
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
  auto expected_coverage_aa_path =
      CoverageAAGoldenPath("tiny_stroke_width.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 200, 100,
      skity::testing::PathList{
          .cpu_tess_path = expected_image_path.c_str(),
          .gpu_tess_path = expected_image_path.c_str(),
          .coverage_aa_path = expected_coverage_aa_path.c_str()}));
}

TEST(ShapeGolden, ScaledTinyStrokeWidth) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200.f, 100.f));
  auto canvas = recorder.GetRecordingCanvas();
  canvas->Clear(skity::Color_WHITE);

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::Color_RED);
  paint.SetStyle(skity::Paint::kStroke_Style);
  paint.SetStrokeWidth(0.08f);
  paint.SetStrokeCap(skity::Paint::kButt_Cap);

  skity::Path path;
  path.MoveTo(0.f, 0.f);
  path.LineTo(1.f, 0.f);
  path.LineTo(0.5f, 0.7f);
  path.Close();

  canvas->Save();
  canvas->Translate(40.f, 25.f);
  canvas->Scale(120.f, 120.f);
  canvas->DrawPath(path, paint);
  canvas->Restore();

  std::filesystem::path expected_image_path(kGoldenTestImageDir);
  expected_image_path.append("scaled_tiny_stroke_width.png");
  auto expected_coverage_aa_path =
      CoverageAAGoldenPath("scaled_tiny_stroke_width.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 200, 100,
      skity::testing::PathList{
          .cpu_tess_path = expected_image_path.c_str(),
          .gpu_tess_path = expected_image_path.c_str(),
          .coverage_aa_path = expected_coverage_aa_path.c_str()}));
}

TEST(ShapeGolden, ScaledBlurMaskFilter) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(500.f, 500.f));
  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::Color_WHITE);
  canvas->DrawPaint(paint);

  paint.SetColor(skity::Color_RED);
  paint.SetMaskFilter(
      skity::MaskFilter::MakeBlur(skity::BlurStyle::kNormal, 20.f));
  canvas->Save();
  canvas->Scale(2.f, 2.f);
  canvas->DrawRect(skity::Rect::MakeLTRB(100.f, 100.f, 200.f, 200.f), paint);

  paint.SetColor(skity::Color_GREEN);
  paint.SetMaskFilter(nullptr);
  canvas->DrawRect(skity::Rect::MakeLTRB(100.f, 100.f, 200.f, 200.f), paint);
  canvas->Restore();

  std::filesystem::path expected_image_path(kGoldenTestImageDir);
  expected_image_path.append("scaled_blur_mask_filter.png");
  auto expected_coverage_aa_path =
      CoverageAAGoldenPath("scaled_blur_mask_filter.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 500, 500,
      skity::testing::PathList{
          .cpu_tess_path = expected_image_path.c_str(),
          .gpu_tess_path = expected_image_path.c_str(),
          .coverage_aa_path = expected_coverage_aa_path.c_str()}));
}

TEST(ShapeGolden, StrokeJoinAndCap) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(500.f, 200.f));

  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint paint;
  paint.SetAntiAlias(true);
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
  auto expected_coverage_aa_path =
      CoverageAAGoldenPath("stroke_join_and_cap.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 500, 200,
      skity::testing::PathList{
          .cpu_tess_path = expected_image_path.c_str(),
          .gpu_tess_path = expected_image_path.c_str(),
          .coverage_aa_path = expected_coverage_aa_path.c_str()}));
}

TEST(ShapeGolden, PathTransformFillType) {
  skity::PictureRecorder recorder;

  recorder.BeginRecording(skity::Rect::MakeWH(400, 200));

  skity::Path path;

  path.AddCircle(100.f, 100.f, 80.f);
  path.AddCircle(100.f, 100.f, 30.f);
  path.SetFillType(skity::Path::PathFillType::kEvenOdd);

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::Color_RED);

  skity::Matrix m{};
  m.PostTranslate(200.f, 0.f);

  recorder.GetRecordingCanvas()->DrawPath(path.CopyWithScale(0.5f), paint);
  recorder.GetRecordingCanvas()->DrawPath(path.CopyWithMatrix(m), paint);

  std::filesystem::path expected_image_path(kGoldenTestImageDir);
  expected_image_path.append("path_copy_fill_typpe.png");
  auto expected_coverage_aa_path =
      CoverageAAGoldenPath("path_copy_fill_typpe.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 400, 200,
      skity::testing::PathList{
          .cpu_tess_path = expected_image_path.c_str(),
          .gpu_tess_path = expected_image_path.c_str(),
          .coverage_aa_path = expected_coverage_aa_path.c_str()}));
}

TEST(ShapeGolden, DrawEmptyPath) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 200.f));

  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint paint;
  paint.SetAntiAlias(true);
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
  auto expected_coverage_aa_path = CoverageAAGoldenPath("draw_empty_path.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 400, 200,
      skity::testing::PathList{
          .cpu_tess_path = expected_image_path.c_str(),
          .gpu_tess_path = expected_image_path.c_str(),
          .coverage_aa_path = expected_coverage_aa_path.c_str()}));
}

// https://dev.w3.org/SVG/tools/svgweb/samples/svg-files/check.svg
TEST(ShapeGolden, DrawCheck) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));
  auto canvas = recorder.GetRecordingCanvas();
  auto path_opt = skity::ParsePath::FromSVGString(
      R"(M30,76q6-14,13-26q6-12,14-23q8-12,13-17q3-4,6-6q1-1,5-2q8-1,12-1q1,0,1,1q0,1-1,2q-13,11-27,33q-14,21-24,44q-4,9-5,11q-1,2-9,2q-5,0-6-1q-1-1-5-6q-5-8-12-15q-3-4-3-6q0-2,4-5q3-2,6-2q3,0,8,3q5,4,10,14z)");
  ASSERT_TRUE(path_opt.has_value());
  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::Color_GREEN);
  canvas->Scale(4, 4);
  canvas->DrawColor(skity::Color_WHITE);
  canvas->DrawPath(path_opt.value(), paint);

  std::filesystem::path expected_image_path(kGoldenTestImageDir);
  expected_image_path.append("draw_check.png");
  auto expected_coverage_aa_path = CoverageAAGoldenPath("draw_check.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 400, 400,
      skity::testing::PathList{
          .cpu_tess_path = expected_image_path.c_str(),
          .gpu_tess_path = expected_image_path.c_str(),
          .coverage_aa_path = expected_coverage_aa_path.c_str()}));
}

TEST(ShapeGolden, DrawCheck2) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));
  auto canvas = recorder.GetRecordingCanvas();

  auto path_opt = skity::ParsePath::FromSVGString(R"(M6 12.5L10.4 17L18 6)");
  ASSERT_TRUE(path_opt.has_value());
  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(0xFFFFFFFF);
  canvas->Scale(20.f, 20.f);
  paint.SetStyle(skity::Paint::kStroke_Style);
  paint.SetStrokeWidth(2.f);
  paint.SetStrokeCap(skity::Paint::kRound_Cap);
  paint.SetStrokeJoin(skity::Paint::kRound_Join);
  canvas->DrawPath(path_opt.value(), paint);

  std::filesystem::path expected_image_path(kGoldenTestImageDir);
  expected_image_path.append("draw_check2.png");
  auto expected_coverage_aa_path = CoverageAAGoldenPath("draw_check2.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 400, 400,
      skity::testing::PathList{
          .cpu_tess_path = expected_image_path.c_str(),
          .gpu_tess_path = expected_image_path.c_str(),
          .coverage_aa_path = expected_coverage_aa_path.c_str()}));
}

TEST(ShapeGolden, DrawDegenerateCubic) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));
  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint paint;
  paint.SetAntiAlias(true);
  skity::Path path;
  path.MoveTo(100, 100);
  path.CubicTo(100, 100, 300, 100, 300, 100);
  path.CubicTo(300, 100, 300, 300, 300, 300);
  path.CubicTo(300, 300, 100, 300, 100, 300);
  path.Close();

  paint.SetColor(skity::Color_GREEN);
  paint.SetStrokeJoin(skity::Paint::Join::kBevel_Join);
  paint.SetStyle(skity::Paint::kStroke_Style);

  paint.SetStrokeWidth(40.f);
  canvas->DrawPath(path, paint);

  std::filesystem::path expected_image_path(kGoldenTestImageDir);
  expected_image_path.append("draw_degenerate_cubic.png");
  auto expected_coverage_aa_path =
      CoverageAAGoldenPath("draw_degenerate_cubic.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 400, 400,
      skity::testing::PathList{
          .cpu_tess_path = expected_image_path.c_str(),
          .gpu_tess_path = expected_image_path.c_str(),
          .coverage_aa_path = expected_coverage_aa_path.c_str()}));
}

TEST(ShapeGolden, TileBoundaryCrossings) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(192.f, 144.f));
  auto canvas = recorder.GetRecordingCanvas();
  canvas->Clear(skity::Color_WHITE);

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::ColorSetARGB(255, 30, 136, 229));

  skity::Path path;
  path.AddRect(skity::Rect::MakeLTRB(16.f, 16.f, 80.f, 64.f));
  path.MoveTo(112.f, 16.f);
  path.LineTo(176.f, 48.f);
  path.LineTo(112.f, 80.f);
  path.LineTo(80.f, 48.f);
  path.Close();
  path.MoveTo(16.f, 95.5f);
  path.LineTo(176.f, 96.5f);
  path.LineTo(176.f, 112.5f);
  path.LineTo(16.f, 111.5f);
  path.Close();
  canvas->DrawPath(path, paint);

  EXPECT_TRUE(CompareShapeGolden(recorder, 192, 144, "tile_boundaries.png"));
}

TEST(CoverageAAGoldenComparison, ExactModeDetectsOneChannelDifference) {
  auto source = std::make_shared<skity::Pixmap>(
      1, 1, skity::AlphaType::kPremul_AlphaType, skity::ColorType::kRGBA);
  auto target = std::make_shared<skity::Pixmap>(
      1, 1, skity::AlphaType::kPremul_AlphaType, skity::ColorType::kRGBA);
  auto* source_pixel = source->WritableAddr8(0, 0);
  auto* target_pixel = target->WritableAddr8(0, 0);
  source_pixel[0] = 64;
  source_pixel[1] = 64;
  source_pixel[2] = 64;
  source_pixel[3] = 128;
  target_pixel[0] = 65;
  target_pixel[1] = 64;
  target_pixel[2] = 64;
  target_pixel[3] = 128;

  EXPECT_EQ(skity::testing::ComparePixels(source, target).diff_pixel_count, 0u);
  EXPECT_EQ(skity::testing::ComparePixelsExact(source, target).diff_pixel_count,
            1u);
}

TEST(ShapeGolden, CanonicalEdgesExact) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(192.f, 144.f));
  auto canvas = recorder.GetRecordingCanvas();
  canvas->Clear(skity::Color_BLACK);

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::Color_WHITE);

  skity::Path winding;
  winding.AddRect(skity::Rect::MakeLTRB(8.5f, 8.5f, 31.5f, 31.5f));
  winding.MoveTo(40.f, 8.f);
  winding.LineTo(72.f, 24.f);
  winding.LineTo(40.f, 40.f);
  winding.Close();
  winding.MoveTo(88.f, 8.f);
  winding.LineTo(88.f, 40.f);
  winding.LineTo(120.f, 24.f);
  winding.Close();
  winding.MoveTo(16.f, 48.f);
  winding.LineTo(48.f, 64.f);
  winding.LineTo(16.f, 80.f);
  winding.LineTo(0.f, 64.f);
  winding.Close();
  winding.AddRect(skity::Rect::MakeLTRB(128.5f, 8.5f, 151.5f, 87.5f));
  canvas->DrawPath(winding, paint);

  skity::Path even_odd;
  even_odd.AddRect(skity::Rect::MakeLTRB(72.5f, 48.5f, 119.5f, 87.5f));
  even_odd.AddRect(skity::Rect::MakeLTRB(88.5f, 60.5f, 103.5f, 76.5f));
  even_odd.SetFillType(skity::Path::PathFillType::kEvenOdd);
  canvas->DrawPath(even_odd, paint);

  ShapePathListContext context("canonical_edges_exact.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 192, 144,
      skity::testing::PathList{
          .cpu_tess_path = context.expected_path.c_str(),
          .gpu_tess_path = context.expected_path.c_str()}));

  skity::testing::GoldenTestEnvConfig config;
  config.enable_coverage_aa = true;
  config.sample_count = 1;
  config.require_exact_pixel_match = true;
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 192, 144, context.coverage_aa_path.c_str(), config));
}

TEST(ShapeGolden, CoverageAAConflationCorrectionTriangleRect) {
  TestCoverageAAConflationCorrectionTriangleRect(
      skity::Path::PathFillType::kWinding);
}

TEST(ShapeGolden, CoverageAAConflationCorrectionTriangleRectEvenOdd) {
  TestCoverageAAConflationCorrectionTriangleRect(
      skity::Path::PathFillType::kEvenOdd);
}

TEST(ShapeGolden, WindingContours) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(288.f, 160.f));
  auto canvas = recorder.GetRecordingCanvas();
  canvas->Clear(skity::Color_WHITE);

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::ColorSetARGB(255, 0, 150, 136));

  skity::Path path;
  path.AddRect(skity::Rect::MakeLTRB(16.f, 16.f, 128.f, 144.f));
  path.MoveTo(48.f, 48.f);
  path.LineTo(48.f, 112.f);
  path.LineTo(96.f, 112.f);
  path.LineTo(96.f, 48.f);
  path.Close();

  path.AddRect(skity::Rect::MakeLTRB(160.f, 16.f, 272.f, 144.f));
  path.AddRect(skity::Rect::MakeLTRB(192.f, 48.f, 240.f, 112.f));
  canvas->DrawPath(path, paint);

  EXPECT_TRUE(CompareShapeGolden(recorder, 288, 160, "winding_contours.png"));
}

TEST(ShapeGolden, EvenOddContours) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(128.f, 128.f));
  auto canvas = recorder.GetRecordingCanvas();
  canvas->Clear(skity::Color_WHITE);

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::ColorSetARGB(255, 0, 150, 136));

  skity::Path path;
  path.AddRect(skity::Rect::MakeLTRB(16.f, 16.f, 112.f, 112.f));
  path.AddRect(skity::Rect::MakeLTRB(48.f, 48.f, 80.f, 80.f));
  path.SetFillType(skity::Path::PathFillType::kEvenOdd);
  canvas->DrawPath(path, paint);

  EXPECT_TRUE(CompareShapeGolden(recorder, 128, 128, "even_odd_contours.png"));
}

TEST(ShapeGolden, Curves) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(256.f, 160.f));
  auto canvas = recorder.GetRecordingCanvas();
  canvas->Clear(skity::Color_WHITE);

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::ColorSetARGB(255, 142, 36, 170));

  skity::Path path;
  path.AddCircle(56.f, 80.f, 40.f);
  path.MoveTo(112.f, 128.f);
  path.QuadTo(144.f, 8.f, 176.f, 64.f);
  path.CubicTo(240.f, 96.f, 224.f, 152.f, 112.f, 128.f);
  path.Close();
  canvas->DrawPath(path, paint);

  EXPECT_TRUE(CompareShapeGolden(recorder, 256, 160, "curves.png"));
}

TEST(ShapeGolden, ConcaveAndSelfIntersecting) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(288.f, 160.f));
  auto canvas = recorder.GetRecordingCanvas();
  canvas->Clear(skity::Color_WHITE);

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::ColorSetARGB(255, 239, 108, 0));

  skity::Path path;
  path.MoveTo(16.f, 16.f);
  path.LineTo(128.f, 16.f);
  path.LineTo(128.f, 48.f);
  path.LineTo(48.f, 48.f);
  path.LineTo(48.f, 112.f);
  path.LineTo(128.f, 112.f);
  path.LineTo(128.f, 144.f);
  path.LineTo(16.f, 144.f);
  path.Close();

  path.MoveTo(210.f, 16.f);
  path.LineTo(249.f, 136.f);
  path.LineTo(146.f, 61.f);
  path.LineTo(274.f, 61.f);
  path.LineTo(171.f, 136.f);
  path.Close();
  canvas->DrawPath(path, paint);

  EXPECT_TRUE(
      CompareShapeGolden(recorder, 288, 160, "concave_self_intersecting.png"));
}

TEST(ShapeGolden, SaveLayerIsolation) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(224.f, 160.f));
  auto canvas = recorder.GetRecordingCanvas();
  canvas->Clear(skity::Color_WHITE);

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::ColorSetARGB(255, 30, 136, 229));

  skity::Path root_path;
  root_path.MoveTo(16.f, 16.f);
  root_path.LineTo(128.f, 80.f);
  root_path.LineTo(16.f, 144.f);
  root_path.Close();
  canvas->DrawPath(root_path, paint);

  canvas->SaveLayer(skity::Rect::MakeLTRB(64.f, 16.f, 208.f, 144.f),
                    skity::Paint{});
  paint.SetColor(skity::ColorSetARGB(255, 239, 108, 0));
  skity::Path layer_path;
  layer_path.MoveTo(136.f, 16.f);
  layer_path.LineTo(208.f, 80.f);
  layer_path.LineTo(136.f, 144.f);
  layer_path.LineTo(64.f, 80.f);
  layer_path.Close();
  canvas->DrawPath(layer_path, paint);
  canvas->Restore();

  EXPECT_TRUE(
      CompareShapeGolden(recorder, 224, 160, "save_layer_isolation.png"));
}
