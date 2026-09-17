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
#include <skity/text/font_arguments.hpp>
#include <skity/text/font_manager.hpp>
#include <skity/text/font_style.hpp>
#include <skity/text/glyph.hpp>
#include <skity/text/typeface.hpp>
#include <string>
#include <utility>
#include <vector>

#include "harness/font/artifact/json_io.hpp"
#include "harness/font/case/case_document.hpp"
#include "harness/font/compare/path/path_normalizer.hpp"
#include "harness/font/probe/backend_support.hpp"
#include "src/render/text/text_transform.hpp"
#include "src/text/scaler_context.hpp"
#include "src/text/scaler_context_desc.hpp"

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

struct FontManagerTypefaceRequest {
  std::string entry;
  std::string family_name;
  bool null_family = true;
  const char* FamilyName() const {
    return null_family ? nullptr : family_name.c_str();
  }
  FontStyle style = FontStyle::Normal();
  bool has_character = false;
  uint32_t character = 0;
  std::string character_label;
  std::vector<std::string> bcp47;
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

bool ReadIntField(const Json::Value& root, const std::string& field, int* out) {
  if (!root.isObject() || !root.isMember(field) || !root[field].isInt()) {
    return false;
  }
  *out = root[field].asInt();
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

void AddValidationError(Json::Value* report, const std::string& path,
                        const std::string& message);

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

std::string TagToString(uint32_t tag) {
  std::string value;
  value.push_back(static_cast<char>((tag >> 24) & 0xFF));
  value.push_back(static_cast<char>((tag >> 16) & 0xFF));
  value.push_back(static_cast<char>((tag >> 8) & 0xFF));
  value.push_back(static_cast<char>(tag & 0xFF));
  return value;
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

bool SlantFromString(const std::string& value, FontStyle::Slant* slant) {
  if (value == "upright") {
    *slant = FontStyle::kUpright_Slant;
    return true;
  }
  if (value == "italic") {
    *slant = FontStyle::kItalic_Slant;
    return true;
  }
  if (value == "oblique") {
    *slant = FontStyle::kOblique_Slant;
    return true;
  }
  return false;
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

bool ParseStyle(const Json::Value& request, const std::string& path,
                FontStyle* style, Json::Value* report) {
  if (!request.isMember("style")) {
    *style = FontStyle::Normal();
    return true;
  }
  const Json::Value& value = request["style"];
  if (!value.isObject()) {
    AddValidationError(report, path + ".style", "expected object");
    return false;
  }

  int weight = FontStyle::kNormal_Weight;
  int width = FontStyle::kNormal_Width;
  FontStyle::Slant slant = FontStyle::kUpright_Slant;
  bool valid = true;

  if (value.isMember("weight") &&
      (!ReadIntField(value, "weight", &weight) || weight < 0 ||
       weight > FontStyle::kExtraBlack_Weight)) {
    AddValidationError(report, path + ".style.weight",
                       "weight must be an integer in [0, 1000]");
    valid = false;
  }
  if (value.isMember("width") && (!ReadIntField(value, "width", &width) ||
                                  width < FontStyle::kUltraCondensed_Width ||
                                  width > FontStyle::kUltraExpanded_Width)) {
    AddValidationError(report, path + ".style.width",
                       "width must be an integer in [1, 9]");
    valid = false;
  }
  if (value.isMember("slant")) {
    std::string slant_value;
    if (!ReadStringField(value, "slant", &slant_value) ||
        !SlantFromString(slant_value, &slant)) {
      AddValidationError(report, path + ".style.slant",
                         "slant must be upright, italic, or oblique");
      valid = false;
    }
  }

  *style = FontStyle(weight, width, slant);
  return valid;
}

bool ParseFontManagerTypefaceRequest(const Json::Value& root,
                                     FontManagerTypefaceRequest* request,
                                     Json::Value* report) {
  if (!root.isMember("font_manager_request") ||
      !root["font_manager_request"].isObject()) {
    AddValidationError(report, "$.font_manager_request",
                       "font_manager_request is required");
    return false;
  }

  const Json::Value& value = root["font_manager_request"];
  if (!ReadStringField(value, "entry", &request->entry)) {
    AddValidationError(report, "$.font_manager_request.entry",
                       "entry is required");
    return false;
  }

  const bool known_entry = request->entry == "GetDefaultTypeface" ||
                           request->entry == "MatchFamilyStyle" ||
                           request->entry == "MatchFamilyStyleCharacter";
  bool valid = known_entry;
  if (!known_entry) {
    AddValidationError(
        report, "$.font_manager_request.entry",
        "glyph path probe supports GetDefaultTypeface, MatchFamilyStyle, and "
        "MatchFamilyStyleCharacter");
  }

  request->null_family = !value["family_name"].isString();
  ReadStringField(value, "family_name", &request->family_name);

  valid =
      ParseStyle(value, "$.font_manager_request", &request->style, report) &&
      valid;

  if (value.isMember("character")) {
    if (!value["character"].isString()) {
      AddValidationError(report, "$.font_manager_request.character",
                         "character must be a U+XXXX code point string");
      valid = false;
    } else {
      request->character_label = value["character"].asString();
      request->character = ParseCodePoint(request->character_label);
      request->has_character = request->character != 0;
      if (!request->has_character) {
        AddValidationError(report, "$.font_manager_request.character",
                           "character must be a U+XXXX code point string");
        valid = false;
      }
    }
  }
  if (request->entry == "MatchFamilyStyleCharacter" &&
      !request->has_character) {
    AddValidationError(report, "$.font_manager_request.character",
                       "character is required for MatchFamilyStyleCharacter");
    valid = false;
  }

  if (value.isMember("bcp47")) {
    if (!value["bcp47"].isArray()) {
      AddValidationError(report, "$.font_manager_request.bcp47",
                         "expected array");
      valid = false;
    } else {
      for (Json::ArrayIndex i = 0; i < value["bcp47"].size(); ++i) {
        if (!value["bcp47"][i].isString()) {
          AddValidationError(report, "$.font_manager_request.bcp47",
                             "bcp47 items must be strings");
          valid = false;
          continue;
        }
        request->bcp47.push_back(value["bcp47"][i].asString());
      }
    }
  }
  return valid;
}

std::shared_ptr<Typeface> MakeTypefaceFromFontManager(
    const FontManagerTypefaceRequest& request) {
  auto font_manager = FontManager::RefDefault();
  if (request.entry == "GetDefaultTypeface") {
    return font_manager->GetDefaultTypeface(request.style);
  }
  if (request.entry == "MatchFamilyStyle") {
    return font_manager->MatchFamilyStyle(request.FamilyName(), request.style);
  }
  if (request.entry == "MatchFamilyStyleCharacter") {
    std::vector<const char*> bcp47;
    bcp47.reserve(request.bcp47.size());
    for (const auto& tag : request.bcp47) {
      bcp47.push_back(tag.c_str());
    }
    return font_manager->MatchFamilyStyleCharacter(
        request.FamilyName(), request.style,
        bcp47.empty() ? nullptr : bcp47.data(), static_cast<int>(bcp47.size()),
        static_cast<Unichar>(request.character));
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

Json::Value FontStyleToJson(const FontStyle& style) {
  Json::Value value(Json::objectValue);
  value["weight"] = style.weight();
  value["width"] = style.width();
  value["slant"] = SlantToString(style.slant());
  return value;
}

Json::Value ExplicitTypefaceSourceToJson(const std::string& entry,
                                         const std::string& font_file_id,
                                         const ResolvedFontFile& font_file) {
  Json::Value value(Json::objectValue);
  value["entry"] = entry;
  value["font_file_id"] = font_file_id;
  value["font_file_uri"] = font_file.uri;
  value["collection_index"] = font_file.collection_index;
  return value;
}

Json::Value FontManagerTypefaceSourceToJson(
    const FontManagerTypefaceRequest& request) {
  Json::Value value(Json::objectValue);
  value["entry"] = "FontManager." + request.entry;
  if (!request.family_name.empty()) {
    value["family_name"] = request.family_name;
  }
  value["style"] = FontStyleToJson(request.style);
  if (request.has_character) {
    value["character"] = request.character_label;
    value["code_point"] = static_cast<Json::UInt64>(request.character);
  }
  Json::Value bcp47(Json::arrayValue);
  for (const auto& tag : request.bcp47) {
    bcp47.append(tag);
  }
  value["bcp47"] = std::move(bcp47);
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

Json::Value BuildGlyphPathBody(const Json::Value& root,
                               const Json::Value& typeface_source,
                               const FontRequest& font_request,
                               const std::shared_ptr<Typeface>& typeface,
                               const PathCompareConfig& compare_config,
                               const PathExpectation& expectation,
                               const PathNormalizeOptions& normalize_options,
                               std::vector<std::string>* errors) {
  Json::Value probe(Json::objectValue);
  probe["category"] = "glyph_path";
  probe["path_compare_mode"] = compare_config.mode;
  probe["path_epsilon"] = compare_config.epsilon;
  probe["typeface_source"] = typeface_source;
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
    scaler_result["available"] = true;
    scaler_result["desc"] = ScalerContextDescToJson(desc);
    scaler_result["load_path_entry"] = "ScalerContext::GetPath";
    scaler_result["glyph_paths"] = BuildScalerGlyphPaths(
        scaler_context.get(), glyphs, expectation, normalize_options, errors);
    probe["scaler_context_result"] = std::move(scaler_result);
  }

  return probe;
}

Json::Value BuildProbeReport(const Json::Value& root,
                             const CaseValidationResult& validation,
                             const Json::Value& typeface_source,
                             const FontRequest& font_request,
                             const std::shared_ptr<Typeface>& typeface,
                             const PathCompareConfig& compare_config,
                             const PathExpectation& expectation,
                             const PathNormalizeOptions& normalize_options,
                             std::vector<std::string>* errors) {
  Json::Value report = BuildBaseReport(validation.case_id, validation.backend,
                                       GlyphPathOutputMode::kProbe);
  report["ok"] = true;
  report["glyph_path_probe"] = BuildGlyphPathBody(
      root, typeface_source, font_request, typeface, compare_config,
      expectation, normalize_options, errors);
  return report;
}

Json::Value BuildDumpReport(const Json::Value& root,
                            const CaseValidationResult& validation,
                            const Json::Value& typeface_source,
                            const FontRequest& font_request,
                            const std::shared_ptr<Typeface>& typeface,
                            const PathCompareConfig& compare_config,
                            const PathExpectation& expectation,
                            const PathNormalizeOptions& normalize_options,
                            std::vector<std::string>* errors) {
  Json::Value report = BuildBaseReport(validation.case_id, validation.backend,
                                       GlyphPathOutputMode::kDump);
  report["ok"] = true;
  report["path_dump"] = BuildGlyphPathBody(
      root, typeface_source, font_request, typeface, compare_config,
      expectation, normalize_options, errors);
  return report;
}

GlyphPathProbeResult RunGlyphPathInternal(const GlyphPathProbeRequest& request,
                                          GlyphPathOutputMode output_mode) {
  const std::string fallback_case_id =
      StemOrFallback(request.case_path, "case");
  if (!IsExplicitSourceProbeBackend(request.backend) ||
      !IsExplicitSourceProbeBackendAvailable(request.backend)) {
    return BuildFailure(GlyphPathProbeStatus::kBackendUnavailable,
                        fallback_case_id, request.backend,
                        "backend_unavailable", output_mode,
                        ExplicitSourceBackendUnavailableMessage(
                            request.backend, "glyph path probe"));
  }

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

  if (!IsExplicitSourceCasePlatformAvailable(root, request.backend)) {
    return BuildFailure(GlyphPathProbeStatus::kBackendUnavailable,
                        validation.case_id, request.backend,
                        "backend_unavailable", output_mode,
                        "case does not target the Linux FreeType host");
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

  Json::Value typeface_source(Json::objectValue);
  std::shared_ptr<Typeface> typeface;
  if (root.isMember("typeface_request") &&
      root["typeface_request"].isObject()) {
    const Json::Value& typeface_request = root["typeface_request"];
    std::string entry;
    std::string font_file_id;
    ReadStringField(typeface_request, "entry", &entry);
    ReadStringField(typeface_request, "font_file", &font_file_id);

    if (entry != "MakeFromFile" && entry != "MakeFromData" &&
        entry != "MakeVariation") {
      GlyphPathProbeResult result = BuildFailure(
          GlyphPathProbeStatus::kSchemaValidationFailed, validation.case_id,
          validation.backend, "schema_validation_failed", output_mode);
      AddValidationError(&result.report, "$.typeface_request.entry",
                         "glyph path probe supports only MakeFromFile and "
                         "MakeFromData, and MakeVariation");
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

    typeface_source =
        ExplicitTypefaceSourceToJson(entry, font_file_id, *font_file);
    typeface =
        MakeTypeface(entry, typeface_request, *font_file, &scratch_report);
    if (scratch_report.isMember("validation_errors")) {
      GlyphPathProbeResult result = BuildFailure(
          GlyphPathProbeStatus::kSchemaValidationFailed, validation.case_id,
          validation.backend, "schema_validation_failed", output_mode);
      result.report["validation_errors"] = scratch_report["validation_errors"];
      return result;
    }
  } else {
    if (!IsHostFontProbeBackendAvailable(request.backend)) {
      return BuildFailure(GlyphPathProbeStatus::kBackendUnavailable,
                          validation.case_id, request.backend,
                          "backend_unavailable", output_mode,
                          HostFontBackendUnavailableMessage(
                              request.backend, "glyph_path probe"));
    }
    FontManagerTypefaceRequest font_manager_request;
    if (!ParseFontManagerTypefaceRequest(root, &font_manager_request,
                                         &scratch_report)) {
      GlyphPathProbeResult result = BuildFailure(
          GlyphPathProbeStatus::kSchemaValidationFailed, validation.case_id,
          validation.backend, "schema_validation_failed", output_mode);
      result.report["validation_errors"] = scratch_report["validation_errors"];
      return result;
    }
    typeface_source = FontManagerTypefaceSourceToJson(font_manager_request);
    typeface = MakeTypefaceFromFontManager(font_manager_request);
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

  if (typeface == nullptr) {
    return BuildFailure(GlyphPathProbeStatus::kProbeFailed, validation.case_id,
                        validation.backend, "typeface_create_failed",
                        output_mode,
                        "failed to create typeface for glyph path probe");
  }

  std::vector<std::string> path_errors;
  GlyphPathProbeResult result;
  result.case_id = validation.case_id;
  result.backend = validation.backend;
  if (output_mode == GlyphPathOutputMode::kDump) {
    result.report = BuildDumpReport(
        root, validation, typeface_source, font_request, typeface,
        compare_config, expectation, normalize_options, &path_errors);
  } else {
    result.report = BuildProbeReport(
        root, validation, typeface_source, font_request, typeface,
        compare_config, expectation, normalize_options, &path_errors);
  }
  if (scratch_report.isMember("source_data")) {
    result.report["source_data"] = scratch_report["source_data"];
  }
  if (scratch_report.isMember("variation_request")) {
    result.report["variation_request"] = scratch_report["variation_request"];
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
