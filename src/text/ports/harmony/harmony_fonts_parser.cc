// Copyright (c) 2023 Huawei Device Co., Ltd. All rights reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/text/ports/harmony/harmony_fonts_parser.hpp"

#include <dirent.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "src/logging.hpp"

namespace skity {

static const char* OHOS_DEFAULT_CONFIG = "/system/etc/fontconfig.json";
static const char* PRODUCT_DEFAULT_CONFIG =
    "/system/etc/productfontconfig.json";

std::unordered_map<std::string, std::shared_ptr<Data>>
    HarmonyFontParser::data_cache;

HarmonyFontParser::HarmonyFontParser(const FontScanner& scanner) {
  CheckProductFile();
  ScanFonts(scanner);
  ResetGenericValue();
  ResetFallbackValue();
}

int HarmonyFontParser::CheckProductFile() {
  int err = ParseConfig(PRODUCT_DEFAULT_CONFIG);
  if ((err != NO_ERROR) || (!JudgeFileExist())) {
    font_dirs_.clear();
    ttc_index_map_.clear();
    alias_map_.clear();
    adjust_map_.clear();
    generic_family_set_.clear();
    generic_name_map_.clear();
    fallback_set_.clear();
    // fallback_name_map_.clear();
    fallback_for_map_.clear();
    err = ParseConfig(OHOS_DEFAULT_CONFIG);
  }
  return err;
}

bool HarmonyFontParser::JudgeFileExist() {
  bool haveFile = false;
  for (unsigned int i = 0; i < font_dirs_.size(); i++) {
    DIR* dir = opendir(font_dirs_[i].c_str());
    if (dir == nullptr) {
      LogErrInfo(ERROR_DIR_NOT_FOUND, font_dirs_[i].c_str());
      continue;
    }
    struct dirent* node = nullptr;

    while ((node = readdir(dir))) {
      if (node->d_type != DT_REG) {
        continue;
      }

      const char* fileName = node->d_name;
      int len = strlen(fileName);
      int suffixLen = strlen(".ttf");
      if (len < suffixLen ||
          (strncmp(fileName + len - suffixLen, ".ttf", suffixLen) &&
           strncmp(fileName + len - suffixLen, ".otf", suffixLen) &&
           strncmp(fileName + len - suffixLen, ".ttc", suffixLen) &&
           strncmp(fileName + len - suffixLen, ".otc", suffixLen))) {
        continue;
      }
      haveFile = true;
      break;
    }
    (void)closedir(dir);
    if (haveFile) {
      break;
    }
  }
  return haveFile;
}

int HarmonyFontParser::CheckConfigFile(const char* fname, Json::Value& root) {
  auto file_data = Data::MakeFromFileName(fname);
  if (file_data == nullptr || file_data->Size() == 0) {
    return LogErrInfo(ERROR_CONFIG_NOT_FOUND, fname);
  }
  std::string errs;
  Json::CharReaderBuilder charReaderBuilder;
  std::unique_ptr<Json::CharReader> jsonReader(
      charReaderBuilder.newCharReader());
  char const* begin_doc = reinterpret_cast<char const*>(file_data->RawData());
  bool isJson =
      jsonReader->parse(begin_doc, begin_doc + file_data->Size(), &root, &errs);

  if (!isJson || !errs.empty()) {
    return LogErrInfo(ERROR_CONFIG_FORMAT_NOT_SUPPORTED, fname);
  }
  return NO_ERROR;
}

int HarmonyFontParser::ParseConfig(const char* fname) {
  Json::Value root;
  int err = CheckConfigFile(fname, root);
  if (err != NO_ERROR) {
    return err;
  }
  // "fontdir" - optional, the data type should be string
  const char* key = "fontdir";
  if (root.isMember(key)) {
    if (root[key].isArray()) {
      ParseFontDir(root[key]);
    } else {
      return LogErrInfo(ERROR_CONFIG_INVALID_VALUE_TYPE, key);
    }
  }

  // "generic", "fallback" - necessary, the data type should be array
  const char* keys[] = {"generic", "fallback", nullptr};
  int index = 0;
  while (true) {
    if (keys[index] == nullptr) {
      break;
    }
    key = keys[index++];
    if (!root.isMember(key)) {
      return LogErrInfo(ERROR_CONFIG_MISSING_TAG, key);
    } else if (!root[key].isArray()) {
      return LogErrInfo(ERROR_CONFIG_INVALID_VALUE_TYPE, key, Json::arrayValue,
                        root[key].type());
    }
    const Json::Value& arr = root[key];
    for (unsigned int i = 0; i < arr.size(); i++) {
      if (arr[i].isObject()) {
        if (!strcmp(key, "generic")) {
          ParseGeneric(arr[i]);
        } else if (!strcmp(key, "fallback")) {
          ParseFallback(arr[i]);
        }
      } else {
        std::stringstream errKey;
        errKey << key << "#" << (i + 1);
        (void)LogErrInfo(ERROR_CONFIG_INVALID_VALUE_TYPE, errKey.str().c_str(),
                         Json::objectValue, arr[i].type());
      }
    }
  }
  root.clear();
  return NO_ERROR;
}

int HarmonyFontParser::ParseFontDir(const Json::Value& root) {
  for (unsigned int i = 0; i < root.size(); i++) {
    if (root[i].isString()) {
      font_dirs_.emplace_back(root[i].asString());
    } else {
      std::stringstream text;
      text << "fontdir#" << (i + 1);
      return LogErrInfo(ERROR_CONFIG_INVALID_VALUE_TYPE, text.str().c_str(),
                        Json::stringValue, root[i].type());
    }
  }
  return NO_ERROR;
}

int HarmonyFontParser::ParseGeneric(const Json::Value& root) {
  // "family" - necessary, the data type should be String
  const char* key = "family";
  if (!root.isMember(key)) {
    return LogErrInfo(ERROR_CONFIG_MISSING_TAG, key);
  } else if (!root[key].isString()) {
    return LogErrInfo(ERROR_CONFIG_INVALID_VALUE_TYPE, key, Json::stringValue,
                      root[key].type());
  }
  std::string familyName = root[key].asString();
  // "alias" - necessary, the data type should be Array
  if (!root.isMember("alias")) {
    return LogErrInfo(ERROR_CONFIG_MISSING_TAG, "alias");
  }
  // "adjust", "variation" - optional
  const char* tags[] = {"alias", "adjust", "variations", "index"};
  std::vector<AliasInfo> alias_set;
  std::vector<AdjustInfo> adjust_set;
  // std::vector<VariationInfo> variationSet;
  for (unsigned int i = 0; i < sizeof(tags) / sizeof(char*); i++) {
    key = tags[i];
    if (!root.isMember(key)) {
      continue;
    }
    if (root[key].isArray()) {
      if (!strcmp(key, "index")) {
        ParseTtcIndex(root[key], familyName);
        continue;
      }
      const Json::Value& arr = root[key];
      for (unsigned int j = 0; j < arr.size(); j++) {
        if (arr[j].isObject()) {
          if (!strcmp(key, "alias")) {
            ParseAlias(arr[j], alias_set);
          } else if (!strcmp(key, "adjust")) {
            ParseAdjust(arr[j], adjust_set);
          } else {
            // parseVariation(arr[j], variationSet);
          }
        } else {
          std::stringstream text;
          text << key << "#" << (j + 1);
          (void)LogErrInfo(ERROR_CONFIG_INVALID_VALUE_TYPE, text.str().c_str(),
                           Json::objectValue, arr[j].type());
        }
      }
    } else {
      (void)LogErrInfo(ERROR_CONFIG_INVALID_VALUE_TYPE, key, Json::arrayValue,
                       root[key].type());
    }
    if (root.size() == 2) {
      break;
    }
  }
  if (alias_set.size()) {
    alias_map_.emplace(familyName, std::move(alias_set));
  }
  if (adjust_set.size()) {
    adjust_map_.emplace(familyName, std::move(adjust_set));
  }
  // if (variationSet.size()) {
  //     variationMap.set(SkString(familyName), variationSet);
  // }
  return NO_ERROR;
}

int HarmonyFontParser::ParseTtcIndex(const Json::Value& root,
                                     const std::string& family_name) {
  unsigned int keyCount = 2;  // the value of 'index' is an array with 2 items.
  if (root.size() == keyCount && root[0].isString() && root[1].isNumeric()) {
    TtcIndexInfo item = {root[0].asString(), root[1].asInt()};
    if (item.ttcIndex != 0 &&
        ttc_index_map_.find(item.familyName) == ttc_index_map_.end()) {
      ttc_index_map_.emplace(item.familyName, TtcIndexInfo{item.familyName, 0});
    }
    ttc_index_map_.emplace(family_name, item);
  } else {
    int ret = ERROR_CONFIG_INVALID_VALUE_TYPE;
    return LogErrInfo(ret, "parsing ttc index fialed");
  }
  return NO_ERROR;
}

int HarmonyFontParser::ParseAlias(const Json::Value& root,
                                  std::vector<AliasInfo>& alias_set) {
  if (root.empty()) {
    return LogErrInfo(ERROR_CONFIG_MISSING_TAG, "generic-alias-name");
  }
  Json::Value::Members members = root.getMemberNames();
  std::string& key = members[0];
  if (!root[key].isInt()) {
    return LogErrInfo(ERROR_CONFIG_INVALID_VALUE_TYPE, "generic-alias-weight",
                      Json::intValue, root[key].type());
  }

  int weight = root[key].asInt();
  std::unique_ptr<GenericFamily> genericFamily =
      std::make_unique<GenericFamily>();
  genericFamily->familyName = key;
  if (alias_set.size() == 0 || weight > 0) {
    genericFamily->font_set = std::make_shared<std::vector<FontInfo>>();
  } else {
    int index = alias_set[0].pos;
    genericFamily->font_set = generic_family_set_[index]->font_set;
  }
  generic_name_map_.emplace(genericFamily->familyName,
                            generic_family_set_.size());
  alias_set.emplace_back(
      AliasInfo{static_cast<int>(generic_family_set_.size()), weight});
  generic_family_set_.emplace_back(std::move(genericFamily));
  return NO_ERROR;
}

int HarmonyFontParser::ParseAdjust(const Json::Value& root,
                                   std::vector<AdjustInfo>& adjust_set) {
  const char* tags[] = {"weight", "to"};
  int values[2];  // value[0] - to save 'weight', value[1] - to save 'to'
  for (unsigned int i = 0; i < sizeof(tags) / sizeof(char*); i++) {
    const char* key = tags[i];
    if (!root.isMember(key)) {
      return LogErrInfo(ERROR_CONFIG_MISSING_TAG, key);
    } else if (!root[key].isInt()) {
      return LogErrInfo(ERROR_CONFIG_INVALID_VALUE_TYPE, key, Json::intValue,
                        root[key].type());
    } else {
      values[i] = root[key].asInt();
    }
  }
  adjust_set.emplace_back(AdjustInfo{values[0], values[1]});
  return NO_ERROR;
}

int HarmonyFontParser::ParseFallback(const Json::Value& root) {
  if (root.empty()) {
    return LogErrInfo(ERROR_CONFIG_MISSING_TAG, "fallback-fallbackFor");
  }
  Json::Value::Members members = root.getMemberNames();
  const char* key = members[0].c_str();
  if (!root[key].isArray()) {
    return LogErrInfo(ERROR_CONFIG_INVALID_VALUE_TYPE, "fallback-items",
                      Json::arrayValue, root[key].type());
  }
  unsigned int startPos = fallback_set_.size();
  std::string fallbackFor = key;
  const Json::Value& fallbackArr = root[key];
  for (unsigned int i = 0; i < fallbackArr.size(); i++) {
    if (!fallbackArr[i].isObject()) {
      std::stringstream text;
      text << "fallback-" << key << "#" << (i + 1);
      (void)LogErrInfo(ERROR_CONFIG_INVALID_VALUE_TYPE, text.str().c_str(),
                       Json::objectValue, fallbackArr[i].type());
      continue;
    }
    ParseFallbackItem(fallbackArr[i]);
  }
  FallbackSetPos setPos = {startPos,
                           (unsigned int)(fallback_set_.size() - startPos)};
  fallback_for_map_.emplace(fallbackFor, setPos);
  return NO_ERROR;
}

int HarmonyFontParser::ParseFallbackItem(const Json::Value& root) {
  if (root.empty()) {
    return LogErrInfo(ERROR_CONFIG_MISSING_TAG, "fallback-item-lang");
  }
  Json::Value::Members members = root.getMemberNames();
  const char* key = nullptr;
  bool hasIndex = false;
  bool hasVariations = false;
  for (unsigned int i = 0; i < members.size(); i++) {
    if (members[i] == "variations") {
      hasVariations = true;
    } else if (members[i] == "index") {
      hasIndex = true;
    } else {
      key = members[i].c_str();
    }
  }
  if (key == nullptr) {
    return LogErrInfo(ERROR_CONFIG_MISSING_TAG, "fallback-item-lang");
  }
  if (!root[key].isString()) {
    return LogErrInfo(ERROR_CONFIG_INVALID_VALUE_TYPE, "fallback-item-family",
                      Json::stringValue, root[key].type());
  }
  std::string lang = key;
  std::string familyName = root[key].asString();
  if (hasVariations) {
    // font variations
  }
  if (hasIndex) {
    key = "index";
    if (root[key].isArray()) {
      ParseTtcIndex(root[key], familyName);
    } else {
      (void)LogErrInfo(ERROR_CONFIG_INVALID_VALUE_TYPE, key, Json::arrayValue,
                       root[key].type());
    }
  }
  std::unique_ptr<FallbackInfo> fallback = std::make_unique<FallbackInfo>();
  fallback->familyName = familyName;
  fallback->langs = lang;
  fallback->font_set = std::make_shared<std::vector<FontInfo>>();
  // fallback_name_map_.emplace(familyName, fallback_set_.size());
  fallback_set_.emplace_back(std::move(fallback));
  return NO_ERROR;
}

int HarmonyFontParser::ScanFonts(const FontScanner& scanner) {
  int err = NO_ERROR;
  if (font_dirs_.size() == 0) {
    font_dirs_.emplace_back("/system/fonts/");
  }
  for (unsigned int i = 0; i < font_dirs_.size(); i++) {
    DIR* dir = opendir(font_dirs_[i].c_str());
    if (dir == nullptr) {
      err = LogErrInfo(ERROR_DIR_NOT_FOUND, font_dirs_[i].c_str());
      continue;
    }
    struct dirent* node = nullptr;
    while ((node = readdir(dir))) {
      if (node->d_type != DT_REG) {
        continue;
      }
      const char* fname = node->d_name;
      int len = strlen(fname);
      int suffixLen = strlen(".ttf");
      if (len < suffixLen ||
          (strncmp(fname + len - suffixLen, ".ttf", suffixLen) &&
           strncmp(fname + len - suffixLen, ".otf", suffixLen) &&
           strncmp(fname + len - suffixLen, ".ttc", suffixLen) &&
           strncmp(fname + len - suffixLen, ".otc", suffixLen))) {
        continue;
      }
      std::stringstream full_name_stream;
      full_name_stream << font_dirs_[i];
      if (font_dirs_[i][font_dirs_[i].size() - 1] != '/') {
        full_name_stream << "/";
      }
      full_name_stream << fname;
      std::shared_ptr<Data> data;
      std::string full_name = full_name_stream.str();
      if (data_cache.find(full_name) == data_cache.end()) {
        data = Data::MakeFromFileMapping(full_name.c_str());
        if (data) {
          data_cache.emplace(full_name, data);
        }
      } else {
        data = data_cache.at(full_name);
      }
      LoadFont(scanner, full_name, data);
    }
    closedir(dir);
  }
  font_dirs_.clear();
  return err;
}

int HarmonyFontParser::LoadFont(const FontScanner& scanner, std::string fname,
                                std::shared_ptr<Data> data) {
  int count = 1;
  FontScanner::AxisDefinitions axis_defs;
  FontInfo font;
  font.fname = fname;
  font.index = 0;
  if (!data || !scanner.RecognizedFont(data, &count) ||
      !scanner.ScanFont(data, 0, &font.familyName, &font.style,
                        &font.isFixedWidth, &axis_defs)) {
    int err = NO_ERROR;
    if (!data) {
      err = ERROR_FONT_NOT_EXIST;
    } else {
      err = ERROR_FONT_INVALID_STREAM;
    }
    LogErrInfo(err, fname.c_str());
    return err;
  }
  // for adjustMap - update weight
  if (adjust_map_.find(font.familyName) != adjust_map_.end()) {
    const std::vector<AdjustInfo>& adjustSet = adjust_map_.at(font.familyName);
    for (unsigned int i = 0; i < adjustSet.size(); i++) {
      if (font.style.weight() == adjustSet[i].origValue) {
        font.style = FontStyle(adjustSet[i].newValue, font.style.width(),
                               font.style.slant());
        break;
      }
    }
  }
  bool ret = false;
  if (count > 1) {
    ret = InsertTtcFont(count, font);
  } else if (axis_defs.size() > 0) {
    // ret = insertVariableFont(axisDefs, font);
  }
  if (!ret) {
    // std::string specified_name;
    // std::vector<FontInfo>* tpSet = GetFontSet(font.familyName,
    // &specified_name); if (tpSet) {
    //   font.specifiedName = specified_name;
    //   tpSet->push_back(font);
    // }
    std::string family_name = font.familyName;
    PushFontIntoSet(family_name, std::move(font));
  }
  return NO_ERROR;
}

bool HarmonyFontParser::InsertTtcFont(int count, FontInfo& font) {
  bool ret = false;
  for (auto& [family_name, ttc_info] : ttc_index_map_) {
    if (ttc_info.familyName == font.familyName && ttc_info.ttcIndex < count) {
      // std::string specified_name;
      // std::vector<FontInfo>* tpSet =
      //     this->GetFontSet(family_name, &specified_name);
      // if (tpSet) {
      //   FontInfo newFont(font);
      //   newFont.familyName = family_name;
      //   newFont.index = ttc_info.ttcIndex;
      //   newFont.specifiedName = specified_name;
      //   tpSet->push_back(std::move(newFont));
      //   ret = true;
      // }

      FontInfo newFont(font);
      newFont.familyName = family_name;
      newFont.index = ttc_info.ttcIndex;
      PushFontIntoSet(family_name, std::move(newFont));
    }
  }
  return ret;
}

void HarmonyFontParser::PushFontIntoSet(const std::string& family_name,
                                        FontInfo font) {
  if (alias_map_.find(family_name) != alias_map_.end()) {
    const std::vector<AliasInfo>& aliasSet = alias_map_.at(family_name);
    if (aliasSet.size() > 0) {
      int index = aliasSet[0].pos;
      font.specifiedName = generic_family_set_[index]->familyName;
      generic_family_set_[index]->font_set->push_back(std::move(font));
    }
  } else {
    // Do not use a map to store family names, as they are not unique. Instead,
    // we must traverse all fallback information to identify duplicates each
    // time.
    for (auto& fallback : fallback_set_) {
      if (fallback->familyName == family_name) {
        // Do not move for the reasons stated above.
        fallback->font_set->push_back(font);
      }
    }
  }
}

void HarmonyFontParser::ResetGenericValue() {
  for (auto& [family_name, alias_info_set] : alias_map_) {
    int index = alias_info_set[0].pos;
    if (generic_family_set_[index]->font_set->size() == 0) {
      this->LogErrInfo(ERROR_FAMILY_NOT_FOUND, family_name.c_str());
    } else {
      SortFontSet(generic_family_set_[index]->font_set.get());
      for (unsigned int i = 1; i < alias_info_set.size(); i++) {
        if (alias_info_set[i].weight == 0) {
          continue;
        }
        BuildSubFontSet(*generic_family_set_[index]->font_set.get(),
                        *generic_family_set_[index + i]->font_set.get(),
                        generic_family_set_[index + i]->familyName,
                        alias_info_set[i].weight);
        if (generic_family_set_[index + i]->font_set->size() == 0) {
          this->LogErrInfo(ERROR_FAMILY_NOT_FOUND,
                           generic_family_set_[index + i]->familyName.c_str());
        }
      }
    }
  }

  alias_map_.clear();
  adjust_map_.clear();
  // variationMap.reset();
  ttc_index_map_.clear();
}

void HarmonyFontParser::BuildSubFontSet(
    const std::vector<FontInfo>& typefaceSet, std::vector<FontInfo>& subSet,
    const std::string& familyName, int weight) {
  if (typefaceSet.size() == 0) {
    return;
  }
  for (unsigned int i = 0; i < typefaceSet.size(); i++) {
    const FontInfo& typeface = typefaceSet[i];
    if (typeface.style.weight() == weight) {
      FontInfo font(typeface);
      subSet.push_back(typeface);
    }
  }
}

void HarmonyFontParser::ResetFallbackValue() {
  for (unsigned int i = 0; i < fallback_set_.size(); i++) {
    if (fallback_set_[i]->font_set->size() == 0) {
      LogErrInfo(ERROR_FAMILY_NOT_FOUND, fallback_set_[i]->familyName.c_str());
    }
    SortFontSet(fallback_set_[i]->font_set.get());
  }
}

bool FontInfoComparator(const FontInfo& left, const FontInfo& right) {
  return left.style.weight() < right.style.weight() ||
         (left.style.weight() == right.style.weight() &&
          left.style.slant() < right.style.slant());
}

void HarmonyFontParser::SortFontSet(std::vector<FontInfo>* font_set) {
  std::sort(font_set->begin(), font_set->end(), FontInfoComparator);
}

int HarmonyFontParser::LogErrInfo(int err, const char* key,
                                  Json::ValueType expected,
                                  Json::ValueType actual) {
  std::stringstream err_str;
  err_str << "err = " << err << ", key msg = " << key;
  static const char* types[] = {
      "null", "int", "unit", "real", "string", "boolean", "array", "object",
  };
  int size = sizeof(types) / sizeof(char*);
  if ((expected >= 0 && expected < size) && (actual >= 0 && actual < size)) {
    err_str << ", expected = " << types[expected]
            << ", actual = " << types[actual];
  }
  return err;
}

}  // namespace skity
