// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <filesystem>
#include <skity/recorder/picture_recorder.hpp>
#include <skity/skity.hpp>

#include "common/golden_test_check.hpp"

static const char* kGoldenTestImageDir = CASE_DIR;

TEST(GradientGolden, LinearGradientTileMode) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(300.f, 300.f));

  auto canvas = recorder.GetRecordingCanvas();

  canvas->Save();
  canvas->Translate(50, 50);

  skity::Paint paint;
  std::vector<skity::Vec3> offsets = {
      {0, 0, 0}, {0, 150, 0}, {150, 0, 0}, {150, 150, 0}};
  std::vector<skity::TileMode> tile_modes = {
      skity::TileMode::kClamp, skity::TileMode::kRepeat,
      skity::TileMode::kMirror, skity::TileMode::kDecal};
  for (int i = 0; i < 4; i++) {
    canvas->Save();
    canvas->Translate(offsets[i].x, offsets[i].y);
    skity::Vec4 gradient_colors[] = {
        skity::Vec4{0.9019f, 0.3921f, 0.3960f, 1.0f},
        skity::Vec4{0.5686f, 0.5960f, 0.8980f, 1.0f}};
    float gradient_positions[] = {0.f, 1.f};
    std::vector<skity::Point> gradient_points = {
        skity::Point{0.f, 0.f, 0.f, 1.f},
        skity::Point{50.f, 50.f, 0.f, 1.f},
    };
    auto lgs =
        skity::Shader::MakeLinear(gradient_points.data(), gradient_colors,
                                  gradient_positions, 2, tile_modes[i]);
    paint.SetShader(lgs);
    canvas->DrawRect({0, 0, 100, 100}, paint);
    canvas->Restore();
  }

  canvas->Restore();

  std::filesystem::path expected_image_path(kGoldenTestImageDir);
  expected_image_path.append("linear_gradient_tile_mode.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 500, 500,
      skity::testing::PathList{.cpu_tess_path = expected_image_path.c_str(),
                               .gpu_tess_path = expected_image_path.c_str()}));
}

TEST(GradientGolden, RadialGradient) {
  static constexpr float kCaseSize = 300;

  skity::Vec4 colors[] = {skity::Color4fFromColor(skity::Color_RED),
                          skity::Color4fFromColor(skity::Color_GREEN),
                          skity::Color4fFromColor(skity::Color_BLUE)};
  float positions[] = {0.f, 0.4, 1.f};

  auto gs =
      skity::Shader::MakeRadial({kCaseSize / 2.f, kCaseSize / 2.f, 0, 1}, 100.f,
                                colors, positions, 3, skity::TileMode::kClamp);

  skity::Paint paint;
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetShader(gs);

  auto r = skity::Rect::MakeLTRB(0, 0, kCaseSize, kCaseSize);

  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(kCaseSize, kCaseSize));
  auto canvas = recorder.GetRecordingCanvas();

  canvas->DrawRect(r, paint);

  std::filesystem::path expected_image_path(kGoldenTestImageDir);

  expected_image_path.append("radial_gradient.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 300, 300,
      skity::testing::PathList{.cpu_tess_path = expected_image_path.c_str(),
                               .gpu_tess_path = expected_image_path.c_str()}));
}

static void draw_radial_gradient(skity::Canvas* canvas, float x0, float y0,
                                 float r0, float x1, float y1, float r1,
                                 float sz) {
  skity::Vec4 colors[] = {skity::Color4fFromColor(skity::Color_RED),
                          skity::Color4fFromColor(skity::Color_YELLOW),
                          skity::Color4fFromColor(skity::Color_GREEN),
                          skity::Color4fFromColor(skity::Color_BLUE)};
  float positions[] = {0.f, 0.33, 0.66, 1.f};

  auto gs = skity::Shader::MakeTwoPointConical(
      {x0, y0, 0, 1}, r0, {x1, y1, 0, 1}, r1, colors, positions,
      sizeof(colors) / sizeof(colors[0]), skity::TileMode::kClamp);

  skity::Paint paint;
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetShader(gs);

  auto r = skity::Rect::MakeLTRB(0, 0, sz, sz);
  canvas->DrawRect(r, paint);
}

static constexpr float kCaseSize = 128;

TEST(GradientGolden, TwoPointConicalGradient_0_64) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(150.f, 150.f));

  auto canvas = recorder.GetRecordingCanvas();

  auto align = (150.f - kCaseSize) / 2.f;

  canvas->Save();
  canvas->Translate(align, align);

  draw_radial_gradient(canvas, kCaseSize / 2.f, kCaseSize / 2.f, 0.f,
                       kCaseSize / 2.f, kCaseSize / 2.f, kCaseSize / 2.f,
                       kCaseSize);

  canvas->Restore();

  std::filesystem::path expected_image_path(kGoldenTestImageDir);
  expected_image_path.append("two_point_conical_gradient_0_64.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 150, 150,
      skity::testing::PathList{.cpu_tess_path = expected_image_path.c_str(),
                               .gpu_tess_path = expected_image_path.c_str()}));
}

TEST(GradientGolden, TwoPointConicalGradient_32_64) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(150.f, 150.f));
  auto canvas = recorder.GetRecordingCanvas();

  auto align = (150.f - kCaseSize) / 2.f;
  canvas->Save();

  canvas->Translate(align, align);

  draw_radial_gradient(canvas, kCaseSize / 2.f, kCaseSize / 2.f,
                       kCaseSize / 4.f, kCaseSize / 2.f, kCaseSize / 2.f,
                       kCaseSize / 2.f, kCaseSize);
  canvas->Restore();
  std::filesystem::path expected_image_path(kGoldenTestImageDir);
  expected_image_path.append("two_point_conical_gradient_32_64.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 150, 150,
      skity::testing::PathList{.cpu_tess_path = expected_image_path.c_str(),
                               .gpu_tess_path = expected_image_path.c_str()}));
}

TEST(GradientGolden, TwoPointConicalGradient_no_center_0_64) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(150.f, 150.f));

  auto canvas = recorder.GetRecordingCanvas();
  auto align = (150.f - kCaseSize) / 2.f;

  canvas->Save();
  canvas->Translate(align, align);

  draw_radial_gradient(canvas, kCaseSize / 4.f, kCaseSize / 4.f, 0.f,
                       kCaseSize / 2.f, kCaseSize / 2.f, kCaseSize / 2.f,
                       kCaseSize);

  canvas->Restore();

  std::filesystem::path expected_image_path(kGoldenTestImageDir);
  expected_image_path.append("two_point_conical_gradient_no_center_0_64.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 150, 150,
      skity::testing::PathList{.cpu_tess_path = expected_image_path.c_str(),
                               .gpu_tess_path = expected_image_path.c_str()}));
}

TEST(GradientGolden, TwoPointConicalGradient_no_center_64_0) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(150.f, 150.f));
  auto canvas = recorder.GetRecordingCanvas();

  auto align = (150.f - kCaseSize) / 2.f;

  canvas->Save();
  canvas->Translate(align, align);

  draw_radial_gradient(canvas, kCaseSize / 4.f, kCaseSize / 4.f,
                       kCaseSize / 2.f, kCaseSize / 2.f, kCaseSize / 2.f, 0.f,
                       kCaseSize);
  canvas->Restore();

  std::filesystem::path expected_image_path(kGoldenTestImageDir);
  expected_image_path.append("two_point_conical_gradient_no_center_64_0.png");

  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 150, 150,
      skity::testing::PathList{.cpu_tess_path = expected_image_path.c_str(),
                               .gpu_tess_path = expected_image_path.c_str()}));
}

TEST(GradientGolden, TwoPointConicalGradient_no_center_32_64) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(150.f, 150.f));
  auto canvas = recorder.GetRecordingCanvas();

  auto align = (150.f - kCaseSize) / 2.f;
  canvas->Save();
  canvas->Translate(align, align);

  draw_radial_gradient(canvas, kCaseSize / 4.f, kCaseSize / 4.f,
                       kCaseSize / 4.f, kCaseSize / 2.f, kCaseSize / 2.f,
                       kCaseSize / 2.f, kCaseSize);
  canvas->Restore();

  std::filesystem::path expected_image_path(kGoldenTestImageDir);
  expected_image_path.append("two_point_conical_gradient_no_center_32_64.png");

  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 150, 150,
      skity::testing::PathList{.cpu_tess_path = expected_image_path.c_str(),
                               .gpu_tess_path = expected_image_path.c_str()}));
}

TEST(GradientGolden, TwoPointConicalGradient_no_center_8_16) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(150.f, 150.f));

  auto canvas = recorder.GetRecordingCanvas();

  auto align = (150.f - kCaseSize) / 2.f;

  canvas->Save();
  canvas->Translate(align, align);
  draw_radial_gradient(canvas, kCaseSize / 4.f, kCaseSize / 4.f,
                       kCaseSize / 16.f, kCaseSize / 2.f, kCaseSize / 2.f,
                       kCaseSize / 8.f, kCaseSize);
  canvas->Restore();

  std::filesystem::path expected_image_path(kGoldenTestImageDir);
  expected_image_path.append("two_point_conical_gradient_no_center_8_16.png");

  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 150, 150,
      skity::testing::PathList{.cpu_tess_path = expected_image_path.c_str(),
                               .gpu_tess_path = expected_image_path.c_str()}));
}

TEST(GradientGolden, TwoPointConicalGradient_no_center_16_8) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(150.f, 150.f));
  auto canvas = recorder.GetRecordingCanvas();

  auto align = (150.f - kCaseSize) / 2.f;
  canvas->Save();
  canvas->Translate(align, align);

  draw_radial_gradient(canvas, kCaseSize / 4.f, kCaseSize / 4.f,
                       kCaseSize / 8.f, kCaseSize / 2.f, kCaseSize / 2.f,
                       kCaseSize / 16.f, kCaseSize);

  canvas->Restore();
  std::filesystem::path expected_image_path(kGoldenTestImageDir);
  expected_image_path.append("two_point_conical_gradient_no_center_16_8.png");

  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 150, 150,
      skity::testing::PathList{.cpu_tess_path = expected_image_path.c_str(),
                               .gpu_tess_path = expected_image_path.c_str()}));
}

TEST(GradientGolden, TwoPointConicalGradient_no_center_16_16) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(150.f, 150.f));
  auto canvas = recorder.GetRecordingCanvas();
  auto align = (150.f - kCaseSize) / 2.f;

  canvas->Save();
  canvas->Translate(align, align);

  draw_radial_gradient(canvas, kCaseSize / 8.f, kCaseSize / 8.f,
                       kCaseSize / 8.f, kCaseSize / 2.f, kCaseSize / 2.f,
                       kCaseSize / 8.f, kCaseSize);

  canvas->Restore();
  std::filesystem::path expected_image_path(kGoldenTestImageDir);
  expected_image_path.append("two_point_conical_gradient_no_center_16_16.png");

  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 150, 150,
      skity::testing::PathList{.cpu_tess_path = expected_image_path.c_str(),
                               .gpu_tess_path = expected_image_path.c_str()}));
}

TEST(GradientGolden, LinearGradientWithColorStops) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(170.f, 170.f));
  auto canvas = recorder.GetRecordingCanvas();

  skity::Vec4 gradient_colors[] = {
      skity::Colors::kRed,  skity::Colors::kWhite, skity::Colors::kBlack,
      skity::Colors::kRed,  skity::Colors::kGreen, skity::Colors::kWhite,
      skity::Colors::kBlue, skity::Colors::kRed,
  };

  float gradient_positions[] = {0.f, 0.f, 0.2f, 0.2f, 0.5f, 0.7f, 1.f, 1.f};

  std::vector<skity::Point> gradient_points = {
      skity::Point{40.f, 40.f, 0.f, 1.f},
      skity::Point{80.f, 80.f, 0.f, 1.f},
  };
  auto lgs =
      skity::Shader::MakeLinear(gradient_points.data(), gradient_colors,
                                gradient_positions, 8, skity::TileMode::kClamp);
  skity::Paint paint;
  paint.SetShader(lgs);

  canvas->DrawRect(skity::Rect::MakeXYWH(0.f, 0.f, 170.f, 170.f), paint);

  std::filesystem::path expected_image_path(kGoldenTestImageDir);

  expected_image_path.append("linear_gradient_with_color_stops.png");

  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 170, 170,
      skity::testing::PathList{.cpu_tess_path = expected_image_path.c_str(),
                               .gpu_tess_path = expected_image_path.c_str()}));
}

TEST(GradientGolden, LinearGradientFallbackTileMode) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(300.f, 300.f));

  auto canvas = recorder.GetRecordingCanvas();

  canvas->Save();
  canvas->Translate(50, 50);

  skity::Paint paint;
  std::vector<skity::Vec3> offsets = {
      {0, 0, 0}, {0, 150, 0}, {150, 0, 0}, {150, 150, 0}};
  std::vector<skity::TileMode> tile_modes = {
      skity::TileMode::kClamp, skity::TileMode::kRepeat,
      skity::TileMode::kMirror, skity::TileMode::kDecal};
  for (int i = 0; i < 4; i++) {
    canvas->Save();
    canvas->Translate(offsets[i].x, offsets[i].y);
    skity::Vec4 gradient_colors[] = {skity::Colors::kRed, skity::Colors::kBlue};
    float gradient_positions[] = {0.f, 1.f};
    std::vector<skity::Point> gradient_points = {
        skity::Point{50.f, 50.f, 0.f, 1.f},
        skity::Point{50.f, 50.f, 0.f, 1.f},
    };
    auto lgs =
        skity::Shader::MakeLinear(gradient_points.data(), gradient_colors,
                                  gradient_positions, 2, tile_modes[i]);
    paint.SetShader(lgs);
    canvas->DrawRect({0, 0, 100, 100}, paint);
    canvas->Restore();
  }

  canvas->Restore();

  std::filesystem::path expected_image_path(kGoldenTestImageDir);
  expected_image_path.append("gradient_fallback_tile_mode.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 500, 500,
      skity::testing::PathList{.cpu_tess_path = expected_image_path.c_str(),
                               .gpu_tess_path = expected_image_path.c_str()}));
}

TEST(GradientGolden, RadialGradientFallbackTileMode) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(300.f, 300.f));

  auto canvas = recorder.GetRecordingCanvas();

  canvas->Save();
  canvas->Translate(50, 50);

  skity::Paint paint;
  std::vector<skity::Vec3> offsets = {
      {0, 0, 0}, {0, 150, 0}, {150, 0, 0}, {150, 150, 0}};
  std::vector<skity::TileMode> tile_modes = {
      skity::TileMode::kClamp, skity::TileMode::kRepeat,
      skity::TileMode::kMirror, skity::TileMode::kDecal};
  for (int i = 0; i < 4; i++) {
    canvas->Save();
    canvas->Translate(offsets[i].x, offsets[i].y);
    skity::Vec4 gradient_colors[] = {skity::Colors::kRed, skity::Colors::kBlue};
    float gradient_positions[] = {0.f, 1.f};
    auto lgs =
        skity::Shader::MakeRadial({50.f, 50.f, 0.f, 1.f}, 0.f, gradient_colors,
                                  gradient_positions, 2, tile_modes[i]);
    paint.SetShader(lgs);
    canvas->DrawRect({0, 0, 100, 100}, paint);
    canvas->Restore();
  }

  canvas->Restore();

  std::filesystem::path expected_image_path(kGoldenTestImageDir);
  expected_image_path.append("gradient_fallback_tile_mode.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 500, 500,
      skity::testing::PathList{.cpu_tess_path = expected_image_path.c_str(),
                               .gpu_tess_path = expected_image_path.c_str()}));
}
