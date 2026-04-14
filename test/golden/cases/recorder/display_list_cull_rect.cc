// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <filesystem>
#include <skity/recorder/picture_recorder.hpp>

#include "common/golden_test_check.hpp"

static const char* kGoldenTestImageDir = CASE_DIR;

namespace {

skity::Path MakeClipPath() {
  skity::Path path;
  path.MoveTo(20.f, 10.f);
  path.LineTo(110.f, 25.f);
  path.LineTo(90.f, 95.f);
  path.QuadTo(45.f, 115.f, 10.f, 70.f);
  path.Close();
  return path;
}

std::unique_ptr<skity::DisplayList> BuildClipRestoreDisplayList() {
  skity::PictureRecorder recorder;
  skity::DisplayListBuildOptions options;
  options.build_rtree = true;
  recorder.BeginRecording(skity::Rect::MakeWH(256.f, 256.f), options);
  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint blue;
  blue.SetColor(skity::Color_BLUE);

  const int outer = canvas->Save();
  canvas->Translate(40.f, 30.f);
  canvas->Save();
  canvas->ClipRect(skity::Rect::MakeXYWH(10.f, 10.f, 100.f, 70.f));
  canvas->DrawRect(skity::Rect::MakeXYWH(0.f, 0.f, 140.f, 110.f), blue);
  canvas->RestoreToCount(outer);
  canvas->Restore();

  skity::Paint red;
  red.SetColor(skity::Color_RED);
  canvas->DrawRect(skity::Rect::MakeXYWH(180.f, 30.f, 50.f, 50.f), red);

  skity::Paint green;
  green.SetColor(skity::Color_GREEN);
  canvas->DrawRect(skity::Rect::MakeXYWH(20.f, 180.f, 40.f, 40.f), green);

  return recorder.FinishRecording();
}

std::unique_ptr<skity::DisplayList> BuildSaveLayerDisplayList() {
  skity::PictureRecorder recorder;
  skity::DisplayListBuildOptions options;
  options.build_rtree = true;
  recorder.BeginRecording(skity::Rect::MakeWH(256.f, 256.f), options);
  auto canvas = recorder.GetRecordingCanvas();

  canvas->Save();
  canvas->Translate(20.f, 20.f);

  skity::Paint layer_paint;
  layer_paint.SetAlphaF(0.65f);
  canvas->SaveLayer(skity::Rect::MakeXYWH(0.f, 0.f, 150.f, 120.f), layer_paint);

  skity::Paint red;
  red.SetColor(skity::Color_RED);
  canvas->DrawRect(skity::Rect::MakeXYWH(0.f, 0.f, 100.f, 80.f), red);

  skity::Paint blue;
  blue.SetColor(skity::Color_BLUE);
  blue.SetAlphaF(0.75f);
  canvas->DrawCircle(70.f, 60.f, 45.f, blue);

  canvas->Restore();
  canvas->Restore();

  skity::Paint yellow;
  yellow.SetColor(skity::Color_YELLOW);
  canvas->DrawRect(skity::Rect::MakeXYWH(180.f, 160.f, 40.f, 40.f), yellow);

  return recorder.FinishRecording();
}

std::unique_ptr<skity::DisplayList> BuildClipPathTransformDisplayList() {
  skity::PictureRecorder recorder;
  skity::DisplayListBuildOptions options;
  options.build_rtree = true;
  recorder.BeginRecording(skity::Rect::MakeWH(256.f, 256.f), options);
  auto canvas = recorder.GetRecordingCanvas();

  canvas->Save();
  canvas->Translate(55.f, 45.f);
  canvas->Rotate(18.f);
  canvas->ClipPath(MakeClipPath());

  skity::Paint blue;
  blue.SetColor(skity::Color_BLUE);
  canvas->DrawRect(skity::Rect::MakeXYWH(-10.f, -5.f, 160.f, 120.f), blue);

  skity::Paint red;
  red.SetColor(skity::Color_RED);
  red.SetStyle(skity::Paint::Style::kStroke_Style);
  red.SetStrokeWidth(4.f);
  canvas->DrawPath(MakeClipPath(), red);
  canvas->Restore();

  skity::Paint yellow;
  yellow.SetColor(skity::Color_YELLOW);
  canvas->DrawRect(skity::Rect::MakeXYWH(185.f, 160.f, 30.f, 30.f), yellow);

  return recorder.FinishRecording();
}

std::unique_ptr<skity::DisplayList> BuildSetResetMatrixDisplayList() {
  skity::PictureRecorder recorder;
  skity::DisplayListBuildOptions options;
  options.build_rtree = true;
  recorder.BeginRecording(skity::Rect::MakeWH(256.f, 256.f), options);
  auto canvas = recorder.GetRecordingCanvas();

  auto matrix = skity::Matrix::Translate(35.f, 28.f);
  matrix.PostScale(1.25f, 0.85f);
  canvas->SetMatrix(matrix);

  skity::Paint green;
  green.SetColor(skity::Color_GREEN);
  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeXYWH(10.f, 12.f, 80.f, 60.f),
                               12.f, 12.f),
      green);

  canvas->ResetMatrix();

  skity::Paint red;
  red.SetColor(skity::Color_RED);
  canvas->DrawRect(skity::Rect::MakeXYWH(160.f, 35.f, 45.f, 45.f), red);

  canvas->SetMatrix(skity::Matrix::Translate(0.f, 120.f));
  skity::Paint blue;
  blue.SetColor(skity::Color_BLUE);
  canvas->DrawCircle(40.f, 40.f, 20.f, blue);
  canvas->ResetMatrix();

  return recorder.FinishRecording();
}

std::unique_ptr<skity::DisplayList> BuildClipDrawPaintDisplayList() {
  skity::PictureRecorder recorder;
  skity::DisplayListBuildOptions options;
  options.build_rtree = true;
  recorder.BeginRecording(skity::Rect::MakeWH(256.f, 256.f), options);
  auto canvas = recorder.GetRecordingCanvas();

  canvas->Save();
  canvas->ClipRect(skity::Rect::MakeXYWH(30.f, 35.f, 110.f, 85.f));
  skity::Paint blue;
  blue.SetColor(skity::Color_BLUE);
  canvas->DrawPaint(blue);
  canvas->Restore();

  canvas->Save();
  canvas->ClipRect(skity::Rect::MakeXYWH(95.f, 70.f, 50.f, 30.f),
                   skity::Canvas::ClipOp::kDifference);
  skity::Paint green;
  green.SetColor(skity::Color_GREEN);
  green.SetAlphaF(0.45f);
  canvas->DrawPaint(green);
  canvas->Restore();

  skity::Paint red;
  red.SetColor(skity::Color_RED);
  canvas->DrawRect(skity::Rect::MakeXYWH(185.f, 180.f, 25.f, 25.f), red);

  return recorder.FinishRecording();
}

std::unique_ptr<skity::DisplayList> BuildConcatNestedSaveDisplayList() {
  skity::PictureRecorder recorder;
  skity::DisplayListBuildOptions options;
  options.build_rtree = true;
  recorder.BeginRecording(skity::Rect::MakeWH(256.f, 256.f), options);
  auto canvas = recorder.GetRecordingCanvas();

  canvas->Save();
  auto matrix = skity::Matrix::Translate(35.f, 25.f);
  matrix.PostRotate(12.f, 40.f, 30.f);
  matrix.PostScale(1.1f, 0.9f);
  canvas->Concat(matrix);

  canvas->Save();
  canvas->ClipRect(skity::Rect::MakeXYWH(0.f, 0.f, 110.f, 90.f));
  skity::Paint blue;
  blue.SetColor(skity::Color_BLUE);
  canvas->DrawRect(skity::Rect::MakeXYWH(5.f, 5.f, 100.f, 70.f), blue);

  canvas->Save();
  skity::Paint green;
  green.SetColor(skity::Color_GREEN);
  canvas->DrawCircle(85.f, 55.f, 28.f, green);
  canvas->Restore();

  canvas->Restore();
  canvas->Restore();

  skity::Paint red;
  red.SetColor(skity::Color_RED);
  canvas->DrawRect(skity::Rect::MakeXYWH(185.f, 25.f, 35.f, 35.f), red);

  return recorder.FinishRecording();
}

std::unique_ptr<skity::DisplayList> BuildClipRRectDifferenceDisplayList() {
  skity::PictureRecorder recorder;
  skity::DisplayListBuildOptions options;
  options.build_rtree = true;
  recorder.BeginRecording(skity::Rect::MakeWH(256.f, 256.f), options);
  auto canvas = recorder.GetRecordingCanvas();

  canvas->Save();
  auto clip_rrect = skity::RRect::MakeRectXY(
      skity::Rect::MakeXYWH(35.f, 30.f, 120.f, 100.f), 18.f, 18.f);
  canvas->ClipRRect(clip_rrect, skity::Canvas::ClipOp::kDifference);

  skity::Paint blue;
  blue.SetColor(skity::Color_BLUE);
  canvas->DrawPaint(blue);
  canvas->Restore();

  skity::Paint red;
  red.SetColor(skity::Color_RED);
  red.SetStyle(skity::Paint::Style::kStroke_Style);
  red.SetStrokeWidth(3.f);
  canvas->DrawRRect(clip_rrect, red);

  skity::Paint green;
  green.SetColor(skity::Color_GREEN);
  canvas->DrawRect(skity::Rect::MakeXYWH(185.f, 180.f, 22.f, 22.f), green);

  return recorder.FinishRecording();
}

std::unique_ptr<skity::DisplayList> BuildDisconnectedHitsDisplayList() {
  skity::PictureRecorder recorder;
  skity::DisplayListBuildOptions options;
  options.build_rtree = true;
  recorder.BeginRecording(skity::Rect::MakeWH(256.f, 256.f), options);
  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint red;
  red.SetColor(skity::Color_RED);
  canvas->DrawRect(skity::Rect::MakeXYWH(20.f, 20.f, 40.f, 40.f), red);

  canvas->Save();
  canvas->Translate(80.f, 60.f);
  skity::Paint blue;
  blue.SetColor(skity::Color_BLUE);
  canvas->DrawRect(skity::Rect::MakeXYWH(0.f, 0.f, 50.f, 35.f), blue);
  canvas->Restore();

  canvas->Save();
  canvas->ClipRect(skity::Rect::MakeXYWH(160.f, 150.f, 55.f, 45.f));
  skity::Paint green;
  green.SetColor(skity::Color_GREEN);
  canvas->DrawPaint(green);
  canvas->Restore();

  skity::Paint yellow;
  yellow.SetColor(skity::Color_YELLOW);
  canvas->DrawCircle(210.f, 50.f, 18.f, yellow);

  return recorder.FinishRecording();
}

}  // namespace

TEST(RecorderGolden, DisplayListCullRectClipRestoreToCount) {
  auto dl = BuildClipRestoreDisplayList();
  const auto cull_rect = skity::Rect::MakeXYWH(45.f, 35.f, 110.f, 80.f);

  std::filesystem::path golden_path = kGoldenTestImageDir;
  golden_path.append("display_list_cull_rect_clip_restore_to_count.png");

  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      256, 256, golden_path.c_str(),
      [display_list = dl.get(), cull_rect](skity::Canvas* canvas) {
        display_list->Draw(canvas, cull_rect);
      }));
}

TEST(RecorderGolden, DisplayListCullRectSaveLayerBlend) {
  auto dl = BuildSaveLayerDisplayList();
  const auto cull_rect = skity::Rect::MakeXYWH(15.f, 15.f, 130.f, 110.f);

  std::filesystem::path golden_path = kGoldenTestImageDir;
  golden_path.append("display_list_cull_rect_save_layer_blend.png");

  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      256, 256, golden_path.c_str(),
      [display_list = dl.get(), cull_rect](skity::Canvas* canvas) {
        display_list->Draw(canvas, cull_rect);
      }));
}

TEST(RecorderGolden, DisplayListCullRectClipPathTransform) {
  auto dl = BuildClipPathTransformDisplayList();
  const auto cull_rect = skity::Rect::MakeXYWH(50.f, 40.f, 120.f, 110.f);

  std::filesystem::path golden_path = kGoldenTestImageDir;
  golden_path.append("display_list_cull_rect_clip_path_transform.png");

  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      256, 256, golden_path.c_str(),
      [display_list = dl.get(), cull_rect](skity::Canvas* canvas) {
        display_list->Draw(canvas, cull_rect);
      }));
}

TEST(RecorderGolden, DisplayListCullRectSetResetMatrix) {
  auto dl = BuildSetResetMatrixDisplayList();
  const auto cull_rect = skity::Rect::MakeXYWH(40.f, 30.f, 110.f, 70.f);

  std::filesystem::path golden_path = kGoldenTestImageDir;
  golden_path.append("display_list_cull_rect_set_reset_matrix.png");

  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      256, 256, golden_path.c_str(),
      [display_list = dl.get(), cull_rect](skity::Canvas* canvas) {
        display_list->Draw(canvas, cull_rect);
      }));
}

TEST(RecorderGolden, DisplayListCullRectClipDrawPaint) {
  auto dl = BuildClipDrawPaintDisplayList();
  const auto cull_rect = skity::Rect::MakeXYWH(25.f, 30.f, 135.f, 95.f);

  std::filesystem::path golden_path = kGoldenTestImageDir;
  golden_path.append("display_list_cull_rect_clip_draw_paint.png");

  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      256, 256, golden_path.c_str(),
      [display_list = dl.get(), cull_rect](skity::Canvas* canvas) {
        display_list->Draw(canvas, cull_rect);
      }));
}

TEST(RecorderGolden, DisplayListCullRectConcatNestedSave) {
  auto dl = BuildConcatNestedSaveDisplayList();
  const auto cull_rect = skity::Rect::MakeXYWH(30.f, 20.f, 130.f, 110.f);

  std::filesystem::path golden_path = kGoldenTestImageDir;
  golden_path.append("display_list_cull_rect_concat_nested_save.png");

  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      256, 256, golden_path.c_str(),
      [display_list = dl.get(), cull_rect](skity::Canvas* canvas) {
        display_list->Draw(canvas, cull_rect);
      }));
}

TEST(RecorderGolden, DisplayListCullRectClipRRectDifference) {
  auto dl = BuildClipRRectDifferenceDisplayList();
  const auto cull_rect = skity::Rect::MakeXYWH(25.f, 20.f, 145.f, 120.f);

  std::filesystem::path golden_path = kGoldenTestImageDir;
  golden_path.append("display_list_cull_rect_clip_rrect_difference.png");

  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      256, 256, golden_path.c_str(),
      [display_list = dl.get(), cull_rect](skity::Canvas* canvas) {
        display_list->Draw(canvas, cull_rect);
      }));
}

TEST(RecorderGolden, DisplayListCullRectDisconnectedHits) {
  auto dl = BuildDisconnectedHitsDisplayList();
  const auto cull_rect = skity::Rect::MakeXYWH(10.f, 10.f, 210.f, 190.f);

  std::filesystem::path golden_path = kGoldenTestImageDir;
  golden_path.append("display_list_cull_rect_disconnected_hits.png");

  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      256, 256, golden_path.c_str(),
      [display_list = dl.get(), cull_rect](skity::Canvas* canvas) {
        display_list->Draw(canvas, cull_rect);
      }));
}
