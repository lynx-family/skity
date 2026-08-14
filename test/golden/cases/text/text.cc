// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
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
#include <string>
#include <utility>
#include <vector>

#include "common/golden_test_check.hpp"

static const char* kGoldenTestImageDir = CASE_DIR;

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
