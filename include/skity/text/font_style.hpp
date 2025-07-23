/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_TEXT_FONT_STYLE_HPP
#define INCLUDE_SKITY_TEXT_FONT_STYLE_HPP

#include <algorithm>
#include <skity/macros.hpp>

namespace skity {

class SKITY_API FontStyle {
 public:
  enum Weight {
    kInvisible_Weight = 0,
    kThin_Weight = 100,
    kExtraLight_Weight = 200,
    kLight_Weight = 300,
    kNormal_Weight = 400,
    kMedium_Weight = 500,
    kSemiBold_Weight = 600,
    kBold_Weight = 700,
    kExtraBold_Weight = 800,
    kBlack_Weight = 900,
    kExtraBlack_Weight = 1000,
  };

  enum Width {
    kUltraCondensed_Width = 1,
    kExtraCondensed_Width = 2,
    kCondensed_Width = 3,
    kSemiCondensed_Width = 4,
    kNormal_Width = 5,
    kSemiExpanded_Width = 6,
    kExpanded_Width = 7,
    kExtraExpanded_Width = 8,
    kUltraExpanded_Width = 9,
  };

  enum Slant {
    kUpright_Slant,
    kItalic_Slant,
    kOblique_Slant,
  };

  FontStyle(int weight, int width, Slant slant);

  FontStyle();

  bool operator==(const FontStyle& rhs) const { return value_ == rhs.value_; }

  int weight() const { return value_ & 0xFFFF; }
  int width() const { return (value_ >> 16) & 0xFF; }
  Slant slant() const { return (Slant)((value_ >> 24) & 0xFF); }

  static FontStyle Normal() {
    return FontStyle(kNormal_Weight, kNormal_Width, kUpright_Slant);
  }
  static FontStyle Bold() {
    return FontStyle(kBold_Weight, kNormal_Width, kUpright_Slant);
  }
  static FontStyle Italic() {
    return FontStyle(kNormal_Weight, kNormal_Width, kItalic_Slant);
  }
  static FontStyle BoldItalic() {
    return FontStyle(kBold_Weight, kNormal_Width, kItalic_Slant);
  }

  static FontStyle::Width WidthFromAxisWidth(float axis_width);

 private:
  int32_t value_;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_TEXT_FONT_STYLE_HPP
