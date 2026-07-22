// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <filesystem>
#include <skity/graphic/bitmap.hpp>
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

struct PathListContext {
  explicit PathListContext(const char* name)
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

bool CompareGradientGolden(skity::PictureRecorder& recorder, uint32_t width,
                           uint32_t height, const char* name) {
  PathListContext context(name);
  auto display_list = recorder.FinishRecording();
  return skity::testing::CompareGoldenTexture(display_list.get(), width, height,
                                              context.ToPathList());
}

skity::Path MakeTestTriangle() {
  skity::Path path;
  path.MoveTo(0.f, 96.f);
  path.LineTo(48.f, 0.f);
  path.LineTo(96.f, 96.f);
  path.Close();
  return path;
}

std::shared_ptr<skity::Shader> MakeTestLinearShader() {
  skity::Vec4 colors[] = {skity::Color4fFromColor(skity::Color_RED),
                          skity::Color4fFromColor(skity::Color_BLUE)};
  float positions[] = {0.f, 1.f};
  skity::Point pts[] = {{0.f, 0.f, 0.f, 1.f}, {96.f, 0.f, 0.f, 1.f}};
  return skity::Shader::MakeLinear(pts, colors, positions, 2);
}

std::shared_ptr<skity::Shader> MakeTestRadialShader() {
  skity::Vec4 colors[] = {skity::Color4fFromColor(skity::Color_YELLOW),
                          skity::Color4fFromColor(skity::Color_GREEN),
                          skity::Color4fFromColor(skity::Color_BLUE)};
  float positions[] = {0.f, 0.55f, 1.f};
  return skity::Shader::MakeRadial({48.f, 48.f, 0.f, 1.f}, 58.f, colors,
                                   positions, 3);
}

std::shared_ptr<skity::Shader> MakeTestSweepShader() {
  skity::Vec4 colors[] = {skity::Color4fFromColor(skity::Color_RED),
                          skity::Color4fFromColor(skity::Color_GREEN),
                          skity::Color4fFromColor(skity::Color_BLUE),
                          skity::Color4fFromColor(skity::Color_RED)};
  float positions[] = {0.f, 0.33f, 0.66f, 1.f};
  return skity::Shader::MakeSweep(48.f, 48.f, 0.f, 360.f, colors, positions, 4);
}

std::shared_ptr<skity::Image> MakeTestImage() {
  skity::Bitmap bitmap(4, 4, skity::AlphaType::kUnpremul_AlphaType,
                       skity::ColorType::kRGBA);
  for (uint32_t y = 0; y < bitmap.Height(); y++) {
    for (uint32_t x = 0; x < bitmap.Width(); x++) {
      skity::Color color = ((x + y) % 2 == 0)
                               ? skity::ColorSetARGB(255, 244, 67, 54)
                               : skity::ColorSetARGB(255, 33, 150, 243);
      if (x == y) {
        color = skity::ColorSetARGB(255, 255, 235, 59);
      }
      bitmap.SetPixel(x, y, color);
    }
  }
  return skity::Image::MakeImage(bitmap.GetPixmap());
}

void DrawTransformedTestPath(skity::Canvas* canvas,
                             std::shared_ptr<skity::Shader> shader, float tx,
                             float ty, float scale, float rotation) {
  skity::Paint paint;
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetAntiAlias(true);
  paint.SetShader(std::move(shader));

  canvas->Save();
  canvas->Translate(tx, ty);
  canvas->Scale(scale, scale);
  canvas->Rotate(rotation, 48.f, 48.f);
  canvas->DrawPath(MakeTestTriangle(), paint);
  canvas->Restore();
}

}  // namespace

TEST(GradientGolden, GradientLocalCoordinates) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(360.f, 150.f));
  auto canvas = recorder.GetRecordingCanvas();
  canvas->Clear(skity::Color_WHITE);

  DrawTransformedTestPath(canvas, MakeTestLinearShader(), 28.f, 28.f, 1.f, 0.f);
  DrawTransformedTestPath(canvas, MakeTestRadialShader(), 138.f, 20.f, 1.15f,
                          0.f);
  DrawTransformedTestPath(canvas, MakeTestSweepShader(), 270.f, 24.f, 1.f,
                          28.f);

  EXPECT_TRUE(CompareGradientGolden(recorder, 360, 150,
                                    "gradient_local_coordinates.png"));
}

TEST(GradientGolden, ImageShaderLocalCoordinates) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(180.f, 160.f));
  auto canvas = recorder.GetRecordingCanvas();
  canvas->Clear(skity::Color_WHITE);

  auto image = MakeTestImage();
  auto shader = skity::Shader::MakeShader(
      image,
      skity::SamplingOptions{skity::FilterMode::kNearest,
                             skity::MipmapMode::kNone},
      skity::TileMode::kDecal, skity::TileMode::kDecal,
      skity::Matrix::Scale(24.f, 24.f));

  DrawTransformedTestPath(canvas, std::move(shader), 42.f, 28.f, 1.1f, -18.f);

  EXPECT_TRUE(CompareGradientGolden(recorder, 180, 160,
                                    "image_shader_local_coordinates.png"));
}

TEST(GradientGolden, LinearGradientTileMode) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(300.f, 300.f));

  auto canvas = recorder.GetRecordingCanvas();

  canvas->Save();
  canvas->Translate(50, 50);

  skity::Paint paint;
  paint.SetAntiAlias(true);
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

  EXPECT_TRUE(CompareGradientGolden(recorder, 500, 500,
                                    "linear_gradient_tile_mode.png"));
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
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetShader(gs);

  auto r = skity::Rect::MakeLTRB(0, 0, kCaseSize, kCaseSize);

  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(kCaseSize, kCaseSize));
  auto canvas = recorder.GetRecordingCanvas();

  canvas->DrawRect(r, paint);

  EXPECT_TRUE(CompareGradientGolden(recorder, 300, 300, "radial_gradient.png"));
}

TEST(GradientGolden, RadialGradient2) {
  static constexpr float kCaseSize = 300;

  skity::Vec4 colors[] = {skity::Color4fFromColor(skity::Color_GREEN),
                          skity::Color4fFromColor(skity::Color_TRANSPARENT)};
  float positions[] = {0.75f, 1.f};

  auto gs = skity::Shader::MakeRadial({kCaseSize / 2.f, kCaseSize / 2.f, 0, 1},
                                      kCaseSize / 2.f, colors, positions, 2,
                                      skity::TileMode::kClamp);

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetShader(gs);

  auto r = skity::Rect::MakeLTRB(0, 0, kCaseSize, kCaseSize);

  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(kCaseSize, kCaseSize));
  auto canvas = recorder.GetRecordingCanvas();

  canvas->DrawRect(r, paint);

  EXPECT_TRUE(
      CompareGradientGolden(recorder, 300, 300, "radial_gradient2.png"));
}

TEST(GradientGolden, RadialGradientFlags) {
  static constexpr float kCaseSize = 300;

  skity::Vec4 colors[] = {skity::Color4fFromColor(skity::Color_GREEN),
                          skity::Color4fFromColor(skity::Color_TRANSPARENT)};
  float positions[] = {0.75f, 1.f};

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);

  auto r = skity::Rect::MakeLTRB(0, 0, kCaseSize, kCaseSize);

  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(kCaseSize, kCaseSize));
  auto canvas = recorder.GetRecordingCanvas();

  canvas->Clear(skity::Color_WHITE);

  canvas->Save();

  for (uint32_t i = 0; i < 2; i++) {
    canvas->Translate(kCaseSize * i, 0);
    uint32_t flags = i;
    auto gs = skity::Shader::MakeRadial(
        {kCaseSize / 2.f, kCaseSize / 2.f, 0, 1}, kCaseSize / 2.f, colors,
        positions, 2, skity::TileMode::kClamp, flags);
    paint.SetShader(gs);
    canvas->DrawRect(r, paint);
  }
  canvas->Restore();

  EXPECT_TRUE(
      CompareGradientGolden(recorder, 600, 300, "radial_gradient_flags.png"));
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
  paint.SetAntiAlias(true);
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

  EXPECT_TRUE(CompareGradientGolden(recorder, 150, 150,
                                    "two_point_conical_gradient_0_64.png"));
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
  EXPECT_TRUE(CompareGradientGolden(recorder, 150, 150,
                                    "two_point_conical_gradient_32_64.png"));
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

  EXPECT_TRUE(CompareGradientGolden(
      recorder, 150, 150, "two_point_conical_gradient_no_center_0_64.png"));
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

  EXPECT_TRUE(CompareGradientGolden(
      recorder, 150, 150, "two_point_conical_gradient_no_center_64_0.png"));
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

  EXPECT_TRUE(CompareGradientGolden(
      recorder, 150, 150, "two_point_conical_gradient_no_center_32_64.png"));
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

  EXPECT_TRUE(CompareGradientGolden(
      recorder, 150, 150, "two_point_conical_gradient_no_center_8_16.png"));
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
  EXPECT_TRUE(CompareGradientGolden(
      recorder, 150, 150, "two_point_conical_gradient_no_center_16_8.png"));
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
  EXPECT_TRUE(CompareGradientGolden(
      recorder, 150, 150, "two_point_conical_gradient_no_center_16_16.png"));
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
  paint.SetAntiAlias(true);
  paint.SetShader(lgs);

  canvas->DrawRect(skity::Rect::MakeXYWH(0.f, 0.f, 170.f, 170.f), paint);

  EXPECT_TRUE(CompareGradientGolden(recorder, 170, 170,
                                    "linear_gradient_with_color_stops.png"));
}

TEST(GradientGolden, LinearGradientFallbackTileMode) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(300.f, 300.f));

  auto canvas = recorder.GetRecordingCanvas();

  canvas->Save();
  canvas->Translate(50, 50);

  skity::Paint paint;
  paint.SetAntiAlias(true);
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

  EXPECT_TRUE(CompareGradientGolden(recorder, 500, 500,
                                    "gradient_fallback_tile_mode.png"));
}

TEST(GradientGolden, RadialGradientFallbackTileMode) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(300.f, 300.f));

  auto canvas = recorder.GetRecordingCanvas();

  canvas->Save();
  canvas->Translate(50, 50);

  skity::Paint paint;
  paint.SetAntiAlias(true);
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

  EXPECT_TRUE(CompareGradientGolden(recorder, 500, 500,
                                    "gradient_fallback_tile_mode.png"));
}
