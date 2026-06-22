// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "harness/font/compare/compare_engine.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "harness/font/artifact/artifact_validator.hpp"
#include "harness/font/artifact/artifact_writer.hpp"
#include "harness/font/artifact/json_io.hpp"
#include "harness/font/case/case_document.hpp"
#include "harness/font/case/validation.hpp"

namespace skity {
namespace font_harness {

namespace {

constexpr double kDefaultMetricEpsilon = 0.001;
constexpr double kDefaultPathEpsilon = 0.0001;
constexpr size_t kMaxDiffs = 64;

struct CompareConfig {
  std::string category;
  std::string typeface_identity = "normalized_descriptor";
  std::string font_style = "exact";
  std::string font_metrics_mode = "epsilon";
  std::string glyph_metrics_mode = "epsilon";
  std::string glyph_path_mode = "path_normalized";
  std::string glyph_image_mode = "exact";
  double font_metrics_epsilon = kDefaultMetricEpsilon;
  double glyph_metrics_epsilon = kDefaultMetricEpsilon;
  double glyph_path_epsilon = kDefaultPathEpsilon;
};

struct Diff {
  std::string path;
  std::string reason_code;
  std::string rule;
  std::string message;
  Json::Value expected;
  Json::Value actual;
};

std::string StemOrFallback(const std::filesystem::path& path,
                           const std::string& fallback) {
  const std::string stem = path.stem().string();
  return stem.empty() ? fallback : stem;
}

bool ReadStringField(const Json::Value& root, const std::string& field,
                     std::string* out) {
  if (!root.isObject() || !root.isMember(field) || !root[field].isString()) {
    return false;
  }
  *out = root[field].asString();
  return true;
}

std::string PathArg(const std::filesystem::path& path) {
  return path.generic_string();
}

std::string TrimNullPadding(std::string value) {
  while (!value.empty() && value.back() == '\0') {
    value.pop_back();
  }
  return value;
}

std::string ChildJsonPath(const std::string& path, const std::string& field) {
  return path.empty() ? field : path + "." + field;
}

std::string IndexJsonPath(const std::string& path, Json::ArrayIndex index) {
  return path + "[" + std::to_string(index) + "]";
}

const Json::Value* ObjectMember(const Json::Value& root,
                                const std::string& field) {
  if (!root.isObject() || !root.isMember(field)) {
    return nullptr;
  }
  return &root[field];
}

bool IsIgnoreMode(const std::string& mode) { return mode == "ignore"; }

void AddDiff(std::vector<Diff>* diffs, const std::string& path,
             const std::string& reason_code, const std::string& rule,
             const std::string& message, const Json::Value& expected,
             const Json::Value& actual) {
  if (diffs->size() >= kMaxDiffs) {
    return;
  }
  Diff diff;
  diff.path = path;
  diff.reason_code = reason_code;
  diff.rule = rule;
  diff.message = message;
  diff.expected = expected;
  diff.actual = actual;
  diffs->push_back(std::move(diff));
}

Json::Value MissingValue() {
  Json::Value value(Json::objectValue);
  value["missing"] = true;
  return value;
}

Json::Value ToDiffsJson(const std::vector<Diff>& diffs) {
  Json::Value value(Json::arrayValue);
  for (const auto& diff : diffs) {
    Json::Value item(Json::objectValue);
    item["path"] = diff.path;
    item["reason_code"] = diff.reason_code;
    item["rule"] = diff.rule;
    item["message"] = diff.message;
    item["expected"] = diff.expected;
    item["actual"] = diff.actual;
    value.append(std::move(item));
  }
  return value;
}

bool SameJsonScalar(const Json::Value& expected, const Json::Value& actual) {
  if (expected.isString() && actual.isString()) {
    return TrimNullPadding(expected.asString()) ==
           TrimNullPadding(actual.asString());
  }
  if (expected.isBool() && actual.isBool()) {
    return expected.asBool() == actual.asBool();
  }
  if (expected.isNull() && actual.isNull()) {
    return true;
  }
  if (expected.isIntegral() && actual.isIntegral()) {
    return expected.asLargestInt() == actual.asLargestInt();
  }
  return expected == actual;
}

void CompareNumber(const Json::Value& expected, const Json::Value& actual,
                   const std::string& path, double epsilon,
                   const std::string& reason_code, const std::string& rule,
                   std::vector<Diff>* diffs) {
  if (!expected.isNumeric() || !actual.isNumeric()) {
    AddDiff(diffs, path, reason_code, rule, "expected numeric values", expected,
            actual);
    return;
  }
  const double lhs = expected.asDouble();
  const double rhs = actual.asDouble();
  if (!std::isfinite(lhs) || !std::isfinite(rhs) ||
      std::fabs(lhs - rhs) > epsilon) {
    AddDiff(diffs, path, reason_code, rule,
            "numeric values differ beyond "
            "epsilon",
            expected, actual);
  }
}

void CompareJsonSubset(const Json::Value& expected, const Json::Value& actual,
                       const std::string& path, const std::string& reason_code,
                       const std::string& rule, double epsilon,
                       bool use_epsilon, std::vector<Diff>* diffs) {
  if (diffs->size() >= kMaxDiffs) {
    return;
  }

  if (expected.isNumeric()) {
    if (use_epsilon) {
      CompareNumber(expected, actual, path, epsilon, reason_code, rule, diffs);
    } else if (!expected.isNumeric() || !actual.isNumeric() ||
               expected.asDouble() != actual.asDouble()) {
      AddDiff(diffs, path, reason_code, rule, "numeric values differ", expected,
              actual);
    }
    return;
  }

  if (expected.isObject()) {
    if (!actual.isObject()) {
      AddDiff(diffs, path, reason_code, rule, "expected object", expected,
              actual);
      return;
    }
    std::vector<std::string> names;
    names.reserve(expected.size());
    for (const auto& name : expected.getMemberNames()) {
      names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    for (const auto& name : names) {
      const std::string child_path = ChildJsonPath(path, name);
      if (!actual.isMember(name)) {
        AddDiff(diffs, child_path, reason_code, rule, "field is missing",
                expected[name], MissingValue());
        continue;
      }
      CompareJsonSubset(expected[name], actual[name], child_path, reason_code,
                        rule, epsilon, use_epsilon, diffs);
    }
    return;
  }

  if (expected.isArray()) {
    if (!actual.isArray()) {
      AddDiff(diffs, path, reason_code, rule, "expected array", expected,
              actual);
      return;
    }
    if (expected.size() != actual.size()) {
      AddDiff(diffs, path, reason_code, rule, "array sizes differ", expected,
              actual);
      return;
    }
    for (Json::ArrayIndex i = 0; i < expected.size(); ++i) {
      CompareJsonSubset(expected[i], actual[i], IndexJsonPath(path, i),
                        reason_code, rule, epsilon, use_epsilon, diffs);
    }
    return;
  }

  if (!SameJsonScalar(expected, actual)) {
    AddDiff(diffs, path, reason_code, rule, "values differ", expected, actual);
  }
}

Json::Value OptionalStringValue(const Json::Value& root,
                                const std::string& field) {
  if (!root.isObject() || !root.isMember(field) || !root[field].isString()) {
    return Json::Value(Json::nullValue);
  }
  return Json::Value(TrimNullPadding(root[field].asString()));
}

Json::Value OptionalValue(const Json::Value& root, const std::string& field) {
  if (!root.isObject() || !root.isMember(field)) {
    return Json::Value(Json::nullValue);
  }
  return root[field];
}

void AddIfPresent(Json::Value* target, const std::string& field,
                  const Json::Value& value) {
  if (!value.isNull()) {
    (*target)[field] = value;
  }
}

Json::Value NormalizeStyle(const Json::Value& style) {
  Json::Value value(Json::objectValue);
  if (!style.isObject()) {
    return value;
  }
  AddIfPresent(&value, "weight", OptionalValue(style, "weight"));
  AddIfPresent(&value, "width", OptionalValue(style, "width"));
  AddIfPresent(&value, "slant", OptionalStringValue(style, "slant"));
  return value;
}

Json::Value NormalizeDescriptor(const Json::Value& descriptor,
                                const Json::Value* owner = nullptr) {
  Json::Value value(Json::objectValue);
  if (!descriptor.isObject()) {
    value["available"] = false;
    return value;
  }

  AddIfPresent(&value, "family_name",
               OptionalStringValue(descriptor, "family_name"));
  AddIfPresent(&value, "post_script_name",
               OptionalStringValue(descriptor, "post_script_name"));
  AddIfPresent(&value, "full_name",
               OptionalStringValue(descriptor, "full_name"));
  AddIfPresent(&value, "collection_index",
               OptionalValue(descriptor, "collection_index"));
  if (!value.isMember("collection_index") && owner != nullptr) {
    AddIfPresent(&value, "collection_index",
                 OptionalValue(*owner, "collection_index"));
  }
  if (descriptor.isMember("style")) {
    value["style"] = NormalizeStyle(descriptor["style"]);
  }
  return value;
}

Json::Value NormalizeTypefaceIdentity(const Json::Value& identity,
                                      const Json::Value* owner = nullptr) {
  Json::Value value(Json::objectValue);
  if (!identity.isObject()) {
    return value;
  }

  AddIfPresent(&value, "family_name",
               OptionalStringValue(identity, "family_name"));
  AddIfPresent(&value, "post_script_name",
               OptionalStringValue(identity, "post_script_name"));
  AddIfPresent(&value, "full_name", OptionalStringValue(identity, "full_name"));
  AddIfPresent(&value, "collection_index",
               OptionalValue(identity, "collection_index"));
  if (!value.isMember("collection_index") && owner != nullptr) {
    AddIfPresent(&value, "collection_index",
                 OptionalValue(*owner, "collection_index"));
  }
  if (identity.isMember("style")) {
    value["style"] = NormalizeStyle(identity["style"]);
  }
  AddIfPresent(&value, "units_per_em", OptionalValue(identity, "units_per_em"));
  AddIfPresent(&value, "glyph_count", OptionalValue(identity, "glyph_count"));
  AddIfPresent(&value, "table_count", OptionalValue(identity, "table_count"));
  AddIfPresent(&value, "variation_position",
               OptionalValue(identity, "variation_position"));
  AddIfPresent(&value, "variation_axes",
               OptionalValue(identity, "variation_axes"));
  return value;
}

Json::Value NormalizeTypefaceSelectionIdentity(const Json::Value& typeface) {
  const Json::Value* identity = ObjectMember(typeface, "identity");
  if (identity != nullptr && identity->isObject()) {
    Json::Value normalized = NormalizeTypefaceIdentity(*identity, &typeface);
    if (!normalized.empty()) {
      return normalized;
    }
  }
  return NormalizeDescriptor(OptionalValue(typeface, "descriptor"), &typeface);
}

const Json::Value* FindTypefaceDescriptor(const Json::Value& root) {
  const Json::Value* typeface_result = ObjectMember(root, "typeface_result");
  if (typeface_result == nullptr || !typeface_result->isObject()) {
    return nullptr;
  }
  return ObjectMember(*typeface_result, "typeface");
}

const Json::Value* FindTypefaceIdentity(const Json::Value& root) {
  const Json::Value* typeface_result = ObjectMember(root, "typeface_result");
  if (typeface_result == nullptr || !typeface_result->isObject()) {
    return nullptr;
  }
  return ObjectMember(*typeface_result, "identity");
}

Json::Value NormalizeTypefaceDescriptor(const Json::Value& root) {
  const Json::Value* typeface_result = ObjectMember(root, "typeface_result");
  const Json::Value* descriptor = FindTypefaceDescriptor(root);
  if (descriptor == nullptr) {
    return Json::Value(Json::objectValue);
  }
  return NormalizeDescriptor(*descriptor, typeface_result);
}

Json::Value NormalizeTypefaceProbeIdentity(const Json::Value& root) {
  const Json::Value* typeface_result = ObjectMember(root, "typeface_result");
  const Json::Value* identity = FindTypefaceIdentity(root);
  if (identity != nullptr) {
    return NormalizeTypefaceIdentity(*identity, typeface_result);
  }
  return NormalizeTypefaceDescriptor(root);
}

Json::Value NormalizeGlyphItem(const Json::Value& item) {
  Json::Value value(Json::objectValue);
  if (!item.isObject()) {
    return value;
  }
  if (item.isMember("label")) {
    AddIfPresent(&value, "label", OptionalStringValue(item, "label"));
  } else {
    AddIfPresent(&value, "label", OptionalStringValue(item, "char"));
  }
  AddIfPresent(&value, "code_point", OptionalValue(item, "code_point"));
  AddIfPresent(&value, "glyph_id", OptionalValue(item, "glyph_id"));
  return value;
}

Json::Value NormalizeGlyphArray(const Json::Value& glyphs) {
  Json::Value value(Json::arrayValue);
  if (!glyphs.isArray()) {
    return value;
  }
  for (const auto& glyph : glyphs) {
    value.append(NormalizeGlyphItem(glyph));
  }
  return value;
}

Json::Value NormalizeTableArray(const Json::Value& tables) {
  Json::Value normalized(Json::arrayValue);
  if (!tables.isArray()) {
    return normalized;
  }

  std::vector<Json::Value> items;
  for (const auto& table : tables) {
    if (!table.isObject()) {
      continue;
    }
    Json::Value item(Json::objectValue);
    AddIfPresent(&item, "tag", OptionalStringValue(table, "tag"));
    AddIfPresent(&item, "tag_value", OptionalValue(table, "tag_value"));
    AddIfPresent(&item, "size", OptionalValue(table, "size"));
    AddIfPresent(&item, "sample_size", OptionalValue(table, "sample_size"));
    AddIfPresent(&item, "copied_size", OptionalValue(table, "copied_size"));
    AddIfPresent(&item, "digest", OptionalStringValue(table, "digest"));
    AddIfPresent(&item, "full_copied_size",
                 OptionalValue(table, "full_copied_size"));
    AddIfPresent(&item, "full_digest",
                 OptionalStringValue(table, "full_digest"));
    items.push_back(std::move(item));
  }
  std::sort(items.begin(), items.end(),
            [](const Json::Value& lhs, const Json::Value& rhs) {
              return lhs["tag"].asString() < rhs["tag"].asString();
            });
  for (auto& item : items) {
    normalized.append(std::move(item));
  }
  return normalized;
}

Json::Value NormalizeFontMetrics(const Json::Value& metrics) {
  static const std::vector<std::string> fields = {
      "top",
      "ascent",
      "descent",
      "bottom",
      "leading",
      "avg_char_width",
      "max_char_width",
      "x_min",
      "x_max",
      "x_height",
      "cap_height",
      "underline_thickness",
      "underline_position",
      "strikeout_thickness",
      "strikeout_position",
  };

  Json::Value value(Json::objectValue);
  if (!metrics.isObject()) {
    return value;
  }
  for (const auto& field : fields) {
    AddIfPresent(&value, field, OptionalValue(metrics, field));
  }
  return value;
}

Json::Value BoundsFromGlyphData(const Json::Value& glyph_data) {
  Json::Value bounds(Json::objectValue);
  if (!glyph_data.isObject() || !glyph_data.isMember("left") ||
      !glyph_data.isMember("top") || !glyph_data.isMember("width") ||
      !glyph_data.isMember("height")) {
    return bounds;
  }
  const double left = glyph_data["left"].asDouble();
  const double top = -glyph_data["top"].asDouble();
  const double width = glyph_data["width"].asDouble();
  const double height = glyph_data["height"].asDouble();
  bounds["left"] = left;
  bounds["top"] = top;
  bounds["right"] = left + width;
  bounds["bottom"] = top + height;
  bounds["width"] = width;
  bounds["height"] = height;
  bounds["empty"] = width <= 0.0 || height <= 0.0;
  return bounds;
}

Json::Value NormalizeBounds(const Json::Value& bounds) {
  static const std::vector<std::string> fields = {
      "left", "top", "right", "bottom", "width", "height", "empty"};
  Json::Value value(Json::objectValue);
  if (!bounds.isObject()) {
    return value;
  }
  for (const auto& field : fields) {
    AddIfPresent(&value, field, OptionalValue(bounds, field));
  }
  return value;
}

Json::Value NormalizeGlyphMetricsItem(const Json::Value& item) {
  Json::Value value = NormalizeGlyphItem(item);
  if (!item.isObject()) {
    return value;
  }

  const Json::Value* glyph_data = ObjectMember(item, "glyph_data");
  const Json::Value* source = glyph_data != nullptr ? glyph_data : &item;
  AddIfPresent(&value, "advance_x", OptionalValue(*source, "advance_x"));
  AddIfPresent(&value, "advance_y", OptionalValue(*source, "advance_y"));
  AddIfPresent(&value, "width", OptionalValue(*source, "width"));
  AddIfPresent(&value, "height", OptionalValue(*source, "height"));

  if (item.isMember("bounds")) {
    value["bounds"] = NormalizeBounds(item["bounds"]);
  } else if (glyph_data != nullptr) {
    Json::Value bounds = BoundsFromGlyphData(*glyph_data);
    if (!bounds.empty()) {
      value["bounds"] = NormalizeBounds(bounds);
    }
  }
  return value;
}

Json::Value NormalizeGlyphMetricsArray(const Json::Value& glyphs) {
  Json::Value value(Json::arrayValue);
  if (!glyphs.isArray()) {
    return value;
  }
  for (const auto& glyph : glyphs) {
    value.append(NormalizeGlyphMetricsItem(glyph));
  }
  return value;
}

Json::Value NormalizeMetricsProbe(const Json::Value& root) {
  Json::Value value(Json::objectValue);
  const Json::Value* probe = ObjectMember(root, "metrics_probe");
  if (probe == nullptr || !probe->isObject()) {
    return value;
  }

  const Json::Value* font_result = ObjectMember(*probe, "font_result");
  if (font_result != nullptr) {
    Json::Value font(Json::objectValue);
    font["font_metrics"] =
        NormalizeFontMetrics(OptionalValue(*font_result, "font_metrics"));
    font["glyph_metrics"] = NormalizeGlyphMetricsArray(
        OptionalValue(*font_result, "glyph_metrics"));
    value["font_result"] = std::move(font);
  }

  const Json::Value* scaler = ObjectMember(*probe, "scaler_context_result");
  if (scaler != nullptr) {
    Json::Value scaler_result(Json::objectValue);
    scaler_result["font_metrics"] =
        NormalizeFontMetrics(OptionalValue(*scaler, "font_metrics"));
    scaler_result["glyph_metrics"] =
        NormalizeGlyphMetricsArray(OptionalValue(*scaler, "glyph_metrics"));
    value["scaler_context_result"] = std::move(scaler_result);
  }
  return value;
}

Json::Value NormalizeTypefaceProbe(const Json::Value& root) {
  Json::Value value(Json::objectValue);
  value["descriptor"] = NormalizeTypefaceDescriptor(root);
  value["identity"] = NormalizeTypefaceProbeIdentity(root);

  const Json::Value* probe = ObjectMember(root, "typeface_probe");
  if (probe == nullptr || !probe->isObject()) {
    return value;
  }
  AddIfPresent(&value, "units_per_em", OptionalValue(*probe, "units_per_em"));
  AddIfPresent(&value, "contains_color_table",
               OptionalValue(*probe, "contains_color_table"));
  AddIfPresent(&value, "table_count", OptionalValue(*probe, "table_count"));
  value["glyphs"] = NormalizeGlyphArray(OptionalValue(*probe, "glyphs"));
  value["tables"] = NormalizeTableArray(OptionalValue(*probe, "tables"));
  return value;
}

Json::Value NormalizePathGlyphItem(const Json::Value& item) {
  Json::Value value = NormalizeGlyphItem(item);
  if (item.isObject() && item.isMember("path")) {
    value["path"] = item["path"];
  } else if (item.isObject() && item.isMember("normalized_path")) {
    value["path"] = item["normalized_path"];
  }
  return value;
}

Json::Value NormalizePathGlyphArray(const Json::Value& glyphs) {
  Json::Value value(Json::arrayValue);
  if (!glyphs.isArray()) {
    return value;
  }
  for (const auto& glyph : glyphs) {
    value.append(NormalizePathGlyphItem(glyph));
  }
  return value;
}

bool IsUnavailableResult(const Json::Value& value) {
  return value.isObject() && value.isMember("available") &&
         value["available"].isBool() && !value["available"].asBool();
}

Json::Value NormalizePathProbe(const Json::Value& root) {
  Json::Value value(Json::objectValue);
  const Json::Value* probe = ObjectMember(root, "glyph_path_probe");
  if (probe == nullptr || !probe->isObject()) {
    return value;
  }

  const Json::Value* font_result = ObjectMember(*probe, "font_result");
  if (font_result != nullptr) {
    Json::Value font(Json::objectValue);
    font["glyph_paths"] =
        NormalizePathGlyphArray(OptionalValue(*font_result, "glyph_paths"));
    value["font_result"] = std::move(font);
  }

  const Json::Value* scaler = ObjectMember(*probe, "scaler_context_result");
  if (scaler != nullptr) {
    Json::Value scaler_result(Json::objectValue);
    if (IsUnavailableResult(*scaler)) {
      scaler_result["available"] = false;
    } else {
      scaler_result["glyph_paths"] =
          NormalizePathGlyphArray(OptionalValue(*scaler, "glyph_paths"));
    }
    value["scaler_context_result"] = std::move(scaler_result);
  }
  return value;
}

Json::Value NormalizeGlyphImageItem(const Json::Value& item,
                                    bool include_digest) {
  Json::Value value = NormalizeGlyphItem(item);
  const Json::Value* image = ObjectMember(item, "image");
  if (image == nullptr || !image->isObject()) {
    return value;
  }

  Json::Value normalized_image(Json::objectValue);
  AddIfPresent(&normalized_image, "origin_x",
               OptionalValue(*image, "origin_x"));
  AddIfPresent(&normalized_image, "origin_y",
               OptionalValue(*image, "origin_y"));
  AddIfPresent(&normalized_image, "origin_x_for_raster",
               OptionalValue(*image, "origin_x_for_raster"));
  AddIfPresent(&normalized_image, "origin_y_for_raster",
               OptionalValue(*image, "origin_y_for_raster"));
  AddIfPresent(&normalized_image, "width", OptionalValue(*image, "width"));
  AddIfPresent(&normalized_image, "height", OptionalValue(*image, "height"));
  AddIfPresent(&normalized_image, "format",
               OptionalStringValue(*image, "format"));
  // The numeric enum value is runner-local; compare the stable format string.
  AddIfPresent(&normalized_image, "has_buffer",
               OptionalValue(*image, "has_buffer"));
  AddIfPresent(&normalized_image, "byte_size",
               OptionalValue(*image, "byte_size"));
  if (include_digest) {
    AddIfPresent(&normalized_image, "digest",
                 OptionalStringValue(*image, "digest"));
  }
  value["image"] = std::move(normalized_image);
  return value;
}

Json::Value NormalizeGlyphImageArray(const Json::Value& glyphs,
                                     bool include_digest) {
  Json::Value value(Json::arrayValue);
  if (!glyphs.isArray()) {
    return value;
  }
  for (const auto& glyph : glyphs) {
    value.append(NormalizeGlyphImageItem(glyph, include_digest));
  }
  return value;
}

Json::Value NormalizeGlyphImageProbe(const Json::Value& root,
                                     bool include_digest) {
  Json::Value value(Json::objectValue);
  const Json::Value* probe = ObjectMember(root, "glyph_image_probe");
  if (probe == nullptr || !probe->isObject()) {
    return value;
  }

  const Json::Value* font_result = ObjectMember(*probe, "font_result");
  if (font_result != nullptr) {
    Json::Value font(Json::objectValue);
    font["glyph_images"] = NormalizeGlyphImageArray(
        OptionalValue(*font_result, "glyph_images"), include_digest);
    value["font_result"] = std::move(font);
  }
  return value;
}

Json::Value NormalizeMatchedTypeface(const Json::Value& typeface) {
  Json::Value value(Json::objectValue);
  if (!typeface.isObject()) {
    return value;
  }
  AddIfPresent(&value, "available", OptionalValue(typeface, "available"));
  value["descriptor"] =
      NormalizeDescriptor(OptionalValue(typeface, "descriptor"), &typeface);
  value["identity"] = NormalizeTypefaceSelectionIdentity(typeface);
  if (typeface.isMember("font_style")) {
    value["font_style"] = NormalizeStyle(typeface["font_style"]);
  }
  AddIfPresent(&value, "units_per_em", OptionalValue(typeface, "units_per_em"));
  AddIfPresent(&value, "contains_color_table",
               OptionalValue(typeface, "contains_color_table"));
  AddIfPresent(&value, "table_count", OptionalValue(typeface, "table_count"));
  AddIfPresent(&value, "copied_table_tag_count",
               OptionalValue(typeface, "copied_table_tag_count"));
  if (typeface.isMember("tables")) {
    value["tables"] = NormalizeTableArray(typeface["tables"]);
  }

  const Json::Value* summary = ObjectMember(typeface, "probe_summary");
  if (summary != nullptr) {
    Json::Value probe_summary(Json::objectValue);
    const Json::Value* font_result = ObjectMember(*summary, "font_result");
    if (font_result != nullptr) {
      probe_summary["font_metrics"] =
          NormalizeFontMetrics(OptionalValue(*font_result, "font_metrics"));
    }
    const Json::Value* scaler = ObjectMember(*summary, "scaler_context_result");
    if (scaler != nullptr) {
      probe_summary["scaler_font_metrics"] =
          NormalizeFontMetrics(OptionalValue(*scaler, "font_metrics"));
    }
    probe_summary["glyphs"] =
        NormalizeGlyphArray(OptionalValue(*summary, "glyphs"));
    value["probe_summary"] = std::move(probe_summary);
  }
  return value;
}

Json::Value NormalizeTypefaceSelection(const Json::Value& typeface) {
  Json::Value value(Json::objectValue);
  if (!typeface.isObject()) {
    return value;
  }
  AddIfPresent(&value, "available", OptionalValue(typeface, "available"));
  value["descriptor"] =
      NormalizeDescriptor(OptionalValue(typeface, "descriptor"), &typeface);
  value["identity"] = NormalizeTypefaceSelectionIdentity(typeface);
  if (typeface.isMember("font_style")) {
    value["font_style"] = NormalizeStyle(typeface["font_style"]);
  }
  AddIfPresent(&value, "units_per_em", OptionalValue(typeface, "units_per_em"));
  AddIfPresent(&value, "contains_color_table",
               OptionalValue(typeface, "contains_color_table"));
  return value;
}

Json::Value NormalizeMatchedTypefaces(const Json::Value& root) {
  Json::Value value(Json::arrayValue);
  const Json::Value* probe = ObjectMember(root, "font_manager_probe");
  if (probe == nullptr || !probe->isObject() ||
      !probe->isMember("matched_typefaces") ||
      !(*probe)["matched_typefaces"].isArray()) {
    return value;
  }

  for (const auto& typeface : (*probe)["matched_typefaces"]) {
    value.append(NormalizeMatchedTypeface(typeface));
  }
  return value;
}

Json::Value NormalizeStyleSet(const Json::Value& style_set) {
  Json::Value value(Json::objectValue);
  if (!style_set.isObject()) {
    return value;
  }
  AddIfPresent(&value, "available", OptionalValue(style_set, "available"));
  AddIfPresent(&value, "style_count", OptionalValue(style_set, "style_count"));
  if (style_set.isMember("match_style")) {
    value["match_style"]["typeface"] =
        NormalizeTypefaceSelection(style_set["match_style"]["typeface"]);
  }
  return value;
}

Json::Value NormalizeFontManagerProbe(const Json::Value& root) {
  Json::Value value(Json::objectValue);
  const Json::Value* probe = ObjectMember(root, "font_manager_probe");
  if (probe == nullptr || !probe->isObject()) {
    return value;
  }

  const Json::Value* operation = ObjectMember(*probe, "operation");
  if (operation != nullptr) {
    AddIfPresent(&value, "entry", OptionalStringValue(*operation, "entry"));
    if (operation->isMember("create_style_set")) {
      value["create_style_set"] =
          NormalizeStyleSet((*operation)["create_style_set"]);
    }
    if (operation->isMember("style_set")) {
      value["style_set"] = NormalizeStyleSet((*operation)["style_set"]);
    }
    if (operation->isMember("match_family")) {
      value["match_family"] = NormalizeStyleSet((*operation)["match_family"]);
    }
  }
  value["matched_typefaces"] = NormalizeMatchedTypefaces(root);
  return value;
}

double ReadCompareEpsilon(const Json::Value& compare, const std::string& field,
                          double fallback) {
  if (!compare.isObject() || !compare.isMember(field) ||
      !compare[field].isObject() || !compare[field].isMember("epsilon") ||
      !compare[field]["epsilon"].isNumeric()) {
    return fallback;
  }
  return compare[field]["epsilon"].asDouble();
}

std::string ReadCompareMode(const Json::Value& compare,
                            const std::string& field,
                            const std::string& fallback) {
  if (!compare.isObject() || !compare.isMember(field)) {
    return fallback;
  }
  if (compare[field].isString()) {
    return compare[field].asString();
  }
  if (compare[field].isObject() && compare[field].isMember("mode") &&
      compare[field]["mode"].isString()) {
    return compare[field]["mode"].asString();
  }
  return fallback;
}

CompareConfig ParseCompareConfig(const Json::Value& case_root) {
  CompareConfig config;
  ReadStringField(case_root, "category", &config.category);

  const Json::Value& compare = case_root["compare"];
  config.typeface_identity =
      ReadCompareMode(compare, "typeface_identity", config.typeface_identity);
  config.font_style = ReadCompareMode(compare, "font_style", config.font_style);
  config.font_metrics_mode =
      ReadCompareMode(compare, "font_metrics", config.font_metrics_mode);
  config.glyph_metrics_mode =
      ReadCompareMode(compare, "glyph_metrics", config.glyph_metrics_mode);
  config.glyph_path_mode =
      ReadCompareMode(compare, "glyph_path", config.glyph_path_mode);
  config.glyph_image_mode =
      ReadCompareMode(compare, "glyph_image", config.glyph_image_mode);
  config.font_metrics_epsilon =
      ReadCompareEpsilon(compare, "font_metrics", kDefaultMetricEpsilon);
  config.glyph_metrics_epsilon =
      ReadCompareEpsilon(compare, "glyph_metrics", kDefaultMetricEpsilon);
  config.glyph_path_epsilon =
      ReadCompareEpsilon(compare, "glyph_path", kDefaultPathEpsilon);
  return config;
}

Json::Value CompareConfigToJson(const CompareConfig& config) {
  Json::Value value(Json::objectValue);
  value["category"] = config.category;
  value["typeface_identity"] = config.typeface_identity;
  value["font_style"] = config.font_style;
  value["font_metrics_mode"] = config.font_metrics_mode;
  value["glyph_metrics_mode"] = config.glyph_metrics_mode;
  value["glyph_path_mode"] = config.glyph_path_mode;
  value["glyph_image_mode"] = config.glyph_image_mode;
  value["font_metrics_epsilon"] = config.font_metrics_epsilon;
  value["glyph_metrics_epsilon"] = config.glyph_metrics_epsilon;
  value["glyph_path_epsilon"] = config.glyph_path_epsilon;
  return value;
}

Json::Value BuildReport(const std::string& case_id, const std::string& backend,
                        const std::string& stage,
                        const std::string& reason_code,
                        const CompareRequest& request) {
  Json::Value report = BuildCompareReport(case_id, backend, stage, reason_code);
  report["artifacts"]["case"] = PathArg(request.case_path);
  report["artifacts"]["expected"] = PathArg(request.expected_path);
  report["artifacts"]["actual"] = PathArg(request.actual_path);
  return report;
}

CompareResult BuildInputFailure(const std::string& case_id,
                                const std::string& backend,
                                const CompareRequest& request,
                                const std::string& stage,
                                const std::string& reason_code,
                                const std::string& message) {
  CompareResult result;
  result.status = CompareStatus::kInputFailed;
  result.case_id = case_id;
  result.backend = backend;
  result.report = BuildReport(case_id, backend, stage, reason_code, request);
  result.report["message"] = message;
  return result;
}

void AddArtifactValidationReport(Json::Value* report, const std::string& field,
                                 const ArtifactValidationResult& validation) {
  (*report)["artifact_validation"][field]["valid"] = validation.valid;
  (*report)["artifact_validation"][field]["artifact_type"] =
      validation.artifact_type;
  (*report)["artifact_validation"][field]["case_id"] = validation.case_id;
  (*report)["artifact_validation"][field]["errors"] =
      validation.errors.ToJson();
}

std::string FirstDiffReason(const std::vector<Diff>& diffs,
                            const std::string& fallback) {
  if (diffs.empty() || diffs.front().reason_code.empty()) {
    return fallback;
  }
  return diffs.front().reason_code;
}

bool HasNewDiffs(size_t diff_count, const std::vector<Diff>& diffs) {
  return diffs.size() > diff_count;
}

bool CompareSelectionSubset(const Json::Value& expected,
                            const Json::Value& actual, const std::string& path,
                            const std::string& rule, std::vector<Diff>* diffs) {
  const size_t diff_count = diffs->size();
  CompareJsonSubset(expected, actual, path, "selection_mismatch", rule, 0.0,
                    false, diffs);
  return HasNewDiffs(diff_count, *diffs);
}

void CompareTypefaceIdentity(const Json::Value& expected,
                             const Json::Value& actual, const std::string& path,
                             const std::string& reason_code,
                             const CompareConfig& config,
                             std::vector<Diff>* diffs) {
  if (config.typeface_identity == "none") {
    return;
  }
  CompareJsonSubset(expected, actual, path, reason_code, "normalized_identity",
                    0.0, false, diffs);
}

void CompareFontStyle(const Json::Value& expected, const Json::Value& actual,
                      const std::string& path, const CompareConfig& config,
                      std::vector<Diff>* diffs) {
  if (config.font_style == "none") {
    return;
  }
  CompareJsonSubset(expected, actual, path, "font_style_mismatch",
                    "font_style_exact", 0.0, false, diffs);
}

void CompareGlyphIds(const Json::Value& expected, const Json::Value& actual,
                     const std::string& path, std::vector<Diff>* diffs) {
  if (!expected.isArray() || !actual.isArray()) {
    CompareJsonSubset(expected, actual, path, "glyph_id_mismatch",
                      "glyph_id_exact", 0.0, false, diffs);
    return;
  }
  if (expected.size() != actual.size()) {
    AddDiff(diffs, path, "glyph_id_mismatch", "glyph_id_exact",
            "glyph array sizes differ", expected, actual);
    return;
  }
  for (Json::ArrayIndex i = 0; i < expected.size(); ++i) {
    const std::string item_path = IndexJsonPath(path, i);
    CompareJsonSubset(OptionalValue(expected[i], "label"),
                      OptionalValue(actual[i], "label"),
                      ChildJsonPath(item_path, "label"), "glyph_id_mismatch",
                      "glyph_id_exact", 0.0, false, diffs);
    if (expected[i].isMember("code_point")) {
      CompareJsonSubset(
          expected[i]["code_point"], OptionalValue(actual[i], "code_point"),
          ChildJsonPath(item_path, "code_point"), "glyph_id_mismatch",
          "glyph_id_exact", 0.0, false, diffs);
    }
    CompareJsonSubset(OptionalValue(expected[i], "glyph_id"),
                      OptionalValue(actual[i], "glyph_id"),
                      ChildJsonPath(item_path, "glyph_id"), "glyph_id_mismatch",
                      "glyph_id_exact", 0.0, false, diffs);
  }
}

void CompareMetricSet(const Json::Value& expected, const Json::Value& actual,
                      const std::string& path, double epsilon,
                      const std::string& rule, std::vector<Diff>* diffs) {
  CompareJsonSubset(expected, actual, path, "metrics_mismatch", rule, epsilon,
                    true, diffs);
}

void CompareGlyphMetricSet(const Json::Value& expected,
                           const Json::Value& actual, const std::string& path,
                           double epsilon, std::vector<Diff>* diffs) {
  CompareJsonSubset(expected, actual, path, "metrics_mismatch",
                    "glyph_metrics_epsilon", epsilon, true, diffs);
}

void ComparePathSet(const Json::Value& expected, const Json::Value& actual,
                    const std::string& path, double epsilon,
                    std::vector<Diff>* diffs) {
  CompareJsonSubset(expected, actual, path, "path_mismatch", "path_normalized",
                    epsilon, true, diffs);
}

void CompareTypefaceProbe(const Json::Value& expected_root,
                          const Json::Value& actual_root,
                          const CompareConfig& config,
                          std::vector<Diff>* diffs) {
  Json::Value expected = NormalizeTypefaceProbe(expected_root);
  Json::Value actual = NormalizeTypefaceProbe(actual_root);
  CompareTypefaceIdentity(expected["identity"], actual["identity"],
                          "typeface_result.identity", "descriptor_mismatch",
                          config, diffs);

  if (expected["descriptor"].isMember("style") &&
      actual["descriptor"].isMember("style")) {
    CompareFontStyle(expected["descriptor"]["style"],
                     actual["descriptor"]["style"],
                     "typeface_result.typeface.style", config, diffs);
  }
  CompareJsonSubset(OptionalValue(expected, "units_per_em"),
                    OptionalValue(actual, "units_per_em"),
                    "typeface_probe.units_per_em", "descriptor_mismatch",
                    "exact", 0.0, false, diffs);
  CompareJsonSubset(OptionalValue(expected, "table_count"),
                    OptionalValue(actual, "table_count"),
                    "typeface_probe.table_count", "table_mismatch", "exact",
                    0.0, false, diffs);
  CompareGlyphIds(expected["glyphs"], actual["glyphs"], "typeface_probe.glyphs",
                  diffs);
  CompareJsonSubset(expected["tables"], actual["tables"],
                    "typeface_probe.tables", "table_mismatch", "table_exact",
                    0.0, false, diffs);
}

void CompareMetricsProbe(const Json::Value& expected_root,
                         const Json::Value& actual_root,
                         const CompareConfig& config,
                         std::vector<Diff>* diffs) {
  Json::Value expected = NormalizeMetricsProbe(expected_root);
  Json::Value actual = NormalizeMetricsProbe(actual_root);

  if (!IsIgnoreMode(config.font_metrics_mode)) {
    CompareMetricSet(expected["font_result"]["font_metrics"],
                     actual["font_result"]["font_metrics"],
                     "metrics_probe.font_result.font_metrics",
                     config.font_metrics_epsilon, "font_metrics_epsilon",
                     diffs);
    CompareMetricSet(expected["scaler_context_result"]["font_metrics"],
                     actual["scaler_context_result"]["font_metrics"],
                     "metrics_probe.scaler_context_result.font_metrics",
                     config.font_metrics_epsilon, "font_metrics_epsilon",
                     diffs);
  }

  if (!IsIgnoreMode(config.glyph_metrics_mode)) {
    CompareGlyphMetricSet(expected["font_result"]["glyph_metrics"],
                          actual["font_result"]["glyph_metrics"],
                          "metrics_probe.font_result.glyph_metrics",
                          config.glyph_metrics_epsilon, diffs);
    CompareGlyphMetricSet(expected["scaler_context_result"]["glyph_metrics"],
                          actual["scaler_context_result"]["glyph_metrics"],
                          "metrics_probe.scaler_context_result.glyph_metrics",
                          config.glyph_metrics_epsilon, diffs);
  }
}

void CompareGlyphPathProbe(const Json::Value& expected_root,
                           const Json::Value& actual_root,
                           const CompareConfig& config,
                           std::vector<Diff>* diffs) {
  if (IsIgnoreMode(config.glyph_path_mode)) {
    return;
  }
  Json::Value expected = NormalizePathProbe(expected_root);
  Json::Value actual = NormalizePathProbe(actual_root);
  ComparePathSet(expected["font_result"]["glyph_paths"],
                 actual["font_result"]["glyph_paths"],
                 "glyph_path_probe.font_result.glyph_paths",
                 config.glyph_path_epsilon, diffs);
  if (!IsUnavailableResult(expected["scaler_context_result"])) {
    ComparePathSet(expected["scaler_context_result"]["glyph_paths"],
                   actual["scaler_context_result"]["glyph_paths"],
                   "glyph_path_probe.scaler_context_result.glyph_paths",
                   config.glyph_path_epsilon, diffs);
  }
}

void CompareGlyphImageProbe(const Json::Value& expected_root,
                            const Json::Value& actual_root,
                            const CompareConfig& config,
                            std::vector<Diff>* diffs) {
  if (IsIgnoreMode(config.glyph_image_mode)) {
    return;
  }
  const bool include_digest = config.glyph_image_mode != "metadata";
  Json::Value expected =
      NormalizeGlyphImageProbe(expected_root, include_digest);
  Json::Value actual = NormalizeGlyphImageProbe(actual_root, include_digest);
  CompareJsonSubset(expected["font_result"]["glyph_images"],
                    actual["font_result"]["glyph_images"],
                    "glyph_image_probe.font_result.glyph_images",
                    "image_mismatch", config.glyph_image_mode, 0.0, false,
                    diffs);
}

void CompareFontManagerProbe(const Json::Value& expected_root,
                             const Json::Value& actual_root,
                             const CompareConfig& config,
                             std::vector<Diff>* diffs) {
  Json::Value expected = NormalizeFontManagerProbe(expected_root);
  Json::Value actual = NormalizeFontManagerProbe(actual_root);
  if (CompareSelectionSubset(
          OptionalValue(expected, "entry"), OptionalValue(actual, "entry"),
          "font_manager_probe.operation.entry", "exact", diffs)) {
    return;
  }
  if (CompareSelectionSubset(OptionalValue(expected, "create_style_set"),
                             OptionalValue(actual, "create_style_set"),
                             "font_manager_probe.operation.create_style_set",
                             "style_set_exact", diffs)) {
    return;
  }
  if (CompareSelectionSubset(OptionalValue(expected, "style_set"),
                             OptionalValue(actual, "style_set"),
                             "font_manager_probe.operation.style_set",
                             "style_set_exact", diffs)) {
    return;
  }
  if (CompareSelectionSubset(OptionalValue(expected, "match_family"),
                             OptionalValue(actual, "match_family"),
                             "font_manager_probe.operation.match_family",
                             "style_set_exact", diffs)) {
    return;
  }

  const Json::Value& expected_typefaces = expected["matched_typefaces"];
  const Json::Value& actual_typefaces = actual["matched_typefaces"];
  if (!expected_typefaces.isArray() || !actual_typefaces.isArray() ||
      expected_typefaces.size() != actual_typefaces.size()) {
    AddDiff(diffs, "font_manager_probe.matched_typefaces", "selection_mismatch",
            "selection_exact", "matched typeface array sizes differ",
            expected_typefaces, actual_typefaces);
    return;
  }

  for (Json::ArrayIndex i = 0; i < expected_typefaces.size(); ++i) {
    const std::string path =
        IndexJsonPath("font_manager_probe.matched_typefaces", i);
    if (CompareSelectionSubset(
            OptionalValue(expected_typefaces[i], "available"),
            OptionalValue(actual_typefaces[i], "available"),
            ChildJsonPath(path, "available"), "selection_exact", diffs)) {
      return;
    }

    const size_t descriptor_diff_count = diffs->size();
    CompareTypefaceIdentity(
        expected_typefaces[i]["identity"], actual_typefaces[i]["identity"],
        ChildJsonPath(path, "identity"), "selection_mismatch", config, diffs);
    if (HasNewDiffs(descriptor_diff_count, *diffs)) {
      return;
    }

    if (expected_typefaces[i].isMember("font_style") &&
        actual_typefaces[i].isMember("font_style")) {
      CompareFontStyle(expected_typefaces[i]["font_style"],
                       actual_typefaces[i]["font_style"],
                       ChildJsonPath(path, "font_style"), config, diffs);
    }
    CompareGlyphIds(expected_typefaces[i]["probe_summary"]["glyphs"],
                    actual_typefaces[i]["probe_summary"]["glyphs"],
                    ChildJsonPath(path, "probe_summary.glyphs"), diffs);
    if (expected_typefaces[i].isMember("tables")) {
      CompareJsonSubset(expected_typefaces[i]["tables"],
                        actual_typefaces[i]["tables"],
                        ChildJsonPath(path, "tables"), "table_mismatch",
                        "table_exact", 0.0, false, diffs);
    }
    if (!IsIgnoreMode(config.font_metrics_mode)) {
      CompareMetricSet(expected_typefaces[i]["probe_summary"]["font_metrics"],
                       actual_typefaces[i]["probe_summary"]["font_metrics"],
                       ChildJsonPath(path, "probe_summary.font_metrics"),
                       config.font_metrics_epsilon, "font_metrics_epsilon",
                       diffs);
      CompareMetricSet(
          expected_typefaces[i]["probe_summary"]["scaler_font_metrics"],
          actual_typefaces[i]["probe_summary"]["scaler_font_metrics"],
          ChildJsonPath(path, "probe_summary.scaler_font_metrics"),
          config.font_metrics_epsilon, "font_metrics_epsilon", diffs);
    }
  }
}

void RunCategoryCompare(const Json::Value& expected, const Json::Value& actual,
                        const CompareConfig& config, std::vector<Diff>* diffs) {
  if (config.category == "typeface_probe" ||
      config.category == "typeface_descriptor" ||
      config.category == "font_tables" || config.category == "variation" ||
      config.category == "glyph_mapping") {
    CompareTypefaceProbe(expected, actual, config, diffs);
    return;
  }

  if (config.category == "font_metrics" || config.category == "glyph_metrics" ||
      config.category == "scaler_context") {
    CompareMetricsProbe(expected, actual, config, diffs);
    return;
  }

  if (config.category == "glyph_path") {
    CompareGlyphPathProbe(expected, actual, config, diffs);
    return;
  }

  if (config.category == "glyph_image") {
    CompareGlyphImageProbe(expected, actual, config, diffs);
    return;
  }

  if (config.category == "font_manager" ||
      config.category == "family_style_set") {
    CompareFontManagerProbe(expected, actual, config, diffs);
    return;
  }

  AddDiff(diffs, "category", "schema_mismatch", "category_supported",
          "compare category is not implemented", Json::Value(config.category),
          Json::Value(Json::nullValue));
}

bool LoadJsonInput(const std::filesystem::path& path, Json::Value* root,
                   std::string* error) {
  return LoadJsonFile(path, root, error);
}

}  // namespace

CompareResult RunCompare(const CompareRequest& request) {
  const std::string fallback_case_id =
      StemOrFallback(request.case_path, "case");
  std::string backend = request.backend.empty() ? "unknown" : request.backend;

  Json::Value case_root;
  std::string error;
  if (!LoadJsonInput(request.case_path, &case_root, &error)) {
    return BuildInputFailure(fallback_case_id, backend, request, "case_load",
                             "schema_mismatch", error);
  }

  RepoUriResolver resolver(request.repo_root);
  CaseValidationResult case_validation =
      ValidateCaseDocument(case_root, resolver);
  if (case_validation.case_id.empty()) {
    case_validation.case_id = fallback_case_id;
  }
  if (case_validation.backend.empty()) {
    case_validation.backend = backend;
  }
  if (backend == "unknown") {
    backend =
        case_validation.backend.empty() ? "unknown" : case_validation.backend;
  }

  if (!case_validation.valid) {
    CompareResult result = BuildInputFailure(
        case_validation.case_id, backend, request, "case_schema",
        "schema_mismatch", "case document is invalid");
    result.report["case_validation_errors"] = case_validation.errors.ToJson();
    return result;
  }

  if (!request.backend.empty() && case_validation.backend != request.backend) {
    CompareResult result = BuildInputFailure(
        case_validation.case_id, backend, request, "case_schema",
        "schema_mismatch", "case backend does not match --backend");
    result.report["case_validation_errors"] = Json::Value(Json::arrayValue);
    Json::Value item(Json::objectValue);
    item["path"] = "$.backend";
    item["message"] = "case backend does not match --backend";
    result.report["case_validation_errors"].append(std::move(item));
    return result;
  }

  Json::Value expected_root;
  if (!LoadJsonInput(request.expected_path, &expected_root, &error)) {
    return BuildInputFailure(case_validation.case_id, backend, request,
                             "expected_load", "oracle_unavailable", error);
  }

  Json::Value actual_root;
  if (!LoadJsonInput(request.actual_path, &actual_root, &error)) {
    return BuildInputFailure(case_validation.case_id, backend, request,
                             "actual_load", "schema_mismatch", error);
  }

  ArtifactValidationResult expected_validation =
      ValidateProbeResultDocument(expected_root);
  ArtifactValidationResult actual_validation =
      ValidateProbeResultDocument(actual_root);
  if (!expected_validation.valid || !actual_validation.valid) {
    CompareResult result = BuildInputFailure(
        case_validation.case_id, backend, request, "artifact_schema",
        "schema_mismatch", "probe result artifact schema is invalid");
    AddArtifactValidationReport(&result.report, "expected",
                                expected_validation);
    AddArtifactValidationReport(&result.report, "actual", actual_validation);
    return result;
  }

  if (expected_validation.case_id != case_validation.case_id ||
      actual_validation.case_id != case_validation.case_id) {
    CompareResult result = BuildInputFailure(
        case_validation.case_id, backend, request, "artifact_schema",
        "schema_mismatch", "probe result case_id does not match case");
    result.report["expected_case_id"] = expected_validation.case_id;
    result.report["actual_case_id"] = actual_validation.case_id;
    return result;
  }

  if (!expected_root.isMember("ok") || !expected_root["ok"].asBool() ||
      !actual_root.isMember("ok") || !actual_root["ok"].asBool()) {
    CompareResult result = BuildInputFailure(
        case_validation.case_id, backend, request, "artifact_schema",
        "schema_mismatch", "probe result is not ok");
    result.report["expected_ok"] =
        expected_root.isMember("ok") && expected_root["ok"].asBool();
    result.report["actual_ok"] =
        actual_root.isMember("ok") && actual_root["ok"].asBool();
    return result;
  }

  CompareConfig config = ParseCompareConfig(case_root);
  std::vector<Diff> diffs;
  RunCategoryCompare(expected_root, actual_root, config, &diffs);

  CompareResult result;
  result.case_id = case_validation.case_id;
  result.backend = backend;
  if (diffs.empty()) {
    result.status = CompareStatus::kPass;
    result.report = BuildReport(case_validation.case_id, backend, "compare",
                                "pass", request);
    result.report["passed"] = true;
  } else {
    result.status = CompareStatus::kMismatch;
    result.report =
        BuildReport(case_validation.case_id, backend, "compare",
                    FirstDiffReason(diffs, "compare_mismatch"), request);
    result.report["passed"] = false;
    result.report["diff_path"] = diffs.front().path;
    result.report["diff_count"] = static_cast<Json::UInt64>(diffs.size());
    result.report["diffs"] = ToDiffsJson(diffs);
  }
  result.report["compare_config"] = CompareConfigToJson(config);
  return result;
}

}  // namespace font_harness
}  // namespace skity
