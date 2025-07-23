/*
 * Copyright 2011 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_TEXT_PORTS_ANDROID_FONTS_PARSER_HPP
#define SRC_TEXT_PORTS_ANDROID_FONTS_PARSER_HPP

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace skity {

enum class FontVariants {
  kDefault_FontVariant = 0x01,
  kCompact_FontVariant = 0x02,
  kElegant_FontVariant = 0x04,
  kLast_FontVariant = kElegant_FontVariant,
};

class FontFileInfo {
 public:
  std::string file_name;
  int index = 0;
  int weight = 0;
  enum class Style { kAuto, kNormal, kItalic } style = Style::kAuto;
  std::map<std::string, float> axis_tags;
};

class FontFamily {
 public:
  FontFamily(std::string arg_base_path, bool arg_is_fallback)
      : variant(FontVariants::kDefault_FontVariant),
        order(-1),
        base_path(std::move(arg_base_path)),
        is_fallback_font(arg_is_fallback) {}

  std::vector<std::string> names;
  std::vector<std::string> languages;
  std::vector<FontFileInfo> fonts;
  std::map<std::string, std::unique_ptr<FontFamily>> fallback_families;
  FontVariants variant;
  int order;  // internal to the parser, not useful to users.
  std::string base_path;
  bool is_fallback_font;
  std::string fallback_for;
};

class FontResources {
 public:
  static std::unique_ptr<FontResources> MakeFromFile();

  std::vector<FontFamily>& FontFamilies() { return font_families_; }

 private:
  explicit FontResources(std::vector<FontFamily> font_families);

  std::vector<FontFamily> font_families_;
};

}  // namespace skity

#endif  // SRC_TEXT_PORTS_ANDROID_FONTS_PARSER_HPP
