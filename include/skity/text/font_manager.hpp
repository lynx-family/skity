// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_TEXT_FONT_MANAGER_HPP
#define INCLUDE_SKITY_TEXT_FONT_MANAGER_HPP

#include <memory>
#include <skity/macros.hpp>
#include <skity/text/font_style.hpp>
#include <skity/text/typeface.hpp>
#include <string>
#include <vector>

namespace skity {

class SKITY_API FontStyleSet {
 public:
  virtual ~FontStyleSet() = default;
  FontStyleSet() = default;
  FontStyleSet(const FontStyleSet&) = delete;
  FontStyleSet& operator=(const FontStyleSet&) = delete;

  virtual int Count() = 0;
  virtual void GetStyle(int index, FontStyle*, std::string*) = 0;
  virtual std::shared_ptr<Typeface> CreateTypeface(int index) = 0;
  virtual std::shared_ptr<Typeface> MatchStyle(const FontStyle& pattern) = 0;

  static std::shared_ptr<FontStyleSet> CreateEmpty();

 protected:
  std::shared_ptr<Typeface> MatchStyleCSS3(const FontStyle& pattern);
};

class SKITY_API FontManager {
 public:
  virtual ~FontManager() = default;
  FontManager(FontManager&) = delete;
  FontManager& operator=(const FontManager&) = delete;

  int CountFamilies() const;
  std::string GetFamilyName(int index) const;

  std::shared_ptr<FontStyleSet> CreateStyleSet(int index) const;
  std::shared_ptr<FontStyleSet> MatchFamily(const char familyName[]) const;

  std::shared_ptr<Typeface> MatchFamilyStyle(const char familyName[],
                                             const FontStyle&);
  std::shared_ptr<Typeface> MatchFamilyStyleCharacter(const char familyName[],
                                                      const FontStyle&,
                                                      const char* bcp47[],
                                                      int bcp47Count,
                                                      Unichar character);

  std::shared_ptr<Typeface> MakeFromData(std::shared_ptr<Data> const& data,
                                         int ttcIndex = 0);
  std::shared_ptr<Typeface> MakeFromFile(const char* path, int ttcIndex = 0);

  std::shared_ptr<Typeface> GetDefaultTypeface(FontStyle font_style) const;

  // TODO(jingle): We will remove this after implementing PC portable font
  // manager
  virtual void SetDefaultTypeface(std::shared_ptr<Typeface>) {}

  /** Return the default fontmgr. */
  static std::shared_ptr<FontManager> RefDefault();

 protected:
  FontManager() = default;
  virtual int OnCountFamilies() const = 0;
  virtual std::string OnGetFamilyName(int index) const = 0;

  virtual std::shared_ptr<FontStyleSet> OnCreateStyleSet(int index) const = 0;
  /** May return NULL if the name is not found. */
  virtual std::shared_ptr<FontStyleSet> OnMatchFamily(
      const char familyName[]) const = 0;

  virtual std::shared_ptr<Typeface> OnMatchFamilyStyle(
      const char familyName[], const FontStyle&) const = 0;
  virtual std::shared_ptr<Typeface> OnMatchFamilyStyleCharacter(
      const char familyName[], const FontStyle&, const char* bcp47[],
      int bcp47Count, Unichar character) const = 0;

  virtual std::shared_ptr<Typeface> OnMakeFromData(std::shared_ptr<Data> const&,
                                                   int ttcIndex) const = 0;

  virtual std::shared_ptr<Typeface> OnMakeFromFile(const char path[],
                                                   int ttcIndex) const = 0;

  virtual std::shared_ptr<Typeface> OnGetDefaultTypeface(
      FontStyle const& font_style) const = 0;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_TEXT_FONT_MANAGER_HPP
