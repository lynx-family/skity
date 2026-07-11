// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <cstdint>
#include <skity/io/data.hpp>
#include <skity/text/font_manager.hpp>
#include <skity/text/font_style.hpp>
#include <skity/text/typeface.hpp>

using namespace skity;

// ---------- RefDefault ----------
TEST(FontManagerTest, RefDefaultReturnsNonNull) {
  auto font_manager = FontManager::RefDefault();

  ASSERT_NE(font_manager, nullptr);
}

TEST(FontManagerTest, RefDefaultIsSingleton) {
  auto fm1 = FontManager::RefDefault();
  auto fm2 = FontManager::RefDefault();

  ASSERT_NE(fm1, nullptr);
  ASSERT_NE(fm2, nullptr);
  EXPECT_EQ(fm1.get(), fm2.get());
}

// ---------- GetDefaultTypeface ----------
TEST(FontManagerTest, GetDefaultTypefaceIsNonNull) {
  auto font_manager = FontManager::RefDefault();

  auto typeface = font_manager->GetDefaultTypeface(FontStyle());

  EXPECT_NE(typeface, nullptr);
}

// ---------- CountFamilies / GetFamilyName ----------
TEST(FontManagerTest, CountFamiliesAndGetFamilyNameDoNotCrash) {
  auto font_manager = FontManager::RefDefault();

  int count = font_manager->CountFamilies();
  EXPECT_GE(count, 0);

  // Every index in [0, count) must be safe to query and should not crash.
  for (int i = 0; i < count; i++) {
    std::string name = font_manager->GetFamilyName(i);
    // No assumption is made about the actual family names, only that
    // querying them is well defined (does not crash).
    (void)name;
  }
}

TEST(FontManagerTest, GetFamilyNameOutOfRangeDoesNotCrash) {
  auto font_manager = FontManager::RefDefault();

  // Negative and too-large indices should be handled gracefully rather
  // than crashing.
  std::string negative = font_manager->GetFamilyName(-1);
  std::string too_large =
      font_manager->GetFamilyName(font_manager->CountFamilies() + 1000);

  (void)negative;
  (void)too_large;
}

// ---------- MatchFamily / CreateStyleSet ----------
TEST(FontManagerTest, MatchFamilyUnknownNameReturnsNonNullStyleSet) {
  auto font_manager = FontManager::RefDefault();

  // FontManager::MatchFamily() is documented (via FontManager::CreateEmpty
  // fallback in font_manager.cc) to never return null: an unknown family
  // name should still yield a valid, possibly empty, FontStyleSet.
  auto style_set =
      font_manager->MatchFamily("this-family-definitely-does-not-exist");

  ASSERT_NE(style_set, nullptr);
  EXPECT_GE(style_set->Count(), 0);
}

TEST(FontManagerTest, CreateStyleSetOutOfRangeReturnsNonNullStyleSet) {
  auto font_manager = FontManager::RefDefault();

  // Same null-safety guarantee applies to CreateStyleSet() for an
  // out-of-range index.
  auto style_set = font_manager->CreateStyleSet(-1);

  ASSERT_NE(style_set, nullptr);
  EXPECT_GE(style_set->Count(), 0);
}

// ---------- MakeFromFile ----------
TEST(FontManagerTest, MakeFromFileNonexistentPathReturnsNull) {
  auto font_manager = FontManager::RefDefault();

  auto typeface = font_manager->MakeFromFile("/definitely/does/not/exist.ttf");

  EXPECT_EQ(typeface, nullptr);
}

// ---------- MakeFromData ----------
TEST(FontManagerTest, MakeFromDataGarbageReturnsNull) {
  auto font_manager = FontManager::RefDefault();

  uint8_t garbage[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
  auto data = Data::MakeWithCopy(garbage, sizeof(garbage));
  ASSERT_NE(data, nullptr);

  auto typeface = font_manager->MakeFromData(data);

  EXPECT_EQ(typeface, nullptr);
}

TEST(FontManagerTest, MakeFromDataNullDataReturnsNull) {
  auto font_manager = FontManager::RefDefault();

  auto typeface = font_manager->MakeFromData(nullptr);

  EXPECT_EQ(typeface, nullptr);
}

// ---------- FontStyleSet::CreateEmpty ----------
TEST(FontStyleSetTest, CreateEmptyHasZeroCount) {
  auto style_set = FontStyleSet::CreateEmpty();

  ASSERT_NE(style_set, nullptr);
  EXPECT_EQ(style_set->Count(), 0);
}

TEST(FontStyleSetTest, CreateEmptyMatchStyleReturnsNull) {
  auto style_set = FontStyleSet::CreateEmpty();

  ASSERT_NE(style_set, nullptr);
  EXPECT_EQ(style_set->MatchStyle(FontStyle()), nullptr);
}
