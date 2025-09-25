// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <cstddef>
#include <memory>
#include <skity/text/font_manager.hpp>
#include <skity/text/typeface.hpp>
#include <skity/utils/settings.hpp>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "src/logging.hpp"
#include "src/text/ports/android/ndk_font_api.hpp"
#include "src/text/ports/android_fonts_parser.hpp"
#include "src/text/ports/typeface_freetype.hpp"
#include "src/utils/no_destructor.hpp"

#define DEFAULT_FONT_NAME "sans-serif"
namespace skity {

namespace {

class TypefaceFreeTypeAndroid : public TypefaceFreeType {
 public:
  TypefaceFreeTypeAndroid(std::string font_file,
                          const std::vector<std::string>& font_languages,
                          const FontVariants variant,
                          const FontArguments& font_args,
                          const FontStyle& font_style)
      : TypefaceFreeType(font_style),
        font_file_(std::move(font_file)),
        font_languages_(font_languages),
        variant_(variant),
        font_args_(font_args) {}
  ~TypefaceFreeTypeAndroid() override = default;

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

FaceData TypefaceFreeTypeAndroid::OnGetFaceData() const {
  FaceData face_data;
  face_data.data = Data::MakeFromFileMapping(font_file_.c_str());
  face_data.font_args = font_args_;
  return face_data;
}

class FontStyleSetAndroid : public FontStyleSet {
 public:
  explicit FontStyleSetAndroid(const FontFamily& family) {
    fallback_for_ = family.fallback_for;
    for (size_t index = 0; index < family.fonts.size(); ++index) {
      const FontFileInfo& font_file = family.fonts[index];
      std::string full_path = family.base_path + font_file.file_name;

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

      std::unique_ptr<TypefaceFreeType> typeface_android =
          std::make_unique<TypefaceFreeTypeAndroid>(
              full_path, family.languages, family.variant, font_args,
              use_xml_style ? xml_font_style : font_style);
      typefaces_freetype_.push_back(std::move(typeface_android));
    }
  }

  int Count() override { return typefaces_freetype_.size(); }

  void GetStyle(int index, FontStyle* style, std::string* name) override {
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

  friend class FontManagerAndroid;
};

/** On Android a single family can have many names, but our API assumes unique
 * names. Map names to the back end so that all names for a given family refer
 * to the same (non-replicated) set of typefaces. SkTDict<> doesn't let us do
 * index-based lookup, so we write our own mapping.
 */
struct NameToFamily {
  std::string name;
  std::shared_ptr<FontStyleSetAndroid> styleSet;
};

class FontManagerAndroid : public FontManager {
 public:
  FontManagerAndroid() {
    std::unique_ptr<FontResources> font_resources =
        FontResources::MakeFromFile();
    if (font_resources && !font_resources->FontFamilies().empty()) {
      valid_ = true;
      BuildNameToFamilyMap(font_resources->FontFamilies());
      if (Settings::GetSettings().EnableThemeFont()) {
        FindThemeTypeface(font_resources->FontFamilies()[0]);
      }
    } else {
      valid_ = false;
    }
  }

 protected:
  int OnCountFamilies() const override { return name_to_family_map_.size(); }

  std::string OnGetFamilyName(int index) const override {
    if (index < 0 || name_to_family_map_.size() <= static_cast<size_t>(index)) {
      return "";
    }
    return name_to_family_map_[index].name;
  }

  std::shared_ptr<FontStyleSet> OnCreateStyleSet(int index) const override {
    if (index < 0 || name_to_family_map_.size() <= static_cast<size_t>(index)) {
      return nullptr;
    }
    return name_to_family_map_[index].styleSet;
  }

  std::shared_ptr<FontStyleSet> OnMatchFamily(
      const char familyName[]) const override {
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

  std::shared_ptr<Typeface> OnMatchFamilyStyle(
      const char familyName[], const FontStyle& style) const override {
    std::shared_ptr<FontStyleSet> sset(this->MatchFamily(familyName));
    return sset->MatchStyle(style);
  }

  static std::shared_ptr<Typeface> find_family_style_character(
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
      std::shared_ptr<FontStyleSetAndroid> style_set = fallback_map[i].styleSet;
      if (style_set->fallback_for_ != family_name) {
        continue;
      }
      std::shared_ptr<Typeface> typeface = style_set->MatchStyle(style);
      std::shared_ptr<TypefaceFreeTypeAndroid> typeface_android =
          std::static_pointer_cast<TypefaceFreeTypeAndroid>(typeface);

      if (!match_language(typeface_android->GetFontLanguages(),
                          lang_patterns)) {
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

  std::shared_ptr<Typeface> OnMatchFamilyStyleCharacter(
      const char familyName[], const FontStyle& style, const char* bcp47[],
      int bcp47_count, Unichar character) const override {
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
        std::shared_ptr<Typeface> matching_typeface =
            find_family_style_character(
                family_name, fallback_name_to_family_map_, style,
                static_cast<bool>(elegant), lang_patterns, character);
        if (matching_typeface) {
          return std::move(matching_typeface);
        }
      }
    }

    return nullptr;
  }

  std::shared_ptr<Typeface> OnMakeFromData(std::shared_ptr<Data> const& data,
                                           int ttcIndex) const override {
    return TypefaceFreeType::Make(data,
                                  FontArguments().SetCollectionIndex(ttcIndex));
  }

  std::shared_ptr<Typeface> OnMakeFromFile(const char path[],
                                           int ttcIndex) const override {
    auto data = Data::MakeFromFileName(path);
    return TypefaceFreeType::Make(data,
                                  FontArguments().SetCollectionIndex(ttcIndex));
  }

  std::shared_ptr<Typeface> OnGetDefaultTypeface(
      FontStyle const& font_style) const override {
    if (theme_typeface_) {
      return theme_typeface_;
    } else if (theme_font_style_set_ && theme_font_style_set_->Count() > 0) {
      // We could do better than native if we use parameter font_style
      std::shared_ptr<Typeface> theme_typeface =
          theme_font_style_set_->MatchStyle(FontStyle());
      if (theme_typeface) {
        return theme_typeface;
      }
    }
    return OnMatchFamilyStyle(DEFAULT_FONT_NAME, font_style);
  }

 private:
  void BuildNameToFamilyMap(std::vector<FontFamily>& font_families) {
    size_t index = 0;
    for (FontFamily& family : font_families) {
      AddFamily(family, index++);
      for (const auto& [unused, fallbackFamily] : family.fallback_families) {
        AddFamily(*fallbackFamily, index++);
      }
    }
  }

  void AddFamily(FontFamily& family, size_t index) {
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

    std::shared_ptr<FontStyleSetAndroid> newSet =
        std::make_shared<FontStyleSetAndroid>(family);
    if (0 == newSet->Count()) {
      return;
    }

    for (const std::string& name : family.names) {
      nameToFamily->emplace_back(NameToFamily{name, newSet});
    }
    style_sets_.emplace_back(std::move(newSet));
  }

  bool FindThemeTypeface(const FontFamily& family) {
    NdkFontAPI* api_pointer = NdkFontAPI::GetNdkFontAPI();
    if (!api_pointer) {
      return false;
    }
    const NdkFontAPI& api = *api_pointer;
    SystemFontIterator fontIter(api);
    if (!fontIter) {
      return false;
    }

    size_t non_system = 0;
    SystemFont theme_font(api, nullptr);
    while (auto font = fontIter.next()) {
      const char* font_path = font.getFontFilePath();
      if (std::string(font_path).find("/system/fonts") == std::string::npos) {
        non_system++;
        theme_font = std::move(font);
      }
    }
    if (non_system > 1) {
      // have no idea what happened
      return false;
    } else if (non_system == 1) {
      theme_typeface_ = Make(std::move(theme_font));
    } else {
      std::string theme_font_path_candidate =
          "/data/skin/fonts/DroidSansChinese.ttf";

      auto data = Data::MakeFromFileMapping(theme_font_path_candidate.c_str());
      if (!data) {
        theme_font_path_candidate =
            "data/system/theme/fonts/Roboto-Regular.ttf";
        data = Data::MakeFromFileMapping(theme_font_path_candidate.c_str());
      }
      if (data) {
        FontArguments font_args;
        font_args.SetCollectionIndex(0);
        theme_typeface_ = TypefaceFreeType::Make(data, font_args);
      }

      // Reset theme font if it is variation font.
      // Because if the theme font is a variation font, we can either manipulate
      // the theme font as configured for the default font in the xml, or be
      // free to create its variants. However, we currently do not have enough
      // information to determine which is correct.
      if (theme_typeface_ && theme_typeface_->IsVariationTypeface()) {
        theme_typeface_ = nullptr;
        FontFamily copy_default("", family.is_fallback_font);
        copy_default.names = family.names;
        copy_default.languages = family.languages;
        copy_default.fonts = family.fonts;
        for (auto& font_file : copy_default.fonts) {
          font_file.file_name = theme_font_path_candidate;
        }
        copy_default.variant = family.variant;
        copy_default.order = family.order;
        copy_default.fallback_for = family.fallback_for;
        theme_font_style_set_ =
            std::make_unique<FontStyleSetAndroid>(copy_default);
      }
    }

    return theme_typeface_ != nullptr ||
           (theme_font_style_set_ && theme_font_style_set_->Count() > 0);
  }

  std::shared_ptr<TypefaceFreeType> Make(SystemFont font) {
    if (!font) {
      return nullptr;
    }

    std::string full_path = font.getFontFilePath();
    auto data = Data::MakeFromFileMapping(full_path.c_str());
    if (!data) {
      return nullptr;
    }

    VariationPosition position;
    for (size_t i = 0; i < font.getAxisCount(); i++) {
      uint32_t tag = font.getAxisTag(i);
      position.AddCoordinate(tag, font.getAxisValue(i));
    }
    FontArguments font_args;
    font_args.SetCollectionIndex(font.getCollectionIndex())
        .SetVariationDesignPosition(position);
    std::shared_ptr<TypefaceFreeType> typeface =
        TypefaceFreeType::Make(data, font_args);
    return typeface;
  }

  // acquire lock before getting state
  bool IsValid() { return valid_; }

  bool valid_ = false;
  std::vector<std::shared_ptr<FontStyleSetAndroid>> style_sets_;
  std::vector<NameToFamily> name_to_family_map_;
  std::vector<NameToFamily> fallback_name_to_family_map_;

  std::shared_ptr<TypefaceFreeType> theme_typeface_ = nullptr;
  std::shared_ptr<FontStyleSetAndroid> theme_font_style_set_ = nullptr;

  friend std::shared_ptr<FontManager> FontManager::RefDefault();
};

}  // namespace

std::shared_ptr<FontManager> FontManager::RefDefault() {
  static std::mutex mutex;
  static std::shared_ptr<FontManagerAndroid> instance;

  std::lock_guard<std::mutex> lock(mutex);

  if (!instance || !instance->IsValid()) {
    instance = std::make_shared<FontManagerAndroid>();
  }

  return instance;
}

}  // namespace skity
