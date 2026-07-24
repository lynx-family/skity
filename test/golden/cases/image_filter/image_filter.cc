// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <filesystem>
#include <skity/effect/image_filter.hpp>
#include <skity/recorder/picture_recorder.hpp>

#include "common/golden_test_check.hpp"

constexpr const char* kGoldenTestDir = CASE_DIR;
constexpr const char* kGoldenTestCoverageAADir = CASE_DIR "coverage_aa_images/";

namespace {

struct PathListContext {
  explicit PathListContext(const char* name)
      : expected_path(kGoldenTestDir),
        coverage_aa_path(kGoldenTestCoverageAADir) {
    expected_path.append(name);
    coverage_aa_path.append(name);
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

}  // namespace

TEST(ImageFilterGolden, BlurFilter_10_5) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200, 200));

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::Color_RED);
  paint.SetImageFilter(skity::ImageFilters::Blur(10, 5));

  auto canvas = recorder.GetRecordingCanvas();

  canvas->Save();
  canvas->Translate(50.f, 50.f);
  canvas->DrawRect(skity::Rect::MakeWH(100, 100), paint);
  canvas->Restore();

  PathListContext context("blur_filter_10_5.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 200, 200,
                                                   context.ToPathList()));
}

TEST(ImageFilterGolden, BlurFilter_10_5_Perspective) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200, 200));

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::Color_RED);
  paint.SetImageFilter(skity::ImageFilters::Blur(10, 5));

  auto canvas = recorder.GetRecordingCanvas();

  canvas->Save();
  canvas->Translate(50.f, 50.f);
  canvas->Concat(skity::Matrix{
      2000, 0, 0,
      0,  //
      0, 2000, 0,
      0,  //
      800, 1200, 1,
      1,  //
      0, 0, 2000,
      2000,  //
  });
  canvas->DrawRect(skity::Rect::MakeWH(100, 100), paint);
  canvas->Restore();

  PathListContext context("blur_filter_10_5_perspective.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 200, 200,
                                                   context.ToPathList()));
}

TEST(ImageFilterGolden, BlurFilter_10_10) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200, 200));

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::Color_RED);
  paint.SetImageFilter(skity::ImageFilters::Blur(10, 10));

  auto canvas = recorder.GetRecordingCanvas();

  canvas->Save();
  canvas->Translate(50.f, 50.f);
  canvas->Concat(skity::Matrix::RotateDeg(45.f, skity::Vec2{50.f, 50.f}));

  canvas->DrawRect(skity::Rect::MakeWH(100, 100), paint);
  canvas->Restore();

  PathListContext context("blur_filter_10_10.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 200, 200,
                                                   context.ToPathList()));
}

TEST(ImageFilterGolden, BlurFilter_10_0) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200, 200));

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::Color_RED);
  paint.SetImageFilter(skity::ImageFilters::Blur(10, 0));

  auto canvas = recorder.GetRecordingCanvas();
  canvas->Save();
  canvas->Translate(50.f, 50.f);
  canvas->DrawRect(skity::Rect::MakeWH(100, 100), paint);
  canvas->Restore();

  PathListContext context("blur_filter_10_0.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 200, 200,
                                                   context.ToPathList()));
}

TEST(ImageFilterGolden, DropShadow_0_0_10_10) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200, 200));
  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::Color_RED);
  paint.SetImageFilter(skity::ImageFilters::DropShadow(
      0, 0, 10, 10, skity::Color_GREEN, nullptr));

  auto canvas = recorder.GetRecordingCanvas();
  canvas->Save();
  canvas->Translate(50.f, 50.f);
  canvas->DrawRect(skity::Rect::MakeWH(100, 100), paint);
  canvas->Restore();
  PathListContext context("drop_shadow_0_0_10_10.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 200, 200,
                                                   context.ToPathList()));
}

TEST(ImageFilterGolden, DropShadow_10_n10_5_5) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200, 200));

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::Color_RED);
  paint.SetImageFilter(skity::ImageFilters::DropShadow(
      10, -10, 5, 5, skity::Color_GREEN, nullptr));

  auto canvas = recorder.GetRecordingCanvas();
  canvas->Save();
  canvas->Translate(50.f, 50.f);
  canvas->DrawRect(skity::Rect::MakeWH(100, 100), paint);
  canvas->Restore();

  PathListContext context("drop_shadow_10_n10_5_5.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 200, 200,
                                                   context.ToPathList()));
}

TEST(ImageFilterGolden, Matrix_translate_50_50) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200, 200));
  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::Color_RED);
  paint.SetImageFilter(
      skity::ImageFilters::MatrixTransform(skity::Matrix::Translate(50, 50)));

  auto canvas = recorder.GetRecordingCanvas();

  canvas->DrawRect(skity::Rect::MakeWH(100, 100), paint);

  {
    skity::Paint bound_paint;
    bound_paint.SetAntiAlias(true);
    bound_paint.SetStyle(skity::Paint::kStroke_Style);
    bound_paint.SetColor(skity::Color_CYAN);
    bound_paint.SetStrokeWidth(1);

    canvas->DrawRect(skity::Rect::MakeWH(100, 100), bound_paint);
  }

  PathListContext context("matrix_translate_50_50.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 200, 200,
                                                   context.ToPathList()));
}

TEST(ImageFilterGolden, RotateMatrix_10_deg_50_50) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200, 200));
  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::Color_RED);
  paint.SetImageFilter(skity::ImageFilters::MatrixTransform(
      skity::Matrix::RotateRad(skity::FloatDegreesToRadians(10), {100, 100})));

  auto canvas = recorder.GetRecordingCanvas();

  canvas->DrawRect(skity::Rect::MakeXYWH(50, 50, 100, 100), paint);

  {
    skity::Paint bound_paint;
    bound_paint.SetAntiAlias(true);
    bound_paint.SetStyle(skity::Paint::kStroke_Style);
    bound_paint.SetColor(skity::Color_CYAN);
    bound_paint.SetStrokeWidth(1);

    canvas->DrawRect(skity::Rect::MakeXYWH(50, 50, 100, 100), bound_paint);
  }

  PathListContext context("rotate_matrix_10_deg_50_50.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 200, 200,
                                                   context.ToPathList()));
}

TEST(ImageFilterGolden, ComposeBlurMatrix) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200, 200));
  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::Color_RED);

  auto blur_filter = skity::ImageFilters::Blur(10, 10);
  auto matrix_filter =
      skity::ImageFilters::MatrixTransform(skity::Matrix::Translate(50, 50));

  auto filter = skity::ImageFilters::Compose(matrix_filter, blur_filter);
  paint.SetImageFilter(filter);

  auto canvas = recorder.GetRecordingCanvas();

  canvas->DrawRect(skity::Rect::MakeWH(100, 100), paint);

  {
    skity::Paint bound_paint;
    bound_paint.SetAntiAlias(true);
    bound_paint.SetStyle(skity::Paint::kStroke_Style);
    bound_paint.SetColor(skity::Color_CYAN);
    bound_paint.SetStrokeWidth(1);

    canvas->DrawRect(skity::Rect::MakeWH(100, 100), bound_paint);
  }

  PathListContext context("compose_blur_matrix.png");

  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 200, 200,
                                                   context.ToPathList()));
}
