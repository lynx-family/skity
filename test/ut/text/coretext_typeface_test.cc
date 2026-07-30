// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <iterator>
#include <skity/io/data.hpp>
#include <skity/text/font_arguments.hpp>
#include <skity/text/font_manager.hpp>
#include <skity/text/ports/typeface_ct.hpp>
#include <skity/text/typeface.hpp>
#include <string>

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

}  // namespace
}  // namespace skity
