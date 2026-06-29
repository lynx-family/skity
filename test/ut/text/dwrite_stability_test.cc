// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <array>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <skity/geometry/matrix.hpp>
#include <skity/graphic/color.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/io/data.hpp>
#include <skity/text/font.hpp>
#include <skity/text/font_arguments.hpp>
#include <skity/text/font_manager.hpp>
#include <skity/text/glyph.hpp>
#include <skity/text/typeface.hpp>
#include <sstream>
#include <string>
#include <vector>

#include "concurrent_runner.h"
#include "src/render/text/text_transform.hpp"
#include "src/text/scaler_context_desc.hpp"

namespace skity {

std::shared_ptr<FontManager> InitFontManagerDWrite();

namespace {

constexpr int kStressIterations = 32;
constexpr int kThreadCount = 4;
constexpr int kThreadIterations = 24;

std::string FontPath(const char* name) {
  return std::string(SKITY_TEST_FONT_ROOT) + "/" + name;
}

std::shared_ptr<Data> ReadFontData(const char* name) {
  const std::string path = FontPath(name);
  return Data::MakeFromFileName(path.c_str());
}

std::shared_ptr<Typeface> MakeDataTypeface(
    const std::shared_ptr<FontManager>& manager, const char* name,
    int ttc_index = 0) {
  auto data = ReadFontData(name);
  if (!data) {
    return nullptr;
  }
  return manager->MakeFromData(data, ttc_index);
}

Paint MakePaint(Color color, Paint::Style style = Paint::kFill_Style,
                float stroke_width = 1.5f) {
  Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(color);
  paint.SetStyle(style);
  paint.SetStrokeWidth(stroke_width);
  return paint;
}

std::shared_ptr<Typeface> MakeWeightVariation(
    const std::shared_ptr<Typeface>& typeface, float weight) {
  if (!typeface) {
    return nullptr;
  }

  VariationPosition position;
  position.AddCoordinate(SetFourByteTag('w', 'g', 'h', 't'), weight);

  FontArguments arguments;
  arguments.SetVariationDesignPosition(position);
  return typeface->MakeVariation(arguments);
}

const char* GlyphFormatName(GlyphFormat format) {
  switch (format) {
    case GlyphFormat::A8:
      return "A8";
    case GlyphFormat::RGBA32:
      return "RGBA32";
    case GlyphFormat::BGRA32:
      return "BGRA32";
  }
  return "InvalidGlyphFormat";
}

const char* BitmapFormatName(BitmapFormat format) {
  switch (format) {
    case BitmapFormat::kUnknown:
      return "Unknown";
    case BitmapFormat::kGray8:
      return "Gray8";
    case BitmapFormat::kBGRA8:
      return "BGRA8";
    case BitmapFormat::kRGBA8:
      return "RGBA8";
  }
  return "InvalidBitmapFormat";
}

std::string CodepointString(Unichar unichar) {
  std::ostringstream stream;
  stream << "U+" << std::hex << std::uppercase << unichar;
  return stream.str();
}

std::string GlyphDebugString(const GlyphData* glyph) {
  if (!glyph) {
    return "glyph_data=null";
  }

  std::ostringstream stream;
  stream << "glyph_id=" << glyph->Id();
  auto format = glyph->GetFormat();
  stream << ", glyph_format=" << (format ? GlyphFormatName(*format) : "none");
  const auto& image = glyph->Image();
  stream << ", bitmap_format=" << BitmapFormatName(image.format)
         << ", image_size=" << image.width << "x" << image.height
         << ", origin=(" << image.origin_x << "," << image.origin_y << ")"
         << ", raster_origin=(" << image.origin_x_for_raster << ","
         << image.origin_y_for_raster << ")"
         << ", has_buffer=" << (image.buffer ? "yes" : "no")
         << ", need_free=" << (image.need_free ? "yes" : "no");
  return stream.str();
}

std::string JoinFailures(const std::vector<std::string>& failures) {
  std::ostringstream stream;
  stream << failures.size() << " failure(s):";
  for (const auto& failure : failures) {
    stream << "\n" << failure;
  }
  return stream.str();
}

void ReleaseOwnedImage(const GlyphData* glyph) {
  if (!glyph || !glyph->NeedFree() || glyph->Image().buffer == nullptr) {
    return;
  }

  std::free(glyph->Image().buffer);
  auto& image = const_cast<GlyphBitmapData&>(glyph->Image());
  image.buffer = nullptr;
  image.need_free = false;
}

bool ExerciseGlyphPipeline(const std::shared_ptr<Typeface>& typeface,
                           Unichar unichar, const Paint& paint, float size,
                           bool embolden, const Matrix& transform = Matrix(),
                           std::string* failure = nullptr) {
  auto fail = [&](const std::string& message) {
    if (failure) {
      std::ostringstream stream;
      stream << message << ", codepoint=" << CodepointString(unichar)
             << ", size=" << size
             << ", embolden=" << (embolden ? "true" : "false");
      *failure = stream.str();
    }
    return false;
  };

  if (!typeface) {
    return fail("typeface is null");
  }

  GlyphID glyph = typeface->UnicharToGlyph(unichar);
  if (glyph == 0) {
    return fail("UnicharToGlyph returned 0");
  }

  Font font(typeface, size);
  font.SetEmbolden(embolden);

  const GlyphData* glyph_data[1] = {};
  font.LoadGlyphMetrics(&glyph, 1, glyph_data, paint);
  if (glyph_data[0] == nullptr) {
    return fail("LoadGlyphMetrics returned null");
  }

  font.LoadGlyphBitmapInfo(&glyph, 1, glyph_data, paint, 1.f, transform);
  if (glyph_data[0] == nullptr) {
    return fail("LoadGlyphBitmapInfo returned null");
  }

  font.LoadGlyphBitmap(&glyph, 1, glyph_data, paint, 1.f, transform);
  if (glyph_data[0] == nullptr) {
    return fail("LoadGlyphBitmap returned null");
  }
  ReleaseOwnedImage(glyph_data[0]);

  font.LoadGlyphPath(&glyph, 1, glyph_data);
  if (glyph_data[0] == nullptr) {
    return fail("LoadGlyphPath returned null");
  }
  return true;
}

TEST(DWriteFontStabilityTest, DataBackedAndTtcFacesSurviveRepeatedUse) {
  auto manager = InitFontManagerDWrite();
  ASSERT_NE(manager, nullptr);

  for (int i = 0; i < kStressIterations; ++i) {
    auto flex = MakeDataTypeface(manager, "RobotoFlex-Regular.ttf");
    ASSERT_NE(flex, nullptr);

    auto variation = MakeWeightVariation(flex, (i & 1) == 0 ? 300.f : 800.f);
    ASSERT_NE(variation, nullptr);
    EXPECT_TRUE(ExerciseGlyphPipeline(variation, 'A',
                                      MakePaint(ColorSetRGB(20, 70, 130)),
                                      18.f + i % 5, (i & 1) != 0));

    int ttc_index = i % 5;
    auto cjk = MakeDataTypeface(manager, "NotoSansCJK-Regular.ttc", ttc_index);
    ASSERT_NE(cjk, nullptr);
    EXPECT_EQ(cjk->GetFontDescriptor().collection_index, ttc_index);
    EXPECT_TRUE(ExerciseGlyphPipeline(
        cjk, 0x95E8, MakePaint(ColorSetRGB(40, 80, 40)), 24.f + i % 3, false));
  }
}

TEST(DWriteFontStabilityTest, TtcCollectionIndexCanSwitchInMakeVariation) {
  auto manager = InitFontManagerDWrite();
  ASSERT_NE(manager, nullptr);

  auto data = ReadFontData("NotoSansCJK-Regular.ttc");
  ASSERT_NE(data, nullptr);

  auto face0 = manager->MakeFromData(data, 0);
  ASSERT_NE(face0, nullptr);
  ASSERT_EQ(face0->GetFontDescriptor().collection_index, 0);

  FontArguments arguments;
  arguments.SetCollectionIndex(1);

  auto face1 = face0->MakeVariation(arguments);
  ASSERT_NE(face1, nullptr);
  EXPECT_EQ(face1->GetFontDescriptor().collection_index, 1);
  EXPECT_TRUE(ExerciseGlyphPipeline(
      face1, 0x95E8, MakePaint(ColorSetRGB(60, 90, 60)), 26.f, false));
}

TEST(DWriteFontStabilityTest, VariableFontCanBeCreatedAndReleasedRepeatedly) {
  auto manager = InitFontManagerDWrite();
  ASSERT_NE(manager, nullptr);

  auto flex = MakeDataTypeface(manager, "RobotoFlex-Regular.ttf");
  ASSERT_NE(flex, nullptr);
  if (flex->GetVariationDesignParameters().empty()) {
    GTEST_SKIP() << "DirectWrite variation APIs are unavailable.";
  }

  for (int i = 0; i < kStressIterations; ++i) {
    auto data = ReadFontData("RobotoFlex-Regular.ttf");
    ASSERT_NE(data, nullptr);

    auto typeface = manager->MakeFromData(data);
    ASSERT_NE(typeface, nullptr);

    auto variation =
        MakeWeightVariation(typeface, (i % 2 == 0) ? 100.f : 900.f);
    ASSERT_NE(variation, nullptr);
    EXPECT_FALSE(
        variation->GetVariationDesignPosition().GetCoordinates().empty());
    EXPECT_TRUE(ExerciseGlyphPipeline(variation, 'g',
                                      MakePaint(ColorSetRGB(20, 50, 100)),
                                      20.f + i % 4, false));
  }
}

TEST(DWriteFontStabilityTest, ColorEmojiFakeBoldKeepsColorBitmapFormat) {
  auto manager = InitFontManagerDWrite();
  ASSERT_NE(manager, nullptr) << "InitFontManagerDWrite returned null.";

  auto emoji = MakeDataTypeface(manager, "NotoColorEmoji.ttf");
  ASSERT_NE(emoji, nullptr) << "Failed to create DirectWrite typeface from "
                            << FontPath("NotoColorEmoji.ttf");

  GlyphID glyph = emoji->UnicharToGlyph(0x1F600);
  ASSERT_NE(glyph, 0) << "NotoColorEmoji.ttf has no glyph for U+1F600.";

  Font regular_font(emoji, 48.f);
  const Paint paint = MakePaint(Color_RED);
  const GlyphData* regular_glyph_data[1] = {};
  regular_font.LoadGlyphBitmapInfo(&glyph, 1, regular_glyph_data, paint, 1.f,
                                   Matrix());
  ASSERT_NE(regular_glyph_data[0], nullptr)
      << "LoadGlyphBitmapInfo returned null for regular color emoji.";
  if (!regular_glyph_data[0]->GetFormat().has_value()) {
    GTEST_SKIP() << "DirectWrite color glyph image is unavailable: "
                 << GlyphDebugString(regular_glyph_data[0]);
  }
  if (*regular_glyph_data[0]->GetFormat() != GlyphFormat::BGRA32) {
    GTEST_SKIP() << "DirectWrite did not expose this emoji as color bitmap in "
                    "this environment: "
                 << GlyphDebugString(regular_glyph_data[0]);
  }

  Font font(emoji, 48.f);
  font.SetEmbolden(true);

  const GlyphData* glyph_data[1] = {};
  font.LoadGlyphBitmapInfo(&glyph, 1, glyph_data, paint, 1.f, Matrix());
  ASSERT_NE(glyph_data[0], nullptr)
      << "LoadGlyphBitmapInfo returned null for fake-bold color emoji.";
  if (!glyph_data[0]->GetFormat().has_value()) {
    GTEST_SKIP() << "DirectWrite color glyph image is unavailable: "
                 << GlyphDebugString(glyph_data[0]);
  }
  ASSERT_EQ(*glyph_data[0]->GetFormat(), GlyphFormat::BGRA32)
      << "Expected fake-bold color emoji to keep BGRA32 format: "
      << GlyphDebugString(glyph_data[0]);

  font.LoadGlyphBitmap(&glyph, 1, glyph_data, paint, 1.f, Matrix());
  ASSERT_NE(glyph_data[0], nullptr)
      << "LoadGlyphBitmap returned null for fake-bold color emoji.";
  EXPECT_EQ(glyph_data[0]->Image().format, BitmapFormat::kBGRA8)
      << "Expected fake-bold color emoji bitmap to stay BGRA8: "
      << GlyphDebugString(glyph_data[0]);
  EXPECT_GT(glyph_data[0]->Image().width, 0.f)
      << GlyphDebugString(glyph_data[0]);
  EXPECT_GT(glyph_data[0]->Image().height, 0.f)
      << GlyphDebugString(glyph_data[0]);
  ReleaseOwnedImage(glyph_data[0]);
}

TEST(DWriteFontStabilityTest, DISABLED_ConcurrentManagerDataAndFallbackSmoke) {
  auto manager = InitFontManagerDWrite();
  ASSERT_NE(manager, nullptr);

  const char* emoji_bcp47[] = {"und-Zsye"};
  const bool has_emoji_fallback =
      manager->MatchFamilyStyleCharacter(nullptr, FontStyle(), emoji_bcp47, 1,
                                         0x1F600) != nullptr;
  const char* cjk_bcp47[] = {"zh-CN"};
  const bool has_cjk_fallback =
      manager->MatchFamilyStyleCharacter(nullptr, FontStyle(), cjk_bcp47, 1,
                                         0x95E8) != nullptr;

  std::mutex failures_mutex;
  std::vector<std::string> failures;
  auto add_failure = [&](int iteration, const std::string& message) {
    std::lock_guard<std::mutex> lock(failures_mutex);
    if (failures.size() >= 64) {
      return;
    }
    std::ostringstream stream;
    stream << "iteration " << iteration << ": " << message;
    failures.push_back(stream.str());
  };

  ConcurrentRunner runner(kThreadCount, kThreadIterations);
  runner.Run([&](int i) {
    auto thread_manager = InitFontManagerDWrite();
    if (!thread_manager) {
      add_failure(i, "InitFontManagerDWrite returned null");
      return;
    }

    auto default_typeface = thread_manager->GetDefaultTypeface(FontStyle());
    if (!default_typeface) {
      add_failure(i, "GetDefaultTypeface(FontStyle()) returned null");
    }

    auto emoji_fallback = thread_manager->MatchFamilyStyleCharacter(
        nullptr, FontStyle(), emoji_bcp47, 1, 0x1F600);
    if (has_emoji_fallback && !emoji_fallback) {
      add_failure(i,
                  "emoji fallback returned null for U+1F600, bcp47=und-Zsye");
    }

    auto cjk_fallback = thread_manager->MatchFamilyStyleCharacter(
        nullptr, FontStyle(), cjk_bcp47, 1, 0x95E8);
    if (has_cjk_fallback && !cjk_fallback) {
      add_failure(i, "CJK fallback returned null for U+95E8, bcp47=zh-CN");
    }

    auto data_typeface =
        MakeDataTypeface(thread_manager, "RobotoFlex-Regular.ttf");
    if (!data_typeface) {
      add_failure(i, "MakeFromData(RobotoFlex-Regular.ttf) returned null");
      return;
    }

    const Paint paint = MakePaint((i & 1) ? ColorSetRGB(180, 30, 30)
                                          : ColorSetRGB(30, 80, 180));
    std::string pipeline_failure;
    if (!ExerciseGlyphPipeline(data_typeface, 'A', paint, 14.f + (i % 5),
                               (i & 1) != 0, Matrix(), &pipeline_failure)) {
      add_failure(i, "RobotoFlex glyph pipeline failed: " + pipeline_failure);
    }
  });

  EXPECT_TRUE(failures.empty()) << JoinFailures(failures);
}

TEST(DWriteFontStabilityTest, CachePressureAcrossColorsTransformsAndStyles) {
  auto manager = InitFontManagerDWrite();
  ASSERT_NE(manager, nullptr);

  auto typeface = MakeDataTypeface(manager, "RobotoFlex-Regular.ttf");
  ASSERT_NE(typeface, nullptr);

  Font font(typeface, 24.f);
  Paint red = MakePaint(Color_RED);
  Paint green = MakePaint(Color_GREEN);
  Matrix22 transform;
  ScalerContextDesc red_desc =
      ScalerContextDesc::MakeTransformed(font, red, 1.f, transform);
  ScalerContextDesc green_desc =
      ScalerContextDesc::MakeTransformed(font, green, 1.f, transform);
  EXPECT_NE(red_desc.foreground_color, green_desc.foreground_color);
  EXPECT_NE(red_desc, green_desc);

  std::array<Color, 4> colors = {Color_RED, Color_GREEN, Color_BLUE,
                                 ColorSetRGB(120, 60, 10)};
  std::array<Paint::Style, 4> styles = {
      Paint::kFill_Style, Paint::kStroke_Style, Paint::kStrokeAndFill_Style,
      Paint::kStrokeThenFill_Style};
  std::array<Matrix, 4> transforms = {Matrix(), Matrix::Scale(1.25f, 0.85f),
                                      Matrix::Skew(0.18f, 0.f),
                                      Matrix::RotateDeg(8.f)};

  for (int i = 0; i < 96; ++i) {
    font.SetSize(12.f + static_cast<float>(i % 9));
    font.SetEmbolden((i & 1) != 0);
    Paint paint =
        MakePaint(colors[i % colors.size()], styles[(i / 3) % styles.size()],
                  1.f + static_cast<float>(i % 4));
    EXPECT_TRUE(ExerciseGlyphPipeline(font.GetTypeface(), 'B', paint,
                                      font.GetSize(), font.IsEmbolden(),
                                      transforms[(i / 5) % transforms.size()]));
  }
}

}  // namespace

}  // namespace skity
