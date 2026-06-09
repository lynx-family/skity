// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "harness/font/probe/glyph_path_probe.hpp"

#include <cstdint>
#include <limits>
#include <memory>
#include <skity/graphic/path.hpp>
#include <skity/io/data.hpp>
#include <skity/text/font.hpp>
#include <skity/text/font_manager.hpp>
#include <skity/text/glyph.hpp>
#include <skity/text/typeface.hpp>
#include <string>
#include <utility>
#include <vector>

#include "harness/font/artifact/json_io.hpp"
#include "harness/font/case/case_document.hpp"
#include "harness/font/compare/path/path_normalizer.hpp"
#include "src/render/text/text_transform.hpp"
#include "src/text/scaler_context.hpp"
#include "src/text/scaler_context_desc.hpp"

#ifndef SKITY_FONT_HARNESS_HAS_CORETEXT
#define SKITY_FONT_HARNESS_HAS_CORETEXT 0
#endif

namespace skity {
namespace font_harness {

namespace {

struct GlyphRequest {
  std::string source;
  std::string label;
  bool has_code_point = false;
  uint32_t code_point = 0;
  GlyphID glyph_id = 0;
};

struct FontRequest {
  float size = 0.0f;
  float scale_x = 1.0f;
  float skew_x = 0.0f;
  bool linear_metrics = false;
  bool subpixel = false;
  bool embolden = false;
  Font::FontHinting hinting = Font::FontHinting::kNormal;
};

struct PathExpectation {
  bool has_empty = false;
  bool empty = false;
};

struct PathCompareConfig {
  std::string mode = "path_normalized";
  double epsilon = kDefaultPathEpsilon;
};

enum class GlyphPathOutputMode {
  kProbe,
  kDump,
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

bool ReadNumberField(const Json::Value& root, const std::string& field,
                     float* out) {
  if (!root.isObject() || !root.isMember(field) || !root[field].isNumeric()) {
    return false;
  }
  *out = root[field].asFloat();
  return true;
}

bool ReadDoubleField(const Json::Value& root, const std::string& field,
                     double* out) {
  if (!root.isObject() || !root.isMember(field) || !root[field].isNumeric()) {
    return false;
  }
  *out = root[field].asDouble();
  return true;
}

bool ReadBoolField(const Json::Value& root, const std::string& field,
                   bool* out) {
  if (!root.isObject() || !root.isMember(field) || !root[field].isBool()) {
    return false;
  }
  *out = root[field].asBool();
  return true;
}

uint32_t ParseCodePoint(const std::string& value) {
  if (value.rfind("U+", 0) != 0) {
    return 0;
  }
  try {
    return static_cast<uint32_t>(std::stoul(value.substr(2), nullptr, 16));
  } catch (...) {
    return 0;
  }
}

std::string HintingToString(Font::FontHinting hinting) {
  switch (hinting) {
    case Font::FontHinting::kNone:
      return "none";
    case Font::FontHinting::kSlight:
      return "slight";
    case Font::FontHinting::kNormal:
      return "normal";
    case Font::FontHinting::kFull:
      return "full";
  }
  return "unknown";
}

Font::FontHinting HintingFromString(const std::string& value) {
  if (value == "none") {
    return Font::FontHinting::kNone;
  }
  if (value == "slight") {
    return Font::FontHinting::kSlight;
  }
  if (value == "full") {
    return Font::FontHinting::kFull;
  }
  return Font::FontHinting::kNormal;
}

Json::Value BuildBaseReport(const std::string& case_id,
                            const std::string& backend,
                            GlyphPathOutputMode output_mode) {
  Json::Value report(Json::objectValue);
  report["schema_version"] = 1;
  report["artifact_type"] = output_mode == GlyphPathOutputMode::kDump
                                ? "font_path_dump"
                                : "font_probe_result";
  report["runner"] = "skity";
  report["case_id"] = case_id;
  report["backend"] = backend;
  report["ok"] = false;
  return report;
}

GlyphPathProbeResult BuildFailure(GlyphPathProbeStatus status,
                                  const std::string& case_id,
                                  const std::string& backend,
                                  const std::string& reason_code,
                                  GlyphPathOutputMode output_mode,
                                  const std::string& message = {}) {
  GlyphPathProbeResult result;
  result.status = status;
  result.case_id = case_id;
  result.backend = backend;
  result.report = BuildBaseReport(case_id, backend, output_mode);
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

Json::Value GlyphRequestToJson(const GlyphRequest& glyph) {
  Json::Value value(Json::objectValue);
  value["source"] = glyph.source;
  value["label"] = glyph.label;
  if (glyph.has_code_point) {
    value["code_point"] = static_cast<Json::UInt64>(glyph.code_point);
  }
  value["glyph_id"] = static_cast<Json::UInt64>(glyph.glyph_id);
  return value;
}

Json::Value Matrix22ToJson(const Matrix22& matrix) {
  Json::Value value(Json::objectValue);
  value["scale_x"] = matrix.GetScaleX();
  value["skew_x"] = matrix.GetSkewX();
  value["skew_y"] = matrix.GetSkewY();
  value["scale_y"] = matrix.GetScaleY();
  return value;
}

Json::Value ScalerContextDescToJson(const ScalerContextDesc& desc) {
  Json::Value value(Json::objectValue);
  value["typeface_id"] = static_cast<Json::UInt64>(desc.typeface_id);
  value["text_size"] = desc.text_size;
  value["scale_x"] = desc.scale_x;
  value["skew_x"] = desc.skew_x;
  value["transform"] = Matrix22ToJson(desc.transform);
  value["context_scale"] = desc.context_scale;
  value["foreground_color"] = static_cast<Json::UInt64>(desc.foreground_color);
  value["stroke_width"] = desc.stroke_width;
  value["miter_limit"] = desc.miter_limit;
  value["cap"] = static_cast<int>(desc.cap);
  value["join"] = static_cast<int>(desc.join);
  value["fake_bold"] = desc.fake_bold != 0;
  value["hinting"] = HintingToString(desc.GetHinting());
  value["hash"] = static_cast<Json::UInt64>(desc.hash());
  return value;
}

Json::Value FontRequestToJson(const FontRequest& request) {
  Json::Value value(Json::objectValue);
  value["size"] = request.size;
  value["scale_x"] = request.scale_x;
  value["skew_x"] = request.skew_x;
  value["linear_metrics"] = request.linear_metrics;
  value["subpixel"] = request.subpixel;
  value["embolden"] = request.embolden;
  value["hinting"] = HintingToString(request.hinting);
  return value;
}

Font MakeFont(const std::shared_ptr<Typeface>& typeface,
              const FontRequest& request) {
  Font font(typeface, request.size, request.scale_x, request.skew_x);
  font.SetLinearMetrics(request.linear_metrics);
  font.SetSubpixel(request.subpixel);
  font.SetEmbolden(request.embolden);
  font.SetHinting(request.hinting);
  return font;
}

bool ParseFontRequest(const Json::Value& root, FontRequest* request,
                      Json::Value* report) {
  if (!root.isMember("font_request") || !root["font_request"].isObject()) {
    AddValidationError(report, "$.font_request",
                       "font_request is required for glyph path probe");
    return false;
  }

  const Json::Value& font_request = root["font_request"];
  if (!ReadNumberField(font_request, "size", &request->size)) {
    AddValidationError(report, "$.font_request.size",
                       "size is required for glyph path probe");
    return false;
  }
  ReadNumberField(font_request, "scale_x", &request->scale_x);
  ReadNumberField(font_request, "skew_x", &request->skew_x);
  ReadBoolField(font_request, "linear_metrics", &request->linear_metrics);
  ReadBoolField(font_request, "subpixel", &request->subpixel);
  ReadBoolField(font_request, "embolden", &request->embolden);

  std::string hinting;
  if (ReadStringField(font_request, "hinting", &hinting)) {
    request->hinting = HintingFromString(hinting);
  }
  return true;
}

std::vector<GlyphRequest> BuildGlyphRequests(
    const Json::Value& root, const std::shared_ptr<Typeface>& typeface,
    Json::Value* report) {
  std::vector<GlyphRequest> glyphs;
  if (!root.isMember("glyphs") || !root["glyphs"].isObject()) {
    return glyphs;
  }

  const Json::Value& glyph_spec = root["glyphs"];
  if (glyph_spec.isMember("chars") && glyph_spec["chars"].isArray()) {
    std::vector<uint32_t> code_points;
    std::vector<std::string> labels;
    for (const auto& item : glyph_spec["chars"]) {
      if (!item.isString()) {
        continue;
      }
      labels.push_back(item.asString());
      code_points.push_back(ParseCodePoint(labels.back()));
    }

    std::vector<GlyphID> glyph_ids(code_points.size());
    if (!code_points.empty()) {
      typeface->UnicharsToGlyphs(code_points.data(),
                                 static_cast<int>(code_points.size()),
                                 glyph_ids.data());
    }

    for (size_t i = 0; i < code_points.size(); ++i) {
      GlyphRequest glyph;
      glyph.source = "char";
      glyph.label = labels[i];
      glyph.has_code_point = true;
      glyph.code_point = code_points[i];
      glyph.glyph_id = glyph_ids[i];
      glyphs.push_back(std::move(glyph));
    }
  }

  if (glyph_spec.isMember("glyph_ids") && glyph_spec["glyph_ids"].isArray()) {
    for (Json::ArrayIndex i = 0; i < glyph_spec["glyph_ids"].size(); ++i) {
      const Json::Value& item = glyph_spec["glyph_ids"][i];
      if (!item.isInt()) {
        continue;
      }
      const int glyph_id = item.asInt();
      if (glyph_id < 0 || glyph_id > std::numeric_limits<GlyphID>::max()) {
        AddValidationError(report, "$.glyphs.glyph_ids",
                           "glyph id must fit uint16_t");
        continue;
      }
      GlyphRequest glyph;
      glyph.source = "glyph_id";
      glyph.label = "gid:" + std::to_string(glyph_id);
      glyph.glyph_id = static_cast<GlyphID>(glyph_id);
      glyphs.push_back(std::move(glyph));
    }
  }

  return glyphs;
}

PathCompareConfig ParsePathCompareConfig(const Json::Value& root) {
  PathCompareConfig config;
  const Json::Value& compare = root["compare"];
  if (!compare.isObject() || !compare["glyph_path"].isObject()) {
    return config;
  }

  const Json::Value& glyph_path = compare["glyph_path"];
  std::string mode;
  if (ReadStringField(glyph_path, "mode", &mode) && mode != "backend_default") {
    config.mode = mode;
  }
  ReadDoubleField(glyph_path, "epsilon", &config.epsilon);
  return config;
}

bool ParsePathExpectation(const Json::Value& root, PathExpectation* expectation,
                          Json::Value* report) {
  if (!root.isMember("path_expectation")) {
    return true;
  }

  const Json::Value& path_expectation = root["path_expectation"];
  if (!path_expectation.isObject()) {
    AddValidationError(report, "$.path_expectation", "expected object");
    return false;
  }
  if (path_expectation.isMember("empty") &&
      !path_expectation["empty"].isBool()) {
    AddValidationError(report, "$.path_expectation.empty", "expected bool");
    return false;
  }
  if (ReadBoolField(path_expectation, "empty", &expectation->empty)) {
    expectation->has_empty = true;
  }
  return true;
}

Json::Value PathExpectationToJson(const PathExpectation& expectation) {
  Json::Value value(Json::objectValue);
  value["has_empty"] = expectation.has_empty;
  if (expectation.has_empty) {
    value["empty"] = expectation.empty;
  }
  return value;
}

Json::Value BuildPathItem(const GlyphRequest& glyph, const Path& path,
                          const std::string& path_prefix,
                          const PathExpectation& expectation,
                          const PathNormalizeOptions& normalize_options,
                          std::vector<std::string>* errors) {
  Json::Value item = GlyphRequestToJson(glyph);
  const bool path_empty = path.IsEmpty();
  const bool path_finite = path.IsFinite();
  item["path_empty"] = path_empty;
  item["path_finite"] = path_finite;
  item["expectation"] = PathExpectationToJson(expectation);
  item["path"] = BuildNormalizedPathJson(path, normalize_options);

  if (!path_finite) {
    errors->push_back(path_prefix + ".path is not finite");
  }
  if (expectation.has_empty && expectation.empty != path_empty) {
    errors->push_back(path_prefix + ".path_empty expected " +
                      std::string(expectation.empty ? "true" : "false"));
  }
  return item;
}

Json::Value BuildFontGlyphPaths(const Font& font,
                                const std::vector<GlyphRequest>& glyphs,
                                const PathExpectation& expectation,
                                const PathNormalizeOptions& normalize_options,
                                std::vector<std::string>* errors) {
  Json::Value value(Json::arrayValue);
  if (glyphs.empty()) {
    return value;
  }

  std::vector<GlyphID> glyph_ids;
  glyph_ids.reserve(glyphs.size());
  for (const auto& glyph : glyphs) {
    glyph_ids.push_back(glyph.glyph_id);
  }

  std::vector<const GlyphData*> glyph_data(glyphs.size(), nullptr);
  font.LoadGlyphPath(glyph_ids.data(), static_cast<uint32_t>(glyph_ids.size()),
                     glyph_data.data());

  for (size_t i = 0; i < glyphs.size(); ++i) {
    const std::string item_path =
        "$.glyph_path_probe.font_result.glyph_paths[" + std::to_string(i) + "]";
    if (glyph_data[i] == nullptr) {
      Json::Value item = GlyphRequestToJson(glyphs[i]);
      item["path_empty"] = true;
      item["path_finite"] = false;
      item["expectation"] = PathExpectationToJson(expectation);
      errors->push_back(item_path + ".glyph_data is missing");
      value.append(std::move(item));
      continue;
    }
    value.append(BuildPathItem(glyphs[i], glyph_data[i]->GetPath(), item_path,
                               expectation, normalize_options, errors));
  }
  return value;
}

Json::Value BuildScalerGlyphPaths(ScalerContext* scaler_context,
                                  const std::vector<GlyphRequest>& glyphs,
                                  const PathExpectation& expectation,
                                  const PathNormalizeOptions& normalize_options,
                                  std::vector<std::string>* errors) {
  Json::Value value(Json::arrayValue);
  for (size_t i = 0; i < glyphs.size(); ++i) {
    const std::string item_path =
        "$.glyph_path_probe.scaler_context_result.glyph_paths[" +
        std::to_string(i) + "]";
    GlyphData glyph_data(glyphs[i].glyph_id);
    scaler_context->MakeGlyph(&glyph_data);
    scaler_context->GetPath(&glyph_data);
    value.append(BuildPathItem(glyphs[i], glyph_data.GetPath(), item_path,
                               expectation, normalize_options, errors));
  }
  return value;
}

Json::Value BuildGlyphPathBody(
    const Json::Value& root, const std::string& entry,
    const std::string& font_file_id, const ResolvedFontFile& font_file,
    const FontRequest& font_request, const std::shared_ptr<Typeface>& typeface,
    const PathCompareConfig& compare_config, const PathExpectation& expectation,
    const PathNormalizeOptions& normalize_options,
    std::vector<std::string>* errors) {
  Json::Value probe(Json::objectValue);
  probe["category"] = "glyph_path";
  probe["path_compare_mode"] = compare_config.mode;
  probe["path_epsilon"] = compare_config.epsilon;
  probe["typeface_source"]["entry"] = entry;
  probe["typeface_source"]["font_file_id"] = font_file_id;
  probe["typeface_source"]["font_file_uri"] = font_file.uri;
  probe["typeface_source"]["collection_index"] = font_file.collection_index;
  probe["font_request"] = FontRequestToJson(font_request);
  probe["path_expectation"] = PathExpectationToJson(expectation);

  Font font = MakeFont(typeface, font_request);
  const std::vector<GlyphRequest> glyphs =
      BuildGlyphRequests(root, typeface, &probe);

  Json::Value glyph_requests(Json::arrayValue);
  for (const auto& glyph : glyphs) {
    glyph_requests.append(GlyphRequestToJson(glyph));
  }
  probe["glyph_requests"] = std::move(glyph_requests);

  Json::Value font_result(Json::objectValue);
  font_result["load_path_entry"] = "Font::LoadGlyphPath";
  font_result["glyph_paths"] =
      BuildFontGlyphPaths(font, glyphs, expectation, normalize_options, errors);
  probe["font_result"] = std::move(font_result);

  ScalerContextDesc desc = ScalerContextDesc::MakeCanonicalized(font, Paint());
  auto scaler_context = typeface->CreateScalerContext(&desc);
  if (scaler_context == nullptr) {
    errors->push_back(
        "$.glyph_path_probe.scaler_context_result.context is "
        "missing");
  } else {
    Json::Value scaler_result(Json::objectValue);
    scaler_result["desc"] = ScalerContextDescToJson(desc);
    scaler_result["load_path_entry"] = "ScalerContext::GetPath";
    scaler_result["glyph_paths"] = BuildScalerGlyphPaths(
        scaler_context.get(), glyphs, expectation, normalize_options, errors);
    probe["scaler_context_result"] = std::move(scaler_result);
  }

  return probe;
}

Json::Value BuildProbeReport(
    const Json::Value& root, const CaseValidationResult& validation,
    const std::string& entry, const std::string& font_file_id,
    const ResolvedFontFile& font_file, const FontRequest& font_request,
    const std::shared_ptr<Typeface>& typeface,
    const PathCompareConfig& compare_config, const PathExpectation& expectation,
    const PathNormalizeOptions& normalize_options,
    std::vector<std::string>* errors) {
  Json::Value report = BuildBaseReport(validation.case_id, validation.backend,
                                       GlyphPathOutputMode::kProbe);
  report["ok"] = true;
  report["glyph_path_probe"] = BuildGlyphPathBody(
      root, entry, font_file_id, font_file, font_request, typeface,
      compare_config, expectation, normalize_options, errors);
  return report;
}

Json::Value BuildDumpReport(
    const Json::Value& root, const CaseValidationResult& validation,
    const std::string& entry, const std::string& font_file_id,
    const ResolvedFontFile& font_file, const FontRequest& font_request,
    const std::shared_ptr<Typeface>& typeface,
    const PathCompareConfig& compare_config, const PathExpectation& expectation,
    const PathNormalizeOptions& normalize_options,
    std::vector<std::string>* errors) {
  Json::Value report = BuildBaseReport(validation.case_id, validation.backend,
                                       GlyphPathOutputMode::kDump);
  report["ok"] = true;
  report["path_dump"] = BuildGlyphPathBody(
      root, entry, font_file_id, font_file, font_request, typeface,
      compare_config, expectation, normalize_options, errors);
  return report;
}

GlyphPathProbeResult RunGlyphPathInternal(const GlyphPathProbeRequest& request,
                                          GlyphPathOutputMode output_mode) {
  const std::string fallback_case_id =
      StemOrFallback(request.case_path, "case");
  if (request.backend != "coretext") {
    return BuildFailure(GlyphPathProbeStatus::kBackendUnavailable,
                        fallback_case_id, request.backend,
                        "backend_unavailable", output_mode,
                        "glyph path probe supports only the coretext backend");
  }
#if !SKITY_FONT_HARNESS_HAS_CORETEXT
  return BuildFailure(GlyphPathProbeStatus::kBackendUnavailable,
                      fallback_case_id, request.backend, "backend_unavailable",
                      output_mode,
                      "CoreText backend is unavailable; build on macOS with "
                      "SKITY_CT_FONT=ON");
#else
  Json::Value root;
  std::string error;
  if (!LoadJsonFile(request.case_path, &root, &error)) {
    return BuildFailure(GlyphPathProbeStatus::kSchemaValidationFailed,
                        fallback_case_id, request.backend,
                        "schema_validation_failed", output_mode, error);
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
    GlyphPathProbeResult result = BuildFailure(
        GlyphPathProbeStatus::kSchemaValidationFailed, validation.case_id,
        validation.backend, "schema_validation_failed", output_mode);
    result.report["validation_errors"] = validation.errors.ToJson();
    result.report["normalized_case"] = validation.normalized_case;
    return result;
  }

  if (validation.backend != request.backend) {
    GlyphPathProbeResult result = BuildFailure(
        GlyphPathProbeStatus::kSchemaValidationFailed, validation.case_id,
        validation.backend, "schema_validation_failed", output_mode);
    AddValidationError(&result.report, "$.backend",
                       "case backend does not match --backend");
    return result;
  }

  std::string category;
  ReadStringField(root, "category", &category);
  if (category != "glyph_path") {
    return BuildFailure(GlyphPathProbeStatus::kProbeFailed, validation.case_id,
                        validation.backend, "probe_category_unimplemented",
                        output_mode,
                        "unsupported case category for glyph path probe");
  }

  const Json::Value& typeface_request = root["typeface_request"];
  std::string entry;
  std::string font_file_id;
  ReadStringField(typeface_request, "entry", &entry);
  ReadStringField(typeface_request, "font_file", &font_file_id);

  if (entry != "MakeFromFile" && entry != "MakeFromData") {
    GlyphPathProbeResult result = BuildFailure(
        GlyphPathProbeStatus::kSchemaValidationFailed, validation.case_id,
        validation.backend, "schema_validation_failed", output_mode);
    AddValidationError(&result.report, "$.typeface_request.entry",
                       "glyph path probe supports only MakeFromFile and "
                       "MakeFromData");
    return result;
  }

  const ResolvedFontFile* font_file =
      FindResolvedFont(validation.resolved_font_files, font_file_id);
  if (font_file == nullptr) {
    return BuildFailure(GlyphPathProbeStatus::kSchemaValidationFailed,
                        validation.case_id, validation.backend,
                        "schema_validation_failed", output_mode,
                        "typeface_request.font_file was not resolved");
  }

  Json::Value scratch_report =
      BuildBaseReport(validation.case_id, validation.backend, output_mode);
  FontRequest font_request;
  if (!ParseFontRequest(root, &font_request, &scratch_report)) {
    GlyphPathProbeResult result = BuildFailure(
        GlyphPathProbeStatus::kSchemaValidationFailed, validation.case_id,
        validation.backend, "schema_validation_failed", output_mode);
    result.report["validation_errors"] = scratch_report["validation_errors"];
    return result;
  }

  PathExpectation expectation;
  if (!ParsePathExpectation(root, &expectation, &scratch_report)) {
    GlyphPathProbeResult result = BuildFailure(
        GlyphPathProbeStatus::kSchemaValidationFailed, validation.case_id,
        validation.backend, "schema_validation_failed", output_mode);
    result.report["validation_errors"] = scratch_report["validation_errors"];
    return result;
  }

  PathCompareConfig compare_config = ParsePathCompareConfig(root);
  PathNormalizeOptions normalize_options;
  normalize_options.epsilon = compare_config.epsilon;

  if (compare_config.mode != "path_normalized") {
    GlyphPathProbeResult result = BuildFailure(
        GlyphPathProbeStatus::kSchemaValidationFailed, validation.case_id,
        validation.backend, "schema_validation_failed", output_mode);
    AddValidationError(&result.report, "$.compare.glyph_path.mode",
                       "glyph path probe supports only path_normalized");
    return result;
  }

  auto typeface = MakeTypeface(entry, *font_file, &scratch_report);
  if (typeface == nullptr) {
    return BuildFailure(GlyphPathProbeStatus::kProbeFailed, validation.case_id,
                        validation.backend, "typeface_create_failed",
                        output_mode,
                        "failed to create typeface from explicit font source");
  }

  std::vector<std::string> path_errors;
  GlyphPathProbeResult result;
  result.case_id = validation.case_id;
  result.backend = validation.backend;
  if (output_mode == GlyphPathOutputMode::kDump) {
    result.report = BuildDumpReport(
        root, validation, entry, font_file_id, *font_file, font_request,
        typeface, compare_config, expectation, normalize_options, &path_errors);
  } else {
    result.report = BuildProbeReport(
        root, validation, entry, font_file_id, *font_file, font_request,
        typeface, compare_config, expectation, normalize_options, &path_errors);
  }
  if (scratch_report.isMember("source_data")) {
    result.report["source_data"] = scratch_report["source_data"];
  }

  if (!path_errors.empty()) {
    result.status = GlyphPathProbeStatus::kProbeFailed;
    result.report["ok"] = false;
    result.report["reason_code"] = "path_invalid";
    Json::Value errors(Json::arrayValue);
    for (const auto& path_error : path_errors) {
      errors.append(path_error);
    }
    result.report["path_errors"] = std::move(errors);
    return result;
  }

  result.status = GlyphPathProbeStatus::kSuccess;
  return result;
#endif
}

}  // namespace

GlyphPathProbeResult RunGlyphPathProbe(const GlyphPathProbeRequest& request) {
  return RunGlyphPathInternal(request, GlyphPathOutputMode::kProbe);
}

GlyphPathProbeResult RunGlyphPathDump(const GlyphPathProbeRequest& request) {
  return RunGlyphPathInternal(request, GlyphPathOutputMode::kDump);
}

}  // namespace font_harness
}  // namespace skity
