// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <filesystem>
#include <skity/graphic/blend_mode.hpp>
#include <skity/graphic/color.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/recorder/picture_recorder.hpp>

#include "common/golden_test_check.hpp"

static const char* kGoldenTestImageDir = CASE_DIR;

static void DrawBlendMode(skity::Canvas* canvas, skity::BlendMode mode) {
  // Draw destination (a circle)
  skity::Paint paint;
  paint.SetColor(skity::ColorSetARGB(255, 233, 30, 99));
  canvas->DrawCircle(40, 40, 30, paint);

  // Draw source (a rectangle)
  paint.SetBlendMode(mode);
  paint.SetColor(skity::ColorSetARGB(255, 22, 150, 243));
  canvas->DrawRect(skity::Rect::MakeLTRB(30, 30, 90, 90), paint);
}

static void RunBlendModeTest(skity::BlendMode mode, const char* name) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(100.f, 100.f));
  auto canvas = recorder.GetRecordingCanvas();

  DrawBlendMode(canvas, mode);

  std::filesystem::path golden_path = kGoldenTestImageDir;
  golden_path.append(std::string("blend_mode_") + name + ".png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 100.f, 100.f,
      skity::testing::PathList{.cpu_tess_path = golden_path.c_str(),
                               .gpu_tess_path = golden_path.c_str()}));
}

#define BLEND_MODE_TEST(mode_name)                                \
  TEST(BlendModeGolden, mode_name) {                              \
    RunBlendModeTest(skity::BlendMode::k##mode_name, #mode_name); \
  }

BLEND_MODE_TEST(Modulate)
BLEND_MODE_TEST(Screen)
BLEND_MODE_TEST(Overlay)
BLEND_MODE_TEST(Darken)
BLEND_MODE_TEST(Lighten)
BLEND_MODE_TEST(ColorDodge)
BLEND_MODE_TEST(ColorBurn)
BLEND_MODE_TEST(HardLight)
BLEND_MODE_TEST(SoftLight)
BLEND_MODE_TEST(Difference)
BLEND_MODE_TEST(Exclusion)
BLEND_MODE_TEST(Multiply)
BLEND_MODE_TEST(Hue)
BLEND_MODE_TEST(Saturation)
BLEND_MODE_TEST(Color)
BLEND_MODE_TEST(Luminosity)

static void DrawBlendModeSaveLayer(skity::Canvas* canvas,
                                   skity::BlendMode mode) {
  skity::Paint paint;
  paint.SetColor(skity::ColorSetARGB(255, 233, 30, 99));

  canvas->SaveLayer(skity::Rect::MakeLTRB(0, 0, 100, 100), skity::Paint{});
  canvas->DrawRect(skity::Rect::MakeLTRB(10, 10, 70, 70), paint);

  skity::Paint save_paint;
  save_paint.SetBlendMode(mode);
  canvas->SaveLayer(skity::Rect::MakeLTRB(0, 0, 100, 100), save_paint);

  paint.SetColor(skity::ColorSetARGB(255, 22, 150, 243));
  canvas->DrawRect(skity::Rect::MakeLTRB(30, 30, 90, 90), paint);

  canvas->Restore();
  canvas->Restore();
}

static void RunBlendModeSaveLayerTest(skity::BlendMode mode, const char* name) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(100.f, 100.f));
  auto canvas = recorder.GetRecordingCanvas();

  DrawBlendModeSaveLayer(canvas, mode);

  std::filesystem::path golden_path = kGoldenTestImageDir;
  golden_path.append(std::string("blend_mode_savelayer_") + name + ".png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 100.f, 100.f,
      skity::testing::PathList{.cpu_tess_path = golden_path.c_str(),
                               .gpu_tess_path = golden_path.c_str()}));
}

#define BLEND_MODE_SAVE_LAYER_TEST(mode_name)                              \
  TEST(BlendModeGolden, SaveLayer_##mode_name) {                           \
    RunBlendModeSaveLayerTest(skity::BlendMode::k##mode_name, #mode_name); \
  }

BLEND_MODE_SAVE_LAYER_TEST(Modulate)
BLEND_MODE_SAVE_LAYER_TEST(Screen)
BLEND_MODE_SAVE_LAYER_TEST(Overlay)
BLEND_MODE_SAVE_LAYER_TEST(Darken)
BLEND_MODE_SAVE_LAYER_TEST(Lighten)
BLEND_MODE_SAVE_LAYER_TEST(ColorDodge)
BLEND_MODE_SAVE_LAYER_TEST(ColorBurn)
BLEND_MODE_SAVE_LAYER_TEST(HardLight)
BLEND_MODE_SAVE_LAYER_TEST(SoftLight)
BLEND_MODE_SAVE_LAYER_TEST(Difference)
BLEND_MODE_SAVE_LAYER_TEST(Exclusion)
BLEND_MODE_SAVE_LAYER_TEST(Multiply)
BLEND_MODE_SAVE_LAYER_TEST(Hue)
BLEND_MODE_SAVE_LAYER_TEST(Saturation)
BLEND_MODE_SAVE_LAYER_TEST(Color)
BLEND_MODE_SAVE_LAYER_TEST(Luminosity)
