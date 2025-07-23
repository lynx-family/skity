// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <cstddef>
#include <memory>
#include <skity/text/font_arguments.hpp>
#include <skity/text/font_manager.hpp>
#include <skity/text/typeface.hpp>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "src/logging.hpp"
#include "src/text/ports/harmony/harmony_fonts_parser.hpp"
#include "src/text/ports/typeface_freetype.hpp"
#include "src/utils/no_destructor.hpp"

namespace skity {

class FontStyleSetHarmony : public FontStyleSet {
 public:
  explicit FontStyleSetHarmony(
      const std::unordered_map<std::string, std::shared_ptr<Data>>& data_cache,
      const std::vector<FontInfo>& fonts);

  int Count() override { return typefaces_freetype_.size(); }

  void GetStyle(int index, FontStyle* style, std::string* name) override {
    if (index < 0 || typefaces_freetype_.size() <= static_cast<size_t>(index)) {
      return;
    }
    if (style) {
      *style = typefaces_freetype_[index]->GetFontStyle();
    }
    if (name) {
      const char* names[] = {"invisible", "thin",   "extralight", "light",
                             "normal",    "medium", "semibold",   "bold",
                             "extrabold", "black",  "extrablack"};
      unsigned int i =
          typefaces_freetype_[index]->GetFontStyle().weight() / 100;
      if (i < sizeof(names) / sizeof(char*)) {
        *name = names[i];
      } else {
        name->clear();
      }
    }
  }

  Typeface* CreateTypeface(int index) override {
    if (index < 0 || typefaces_freetype_.size() <= static_cast<size_t>(index)) {
      return nullptr;
    }
    return typefaces_freetype_[index].get();
  }

  Typeface* MatchStyle(const FontStyle& pattern) override {
    return this->MatchStyleCSS3(pattern);
  }

 private:
  std::vector<std::unique_ptr<TypefaceFreeType>> typefaces_freetype_;
};

FontStyleSetHarmony::FontStyleSetHarmony(
    const std::unordered_map<std::string, std::shared_ptr<Data>>& data_cache,
    const std::vector<FontInfo>& fonts) {
  for (size_t index = 0; index < fonts.size(); ++index) {
    if (data_cache.find(fonts[index].fname) != data_cache.end()) {
      std::shared_ptr<Data> data = data_cache.at(fonts[index].fname);
      std::unique_ptr<TypefaceFreeType> typeface = TypefaceFreeType::Make(
          data, FontArguments().SetCollectionIndex(fonts[index].index));
      if (typeface) {
        typefaces_freetype_.push_back(std::move(typeface));
      }
    }
  }
}

struct NameToFamily {
  std::string name;
  FontStyleSetHarmony* style_set;
};

class FontManagerHarmony : public FontManager {
 public:
  FontManagerHarmony() {
    FontScanner scanner;
    parser_ = std::make_unique<HarmonyFontParser>(scanner);
    BuildNameToFamilyMap();
  }

 protected:
  int OnCountFamilies() const override { return name_to_family_map_.size(); }

  std::string OnGetFamilyName(int index) const override {
    if (index < 0 || name_to_family_map_.size() <= static_cast<size_t>(index)) {
      return "";
    }
    return name_to_family_map_[index].name;
  }

  FontStyleSet* OnCreateStyleSet(int index) const override {
    if (index < 0 || name_to_family_map_.size() <= static_cast<size_t>(index)) {
      return nullptr;
    }
    return name_to_family_map_[index].style_set;
  }

  FontStyleSet* OnMatchFamily(const char familyName[]) const override {
    if (!familyName) {
      return nullptr;
    }

    for (size_t i = 0; i < name_to_family_map_.size(); ++i) {
      if (name_to_family_map_[i].name == familyName) {
        return name_to_family_map_[i].style_set;
      }
    }

    for (size_t i = 0; i < fallback_name_to_family_map_.size(); ++i) {
      if (fallback_name_to_family_map_[i].name == familyName) {
        return fallback_name_to_family_map_[i].style_set;
      }
    }
    return nullptr;
  }

  Typeface* OnMatchFamilyStyle(const char familyName[],
                               const FontStyle& style) const override {
    FontStyleSet* sset(this->MatchFamily(familyName));
    return sset->MatchStyle(style);
  }

  Typeface* OnMatchFamilyStyleCharacter(const char familyName[],
                                        const FontStyle& style, const char*[],
                                        int, Unichar character) const override {
    std::string name_str = familyName == nullptr ? "" : familyName;
    std::vector<FallbackSetPos> fallback_pos(2);
    if (parser_->fallback_for_map_.find("") !=
        parser_->fallback_for_map_.end()) {
      fallback_pos.push_back(parser_->fallback_for_map_.at(""));
    }
    if (!name_str.empty() && parser_->fallback_for_map_.find(name_str) !=
                                 parser_->fallback_for_map_.end()) {
      fallback_pos.push_back(parser_->fallback_for_map_.at(name_str));
    }
    for (auto iter = fallback_pos.crbegin(); iter != fallback_pos.crend();
         iter++) {
      for (size_t index = iter->index; index < iter->index + iter->count;
           index++) {
        Typeface* typeface =
            fallback_name_to_family_map_[index].style_set->MatchStyle(style);
        if (typeface && typeface->UnicharToGlyph(character) != 0) {
          return typeface;
        }
      }
    }
    return nullptr;
  }

  std::unique_ptr<Typeface> OnMakeFromData(std::shared_ptr<Data> const& data,
                                           int ttcIndex) const override {
    return TypefaceFreeType::Make(data,
                                  FontArguments().SetCollectionIndex(ttcIndex));
  }

  std::unique_ptr<Typeface> OnMakeFromFile(const char path[],
                                           int ttcIndex) const override {
    auto data = Data::MakeFromFileName(path);
    return TypefaceFreeType::Make(data,
                                  FontArguments().SetCollectionIndex(ttcIndex));
  }

  Typeface* OnGetDefaultTypeface(FontStyle const& font_style) const override {
    return OnMatchFamilyStyle(default_family_name_.c_str(), font_style);
  }

 private:
  void BuildNameToFamilyMap() {
    for (auto& [family_name, index] : parser_->generic_name_map_) {
      if (index == 0) {
        default_family_name_ = family_name;
      }
      const std::vector<FontInfo>& fonts =
          *parser_->generic_family_set_[index]->font_set.get();
      style_sets_.push_back(
          std::make_unique<FontStyleSetHarmony>(parser_->data_cache, fonts));
      name_to_family_map_.emplace_back(
          NameToFamily{family_name, style_sets_.back().get()});
    }
    for (auto& fallback : parser_->fallback_set_) {
      const std::vector<FontInfo>& fonts = *fallback->font_set.get();
      style_sets_.push_back(
          std::make_unique<FontStyleSetHarmony>(parser_->data_cache, fonts));
      fallback_name_to_family_map_.emplace_back(
          NameToFamily{fallback->familyName, style_sets_.back().get()});
    }
  }

  std::string default_family_name_;

  std::vector<std::unique_ptr<FontStyleSetHarmony>> style_sets_;
  std::vector<NameToFamily> name_to_family_map_;
  std::vector<NameToFamily> fallback_name_to_family_map_;
  std::unique_ptr<HarmonyFontParser> parser_;
};

std::shared_ptr<FontManager> FontManager::RefDefault() {
  static const NoDestructor<std::shared_ptr<FontManager>> font_manager(
      [] { return std::make_shared<FontManagerHarmony>(); }());
  return *font_manager;
}

}  // namespace skity
