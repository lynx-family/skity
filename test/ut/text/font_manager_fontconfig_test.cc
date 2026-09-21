// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/text/ports/linux/font_manager_fontconfig.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <skity/text/font.hpp>
#include <skity/text/glyph.hpp>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace skity {
namespace {

std::string EscapeXml(std::string text) {
  for (const auto& pair : {std::pair<std::string, std::string>{"&", "&amp;"},
                           {"<", "&lt;"},
                           {">", "&gt;"}}) {
    size_t offset = 0;
    while ((offset = text.find(pair.first, offset)) != std::string::npos) {
      text.replace(offset, pair.first.size(), pair.second);
      offset += pair.second.size();
    }
  }
  return text;
}

class FontManagerFontConfigTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::string directory =
        (std::filesystem::temp_directory_path() / "skity-fontconfig-XXXXXX")
            .string();
    char* created = mkdtemp(directory.data());
    ASSERT_NE(created, nullptr);
    root_ = created;
    std::filesystem::create_directory(root_ / "fonts");
    std::filesystem::create_directory(root_ / "cache");
    for (const char* name :
         {"SourceSansPro-Regular.ttf", "SourceSansPro-Bold.ttf",
          "SourceSansPro-Italic.ttf", "NotoSerif-Regular.ttf",
          "NotoSansCJK-Regular.ttc", "NotoColorEmoji.ttf",
          "RobotoFlex-Regular.ttf"}) {
      const auto source = std::filesystem::path(SKITY_TEST_FONT_ROOT) / name;
      ASSERT_TRUE(std::filesystem::is_regular_file(source)) << source;
      std::filesystem::create_symlink(source, root_ / "fonts" / name);
    }
    config_ = WriteConfig("fonts.conf", true);
    manager_ = MakeFontManagerFontConfig(config_.c_str());
    ASSERT_NE(manager_, nullptr);
  }

  void TearDown() override {
    manager_.reset();
    if (!root_.empty()) {
      std::error_code error;
      std::filesystem::remove_all(root_, error);
    }
  }

  std::string WriteConfig(const char* name, bool fonts,
                          const std::string& rules = "") {
    const auto path = root_ / name;
    std::ofstream file(path);
    file << "<?xml version=\"1.0\"?><fontconfig>";
    if (fonts) {
      file << "<dir>" << EscapeXml((root_ / "fonts").string()) << "</dir>";
    }
    file
        << "<cachedir>" << EscapeXml((root_ / "cache").string())
        << "</cachedir><config><rescan><int>0</int></rescan></config>"
        << R"(<alias><family>Harness Weak</family><prefer><family>Source Sans Pro</family></prefer></alias>)"
        << R"(<match target="pattern"><edit name="family" mode="append" binding="weak"><string>Source Sans Pro</string></edit></match>)"
        << rules << "</fontconfig>";
    return path.string();
  }

  std::filesystem::path root_;
  std::string config_;
  std::shared_ptr<FontManager> manager_;
};

TEST_F(FontManagerFontConfigTest, EnumeratesFamiliesAndStyles) {
  std::set<std::string> names;
  for (int i = 0; i < manager_->CountFamilies(); ++i) {
    EXPECT_TRUE(names.insert(manager_->GetFamilyName(i)).second);
  }
  EXPECT_EQ(names.count("Source Sans Pro"), 1u);
  EXPECT_EQ(names.count("Noto Sans CJK TC"), 1u);
  auto styles = manager_->MatchFamily("Source Sans Pro");
  ASSERT_EQ(styles->Count(), 3);
  std::set<std::string> style_names;
  for (int i = 0; i < styles->Count(); ++i) {
    FontStyle style;
    std::string name;
    styles->GetStyle(i, &style, &name);
    style_names.insert(name);
    auto face = styles->CreateTypeface(i);
    ASSERT_NE(face, nullptr);
    EXPECT_EQ(face->GetFontStyle().weight(), style.weight());
    EXPECT_EQ(face->GetFontStyle().slant(), style.slant());
    EXPECT_NE(face->UnicharToGlyph('A'), 0);
  }
  EXPECT_EQ(style_names.size(), 3u);
}

TEST_F(FontManagerFontConfigTest, FamilyAndStyleIndicesAreBounded) {
  EXPECT_TRUE(manager_->GetFamilyName(-1).empty());
  EXPECT_TRUE(manager_->GetFamilyName(manager_->CountFamilies()).empty());
  EXPECT_EQ(manager_->CreateStyleSet(-1)->Count(), 0);
  EXPECT_EQ(manager_->CreateStyleSet(manager_->CountFamilies())->Count(), 0);
  auto styles = manager_->MatchFamily("Source Sans Pro");
  EXPECT_EQ(styles->CreateTypeface(-1), nullptr);
  EXPECT_EQ(styles->CreateTypeface(styles->Count()), nullptr);
  std::string unchanged = "sentinel";
  styles->GetStyle(-1, nullptr, &unchanged);
  EXPECT_EQ(unchanged, "sentinel");
}

TEST_F(FontManagerFontConfigTest, StrictMatchDoesNotUseUnrelatedFallback) {
  EXPECT_EQ(manager_->MatchFamily(nullptr)->Count(), 0);
  EXPECT_EQ(manager_->MatchFamily("")->Count(), 0);
  EXPECT_EQ(manager_->MatchFamily("Harness Missing")->Count(), 0);
  EXPECT_EQ(manager_->MatchFamilyStyle("Harness Missing", FontStyle()),
            nullptr);
  EXPECT_EQ(manager_->MatchFamilyStyle("", FontStyle()), nullptr);
  ASSERT_NE(manager_->MatchFamilyStyle(nullptr, FontStyle()), nullptr);
  auto face = manager_->GetDefaultTypeface(FontStyle());
  ASSERT_NE(face, nullptr);
  EXPECT_EQ(face->GetFontDescriptor().family_name, "Source Sans Pro");
}

TEST_F(FontManagerFontConfigTest, AcceptsCasefoldAndWeakPreferredAliases) {
  for (const char* name : {"source sans pro", "Harness Weak"}) {
    auto face = manager_->MatchFamilyStyle(name, FontStyle());
    ASSERT_NE(face, nullptr) << name;
    EXPECT_EQ(face->GetFontDescriptor().family_name, "Source Sans Pro");
    EXPECT_EQ(manager_->MatchFamily(name)->Count(), 3);
  }
}

TEST_F(FontManagerFontConfigTest, StyleSetMatchStaysWithinItsFamily) {
  auto styles = manager_->MatchFamily("Source Sans Pro");
  auto bold = styles->MatchStyle(FontStyle(700, 5, FontStyle::kUpright_Slant));
  auto italic = styles->MatchStyle(FontStyle(400, 5, FontStyle::kItalic_Slant));
  ASSERT_NE(bold, nullptr);
  ASSERT_NE(italic, nullptr);
  EXPECT_EQ(bold->GetFontDescriptor().family_name, "Source Sans Pro");
  EXPECT_EQ(bold->GetFontStyle().weight(), 700);
  EXPECT_EQ(italic->GetFontStyle().slant(), FontStyle::kItalic_Slant);
  auto serif = manager_->MatchFamily("Noto Serif")
                   ->MatchStyle(FontStyle(700, 5, FontStyle::kUpright_Slant));
  ASSERT_NE(serif, nullptr);
  EXPECT_EQ(serif->GetFontDescriptor().family_name, "Noto Serif");
  EXPECT_EQ(manager_->MatchFamily("Harness Missing")->MatchStyle(FontStyle()),
            nullptr);
}

TEST_F(FontManagerFontConfigTest, CharacterFallbackChecksActualCoverage) {
  for (uint32_t character : {0x0041u, 0x4E00u, 0x1F600u}) {
    auto face = manager_->MatchFamilyStyleCharacter(
        "Harness Missing", FontStyle(), nullptr, 0, character);
    ASSERT_NE(face, nullptr);
    EXPECT_NE(face->UnicharToGlyph(character), 0);
  }
  EXPECT_EQ(manager_->MatchFamilyStyleCharacter(nullptr, FontStyle(), nullptr,
                                                0, 0x10FFFF),
            nullptr);
}

TEST_F(FontManagerFontConfigTest, RejectsInvalidFallbackArguments) {
  EXPECT_EQ(manager_->MatchFamilyStyleCharacter(nullptr, FontStyle(), nullptr,
                                                1, 'A'),
            nullptr);
  EXPECT_EQ(manager_->MatchFamilyStyleCharacter(nullptr, FontStyle(), nullptr,
                                                -1, 'A'),
            nullptr);
  const char* languages[] = {nullptr};
  EXPECT_EQ(manager_->MatchFamilyStyleCharacter(nullptr, FontStyle(), languages,
                                                1, 'A'),
            nullptr);
  for (uint32_t character : {0xD800u, 0xDFFFu, 0x110000u}) {
    EXPECT_EQ(manager_->MatchFamilyStyleCharacter(nullptr, FontStyle(), nullptr,
                                                  0, character),
              nullptr);
  }
}

TEST_F(FontManagerFontConfigTest, PreservesCollectionFaceAndFontData) {
  auto face = manager_->MatchFamilyStyle("Noto Sans CJK TC", FontStyle());
  ASSERT_NE(face, nullptr);
  EXPECT_EQ(face->GetFontDescriptor().family_name, "Noto Sans CJK TC");
  EXPECT_EQ(face->GetFontDescriptor().collection_index, 3);
  EXPECT_NE(face->UnicharToGlyph(0x4E00), 0);
  EXPECT_GT(face->GetUnitsPerEm(), 0u);
  EXPECT_GT(face->CountTables(), 0);
  ASSERT_NE(face->GetData(), nullptr);
}

TEST_F(FontManagerFontConfigTest, VariationClonePreservesIdentityAndSource) {
  auto face = manager_->MatchFamilyStyle("Roboto Flex", FontStyle());
  ASSERT_NE(face, nullptr);
  const auto original_position =
      face->GetVariationDesignPosition().GetCoordinates();
  VariationPosition position;
  position.AddCoordinate(SetFourByteTag('w', 'g', 'h', 't'), 700);
  FontArguments args;
  args.SetVariationDesignPosition(position);
  auto clone = face->MakeVariation(args);
  ASSERT_NE(clone, nullptr);
  EXPECT_EQ(clone->GetFontDescriptor().family_name, "Roboto Flex");
  EXPECT_EQ(clone->GetFontStyle().weight(), 700);
  EXPECT_EQ(face->GetFontStyle().weight(), 400);
  const auto after = face->GetVariationDesignPosition().GetCoordinates();
  ASSERT_EQ(after.size(), original_position.size());
  for (size_t i = 0; i < after.size(); ++i) {
    EXPECT_EQ(after[i].axis, original_position[i].axis);
    EXPECT_EQ(after[i].value, original_position[i].value);
  }
}

TEST_F(FontManagerFontConfigTest, StyleSetAndTypefaceOutliveManager) {
  auto styles = manager_->MatchFamily("Source Sans Pro");
  auto face = manager_->GetDefaultTypeface(FontStyle());
  ASSERT_NE(face, nullptr);
  manager_.reset();
  auto bold = styles->MatchStyle(FontStyle(700, 5, FontStyle::kUpright_Slant));
  ASSERT_NE(bold, nullptr);
  styles.reset();
  EXPECT_NE(face->UnicharToGlyph('A'), 0);
  EXPECT_NE(bold->UnicharToGlyph('B'), 0);
  EXPECT_GT(face->GetUnitsPerEm(), 0u);
  EXPECT_GT(bold->CountTables(), 0);
}

TEST_F(FontManagerFontConfigTest, IndependentEmptyConfigDoesNotReadHostFonts) {
  auto empty =
      MakeFontManagerFontConfig(WriteConfig("empty.conf", false).c_str());
  ASSERT_NE(empty, nullptr);
  EXPECT_EQ(empty->CountFamilies(), 0);
  EXPECT_EQ(empty->GetDefaultTypeface(FontStyle()), nullptr);
  EXPECT_EQ(
      empty->MatchFamilyStyleCharacter(nullptr, FontStyle(), nullptr, 0, 'A'),
      nullptr);
  EXPECT_GT(manager_->CountFamilies(), 0);
  const auto path =
      std::filesystem::path(SKITY_TEST_FONT_ROOT) / "Roboto-Regular.ttf";
  auto explicit_face = empty->MakeFromFile(path.c_str());
  ASSERT_NE(explicit_face, nullptr);
  EXPECT_EQ(explicit_face->GetFontDescriptor().family_name, "Roboto");
  EXPECT_EQ(empty->MakeFromFile(path.c_str(), 99), nullptr);
  EXPECT_EQ(empty->MakeFromFile((root_ / "missing.ttf").c_str()), nullptr);
}

TEST_F(FontManagerFontConfigTest, InvalidConfigDoesNotFallBackToHost) {
  EXPECT_EQ(MakeFontManagerFontConfig((root_ / "missing.conf").c_str()),
            nullptr);
  std::ofstream(root_ / "invalid.conf") << "<fontconfig><broken>";
  EXPECT_EQ(MakeFontManagerFontConfig((root_ / "invalid.conf").c_str()),
            nullptr);
  EXPECT_EQ(MakeFontManagerFontConfig(""), nullptr);
}

TEST_F(FontManagerFontConfigTest,
       SyntheticBoldPreservesCacheAndReachesFreeType) {
  const auto config = WriteConfig("synthetic.conf", true, R"(
    <alias><family>Harness Bold</family>
      <prefer><family>Source Sans Pro</family></prefer>
    </alias>
    <match target="font">
      <test name="family" target="pattern"><string>Harness Bold</string></test>
      <edit name="embolden" mode="assign"><bool>true</bool></edit>
    </match>)");
  auto manager = MakeFontManagerFontConfig(config.c_str());
  ASSERT_NE(manager, nullptr);
  auto regular = manager->MatchFamilyStyle("Source Sans Pro", FontStyle());
  auto bold = manager->MatchFamilyStyle("Harness Bold", FontStyle());
  ASSERT_NE(regular, nullptr);
  ASSERT_NE(bold, nullptr);
  EXPECT_NE(regular->TypefaceId(), bold->TypefaceId());
  EXPECT_EQ(regular->GetFontDescriptor().post_script_name,
            bold->GetFontDescriptor().post_script_name);

  const GlyphID glyph = regular->UnicharToGlyph('A');
  Font regular_font(regular, 64);
  Font bold_font(bold, 64);
  const GlyphData* regular_glyph = nullptr;
  const GlyphData* bold_glyph = nullptr;
  regular_font.LoadGlyphPath(&glyph, 1, &regular_glyph);
  bold_font.LoadGlyphPath(&glyph, 1, &bold_glyph);
  ASSERT_NE(regular_glyph, nullptr);
  ASSERT_NE(bold_glyph, nullptr);
  EXPECT_FALSE(bold_glyph->GetPath().IsEmpty());
  EXPECT_NE(regular_glyph->GetPath().GetBounds(),
            bold_glyph->GetPath().GetBounds());

  regular_font.SetEmbolden(true);
  const GlyphData* explicit_bold_glyph = nullptr;
  regular_font.LoadGlyphPath(&glyph, 1, &explicit_bold_glyph);
  ASSERT_NE(explicit_bold_glyph, nullptr);
  const Path& expected = explicit_bold_glyph->GetPath();
  const Path& actual = bold_glyph->GetPath();
  ASSERT_EQ(expected.CountPoints(), actual.CountPoints());
  ASSERT_EQ(expected.CountVerbs(), actual.CountVerbs());
  EXPECT_TRUE(std::equal(expected.Points(),
                         expected.Points() + expected.CountPoints(),
                         actual.Points()));
  EXPECT_TRUE(std::equal(expected.VerbsBegin(), expected.VerbsEnd(),
                         actual.VerbsBegin()));
  EXPECT_EQ(
      manager->MatchFamilyStyle("Source Sans Pro", FontStyle())->TypefaceId(),
      regular->TypefaceId());
}

TEST_F(FontManagerFontConfigTest, ConcurrentMatchingReusesTypeface) {
  auto reference = manager_->MatchFamilyStyle("Source Sans Pro", FontStyle());
  ASSERT_NE(reference, nullptr);
  std::atomic<int> failures{0};
  std::vector<std::thread> threads;
  for (int i = 0; i < 8; ++i) {
    threads.emplace_back([&] {
      for (int j = 0; j < 30; ++j) {
        auto face = manager_->MatchFamilyStyle("Source Sans Pro", FontStyle());
        if (!face || face->TypefaceId() != reference->TypefaceId() ||
            face->UnicharToGlyph('A') == 0) {
          ++failures;
        }
      }
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  EXPECT_EQ(failures.load(), 0);
}

}  // namespace
}  // namespace skity
