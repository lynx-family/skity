// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <filesystem>
#include <skity/effect/image_filter.hpp>
#include <skity/recorder/picture_recorder.hpp>

#include "common/golden_test_check.hpp"

constexpr const char* kGoldenTestDir = CASE_DIR;

TEST(ImageFilterGolden, BlurFilter_10_5) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200, 200));

  skity::Paint paint;
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::Color_RED);
  paint.SetImageFilter(skity::ImageFilters::Blur(10, 5));

  auto canvas = recorder.GetRecordingCanvas();

  canvas->Save();
  canvas->Translate(50.f, 50.f);
  canvas->DrawRect(skity::Rect::MakeWH(100, 100), paint);
  canvas->Restore();

  std::filesystem::path expected_image_path(kGoldenTestDir);
  expected_image_path.append("blur_filter_10_5.png");

  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      recorder.FinishRecording(), 200, 200, expected_image_path.c_str()));
}

TEST(ImageFilterGolden, BlurFilter_10_10) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200, 200));

  skity::Paint paint;
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::Color_RED);
  paint.SetImageFilter(skity::ImageFilters::Blur(10, 10));

  auto canvas = recorder.GetRecordingCanvas();

  canvas->Save();
  canvas->Translate(50.f, 50.f);
  canvas->Concat(skity::Matrix::RotateDeg(45.f, {50.f, 50.f}));

  canvas->DrawRect(skity::Rect::MakeWH(100, 100), paint);
  canvas->Restore();

  std::filesystem::path expected_image_path(kGoldenTestDir);
  expected_image_path.append("blur_filter_10_10.png");

  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      recorder.FinishRecording(), 200, 200, expected_image_path.c_str()));
}

TEST(ImageFilterGolden, BlurFilter_10_0) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200, 200));

  skity::Paint paint;
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::Color_RED);
  paint.SetImageFilter(skity::ImageFilters::Blur(10, 0));

  auto canvas = recorder.GetRecordingCanvas();
  canvas->Save();
  canvas->Translate(50.f, 50.f);
  canvas->DrawRect(skity::Rect::MakeWH(100, 100), paint);
  canvas->Restore();

  std::filesystem::path expected_image_path(kGoldenTestDir);
  expected_image_path.append("blur_filter_10_0.png");

  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      recorder.FinishRecording(), 200, 200, expected_image_path.c_str()));
}

TEST(ImageFilterGolden, DropShadow_0_0_10_10) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200, 200));
  skity::Paint paint;
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::Color_RED);
  paint.SetImageFilter(skity::ImageFilters::DropShadow(
      0, 0, 10, 10, skity::Color_GREEN, nullptr));

  auto canvas = recorder.GetRecordingCanvas();
  canvas->Save();
  canvas->Translate(50.f, 50.f);
  canvas->DrawRect(skity::Rect::MakeWH(100, 100), paint);
  canvas->Restore();
  std::filesystem::path expected_image_path(kGoldenTestDir);
  expected_image_path.append("drop_shadow_0_0_10_10.png");

  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      recorder.FinishRecording(), 200, 200, expected_image_path.c_str()));
}

TEST(ImageFilterGolden, DropShadow_10_n10_5_5) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200, 200));

  skity::Paint paint;
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::Color_RED);
  paint.SetImageFilter(skity::ImageFilters::DropShadow(
      10, -10, 5, 5, skity::Color_GREEN, nullptr));

  auto canvas = recorder.GetRecordingCanvas();
  canvas->Save();
  canvas->Translate(50.f, 50.f);
  canvas->DrawRect(skity::Rect::MakeWH(100, 100), paint);
  canvas->Restore();

  std::filesystem::path expected_image_path(kGoldenTestDir);
  expected_image_path.append("drop_shadow_10_n10_5_5.png");

  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      recorder.FinishRecording(), 200, 200, expected_image_path.c_str()));
}

TEST(ImageFilterGolden, Matrix_translate_50_50) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200, 200));
  skity::Paint paint;
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::Color_RED);
  paint.SetImageFilter(
      skity::ImageFilters::MatrixTransform(skity::Matrix::Translate(50, 50)));

  auto canvas = recorder.GetRecordingCanvas();

  canvas->DrawRect(skity::Rect::MakeWH(100, 100), paint);

  {
    skity::Paint bound_paint;
    bound_paint.SetStyle(skity::Paint::kStroke_Style);
    bound_paint.SetColor(skity::Color_CYAN);
    bound_paint.SetStrokeWidth(1);

    canvas->DrawRect(skity::Rect::MakeWH(100, 100), bound_paint);
  }

  std::filesystem::path expected_image_path(kGoldenTestDir);
  expected_image_path.append("matrix_translate_50_50.png");
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      recorder.FinishRecording(), 200, 200, expected_image_path.c_str()));
}

TEST(ImageFilterGolden, ComposeBlurMatrix) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200, 200));
  skity::Paint paint;
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
    bound_paint.SetStyle(skity::Paint::kStroke_Style);
    bound_paint.SetColor(skity::Color_CYAN);
    bound_paint.SetStrokeWidth(1);

    canvas->DrawRect(skity::Rect::MakeWH(100, 100), bound_paint);
  }

  std::filesystem::path expected_image_path(kGoldenTestDir);
  expected_image_path.append("compose_blur_matrix.png");

  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      recorder.FinishRecording(), 200, 200, expected_image_path.c_str()));
}
