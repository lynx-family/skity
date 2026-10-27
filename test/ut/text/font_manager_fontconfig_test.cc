// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/text/ports/linux/font_manager_fontconfig.hpp"

#include <fontconfig/fontconfig.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
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

using NativeConfig = std::unique_ptr<FcConfig, decltype(&FcConfigDestroy)>;
using NativePattern = std::unique_ptr<FcPattern, decltype(&FcPatternDestroy)>;

std::string NativeString(FcPattern* pattern, const char* key) {
  FcChar8* value = nullptr;
  return FcPatternGetString(pattern, key, 0, &value) == FcResultMatch
             ? reinterpret_cast<const char*>(value)
             : "";
}

NativePattern MatchNative(FcConfig* config, Unichar character) {
  NativePattern request(FcPatternCreate(), FcPatternDestroy);
  std::unique_ptr<FcCharSet, decltype(&FcCharSetDestroy)> charset(
      FcCharSetCreate(), FcCharSetDestroy);
  if (!request || !charset ||
      !FcPatternAddInteger(request.get(), FC_WEIGHT, FC_WEIGHT_REGULAR) ||
      !FcPatternAddInteger(request.get(), FC_WIDTH, FC_WIDTH_NORMAL) ||
      !FcPatternAddInteger(request.get(), FC_SLANT, FC_SLANT_ROMAN) ||
      !FcCharSetAddChar(charset.get(), character) ||
      !FcPatternAddCharSet(request.get(), FC_CHARSET, charset.get()) ||
      !FcConfigSubstitute(config, request.get(), FcMatchPattern)) {
    return NativePattern(nullptr, FcPatternDestroy);
  }
  FcDefaultSubstitute(request.get());
  FcResult result;
  return NativePattern(FcFontMatch(config, request.get(), &result),
                       FcPatternDestroy);
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
        // This Latin alias tests weak family expansion, independent of the
        // host locale. CJK locale behavior has separate fresh-process fixtures.
        << R"(<match target="pattern">
          <test name="family"><string>Harness Weak</string></test>
          <edit name="lang" mode="assign_replace"><string>en</string></edit>
        </match>)"
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
  // A null family allows the locale-dependent default; named missing families
  // above must still reject unrelated fallback.
  auto face = manager_->GetDefaultTypeface(FontStyle());
  ASSERT_NE(face, nullptr);
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

TEST_F(FontManagerFontConfigTest, NamedFamilyDoesNotInjectUnrelatedGeneric) {
  const auto config = WriteConfig("named-family.conf", true, R"(
    <alias>
      <family>Noto Serif</family>
      <default><family>serif</family></default>
    </alias>
    <match target="pattern">
      <test name="family"><string>sans-serif</string></test>
      <edit name="family" mode="append_last" binding="strong">
        <string>Source Sans Pro</string>
      </edit>
    </match>)");
  auto manager = MakeFontManagerFontConfig(config.c_str());
  ASSERT_NE(manager, nullptr);
  auto styles = manager->MatchFamily("Noto Serif");
  ASSERT_EQ(styles->Count(), 1);
  auto face = styles->MatchStyle(FontStyle());
  ASSERT_NE(face, nullptr);
  EXPECT_EQ(face->GetFontDescriptor().family_name, "Noto Serif");
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
    <match target="pattern">
      <test name="family"><string>Harness Bold</string></test>
      <edit name="lang" mode="assign_replace"><string>en</string></edit>
    </match>
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

TEST_F(FontManagerFontConfigTest, ExplicitConfigOverridesRegionalPreference) {
  auto config = WriteConfig("regional-override.conf", true, R"(
    <match target="pattern">
      <test name="lang" compare="contains"><string>zh-CN</string></test>
      <edit name="family" mode="assign_replace" binding="strong">
        <string>Noto Sans CJK TC</string>
      </edit>
    </match>)");
  auto manager = MakeFontManagerFontConfig(config.c_str());
  ASSERT_NE(manager, nullptr);
  const char* languages[] = {"zh-CN"};
  auto face = manager->MatchFamilyStyleCharacter(nullptr, FontStyle(),
                                                 languages, 1, 0x589E);
  ASSERT_NE(face, nullptr);
  EXPECT_EQ(face->GetFontDescriptor().family_name, "Noto Sans CJK TC");
  EXPECT_EQ(face->GetFontDescriptor().collection_index, 3);
}

struct CjkRegionCase {
  const char* name;
  const char* system_locale;
  const char* language;
  const char* family;
  const char* expected_family;
  int expected_index;
  const char* file = "NotoSansCJK-Regular.ttc";
  Unichar character = 0x589E;
  bool match_family_only = false;
};

class FontConfigEnvironmentTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Re-exec for every case: even older Fontconfig versions cache environment
    // languages globally, and RefDefault() caches its manager for the process.
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    std::string directory =
        (std::filesystem::temp_directory_path() / "skity-cjk-region-XXXXXX")
            .string();
    char* created = mkdtemp(directory.data());
    ASSERT_NE(created, nullptr);
    root_ = created;
    std::filesystem::create_directory(root_ / "cache");
  }

  void TearDown() override {
    if (!root_.empty()) {
      std::error_code error;
      std::filesystem::remove_all(root_, error);
    }
  }

  void SetEnvironment(const char* locale) {
    // Set these inside the fresh test process, after Python's locale coercion.
    // Never change the parent test runner's environment or system
    // configuration.
    for (const char* name :
         {"FC_LANG", "LC_ALL", "LC_CTYPE", "LANGUAGE", "FONTCONFIG_FILE",
          "FONTCONFIG_PATH", "FONTCONFIG_SYSROOT"}) {
      ASSERT_EQ(unsetenv(name), 0);
    }
    ASSERT_EQ(setenv("LANG", locale, 1), 0);
    ASSERT_EQ(setenv("XDG_CACHE_HOME", (root_ / "cache").c_str(), 1), 0);
  }

  void CheckSystemFonts() {
    SetEnvironment("en_US.UTF-8");
    ASSERT_FALSE(::testing::Test::HasFatalFailure());
    const auto config_home = root_ / "config";
    std::filesystem::create_directory(config_home);
    ASSERT_EQ(setenv("XDG_CONFIG_HOME", config_home.c_str(), 1), 0);
    NativeConfig baseline(FcInitLoadConfigAndFonts(), FcConfigDestroy);
    ASSERT_NE(baseline, nullptr);

    // No external regional rule is installed in the empty user config home.
    auto manager = FontManager::RefDefault();
    ASSERT_NE(manager, nullptr);
    const auto info = GetDefaultFontConfigInfo();
    ASSERT_TRUE(info.initialized);
    std::set<std::string> files;
    for (FcSetName set : {FcSetSystem, FcSetApplication}) {
      auto* fonts = FcConfigGetFonts(baseline.get(), set);
      if (fonts) {
        for (int i = 0; i < fonts->nfont; ++i) {
          files.insert(NativeString(fonts->fonts[i], FC_FILE));
        }
      }
    }
    EXPECT_EQ(
        std::set<std::string>(info.font_files.begin(), info.font_files.end()),
        files);
    // The same system configuration still supplies the original Latin fallback.
    auto expected = MatchNative(baseline.get(), 'A');
    auto actual = manager->MatchFamilyStyleCharacter(nullptr, FontStyle(),
                                                     nullptr, 0, 'A');
    if (!expected) {
      EXPECT_EQ(actual, nullptr);
      return;
    }
    ASSERT_NE(actual, nullptr);
    EXPECT_EQ(actual->GetFontDescriptor().family_name,
              NativeString(expected.get(), FC_FAMILY));
    int index = -1;
    ASSERT_EQ(FcPatternGetInteger(expected.get(), FC_INDEX, 0, &index),
              FcResultMatch);
    EXPECT_EQ(actual->GetFontDescriptor().collection_index, index);
    auto reference = manager->MakeFromFile(
        NativeString(expected.get(), FC_FILE).c_str(), index);
    ASSERT_NE(reference, nullptr);
    EXPECT_EQ(actual->UnicharToGlyph('A'), reference->UnicharToGlyph('A'));
  }

  void ExitChild() {
    TearDown();
    // Preserve assertion details in the runner's structured failure report.
    const auto* result =
        ::testing::UnitTest::GetInstance()->current_test_info()->result();
    for (int i = 0; i < result->total_part_count(); ++i) {
      const auto& part = result->GetTestPartResult(i);
      if (part.failed()) {
        std::fprintf(stderr, "%s\n", part.message());
      }
    }
    std::exit(::testing::Test::HasFailure() ? 1 : 0);
  }

  std::filesystem::path root_;
};

class FontConfigCjkTest : public FontConfigEnvironmentTest {
 protected:
  void SetUp() override {
    FontConfigEnvironmentTest::SetUp();
    ASSERT_FALSE(::testing::Test::HasFatalFailure());
    std::filesystem::create_directory(root_ / "fonts");
    std::filesystem::copy_file(
        std::filesystem::path(SKITY_TEST_FONTCONFIG_FIXTURE_ROOT) /
            "fonts.conf",
        root_ / "fonts.conf");
    for (const char* name :
         {"NotoSansCJK-Regular.ttc", "SourceSansPro-Regular.ttf"}) {
      std::filesystem::create_symlink(
          std::filesystem::path(SKITY_TEST_FONT_ROOT) / name,
          root_ / "fonts" / name);
    }
  }

  void UseTestFonts(const char* locale) {
    SetEnvironment(locale);
    ASSERT_FALSE(::testing::Test::HasFatalFailure());
    ASSERT_EQ(setenv("FONTCONFIG_FILE", (root_ / "fonts.conf").c_str(), 1), 0);
  }

  void CheckFace(const std::shared_ptr<FontManager>& manager,
                 const std::shared_ptr<Typeface>& face,
                 const char* expected_family, int expected_index,
                 const char* file = "NotoSansCJK-Regular.ttc",
                 Unichar character = 0x589E) {
    ASSERT_NE(face, nullptr);
    EXPECT_EQ(face->GetFontDescriptor().family_name, expected_family);
    EXPECT_EQ(face->GetFontDescriptor().collection_index, expected_index);

    // Compare the actual loaded face/data/glyphs with explicit TTC loading.
    // Glyph IDs may change when the fixture font is updated.
    const auto path = std::filesystem::path(SKITY_TEST_FONT_ROOT) / file;
    auto reference = manager->MakeFromFile(path.c_str(), expected_index);
    ASSERT_NE(reference, nullptr);
    const auto actual_data = face->GetData();
    const auto expected_data = reference->GetData();
    ASSERT_NE(actual_data, nullptr);
    ASSERT_NE(expected_data, nullptr);
    ASSERT_EQ(actual_data->Size(), expected_data->Size());
    EXPECT_EQ(std::memcmp(actual_data->RawData(), expected_data->RawData(),
                          actual_data->Size()),
              0);
    EXPECT_NE(face->UnicharToGlyph(character), 0);
    EXPECT_EQ(face->UnicharToGlyph(character),
              reference->UnicharToGlyph(character));
    if (character == 0x589E) {
      for (Unichar character :
           {0x589Eu, 0x52A0u, 0x5217u, 0x8868u, 0x5143u, 0x7D20u}) {
        EXPECT_NE(face->UnicharToGlyph(character), 0);
        EXPECT_EQ(face->UnicharToGlyph(character),
                  reference->UnicharToGlyph(character));
      }
    }
  }

  void CheckDistroAliases(bool user_override) {
    UseTestFonts("zh_CN.UTF-8");
    ASSERT_FALSE(::testing::Test::HasFatalFailure());
    const auto fixture =
        std::filesystem::path(SKITY_TEST_FONTCONFIG_FIXTURE_ROOT) /
        "distro-cjk.conf";
    const auto config_home = root_ / "config";
    std::filesystem::create_directories(config_home / "fontconfig");
    ASSERT_EQ(setenv("XDG_CONFIG_HOME", config_home.c_str(), 1), 0);
    if (user_override) {
      std::ofstream user_config(config_home / "fontconfig/fonts.conf");
      user_config << R"(<fontconfig>
        <match target="pattern">
          <edit name="family" mode="assign_replace" binding="strong">
            <string>Noto Sans CJK TC</string>
          </edit>
        </match>
      </fontconfig>)";
    }
    const auto path = root_ / "distro-fonts.conf";
    {
      std::ofstream config(path);
      config
          << "<fontconfig><include ignore_missing=\"no\">"
          << EscapeXml((root_ / "fonts.conf").string()) << "</include>"
          << "<include ignore_missing=\"no\">" << EscapeXml(fixture.string())
          << "</include>"
          << R"(<include ignore_missing="yes" prefix="xdg">fontconfig/fonts.conf</include>)"
          << "</fontconfig>";
    }
    ASSERT_EQ(setenv("FONTCONFIG_FILE", path.c_str(), 1), 0);
    auto manager = FontManager::RefDefault();
    ASSERT_NE(manager, nullptr);
    // Repeat the default request after an explicit locale to catch cached or
    // process-global language changes as well as distro strong-family
    // conflicts.
    const char* requests[] = {nullptr, "ja", nullptr};
    for (const char* language : requests) {
      SCOPED_TRACE(language ? language : "<default>");
      const char* languages[] = {language};
      auto face = manager->MatchFamilyStyleCharacter(
          nullptr, FontStyle(400, 5, FontStyle::kUpright_Slant),
          language ? languages : nullptr, language ? 1 : 0, 0x589E);
      CheckFace(manager, face,
                user_override ? "Noto Sans CJK TC"
                : language    ? "Noto Sans CJK JP"
                              : "Noto Sans CJK SC",
                user_override ? 3
                : language    ? 0
                              : 2);
      ASSERT_FALSE(::testing::Test::HasFatalFailure());
    }
    EXPECT_STREQ(std::getenv("LANG"), "zh_CN.UTF-8");
    EXPECT_EQ(std::getenv("FC_LANG"), nullptr);
    EXPECT_EQ(std::getenv("LC_ALL"), nullptr);
    const auto info = GetDefaultFontConfigInfo();
    EXPECT_NE(std::find(info.config_files.begin(), info.config_files.end(),
                        fixture.string()),
              info.config_files.end());
    if (user_override) {
      EXPECT_NE(std::find(info.config_files.begin(), info.config_files.end(),
                          (config_home / "fontconfig/fonts.conf").string()),
                info.config_files.end());
    }
  }

  void CheckPrivateConfig() {
    UseTestFonts("zh_CN.UTF-8");
    ASSERT_FALSE(::testing::Test::HasFatalFailure());
    NativeConfig global(FcConfigReference(FcConfigGetCurrent()),
                        FcConfigDestroy);
    ASSERT_NE(global, nullptr);
    auto before = MatchNative(global.get(), 0x589E);
    ASSERT_NE(before, nullptr);

    auto manager = FontManager::RefDefault();
    ASSERT_NE(manager, nullptr);
    auto face = manager->MatchFamilyStyleCharacter(nullptr, FontStyle(),
                                                   nullptr, 0, 0x589E);
    ASSERT_NE(face, nullptr);
    EXPECT_EQ(face->GetFontDescriptor().family_name, "Noto Sans CJK SC");
    EXPECT_EQ(face->GetFontDescriptor().collection_index, 2);

    // The pre-existing process-global config and newly created native configs
    // must both retain the native result after Skity has initialized and
    // matched.
    EXPECT_EQ(FcConfigGetCurrent(), global.get());
    auto after = MatchNative(global.get(), 0x589E);
    ASSERT_NE(after, nullptr);
    EXPECT_TRUE(FcPatternEqual(before.get(), after.get()));
    NativeConfig independent(FcInitLoadConfigAndFonts(), FcConfigDestroy);
    ASSERT_NE(independent, nullptr);
    auto separate = MatchNative(independent.get(), 0x589E);
    ASSERT_NE(separate, nullptr);
    EXPECT_TRUE(FcPatternEqual(before.get(), separate.get()));
    EXPECT_EQ(std::string(std::getenv("FONTCONFIG_FILE")),
              (root_ / "fonts.conf").string());
    EXPECT_EQ(std::getenv("FONTCONFIG_PATH"), nullptr);
  }
};

class FontConfigCjkRegionTest
    : public FontConfigCjkTest,
      public ::testing::WithParamInterface<CjkRegionCase> {
 protected:
  void CheckMatch() {
    const auto& param = GetParam();
    UseTestFonts(param.system_locale);
    ASSERT_FALSE(::testing::Test::HasFatalFailure());

    auto manager = FontManager::RefDefault();
    ASSERT_NE(manager, nullptr);
    const char* languages[] = {param.language};
    const FontStyle style(400, 5, FontStyle::kUpright_Slant);
    auto face =
        param.match_family_only
            ? manager->MatchFamilyStyle(param.family, style)
            : manager->MatchFamilyStyleCharacter(
                  param.family, style, param.language ? languages : nullptr,
                  param.language ? 1 : 0, param.character);
    CheckFace(manager, face, param.expected_family, param.expected_index,
              param.file, param.character);
  }
};

TEST_P(FontConfigCjkRegionTest, SelectsFace) {
  ASSERT_EXIT(
      {
        CheckMatch();
        ExitChild();
      },
      ::testing::ExitedWithCode(0), "");
}

TEST_F(FontConfigEnvironmentTest, AutomaticRulesPreserveSystemFontInventory) {
  ASSERT_EXIT(
      {
        CheckSystemFonts();
        ExitChild();
      },
      ::testing::ExitedWithCode(0), "");
}

TEST_F(FontConfigCjkTest, AutomaticRulesDoNotModifyOtherFontconfigUsers) {
  ASSERT_EXIT(
      {
        CheckPrivateConfig();
        ExitChild();
      },
      ::testing::ExitedWithCode(0), "");
}

TEST_F(FontConfigCjkTest, DistroAliasesHonorExplicitLanguage) {
  ASSERT_EXIT(
      {
        CheckDistroAliases(false);
        ExitChild();
      },
      ::testing::ExitedWithCode(0), "");
}

TEST_F(FontConfigCjkTest, UserConfigStillOverridesDistroAliases) {
  ASSERT_EXIT(
      {
        CheckDistroAliases(true);
        ExitChild();
      },
      ::testing::ExitedWithCode(0), "");
}

INSTANTIATE_TEST_SUITE_P(
    RegionalFallback, FontConfigCjkRegionTest,
    ::testing::Values(
        CjkRegionCase{"DefaultChinese", "zh_CN.UTF-8", nullptr, nullptr,
                      "Noto Sans CJK SC", 2},
        CjkRegionCase{"DefaultJapanese", "ja_JP.UTF-8", nullptr, nullptr,
                      "Noto Sans CJK JP", 0},
        CjkRegionCase{"DefaultTraditionalChinese", "zh_TW.UTF-8", nullptr,
                      nullptr, "Noto Sans CJK TC", 3},
        CjkRegionCase{"DefaultKorean", "ko_KR.UTF-8", nullptr, nullptr,
                      "Noto Sans CJK KR", 1},
        CjkRegionCase{"SansSerifChinese", "zh_CN.UTF-8", nullptr, "sans-serif",
                      "Noto Sans CJK SC", 2},
        CjkRegionCase{"SansSerifJapanese", "ja_JP.UTF-8", nullptr, "sans-serif",
                      "Noto Sans CJK JP", 0},
        CjkRegionCase{"JapaneseOverridesChinese", "zh_CN.UTF-8", "ja", nullptr,
                      "Noto Sans CJK JP", 0},
        CjkRegionCase{"JapaneseOverridesTraditionalChinese", "zh_TW.UTF-8",
                      "ja", nullptr, "Noto Sans CJK JP", 0},
        CjkRegionCase{"JapaneseOverridesKorean", "ko_KR.UTF-8", "ja", nullptr,
                      "Noto Sans CJK JP", 0},
        CjkRegionCase{"ChineseOverridesJapanese", "ja_JP.UTF-8", "zh-CN",
                      nullptr, "Noto Sans CJK SC", 2},
        CjkRegionCase{"HansOverridesJapanese", "ja_JP.UTF-8", "zh-Hans",
                      nullptr, "Noto Sans CJK SC", 2},
        CjkRegionCase{"TraditionalChineseOverridesChinese", "zh_CN.UTF-8",
                      "zh-TW", nullptr, "Noto Sans CJK TC", 3},
        CjkRegionCase{"HantOverridesChinese", "zh_CN.UTF-8", "zh-Hant", nullptr,
                      "Noto Sans CJK TC", 3},
        CjkRegionCase{"KoreanOverridesJapanese", "ja_JP.UTF-8", "ko", nullptr,
                      "Noto Sans CJK KR", 1},
        CjkRegionCase{"PreservesExplicitCjkFamily", "zh_CN.UTF-8", "zh-CN",
                      "Noto Sans CJK JP", "Noto Sans CJK JP", 0},
        CjkRegionCase{"PreservesStrongLatinFamily", "zh_CN.UTF-8", nullptr,
                      "Source Sans Pro", "Source Sans Pro", 0,
                      "SourceSansPro-Regular.ttf", 'A', true},
        CjkRegionCase{"LanguagePrecedesWeakLatinFamily", "zh_CN.UTF-8", "ja",
                      "Source Sans Pro", "Noto Sans CJK JP", 0,
                      "NotoSansCJK-Regular.ttc", 'A'},
        CjkRegionCase{"MissingFamilyUsesRegionalFallback", "ja_JP.UTF-8",
                      "zh-CN", "Missing Family", "Noto Sans CJK SC", 2}),
    [](const ::testing::TestParamInfo<CjkRegionCase>& info) {
      return info.param.name;
    });

}  // namespace
}  // namespace skity
