// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <skity/io/data.hpp>
#include <skity/text/font_arguments.hpp>
#include <skity/text/font_manager.hpp>
#include <skity/text/typeface.hpp>
#include <string>

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
  EXPECT_FALSE(desc.post_script_name.empty());
  EXPECT_NE(typeface->UnicharToGlyph(0x4E00), 0);
  EXPECT_GT(typeface->CountTables(), 0);
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

}  // namespace
}  // namespace skity
