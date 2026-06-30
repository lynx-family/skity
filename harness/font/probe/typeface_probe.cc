// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "harness/font/probe/typeface_probe.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
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
#include "harness/font/probe/backend_support.hpp"
#include "harness/font/probe/typeface_identity.hpp"

namespace skity {
namespace font_harness {

namespace {

constexpr size_t kMaxTableSampleBytes = 4096;

std::string DigestBytes(const uint8_t* bytes, size_t size) {
  static constexpr uint64_t kOffsetBasis = 14695981039346656037ull;
  static constexpr uint64_t kPrime = 1099511628211ull;

  uint64_t hash = kOffsetBasis;
  for (size_t i = 0; i < size; ++i) {
    hash ^= static_cast<uint64_t>(bytes[i]);
    hash *= kPrime;
  }

  std::ostringstream stream;
  stream << "fnv1a64:" << std::hex << std::setw(16) << std::setfill('0')
         << hash;
  return stream.str();
}

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

void AddValidationError(Json::Value* report, const std::string& path,
                        const std::string& message);

uint32_t ReadBigEndian32(const std::vector<uint8_t>& bytes, size_t offset) {
  if (bytes.size() < offset + 4) {
    return 0;
  }
  return (static_cast<uint32_t>(bytes[offset]) << 24) |
         (static_cast<uint32_t>(bytes[offset + 1]) << 16) |
         (static_cast<uint32_t>(bytes[offset + 2]) << 8) |
         static_cast<uint32_t>(bytes[offset + 3]);
}

bool ReadFilePrefix(const std::filesystem::path& path, size_t size,
                    std::vector<uint8_t>* bytes) {
  bytes->assign(size, 0);
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return false;
  }
  stream.read(reinterpret_cast<char*>(bytes->data()),
              static_cast<std::streamsize>(bytes->size()));
  bytes->resize(static_cast<size_t>(stream.gcount()));
  return bytes->size() >= size;
}

bool UsesAllCollectionIndices(const Json::Value& typeface_request) {
  std::string collection_indices;
  return ReadStringField(typeface_request, "collection_indices",
                         &collection_indices) &&
         collection_indices == "all";
}

bool ResolveCollectionIndices(const ResolvedFontFile& font_file,
                              const Json::Value& typeface_request,
                              std::vector<int>* indices, Json::Value* report) {
  if (!UsesAllCollectionIndices(typeface_request)) {
    indices->push_back(font_file.collection_index);
    return true;
  }

  std::vector<uint8_t> header;
  if (!ReadFilePrefix(font_file.absolute_path, 12, &header)) {
    AddValidationError(report, "$.font_files[].uri",
                       "failed to read font file header");
    return false;
  }

  int collection_count = 1;
  const uint32_t tag = ReadBigEndian32(header, 0);
  if (tag == SetFourByteTag('t', 't', 'c', 'f')) {
    collection_count = static_cast<int>(ReadBigEndian32(header, 8));
    if (collection_count <= 0) {
      AddValidationError(report, "$.font_files[].uri",
                         "TTC collection count must be positive");
      return false;
    }
  }

  Json::Value scan(Json::objectValue);
  scan["mode"] = "all";
  scan["font_file_uri"] = font_file.uri;
  scan["collection_count"] = collection_count;
  scan["indices"] = Json::Value(Json::arrayValue);
  for (int i = 0; i < collection_count; ++i) {
    indices->push_back(i);
    scan["indices"].append(i);
  }
  (*report)["collection_scan"] = std::move(scan);
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

void AddValidationError(Json::Value* report, const std::string& path,
                        const std::string& message);

Json::Value FontStyleToJson(const FontStyle& style) {
  Json::Value value(Json::objectValue);
  value["weight"] = style.weight();
  value["width"] = style.width();
  value["slant"] = SlantToString(style.slant());
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
      TypefaceVariationPositionToJson(descriptor.variation_position);
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

bool ParseAxisTag(const Json::Value& value, FourByteTag* tag) {
  if (value.isString()) {
    const std::string axis = value.asString();
    if (axis.size() != 4) {
      return false;
    }
    *tag = SetFourByteTag(axis[0], axis[1], axis[2], axis[3]);
    return true;
  }
  if (value.isUInt()) {
    *tag = value.asUInt();
    return true;
  }
  return false;
}

bool ParseVariationPosition(const Json::Value& request,
                            VariationPosition* position, Json::Value* report) {
  if (!request.isMember("variation_position") ||
      !request["variation_position"].isArray()) {
    AddValidationError(report, "$.typeface_request.variation_position",
                       "variation_position array is required for "
                       "MakeVariation");
    return false;
  }

  const Json::Value& coordinates = request["variation_position"];
  for (Json::ArrayIndex i = 0; i < coordinates.size(); ++i) {
    const Json::Value& coordinate = coordinates[i];
    const std::string path =
        "$.typeface_request.variation_position[" + std::to_string(i) + "]";
    if (!coordinate.isObject()) {
      AddValidationError(report, path, "coordinate must be an object");
      return false;
    }

    FourByteTag axis = 0;
    if (coordinate.isMember("axis")) {
      if (!ParseAxisTag(coordinate["axis"], &axis)) {
        AddValidationError(report, path + ".axis",
                           "axis must be a four-character string or uint tag");
        return false;
      }
    } else if (coordinate.isMember("axis_value")) {
      if (!ParseAxisTag(coordinate["axis_value"], &axis)) {
        AddValidationError(report, path + ".axis_value",
                           "axis_value must be a uint tag");
        return false;
      }
    } else {
      AddValidationError(report, path + ".axis",
                         "axis or axis_value is required");
      return false;
    }

    if (!coordinate.isMember("value") || !coordinate["value"].isNumeric()) {
      AddValidationError(report, path + ".value", "value must be numeric");
      return false;
    }
    position->AddCoordinate(axis, coordinate["value"].asFloat());
  }
  return true;
}

Json::Value VariationPositionRequestToJson(const VariationPosition& position) {
  Json::Value value(Json::arrayValue);
  for (const auto& coordinate : position.GetCoordinates()) {
    Json::Value item(Json::objectValue);
    item["axis"] = TagToString(coordinate.axis);
    item["axis_value"] = static_cast<Json::UInt64>(coordinate.axis);
    item["value"] = coordinate.value;
    value.append(std::move(item));
  }
  return value;
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
    std::vector<uint8_t> full_data(table_size);
    const size_t full_copied_size =
        table_size == 0
            ? 0
            : typeface->GetTableData(tag, 0, table_size, full_data.data());

    Json::Value table(Json::objectValue);
    table["tag"] = TagToString(tag);
    table["tag_value"] = static_cast<Json::UInt64>(tag);
    table["size"] = static_cast<Json::UInt64>(table_size);
    table["sample_size"] = static_cast<Json::UInt64>(sample_size);
    table["copied_size"] = static_cast<Json::UInt64>(copied_size);
    if (copied_size > 0 && copied_size == sample_size) {
      table["digest"] = DigestBytes(sample.data(), copied_size);
    }
    table["full_copied_size"] = static_cast<Json::UInt64>(full_copied_size);
    if (full_copied_size > 0 && full_copied_size == table_size) {
      table["full_digest"] = DigestBytes(full_data.data(), full_copied_size);
    }
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
                                       const Json::Value& typeface_request,
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
  if (entry == "MakeVariation") {
    std::string source_entry = "MakeFromFile";
    ReadStringField(typeface_request, "source_entry", &source_entry);
    if (source_entry != "MakeFromFile" && source_entry != "MakeFromData") {
      AddValidationError(report, "$.typeface_request.source_entry",
                         "source_entry must be MakeFromFile or MakeFromData");
      return nullptr;
    }

    auto base_typeface =
        MakeTypeface(source_entry, typeface_request, font_file, report);
    if (!base_typeface) {
      return nullptr;
    }

    VariationPosition position;
    if (!ParseVariationPosition(typeface_request, &position, report)) {
      return nullptr;
    }

    FontArguments args;
    args.SetCollectionIndex(font_file.collection_index)
        .SetVariationDesignPosition(position);
    (*report)["variation_request"]["source_entry"] = source_entry;
    (*report)["variation_request"]["variation_position"] =
        VariationPositionRequestToJson(position);
    return base_typeface->MakeVariation(args);
  }
  return nullptr;
}

Json::Value BuildTypefaceResult(const std::string& entry,
                                const std::string& font_file_id,
                                const ResolvedFontFile& font_file,
                                const std::shared_ptr<Typeface>& typeface) {
  Json::Value typeface_result(Json::objectValue);
  typeface_result["ok"] = true;
  typeface_result["request_entry"] = entry;
  typeface_result["font_file_id"] = font_file_id;
  typeface_result["font_file_uri"] = font_file.uri;
  typeface_result["collection_index"] = font_file.collection_index;
  typeface_result["typeface"] =
      FontDescriptorToJson(typeface->GetFontDescriptor(), font_file.uri);
  typeface_result["identity"] =
      TypefaceIdentityToJson(typeface, typeface->GetFontDescriptor());
  return typeface_result;
}

Json::Value BuildTypefaceProbe(const Json::Value& root,
                               const std::shared_ptr<Typeface>& typeface) {
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
      TypefaceVariationPositionToJson(typeface->GetVariationDesignPosition());
  probe["variation_axes"] =
      TypefaceVariationAxesToJson(typeface->GetVariationDesignParameters());
  probe["glyphs"] = GlyphsToJson(root, typeface);
  return probe;
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
  report["typeface_result"] =
      BuildTypefaceResult(entry, font_file_id, font_file, typeface);
  report["typeface_probe"] = BuildTypefaceProbe(root, typeface);

  return report;
}

Json::Value BuildCollectionSuccessReport(
    const Json::Value& root, const CaseValidationResult& validation,
    const std::string& entry, const std::string& font_file_id,
    const ResolvedFontFile& font_file, const std::vector<int>& indices,
    const std::vector<std::shared_ptr<Typeface>>& typefaces) {
  Json::Value report =
      BuildBaseProbeReport(validation.case_id, validation.backend);
  report["ok"] = true;

  Json::Value collection(Json::objectValue);
  collection["mode"] = "all";
  collection["request_entry"] = entry;
  collection["font_file_id"] = font_file_id;
  collection["font_file_uri"] = font_file.uri;
  collection["collection_count"] = static_cast<Json::UInt64>(indices.size());
  collection["indices"] = Json::Value(Json::arrayValue);

  Json::Value results(Json::arrayValue);
  Json::Value probes(Json::arrayValue);
  for (size_t i = 0; i < indices.size(); ++i) {
    ResolvedFontFile indexed_font_file = font_file;
    indexed_font_file.collection_index = indices[i];
    collection["indices"].append(indices[i]);
    results.append(BuildTypefaceResult(entry, font_file_id, indexed_font_file,
                                       typefaces[i]));
    Json::Value probe = BuildTypefaceProbe(root, typefaces[i]);
    probe["collection_index"] = indices[i];
    probes.append(std::move(probe));
  }

  report["typeface_collection"] = std::move(collection);
  report["typeface_results"] = std::move(results);
  report["typeface_probes"] = std::move(probes);
  return report;
}

}  // namespace

TypefaceProbeResult RunTypefaceProbe(const TypefaceProbeRequest& request) {
  const std::string fallback_case_id =
      StemOrFallback(request.case_path, "case");
  if (!IsExplicitSourceProbeBackend(request.backend) ||
      !IsExplicitSourceProbeBackendAvailable(request.backend)) {
    return BuildFailure(TypefaceProbeStatus::kBackendUnavailable,
                        fallback_case_id, request.backend,
                        "backend_unavailable",
                        ExplicitSourceBackendUnavailableMessage(
                            request.backend, "typeface probe"));
  }

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

  if (entry != "MakeFromFile" && entry != "MakeFromData" &&
      entry != "MakeVariation") {
    TypefaceProbeResult result = BuildFailure(
        TypefaceProbeStatus::kSchemaValidationFailed, validation.case_id,
        validation.backend, "schema_validation_failed");
    AddValidationError(&result.report, "$.typeface_request.entry",
                       "typeface probe supports only MakeFromFile, "
                       "MakeFromData, and MakeVariation");
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
  std::vector<int> collection_indices;
  if (!ResolveCollectionIndices(*font_file, typeface_request,
                                &collection_indices, &report)) {
    TypefaceProbeResult result = BuildFailure(
        TypefaceProbeStatus::kSchemaValidationFailed, validation.case_id,
        validation.backend, "schema_validation_failed");
    result.report["validation_errors"] = report["validation_errors"];
    return result;
  }

  std::vector<std::shared_ptr<Typeface>> typefaces;
  typefaces.reserve(collection_indices.size());
  for (int collection_index : collection_indices) {
    ResolvedFontFile indexed_font_file = *font_file;
    indexed_font_file.collection_index = collection_index;

    auto typeface =
        MakeTypeface(entry, typeface_request, indexed_font_file, &report);
    if (report.isMember("validation_errors")) {
      TypefaceProbeResult result = BuildFailure(
          TypefaceProbeStatus::kSchemaValidationFailed, validation.case_id,
          validation.backend, "schema_validation_failed");
      result.report["validation_errors"] = report["validation_errors"];
      return result;
    }
    if (typeface == nullptr) {
      TypefaceProbeResult result =
          BuildFailure(TypefaceProbeStatus::kProbeFailed, validation.case_id,
                       validation.backend, "typeface_create_failed",
                       "failed to create typeface from explicit font source");
      result.report["collection_index"] = collection_index;
      if (report.isMember("collection_scan")) {
        result.report["collection_scan"] = report["collection_scan"];
      }
      return result;
    }
    typefaces.push_back(std::move(typeface));
  }

  if (typefaces.empty()) {
    return BuildFailure(TypefaceProbeStatus::kProbeFailed, validation.case_id,
                        validation.backend, "typeface_create_failed",
                        "failed to create typeface from explicit font source");
  }

  TypefaceProbeResult result;
  result.status = TypefaceProbeStatus::kSuccess;
  result.case_id = validation.case_id;
  result.backend = validation.backend;
  if (UsesAllCollectionIndices(typeface_request)) {
    result.report =
        BuildCollectionSuccessReport(root, validation, entry, font_file_id,
                                     *font_file, collection_indices, typefaces);
  } else {
    result.report = BuildSuccessReport(root, validation, entry, font_file_id,
                                       *font_file, typefaces.front());
  }
  if (report.isMember("source_data")) {
    result.report["source_data"] = report["source_data"];
  }
  if (report.isMember("collection_scan")) {
    result.report["collection_scan"] = report["collection_scan"];
  }
  if (report.isMember("variation_request")) {
    result.report["variation_request"] = report["variation_request"];
  }
  return result;
}

}  // namespace font_harness
}  // namespace skity
