// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "harness/font/probe/typeface_probe.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <memory>
#include <skity/io/data.hpp>
#include <skity/text/font_manager.hpp>
#include <skity/text/typeface.hpp>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "harness/font/artifact/json_io.hpp"
#include "harness/font/case/case_document.hpp"

#ifndef SKITY_FONT_HARNESS_HAS_CORETEXT
#define SKITY_FONT_HARNESS_HAS_CORETEXT 0
#endif

namespace skity {
namespace font_harness {

namespace {

constexpr size_t kMaxTableSampleBytes = 4096;

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

std::string TagToString(uint32_t tag) {
  std::string value;
  value.push_back(static_cast<char>((tag >> 24) & 0xFF));
  value.push_back(static_cast<char>((tag >> 16) & 0xFF));
  value.push_back(static_cast<char>((tag >> 8) & 0xFF));
  value.push_back(static_cast<char>(tag & 0xFF));
  for (char c : value) {
    if (!std::isprint(static_cast<unsigned char>(c))) {
      std::ostringstream stream;
      stream << "0x" << std::hex << std::setw(8) << std::setfill('0') << tag;
      return stream.str();
    }
  }
  return value;
}

std::string SlantToString(FontStyle::Slant slant) {
  switch (slant) {
    case FontStyle::kUpright_Slant:
      return "upright";
    case FontStyle::kItalic_Slant:
      return "italic";
    case FontStyle::kOblique_Slant:
      return "oblique";
  }
  return "unknown";
}

Json::Value FontStyleToJson(const FontStyle& style) {
  Json::Value value(Json::objectValue);
  value["weight"] = style.weight();
  value["width"] = style.width();
  value["slant"] = SlantToString(style.slant());
  return value;
}

Json::Value VariationPositionToJson(const VariationPosition& position) {
  std::vector<VariationPosition::Coordinate> coordinates(
      position.GetCoordinates().begin(), position.GetCoordinates().end());
  std::sort(coordinates.begin(), coordinates.end(),
            [](const auto& lhs, const auto& rhs) {
              if (lhs.axis != rhs.axis) {
                return lhs.axis < rhs.axis;
              }
              return lhs.value < rhs.value;
            });

  Json::Value value(Json::arrayValue);
  for (const auto& coordinate : coordinates) {
    Json::Value item(Json::objectValue);
    item["axis"] = TagToString(coordinate.axis);
    item["axis_value"] = static_cast<Json::UInt64>(coordinate.axis);
    item["value"] = coordinate.value;
    value.append(std::move(item));
  }
  return value;
}

Json::Value VariationAxesToJson(std::vector<VariationAxis> axes) {
  std::sort(axes.begin(), axes.end(),
            [](const auto& lhs, const auto& rhs) { return lhs.tag < rhs.tag; });

  Json::Value value(Json::arrayValue);
  for (const auto& axis : axes) {
    Json::Value item(Json::objectValue);
    item["tag"] = TagToString(axis.tag);
    item["tag_value"] = static_cast<Json::UInt64>(axis.tag);
    item["min"] = axis.min;
    item["default"] = axis.def;
    item["max"] = axis.max;
    item["hidden"] = axis.hidden;
    value.append(std::move(item));
  }
  return value;
}

Json::Value FontDescriptorToJson(const FontDescriptor& descriptor,
                                 const std::string& font_uri) {
  Json::Value value(Json::objectValue);
  value["family_name"] = descriptor.family_name;
  value["post_script_name"] = descriptor.post_script_name;
  value["full_name"] = descriptor.full_name;
  value["style"] = FontStyleToJson(descriptor.style);
  value["collection_index"] = descriptor.collection_index;
  value["font_file"] = font_uri;
  value["variation_position"] =
      VariationPositionToJson(descriptor.variation_position);
  value["factory_id"] = TagToString(descriptor.factory_id);
  value["factory_id_value"] = static_cast<Json::UInt64>(descriptor.factory_id);
  return value;
}

uint32_t ParseCodePoint(const std::string& value) {
  if (value.rfind("U+", 0) != 0) {
    return 0;
  }
  return static_cast<uint32_t>(std::stoul(value.substr(2), nullptr, 16));
}

Json::Value GlyphsToJson(const Json::Value& root,
                         const std::shared_ptr<Typeface>& typeface) {
  Json::Value glyphs(Json::arrayValue);
  if (!root.isMember("glyphs") || !root["glyphs"].isObject() ||
      !root["glyphs"].isMember("chars") || !root["glyphs"]["chars"].isArray()) {
    return glyphs;
  }

  std::vector<uint32_t> code_points;
  std::vector<std::string> labels;
  for (const auto& item : root["glyphs"]["chars"]) {
    if (!item.isString()) {
      continue;
    }
    labels.push_back(item.asString());
    code_points.push_back(ParseCodePoint(labels.back()));
  }

  std::vector<GlyphID> sequence_glyphs(code_points.size());
  if (!code_points.empty()) {
    typeface->UnicharsToGlyphs(code_points.data(),
                               static_cast<int>(code_points.size()),
                               sequence_glyphs.data());
  }

  for (size_t i = 0; i < code_points.size(); ++i) {
    const GlyphID single_glyph = typeface->UnicharToGlyph(code_points[i]);
    Json::Value glyph(Json::objectValue);
    glyph["char"] = labels[i];
    glyph["code_point"] = static_cast<Json::UInt64>(code_points[i]);
    glyph["glyph_id"] = static_cast<Json::UInt64>(single_glyph);
    glyph["sequence_glyph_id"] = static_cast<Json::UInt64>(sequence_glyphs[i]);
    glyph["contains"] = typeface->ContainGlyph(code_points[i]);
    glyphs.append(std::move(glyph));
  }
  return glyphs;
}

Json::Value TablesToJson(const std::shared_ptr<Typeface>& typeface,
                         int* table_count, int* copied_tag_count) {
  *table_count = typeface->CountTables();
  *copied_tag_count = 0;
  Json::Value tables(Json::arrayValue);
  if (*table_count <= 0) {
    return tables;
  }

  std::vector<FontTableTag> tags(static_cast<size_t>(*table_count));
  *copied_tag_count = typeface->GetTableTags(tags.data());
  tags.resize(static_cast<size_t>(*copied_tag_count));
  std::sort(tags.begin(), tags.end());

  for (FontTableTag tag : tags) {
    const size_t table_size = typeface->GetTableSize(tag);
    const size_t sample_size = std::min(table_size, kMaxTableSampleBytes);
    std::vector<uint8_t> sample(sample_size);
    const size_t copied_size =
        sample_size == 0
            ? 0
            : typeface->GetTableData(tag, 0, sample_size, sample.data());

    Json::Value table(Json::objectValue);
    table["tag"] = TagToString(tag);
    table["tag_value"] = static_cast<Json::UInt64>(tag);
    table["size"] = static_cast<Json::UInt64>(table_size);
    table["sample_size"] = static_cast<Json::UInt64>(sample_size);
    table["copied_size"] = static_cast<Json::UInt64>(copied_size);
    tables.append(std::move(table));
  }
  return tables;
}

Json::Value BuildBaseProbeReport(const std::string& case_id,
                                 const std::string& backend) {
  Json::Value report(Json::objectValue);
  report["schema_version"] = 1;
  report["artifact_type"] = "font_probe_result";
  report["runner"] = "skity";
  report["case_id"] = case_id;
  report["backend"] = backend;
  report["ok"] = false;
  return report;
}

TypefaceProbeResult BuildFailure(TypefaceProbeStatus status,
                                 const std::string& case_id,
                                 const std::string& backend,
                                 const std::string& reason_code,
                                 const std::string& message = {}) {
  TypefaceProbeResult result;
  result.status = status;
  result.case_id = case_id;
  result.backend = backend;
  result.report = BuildBaseProbeReport(case_id, backend);
  result.report["reason_code"] = reason_code;
  if (!message.empty()) {
    result.report["message"] = message;
  }
  return result;
}

void AddValidationError(Json::Value* report, const std::string& path,
                        const std::string& message) {
  Json::Value error(Json::objectValue);
  error["path"] = path;
  error["message"] = message;
  if (!report->isMember("validation_errors")) {
    (*report)["validation_errors"] = Json::Value(Json::arrayValue);
  }
  (*report)["validation_errors"].append(std::move(error));
}

const ResolvedFontFile* FindResolvedFont(
    const std::vector<ResolvedFontFile>& files, const std::string& id) {
  for (const auto& file : files) {
    if (file.id == id) {
      return &file;
    }
  }
  return nullptr;
}

std::shared_ptr<Typeface> MakeTypeface(const std::string& entry,
                                       const ResolvedFontFile& font_file,
                                       Json::Value* report) {
  auto font_manager = FontManager::RefDefault();
  if (entry == "MakeFromFile") {
    return font_manager->MakeFromFile(font_file.absolute_path.string().c_str(),
                                      font_file.collection_index);
  }
  if (entry == "MakeFromData") {
    auto data =
        Data::MakeFromFileName(font_file.absolute_path.string().c_str());
    Json::Value source(Json::objectValue);
    source["data_size"] =
        data ? static_cast<Json::UInt64>(data->Size()) : Json::UInt64(0);
    (*report)["source_data"] = std::move(source);
    if (!data || data->IsEmpty()) {
      return nullptr;
    }
    return font_manager->MakeFromData(data, font_file.collection_index);
  }
  return nullptr;
}

Json::Value BuildSuccessReport(const Json::Value& root,
                               const CaseValidationResult& validation,
                               const std::string& entry,
                               const std::string& font_file_id,
                               const ResolvedFontFile& font_file,
                               const std::shared_ptr<Typeface>& typeface) {
  Json::Value report =
      BuildBaseProbeReport(validation.case_id, validation.backend);
  report["ok"] = true;

  Json::Value typeface_result(Json::objectValue);
  typeface_result["ok"] = true;
  typeface_result["request_entry"] = entry;
  typeface_result["font_file_id"] = font_file_id;
  typeface_result["font_file_uri"] = font_file.uri;
  typeface_result["collection_index"] = font_file.collection_index;
  typeface_result["typeface"] =
      FontDescriptorToJson(typeface->GetFontDescriptor(), font_file.uri);
  report["typeface_result"] = std::move(typeface_result);

  Json::Value probe(Json::objectValue);
  probe["units_per_em"] = static_cast<Json::UInt64>(typeface->GetUnitsPerEm());
  probe["contains_color_table"] = typeface->ContainsColorTable();

  auto data = typeface->GetData();
  probe["data_size"] =
      data ? static_cast<Json::UInt64>(data->Size()) : Json::UInt64(0);

  int table_count = 0;
  int copied_tag_count = 0;
  probe["tables"] = TablesToJson(typeface, &table_count, &copied_tag_count);
  probe["table_count"] = table_count;
  probe["copied_table_tag_count"] = copied_tag_count;
  probe["variation_position"] =
      VariationPositionToJson(typeface->GetVariationDesignPosition());
  probe["variation_axes"] =
      VariationAxesToJson(typeface->GetVariationDesignParameters());
  probe["glyphs"] = GlyphsToJson(root, typeface);
  report["typeface_probe"] = std::move(probe);

  return report;
}

}  // namespace

TypefaceProbeResult RunTypefaceProbe(const TypefaceProbeRequest& request) {
  const std::string fallback_case_id =
      StemOrFallback(request.case_path, "case");
  if (request.backend != "coretext") {
    return BuildFailure(TypefaceProbeStatus::kBackendUnavailable,
                        fallback_case_id, request.backend,
                        "backend_unavailable",
                        "typeface probe supports only the coretext backend");
  }
#if !SKITY_FONT_HARNESS_HAS_CORETEXT
  return BuildFailure(TypefaceProbeStatus::kBackendUnavailable,
                      fallback_case_id, request.backend, "backend_unavailable",
                      "CoreText backend is unavailable; build on macOS with "
                      "SKITY_CT_FONT=ON");
#else
  Json::Value root;
  std::string error;
  if (!LoadJsonFile(request.case_path, &root, &error)) {
    return BuildFailure(TypefaceProbeStatus::kSchemaValidationFailed,
                        fallback_case_id, request.backend,
                        "schema_validation_failed", error);
  }

  RepoUriResolver resolver(request.repo_root);
  CaseValidationResult validation = ValidateCaseDocument(root, resolver);
  if (validation.case_id.empty()) {
    validation.case_id = fallback_case_id;
  }
  if (validation.backend.empty()) {
    validation.backend = request.backend;
  }

  if (!validation.valid) {
    TypefaceProbeResult result = BuildFailure(
        TypefaceProbeStatus::kSchemaValidationFailed, validation.case_id,
        validation.backend, "schema_validation_failed");
    result.report["validation_errors"] = validation.errors.ToJson();
    result.report["normalized_case"] = validation.normalized_case;
    return result;
  }

  if (validation.backend != request.backend) {
    TypefaceProbeResult result = BuildFailure(
        TypefaceProbeStatus::kSchemaValidationFailed, validation.case_id,
        validation.backend, "schema_validation_failed");
    AddValidationError(&result.report, "$.backend",
                       "case backend does not match --backend");
    return result;
  }

  std::string category;
  ReadStringField(root, "category", &category);
  if (category != "typeface_probe") {
    return BuildFailure(TypefaceProbeStatus::kProbeFailed, validation.case_id,
                        validation.backend, "probe_category_unimplemented",
                        "unsupported case category for typeface probe");
  }

  const Json::Value& typeface_request = root["typeface_request"];
  std::string entry;
  std::string font_file_id;
  ReadStringField(typeface_request, "entry", &entry);
  ReadStringField(typeface_request, "font_file", &font_file_id);

  if (entry != "MakeFromFile" && entry != "MakeFromData") {
    TypefaceProbeResult result = BuildFailure(
        TypefaceProbeStatus::kSchemaValidationFailed, validation.case_id,
        validation.backend, "schema_validation_failed");
    AddValidationError(&result.report, "$.typeface_request.entry",
                       "typeface probe supports only MakeFromFile and "
                       "MakeFromData");
    return result;
  }

  const ResolvedFontFile* font_file =
      FindResolvedFont(validation.resolved_font_files, font_file_id);
  if (font_file == nullptr) {
    return BuildFailure(TypefaceProbeStatus::kSchemaValidationFailed,
                        validation.case_id, validation.backend,
                        "schema_validation_failed",
                        "typeface_request.font_file was not resolved");
  }

  Json::Value report =
      BuildBaseProbeReport(validation.case_id, validation.backend);
  auto typeface = MakeTypeface(entry, *font_file, &report);
  if (typeface == nullptr) {
    return BuildFailure(TypefaceProbeStatus::kProbeFailed, validation.case_id,
                        validation.backend, "typeface_create_failed",
                        "failed to create typeface from explicit font source");
  }

  TypefaceProbeResult result;
  result.status = TypefaceProbeStatus::kSuccess;
  result.case_id = validation.case_id;
  result.backend = validation.backend;
  result.report = BuildSuccessReport(root, validation, entry, font_file_id,
                                     *font_file, typeface);
  if (report.isMember("source_data")) {
    result.report["source_data"] = report["source_data"];
  }
  return result;
#endif
}

}  // namespace font_harness
}  // namespace skity
