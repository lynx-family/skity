// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_TEXT_PORTS_TEST_FONT_MANAGER_TEST_HPP
#define SRC_TEXT_PORTS_TEST_FONT_MANAGER_TEST_HPP

#include <skity/text/font_manager.hpp>
#include <skity/text/typeface.hpp>
#include <string>

#include "src/text/ports/android_fonts_parser.hpp"
#include "src/text/ports/typeface_freetype.hpp"

namespace skity {

struct NameToFamily;

class TypefaceFreeTypeTest : public TypefaceFreeType {
 public:
  TypefaceFreeTypeTest(std::string font_file,
                       const std::vector<std::string>& font_languages,
                       const FontVariants variant,
                       const FontArguments& font_args,
                       const FontStyle& font_style)
      : TypefaceFreeType(font_style),
        font_file_(std::move(font_file)),
        font_languages_(font_languages),
        variant_(variant),
        font_args_(font_args) {}
  ~TypefaceFreeTypeTest() override = default;

  const std::vector<std::string>& GetFontLanguages() const {
    return font_languages_;
  }

  FontVariants GetFontVariants() const { return variant_; }

 protected:
  FaceData OnGetFaceData() const override;

 private:
  std::string font_file_;
  std::vector<std::string> font_languages_;
  FontVariants variant_;
  FontArguments font_args_;
};

class FontStyleSetTest : public FontStyleSet {
 public:
  explicit FontStyleSetTest(const FontFamily& family);

  int Count() override { return typefaces_freetype_.size(); }

  void GetStyle(int index, FontStyle* style, std::string* name) override;

  std::shared_ptr<Typeface> CreateTypeface(int index) override {
    if (index < 0 || typefaces_freetype_.size() <= static_cast<size_t>(index)) {
      return nullptr;
    }
    return typefaces_freetype_[index];
  }

  std::shared_ptr<Typeface> MatchStyle(const FontStyle& pattern) override {
    return this->MatchStyleCSS3(pattern);
  }

 private:
  std::vector<std::shared_ptr<TypefaceFreeType>> typefaces_freetype_;
  std::string fallback_for_;

  friend class FontManagerTest;
  friend std::shared_ptr<Typeface> find_family_style_character(
      const std::string& family_name,
      const std::vector<NameToFamily>& fallback_map, const FontStyle& style,
      bool elegant, const std::vector<std::string>& lang_patterns,
      Unichar character);
};

struct NameToFamily {
  std::string name;
  std::shared_ptr<FontStyleSetTest> styleSet;
};

class FontManagerTest : public FontManager {
 public:
  FontManagerTest();

  void SetDefaultTypeface(std::shared_ptr<Typeface> typeface) override {
    // default_typeface_ = typeface;
  }

 protected:
  int OnCountFamilies() const override;

  std::string OnGetFamilyName(int) const override;

  std::shared_ptr<FontStyleSet> OnCreateStyleSet(int) const override;

  std::shared_ptr<FontStyleSet> OnMatchFamily(const char[]) const override;

  std::shared_ptr<Typeface> OnMatchFamilyStyle(const char[],
                                               const FontStyle&) const override;

  std::shared_ptr<Typeface> OnMatchFamilyStyleCharacter(const char[],
                                                        const FontStyle&,
                                                        const char*[], int,
                                                        Unichar) const override;

  std::shared_ptr<Typeface> OnMakeFromData(std::shared_ptr<Data> const& data,
                                           int ttcIndex) const override;

  std::shared_ptr<Typeface> OnMakeFromFile(const char path[],
                                           int ttcIndex) const override;

  std::shared_ptr<Typeface> OnGetDefaultTypeface(
      FontStyle const&) const override;

 private:
  std::vector<std::shared_ptr<FontStyleSetTest>> style_sets_;
  std::vector<NameToFamily> name_to_family_map_;
  std::vector<NameToFamily> fallback_name_to_family_map_;

  void BuildNameToFamilyMap(std::vector<FontFamily>& font_families);
  void AddFamily(FontFamily& family, size_t index);
};

}  // namespace skity

#endif  // SRC_TEXT_PORTS_TEST_FONT_MANAGER_TEST_HPP
