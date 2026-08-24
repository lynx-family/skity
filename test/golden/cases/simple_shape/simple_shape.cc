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
