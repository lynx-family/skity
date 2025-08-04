/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_TEXT_PORTS_DARWIN_FONT_MANAGER_DARWIN_HPP
#define SRC_TEXT_PORTS_DARWIN_FONT_MANAGER_DARWIN_HPP

#include <skity/text/font_manager.hpp>
#include <string>
#include <vector>

#include "src/text/ports/darwin/typeface_darwin.hpp"
#include "src/text/ports/darwin/types_darwin.hpp"

namespace skity {

class FontStyleSetDarwin : public FontStyleSet {
 public:
  explicit FontStyleSetDarwin(UniqueCFRef<CTFontDescriptorRef> desc);
  ~FontStyleSetDarwin() override = default;

  int Count() override;

  void GetStyle(int index, FontStyle*, std::string*) override;

  std::shared_ptr<Typeface> CreateTypeface(int index) override;

  std::shared_ptr<Typeface> MatchStyle(const FontStyle& pattern) override;

  CTFontDescriptorRef GetCTFontDescriptor() const;

 private:
  std::unique_ptr<TypefaceDarwin> TypefaceFromDesc(CTFontDescriptorRef desc);

 private:
  UniqueCFRef<CTFontDescriptorRef> cf_desc;
  UniqueCFRef<CFArrayRef> mached_desc_;
  std::vector<std::shared_ptr<TypefaceDarwin>> typefaces_;
};

class FontManagerDarwin : public FontManager {
 public:
  FontManagerDarwin();
  ~FontManagerDarwin() override = default;

  void SetDefaultTypeface(std::shared_ptr<Typeface> typeface) override {
    default_typeface_ = std::move(typeface);
  }

 protected:
  int OnCountFamilies() const override;

  std::string OnGetFamilyName(int index) const override;

  std::shared_ptr<FontStyleSet> OnCreateStyleSet(int index) const override;

  std::shared_ptr<FontStyleSet> OnMatchFamily(
      const char* family_name) const override;

  std::shared_ptr<Typeface> OnMatchFamilyStyle(
      const char* family_name, const FontStyle& style) const override;

  std::shared_ptr<Typeface> OnMatchFamilyStyleCharacter(
      const char* family_name, const FontStyle& style, const char* bcp47[],
      int bcp47Count, Unichar character) const override;

  std::shared_ptr<Typeface> OnMakeFromData(std::shared_ptr<Data> const&,
                                           int ttcIndex) const override;

  std::shared_ptr<Typeface> OnMakeFromFile(const char path[],
                                           int ttcIndex) const override;

  std::shared_ptr<Typeface> OnGetDefaultTypeface(
      FontStyle const& font_style) const override;

 private:
  void InitSystemFamily();

  int32_t GetIndexByFamilyName(const char* family_name) const;

  std::shared_ptr<FontStyleSetDarwin> MatchFamilyByIndex(int32_t index) const;

  std::shared_ptr<TypefaceDarwin> SavedFallbackTypeface(
      UniqueCFRef<CTFontRef> ct_font, FontStyle const& style) const;

 private:
  UniqueCFRef<CFArrayRef> cf_family_names_;
  std::vector<std::string> sys_family_names_;
  int default_name_index_ = -1;
  mutable std::vector<std::shared_ptr<FontStyleSetDarwin>> sys_style_sets_;
  mutable std::vector<std::shared_ptr<TypefaceDarwin>> sys_fallbacked_;
  std::shared_ptr<Typeface> default_typeface_ = nullptr;
};

}  // namespace skity

#endif  // SRC_TEXT_PORTS_DARWIN_FONT_MANAGER_DARWIN_HPP
