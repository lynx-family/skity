// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <skity/effect/color_filter.hpp>
#include <skity/effect/shader.hpp>
#include <skity/graphic/bitmap.hpp>
#include <skity/graphic/color.hpp>
#include <skity/graphic/sampling_options.hpp>
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
#include <string>
#include <utility>
#include <vector>

#include "common/golden_test_check.hpp"

static const char* kGoldenTestImageDir = CASE_DIR;
static const char* kRobotoRegular =
    SKITY_FONT_DIR "fonts/resources/Roboto-Regular.ttf";

static std::shared_ptr<skity::TextBlob> MakeSubpixelTextBlob(
    const char* text, const std::shared_ptr<skity::Typeface>& typeface,
    float text_size) {
  if (!text || !typeface) {
    return nullptr;
  }

  std::string utf8{text};
  std::vector<skity::Unichar> code_points;
  if (!skity::UTF::UTF8ToCodePoint(utf8.data(), utf8.size(), code_points) ||
      code_points.empty()) {
    return nullptr;
  }

  std::vector<skity::GlyphID> glyphs(code_points.size());
  typeface->UnicharsToGlyphs(code_points.data(), code_points.size(),
                             glyphs.data());

  skity::Font font{typeface, text_size};
  font.SetSubpixel(true);
  std::vector<skity::TextRun> runs;
  runs.emplace_back(font, std::move(glyphs));
  return std::make_shared<skity::TextBlob>(std::move(runs));
}

static std::shared_ptr<skity::TextBlob> MakeOverlappingTextBlob(
    const std::shared_ptr<skity::Typeface>& typeface, float text_size) {
  if (!typeface) {
    return nullptr;
  }

  const skity::GlyphID glyph = typeface->UnicharToGlyph('O');
  if (glyph == 0) {
    return nullptr;
  }

  skity::Font font{typeface, text_size};
  font.SetSubpixel(true);
  std::vector<skity::TextRun> runs;
  runs.emplace_back(font, std::vector<skity::GlyphID>(4, glyph),
                    std::vector<float>{0.f, 36.f, 72.f, 108.f},
                    std::vector<float>(4, 0.f));
  return std::make_shared<skity::TextBlob>(std::move(runs));
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

  auto latin_text = MakeSubpixelTextBlob("SKITY skity", typeface, 64.f);
  ASSERT_NE(latin_text, nullptr);
  canvas->DrawTextBlob(latin_text.get(), 20.f, 50.f, paint);

  auto typeface_cjk =
      skity::FontManager::RefDefault()->MatchFamilyStyleCharacter(
          nullptr, skity::FontStyle(), nullptr, 0, 0x95E8);
  paint.SetTypeface(typeface_cjk);
  auto cjk_text = MakeSubpixelTextBlob("你好", typeface_cjk, 64.f);
  ASSERT_NE(cjk_text, nullptr);
  canvas->DrawTextBlob(cjk_text.get(), 20.f, 150.f, paint);

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
  auto typeface_cjk =
      skity::FontManager::RefDefault()->MatchFamilyStyleCharacter(
          nullptr, skity::FontStyle(), nullptr, 0, 0x95E8);

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
    auto latin_text =
        MakeSubpixelTextBlob("SKITY skity", paint.GetTypeface(), 64.f);
    ASSERT_NE(latin_text, nullptr);
    canvas->DrawTextBlob(latin_text.get(), 20.f, 50.f, paint);

    paint.SetTypeface(typeface_cjk);
    auto cjk_text = MakeSubpixelTextBlob("你好", typeface_cjk, 64.f);
    ASSERT_NE(cjk_text, nullptr);
    canvas->DrawTextBlob(cjk_text.get(), 20.f, 150.f, paint);
  }

  std::filesystem::path golden_path = kGoldenTestImageDir;
  golden_path.append("text_linear_gradient_flags.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), 400.f, 400.f,
                                                   golden_path.c_str()));
}

TEST(TextGolden, SaveLayerPreservesRasterScale) {
  constexpr float kWidth = 400.f;
  constexpr float kHeight = 400.f;
  constexpr float kScale = 4.f;

  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(kWidth, kHeight));
  auto* canvas = recorder.GetRecordingCanvas();
  canvas->Clear(skity::Color_WHITE);

  auto typeface = skity::Typeface::MakeFromFile(kRobotoRegular);
  ASSERT_NE(typeface, nullptr);
  auto text = MakeSubpixelTextBlob("Skity", typeface, 20.f);
  ASSERT_NE(text, nullptr);

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetFillColor(skity::Color_BLACK);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetTypeface(typeface);

  canvas->Scale(kScale, kScale);
  canvas->DrawTextBlob(text.get(), 8.f, 38.f, paint);

  canvas->SaveLayer(skity::Rect::MakeLTRB(2.f, 52.f, 98.f, 98.f),
                    skity::Paint{});
  canvas->DrawTextBlob(text.get(), 8.f, 88.f, paint);
  canvas->Restore();

  std::filesystem::path golden_path = kGoldenTestImageDir;
  golden_path.append("text_save_layer_raster_scale.png");
  auto dl = recorder.FinishRecording();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(dl.get(), kWidth, kHeight,
                                                   golden_path.c_str()));
}

TEST(TextGolden, CoverageAwareBlending) {
  constexpr float kWidth = 480.f;
  constexpr float kHeight = 360.f;
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(kWidth, kHeight));
  auto* canvas = recorder.GetRecordingCanvas();
  canvas->Clear(skity::Color_WHITE);

  skity::Paint background;
  background.SetStyle(skity::Paint::kFill_Style);
  const skity::Color row_colors[] = {0xFF243B53, 0xFF7B2C3B, 0xFF355C3A,
                                     0xFF314A75};
  for (size_t row = 0; row < 4; ++row) {
    background.SetFillColor(row_colors[row]);
    canvas->DrawRect(
        skity::Rect::MakeLTRB(0.f, 90.f * row, kWidth, 90.f * (row + 1)),
        background);
  }

  auto typeface = skity::Typeface::MakeFromFile(kRobotoRegular);
  ASSERT_NE(typeface, nullptr);
  auto text = MakeSubpixelTextBlob("BLEND", typeface, 68.f);
  auto overlapping = MakeOverlappingTextBlob(typeface, 82.f);
  ASSERT_NE(text, nullptr);
  ASSERT_NE(overlapping, nullptr);

  skity::Paint paint;
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetTypeface(typeface);
  paint.SetAntiAlias(true);

  paint.SetBlendMode(skity::BlendMode::kSrc);
  paint.SetFillColor(0xFFFFD166);
  canvas->DrawTextBlob(text.get(), 20.f, 72.f, paint);

  paint.SetFillColor(0x7318D9FF);
  canvas->DrawTextBlob(text.get(), 20.f, 162.f, paint);

  paint.SetBlendMode(skity::BlendMode::kOverlay);
  paint.SetFillColor(0xB3FF8C42);
  canvas->DrawTextBlob(overlapping.get(), 24.f, 254.f, paint);

  skity::Point gradient_points[] = {{0.f, 0.f, 0.f, 1.f},
                                    {kWidth, 0.f, 0.f, 1.f}};
  skity::Color4f gradient_colors[] = {
      skity::Colors::kRed, skity::Colors::kGreen, skity::Colors::kBlue};
  float gradient_positions[] = {0.f, 0.5f, 1.f};
  paint.SetShader(skity::Shader::MakeLinear(gradient_points, gradient_colors,
                                            gradient_positions, 3));
  float color_matrix[20] = {0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f,
                            1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f};
  paint.SetColorFilter(skity::ColorFilters::Matrix(color_matrix));
  paint.SetAlphaF(0.8f);
  canvas->DrawTextBlob(text.get(), 20.f, 344.f, paint);

  std::filesystem::path golden_path = kGoldenTestImageDir;
  golden_path.append("text_coverage_aware_blending.png");
  auto dl = recorder.FinishRecording();
  skity::testing::GoldenTestEnvConfig config;
  config.sample_count = 1;
  config.supports_framebuffer_fetch = false;
  config.supports_native_advanced_blend = false;
  config.supports_native_advanced_blend_coherent = false;
  config.supports_dual_source_blending =
      skity::testing::SupportsDualSourceBlending();
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), kWidth, kHeight, golden_path.c_str(), config));
}

TEST(TextGolden, ShaderVariantsExact) {
  constexpr float kWidth = 520.f;
  constexpr float kHeight = 480.f;
  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(kWidth, kHeight));
  auto* canvas = recorder.GetRecordingCanvas();
  canvas->Clear(skity::Color_WHITE);

  auto typeface = skity::Typeface::MakeFromFile(kRobotoRegular);
  ASSERT_NE(typeface, nullptr);

  skity::Bitmap bitmap(4, 1);
  bitmap.SetPixel(0, 0, skity::ColorSetARGB(255, 244, 67, 54));
  bitmap.SetPixel(1, 0, skity::ColorSetARGB(255, 33, 150, 243));
  bitmap.SetPixel(2, 0, skity::ColorSetARGB(255, 76, 175, 80));
  bitmap.SetPixel(3, 0, skity::ColorSetARGB(255, 255, 193, 7));
  auto image = skity::Image::MakeImage(bitmap.GetPixmap());
  ASSERT_NE(image, nullptr);

  skity::Color4f colors[] = {skity::Colors::kRed, skity::Colors::kGreen,
                             skity::Colors::kBlue, skity::Colors::kRed};
  float positions[] = {0.f, 0.33f, 0.66f, 1.f};
  skity::Point linear_points[] = {{24.f, 0.f, 0.f, 1.f},
                                  {440.f, 0.f, 0.f, 1.f}};
  const char* labels[] = {"IMAGE", "LINEAR", "RADIAL", "SWEEP", "CONICAL"};
  float baselines[] = {78.f, 170.f, 262.f, 354.f, 446.f};
  std::shared_ptr<skity::Shader> shaders[] = {
      skity::Shader::MakeShader(
          image,
          skity::SamplingOptions{skity::FilterMode::kNearest,
                                 skity::MipmapMode::kNone},
          skity::TileMode::kClamp, skity::TileMode::kClamp,
          skity::Matrix::Scale(64.f, 64.f)),
      skity::Shader::MakeLinear(linear_points, colors, positions, 4),
      skity::Shader::MakeRadial({180.f, 230.f, 0.f, 1.f}, 220.f, colors,
                                positions, 4),
      skity::Shader::MakeSweep(180.f, 322.f, 0.f, 360.f, colors, positions, 4),
      skity::Shader::MakeTwoPointConical({180.f, 414.f, 0.f, 1.f}, 30.f,
                                         {180.f, 414.f, 0.f, 1.f}, 300.f,
                                         colors, positions, 4),
  };

  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetTypeface(typeface);
  for (size_t i = 0; i < 5; ++i) {
    auto text = MakeSubpixelTextBlob(labels[i], typeface, 64.f);
    ASSERT_NE(text, nullptr);
    ASSERT_NE(shaders[i], nullptr);
    paint.SetShader(shaders[i]);
    canvas->DrawTextBlob(text.get(), 24.f, baselines[i], paint);
  }

  std::filesystem::path golden_path = kGoldenTestImageDir;
  golden_path.append("text_shader_variants_exact.png");
  auto dl = recorder.FinishRecording();
  skity::testing::GoldenTestEnvConfig config;
  config.sample_count = 1;
  config.require_exact_pixel_match = true;
  EXPECT_TRUE(skity::testing::CompareGoldenTexture(
      dl.get(), kWidth, kHeight, golden_path.c_str(), config));
}
