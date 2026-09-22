// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <filesystem>
#include <skity/recorder/picture_recorder.hpp>

#include "common/golden_test_check.hpp"
#include "common/golden_test_env.hpp"
#include "skity/effect/image_filter.hpp"
#include "skity/effect/shader.hpp"
#include "skity/geometry/camera.hpp"
#include "skity/geometry/matrix.hpp"
#include "skity/graphic/bitmap.hpp"
#include "skity/graphic/color.hpp"
#include "skity/graphic/image.hpp"

static const char* kGoldenTestImageDir = CASE_DIR;
static const char* kGoldenTestCoverageAAImageDir =
    CASE_DIR "coverage_aa_images/";

namespace {

std::filesystem::path CoverageAAGoldenPath(const char* name) {
  std::filesystem::path path(kGoldenTestCoverageAAImageDir);
  path.append(name);
  return path;
}

void CheckFractionalLayerBounds(bool draw_color, bool clipped) {
  SCOPED_TRACE(::testing::Message() << "parent_clip=" << clipped);
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  ASSERT_NE(env, nullptr);
  for (uint32_t samples : {1u, 4u}) {
    for (auto mode : {skity::BlendMode::kSrcOver, skity::BlendMode::kSrc,
                      skity::BlendMode::kClear}) {
      for (float scale : {1.f, 1.25f}) {
        for (auto reflection :
             {skity::Vec2{1.f, 1.f}, skity::Vec2{-1.f, 1.f},
              skity::Vec2{1.f, -1.f}, skity::Vec2{-1.f, -1.f}}) {
          for (bool nested : {false, true}) {
            SCOPED_TRACE(::testing::Message()
                         << "samples=" << samples << " mode=" << int(mode)
                         << " scale=" << scale << " reflection=" << reflection.x
                         << "," << reflection.y << " nested=" << nested);
            auto render = [&](bool use_layer) {
              auto previous_samples = env->GetSampleCount();
              env->SetSampleCount(samples);
              auto texture =
                  env->RenderToTexture(96, 96, [&](skity::Canvas* canvas) {
                    canvas->Clear(skity::Color_GREEN);
                    canvas->Translate(reflection.x < 0.f ? 96.f : 0.f,
                                      reflection.y < 0.f ? 96.f : 0.f);
                    canvas->Scale(reflection.x * scale, reflection.y * scale);
                    if (clipped) {
                      canvas->ClipRect(
                          skity::Rect::MakeLTRB(25.25f, 0.f, 73.75f, 60.f));
                    }
                    if (nested) {
                      canvas->SaveLayer(
                          skity::Rect::MakeLTRB(10.3f, 10.3f, 70.3f, 70.3f),
                          skity::Paint{});
                      canvas->DrawColor(skity::Color_YELLOW);
                    }
                    auto bounds =
                        skity::Rect::MakeLTRB(20.75f, 20.75f, 40.25f, 40.25f);
                    skity::Paint paint;
                    paint.SetBlendMode(mode);
                    if (use_layer) {
                      canvas->SaveLayer(bounds, paint);
                      if (draw_color) {
                        canvas->DrawColor(skity::Color_BLUE);
                      }
                      canvas->Restore();
                    } else {
                      paint.SetColor(draw_color ? skity::Color_BLUE
                                                : skity::Color_TRANSPARENT);
                      canvas->DrawRect(bounds, paint);
                    }
                    if (nested) {
                      canvas->Restore();
                    }
                  });
              env->SetSampleCount(previous_samples);
              return texture;
            };
            auto reference = render(false);
            auto layered = render(true);
            ASSERT_NE(reference, nullptr);
            ASSERT_NE(layered, nullptr);
            auto reference_pixels = reference->ReadPixels();
            auto layered_pixels = layered->ReadPixels();
            ASSERT_NE(reference_pixels, nullptr);
            ASSERT_NE(layered_pixels, nullptr);
            auto diff = skity::testing::ComparePixelsExact(layered_pixels,
                                                           reference_pixels);
            ASSERT_EQ(diff.diff_pixel_count, 0u);
          }
        }
      }
    }
  }
}

}  // namespace

TEST(SaveLayerGolden, FractionalBoundsLimitDrawColor) {
  CheckFractionalLayerBounds(true, false);
  CheckFractionalLayerBounds(true, true);
}

TEST(SaveLayerGolden, FractionalBoundsLimitEmptyLayerComposition) {
  CheckFractionalLayerBounds(false, false);
  CheckFractionalLayerBounds(false, true);
}

TEST(SaveLayerGolden, TwoCircle) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));
  auto canvas = recorder.GetRecordingCanvas();

  canvas->Save();
  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::Color_GREEN);
  canvas->Scale(10, 10);
  canvas->DrawCircle(20, 20, 10, paint);

  canvas->SaveLayer(skity::Rect::MakeLTRB(0, 0, 40, 40), skity::Paint{});
  paint.SetColor(skity::Color_RED);
  canvas->DrawCircle(20, 20, 10, paint);
  canvas->Restore();
  canvas->Restore();

  std::filesystem::path golden_path = kGoldenTestImageDir;
  golden_path.append("two_circle.png");
  auto coverage_aa_path = CoverageAAGoldenPath("two_circle.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 400.f, 400.f,
      skity::testing::PathList{.cpu_tess_path = golden_path.c_str(),
                               .gpu_tess_path = golden_path.c_str(),
                               .coverage_aa_path = coverage_aa_path.c_str()}));
}

TEST(SaveLayerGolden, ThreeCircle) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));
  auto canvas = recorder.GetRecordingCanvas();

  canvas->Save();
  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::Color_GREEN);
  canvas->Scale(10.1, 10.1);
  canvas->DrawCircle(20.3, 20.3, 10, paint);

  canvas->SaveLayer(skity::Rect::MakeLTRB(10.3, 10.3, 30.3, 30.3),
                    skity::Paint{});
  paint.SetColor(skity::Color_RED);
  canvas->DrawCircle(20.3, 20.3, 10, paint);
  canvas->SaveLayer(skity::Rect::MakeLTRB(10.3, 10.3, 30.3, 30.3),
                    skity::Paint{});
  paint.SetColor(skity::Color_BLUE);
  canvas->DrawCircle(20.3, 20.3, 10, paint);

  canvas->Restore();
  canvas->Restore();
  canvas->Restore();

  std::filesystem::path golden_path = kGoldenTestImageDir;
  golden_path.append("three_circle.png");
  auto coverage_aa_path = CoverageAAGoldenPath("three_circle.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 400.f, 400.f,
      skity::testing::PathList{.cpu_tess_path = golden_path.c_str(),
                               .gpu_tess_path = golden_path.c_str(),
                               .coverage_aa_path = coverage_aa_path.c_str()}));
}

TEST(SaveLayerGolden, TwoCircleWithTranslate) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));
  auto canvas = recorder.GetRecordingCanvas();

  canvas->Save();
  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::Color_GREEN);
  canvas->Scale(10, 10);
  canvas->DrawCircle(20, 20, 10, paint);
  skity::Paint restore_paint;
  restore_paint.SetImageFilter(
      skity::ImageFilters::MatrixTransform(skity::Matrix::Translate(5, 0)));
  canvas->SaveLayer(skity::Rect::MakeLTRB(0, 0, 400, 400), restore_paint);
  paint.SetColor(skity::Color_RED);
  canvas->DrawCircle(20, 20, 10, paint);
  canvas->Restore();
  canvas->Restore();

  std::filesystem::path golden_path = kGoldenTestImageDir;
  golden_path.append("two_circle_with_translate.png");
  auto coverage_aa_path = CoverageAAGoldenPath("two_circle_with_translate.png");

  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 400.f, 400.f,
      skity::testing::PathList{.cpu_tess_path = golden_path.c_str(),
                               .gpu_tess_path = golden_path.c_str(),
                               .coverage_aa_path = coverage_aa_path.c_str()}));
}

TEST(SaveLayerGolden, PerspectiveZ0Plane) {
  constexpr uint32_t kWidth = 400;
  constexpr uint32_t kHeight = 400;
  const auto layer_bounds =
      skity::Rect::MakeLTRB(-23.f, -16.5f, 558.5f, 172.5f);

  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(kWidth, kHeight));
  auto canvas = recorder.GetRecordingCanvas();
  canvas->Clear(skity::Color_WHITE);

  constexpr float kCompositionWidth = 750.f;
  constexpr float kCompositionHeight = 1628.f;
  float scale = kHeight / kCompositionHeight;
  canvas->Translate((kWidth - kCompositionWidth * scale) * 0.5f, 0.f);
  canvas->Scale(scale, scale);
  canvas->Concat(
      skity::Camera(kCompositionWidth, kCompositionHeight).GetFixedCamera());

  auto model = skity::Matrix::Translate(565.f, 811.f) *
               skity::Matrix::Translate(0.f, 60.f) *
               skity::Matrix::Translate(-249.f, -131.f) *
               skity::Matrix::Translate(248.5f, -18.5f) *
               skity::Matrix::RotateDeg(-135.f, {1.f, 0.f, 0.f}) *
               skity::Matrix::Translate(-249.f, -172.5f);
  canvas->Concat(model);

  canvas->SaveLayer(layer_bounds, skity::Paint{});
  skity::Bitmap bitmap(8, 3);
  for (uint32_t y = 0; y < bitmap.Height(); y++) {
    for (uint32_t x = 0; x < bitmap.Width(); x++) {
      auto color = (x + y) % 2 == 0 ? skity::ColorSetARGB(255, 30, 64, 175)
                                    : skity::ColorSetARGB(255, 96, 165, 250);
      if (x == y) {
        color = skity::Color_YELLOW;
      }
      bitmap.SetPixel(x, y, color);
    }
  }
  auto image = skity::Image::MakeImage(bitmap.GetPixmap());
  canvas->DrawImage(image, layer_bounds);

  skity::Paint mask_layer_paint;
  mask_layer_paint.SetBlendMode(skity::BlendMode::kDstIn);
  canvas->SaveLayer(layer_bounds, mask_layer_paint);
  skity::Path flap;
  flap.MoveTo(55.f, -16.5f);
  flap.LineTo(480.f, -16.5f);
  flap.LineTo(558.5f, 172.5f);
  flap.LineTo(-23.f, 172.5f);
  flap.Close();
  skity::Paint mask_paint;
  mask_paint.SetColor(skity::Color_WHITE);
  canvas->DrawPath(flap, mask_paint);
  canvas->Restore();
  canvas->Restore();

  std::filesystem::path golden_path = kGoldenTestImageDir;
  golden_path.append("perspective_z0_plane.png");
  auto coverage_aa_path = CoverageAAGoldenPath("perspective_z0_plane.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), kWidth, kHeight,
      skity::testing::PathList{.cpu_tess_path = golden_path.c_str(),
                               .gpu_tess_path = golden_path.c_str(),
                               .coverage_aa_path = coverage_aa_path.c_str()}));
}

TEST(SaveLayerGolden, SingularMatrixDoesNotCrash) {
  const float values[9] = {
      1.f, 1.f, 0.f,  //
      1.f, 1.f, 0.f,  //
      0.f, 0.f, 1.f,  //
  };
  skity::Matrix singular_matrix;
  singular_matrix.Set9(values);
  ASSERT_FALSE(singular_matrix.InvertZ0Plane(nullptr));

  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  ASSERT_NE(env, nullptr);
  auto texture =
      env->RenderToTexture(64, 64, [singular_matrix](skity::Canvas* canvas) {
        canvas->SetMatrix(singular_matrix);
        const int save_count = canvas->GetSaveCount();
        canvas->SaveLayer(skity::Rect::MakeWH(32.f, 32.f), skity::Paint{});

        skity::Paint paint;
        paint.SetColor(skity::Color_RED);
        canvas->SetMatrix(skity::Matrix::Translate(8.f, 8.f));
        canvas->DrawRect(skity::Rect::MakeWH(24.f, 24.f), paint);

        paint.SetColor(skity::Color_BLUE);
        canvas->ResetMatrix();
        canvas->DrawCircle(24.f, 24.f, 12.f, paint);
        canvas->Restore();
        EXPECT_EQ(canvas->GetSaveCount(), save_count);

        paint.SetColor(skity::Color_GREEN);
        canvas->ResetMatrix();
        canvas->DrawRect(skity::Rect::MakeXYWH(48.f, 48.f, 8.f, 8.f), paint);
      });

  ASSERT_NE(texture, nullptr);
  EXPECT_NE(texture->ReadPixels(), nullptr);
}

TEST(SaveLayerGolden, ResetMatrixDrawColorFillsLayer) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  ASSERT_NE(env, nullptr);
  auto texture = env->RenderToTexture(80, 40, [](skity::Canvas* canvas) {
    canvas->Clear(skity::Color_GREEN);
    canvas->Translate(20.f, 0.f);
    canvas->SaveLayer(skity::Rect::MakeWH(40.f, 40.f), skity::Paint{});
    canvas->ResetMatrix();
    canvas->DrawColor(skity::Color_RED);
    canvas->Restore();
  });

  ASSERT_NE(texture, nullptr);
  auto pixels = texture->ReadPixels();
  ASSERT_NE(pixels, nullptr);
  skity::Bitmap bitmap(std::move(pixels));
  EXPECT_EQ(bitmap.GetPixel(25, 20), skity::Color_RED);
  EXPECT_EQ(bitmap.GetPixel(55, 20), skity::Color_RED);
  EXPECT_EQ(bitmap.GetPixel(65, 20), skity::Color_GREEN);
}

TEST(SaveLayerGolden, FailedSaveLayerPreservesMatrix) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  ASSERT_NE(env, nullptr);
  auto texture = env->RenderToTexture(80, 40, [](skity::Canvas* canvas) {
    canvas->Clear(skity::Color_GREEN);
    canvas->Translate(20.f, 0.f);

    skity::Paint layer_paint;
    layer_paint.SetImageFilter(
        skity::ImageFilters::MatrixTransform(skity::Matrix{}));
    canvas->SaveLayer(skity::Rect::MakeWH(100000.f, 100000.f), layer_paint);

    skity::Paint paint;
    paint.SetColor(skity::Color_RED);
    canvas->SetMatrix(skity::Matrix::Translate(40.f, 0.f));
    canvas->DrawRect(skity::Rect::MakeXYWH(0.f, 4.f, 8.f, 8.f), paint);

    paint.SetColor(skity::Color_BLUE);
    canvas->ResetMatrix();
    canvas->DrawRect(skity::Rect::MakeXYWH(40.f, 16.f, 8.f, 8.f), paint);
    canvas->Restore();

    paint.SetColor(skity::Color_WHITE);
    canvas->DrawRect(skity::Rect::MakeXYWH(0.f, 28.f, 8.f, 8.f), paint);
  });

  ASSERT_NE(texture, nullptr);
  auto pixels = texture->ReadPixels();
  ASSERT_NE(pixels, nullptr);
  skity::Bitmap bitmap(std::move(pixels));
  EXPECT_EQ(bitmap.GetPixel(44, 8), skity::Color_RED);
  EXPECT_EQ(bitmap.GetPixel(24, 8), skity::Color_GREEN);
  EXPECT_EQ(bitmap.GetPixel(44, 20), skity::Color_BLUE);
  EXPECT_EQ(bitmap.GetPixel(24, 20), skity::Color_GREEN);
  EXPECT_EQ(bitmap.GetPixel(24, 32), skity::Color_WHITE);
  EXPECT_EQ(bitmap.GetPixel(4, 32), skity::Color_GREEN);
}

TEST(SaveLayerGolden, NearSingularResetMatrixDrawColorFillsLayer) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  ASSERT_NE(env, nullptr);
  auto texture = env->RenderToTexture(80, 40, [](skity::Canvas* canvas) {
    canvas->Translate(20.f, 0.f);
    canvas->Scale(100.f, 100.f);
    canvas->Skew(0.25f, 0.f);
    canvas->SaveLayer(skity::Rect::MakeWH(0.4f, 0.4f), skity::Paint{});
    canvas->ResetMatrix();
    canvas->DrawColor(skity::Color_RED);
    canvas->Restore();
  });

  ASSERT_NE(texture, nullptr);
  auto pixels = texture->ReadPixels();
  ASSERT_NE(pixels, nullptr);
  skity::Bitmap bitmap(std::move(pixels));
  EXPECT_EQ(bitmap.GetPixel(35, 20), skity::Color_RED);
  EXPECT_EQ(bitmap.GetPixel(55, 20), skity::Color_RED);
}

TEST(SaveLayerGolden, NearSingularDrawPaintPreservesShaderCoordinates) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  ASSERT_NE(env, nullptr);
  auto texture = env->RenderToTexture(80, 40, [](skity::Canvas* canvas) {
    canvas->Translate(20.f, 0.f);
    canvas->Scale(100.f, 100.f);
    canvas->Skew(0.25f, 0.f);
    canvas->SaveLayer(skity::Rect::MakeWH(0.4f, 0.4f), skity::Paint{});
    canvas->ResetMatrix();

    skity::Point points[] = {{20.f, 0.f, 0.f, 1.f}, {40.f, 0.f, 0.f, 1.f}};
    skity::Vec4 colors[] = {skity::Colors::kRed, skity::Colors::kBlue};
    skity::Paint paint;
    paint.SetShader(skity::Shader::MakeLinear(points, colors, nullptr, 2));
    canvas->DrawPaint(paint);
    canvas->Restore();
  });

  ASSERT_NE(texture, nullptr);
  auto pixels = texture->ReadPixels();
  ASSERT_NE(pixels, nullptr);
  skity::Bitmap bitmap(std::move(pixels));
  EXPECT_EQ(bitmap.GetPixel(55, 20), skity::Color_BLUE);
}

TEST(SaveLayerGolden, NearSingularDrawPaintWithImageFilterFillsLayer) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  ASSERT_NE(env, nullptr);
  auto texture = env->RenderToTexture(80, 40, [](skity::Canvas* canvas) {
    canvas->Translate(20.f, 0.f);
    canvas->Scale(100.f, 100.f);
    canvas->Skew(0.25f, 0.f);
    canvas->SaveLayer(skity::Rect::MakeWH(0.4f, 0.4f), skity::Paint{});
    canvas->ResetMatrix();

    skity::Paint paint;
    paint.SetColor(skity::Color_RED);
    paint.SetImageFilter(skity::ImageFilters::MatrixTransform(skity::Matrix{}));
    canvas->DrawPaint(paint);
    canvas->Restore();
  });

  ASSERT_NE(texture, nullptr);
  auto pixels = texture->ReadPixels();
  ASSERT_NE(pixels, nullptr);
  skity::Bitmap bitmap(std::move(pixels));
  EXPECT_EQ(bitmap.GetPixel(35, 20), skity::Color_RED);
  EXPECT_EQ(bitmap.GetPixel(55, 20), skity::Color_RED);
}
