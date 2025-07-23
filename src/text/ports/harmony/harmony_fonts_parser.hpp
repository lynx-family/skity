// Copyright (c) 2023 Huawei Device Co., Ltd. All rights reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_TEXT_PORTS_HARMONY_HARMONY_FONTS_PARSER_HPP
#define SRC_TEXT_PORTS_HARMONY_HARMONY_FONTS_PARSER_HPP

#include <map>
#include <memory>
#include <skity/text/font_style.hpp>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "src/text/ports/freetype_face.hpp"
#include "third_party/jsoncpp/include/json/json.h"

namespace skity {

enum {
  NO_ERROR = 0,                       // no error
  ERROR_CONFIG_NOT_FOUND,             // the configuration document is not found
  ERROR_CONFIG_FORMAT_NOT_SUPPORTED,  // the formation of configuration is not
                                      // supported
  ERROR_CONFIG_MISSING_TAG,           // missing tag in the configuration
  ERROR_CONFIG_INVALID_VALUE_TYPE,    // invalid value type in the configuration
  ERROR_FONT_NOT_EXIST,               // the font file is not exist
  ERROR_FONT_INVALID_STREAM,          // the stream is not recognized
  ERROR_FONT_NO_STREAM,               // no stream in the font data
  ERROR_FAMILY_NOT_FOUND,     // the family name is not found in the system
  ERROR_NO_AVAILABLE_FAMILY,  // no available family in the system
  ERROR_DIR_NOT_FOUND,        // the directory is not exist

  ERROR_TYPE_COUNT,
};

/*!
 *  \brief To manage the related information of a 'fallbackFor' family name
 */
struct FallbackSetPos {
  unsigned int index;  // the index of the first font style set in the fallback
                       // set for a specified family name
  unsigned int
      count;  // the count of font style sets for a specified family name
};

/*!
 * \brief To manage the adjust information
 */
struct AdjustInfo {
  int origValue;  // the real value of the font weight
  int newValue;   // the specified value of weight for a font
};

/*!
 * \brief To manage the alias information of
 */
struct AliasInfo {
  int pos;     // the index of a font style set in generic family list.
  int weight;  // the weight of the font style set. 0 means no specified weight
};

/*!
 * \brief To manage the variation information
 */
// struct VariationInfo {
//   VariationInfo() : weight(-1), width(-1), slant(-1) {}
//   std::vector<Coordinate>
//       axis;    // the axis set such as 'wght', 'wdth' and 'slnt'.
//   int weight;  // the value of mapping weight
//   int width;   // the value of mapping width
//   int slant;   // the value of mapping slant
// };

/*!
 * \brief To manage the 'index' information for ttc fonts
 */
struct TtcIndexInfo {
  std::string
      familyName;  // the family name of the first typeface in a ttc font
  int ttcIndex;    // the index of a typeface in a ttc font
};

struct FontInfo {
  std::string specifiedName;  // the real family name of the font
  std::string familyName;     // the real family name of the font
  std::string fname;          // the full name of font file
  int index;                  // the index of the font in a ttc font
  FontStyle style;            // the font style
  bool isFixedWidth;  // the flag to indicate if the font has fixed width or not
};

/*!
 * \brief To manage the information for a generic family item
 */
struct GenericFamily {
  std::string familyName;  // the specified family name of the font style set
  std::shared_ptr<std::vector<FontInfo>>
      font_set;  // the typeface set of the font style set
  virtual ~GenericFamily() = default;
};

/*!
 * \brief To manage the information for a fallback family item
 */
struct FallbackInfo : GenericFamily {
  std::string langs;  // the language for which the font style set is
};

class HarmonyFontParser {
 public:
  explicit HarmonyFontParser(const FontScanner& scanner);

 private:
  std::vector<std::string> font_dirs_;
  std::unordered_map<std::string, TtcIndexInfo> ttc_index_map_;
  std::unordered_map<std::string, std::vector<AliasInfo>> alias_map_;
  std::unordered_map<std::string, std::vector<AdjustInfo>> adjust_map_;
  std::vector<std::unique_ptr<GenericFamily>> generic_family_set_;
  std::unordered_map<std::string, int> generic_name_map_;
  std::vector<std::unique_ptr<FallbackInfo>> fallback_set_;
  std::unordered_map<std::string, FallbackSetPos> fallback_for_map_;

  int CheckProductFile();
  bool JudgeFileExist();
  int CheckConfigFile(const char* fname, Json::Value& root);
  int ParseConfig(const char* fname);
  int ParseFontDir(const Json::Value& root);
  int ParseGeneric(const Json::Value& root);
  int ParseTtcIndex(const Json::Value& root, const std::string& family_name);
  int ParseAlias(const Json::Value& root, std::vector<AliasInfo>& alias_set);
  int ParseAdjust(const Json::Value& root, std::vector<AdjustInfo>& adjust_set);
  int ParseFallback(const Json::Value& root);
  int ParseFallbackItem(const Json::Value& root);

  int ScanFonts(const FontScanner& scanner);
  int LoadFont(const FontScanner& scanner, std::string fname,
               std::shared_ptr<Data> data);
  bool InsertTtcFont(int count, FontInfo& font);
  void PushFontIntoSet(const std::string& family_name, FontInfo font);

  void ResetGenericValue();
  void ResetFallbackValue();
  void SortFontSet(std::vector<FontInfo>* typefaceSet);
  void BuildSubFontSet(const std::vector<FontInfo>& typefaceSet,
                       std::vector<FontInfo>& subSet,
                       const std::string& familyName, int weight);

  int LogErrInfo(int err, const char* key,
                 Json::ValueType expected = Json::nullValue,
                 Json::ValueType actual = Json::nullValue);

  static std::unordered_map<std::string, std::shared_ptr<Data>> data_cache;

  friend class FontStyleSetHarmony;
  friend class FontManagerHarmony;
};

}  // namespace skity

#endif  // SRC_TEXT_PORTS_HARMONY_HARMONY_FONTS_PARSER_HPP
