// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// Native Linux font discovery and matching through Fontconfig.
#include "src/text/ports/linux/font_manager_fontconfig.hpp"

#include <fontconfig/fontconfig.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <list>
#include <mutex>
#include <set>
#include <utility>

#include "src/logging.hpp"
#include "src/text/ports/typeface_freetype.hpp"
#include "src/text/scaler_context.hpp"
#include "src/text/scaler_context_desc.hpp"
#include "src/utils/no_destructor.hpp"

namespace skity {
namespace {

// Serialize Fontconfig operations and reference destruction, including on
// older supported runtimes. FreeType never calls back into this lock.
std::recursive_mutex& FontConfigMutex() {
  static NoDestructor<std::recursive_mutex> mutex;
  return *mutex;
}
using FCLock = std::lock_guard<std::recursive_mutex>;

template <typename T, void (*Destroy)(T*)>
struct FCDeleter {
  void operator()(T* object) const {
    if (object) {
      FCLock lock(FontConfigMutex());
      Destroy(object);
    }
  }
};
template <typename T, void (*Destroy)(T*)>
using FCPtr = std::unique_ptr<T, FCDeleter<T, Destroy>>;
using Config = FCPtr<FcConfig, FcConfigDestroy>;
using Pattern = FCPtr<FcPattern, FcPatternDestroy>;
using FontSet = FCPtr<FcFontSet, FcFontSetDestroy>;
using CharSet = FCPtr<FcCharSet, FcCharSetDestroy>;
using LangSet = FCPtr<FcLangSet, FcLangSetDestroy>;

const FcChar8* FCString(const char* text) {
  return reinterpret_cast<const FcChar8*>(text);
}

std::string String(FcPattern* pattern, const char* key) {
  FcChar8* value = nullptr;
  return FcPatternGetString(pattern, key, 0, &value) == FcResultMatch
             ? reinterpret_cast<const char*>(value)
             : "";
}

int Integer(FcPattern* pattern, const char* key, int fallback) {
  int value = fallback;
  return FcPatternGetInteger(pattern, key, 0, &value) == FcResultMatch
             ? value
             : fallback;
}

bool Boolean(FcPattern* pattern, const char* key, bool fallback = false) {
  FcBool value;
  return FcPatternGetBool(pattern, key, 0, &value) == FcResultMatch
             ? value != FcFalse
             : fallback;
}

// Convert weights piecewise, including demilight and book.
constexpr std::array<std::pair<int, int>, 12> kWeights = {
    {{100, FC_WEIGHT_THIN},
     {200, FC_WEIGHT_EXTRALIGHT},
     {300, FC_WEIGHT_LIGHT},
     {350, FC_WEIGHT_DEMILIGHT},
     {380, FC_WEIGHT_BOOK},
     {400, FC_WEIGHT_REGULAR},
     {500, FC_WEIGHT_MEDIUM},
     {600, FC_WEIGHT_DEMIBOLD},
     {700, FC_WEIGHT_BOLD},
     {800, FC_WEIGHT_EXTRABOLD},
     {900, FC_WEIGHT_BLACK},
     {1000, FC_WEIGHT_EXTRABLACK}}};
constexpr std::array<std::pair<int, int>, 9> kWidths = {
    {{1, FC_WIDTH_ULTRACONDENSED},
     {2, FC_WIDTH_EXTRACONDENSED},
     {3, FC_WIDTH_CONDENSED},
     {4, FC_WIDTH_SEMICONDENSED},
     {5, FC_WIDTH_NORMAL},
     {6, FC_WIDTH_SEMIEXPANDED},
     {7, FC_WIDTH_EXPANDED},
     {8, FC_WIDTH_EXTRAEXPANDED},
     {9, FC_WIDTH_ULTRAEXPANDED}}};

template <size_t N>
int Convert(int value, const std::array<std::pair<int, int>, N>& ranges,
            bool from_fontconfig) {
  auto source = [&](size_t i) {
    return from_fontconfig ? ranges[i].second : ranges[i].first;
  };
  auto target = [&](size_t i) {
    return from_fontconfig ? ranges[i].first : ranges[i].second;
  };
  if (value <= source(0)) {
    return target(0);
  }
  for (size_t i = 1; i < N; ++i) {
    if (value < source(i)) {
      return target(i - 1) +
             static_cast<int>(static_cast<float>(value - source(i - 1)) *
                              (target(i) - target(i - 1)) /
                              (source(i) - source(i - 1)));
    }
  }
  return target(N - 1);
}

FontStyle Style(FcPattern* pattern) {
  const int slant = Integer(pattern, FC_SLANT, FC_SLANT_ROMAN);
  return FontStyle(
      Convert(Integer(pattern, FC_WEIGHT, FC_WEIGHT_REGULAR), kWeights, true),
      Convert(Integer(pattern, FC_WIDTH, FC_WIDTH_NORMAL), kWidths, true),
      slant == FC_SLANT_ITALIC    ? FontStyle::kItalic_Slant
      : slant == FC_SLANT_OBLIQUE ? FontStyle::kOblique_Slant
                                  : FontStyle::kUpright_Slant);
}

bool AddStyle(FcPattern* pattern, const FontStyle& style) {
  const int slant = style.slant() == FontStyle::kItalic_Slant ? FC_SLANT_ITALIC
                    : style.slant() == FontStyle::kOblique_Slant
                        ? FC_SLANT_OBLIQUE
                        : FC_SLANT_ROMAN;
  return FcPatternAddInteger(pattern, FC_WEIGHT,
                             Convert(style.weight(), kWeights, false)) &&
         FcPatternAddInteger(pattern, FC_WIDTH,
                             Convert(style.width(), kWidths, false)) &&
         FcPatternAddInteger(pattern, FC_SLANT, slant);
}

// Retain weak aliases before the final strong family; remove weak defaults
// after it. Leave an entirely weak family list unchanged.
Pattern FamilyConstraint(FcPattern* request) {
  Pattern result(FcPatternDuplicate(request));
  if (!result) {
    return result;
  }
  int count = 0;
  int last_strong = -1;
  for (;; ++count) {
    FcValue value;
    FcValueBinding binding;
    if (FcPatternGetWithBinding(result.get(), FC_FAMILY, count, &value,
                                &binding) != FcResultMatch) {
      break;
    }
    if (binding != FcValueBindingWeak) {
      last_strong = count;
    }
  }
  if (last_strong >= 0) {
    for (int i = last_strong + 1; i < count; ++i) {
      FcPatternRemove(result.get(), FC_FAMILY, last_strong + 1);
    }
  }
  return result;
}

bool FamilyMatches(FcPattern* font, FcPattern* request) {
  for (int i = 0; i < 65536; ++i) {
    FcChar8* a;
    FcResult result = FcPatternGetString(font, FC_FAMILY, i, &a);
    if (result == FcResultNoId || result == FcResultNoMatch) {
      break;
    }
    if (result != FcResultMatch) {
      continue;
    }
    for (int j = 0; j < 65536; ++j) {
      FcChar8* b;
      result = FcPatternGetString(request, FC_FAMILY, j, &b);
      if (result == FcResultNoId || result == FcResultNoMatch) {
        break;
      }
      if (result == FcResultMatch && FcStrCmpIgnoreCase(a, b) == 0) {
        return true;
      }
    }
  }
  return false;
}

struct TypefaceProperties {
  std::string family;
  std::string full_name;
  std::string postscript;
  FontStyle style;
  Matrix22 matrix;
  bool embolden = false;
};

class TypefaceFontConfig final : public TypefaceFreeTypeData {
 public:
  TypefaceFontConfig(FaceData face, TypefaceProperties properties,
                     FontStyle real_style, FontStyle proxy_style)
      : TypefaceFreeTypeData(std::move(face.data), face.font_args, proxy_style),
        properties_(std::move(properties)),
        original_real_style_(real_style) {}

 protected:
  void OnGetFontDescriptor(FontDescriptor& descriptor) const override {
    TypefaceFreeType::OnGetFontDescriptor(descriptor);
    descriptor.family_name = properties_.family;
    descriptor.full_name = properties_.full_name;
    descriptor.post_script_name = properties_.postscript;
    descriptor.style = GetFontStyle();
    descriptor.collection_index = GetFaceData().font_args.GetCollectionIndex();
    descriptor.variation_position = GetVariationDesignPosition();
  }

  std::unique_ptr<ScalerContext> OnCreateScalerContext(
      const ScalerContextDesc* descriptor) const override {
    ScalerContextDesc adjusted = *descriptor;
    adjusted.transform = adjusted.transform * properties_.matrix;
    adjusted.fake_bold |= properties_.embolden;
    return TypefaceFreeType::OnCreateScalerContext(&adjusted);
  }

  std::shared_ptr<Typeface> OnMakeVariation(
      const FontArguments& arguments) const override {
    auto real = std::static_pointer_cast<TypefaceFreeType>(
        TypefaceFreeType::OnMakeVariation(arguments));
    if (!real) {
      return nullptr;
    }
    const FontStyle style = real->GetFontStyle();
    FontStyle proxy(properties_.style.weight() + style.weight() -
                        original_real_style_.weight(),
                    properties_.style.width() + style.width() -
                        original_real_style_.width(),
                    style.slant() == original_real_style_.slant()
                        ? properties_.style.slant()
                        : style.slant());
    return std::make_shared<TypefaceFontConfig>(
        real->GetFaceData(), properties_, original_real_style_, proxy);
  }

 private:
  const TypefaceProperties properties_;
  const FontStyle original_real_style_;
};

struct FontConfigState {
  explicit FontConfigState(Config config) : config(std::move(config)) {
    if (!this->config) {
      return;
    }
    const char* root =
        reinterpret_cast<const char*>(FcConfigGetSysRoot(this->config.get()));
    sysroot = root ? root : "";
    std::set<std::string> seen;
    for (FcSetName set : {FcSetSystem, FcSetApplication}) {
      FcFontSet* fonts = FcConfigGetFonts(this->config.get(), set);
      if (!fonts) {
        continue;
      }
      for (int i = 0; i < fonts->nfont; ++i) {
        for (int j = 0; j < 65536; ++j) {
          FcChar8* name;
          FcResult result =
              FcPatternGetString(fonts->fonts[i], FC_FAMILY, j, &name);
          if (result == FcResultNoId || result == FcResultNoMatch) {
            break;
          }
          if (result == FcResultMatch) {
            std::string family(reinterpret_cast<const char*>(name));
            if (seen.insert(family).second) {
              families.push_back(std::move(family));
            }
          }
        }
      }
    }
  }

  std::string FileName(FcPattern* pattern) const {
    const auto file = String(pattern, FC_FILE);
    if (file.empty()) {
      return {};
    }
    if (!sysroot.empty()) {
      std::error_code error;
      if (std::filesystem::is_regular_file(sysroot + file, error)) {
        return sysroot + file;
      }
    }
    return file;
  }

  bool Accessible(FcPattern* pattern) const {
    auto data = Data::MakeFromFileMapping(FileName(pattern).c_str());
    if (!data) {
      return false;
    }
    FontScanner scanner;
    int count = 0;
    return scanner.RecognizedFont(data, &count);
  }

  std::shared_ptr<Typeface> MakeTypeface(Pattern pattern) {
    if (!pattern) {
      return nullptr;
    }
    for (auto i = cache.begin(); i != cache.end(); ++i) {
      if (FcPatternEqual(i->pattern.get(), pattern.get())) {
        auto face = i->face;
        cache.splice(cache.begin(), cache, i);
        return face;
      }
    }
    auto data = Data::MakeFromFileMapping(FileName(pattern.get()).c_str());
    if (!data) {
      return nullptr;
    }
    // Applying FC_FONT_VARIATIONS and synthesizing named instances are not yet
    // implemented. Ordinary collection indices are retained.
    auto real =
        TypefaceFreeType::Make(data, FontArguments().SetCollectionIndex(
                                         Integer(pattern.get(), FC_INDEX, 0)));
    if (!real) {
      return nullptr;
    }
    TypefaceProperties properties;
    properties.family = String(pattern.get(), FC_FAMILY);
    properties.full_name = String(pattern.get(), FC_FULLNAME);
    properties.postscript = String(pattern.get(), FC_POSTSCRIPT_NAME);
    properties.style = Style(pattern.get());
    properties.embolden = Boolean(pattern.get(), FC_EMBOLDEN);
    FcMatrix* matrix;
    if (Boolean(pattern.get(), FC_OUTLINE, true) &&
        FcPatternGetMatrix(pattern.get(), FC_MATRIX, 0, &matrix) ==
            FcResultMatch) {
      properties.matrix =
          Matrix22(matrix->xx, -matrix->xy, -matrix->yx, matrix->yy);
    }
    auto face = std::make_shared<TypefaceFontConfig>(
        real->GetFaceData(), properties, real->GetFontStyle(),
        properties.style);
    cache.push_front({std::move(pattern), face});
    if (cache.size() > 64) {
      cache.pop_back();
    }
    return face;
  }

  struct CacheEntry {
    Pattern pattern;
    std::shared_ptr<Typeface> face;
  };
  Config config;
  std::string sysroot;
  std::vector<std::string> families;
  std::list<CacheEntry> cache;
};

class FontStyleSetFontConfig final : public FontStyleSet {
 public:
  FontStyleSetFontConfig(std::shared_ptr<FontConfigState> state, FontSet fonts)
      : state_(std::move(state)), fonts_(std::move(fonts)) {}

  int Count() override { return fonts_->nfont; }

  void GetStyle(int index, FontStyle* style, std::string* name) override {
    if (index < 0 || index >= Count()) {
      return;
    }
    FCLock lock(FontConfigMutex());
    if (style) {
      *style = Style(fonts_->fonts[index]);
    }
    if (name) {
      *name = String(fonts_->fonts[index], FC_STYLE);
    }
  }

  std::shared_ptr<Typeface> CreateTypeface(int index) override {
    if (index < 0 || index >= Count()) {
      return nullptr;
    }
    FCLock lock(FontConfigMutex());
    return state_->MakeTypeface(
        Pattern(FcPatternDuplicate(fonts_->fonts[index])));
  }

  std::shared_ptr<Typeface> MatchStyle(const FontStyle& style) override {
    FCLock lock(FontConfigMutex());
    Pattern request(FcPatternCreate());
    if (!request || !AddStyle(request.get(), style) ||
        !FcConfigSubstitute(state_->config.get(), request.get(),
                            FcMatchPattern)) {
      return nullptr;
    }
    FcDefaultSubstitute(request.get());
    FcFontSet* sets[] = {fonts_.get()};
    FcResult result;
    return state_->MakeTypeface(Pattern(
        FcFontSetMatch(state_->config.get(), sets, 1, request.get(), &result)));
  }

 private:
  std::shared_ptr<FontConfigState> state_;
  FontSet fonts_;
};

class FontManagerFontConfig final : public FontManager {
 public:
  explicit FontManagerFontConfig(Config config)
      : state_(std::make_shared<FontConfigState>(std::move(config))) {}

  FontConfigInfo Info() const {
    FCLock lock(FontConfigMutex());
    FontConfigInfo info;
    info.runtime_version = FcGetVersion();
    info.initialized = static_cast<bool>(state_->config);
    if (!info.initialized) {
      return info;
    }
    FcStrList* files = FcConfigGetConfigFiles(state_->config.get());
    if (files) {
      while (FcChar8* file = FcStrListNext(files)) {
        info.config_files.emplace_back(reinterpret_cast<const char*>(file));
      }
      FcStrListDone(files);
    }
    std::set<std::string> names;
    for (FcSetName set : {FcSetSystem, FcSetApplication}) {
      FcFontSet* fonts = FcConfigGetFonts(state_->config.get(), set);
      if (fonts) {
        for (int i = 0; i < fonts->nfont; ++i) {
          names.insert(state_->FileName(fonts->fonts[i]));
        }
      }
    }
    info.font_files.assign(names.begin(), names.end());
    return info;
  }

 protected:
  int OnCountFamilies() const override { return state_->families.size(); }

  std::string OnGetFamilyName(int index) const override {
    return index >= 0 && index < OnCountFamilies() ? state_->families[index]
                                                   : "";
  }

  std::shared_ptr<FontStyleSet> OnCreateStyleSet(int index) const override {
    return index >= 0 && index < OnCountFamilies()
               ? OnMatchFamily(state_->families[index].c_str())
               : nullptr;
  }

  std::shared_ptr<FontStyleSet> OnMatchFamily(
      const char name[]) const override {
    if (!name || !state_->config) {
      return nullptr;
    }
    FCLock lock(FontConfigMutex());
    Pattern request(FcPatternCreate());
    if (!request ||
        !FcPatternAddString(request.get(), FC_FAMILY, FCString(name)) ||
        !FcConfigSubstitute(state_->config.get(), request.get(),
                            FcMatchPattern)) {
      return nullptr;
    }
    FcDefaultSubstitute(request.get());
    auto constraint = FamilyConstraint(request.get());
    FontSet matches(FcFontSetCreate());
    if (!constraint || !matches) {
      return nullptr;
    }
    for (FcSetName set : {FcSetSystem, FcSetApplication}) {
      FcFontSet* fonts = FcConfigGetFonts(state_->config.get(), set);
      if (!fonts) {
        continue;
      }
      for (int i = 0; i < fonts->nfont; ++i) {
        FcPattern* font = fonts->fonts[i];
        if (FamilyMatches(font, constraint.get()) && state_->Accessible(font)) {
          Pattern prepared(
              FcFontRenderPrepare(state_->config.get(), request.get(), font));
          if (!prepared || !FcFontSetAdd(matches.get(), prepared.get())) {
            return nullptr;
          }
          prepared.release();
        }
      }
    }
    return std::make_shared<FontStyleSetFontConfig>(state_, std::move(matches));
  }

  std::shared_ptr<Typeface> OnMatchFamilyStyle(
      const char name[], const FontStyle& style) const override {
    if (!state_->config) {
      return nullptr;
    }
    FCLock lock(FontConfigMutex());
    Pattern request(FcPatternCreate());
    if (!request ||
        (name &&
         !FcPatternAddString(request.get(), FC_FAMILY, FCString(name))) ||
        !AddStyle(request.get(), style) ||
        !FcConfigSubstitute(state_->config.get(), request.get(),
                            FcMatchPattern)) {
      return nullptr;
    }
    FcDefaultSubstitute(request.get());
    Pattern constraint(name ? FamilyConstraint(request.get())
                            : Pattern(FcPatternDuplicate(request.get())));
    FcResult result;
    Pattern match(FcFontMatch(state_->config.get(), request.get(), &result));
    if (!constraint || !match ||
        !FamilyMatches(match.get(), constraint.get()) ||
        !state_->Accessible(match.get())) {
      return nullptr;
    }
    return state_->MakeTypeface(std::move(match));
  }

  std::shared_ptr<Typeface> OnMatchFamilyStyleCharacter(
      const char name[], const FontStyle& style, const char* languages[],
      int language_count, Unichar character) const override {
    if (!state_->config || character > 0x10FFFF ||
        (character >= 0xD800 && character <= 0xDFFF) || language_count < 0 ||
        (language_count && !languages)) {
      return nullptr;
    }
    FCLock lock(FontConfigMutex());
    Pattern request(FcPatternCreate());
    CharSet characters(FcCharSetCreate());
    if (!request || !characters) {
      return nullptr;
    }
    if (name) {
      FcValue family;
      family.type = FcTypeString;
      family.u.s = FCString(name);
      if (!FcPatternAddWeak(request.get(), FC_FAMILY, family, FcFalse)) {
        return nullptr;
      }
    }
    if (!AddStyle(request.get(), style) ||
        !FcCharSetAddChar(characters.get(), character) ||
        !FcPatternAddCharSet(request.get(), FC_CHARSET, characters.get())) {
      return nullptr;
    }
    if (language_count > 0) {
      LangSet langs(FcLangSetCreate());
      if (!langs) {
        return nullptr;
      }
      for (int i = language_count; i-- > 0;) {
        if (!languages[i] ||
            !FcLangSetAdd(langs.get(), FCString(languages[i]))) {
          return nullptr;
        }
      }
      if (!FcPatternAddLangSet(request.get(), FC_LANG, langs.get())) {
        return nullptr;
      }
    }
    if (!FcConfigSubstitute(state_->config.get(), request.get(),
                            FcMatchPattern)) {
      return nullptr;
    }
    FcDefaultSubstitute(request.get());
    FcResult result;
    Pattern match(FcFontMatch(state_->config.get(), request.get(), &result));
    if (!match) {
      return nullptr;
    }
    FcCharSet* coverage;
    for (int i = 0; i < 65536; ++i) {
      result = FcPatternGetCharSet(match.get(), FC_CHARSET, i, &coverage);
      if (result == FcResultNoId || result == FcResultNoMatch) {
        break;
      }
      if (result == FcResultMatch && FcCharSetHasChar(coverage, character) &&
          state_->Accessible(match.get())) {
        return state_->MakeTypeface(std::move(match));
      }
    }
    return nullptr;
  }

  std::shared_ptr<Typeface> OnGetDefaultTypeface(
      const FontStyle& style) const override {
    return OnMatchFamilyStyle(nullptr, style);
  }

  std::shared_ptr<Typeface> OnMakeFromData(const std::shared_ptr<Data>& data,
                                           int index) const override {
    return TypefaceFreeType::Make(data,
                                  FontArguments().SetCollectionIndex(index));
  }

  std::shared_ptr<Typeface> OnMakeFromFile(const char* path,
                                           int index) const override {
    auto data = Data::MakeFromFileMapping(path);
    return data ? OnMakeFromData(data, index) : nullptr;
  }

 private:
  std::shared_ptr<FontConfigState> state_;
};

}  // namespace

std::shared_ptr<FontManager> MakeFontManagerFontConfig(
    const char* config_file) {
  FCLock lock(FontConfigMutex());
  if (!config_file) {
    config_file = std::getenv("FONTCONFIG_FILE");
  }
  Config config;
  if (config_file) {
    config.reset(FcConfigCreate());
    if (!config || !*config_file ||
        !FcConfigParseAndLoad(config.get(), FCString(config_file), FcTrue) ||
        !FcConfigBuildFonts(config.get())) {
      return nullptr;
    }
  } else {
    config.reset(FcInitLoadConfigAndFonts());
    if (!config) {
      return nullptr;
    }
  }
  return std::make_shared<FontManagerFontConfig>(std::move(config));
}

FontConfigInfo GetDefaultFontConfigInfo() {
  // This translation unit supplies RefDefault, including the empty fallback.
  return std::static_pointer_cast<FontManagerFontConfig>(
             FontManager::RefDefault())
      ->Info();
}

std::shared_ptr<FontManager> FontManager::RefDefault() {
  static NoDestructor<std::shared_ptr<FontManager>> manager([] {
    auto result = MakeFontManagerFontConfig();
    if (!result) {
      LOGE("Failed to initialize Fontconfig; system font inventory is empty");
      // Keep explicit loading usable without silently choosing host fonts after
      // an explicitly selected configuration failed.
      result = std::make_shared<FontManagerFontConfig>(Config{});
    }
    return result;
  }());
  return *manager;
}

}  // namespace skity
