/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/text/font_manager.hpp>
#include <string>

#include "src/logging.hpp"

namespace skity {

class EmptyFontStyleSet : public FontStyleSet {
 public:
  int Count() override { return 0; }

  void GetStyle(int, FontStyle*, std::string*) override {
    LOGE("SkFontStyleSet::getStyle called on empty set");
  }

  std::shared_ptr<Typeface> CreateTypeface(int) override {
    LOGE("SkFontStyleSet::createTypeface called on empty set");
    return nullptr;
  }

  std::shared_ptr<Typeface> MatchStyle(const FontStyle&) override {
    return nullptr;
  }
};

std::shared_ptr<FontStyleSet> FontStyleSet::CreateEmpty() {
  return std::make_shared<EmptyFontStyleSet>();
}

std::shared_ptr<Typeface> FontStyleSet::MatchStyleCSS3(
    const FontStyle& pattern) {
  int count = this->Count();
  if (0 == count) {
    return nullptr;
  }

  struct Score {
    int score;
    int index;
    Score& operator+=(int rhs) {
      this->score += rhs;
      return *this;
    }
    Score& operator<<=(int rhs) {
      this->score <<= rhs;
      return *this;
    }
    bool operator<(const Score& that) const { return this->score < that.score; }
  };

  Score maxScore = {0, 0};
  for (int i = 0; i < count; ++i) {
    FontStyle current;
    this->GetStyle(i, &current, nullptr);
    Score currentScore = {0, i};

    // CSS stretch / SkFontStyle::Width
    // Takes priority over everything else.
    if (pattern.width() <= FontStyle::kNormal_Width) {
      if (current.width() <= pattern.width()) {
        currentScore += 10 - pattern.width() + current.width();
      } else {
        currentScore += 10 - current.width();
      }
    } else {
      if (current.width() > pattern.width()) {
        currentScore += 10 + pattern.width() - current.width();
      } else {
        currentScore += current.width();
      }
    }
    currentScore <<= 8;

    // CSS style (normal, italic, oblique) / SkFontStyle::Slant (upright,
    // italic, oblique) Takes priority over all valid weights.
    static_assert(FontStyle::kUpright_Slant == 0 &&
                      FontStyle::kItalic_Slant == 1 &&
                      FontStyle::kOblique_Slant == 2,
                  "SkFontStyle::Slant values not as required.");
    // SKITY_ASSERT(0 <= pattern.slant() && pattern.slant() <= 2 &&
    //              0 <= current.slant() && current.slant() <= 2);
    static const int score[3][3] = {
        /*               Upright Italic Oblique  [current]*/
        /*   Upright */ {3, 1, 2},
        /*   Italic  */ {1, 3, 2},
        /*   Oblique */ {1, 2, 3},
        /* [pattern] */
    };
    currentScore += score[pattern.slant()][current.slant()];
    currentScore <<= 8;

    // Synthetics (weight, style) [no stretch synthetic?]

    // CSS weight / SkFontStyle::Weight
    // The 'closer' to the target weight, the higher the score.
    // 1000 is the 'heaviest' recognized weight
    if (pattern.weight() == current.weight()) {
      currentScore += 1000;
      // less than 400 prefer lighter weights
    } else if (pattern.weight() < 400) {
      if (current.weight() <= pattern.weight()) {
        currentScore += 1000 - pattern.weight() + current.weight();
      } else {
        currentScore += 1000 - current.weight();
      }
      // between 400 and 500 prefer heavier up to 500, then lighter weights
    } else if (pattern.weight() <= 500) {
      if (current.weight() >= pattern.weight() && current.weight() <= 500) {
        currentScore += 1000 + pattern.weight() - current.weight();
      } else if (current.weight() <= pattern.weight()) {
        currentScore += 500 + current.weight();
      } else {
        currentScore += 1000 - current.weight();
      }
      // greater than 500 prefer heavier weights
    } else if (pattern.weight() > 500) {
      if (current.weight() > pattern.weight()) {
        currentScore += 1000 + pattern.weight() - current.weight();
      } else {
        currentScore += current.weight();
      }
    }

    if (maxScore < currentScore) {
      maxScore = currentScore;
    }
  }

  return this->CreateTypeface(maxScore.index);
}

static std::shared_ptr<FontStyleSet> emptyOnNull(
    std::shared_ptr<FontStyleSet> fsset) {
  if (nullptr == fsset) {
    fsset = FontStyleSet::CreateEmpty();
  }
  return fsset;
}

int FontManager::CountFamilies() const { return this->OnCountFamilies(); }

std::string FontManager::GetFamilyName(int index) const {
  return this->OnGetFamilyName(index);
}

std::shared_ptr<FontStyleSet> FontManager::CreateStyleSet(int index) const {
  return emptyOnNull(this->OnCreateStyleSet(index));
}

std::shared_ptr<FontStyleSet> FontManager::MatchFamily(
    const char familyName[]) const {
  return emptyOnNull(this->OnMatchFamily(familyName));
}

std::shared_ptr<Typeface> FontManager::MatchFamilyStyle(const char familyName[],
                                                        const FontStyle& fs) {
  return this->OnMatchFamilyStyle(familyName, fs);
}

std::shared_ptr<Typeface> FontManager::MatchFamilyStyleCharacter(
    const char familyName[], const FontStyle& style, const char* bcp47[],
    int bcp47Count, Unichar character) {
  return this->OnMatchFamilyStyleCharacter(familyName, style, bcp47, bcp47Count,
                                           character);
}

std::shared_ptr<Typeface> FontManager::MakeFromData(
    std::shared_ptr<Data> const& data, int ttcIndex) {
  if (nullptr == data) {
    return nullptr;
  }
  auto typeface = this->OnMakeFromData(data, ttcIndex);
  if (!typeface) {
    return nullptr;
  }
  font_lst_.push_back(std::move(typeface));
  return font_lst_.back();
}

std::shared_ptr<Typeface> FontManager::MakeFromFile(const char* path,
                                                    int ttcIndex) {
  if (nullptr == path) {
    return nullptr;
  }
  auto typeface = this->OnMakeFromFile(path, ttcIndex);
  if (!typeface) {
    return nullptr;
  }
  font_lst_.push_back(std::move(typeface));
  return font_lst_.back();
}

std::shared_ptr<Typeface> FontManager::GetDefaultTypeface(
    FontStyle font_style) const {
  return this->OnGetDefaultTypeface(font_style);
}

}  // namespace skity
