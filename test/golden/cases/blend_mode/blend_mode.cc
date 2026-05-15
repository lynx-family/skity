// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <string_view>
#include <skity/graphic/blend_mode.hpp>
#include <skity/graphic/color.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/recorder/picture_recorder.hpp>

#include "common/golden_test_env.hpp"
#include "common/golden_test_check.hpp"

static const char* kGoldenTestImageDir = CASE_DIR;

static bool HasPrefix(std::string_view value, std::string_view prefix) {
  return value.size() >= prefix.size() &&
         value.substr(0, prefix.size()) == prefix;
}

static bool IsBasicSaveLayerBlendModeFileName(std::string_view file_name) {
  static constexpr std::array<std::string_view, 16> kSaveLayerBlendModeFiles = {
      "Modulate.png",   "Screen.png",     "Overlay.png",   "Darken.png",
      "Lighten.png",    "ColorDodge.png", "ColorBurn.png", "HardLight.png",
      "SoftLight.png",  "Difference.png", "Exclusion.png", "Multiply.png",
      "Hue.png",        "Saturation.png", "Color.png",     "Luminosity.png",
  };

  for (auto name : kSaveLayerBlendModeFiles) {
    if (file_name == name) {
      return true;
    }
  }

  return false;
}

static std::string ResolveBlendModeGoldenFileName(std::string_view file_name) {
  if (file_name == "blend_mode_composite_framebuffer_fetch_sample_count_4.png" ||
      file_name == "blend_mode_composite_texture_copy_sample_count_1.png" ||
      file_name == "blend_mode_composite_texture_copy_sample_count_4.png") {
    return "blend_mode_composite_framebuffer_fetch_sample_count_1.png";
  }

  if (file_name == "blend_mode_clip_rect_framebuffer_fetch_sample_count_4.png" ||
      file_name == "blend_mode_clip_rect_texture_copy_sample_count_1.png" ||
      file_name == "blend_mode_clip_rect_texture_copy_sample_count_4.png" ||
      file_name ==
          "blend_mode_savelayer_clip_rect_framebuffer_fetch_sample_count_1.png" ||
      file_name ==
          "blend_mode_savelayer_clip_rect_framebuffer_fetch_sample_count_4.png" ||
      file_name ==
          "blend_mode_savelayer_clip_rect_texture_copy_sample_count_1.png" ||
      file_name ==
          "blend_mode_savelayer_clip_rect_texture_copy_sample_count_4.png") {
    return "blend_mode_clip_rect_framebuffer_fetch_sample_count_1.png";
  }

  if (file_name == "blend_mode_clip_replay_texture_copy_sample_count_1.png" ||
      file_name ==
          "blend_mode_savelayer_clip_replay_framebuffer_fetch_sample_count_1.png" ||
      file_name ==
          "blend_mode_savelayer_clip_replay_texture_copy_sample_count_1.png") {
    return "blend_mode_clip_replay_framebuffer_fetch_sample_count_1.png";
  }

  if (file_name == "blend_mode_clip_replay_texture_copy_sample_count_4.png" ||
      file_name ==
          "blend_mode_savelayer_clip_replay_framebuffer_fetch_sample_count_4.png" ||
      file_name ==
          "blend_mode_savelayer_clip_replay_texture_copy_sample_count_4.png") {
    return "blend_mode_clip_replay_framebuffer_fetch_sample_count_4.png";
  }

  constexpr std::string_view kSaveLayerPrefix = "blend_mode_savelayer_";
  if (HasPrefix(file_name, kSaveLayerPrefix)) {
    auto suffix = file_name.substr(kSaveLayerPrefix.size());
    if (IsBasicSaveLayerBlendModeFileName(suffix)) {
      return "blend_mode_savelayer_texture_copy_sample_count_1_" +
             std::string(suffix);
    }
  }

  constexpr std::string_view kSaveLayerTextureCopySampleCount4Prefix =
      "blend_mode_savelayer_texture_copy_sample_count_4_";
  if (HasPrefix(file_name, kSaveLayerTextureCopySampleCount4Prefix)) {
    auto suffix = file_name.substr(kSaveLayerTextureCopySampleCount4Prefix.size());
    if (IsBasicSaveLayerBlendModeFileName(suffix)) {
      return "blend_mode_savelayer_texture_copy_sample_count_1_" +
             std::string(suffix);
    }
  }

  return std::string(file_name);
}

static std::filesystem::path MakeBlendModeGoldenPath(std::string_view file_name) {
  std::filesystem::path golden_path = kGoldenTestImageDir;
  golden_path.append(ResolveBlendModeGoldenFileName(file_name));
  return golden_path;
}

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

  auto golden_path =
      MakeBlendModeGoldenPath(std::string("blend_mode_") + name + ".png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 100.f, 100.f,
      skity::testing::PathList{.cpu_tess_path = golden_path.c_str(),
                               .gpu_tess_path = golden_path.c_str()}));
}

static skity::testing::GoldenTestEnvConfig TextureCopyConfig(
    uint32_t sample_count) {
  skity::testing::GoldenTestEnvConfig config;
  config.supports_framebuffer_fetch = false;
  config.sample_count = sample_count;
  config.use_backend_specific_golden = true;
  return config;
}

static skity::testing::GoldenTestEnvConfig DrawTextureSurfaceConfig(
    uint32_t sample_count) {
  auto config = TextureCopyConfig(sample_count);
  config.gl_surface_mode = skity::GLSurfaceMode::kDrawTexture;
  return config;
}

static void RunBlendModeDrawTextureModeRootTextureCopyTest(
    skity::BlendMode mode, const char* name, uint32_t sample_count) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  if (env == nullptr || env->GetBackend() != skity::testing::Backend::kGL) {
    GTEST_SKIP() << "DrawTextureSurfaceGL mode override is only validated on GL";
  }

  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(100.f, 100.f));
  auto canvas = recorder.GetRecordingCanvas();

  DrawBlendMode(canvas, mode);

  auto golden_path = MakeBlendModeGoldenPath(
      std::string("blend_mode_texture_copy_sample_count_") +
      std::to_string(sample_count) + "_" + name + ".png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 100.f, 100.f, golden_path.c_str(),
      DrawTextureSurfaceConfig(sample_count)));
}

static skity::testing::GoldenTestEnvConfig FramebufferFetchConfig(
    uint32_t sample_count) {
  skity::testing::GoldenTestEnvConfig config;
  config.supports_framebuffer_fetch = true;
  config.sample_count = sample_count;
  config.use_backend_specific_golden = true;
  return config;
}

static void RunBlendModeTextureCopyTest(skity::BlendMode mode,
                                        const char* name,
                                        uint32_t sample_count) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(100.f, 100.f));
  auto canvas = recorder.GetRecordingCanvas();

  DrawBlendMode(canvas, mode);

  auto golden_path = MakeBlendModeGoldenPath(
      std::string("blend_mode_texture_copy_sample_count_") +
      std::to_string(sample_count) + "_" + name + ".png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 100.f, 100.f, golden_path.c_str(),
      TextureCopyConfig(sample_count)));
}

#define BLEND_MODE_TEST(mode_name)                                \
  TEST(BlendModeGolden, mode_name) {                              \
    RunBlendModeTest(skity::BlendMode::k##mode_name, #mode_name); \
  }

#define BLEND_MODE_TEXTURE_COPY_TEST(mode_name)                         \
  TEST(BlendModeGolden, TextureCopySampleCount1_##mode_name) {          \
    RunBlendModeTextureCopyTest(skity::BlendMode::k##mode_name,         \
                                #mode_name, 1);                         \
  }                                                                     \
  TEST(BlendModeGolden, TextureCopySampleCount4_##mode_name) {          \
    RunBlendModeTextureCopyTest(skity::BlendMode::k##mode_name,         \
                                #mode_name, 4);                         \
  }

#define BLEND_MODE_DRAW_TEXTURE_MODE_TEXTURE_COPY_TEST(mode_name)        \
  TEST(BlendModeGolden, DrawTextureMode_TextureCopySampleCount1_##mode_name) { \
    RunBlendModeDrawTextureModeRootTextureCopyTest(                      \
        skity::BlendMode::k##mode_name, #mode_name, 1);                 \
  }                                                                     \
  TEST(BlendModeGolden, DrawTextureMode_TextureCopySampleCount4_##mode_name) { \
    RunBlendModeDrawTextureModeRootTextureCopyTest(                      \
        skity::BlendMode::k##mode_name, #mode_name, 4);                 \
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

BLEND_MODE_TEXTURE_COPY_TEST(Modulate)
BLEND_MODE_TEXTURE_COPY_TEST(Screen)
BLEND_MODE_TEXTURE_COPY_TEST(Overlay)
BLEND_MODE_TEXTURE_COPY_TEST(Darken)
BLEND_MODE_TEXTURE_COPY_TEST(Lighten)
BLEND_MODE_TEXTURE_COPY_TEST(ColorDodge)
BLEND_MODE_TEXTURE_COPY_TEST(ColorBurn)
BLEND_MODE_TEXTURE_COPY_TEST(HardLight)
BLEND_MODE_TEXTURE_COPY_TEST(SoftLight)
BLEND_MODE_TEXTURE_COPY_TEST(Difference)
BLEND_MODE_TEXTURE_COPY_TEST(Exclusion)
BLEND_MODE_TEXTURE_COPY_TEST(Multiply)
BLEND_MODE_TEXTURE_COPY_TEST(Hue)
BLEND_MODE_TEXTURE_COPY_TEST(Saturation)
BLEND_MODE_TEXTURE_COPY_TEST(Color)
BLEND_MODE_TEXTURE_COPY_TEST(Luminosity)

BLEND_MODE_DRAW_TEXTURE_MODE_TEXTURE_COPY_TEST(Overlay)

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

  auto golden_path =
      MakeBlendModeGoldenPath(std::string("blend_mode_savelayer_") + name +
                              ".png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 100.f, 100.f,
      skity::testing::PathList{.cpu_tess_path = golden_path.c_str(),
                               .gpu_tess_path = golden_path.c_str()}));
}

static void RunBlendModeSaveLayerTextureCopyTest(skity::BlendMode mode,
                                                 const char* name,
                                                 uint32_t sample_count) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(100.f, 100.f));
  auto canvas = recorder.GetRecordingCanvas();

  DrawBlendModeSaveLayer(canvas, mode);

  auto golden_path = MakeBlendModeGoldenPath(
      std::string("blend_mode_savelayer_texture_copy_sample_count_") +
      std::to_string(sample_count) + "_" + name + ".png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 100.f, 100.f, golden_path.c_str(),
      TextureCopyConfig(sample_count)));
}

#define BLEND_MODE_SAVE_LAYER_TEST(mode_name)                              \
  TEST(BlendModeGolden, SaveLayer_##mode_name) {                           \
    RunBlendModeSaveLayerTest(skity::BlendMode::k##mode_name, #mode_name); \
  }

#define BLEND_MODE_SAVE_LAYER_TEXTURE_COPY_TEST(mode_name)                \
  TEST(BlendModeGolden, SaveLayer_TextureCopySampleCount1_##mode_name) {  \
    RunBlendModeSaveLayerTextureCopyTest(skity::BlendMode::k##mode_name,  \
                                         #mode_name, 1);                  \
  }                                                                       \
  TEST(BlendModeGolden, SaveLayer_TextureCopySampleCount4_##mode_name) {  \
    RunBlendModeSaveLayerTextureCopyTest(skity::BlendMode::k##mode_name,  \
                                         #mode_name, 4);                  \
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

BLEND_MODE_SAVE_LAYER_TEXTURE_COPY_TEST(Modulate)
BLEND_MODE_SAVE_LAYER_TEXTURE_COPY_TEST(Screen)
BLEND_MODE_SAVE_LAYER_TEXTURE_COPY_TEST(Overlay)
BLEND_MODE_SAVE_LAYER_TEXTURE_COPY_TEST(Darken)
BLEND_MODE_SAVE_LAYER_TEXTURE_COPY_TEST(Lighten)
BLEND_MODE_SAVE_LAYER_TEXTURE_COPY_TEST(ColorDodge)
BLEND_MODE_SAVE_LAYER_TEXTURE_COPY_TEST(ColorBurn)
BLEND_MODE_SAVE_LAYER_TEXTURE_COPY_TEST(HardLight)
BLEND_MODE_SAVE_LAYER_TEXTURE_COPY_TEST(SoftLight)
BLEND_MODE_SAVE_LAYER_TEXTURE_COPY_TEST(Difference)
BLEND_MODE_SAVE_LAYER_TEXTURE_COPY_TEST(Exclusion)
BLEND_MODE_SAVE_LAYER_TEXTURE_COPY_TEST(Multiply)
BLEND_MODE_SAVE_LAYER_TEXTURE_COPY_TEST(Hue)
BLEND_MODE_SAVE_LAYER_TEXTURE_COPY_TEST(Saturation)
BLEND_MODE_SAVE_LAYER_TEXTURE_COPY_TEST(Color)
BLEND_MODE_SAVE_LAYER_TEXTURE_COPY_TEST(Luminosity)

static void DrawBlendModeComposite(skity::Canvas* canvas) {
  skity::Paint paint;
  paint.SetColor(skity::ColorSetARGB(255, 250, 250, 250));
  canvas->DrawRect(skity::Rect::MakeWH(128.f, 128.f), paint);

  canvas->SaveLayer(skity::Rect::MakeWH(128.f, 128.f), skity::Paint{});

  paint.SetBlendMode(skity::BlendMode::kSrcOver);
  paint.SetColor(skity::ColorSetARGB(255, 233, 30, 99));
  canvas->DrawRect(skity::Rect::MakeLTRB(12.f, 12.f, 116.f, 116.f), paint);

  paint.SetBlendMode(skity::BlendMode::kOverlay);
  paint.SetColor(skity::ColorSetARGB(220, 22, 150, 243));
  canvas->DrawRect(skity::Rect::MakeLTRB(22.f, 22.f, 84.f, 84.f), paint);

  paint.SetBlendMode(skity::BlendMode::kHardLight);
  paint.SetColor(skity::ColorSetARGB(210, 76, 175, 80));
  canvas->DrawRect(skity::Rect::MakeLTRB(50.f, 50.f, 122.f, 122.f), paint);

  canvas->Restore();
}

static void RunBlendModeCompositeTest(
    const char* path_name, skity::testing::GoldenTestEnvConfig config) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(128.f, 128.f));
  auto canvas = recorder.GetRecordingCanvas();

  DrawBlendModeComposite(canvas);

  auto golden_path = MakeBlendModeGoldenPath(path_name);
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 128.f, 128.f, golden_path.c_str(), config));
}

#define BLEND_MODE_COMPOSITE_TEST(path, config)                  \
  RunBlendModeCompositeTest("blend_mode_composite_" path ".png", \
                            config)

TEST(BlendModeGolden, CompositeFramebufferFetchSampleCount1) {
  BLEND_MODE_COMPOSITE_TEST("framebuffer_fetch_sample_count_1",
                            FramebufferFetchConfig(1));
}

TEST(BlendModeGolden, CompositeFramebufferFetchSampleCount4) {
  BLEND_MODE_COMPOSITE_TEST("framebuffer_fetch_sample_count_4",
                            FramebufferFetchConfig(4));
}

TEST(BlendModeGolden, CompositeTextureCopySampleCount1) {
  BLEND_MODE_COMPOSITE_TEST("texture_copy_sample_count_1",
                            TextureCopyConfig(1));
}

TEST(BlendModeGolden, CompositeTextureCopySampleCount4) {
  BLEND_MODE_COMPOSITE_TEST("texture_copy_sample_count_4",
                            TextureCopyConfig(4));
}

TEST(BlendModeGolden, CompositeTextureCopyDrawTextureModeSampleCount1) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  if (env == nullptr || env->GetBackend() != skity::testing::Backend::kGL) {
    GTEST_SKIP() << "DrawTextureSurfaceGL mode override is only validated on GL";
  }

  BLEND_MODE_COMPOSITE_TEST("texture_copy_sample_count_1",
                            DrawTextureSurfaceConfig(1));
}

TEST(BlendModeGolden, CompositeTextureCopyDrawTextureModeSampleCount4) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  if (env == nullptr || env->GetBackend() != skity::testing::Backend::kGL) {
    GTEST_SKIP() << "DrawTextureSurfaceGL mode override is only validated on GL";
  }

  BLEND_MODE_COMPOSITE_TEST("texture_copy_sample_count_4",
                            DrawTextureSurfaceConfig(4));
}

static void DrawBlendModeClipReplay(skity::Canvas* canvas) {
  skity::Paint paint;
  paint.SetColor(skity::ColorSetARGB(255, 250, 250, 250));
  canvas->DrawRect(skity::Rect::MakeWH(128.f, 128.f), paint);

  paint.SetColor(skity::ColorSetARGB(255, 233, 30, 99));
  canvas->DrawRect(skity::Rect::MakeLTRB(12.f, 12.f, 116.f, 116.f), paint);

  skity::Path clip_path;
  clip_path.AddCircle(64.f, 64.f, 38.f);
  canvas->ClipPath(clip_path, skity::Canvas::ClipOp::kIntersect);

  paint.SetBlendMode(skity::BlendMode::kOverlay);
  paint.SetColor(skity::ColorSetARGB(220, 22, 150, 243));
  canvas->DrawRect(skity::Rect::MakeLTRB(18.f, 18.f, 92.f, 92.f), paint);

  paint.SetBlendMode(skity::BlendMode::kHardLight);
  paint.SetColor(skity::ColorSetARGB(220, 76, 175, 80));
  canvas->DrawRect(skity::Rect::MakeLTRB(42.f, 42.f, 120.f, 120.f), paint);
}

static void RunBlendModeClipReplayTest(
    const char* path_name, skity::testing::GoldenTestEnvConfig config) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(128.f, 128.f));
  auto canvas = recorder.GetRecordingCanvas();

  DrawBlendModeClipReplay(canvas);

  auto golden_path = MakeBlendModeGoldenPath(path_name);
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 128.f, 128.f, golden_path.c_str(), config));
}

static void DrawBlendModeSaveLayerClipReplay(skity::Canvas* canvas) {
  skity::Paint paint;
  paint.SetColor(skity::ColorSetARGB(255, 250, 250, 250));
  canvas->DrawRect(skity::Rect::MakeWH(128.f, 128.f), paint);

  canvas->SaveLayer(skity::Rect::MakeWH(128.f, 128.f), skity::Paint{});

  paint.SetColor(skity::ColorSetARGB(255, 233, 30, 99));
  canvas->DrawRect(skity::Rect::MakeLTRB(12.f, 12.f, 116.f, 116.f), paint);

  skity::Path clip_path;
  clip_path.AddCircle(64.f, 64.f, 38.f);
  canvas->ClipPath(clip_path, skity::Canvas::ClipOp::kIntersect);

  paint.SetBlendMode(skity::BlendMode::kOverlay);
  paint.SetColor(skity::ColorSetARGB(220, 22, 150, 243));
  canvas->DrawRect(skity::Rect::MakeLTRB(18.f, 18.f, 92.f, 92.f), paint);

  paint.SetBlendMode(skity::BlendMode::kHardLight);
  paint.SetColor(skity::ColorSetARGB(220, 76, 175, 80));
  canvas->DrawRect(skity::Rect::MakeLTRB(42.f, 42.f, 120.f, 120.f), paint);

  canvas->Restore();
}

static void RunBlendModeSaveLayerClipReplayTest(
    const char* path_name, skity::testing::GoldenTestEnvConfig config) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(128.f, 128.f));
  auto canvas = recorder.GetRecordingCanvas();

  DrawBlendModeSaveLayerClipReplay(canvas);

  auto golden_path = MakeBlendModeGoldenPath(path_name);
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 128.f, 128.f, golden_path.c_str(), config));
}

static void DrawBlendModeClipRect(skity::Canvas* canvas) {
  skity::Paint paint;
  paint.SetColor(skity::ColorSetARGB(255, 250, 250, 250));
  canvas->DrawRect(skity::Rect::MakeWH(128.f, 128.f), paint);

  paint.SetColor(skity::ColorSetARGB(255, 233, 30, 99));
  canvas->DrawRect(skity::Rect::MakeLTRB(12.f, 12.f, 116.f, 116.f), paint);

  canvas->ClipRect(skity::Rect::MakeLTRB(26.f, 42.f, 102.f, 92.f),
                   skity::Canvas::ClipOp::kIntersect);

  paint.SetBlendMode(skity::BlendMode::kOverlay);
  paint.SetColor(skity::ColorSetARGB(220, 22, 150, 243));
  canvas->DrawRect(skity::Rect::MakeLTRB(18.f, 18.f, 92.f, 92.f), paint);

  paint.SetBlendMode(skity::BlendMode::kHardLight);
  paint.SetColor(skity::ColorSetARGB(220, 76, 175, 80));
  canvas->DrawRect(skity::Rect::MakeLTRB(42.f, 42.f, 120.f, 120.f), paint);
}

static void RunBlendModeClipRectTest(
    const char* path_name, skity::testing::GoldenTestEnvConfig config) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(128.f, 128.f));
  auto canvas = recorder.GetRecordingCanvas();

  DrawBlendModeClipRect(canvas);

  auto golden_path = MakeBlendModeGoldenPath(path_name);
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 128.f, 128.f, golden_path.c_str(), config));
}

static void DrawBlendModeSaveLayerClipRect(skity::Canvas* canvas) {
  skity::Paint paint;
  paint.SetColor(skity::ColorSetARGB(255, 250, 250, 250));
  canvas->DrawRect(skity::Rect::MakeWH(128.f, 128.f), paint);

  canvas->SaveLayer(skity::Rect::MakeWH(128.f, 128.f), skity::Paint{});

  paint.SetColor(skity::ColorSetARGB(255, 233, 30, 99));
  canvas->DrawRect(skity::Rect::MakeLTRB(12.f, 12.f, 116.f, 116.f), paint);

  canvas->ClipRect(skity::Rect::MakeLTRB(26.f, 42.f, 102.f, 92.f),
                   skity::Canvas::ClipOp::kIntersect);

  paint.SetBlendMode(skity::BlendMode::kOverlay);
  paint.SetColor(skity::ColorSetARGB(220, 22, 150, 243));
  canvas->DrawRect(skity::Rect::MakeLTRB(18.f, 18.f, 92.f, 92.f), paint);

  paint.SetBlendMode(skity::BlendMode::kHardLight);
  paint.SetColor(skity::ColorSetARGB(220, 76, 175, 80));
  canvas->DrawRect(skity::Rect::MakeLTRB(42.f, 42.f, 120.f, 120.f), paint);

  canvas->Restore();
}

static void RunBlendModeSaveLayerClipRectTest(
    const char* path_name, skity::testing::GoldenTestEnvConfig config) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(128.f, 128.f));
  auto canvas = recorder.GetRecordingCanvas();

  DrawBlendModeSaveLayerClipRect(canvas);

  auto golden_path = MakeBlendModeGoldenPath(path_name);
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 128.f, 128.f, golden_path.c_str(), config));
}

TEST(BlendModeGolden, ClipReplayFramebufferFetchSampleCount1) {
  RunBlendModeClipReplayTest("blend_mode_clip_replay_framebuffer_fetch_sample_count_1.png",
                             FramebufferFetchConfig(1));
}

TEST(BlendModeGolden, ClipReplayFramebufferFetchSampleCount4) {
  RunBlendModeClipReplayTest("blend_mode_clip_replay_framebuffer_fetch_sample_count_4.png",
                             FramebufferFetchConfig(4));
}

TEST(BlendModeGolden, ClipReplayTextureCopySampleCount1) {
  RunBlendModeClipReplayTest("blend_mode_clip_replay_texture_copy_sample_count_1.png",
                             TextureCopyConfig(1));
}

TEST(BlendModeGolden, ClipReplayTextureCopySampleCount4) {
  RunBlendModeClipReplayTest("blend_mode_clip_replay_texture_copy_sample_count_4.png",
                             TextureCopyConfig(4));
}

TEST(BlendModeGolden, SaveLayerClipReplayFramebufferFetchSampleCount1) {
  RunBlendModeSaveLayerClipReplayTest(
      "blend_mode_savelayer_clip_replay_framebuffer_fetch_sample_count_1.png",
      FramebufferFetchConfig(1));
}

TEST(BlendModeGolden, SaveLayerClipReplayFramebufferFetchSampleCount4) {
  RunBlendModeSaveLayerClipReplayTest(
      "blend_mode_savelayer_clip_replay_framebuffer_fetch_sample_count_4.png",
      FramebufferFetchConfig(4));
}

TEST(BlendModeGolden, SaveLayerClipReplayTextureCopySampleCount1) {
  RunBlendModeSaveLayerClipReplayTest(
      "blend_mode_savelayer_clip_replay_texture_copy_sample_count_1.png",
      TextureCopyConfig(1));
}

TEST(BlendModeGolden, SaveLayerClipReplayTextureCopySampleCount4) {
  RunBlendModeSaveLayerClipReplayTest(
      "blend_mode_savelayer_clip_replay_texture_copy_sample_count_4.png",
      TextureCopyConfig(4));
}

TEST(BlendModeGolden, ClipRectFramebufferFetchSampleCount1) {
  RunBlendModeClipRectTest(
      "blend_mode_clip_rect_framebuffer_fetch_sample_count_1.png",
      FramebufferFetchConfig(1));
}

TEST(BlendModeGolden, ClipRectFramebufferFetchSampleCount4) {
  RunBlendModeClipRectTest(
      "blend_mode_clip_rect_framebuffer_fetch_sample_count_4.png",
      FramebufferFetchConfig(4));
}

TEST(BlendModeGolden, ClipRectTextureCopySampleCount1) {
  RunBlendModeClipRectTest("blend_mode_clip_rect_texture_copy_sample_count_1.png",
                           TextureCopyConfig(1));
}

TEST(BlendModeGolden, ClipRectTextureCopySampleCount4) {
  RunBlendModeClipRectTest("blend_mode_clip_rect_texture_copy_sample_count_4.png",
                           TextureCopyConfig(4));
}

TEST(BlendModeGolden, SaveLayerClipRectFramebufferFetchSampleCount1) {
  RunBlendModeSaveLayerClipRectTest(
      "blend_mode_savelayer_clip_rect_framebuffer_fetch_sample_count_1.png",
      FramebufferFetchConfig(1));
}

TEST(BlendModeGolden, SaveLayerClipRectFramebufferFetchSampleCount4) {
  RunBlendModeSaveLayerClipRectTest(
      "blend_mode_savelayer_clip_rect_framebuffer_fetch_sample_count_4.png",
      FramebufferFetchConfig(4));
}

TEST(BlendModeGolden, SaveLayerClipRectTextureCopySampleCount1) {
  RunBlendModeSaveLayerClipRectTest(
      "blend_mode_savelayer_clip_rect_texture_copy_sample_count_1.png",
      TextureCopyConfig(1));
}

TEST(BlendModeGolden, SaveLayerClipRectTextureCopySampleCount4) {
  RunBlendModeSaveLayerClipRectTest(
      "blend_mode_savelayer_clip_rect_texture_copy_sample_count_4.png",
      TextureCopyConfig(4));
}
