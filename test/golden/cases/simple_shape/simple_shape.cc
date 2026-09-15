// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <skity/io/parse_path.hpp>
#include <skity/recorder/picture_recorder.hpp>
#include <skity/skity.hpp>
#include <string>

#include "common/golden_test_check.hpp"
#include "common/golden_test_env.hpp"

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
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 400, 400,
      {.cpu_tess_path = context.expected_image_cpu_tess_path.c_str(),
       .gpu_tess_path = context.expected_image_gpu_tess_path.c_str()}));

  skity::testing::GoldenTestEnvConfig config;
  config.sample_count = 1;

  std::filesystem::path contour_path(CASE_DIR "contour_aa_images/");
  contour_path.append("draw_stroke_rect_with_blending.png");
  config.enable_contour_aa = true;
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 400, 400, contour_path.c_str(), config));

  config.enable_contour_aa = false;
  config.enable_simple_shape_pipeline = true;
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 400, 400, context.expected_image_simple_path.c_str(), config));

  config.enable_simple_shape_pipeline = false;
  config.enable_coverage_aa = true;
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 400, 400, context.expected_image_coverage_aa_path.c_str(),
      config));
}

TEST(SimpleShapeGolden, CoverageAwareBlendModes) {
  constexpr size_t kColumns = 5;
  constexpr std::array<skity::BlendMode, 16> kBlendModes = {
      skity::BlendMode::kClear,   skity::BlendMode::kSrc,
      skity::BlendMode::kDst,     skity::BlendMode::kSrcOver,
      skity::BlendMode::kDstOver, skity::BlendMode::kSrcIn,
      skity::BlendMode::kDstIn,   skity::BlendMode::kSrcOut,
      skity::BlendMode::kDstOut,  skity::BlendMode::kSrcATop,
      skity::BlendMode::kDstATop, skity::BlendMode::kXor,
      skity::BlendMode::kPlus,    skity::BlendMode::kModulate,
      skity::BlendMode::kScreen,  skity::BlendMode::kOverlay,
  };
  constexpr size_t kRows = (kBlendModes.size() + kColumns - 1) / kColumns;
  constexpr uint32_t kWidth = kColumns * 100;
  constexpr uint32_t kHeight = kRows * 2 * 100;

  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(kWidth, kHeight));
  auto* canvas = recorder.GetRecordingCanvas();

  for (size_t opacity = 0; opacity < 2; ++opacity) {
    for (size_t index = 0; index < kBlendModes.size(); ++index) {
      float x = static_cast<float>(index % kColumns) * 100.f;
      float y = static_cast<float>(opacity * kRows + index / kColumns) * 100.f;

      skity::Paint destination;
      destination.SetAntiAlias(true);
      destination.SetColor(skity::ColorSetARGB(170, 220, 75, 55));
      canvas->DrawCircle(x + 43.f, y + 50.f, 34.f, destination);

      skity::Paint source;
      source.SetAntiAlias(true);
      source.SetBlendMode(kBlendModes[index]);
      source.SetColor(
          skity::ColorSetARGB(opacity == 0 ? 255 : 155, 35, 145, 235));
      if (index % 2 != 0) {
        source.SetStyle(skity::Paint::kStroke_Style);
        source.SetStrokeWidth(6.f);
      }
      canvas->Save();
      canvas->Rotate(index % 2 == 0 ? 6.f : -6.f, x + 60.f, y + 50.f);
      canvas->DrawRRect(
          skity::RRect::MakeRectXY(
              skity::Rect::MakeLTRB(x + 28.f, y + 17.f, x + 91.f, y + 83.f),
              13.f, 10.f),
          source);
      canvas->Restore();
    }
  }

  auto display_list = recorder.FinishRecording();
  PathListContext context("coverage_aware_blend_modes.png");
  auto contour_path = std::filesystem::path(CASE_DIR "contour_aa_images/") /
                      "coverage_aware_blend_modes.png";

  auto validate = [&](bool coverage_aa, bool contour_aa, bool simple_shape,
                      bool dual_source, const char* path) {
    skity::testing::GoldenTestEnvConfig config;
    config.enable_coverage_aa = coverage_aa;
    config.enable_contour_aa = contour_aa;
    config.enable_simple_shape_pipeline = simple_shape;
    config.supports_framebuffer_fetch = false;
    config.supports_native_advanced_blend = false;
    config.supports_native_advanced_blend_coherent = false;
    config.supports_dual_source_blending = dual_source;
    config.sample_count = 1;
    EXPECT_TRUE(skity::testing::CompareGoldenTexture(display_list.get(), kWidth,
                                                     kHeight, path, config));
  };

  validate(true, false, false, false,
           context.expected_image_coverage_aa_path.c_str());
  validate(false, true, false, false, contour_path.c_str());
  validate(false, false, true, false,
           context.expected_image_simple_path.c_str());

  if (skity::testing::SupportsDualSourceBlending()) {
    validate(true, false, false, true,
             context.expected_image_coverage_aa_path.c_str());
    validate(false, true, false, true, contour_path.c_str());
    validate(false, false, true, true,
             context.expected_image_simple_path.c_str());
  }
}

TEST(SimpleShapeGolden, PlusWithPartialCoverage) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(200.f, 200.f));

  auto canvas = recorder.GetRecordingCanvas();
  canvas->Clear(0xFFCCCCCC);

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(0xFFCCCC00);
  paint.SetBlendMode(skity::BlendMode::kPlus);
  paint.SetStrokeWidth(20.f);
  paint.SetStyle(skity::Paint::kStroke_Style);
  canvas->DrawRoundRect(skity::Rect::MakeLTRB(40.f, 40.f, 160.f, 160.f), 20.f,
                        20.f, paint);

  auto dl = recorder.FinishRecording();
  PathListContext context("plus_with_partial_coverage.png");
  skity::testing::GoldenTestEnvConfig config;
  config.sample_count = 1;

  std::filesystem::path contour_path(CASE_DIR "contour_aa_images/");
  contour_path.append("plus_with_partial_coverage.png");
  config.enable_contour_aa = true;
  config.use_backend_specific_golden = true;
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 200, 200, contour_path.c_str(), config));

  config.enable_contour_aa = false;
  config.use_backend_specific_golden = false;
  config.enable_coverage_aa = true;
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 200, 200, context.expected_image_coverage_aa_path.c_str(),
      config));

  config.enable_coverage_aa = false;
  config.enable_simple_shape_pipeline = true;
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 200, 200, context.expected_image_simple_path.c_str(), config));
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

TEST(SimpleShapeGolden, ThinStrokeDeviceWidthAndCoverage) {
  struct StrokeCase {
    float width;
    float scale_x;
    float scale_y;
    float device_width_x;
    float device_width_y;
  };
  const StrokeCase cases[] = {
      {0.25f, 8.f, 8.f, 2.f, 2.f},     {0.25f, 2.f, 2.f, 0.5f, 0.5f},
      {0.75f, 1.f, 1.f, 0.75f, 0.75f}, {0.01f, 8.f, 8.f, 0.5f, 0.5f},
      {0.f, 8.f, 8.f, 0.5f, 0.5f},     {0.25f, 8.f, 2.f, 2.f, 0.5f},
      {0.25f, -8.f, 2.f, 2.f, 0.5f},
  };
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  auto* gpu = env->GetGPUContext();
  const bool was_enabled = gpu->IsEnableSimpleShapePipeline();
  const auto sample_count = env->GetSampleCount();
  for (const auto& stroke : cases) {
    for (int shape = 0; shape < 3; ++shape) {
      for (auto samples : {1u, 4u}) {
        for (float phase : {0.f, 0.5f}) {
          SCOPED_TRACE(::testing::Message()
                       << "width=" << stroke.width
                       << " scale=" << stroke.scale_x << "," << stroke.scale_y
                       << " shape=" << shape << " samples=" << samples
                       << " phase=" << phase);
          gpu->SetEnableSimpleShapePipeline(true);
          env->SetSampleCount(samples);
          auto texture =
              env->RenderToTexture(128, 128, [&](skity::Canvas* canvas) {
                canvas->Clear(skity::Color_TRANSPARENT);
                canvas->Translate(80.f + phase, 16.f + phase);
                canvas->Scale(stroke.scale_x, stroke.scale_y);
                skity::Paint paint;
                paint.SetColor(skity::Color_WHITE);
                paint.SetAntiAlias(true);
                paint.SetStyle(skity::Paint::kStroke_Style);
                paint.SetStrokeWidth(stroke.width);
                auto rect = skity::Rect::MakeWH(8.f, 8.f);
                if (shape == 0) {
                  skity::Path source;
                  source.AddRect(rect);
                  canvas->DrawPath(source, paint);
                } else if (shape == 1) {
                  skity::Path path;
                  path.AddRRect(skity::RRect::MakeRectXY(rect, 2.f, 2.f));
                  canvas->DrawPath(path, paint);
                } else {
                  canvas->DrawRRect(skity::RRect::MakeRectXY(rect, 2.f, 2.f),
                                    paint);
                }
              });
          gpu->SetEnableSimpleShapePipeline(was_enabled);
          env->SetSampleCount(sample_count);
          ASSERT_NE(texture, nullptr);
          auto pixels = texture->ReadPixels();
          ASSERT_NE(pixels, nullptr);
          // Away from corners, coverage is the exact overlap of the stroke
          // band and a unit pixel. Check both edges under nonuniform scale.
          for (int offset = -5; offset <= 5; ++offset) {
            for (int axis = 0; axis < 2; ++axis) {
              const float width =
                  axis == 0 ? stroke.device_width_x : stroke.device_width_y;
              const float overlap =
                  std::max(0.f, std::min(offset + 1.f - phase, width * 0.5f) -
                                    std::max(offset - phase, -width * 0.5f));
              const int x = axis == 0
                                ? 80 + offset
                                : 80 + static_cast<int>(4 * stroke.scale_x);
              const int y = axis == 1
                                ? 16 + offset
                                : 16 + static_cast<int>(4 * stroke.scale_y);
              EXPECT_NEAR(pixels->Addr8(x, y)[3], overlap * 255.f, 1.f)
                  << "axis=" << axis << " offset=" << offset;
            }
          }
        }
      }
    }
  }
}
TEST(SimpleShapeGolden, RotatedRRectAAOutset) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  auto* gpu = env->GetGPUContext();
  const bool simple = gpu->IsEnableSimpleShapePipeline();
  const auto samples = env->GetSampleCount();
  gpu->SetEnableSimpleShapePipeline(true);
  env->SetSampleCount(1);
  auto texture = env->RenderToTexture(128, 128, [](skity::Canvas* canvas) {
    canvas->Clear(skity::Color_TRANSPARENT);
    canvas->Translate(64.2f, 64.6f);
    canvas->Rotate(45.f);
    skity::Path path;
    path.AddRoundRect(skity::Rect::MakeLTRB(-20, -20, 20, 20), 2, 2);
    skity::Paint paint;
    paint.SetAntiAlias(true);
    paint.SetColor(skity::Color_WHITE);
    canvas->DrawPath(path, paint);
  });
  gpu->SetEnableSimpleShapePipeline(simple);
  env->SetSampleCount(samples);
  ASSERT_NE(texture, nullptr);
  auto pixels = texture->ReadPixels();
  ASSERT_NE(pixels, nullptr);
  // This point lies on the straight bottom edge, well away from the corners.
  // Apply the pipeline's specified one-device-pixel linear AA ramp.
  const float distance =
      ((79.5f - 64.6f) - (50.5f - 64.2f)) / std::sqrt(2.f) - 20.f;
  const float expected = (0.5f - distance) * 255.f;
  RecordProperty("alpha", static_cast<int>(pixels->Addr8(50, 79)[3]));
  RecordProperty("expected_alpha", std::to_string(expected));
  EXPECT_NEAR(pixels->Addr8(50, 79)[3], expected, 1.f);
}

TEST(SimpleShapeGolden, AffineRRectGradientCoverage) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  auto* gpu = env->GetGPUContext();
  const bool simple = gpu->IsEnableSimpleShapePipeline();
  const auto samples = env->GetSampleCount();
  gpu->SetEnableSimpleShapePipeline(true);
  env->SetSampleCount(1);
  auto texture = env->RenderToTexture(128, 128, [](skity::Canvas* canvas) {
    canvas->Clear(skity::Color_TRANSPARENT);
    canvas->Translate(64.f, 64.f);
    canvas->Rotate(45.f);
    canvas->Scale(2.f, 0.5f);
    skity::Path path;
    path.AddRoundRect(skity::Rect::MakeLTRB(-20, -20, 20, 20), 2, 2);
    skity::Paint paint;
    paint.SetAntiAlias(true);
    paint.SetColor(skity::Color_WHITE);
    canvas->DrawPath(path, paint);
  });
  gpu->SetEnableSimpleShapePipeline(simple);
  env->SetSampleCount(samples);
  ASSERT_NE(texture, nullptr);
  auto pixels = texture->ReadPixels();
  ASSERT_NE(pixels, nullptr);
  // This point lies on the straight right edge, well away from the corners.
  // Apply the pipeline's specified one-device-pixel linear AA ramp.
  const float distance =
      ((92.5f - 64.f) + (92.5f - 64.f)) / std::sqrt(2.f) - 40.f;
  const float expected = (0.5f - distance) * 255.f;
  RecordProperty("alpha", static_cast<int>(pixels->Addr8(92, 92)[3]));
  RecordProperty("expected_alpha", std::to_string(expected));
  EXPECT_NEAR(pixels->Addr8(92, 92)[3], expected, 1.f);
}

TEST(SimpleShapeGolden, AffineRRectCoverageMatchesImplicitReference) {
  // Matrices are written in mathematical row order, independently of the
  // column-major shader instance data. Include rotation, reflection and shear.
  // Keep raster vertices on a 1/16-device-pixel grid: SwiftShader rounds
  // vertex positions to four subpixel bits before interpolating varyings.
  // Dyadic transforms, translation, radii and stroke outsets avoid testing
  // that backend-specific rounding against an unquantized CPU reference.
  // The two 45-degree transforms also have uniform scale sqrt(2).
  const std::array<double, 4> transforms[] = {
      {1, 0, 0, 1},        {1, -1, 1, 1},  {1, 1, -1, 1},   {2, -0.5, 2, 0.5},
      {-2, -0.5, -2, 0.5}, {1, 0.5, 0, 1}, {1, -0.5, 0, 1}, {0, -1, 1, 0},
  };
  constexpr float kTranslateX = 64.25f;
  constexpr float kTranslateY = 64.5f;
  constexpr double kHalfSize = 20;
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  auto* gpu = env->GetGPUContext();
  const bool was_enabled = gpu->IsEnableSimpleShapePipeline();
  const auto sample_count = env->GetSampleCount();
  for (const auto& values : transforms) {
    skity::Matrix transform;
    transform.SetScaleX(values[0]);
    transform.SetSkewX(values[1]);
    transform.SetSkewY(values[2]);
    transform.SetScaleY(values[3]);
    transform.SetTranslateX(kTranslateX);
    transform.SetTranslateY(kTranslateY);
    // Use the float coefficients actually sent to the renderer.
    const double a = transform.GetScaleX(), b = transform.GetSkewX();
    const double c = transform.GetSkewY(), d = transform.GetScaleY();
    const double det = a * d - b * c;
    auto to_local = [&](double x, double y) {
      x -= kTranslateX;
      y -= kTranslateY;
      return std::array<double, 2>{(d * x - b * y) / det,
                                   (a * y - c * x) / det};
    };
    // Perpendicular distances to the transformed straight edges follow from
    // the area of a parallelogram divided by its edge length.
    const double edge_scale_x = std::abs(det) / std::hypot(b, d);
    const double edge_scale_y = std::abs(det) / std::hypot(a, c);
    for (float radius : {0.25f, 2.f, 8.f}) {
      for (float width : {0.f, 0.75f, 3.f}) {
        // Collapsed inner corners are a separate, existing stroke limitation.
        if (width >= 2 * radius) continue;
        for (bool draw_path : {false, true}) {
          SCOPED_TRACE(::testing::Message()
                       << "matrix=" << a << "," << b << "," << c << "," << d
                       << " radius=" << radius << " width=" << width
                       << " path=" << draw_path);
          gpu->SetEnableSimpleShapePipeline(true);
          env->SetSampleCount(1);
          auto texture =
              env->RenderToTexture(128, 128, [&](skity::Canvas* canvas) {
                canvas->Clear(skity::Color_TRANSPARENT);
                canvas->Concat(transform);
                skity::Paint paint;
                paint.SetAntiAlias(true);
                paint.SetColor(skity::Color_WHITE);
                if (width > 0) {
                  paint.SetStyle(skity::Paint::kStroke_Style);
                  paint.SetStrokeWidth(width);
                }
                auto rrect = skity::RRect::MakeRectXY(
                    skity::Rect::MakeLTRB(-20, -20, 20, 20), radius, radius);
                if (draw_path) {
                  skity::Path path;
                  path.AddRRect(rrect);
                  canvas->DrawPath(path, paint);
                } else {
                  canvas->DrawRRect(rrect, paint);
                }
              });
          gpu->SetEnableSimpleShapePipeline(was_enabled);
          env->SetSampleCount(sample_count);
          ASSERT_NE(texture, nullptr);
          auto pixels = texture->ReadPixels();
          ASSERT_NE(pixels, nullptr);
          double max_error = 0;
          int worst_x = 0, worst_y = 0;
          int fractional_pixels = 0;
          for (int y = 0; y < 128; ++y) {
            for (int x = 0; x < 128; ++x) {
              const auto local = to_local(x + 0.5, y + 0.5);
              const double core = kHalfSize - radius;
              auto distance = [&](double outset) {
                double edge = std::max(
                    (std::abs(local[0]) - kHalfSize - outset) * edge_scale_x,
                    (std::abs(local[1]) - kHalfSize - outset) * edge_scale_y);
                if (std::abs(local[0]) > core && std::abs(local[1]) > core) {
                  // Evaluate the circle equation in device coordinates and
                  // differentiate numerically. This exercises the fragment
                  // gradient without reproducing its matrix multiplication.
                  auto f = [&](double px, double py) {
                    const auto p = to_local(px, py);
                    const double u = std::abs(p[0]) - core;
                    const double v = std::abs(p[1]) - core;
                    return u * u + v * v -
                           (radius + outset) * (radius + outset);
                  };
                  constexpr double h = 0.001;
                  const double px = x + 0.5, py = y + 0.5;
                  const double dx = (f(px + h, py) - f(px - h, py)) / (2 * h);
                  const double dy = (f(px, py + h) - f(px, py - h)) / (2 * h);
                  edge = std::max(edge, f(px, py) / std::hypot(dx, dy));
                }
                return edge;
              };
              // Match the specified linear AA model, not exact pixel-area
              // integration or a second render through the same shader.
              const double outer =
                  std::clamp(0.5 - distance(width / 2), 0., 1.);
              const double inner =
                  width > 0 ? std::clamp(0.5 - distance(-width / 2), 0., 1.)
                            : 0.;
              const double expected = (outer - inner) * 255;
              if (expected > 1 && expected < 254) ++fractional_pixels;
              // Compare quantized alpha, allowing one UNORM8 step for GPU
              // arithmetic and coverage conversion.
              const double error =
                  std::abs(pixels->Addr8(x, y)[3] - std::round(expected));
              if (error > max_error) {
                max_error = error;
                worst_x = x;
                worst_y = y;
              }
            }
          }
          EXPECT_GT(fractional_pixels, 20);
          EXPECT_LE(max_error, 1.) << "pixel=" << worst_x << "," << worst_y;
        }
      }
    }
  }
}

TEST(SimpleShapeGolden, RRectRegionBoundaryDoesNotBecomeOpaque) {
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  auto* gpu = env->GetGPUContext();
  const bool was_enabled = gpu->IsEnableSimpleShapePipeline();
  const auto sample_count = env->GetSampleCount();
  for (float origin : {0.f, 4096.f, -4096.f, 65536.f, -65536.f}) {
    for (bool quarter_turn : {false, true}) {
      SCOPED_TRACE(::testing::Message()
                   << "origin=" << origin << " quarter_turn=" << quarter_turn);
      gpu->SetEnableSimpleShapePipeline(true);
      env->SetSampleCount(1);
      auto texture = env->RenderToTexture(128, 128, [&](skity::Canvas* canvas) {
        canvas->Clear(skity::Color_TRANSPARENT);
        canvas->Translate(64.25f, 64.5f);
        if (quarter_turn) {
          skity::Matrix rotation = skity::Matrix::Scale(0.f, 0.f);
          rotation.SetSkewX(-1.f);
          rotation.SetSkewY(1.f);
          canvas->Concat(rotation);
        }
        canvas->Translate(-origin, -origin);
        skity::Paint paint;
        paint.SetColor(skity::Color_WHITE);
        paint.SetAntiAlias(true);
        canvas->DrawRRect(skity::RRect::MakeRectXY(
                              skity::Rect::MakeLTRB(origin - 20, origin - 20,
                                                    origin + 20, origin + 20),
                              2, 2),
                          paint);
      });
      gpu->SetEnableSimpleShapePipeline(was_enabled);
      env->SetSampleCount(sample_count);
      ASSERT_NE(texture, nullptr);
      auto pixels = texture->ReadPixels();
      ASSERT_NE(pixels, nullptr);
      // On the y=82.5 corner/edge boundary, a nominally zero region varying can
      // interpolate slightly positive. It must not trigger the opaque shortcut.
      EXPECT_EQ(pixels->Addr8(43, 82)[3], 0);
      EXPECT_NEAR(pixels->Addr8(44, 82)[3], 0.75f * 255.f, 1.f);
      EXPECT_EQ(pixels->Addr8(64, 64)[3], 255);
    }
  }
}

// A 45-degree rotation used to cancel one component of the local AA outset,
// clipping coverage on two opposite edges despite anti-aliasing being enabled.
TEST(SimpleShapeGolden, FilledRRect45DegreeAAEdges) {
  skity::testing::GoldenTestEnvConfig config;
  config.enable_simple_shape_pipeline = true;
  config.sample_count = 1;
  config.require_exact_pixel_match = true;
  config.use_backend_specific_golden = true;
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      128, 128, CASE_DIR "filled_rrect_45_degree_aa_edges.png", config,
      [](skity::Canvas* canvas) {
        canvas->Clear(skity::Color_WHITE);
        canvas->Translate(64.2f, 64.6f);
        canvas->Rotate(45.f);
        skity::Paint paint;
        paint.SetAntiAlias(true);
        paint.SetColor(skity::Color_BLACK);
        canvas->DrawRRect(skity::RRect::MakeRectXY(
                              skity::Rect::MakeLTRB(-20, -20, 20, 20), 2, 2),
                          paint);
      }));
}
