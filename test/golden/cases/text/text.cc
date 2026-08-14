// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <skity/effect/color_filter.hpp>
#include <skity/graphic/color.hpp>
#include <skity/graphic/tile_mode.hpp>
#include <skity/recorder/picture_recorder.hpp>
#include <skity/text/font.hpp>
#include <skity/text/font_arguments.hpp>
#include <skity/text/font_descriptor.hpp>
#include <skity/text/font_manager.hpp>
#include <skity/text/font_metrics.hpp>
#include <skity/text/font_style.hpp>
#include <skity/text/text_blob.hpp>
#include <skity/text/text_run.hpp>
#include <skity/text/typeface.hpp>
#include <skity/text/utf.hpp>

#include "common/golden_test_check.hpp"

static const char* kGoldenTestImageDir = CASE_DIR;

static void DrawCoverageBlendedText(skity::Canvas* canvas) {
  canvas->Clear(skity::ColorSetARGB(255, 35, 70, 105));

  auto typeface = skity::Typeface::GetDefaultTypeface();
  skity::Font font(typeface, 72.f);
  const skity::GlyphID glyph = typeface->UnicharToGlyph('A');
  const skity::GlyphID glyphs[] = {glyph, glyph, glyph};
  const float positions_x[] = {24.f, 38.f, 150.f};
  const float positions_y[] = {90.f, 90.f, 90.f};

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetFillColor(0.95f, 0.15f, 0.1f, 0.72f);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetBlendMode(skity::BlendMode::kSrc);
  canvas->DrawGlyphs(3, glyphs, positions_x, positions_y, font, paint);

  const skity::Point gradient_points[] = {
      {20.f, 0.f, 0.f, 1.f},
      {230.f, 0.f, 0.f, 1.f},
  };
  const skity::Vec4 gradient_colors[] = {
      {1.f, 0.8f, 0.1f, 0.85f},
      {0.2f, 0.8f, 1.f, 0.5f},
  };
  paint.SetTextSize(44.f);
  paint.SetTypeface(typeface);
  paint.SetShader(
      skity::Shader::MakeLinear(gradient_points, gradient_colors, nullptr, 2));
  paint.SetColorFilter(skity::ColorFilters::Blend(
      skity::ColorSetARGB(210, 90, 235, 150), skity::BlendMode::kSrcATop));
  canvas->DrawSimpleText("Coverage", 20.f, 180.f, paint);
}

static void DrawOpaqueCoverageBlendModes(skity::Canvas* canvas) {
  constexpr std::array<skity::BlendMode, 6> kBlendModes = {
      skity::BlendMode::kSrc,    skity::BlendMode::kSrcIn,
      skity::BlendMode::kSrcOut, skity::BlendMode::kDstATop,
      skity::BlendMode::kDstIn,  skity::BlendMode::kDstOut,
  };

  auto typeface = skity::Typeface::GetDefaultTypeface();
  skity::Font font(typeface, 72.f);
  const skity::GlyphID glyph = typeface->UnicharToGlyph('A');

  for (size_t index = 0; index < kBlendModes.size(); ++index) {
    float x = static_cast<float>(index % 3) * 120.f;
    float y = static_cast<float>(index / 3) * 110.f;

    skity::Paint destination;
    destination.SetAntiAlias(true);
    destination.SetColor(skity::ColorSetARGB(160, 220, 70, 60));
    canvas->DrawCircle(x + 58.f, y + 57.f, 40.f, destination);

    skity::Paint source;
    source.SetAntiAlias(true);
    source.SetStyle(skity::Paint::kFill_Style);
    source.SetColor(skity::ColorSetARGB(255, 30, 145, 235));
    source.SetBlendMode(kBlendModes[index]);
    const float position_x = x + 34.f;
    const float position_y = y + 83.f;
    canvas->DrawGlyphs(1, &glyph, &position_x, &position_y, font, source);
  }
}

static skity::testing::GoldenTestEnvConfig TextBlendConfig(
    bool force_texture_copy) {
  skity::testing::GoldenTestEnvConfig config;
  config.sample_count = 1;
  if (force_texture_copy) {
    config.supports_framebuffer_fetch = false;
    config.supports_native_advanced_blend = false;
    config.supports_native_advanced_blend_coherent = false;
    config.supports_dual_source_blending = false;
  }
  return config;
}

TEST(TextGolden, Basic) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));
  auto canvas = recorder.GetRecordingCanvas();
  canvas->Save();

  auto typeface = skity::Typeface::GetDefaultTypeface();

  // TODO(jingle): Add more test cases
  skity::Paint paint;
  paint.SetTextSize(64.f);
  paint.SetAntiAlias(true);
  paint.SetFillColor(1.f, 0.f, 0.f, 1.f);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetTypeface(typeface);

  canvas->DrawSimpleText("SKITY skity", 20.f, 50.f, paint);

  auto typeface_cjk =
      skity::FontManager::RefDefault()->MatchFamilyStyleCharacter(
          nullptr, skity::FontStyle(), nullptr, 0, 0x95E8);
  paint.SetTypeface(typeface_cjk);
  canvas->DrawSimpleText("你好", 20.f, 150.f, paint);

  std::filesystem::path golden_path = kGoldenTestImageDir;
  golden_path.append("text_basic.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 400.f, 400.f,
                                                   golden_path.c_str()));
}

TEST(TextGolden, TextLinearGradientFlags) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(400.f, 400.f));
  auto canvas = recorder.GetRecordingCanvas();

  canvas->Clear(skity::Color_WHITE);

  auto typeface = skity::Typeface::GetDefaultTypeface();

  skity::Paint paint;
  paint.SetTextSize(64.f);
  paint.SetAntiAlias(true);

  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetTypeface(typeface);
  canvas->Save();
  for (uint32_t i = 0; i < 2; i++) {
    canvas->Translate(0, 200 * i);
    skity::Vec4 gradient_colors[] = {
        skity::Vec4{0.9019f, 0.3921f, 0.3960f, 1.0f},
        skity::Vec4{0.0f, 0.0f, 0.0f, 0.0f}};
    float gradient_positions[] = {0.75f, 1.f};
    std::vector<skity::Point> gradient_points = {
        skity::Point{0.f, 0.f, 0.f, 1.f},
        skity::Point{20.f, 0.f, 0.f, 1.f},
    };
    auto flags = i;
    auto lgs = skity::Shader::MakeLinear(gradient_points.data(),
                                         gradient_colors, gradient_positions, 2,
                                         skity::TileMode::kMirror, flags);

    paint.SetShader(lgs);
    canvas->DrawSimpleText("SKITY skity", 20.f, 50.f, paint);

    auto typeface_cjk =
        skity::FontManager::RefDefault()->MatchFamilyStyleCharacter(
            nullptr, skity::FontStyle(), nullptr, 0, 0x95E8);
    paint.SetTypeface(typeface_cjk);
    canvas->DrawSimpleText("你好", 20.f, 150.f, paint);
  }

  std::filesystem::path golden_path = kGoldenTestImageDir;
  golden_path.append("text_linear_gradient_flags.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 400.f, 400.f,
                                                   golden_path.c_str()));
}

TEST(TextGolden, CoverageBlendSrc) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(260.f, 210.f));
  DrawCoverageBlendedText(recorder.GetRecordingCanvas());

  std::filesystem::path golden_path = kGoldenTestImageDir;
  golden_path.append("text_coverage_blend_src.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 260.f, 210.f, golden_path.c_str(), TextBlendConfig(false)));
}

TEST(TextGolden, CoverageBlendSrcTextureCopy) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(260.f, 210.f));
  DrawCoverageBlendedText(recorder.GetRecordingCanvas());

  std::filesystem::path golden_path = kGoldenTestImageDir;
  golden_path.append("text_coverage_blend_src.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 260.f, 210.f, golden_path.c_str(), TextBlendConfig(true)));
}

TEST(TextGolden, CoverageBlendSrcFramebufferFetch) {
  if (!skity::testing::SupportsFramebufferFetch()) {
    GTEST_SKIP() << "Framebuffer fetch is unavailable";
  }

  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(260.f, 210.f));
  DrawCoverageBlendedText(recorder.GetRecordingCanvas());

  std::filesystem::path golden_path = kGoldenTestImageDir;
  golden_path.append("text_coverage_blend_src.png");
  auto dl = recorder.FinishRecording();
  auto config = TextBlendConfig(false);
  config.supports_framebuffer_fetch = true;
  config.supports_dual_source_blending = false;
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 260.f, 210.f, golden_path.c_str(), config));
}

TEST(TextGolden, OpaqueCoverageBlendModes) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(360.f, 220.f));
  DrawOpaqueCoverageBlendModes(recorder.GetRecordingCanvas());

  std::filesystem::path golden_path = kGoldenTestImageDir;
  golden_path.append("text_opaque_coverage_blend_modes.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), 360.f, 220.f, golden_path.c_str(), TextBlendConfig(true)));
}
