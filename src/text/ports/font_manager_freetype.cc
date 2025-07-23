// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/text/font_manager.hpp>
#include <string>

#include "src/logging.hpp"
#include "src/text/ports/typeface_freetype.hpp"
#include "src/utils/no_destructor.hpp"

namespace skity {

class FontManagerFreetype : public FontManager {
 public:
  void SetDefaultTypeface(Typeface* typeface) override {
    default_typeface_ = typeface;
  }

 protected:
  int OnCountFamilies() const override { return 0; }

  std::string OnGetFamilyName(int) const override {
    LOGE("onGetFamilyName called with bad index");
    return "";
  }

  FontStyleSet* OnCreateStyleSet(int) const override {
    LOGE("onCreateStyleSet called with bad index");
    return nullptr;
  }

  FontStyleSet* OnMatchFamily(const char[]) const override {
    return FontStyleSet::CreateEmpty();
  }

  Typeface* OnMatchFamilyStyle(const char[], const FontStyle&) const override {
    return nullptr;
  }

  Typeface* OnMatchFamilyStyleCharacter(const char[], const FontStyle&,
                                        const char*[], int,
                                        Unichar) const override {
    return nullptr;
  }

  std::unique_ptr<Typeface> OnMakeFromData(std::shared_ptr<Data> const& data,
                                           int ttcIndex) const override {
    return TypefaceFreeType::Make(data,
                                  FontArguments().SetCollectionIndex(ttcIndex));
  }

  std::unique_ptr<Typeface> OnMakeFromFile(const char path[],
                                           int ttcIndex) const override {
    auto data = Data::MakeFromFileMapping(path);
    return this->OnMakeFromData(data, ttcIndex);
  }

  Typeface* OnGetDefaultTypeface(FontStyle const&) const override {
    return default_typeface_;
  }

 private:
  Typeface* default_typeface_;
};

std::shared_ptr<FontManager> FontManager::RefDefault() {
  static const NoDestructor<std::shared_ptr<FontManager>> font_manager(
      [] { return std::make_shared<FontManagerFreetype>(); }());
  return *font_manager;
}

}  // namespace skity
