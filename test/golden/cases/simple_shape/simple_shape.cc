// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <skity/io/parse_path.hpp>
#include <skity/recorder/picture_recorder.hpp>
#include <skity/skity.hpp>
#include <string>

#include "common/golden_test_check.hpp"

static const char* kGoldenTestImageSimpleDir = CASE_DIR;
static const char* kGoldenTestImageCPUTessDir = CASE_DIR "cpu_tess_images/";
static const char* kGoldenTestImageGPUTessDir = CASE_DIR "gpu_tess_images/";
static const char* kGoldenTestImageCoverageAADir =
    CASE_DIR "coverage_aa_images/";

namespace {

struct PathListContext {
  PathListContext(std::string name)
      : expected_image_cpu_tess_path(kGoldenTestImageCPUTessDir),
        expected_image_gpu_tess_path(kGoldenTestImageGPUTessDir),
        expected_image_simple_path(kGoldenTestImageSimpleDir),
        expected_image_coverage_aa_path(kGoldenTestImageCoverageAADir) {
    expected_image_cpu_tess_path.append(name);
    expected_image_gpu_tess_path.append(name);
    expected_image_simple_path.append(name);
    expected_image_coverage_aa_path.append(name);
  }

  skity::testing::PathList ToPathList() const {
    return {
        .cpu_tess_path = expected_image_cpu_tess_path.c_str(),
        .gpu_tess_path = expected_image_gpu_tess_path.c_str(),
        .simple_shape_path = expected_image_simple_path.c_str(),
        .coverage_aa_path = expected_image_coverage_aa_path.c_str(),
    };
  }

  std::filesystem::path expected_image_cpu_tess_path;
  std::filesystem::path expected_image_gpu_tess_path;
  std::filesystem::path expected_image_simple_path;
  std::filesystem::path expected_image_coverage_aa_path;
};

}  // namespace

TEST(SimpleShapeGolden, DrawFilledRect) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));

  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::Color_GREEN);

  canvas->Save();
  canvas->Translate(50.f, 50.f);
  canvas->DrawRect(skity::Rect::MakeWH(50, 50), paint);

  canvas->Translate(100.3f, 50.f);
  canvas->DrawRect(skity::Rect::MakeWH(50, 50), paint);
  canvas->Restore();

  PathListContext context("draw_filled_rect.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 400, 400,
                                                   context.ToPathList()));
}

TEST(SimpleShapeGolden, DrawStrokeRect) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));

  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::Color_GREEN);
  paint.SetStrokeWidth(1);
  paint.SetStyle(skity::Paint::kStroke_Style);
  canvas->Save();
  canvas->Translate(3.f, 50.f);
  canvas->DrawRect(skity::Rect::MakeWH(50, 50), paint);

  canvas->Translate(100, 0);
  paint.SetStrokeWidth(20);
  canvas->DrawRect(skity::Rect::MakeWH(50, 50), paint);

  canvas->Translate(100, 0);
  paint.SetStrokeWidth(49);
  canvas->DrawRect(skity::Rect::MakeWH(50, 50), paint);

  canvas->Translate(120, 0);
  paint.SetStrokeWidth(50);
  canvas->DrawRect(skity::Rect::MakeWH(50, 50), paint);
  canvas->Restore();
  auto dl = recorder.FinishRecording();
  PathListContext context("draw_stroke_rect.png");
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 400, 400,
                                                   context.ToPathList()));
}

TEST(SimpleShapeGolden, DrawStrokeRectWithJoins) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));

  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::Color_GREEN);
  paint.SetStrokeWidth(20);
  paint.SetStyle(skity::Paint::kStroke_Style);
  canvas->Save();
  canvas->Translate(30.f, 50.f);
  paint.SetStrokeJoin(skity::Paint::kBevel_Join);
  canvas->DrawRect(skity::Rect::MakeWH(100, 100), paint);

  canvas->Translate(130, 0);
  paint.SetStrokeJoin(skity::Paint::kRound_Join);
  canvas->DrawRect(skity::Rect::MakeWH(100, 100), paint);

  canvas->Translate(130, 0);
  paint.SetStrokeJoin(skity::Paint::kMiter_Join);
  canvas->DrawRect(skity::Rect::MakeWH(100, 100), paint);
  canvas->Restore();

  auto dl = recorder.FinishRecording();
  PathListContext context("draw_stroke_rect_with_joins.png");
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 400, 400,
                                                   context.ToPathList()));
}

TEST(SimpleShapeGolden, DrawFilledRRect) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));

  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::Color_GREEN);
  canvas->Save();

  canvas->Translate(3.f, 50.f);
  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 150), 20, 20),
      paint);

  canvas->Translate(110, 0);
  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 150), 20, 30),
      paint);

  canvas->Translate(110, 0);

  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 150), 50, 75),
      paint);

  canvas->Restore();

  auto dl = recorder.FinishRecording();
  PathListContext context("draw_filled_rrect.png");
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 400, 400,
                                                   context.ToPathList()));
}

TEST(SimpleShapeGolden, DrawStrokeRRect) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));

  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::Color_GREEN);
  paint.SetStrokeWidth(10);
  paint.SetStyle(skity::Paint::kStroke_Style);

  canvas->Save();

  canvas->Translate(3.f, 50.f);
  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 150), 20, 20),
      paint);

  canvas->Translate(130, 0);
  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 150), 20, 30),
      paint);

  canvas->Translate(130, 0);

  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 150), 50, 75),
      paint);

  canvas->Restore();

  auto dl = recorder.FinishRecording();
  PathListContext context("draw_stroke_rrect.png");
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 400, 400,
                                                   context.ToPathList()));
}

TEST(SimpleShapeGolden, DrawStrokeRRect2) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(600.f, 400.f));

  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::Color_GREEN);

  paint.SetStyle(skity::Paint::kStroke_Style);

  canvas->Save();

  canvas->Translate(30.f, 50.f);
  paint.SetStrokeWidth(10);
  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 150), 20, 20),
      paint);

  canvas->Translate(130, 0);
  paint.SetStrokeWidth(20);
  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 150), 20, 20),
      paint);

  canvas->Translate(150, 0);
  paint.SetStrokeWidth(40);
  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 150), 20, 20),
      paint);

  canvas->Restore();
  auto dl = recorder.FinishRecording();
  PathListContext context("draw_stroke_rrect2.png");
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 600, 400,
                                                   context.ToPathList()));
}

TEST(SimpleShapeGolden, DrawStrokeRRectWithRotate) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));

  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::Color_GREEN);
  paint.SetStrokeWidth(10);
  paint.SetStyle(skity::Paint::kStroke_Style);

  canvas->Save();
  canvas->Rotate(30);
  canvas->Translate(120, -30);

  canvas->Translate(3.f, 50.f);
  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 150), 20, 20),
      paint);

  canvas->Translate(130, 0);
  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 150), 20, 30),
      paint);

  canvas->Translate(130, 0);

  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 150), 50, 75),
      paint);

  canvas->Restore();
  auto dl = recorder.FinishRecording();
  PathListContext context("draw_stroke_rrect_with_rotate.png");

  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 400, 400,
                                                   context.ToPathList()));
}

TEST(SimpleShapeGolden, DrawStrokeRRectWithSkew) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(600.f, 400.f));

  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::Color_GREEN);
  paint.SetStrokeWidth(10);
  paint.SetStyle(skity::Paint::kStroke_Style);

  canvas->Save();
  canvas->Skew(-0.5, 0);

  canvas->Translate(160.f, 50.f);
  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 150), 20, 20),
      paint);

  canvas->Translate(130, 0);
  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 150), 20, 30),
      paint);

  canvas->Translate(130, 0);

  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 100, 150), 50, 75),
      paint);

  canvas->Restore();

  auto dl = recorder.FinishRecording();
  PathListContext context("draw_stroke_rrect_with_skew.png");
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 600, 400,
                                                   context.ToPathList()));
}

TEST(SimpleShapeGolden, DrawStrokeRRectWithScale) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));

  auto canvas = recorder.GetRecordingCanvas();

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::Color_GREEN);
  paint.SetStrokeWidth(1);
  paint.SetStyle(skity::Paint::kStroke_Style);

  canvas->Save();
  canvas->Scale(10, 10);

  canvas->Translate(0.3f, 5.f);
  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 10, 15), 2, 2),
      paint);

  canvas->Translate(13, 0);
  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 10, 15), 2, 3),
      paint);

  canvas->Translate(13, 0);

  canvas->DrawRRect(
      skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(0, 0, 10, 15), 5, 7.5),
      paint);

  canvas->Restore();

  auto dl = recorder.FinishRecording();
  PathListContext context("draw_stroke_rrect_with_scale.png");

  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 400, 400,
                                                   context.ToPathList()));
}

TEST(SimpleShapeGolden, DrawStrokeRRectBlending) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));

  auto canvas = recorder.GetRecordingCanvas();
  canvas->Clear(skity::Color_WHITE);

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::Color_GREEN);
  paint.SetStrokeWidth(10);
  paint.SetStyle(skity::Paint::kStroke_Style);
  canvas->Save();
  canvas->Translate(50.f, 50.f);
  canvas->DrawRoundRect(skity::Rect::MakeWH(80, 200), 10, 10, paint);

  paint.SetBlendMode(skity::BlendMode::kSrc);
  canvas->Translate(100, 0);
  canvas->DrawRoundRect(skity::Rect::MakeWH(80, 200), 10, 10, paint);

  paint.SetAlphaF(0.5f);
  canvas->Translate(100, 0);
  canvas->DrawRoundRect(skity::Rect::MakeWH(80, 200), 10, 10, paint);

  auto dl = recorder.FinishRecording();
  PathListContext context("draw_stroke_rect_with_blending.png");
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 400, 400,
                                                   context.ToPathList()));
}

TEST(SimpleShapeGolden, DrawAnalyticalBlendModes) {
  constexpr uint32_t kWidth = 360;
  constexpr uint32_t kHeight = 360;
  constexpr std::array<skity::BlendMode, 9> kBlendModes = {
      skity::BlendMode::kClear,   skity::BlendMode::kSrc,
      skity::BlendMode::kSrcOver, skity::BlendMode::kSrcIn,
      skity::BlendMode::kDstIn,   skity::BlendMode::kSrcOut,
      skity::BlendMode::kDstATop, skity::BlendMode::kModulate,
      skity::BlendMode::kOverlay,
  };

  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(kWidth, kHeight));
  auto* canvas = recorder.GetRecordingCanvas();

  canvas->SaveLayer(skity::Rect::MakeWH(kWidth, kHeight), skity::Paint{});
  skity::Path clip_path;
  clip_path.AddRRect(skity::RRect::MakeRectXY(
      skity::Rect::MakeLTRB(4.f, 4.f, kWidth - 4.f, kHeight - 4.f), 16.f,
      16.f));
  canvas->ClipPath(clip_path, skity::Canvas::ClipOp::kIntersect);

  for (size_t index = 0; index < kBlendModes.size(); ++index) {
    float x = static_cast<float>(index % 3) * 120.f;
    float y = static_cast<float>(index / 3) * 120.f;

    skity::Paint destination;
    destination.SetAntiAlias(true);
    destination.SetColor(skity::ColorSetARGB(150, 220, 80, 60));
    canvas->DrawRRect(
        skity::RRect::MakeRectXY(
            skity::Rect::MakeLTRB(x + 8.f, y + 10.f, x + 88.f, y + 105.f), 9.f,
            7.f),
        destination);

    skity::Paint source;
    source.SetAntiAlias(true);
    source.SetBlendMode(kBlendModes[index]);
    source.SetColor(skity::ColorSetARGB(180, 30, 150, 240));
    if (index % 2 != 0) {
      source.SetStyle(skity::Paint::kStroke_Style);
      source.SetStrokeWidth(6.f);
    }
    canvas->Save();
    canvas->Rotate(index % 2 == 0 ? 3.f : -3.f, x + 69.25f, y + 60.5f);
    canvas->DrawRRect(
        skity::RRect::MakeRectXY(skity::Rect::MakeLTRB(x + 30.25f, y + 22.5f,
                                                       x + 108.25f, y + 98.5f),
                                 17.5f, 13.5f),
        source);
    canvas->Restore();
  }
  canvas->Restore();

  auto display_list = recorder.FinishRecording();
  PathListContext context("draw_analytical_blend_modes.png");
  auto validate_strategy = [&](bool enable_coverage_aa, bool enable_contour_aa,
                               bool supports_framebuffer_fetch,
                               bool supports_dual_source_blending,
                               const char* golden_path) {
    SCOPED_TRACE(golden_path);
    SCOPED_TRACE(supports_dual_source_blending ? "dual_source"
                 : supports_framebuffer_fetch  ? "framebuffer_fetch"
                                               : "texture_copy");
    SCOPED_TRACE(enable_coverage_aa ? "coverage_aa" : "contour_aa");
    skity::testing::GoldenTestEnvConfig config;
    config.enable_coverage_aa = enable_coverage_aa;
    config.enable_contour_aa = enable_contour_aa;
    config.supports_framebuffer_fetch = supports_framebuffer_fetch;
    config.supports_dual_source_blending = supports_dual_source_blending;
    config.supports_native_advanced_blend = false;
    config.supports_native_advanced_blend_coherent = false;
    config.sample_count = 1;
    EXPECT_TRUE(skity::testing::CompareGoldenTexture(
        display_list.get(), kWidth, kHeight, golden_path, config));
  };

  const auto contour_path =
      std::filesystem::path(CASE_DIR "contour_aa_images/") /
      "draw_analytical_blend_modes.png";
  bool can_test_framebuffer_fetch = skity::testing::SupportsFramebufferFetch();
  bool can_test_dual_source = skity::testing::SupportsDualSourceBlending();

  // Generate and validate the variant baselines before the common path list.
  // Missing variant images otherwise intentionally fall back to the common
  // image, which prevents SKITY_UPDATE_MISSING_GOLDEN from creating them.
  validate_strategy(true, false, false, false,
                    context.expected_image_coverage_aa_path.c_str());
  validate_strategy(false, true, false, false, contour_path.c_str());

  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      display_list.get(), kWidth, kHeight, context.ToPathList()));

  if (can_test_framebuffer_fetch) {
    validate_strategy(true, false, true, false,
                      context.expected_image_coverage_aa_path.c_str());
    validate_strategy(false, true, true, false, contour_path.c_str());
  }
  if (can_test_dual_source) {
    validate_strategy(true, false, false, true,
                      context.expected_image_coverage_aa_path.c_str());
    validate_strategy(false, true, false, true, contour_path.c_str());
  }
}

TEST(SimpleShapeGolden, DrawSourceOpacityBlendModes) {
  constexpr uint32_t kWidth = 360;
  constexpr uint32_t kHeight = 480;
  constexpr std::array<skity::BlendMode, 6> kBlendModes = {
      skity::BlendMode::kSrc,    skity::BlendMode::kSrcIn,
      skity::BlendMode::kSrcOut, skity::BlendMode::kDstATop,
      skity::BlendMode::kDstIn,  skity::BlendMode::kDstOut,
  };

  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(kWidth, kHeight));
  auto* canvas = recorder.GetRecordingCanvas();

  for (size_t opacity_index = 0; opacity_index < 2; ++opacity_index) {
    for (size_t mode_index = 0; mode_index < kBlendModes.size(); ++mode_index) {
      float x = static_cast<float>(mode_index % 3) * 120.f;
      float y = static_cast<float>(opacity_index * 2 + mode_index / 3) * 120.f;

      skity::Paint destination;
      destination.SetAntiAlias(true);
      destination.SetColor(skity::ColorSetARGB(160, 220, 70, 60));
      canvas->DrawCircle(x + 50.f, y + 60.f, 38.f, destination);

      skity::Paint source;
      source.SetAntiAlias(true);
      source.SetBlendMode(kBlendModes[mode_index]);
      source.SetColor(
          skity::ColorSetARGB(opacity_index == 0 ? 255 : 150, 30, 145, 235));
      canvas->Save();
      canvas->Rotate(mode_index % 2 == 0 ? 7.f : -7.f, x + 72.f, y + 60.f);
      canvas->DrawRRect(
          skity::RRect::MakeRectXY(
              skity::Rect::MakeLTRB(x + 38.f, y + 20.f, x + 108.f, y + 102.f),
              15.f, 11.f),
          source);
      canvas->Restore();
    }
  }

  auto display_list = recorder.FinishRecording();
  std::filesystem::path golden_path = kGoldenTestImageSimpleDir;
  golden_path.append("draw_source_opacity_blend_modes.png");

  skity::testing::GoldenTestEnvConfig coverage_config;
  coverage_config.enable_coverage_aa = true;
  coverage_config.enable_contour_aa = false;
  coverage_config.supports_framebuffer_fetch = false;
  coverage_config.supports_dual_source_blending = false;
  coverage_config.supports_native_advanced_blend = false;
  coverage_config.supports_native_advanced_blend_coherent = false;
  coverage_config.sample_count = 1;
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(display_list.get(), kWidth,
                                                   kHeight, golden_path.c_str(),
                                                   coverage_config));
}

// https://dev.w3.org/SVG/tools/svgweb/samples/svg-files/yinyang.svg
TEST(SimpleShapeGolden, DrawYinAndYang) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 200.f));
  auto canvas = recorder.GetRecordingCanvas();
  auto path_opt = skity::ParsePath::FromSVGString(
      "M50,2a48,48 0 1 1 0,96a24 24 0 1 1 0-48a24 24 0 1 0 0-48");
  ASSERT_TRUE(path_opt.has_value());

  skity::Path temp;
  temp.AddPath(path_opt.value());
  temp.AddCircle(50, 26, 6);
  auto str = skity::ParsePath::ToSVGString(temp);
  auto dst = skity::ParsePath::FromSVGString(str.c_str());
  ASSERT_TRUE(dst.has_value());

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::Color_BLACK);
  canvas->Scale(4, 4);
  canvas->DrawColor(skity::Color_WHITE);
  paint.SetStyle(skity::Paint::Style::kStroke_Style);
  canvas->DrawCircle(50, 50, 48, paint);
  paint.SetStyle(skity::Paint::Style::kFill_Style);
  canvas->DrawPath(dst.value(), paint);
  paint.SetColor(skity::Color_WHITE);
  canvas->DrawCircle(50, 74, 6, paint);
  auto dl = recorder.FinishRecording();
  PathListContext context("draw_yin_and_yang.png");
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 400, 400,
                                                   context.ToPathList()));
}

TEST(SimpleShapeGolden, DrawDRRect) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(240.f, 240.f));
  auto canvas = recorder.GetRecordingCanvas();
  canvas->Clear(skity::Color_WHITE);
  skity::RRect outer = skity::RRect::MakeRect({20, 40, 210, 200});
  skity::RRect inner = skity::RRect::MakeOval({60, 70, 170, 160});
  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::Color_GREEN);
  canvas->DrawDRRect(outer, inner, paint);
  auto dl = recorder.FinishRecording();
  PathListContext context("draw_drrect.png");
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 240, 240,
                                                   context.ToPathList()));
}

TEST(SimpleShapeGolden, DrawDRRect2) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(240.f, 240.f));
  auto canvas = recorder.GetRecordingCanvas();
  canvas->Clear(skity::Color_WHITE);
  skity::RRect outer = skity::RRect::MakeRect({20, 40, 210, 200});
  skity::RRect inner = skity::RRect::MakeRectXY({60, 70, 170, 160}, 10, 10);
  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(skity::Color_GREEN);
  paint.SetStyle(skity::Paint::kStroke_Style);
  paint.SetStrokeWidth(20);
  paint.SetStrokeJoin(skity::Paint::Join::kRound_Join);
  canvas->DrawDRRect(outer, inner, paint);
  paint.SetStrokeWidth(3);
  paint.SetColor(skity::Color_WHITE);
  canvas->DrawDRRect(outer, inner, paint);

  auto dl = recorder.FinishRecording();
  PathListContext context("draw_drrect2.png");
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 240, 240,
                                                   context.ToPathList()));
}
