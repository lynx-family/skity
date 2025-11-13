// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/text/ports/test/font_manager_test.hpp"

#include <cstddef>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "src/utils/no_destructor.hpp"

#ifndef SKITY_WASM
#include "third_party/jsoncpp/include/json/json.h"

#define DEFAULT_FONT_NAME "sans-serif"
#endif

namespace skity {

#ifndef SKITY_WASM
// ---------- FontFileInfo ----------
static FontFileInfo FontFileInfoFromJson(const Json::Value& value) {
  FontFileInfo info;

  info.file_name = value["file_name"].asString();
  info.index = value["index"].asInt();
  info.weight = value["weight"].asInt();
  info.style = static_cast<FontFileInfo::Style>(value["style"].asInt());

  const auto& axis = value["axis_tags"];
  for (auto it = axis.begin(); it != axis.end(); ++it) {
    info.axis_tags[it.name()] = it->asFloat();
  }

  return info;
}

// ---------- FontFamily ----------
static std::unique_ptr<FontFamily> FontFamilyFromJson(
    const Json::Value& value) {
  auto family = std::make_unique<FontFamily>("", false);

  for (const auto& name : value["names"]) {
    family->names.push_back(name.asString());
  }

  for (const auto& lang : value["languages"]) {
    family->languages.push_back(lang.asString());
  }

  for (const auto& fontVal : value["fonts"]) {
    family->fonts.push_back(FontFileInfoFromJson(fontVal));
  }

  const auto& fallbacks = value["fallback_families"];
  for (auto it = fallbacks.begin(); it != fallbacks.end(); ++it) {
    family->fallback_families[it.name()] = FontFamilyFromJson(*it);
  }

  family->variant = static_cast<FontVariants>(value["variant"].asInt());

  family->order = value.get("order", -1).asInt();
  family->base_path = value.get("base_path", "").asString();
  family->is_fallback_font = value.get("is_fallback_font", false).asBool();
  family->fallback_for = value.get("fallback_for", "").asString();

  return family;
}

// ---------- 顶层入口 ----------
static std::vector<FontFamily> LoadFontFamiliesFromJsonFile(
    const std::string& filename) {
  std::ifstream ifs(filename);
  if (!ifs.is_open()) {
    return {};
  }

  Json::CharReaderBuilder builder;
  Json::Value root;
  std::string errs;
  if (!Json::parseFromStream(builder, ifs, &root, &errs)) {
    return {};
  }

  std::vector<FontFamily> result;
  const Json::Value* arr = nullptr;
  if (root.isArray()) {
    arr = &root;
  } else if (root.isObject() && root.isMember("font_families") &&
             root["font_families"].isArray()) {
    arr = &root["font_families"];
  } else {
    return {};
  }

  for (const auto& val : *arr) {
    auto famptr = FontFamilyFromJson(val);
    result.push_back(std::move(*famptr));
  }

  return result;
}

FaceData TypefaceFreeTypeTest::OnGetFaceData() const {
  FaceData face_data;
  face_data.data = Data::MakeFromFileMapping(font_file_.c_str());
  face_data.font_args = font_args_;
  return face_data;
}

FontStyleSetTest::FontStyleSetTest(const FontFamily& family) {
  fallback_for_ = family.fallback_for;
  for (size_t index = 0; index < family.fonts.size(); ++index) {
    const FontFileInfo& font_file = family.fonts[index];
    std::string full_path =
        SKITY_FONT_DIR + family.base_path + font_file.file_name;

    auto data = Data::MakeFromFileMapping(full_path.c_str());
    if (!data) {
      continue;
    }

    FontArguments font_args;
    font_args.SetCollectionIndex(font_file.index);
    FontStyle font_style;
    bool use_xml_style = true;
    if (font_file.axis_tags.empty()) {
      std::shared_ptr<TypefaceFreeType> typeface =
          TypefaceFreeType::Make(data, font_args);
      if (!typeface) {
        continue;
      }
      font_style = typeface->GetFontStyle();
    } else {
      std::shared_ptr<TypefaceFreeType> typeface =
          TypefaceFreeType::Make(data, font_args);
      if (!typeface) {
        continue;
      }
      if (typeface->IsVariationTypeface()) {
        VariationPosition position;
        for (auto& axis : font_file.axis_tags) {
          const char* chars = axis.first.c_str();
          position.AddCoordinate(
              SetFourByteTag(chars[0], chars[1], chars[2], chars[3]),
              axis.second);
        }
        font_args.SetVariationDesignPosition(position);
        auto variation_face = typeface->MakeVariation(font_args);
        if (!variation_face) {
          continue;
        }
        TypefaceFreeType* variation_ft_face =
            static_cast<TypefaceFreeType*>(variation_face.get());
        font_style = variation_ft_face->GetFontStyle();
        font_args = variation_ft_face->GetFaceData().font_args;
      } else {
        use_xml_style = false;
        font_style = typeface->GetFontStyle();
      }
    }

    int xml_weight =
        font_file.weight != 0 ? font_file.weight : font_style.weight();
    FontStyle::Slant xml_slant = font_style.slant();
    if (font_file.style == FontFileInfo::Style::kNormal) {
      xml_slant = FontStyle::kUpright_Slant;
    } else if (font_file.style == FontFileInfo::Style::kItalic) {
      xml_slant = FontStyle::kItalic_Slant;
    }
    FontStyle xml_font_style(xml_weight, font_style.width(), xml_slant);

    std::unique_ptr<TypefaceFreeType> typeface_test =
        std::make_unique<TypefaceFreeTypeTest>(
            full_path, family.languages, family.variant, font_args,
            use_xml_style ? xml_font_style : font_style);
    typefaces_freetype_.push_back(std::move(typeface_test));
  }
}

void FontStyleSetTest::GetStyle(int index, FontStyle* style,
                                std::string* name) {
  if (index < 0 || typefaces_freetype_.size() <= static_cast<size_t>(index)) {
    return;
  }
  if (style) {
    *style = typefaces_freetype_[index]->GetFontStyle();
  }
  if (name) {
    name->clear();
  }
}

FontManagerTest::FontManagerTest() {
  std::vector<FontFamily> font_families =
      LoadFontFamiliesFromJsonFile(SKITY_FONT_DIR "fonts/config/fonts.json");
  if (!font_families.empty()) {
    BuildNameToFamilyMap(font_families);
  }
}

void FontManagerTest::BuildNameToFamilyMap(
    std::vector<FontFamily>& font_families) {
  size_t index = 0;
  for (FontFamily& family : font_families) {
    AddFamily(family, index++);
    for (const auto& [unused, fallbackFamily] : family.fallback_families) {
      AddFamily(*fallbackFamily, index++);
    }
  }
}

void FontManagerTest::AddFamily(FontFamily& family, size_t index) {
  std::vector<NameToFamily>* nameToFamily = &name_to_family_map_;
  if (family.is_fallback_font) {
    nameToFamily = &fallback_name_to_family_map_;
    if (0 == family.names.size()) {
      std::stringstream stream;
      stream << '0';
      stream << std::hex << index;
      std::string result(stream.str());
      if (result.size() > 2) {
        result.erase(0, result.size() - 2);
      }
      family.names.emplace_back(result + "##fallback");
    }
  }

  std::shared_ptr<FontStyleSetTest> newSet =
      std::make_shared<FontStyleSetTest>(family);
  if (0 == newSet->Count()) {
    return;
  }

  for (const std::string& name : family.names) {
    nameToFamily->emplace_back(NameToFamily{name, newSet});
  }
  style_sets_.emplace_back(std::move(newSet));
}

int FontManagerTest::OnCountFamilies() const {
  return name_to_family_map_.size();
}

std::string FontManagerTest::OnGetFamilyName(int index) const {
  if (index < 0 || name_to_family_map_.size() <= static_cast<size_t>(index)) {
    return "";
  }
  return name_to_family_map_[index].name;
}

std::shared_ptr<FontStyleSet> FontManagerTest::OnCreateStyleSet(
    int index) const {
  if (index < 0 || name_to_family_map_.size() <= static_cast<size_t>(index)) {
    return nullptr;
  }
  return name_to_family_map_[index].styleSet;
}

std::shared_ptr<FontStyleSet> FontManagerTest::OnMatchFamily(
    const char familyName[]) const {
  if (!familyName) {
    return nullptr;
  }
  std::string name(familyName);
  std::transform(name.begin(), name.end(), name.begin(),
                 [](char c) { return (c & 0x80) ? c : ::tolower(c); });

  for (size_t i = 0; i < name_to_family_map_.size(); ++i) {
    if (name_to_family_map_[i].name == name) {
      return name_to_family_map_[i].styleSet;
    }
  }

  for (size_t i = 0; i < fallback_name_to_family_map_.size(); ++i) {
    if (fallback_name_to_family_map_[i].name == name) {
      return fallback_name_to_family_map_[i].styleSet;
    }
  }
  return nullptr;
}

std::shared_ptr<Typeface> FontManagerTest::OnMatchFamilyStyle(
    const char familyName[], const FontStyle& style) const {
  std::shared_ptr<FontStyleSet> sset(this->MatchFamily(familyName));
  return sset->MatchStyle(style);
}

std::shared_ptr<Typeface> find_family_style_character(
    const std::string& family_name,
    const std::vector<NameToFamily>& fallback_map, const FontStyle& style,
    bool elegant, const std::vector<std::string>& lang_patterns,
    Unichar character) {
  auto match_language = [](const std::vector<std::string>& font_langs,
                           const std::vector<std::string>& patterns) -> bool {
    if (patterns.empty()) {
      return true;
    }
    bool match = false;
    for (const auto& pattern : patterns) {
      if (pattern.empty()) {
        match = true;
        break;
      }
      if (std::any_of(font_langs.begin(), font_langs.end(),
                      [&pattern](const std::string& lang) {
                        return lang.rfind(pattern.c_str(), 0) == 0;
                      })) {
        match = true;
        break;
      }
    }
    return match;
  };
  for (size_t i = 0; i < fallback_map.size(); ++i) {
    std::shared_ptr<FontStyleSetTest> style_set = fallback_map[i].styleSet;
    if (style_set->fallback_for_ != family_name) {
      continue;
    }
    std::shared_ptr<Typeface> typeface = style_set->MatchStyle(style);
    std::shared_ptr<TypefaceFreeTypeTest> typeface_android =
        std::static_pointer_cast<TypefaceFreeTypeTest>(typeface);

    if (!match_language(typeface_android->GetFontLanguages(), lang_patterns)) {
      continue;
    }

    if ((typeface_android->GetFontVariants() ==
         FontVariants::kElegant_FontVariant) != elegant) {
      continue;
    }

    if (typeface->UnicharToGlyph(character) != 0) {
      return typeface;
    }
  }
  return nullptr;
}

std::shared_ptr<Typeface> FontManagerTest::OnMatchFamilyStyleCharacter(
    const char familyName[], const FontStyle& style, const char* bcp47[],
    int bcp47_count, Unichar character) const {
  // split language pattern
  std::vector<std::string> lang_patterns;
  for (int bcp47_index = bcp47_count; bcp47_index-- > 0;) {
    const char* pattern = bcp47[bcp47_index];
    if (pattern == nullptr) {
      continue;
    }
    size_t pattern_len = strlen(pattern);
    lang_patterns.emplace_back(pattern, pattern_len);

    const char* parent = pattern;
    while (auto parent_pos = strrchr(parent, '-')) {
      size_t parent_len = parent_pos - parent;
      lang_patterns.emplace_back(pattern, parent_len);
      parent = lang_patterns.back().c_str();
    }
  }

  std::vector<std::string> family_names;
  if (familyName && strlen(familyName) > 0) {
    family_names.emplace_back(familyName);
  }
  family_names.emplace_back("");

  for (const auto& family_name : family_names) {
    for (int elegant = 2; elegant-- > 0;) {
      std::shared_ptr<Typeface> matching_typeface = find_family_style_character(
          family_name, fallback_name_to_family_map_, style,
          static_cast<bool>(elegant), lang_patterns, character);
      if (matching_typeface) {
        return std::move(matching_typeface);
      }
    }
  }

  return nullptr;
}

std::shared_ptr<Typeface> FontManagerTest::OnGetDefaultTypeface(
    const FontStyle& font_style) const {
  return OnMatchFamilyStyle(DEFAULT_FONT_NAME, font_style);
}

#endif

std::shared_ptr<Typeface> FontManagerTest::OnMakeFromData(
    std::shared_ptr<Data> const& data, int ttcIndex) const {
  return TypefaceFreeType::Make(data,
                                FontArguments().SetCollectionIndex(ttcIndex));
}

std::shared_ptr<Typeface> FontManagerTest::OnMakeFromFile(const char path[],
                                                          int ttcIndex) const {
  auto data = Data::MakeFromFileMapping(path);
  return this->OnMakeFromData(data, ttcIndex);
}

std::shared_ptr<FontManager> FontManager::RefDefault() {
  static const NoDestructor<std::shared_ptr<FontManagerTest>> font_manager(
      [] { return std::make_shared<FontManagerTest>(); }());
  return *font_manager;
}

}  // namespace skity
