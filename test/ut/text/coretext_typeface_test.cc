// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <algorithm>
#include <iterator>
#include <skity/geometry/matrix.hpp>
#include <skity/io/data.hpp>
#include <skity/text/font.hpp>
#include <skity/text/font_arguments.hpp>
#include <skity/text/font_manager.hpp>
#include <skity/text/ports/typeface_ct.hpp>
#include <skity/text/typeface.hpp>
#include <string>
#include <vector>

#include "src/text/ports/darwin/scaler_context_darwin.hpp"
#include "src/text/ports/darwin/types_darwin.hpp"

namespace skity {
namespace {

constexpr const char* kNotoSansCjkTtc =
    SKITY_FONT_DIR "fonts/resources/NotoSansCJK-Regular.ttc";

void ExpectNotoSansCjkIndex1(std::shared_ptr<Typeface> const& typeface) {
  ASSERT_NE(typeface, nullptr);

  FontDescriptor desc = typeface->GetFontDescriptor();
  EXPECT_EQ(desc.collection_index, 1);
  EXPECT_NE(desc.family_name.find("Noto Sans CJK KR"), std::string::npos);
  EXPECT_NE(desc.post_script_name.find("NotoSansCJKKR-Regular"),
            std::string::npos);
  EXPECT_FALSE(desc.family_name.empty());
  EXPECT_FALSE(desc.full_name.empty());
  EXPECT_FALSE(desc.post_script_name.empty());
  EXPECT_EQ(desc.family_name.find('\0'), std::string::npos);
  EXPECT_EQ(desc.full_name.find('\0'), std::string::npos);
  EXPECT_EQ(desc.post_script_name.find('\0'), std::string::npos);
  EXPECT_NE(typeface->UnicharToGlyph(0x4E00), 0);
  EXPECT_GT(typeface->CountTables(), 0);
}

TEST(CoreTextTypefaceTest, ConvertsCFStringToExactUtf8Length) {
  constexpr UniChar kCharacters[] = {'P', 'i', 'n', 'g', 'F',    'a',   'n',
                                     'g', 'S', 'C', '-', 0x4E2D, 0x6587};
  UniqueCFRef<CFStringRef> cf_string(CFStringCreateWithCharacters(
      kCFAllocatorDefault, kCharacters, std::size(kCharacters)));
  ASSERT_NE(cf_string, nullptr);

  std::string converted = cf_string_to_string(cf_string.get());

  EXPECT_EQ(converted, "PingFangSC-\xE4\xB8\xAD\xE6\x96\x87");
  EXPECT_EQ(converted.find('\0'), std::string::npos);
}

TEST(CoreTextTypefaceTest, MakeFromFileSupportsTtcIndex) {
  auto font_manager = FontManager::RefDefault();
  auto typeface = font_manager->MakeFromFile(kNotoSansCjkTtc, 1);

  ExpectNotoSansCjkIndex1(typeface);
}

TEST(CoreTextTypefaceTest, MakeFromDataSupportsTtcIndex) {
  auto data = Data::MakeFromFileName(kNotoSansCjkTtc);
  ASSERT_NE(data, nullptr);
  ASSERT_GT(data->Size(), 0u);

  auto font_manager = FontManager::RefDefault();
  auto typeface = font_manager->MakeFromData(data, 1);

  ExpectNotoSansCjkIndex1(typeface);
}

TEST(CoreTextTypefaceTest, MakeFromDataRejectsInvalidTtcIndex) {
  auto data = Data::MakeFromFileName(kNotoSansCjkTtc);
  ASSERT_NE(data, nullptr);
  ASSERT_GT(data->Size(), 0u);

  auto font_manager = FontManager::RefDefault();
  EXPECT_EQ(font_manager->MakeFromData(data, -1), nullptr);
  EXPECT_EQ(font_manager->MakeFromData(data, 999), nullptr);
}

TEST(CoreTextTypefaceTest, MakeVariationRejectsTtcIndexMismatch) {
  auto font_manager = FontManager::RefDefault();
  auto typeface = font_manager->MakeFromFile(kNotoSansCjkTtc, 0);
  ASSERT_NE(typeface, nullptr);

  FontArguments args;
  args.SetCollectionIndex(1);

  EXPECT_EQ(typeface->MakeVariation(args), nullptr);
}

TEST(CoreTextTypefaceTest, FromCTFontWithoutCacheCreatesIndependentTypefaces) {
  CTFontRef ct_font =
      CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, 16.0, nullptr);
  ASSERT_NE(ct_font, nullptr);

  auto first = TypefaceCT::TypefaceFromCTFontWithoutCache(ct_font);
  auto second = TypefaceCT::TypefaceFromCTFontWithoutCache(ct_font);
  CFRelease(ct_font);

  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  EXPECT_NE(first->TypefaceId(), second->TypefaceId());
}

TEST(CoreTextScalerContextTest, ReusesOffscreenResources) {
  OffScreenContext context(Color_BLACK);

  CGColorSpaceRef gray_color_space = context.GetCGColorSpace(false);
  CGColorSpaceRef rgb_color_space = context.GetCGColorSpace(true);
  ASSERT_NE(gray_color_space, nullptr);
  ASSERT_NE(rgb_color_space, nullptr);
  EXPECT_EQ(context.GetCGColorSpace(false), gray_color_space);
  EXPECT_EQ(context.GetCGColorSpace(true), rgb_color_space);

  context.ResizeContext(32, 32, false);
  void* initial_buffer = context.GetAddr();
  ASSERT_NE(initial_buffer, nullptr);

  context.ResizeContext(16, 16, true);
  EXPECT_EQ(context.GetAddr(), initial_buffer);

  context.ResizeContext(64, 64, false);
  void* grown_buffer = context.GetAddr();
  ASSERT_NE(grown_buffer, nullptr);
  EXPECT_NE(grown_buffer, initial_buffer);

  context.ResizeContext(32, 32, true);
  EXPECT_EQ(context.GetAddr(), grown_buffer);
}

TEST(CoreTextScalerContextTest, ReusedOffscreenBitmapPreservesGlyphPixels) {
  auto font_manager = FontManager::RefDefault();
  auto typeface = font_manager->MakeFromFile(kNotoSansCjkTtc, 0);
  ASSERT_NE(typeface, nullptr);

  Font font(typeface, 64.f);
  const GlyphID wide_glyph = typeface->UnicharToGlyph('W');
  const GlyphID small_glyph = typeface->UnicharToGlyph('.');
  ASSERT_NE(wide_glyph, 0);
  ASSERT_NE(small_glyph, 0);

  const GlyphData* wide_data = nullptr;
  font.LoadGlyphBitmap(&wide_glyph, 1, &wide_data, Paint(), 1.f, Matrix());
  ASSERT_NE(wide_data, nullptr);
  GlyphBitmapData first_image = wide_data->Image();
  ASSERT_NE(first_image.buffer, nullptr);
  ASSERT_EQ(first_image.format, BitmapFormat::kGray8);
  const size_t first_size = static_cast<size_t>(first_image.width) *
                            static_cast<size_t>(first_image.height);
  std::vector<uint8_t> first_pixels(first_image.buffer,
                                    first_image.buffer + first_size);
  EXPECT_TRUE(std::any_of(first_pixels.begin(), first_pixels.end(),
                          [](uint8_t alpha) { return alpha != 0; }));

  const GlyphData* small_data = nullptr;
  font.LoadGlyphBitmap(&small_glyph, 1, &small_data, Paint(), 1.f, Matrix());
  ASSERT_NE(small_data, nullptr);
  ASSERT_NE(small_data->Image().buffer, nullptr);
  EXPECT_EQ(small_data->Image().buffer, first_image.buffer);

  font.LoadGlyphBitmap(&wide_glyph, 1, &wide_data, Paint(), 1.f, Matrix());
  ASSERT_NE(wide_data, nullptr);
  GlyphBitmapData second_image = wide_data->Image();
  ASSERT_NE(second_image.buffer, nullptr);
  ASSERT_EQ(second_image.width, first_image.width);
  ASSERT_EQ(second_image.height, first_image.height);
  EXPECT_EQ(second_image.buffer, first_image.buffer);
  std::vector<uint8_t> second_pixels(second_image.buffer,
                                     second_image.buffer + first_size);
  EXPECT_EQ(second_pixels, first_pixels);
}

}  // namespace
}  // namespace skity
