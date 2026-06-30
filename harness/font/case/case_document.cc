// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "harness/font/case/case_document.hpp"

#include <cctype>
#include <set>
#include <unordered_map>
#include <utility>

#include "harness/font/case/platform_target.hpp"

namespace skity {
namespace font_harness {

namespace {

const std::vector<std::string>& AllowedCategories() {
  static const std::vector<std::string> values = {
      "typeface_probe", "system_match",        "family_style_set",
      "fallback_match", "typeface_descriptor", "font_metrics",
      "glyph_mapping",  "glyph_metrics",       "glyph_path",
      "glyph_image",    "variation",           "font_tables",
      "scaler_context", "font_manager",
  };
  return values;
}

const std::vector<std::string>& AllowedStatuses() {
  static const std::vector<std::string> values = {
      "active", "skity_gap", "expected_diff", "platform_unstable", "blocked"};
  return values;
}

const std::vector<std::string>& AllowedTypefaceEntries() {
  static const std::vector<std::string> values = {
      "MakeFromFile", "MakeFromData", "MakeVariation",
      "MakeFromPlatformDescriptor"};
  return values;
}

const std::vector<std::string>& AllowedHinting() {
  static const std::vector<std::string> values = {"none", "slight", "normal",
                                                  "full"};
  return values;
}

bool ValidateGlyphChar(const std::string& value) {
  if (value.size() < 3 || value.rfind("U+", 0) != 0) {
    return false;
  }
  for (size_t i = 2; i < value.size(); ++i) {
    if (!std::isxdigit(static_cast<unsigned char>(value[i]))) {
      return false;
    }
  }
  return true;
}

bool ValidateColorString(const std::string& value) {
  if (value.size() != 7 && value.size() != 9) {
    return false;
  }
  if (value[0] != '#') {
    return false;
  }
  for (size_t i = 1; i < value.size(); ++i) {
    if (!std::isxdigit(static_cast<unsigned char>(value[i]))) {
      return false;
    }
  }
  return true;
}

bool RequiresTypefaceRequest(const std::string& category) {
  static const std::set<std::string> values = {
      "typeface_probe", "typeface_descriptor", "font_metrics", "glyph_mapping",
      "glyph_metrics",  "glyph_path",          "glyph_image",  "variation",
      "font_tables",    "scaler_context",
  };
  return values.find(category) != values.end();
}

void ValidatePlatforms(const Json::Value& root, const std::string& backend,
                       ValidationContext* context) {
  const Json::Value* platforms = nullptr;
  if (!RequireArrayField(root, "platforms", "$", context, &platforms)) {
    return;
  }
  if (platforms->empty()) {
    context->AddError("$.platforms", "platforms must not be empty");
  }
  for (Json::ArrayIndex i = 0; i < platforms->size(); ++i) {
    const Json::Value& platform = (*platforms)[i];
    const std::string path = IndexPath("$.platforms", i);
    if (!platform.isString()) {
      context->AddError(path, "expected string");
      continue;
    }
    ValidateNonEmptyString(platform.asString(), path, context);
  }
  if (!backend.empty() && !PlatformArrayMatchesBackend(backend, *platforms)) {
    context->AddError("$.platforms",
                      "platforms do not match backend: " + backend);
  }
}

void ValidateFontRequest(const Json::Value& root, ValidationContext* context) {
  if (!root.isMember("font_request")) {
    return;
  }
  const Json::Value& request = root["font_request"];
  const std::string path = "$.font_request";
  if (!RequireObject(request, path, context)) {
    return;
  }

  double number = 0.0;
  if (OptionalNumberField(request, "size", path, context, &number)) {
    ValidatePositiveNumber(number, ChildPath(path, "size"), context);
  }
  if (OptionalNumberField(request, "scale_x", path, context, &number)) {
    ValidatePositiveNumber(number, ChildPath(path, "scale_x"), context);
  }
  OptionalNumberField(request, "skew_x", path, context, nullptr);
  bool bool_value = false;
  OptionalBoolField(request, "linear_metrics", path, context, &bool_value);
  OptionalBoolField(request, "subpixel", path, context, &bool_value);
  OptionalBoolField(request, "embolden", path, context, &bool_value);

  std::string hinting;
  if (OptionalStringField(request, "hinting", path, context, &hinting)) {
    ValidateStringEnum(hinting, ChildPath(path, "hinting"), AllowedHinting(),
                       context);
  }
}

void ValidatePaintRequest(const Json::Value& root, ValidationContext* context) {
  if (!root.isMember("paint_request")) {
    return;
  }
  const Json::Value& request = root["paint_request"];
  const std::string path = "$.paint_request";
  if (!RequireObject(request, path, context)) {
    return;
  }

  std::string color;
  if (OptionalStringField(request, "color", path, context, &color) &&
      !ValidateColorString(color)) {
    context->AddError(ChildPath(path, "color"),
                      "expected #RRGGBB or #RRGGBBAA");
  }
  std::string style;
  if (OptionalStringField(request, "style", path, context, &style)) {
    ValidateStringEnum(style, ChildPath(path, "style"), {"fill", "stroke"},
                       context);
  }
  double stroke_width = 0.0;
  if (OptionalNumberField(request, "stroke_width", path, context,
                          &stroke_width)) {
    ValidatePositiveNumber(stroke_width, ChildPath(path, "stroke_width"),
                           context);
  }
}

void ValidateGlyphs(const Json::Value& root, ValidationContext* context) {
  if (!root.isMember("glyphs")) {
    return;
  }
  const Json::Value& glyphs = root["glyphs"];
  const std::string path = "$.glyphs";
  if (!RequireObject(glyphs, path, context)) {
    return;
  }

  bool has_chars = false;
  bool has_glyph_ids = false;
  if (glyphs.isMember("chars")) {
    const Json::Value& chars = glyphs["chars"];
    const std::string chars_path = ChildPath(path, "chars");
    if (!chars.isArray()) {
      context->AddError(chars_path, "expected array");
    } else {
      has_chars = !chars.empty();
      for (Json::ArrayIndex i = 0; i < chars.size(); ++i) {
        const std::string item_path = IndexPath(chars_path, i);
        if (!chars[i].isString()) {
          context->AddError(item_path, "expected string");
          continue;
        }
        if (!ValidateGlyphChar(chars[i].asString())) {
          context->AddError(item_path, "expected U+XXXX code point string");
        }
      }
    }
  }
  if (glyphs.isMember("glyph_ids")) {
    const Json::Value& ids = glyphs["glyph_ids"];
    const std::string ids_path = ChildPath(path, "glyph_ids");
    if (!ids.isArray()) {
      context->AddError(ids_path, "expected array");
    } else {
      has_glyph_ids = !ids.empty();
      for (Json::ArrayIndex i = 0; i < ids.size(); ++i) {
        const std::string item_path = IndexPath(ids_path, i);
        if (!ids[i].isInt()) {
          context->AddError(item_path, "expected integer");
          continue;
        }
        ValidateNonNegativeInt(ids[i].asInt(), item_path, context);
      }
    }
  }
  if (!has_chars && !has_glyph_ids) {
    context->AddError(path,
                      "chars or glyph_ids must contain at least one item");
  }
}

void ValidateCompareModeObject(const Json::Value& value,
                               const std::string& path,
                               const std::vector<std::string>& modes,
                               ValidationContext* context) {
  if (!RequireObject(value, path, context)) {
    return;
  }
  std::string mode;
  if (OptionalStringField(value, "mode", path, context, &mode)) {
    ValidateStringEnum(mode, ChildPath(path, "mode"), modes, context);
  }
  double epsilon = 0.0;
  if (OptionalNumberField(value, "epsilon", path, context, &epsilon) &&
      epsilon < 0.0) {
    context->AddError(ChildPath(path, "epsilon"),
                      "epsilon must be non-negative");
  }
}

void ValidateCompare(const Json::Value& root, ValidationContext* context) {
  const Json::Value* compare = nullptr;
  if (!RequireObjectField(root, "compare", "$", context, &compare)) {
    return;
  }

  std::string value;
  if (OptionalStringField(*compare, "typeface_identity", "$.compare", context,
                          &value)) {
    ValidateStringEnum(value, "$.compare.typeface_identity",
                       {"normalized_descriptor", "exact", "none"}, context);
  }
  if (OptionalStringField(*compare, "font_style", "$.compare", context,
                          &value)) {
    ValidateStringEnum(value, "$.compare.font_style", {"exact", "none"},
                       context);
  }

  if (compare->isMember("font_metrics")) {
    ValidateCompareModeObject((*compare)["font_metrics"],
                              "$.compare.font_metrics",
                              {"exact", "epsilon", "ignore"}, context);
  }
  if (compare->isMember("glyph_metrics")) {
    ValidateCompareModeObject((*compare)["glyph_metrics"],
                              "$.compare.glyph_metrics",
                              {"exact", "epsilon", "ignore"}, context);
  }
  if (compare->isMember("glyph_path")) {
    ValidateCompareModeObject((*compare)["glyph_path"], "$.compare.glyph_path",
                              {"backend_default", "path_exact",
                               "path_normalized", "path_geometry", "ignore"},
                              context);
  }
  if (compare->isMember("glyph_image")) {
    ValidateCompareModeObject((*compare)["glyph_image"],
                              "$.compare.glyph_image",
                              {"exact", "metadata", "ignore"}, context);
  }
}

std::unordered_map<std::string, ResolvedFontFile> ValidateFontFiles(
    const Json::Value& root, const RepoUriResolver& resolver,
    CaseValidationResult* result) {
  std::unordered_map<std::string, ResolvedFontFile> font_files;
  if (!root.isMember("font_files")) {
    return font_files;
  }

  const Json::Value& files = root["font_files"];
  if (!files.isArray()) {
    result->errors.AddError("$.font_files", "expected array");
    return font_files;
  }

  std::set<std::string> ids;
  for (Json::ArrayIndex i = 0; i < files.size(); ++i) {
    const Json::Value& file = files[i];
    const std::string path = IndexPath("$.font_files", i);
    if (!RequireObject(file, path, &result->errors)) {
      continue;
    }

    std::string id;
    std::string uri;
    RequireStringField(file, "id", path, &result->errors, &id);
    RequireStringField(file, "uri", path, &result->errors, &uri);
    ValidateNonEmptyString(id, ChildPath(path, "id"), &result->errors);
    if (!id.empty() && !ids.insert(id).second) {
      result->errors.AddError(ChildPath(path, "id"),
                              "font file id must be unique");
    }

    int collection_index = 0;
    if (OptionalIntField(file, "collection_index", path, &result->errors,
                         &collection_index)) {
      ValidateNonNegativeInt(collection_index,
                             ChildPath(path, "collection_index"),
                             &result->errors);
    }
    std::string sha256;
    if (OptionalStringField(file, "sha256", path, &result->errors, &sha256)) {
      ValidateSha256String(sha256, ChildPath(path, "sha256"), &result->errors);
    }

    ResolvedFontFile file_record;
    file_record.id = id;
    file_record.uri = uri;
    file_record.collection_index = collection_index;

    ResolvedRepoFile resolved;
    if (!uri.empty() &&
        resolver.ResolveExistingFile(uri, ChildPath(path, "uri"),
                                     &result->errors, &resolved)) {
      file_record.absolute_path = resolved.absolute_path;
      result->resolved_font_files.push_back(file_record);
    }
    if (!id.empty()) {
      font_files.emplace(id, std::move(file_record));
    }
  }

  return font_files;
}

void ValidateTypefaceRequest(
    const Json::Value& root, const std::string& category,
    const std::unordered_map<std::string, ResolvedFontFile>& font_files,
    ValidationContext* context) {
  const Json::Value* request = nullptr;
  if (!root.isMember("typeface_request")) {
    const bool uses_font_manager_source =
        (category == "glyph_path" || category == "glyph_image") &&
        root.isMember("font_manager_request");
    if (RequiresTypefaceRequest(category) && !uses_font_manager_source) {
      context->AddError("$.typeface_request", "required field is missing");
    }
    return;
  }
  if (!RequireObjectField(root, "typeface_request", "$", context, &request)) {
    return;
  }

  std::string entry;
  if (!RequireStringField(*request, "entry", "$.typeface_request", context,
                          &entry)) {
    return;
  }
  ValidateStringEnum(entry, "$.typeface_request.entry",
                     AllowedTypefaceEntries(), context);

  std::string font_file;
  if (OptionalStringField(*request, "font_file", "$.typeface_request", context,
                          &font_file) &&
      !font_file.empty() && font_files.find(font_file) == font_files.end()) {
    context->AddError("$.typeface_request.font_file",
                      "font_file does not reference font_files[].id");
  }

  if ((entry == "MakeFromFile" || entry == "MakeFromData") &&
      font_file.empty()) {
    context->AddError("$.typeface_request.font_file",
                      "font_file is required for " + entry);
  }

  std::string collection_indices;
  if (OptionalStringField(*request, "collection_indices", "$.typeface_request",
                          context, &collection_indices) &&
      collection_indices != "all") {
    context->AddError("$.typeface_request.collection_indices",
                      "collection_indices must be \"all\"");
  }
}

}  // namespace

CaseValidationResult ValidateCaseDocument(const Json::Value& root,
                                          const RepoUriResolver& resolver) {
  CaseValidationResult result;
  result.normalized_case = root;

  if (!RequireObject(root, "$", &result.errors)) {
    result.valid = false;
    return result;
  }

  int schema_version = 0;
  if (RequireIntField(root, "schema_version", "$", &result.errors,
                      &schema_version) &&
      schema_version != 1) {
    result.errors.AddError("$.schema_version", "expected schema_version 1");
  }

  std::string category;
  std::string status;
  RequireStringField(root, "id", "$", &result.errors, &result.case_id);
  RequireStringField(root, "category", "$", &result.errors, &category);
  RequireStringField(root, "status", "$", &result.errors, &status);
  RequireStringField(root, "backend", "$", &result.errors, &result.backend);

  ValidateNonEmptyString(result.case_id, "$.id", &result.errors);
  if (!category.empty()) {
    ValidateStringEnum(category, "$.category", AllowedCategories(),
                       &result.errors);
  }
  if (!status.empty()) {
    ValidateStringEnum(status, "$.status", AllowedStatuses(), &result.errors);
  }
  if (!result.backend.empty()) {
    ValidateStringEnum(result.backend, "$.backend", AllowedBackendIds(),
                       &result.errors);
  }
  ValidatePlatforms(root, result.backend, &result.errors);

  const auto font_files = ValidateFontFiles(root, resolver, &result);
  ValidateTypefaceRequest(root, category, font_files, &result.errors);
  ValidateFontRequest(root, &result.errors);
  ValidatePaintRequest(root, &result.errors);
  ValidateGlyphs(root, &result.errors);
  ValidateCompare(root, &result.errors);

  result.valid = result.errors.IsValid();
  return result;
}

Json::Value BuildCaseInfoReport(const CaseValidationResult& result) {
  Json::Value report(Json::objectValue);
  report["schema_version"] = 1;
  report["artifact_type"] = "font_case_info";
  report["valid"] = result.valid;
  report["case_id"] = result.case_id;
  report["backend"] = result.backend;
  report["normalized_case"] = result.normalized_case;

  Json::Value resolved_files(Json::arrayValue);
  for (const auto& file : result.resolved_font_files) {
    Json::Value item(Json::objectValue);
    item["id"] = file.id;
    item["uri"] = file.uri;
    item["absolute_path"] = file.absolute_path.string();
    item["collection_index"] = file.collection_index;
    resolved_files.append(std::move(item));
  }
  report["resolved_font_files"] = std::move(resolved_files);
  report["errors"] = result.errors.ToJson();
  return report;
}

}  // namespace font_harness
}  // namespace skity
