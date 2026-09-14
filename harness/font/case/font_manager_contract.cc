// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "harness/font/case/font_manager_contract.hpp"

#include <set>
#include <string>

namespace skity {
namespace font_harness {

void ValidateFontManagerCase(const Json::Value& root,
                             ValidationContext* errors) {
  const bool manager_category = root["category"] == "font_manager" ||
                                root["category"] == "family_style_set";
  if (!manager_category && !root.isMember("font_manager_request")) {
    return;
  }
  const Json::Value* request = nullptr;
  if (!RequireObjectField(root, "font_manager_request", "$", errors,
                          &request)) {
    return;
  }
  std::string entry;
  RequireStringField(*request, "entry", "$.font_manager_request", errors,
                     &entry);
  ValidateStringEnum(entry, "$.font_manager_request.entry",
                     {"GetDefaultTypeface", "MatchFamily", "MatchFamilyStyle",
                      "MatchFamilyStyleCharacter", "CreateStyleSet"},
                     errors);
  if (root["category"] == "family_style_set" && entry != "MatchFamily" &&
      entry != "CreateStyleSet") {
    errors->AddError("$.font_manager_request.entry",
                     "expected style set entry");
  }
  const auto& family = (*request)["family_name"];
  if (!family.isNull() && !family.isString()) {
    errors->AddError("$.font_manager_request.family_name",
                     "expected string or null");
  }
  if (entry == "CreateStyleSet" &&
      (!family.isString() || family.asString().empty())) {
    errors->AddError("$.font_manager_request.family_name",
                     "CreateStyleSet requires an enumerated family name");
  }
  if (request->isMember("style")) {
    const auto& style = (*request)["style"];
    if (RequireObject(style, "$.font_manager_request.style", errors)) {
      for (const char* key : {"weight", "width"}) {
        OptionalIntField(style, key, "$.font_manager_request.style", errors);
      }
      std::string slant;
      if (OptionalStringField(style, "slant", "$.font_manager_request.style",
                              errors, &slant)) {
        ValidateStringEnum(slant, "$.font_manager_request.style.slant",
                           {"upright", "italic", "oblique"}, errors);
      }
    }
  }
  if (entry == "MatchFamilyStyleCharacter" || request->isMember("character")) {
    std::string character;
    bool valid = RequireStringField(
        *request, "character", "$.font_manager_request", errors, &character);
    try {
      size_t end = 0;
      const auto scalar = std::stoul(character.substr(2), &end, 16);
      valid = valid && character.rfind("U+", 0) == 0 &&
              end == character.size() - 2 && scalar <= 0x10FFFF &&
              !(scalar >= 0xD800 && scalar <= 0xDFFF);
    } catch (...) {
      valid = false;
    }
    if (!valid) {
      errors->AddError("$.font_manager_request.character",
                       "expected Unicode scalar U+XXXX");
    }
  }
  if (request->isMember("bcp47")) {
    const auto& tags = (*request)["bcp47"];
    if (!tags.isArray()) {
      errors->AddError("$.font_manager_request.bcp47", "expected array");
    } else {
      for (const auto& tag : tags) {
        if (!tag.isString()) {
          errors->AddError("$.font_manager_request.bcp47",
                           "expected string language tags");
        }
      }
    }
  }
  int limit = 0;
  if (OptionalIntField(*request, "sample_limit", "$.font_manager_request",
                       errors, &limit) &&
      limit <= 0) {
    errors->AddError("$.font_manager_request.sample_limit",
                     "expected positive count");
  }
  if (!root.isMember("font_manager_expectation")) {
    return;
  }
  const auto& expectation = root["font_manager_expectation"];
  if (!RequireObject(expectation, "$.font_manager_expectation", errors)) {
    return;
  }
  for (const auto& key : expectation.getMemberNames()) {
    if (key == "matched") {
      OptionalBoolField(expectation, key, "$.font_manager_expectation", errors);
      if (entry == "MatchFamily" || entry == "CreateStyleSet") {
        errors->AddError("$.font_manager_expectation.matched",
                         "use style_count for style set entries");
      }
    } else if (key == "style_count" || key == "inventory_count") {
      int count = 0;
      if (OptionalIntField(expectation, key, "$.font_manager_expectation",
                           errors, &count)) {
        ValidateNonNegativeInt(count, "$.font_manager_expectation." + key,
                               errors);
      }
      if (key == "style_count" && entry != "MatchFamily" &&
          entry != "CreateStyleSet") {
        errors->AddError("$.font_manager_expectation.style_count",
                         "requires style set entry");
      }
    } else {
      errors->AddError("$.font_manager_expectation." + key,
                       "unknown expectation");
    }
  }
}

void ValidateFontManagerResult(const Json::Value& root,
                               const Json::Value& artifact,
                               ValidationContext* errors) {
  if (root["category"] != "font_manager" &&
      root["category"] != "family_style_set") {
    return;
  }
  const auto& probe = artifact["font_manager_probe"];
  if (!probe.isObject()) {
    errors->AddError("$.font_manager_probe", "expected probe object");
    return;
  }
  const auto& request = root["font_manager_request"];
  const auto& expectation = root["font_manager_expectation"];
  const auto& operation = probe["operation"];
  if (!probe.isObject() || !operation.isObject() ||
      operation["entry"] != request["entry"]) {
    errors->AddError("$.font_manager_probe.operation",
                     "missing or different operation");
    return;
  }
  const bool style_entry =
      request["entry"] == "MatchFamily" || request["entry"] == "CreateStyleSet";
  if (style_entry) {
    const auto& styles =
        operation[request["entry"] == "MatchFamily" ? "style_set"
                                                    : "create_style_set"];
    if (!styles.isObject() || !styles["style_count"].isInt() ||
        !styles["styles"].isArray()) {
      errors->AddError("$.font_manager_probe.operation",
                       "missing style inventory");
    } else {
      const int count = styles["style_count"].asInt();
      const auto& match = styles["match_style"];
      if (!match.isObject() || !match["typeface"].isObject() ||
          !match["typeface"]["available"].isBool() ||
          match["typeface"]["available"].asBool() != (count > 0)) {
        errors->AddError("$.font_manager_probe.operation.match_style",
                         "style set match does not agree with its count");
      }
      for (const auto& item : styles["styles"]) {
        if (!item.isObject() || !item["style"].isObject() ||
            !item["create_typeface"].isObject() ||
            item["create_typeface"]["available"] != true) {
          errors->AddError("$.font_manager_probe.operation.styles",
                           "style entry must contain an available typeface");
        }
      }
      const bool count_ok = expectation.isMember("style_count")
                                ? count == expectation["style_count"].asInt()
                                : count > 0;
      if (!count_ok) {
        errors->AddError("$.font_manager_probe.operation.style_count",
                         "style count violates expectation");
      }
      if (expectation.isMember("style_count") &&
          styles["styles"].size() != static_cast<Json::UInt>(count)) {
        errors->AddError("$.font_manager_probe.operation.styles",
                         "incomplete style inventory");
      }
    }
  } else {
    const auto& matches = probe["matched_typefaces"];
    // Cases without expectations compare availability between captures.
    if (!matches.isArray() || matches.size() != 1 || !matches[0].isObject() ||
        !matches[0]["available"].isBool() ||
        (root.isMember("font_manager_expectation") &&
         matches[0]["available"].asBool() !=
             expectation.get("matched", true).asBool())) {
      errors->AddError("$.font_manager_probe.matched_typefaces",
                       "match violates expectation");
    }
  }
  if (expectation.isMember("inventory_count")) {
    const auto& inventory = probe["font_manager"];
    const auto& names = inventory["family_names"];
    const int count = expectation["inventory_count"].asInt();
    std::set<std::string> unique;
    if (names.isArray()) {
      for (const auto& name : names) {
        if (name.isString()) {
          unique.insert(name.asString());
        }
      }
    }
    if (!inventory["family_count"].isInt() ||
        inventory["family_count"].asInt() != count || !names.isArray() ||
        names.size() != static_cast<Json::UInt>(count) ||
        unique.size() != names.size()) {
      errors->AddError("$.font_manager_probe.font_manager",
                       "incomplete or different family inventory");
    }
  }
}

}  // namespace font_harness
}  // namespace skity
