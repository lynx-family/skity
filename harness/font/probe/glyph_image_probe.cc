// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "harness/font/probe/glyph_image_probe.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <memory>
#include <skity/geometry/matrix.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/io/data.hpp>
#include <skity/text/font.hpp>
#include <skity/text/font_arguments.hpp>
#include <skity/text/font_manager.hpp>
#include <skity/text/font_style.hpp>
#include <skity/text/glyph.hpp>
#include <skity/text/typeface.hpp>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "harness/font/artifact/json_io.hpp"
#include "harness/font/case/case_document.hpp"
#include "harness/font/probe/backend_support.hpp"

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

struct ImageRequest {
  float context_scale = 1.0f;
  Matrix transform;
};

struct PaintRequest {
  Color color = Color_BLACK;
  std::string color_string = "#000000FF";
  std::string style = "fill";
  float stroke_width = 1.0f;
};

struct FontManagerTypefaceRequest {
  std::string entry;
  std::string family_name;
  FontStyle style = FontStyle::Normal();
  bool has_character = false;
  uint32_t character = 0;
  std::string character_label;
  std::vector<std::string> bcp47;
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

bool ReadBoolField(const Json::Value& root, const std::string& field,
                   bool* out) {
  if (!root.isObject() || !root.isMember(field) || !root[field].isBool()) {
    return false;
  }
  *out = root[field].asBool();
  return true;
}

int HexValue(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

bool ParseHexByte(const std::string& value, size_t offset, uint8_t* out) {
  int hi = HexValue(value[offset]);
  int lo = HexValue(value[offset + 1]);
  if (hi < 0 || lo < 0) {
    return false;
  }
  *out = static_cast<uint8_t>((hi << 4) | lo);
  return true;
}

bool ParseColorString(const std::string& value, Color* color) {
  if (value.size() != 7 && value.size() != 9) {
    return false;
  }
  if (value[0] != '#') {
    return false;
  }

  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  uint8_t a = 0xFF;
  if (!ParseHexByte(value, 1, &r) || !ParseHexByte(value, 3, &g) ||
      !ParseHexByte(value, 5, &b)) {
    return false;
  }
  if (value.size() == 9 && !ParseHexByte(value, 7, &a)) {
    return false;
  }
  *color = ColorSetARGB(a, r, g, b);
  return true;
}

std::string ColorToString(Color color) {
  std::ostringstream stream;
  stream << "#" << std::uppercase << std::hex << std::setfill('0')
         << std::setw(2) << static_cast<int>(ColorGetR(color)) << std::setw(2)
         << static_cast<int>(ColorGetG(color)) << std::setw(2)
         << static_cast<int>(ColorGetB(color)) << std::setw(2)
         << static_cast<int>(ColorGetA(color));
  return stream.str();
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

GlyphImageProbeResult BuildFailure(GlyphImageProbeStatus status,
                                   const std::string& case_id,
                                   const std::string& backend,
                                   const std::string& reason_code,
                                   const std::string& message = {}) {
  GlyphImageProbeResult result;
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
    AddValidationError(report, "$.font_manager_request.entry",
                       "glyph image probe supports GetDefaultTypeface, "
                       "MatchFamilyStyle, and MatchFamilyStyleCharacter");
  }

  ReadStringField(value, "family_name", &request->family_name);
  if ((request->entry == "MatchFamilyStyle" ||
       request->entry == "MatchFamilyStyleCharacter") &&
      request->family_name.empty()) {
    AddValidationError(report, "$.font_manager_request.family_name",
                       "family_name is required for this entry");
    valid = false;
  }

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
    return font_manager->MatchFamilyStyle(request.family_name.c_str(),
                                          request.style);
  }
  if (request.entry == "MatchFamilyStyleCharacter") {
    std::vector<const char*> bcp47;
    bcp47.reserve(request.bcp47.size());
    for (const auto& tag : request.bcp47) {
      bcp47.push_back(tag.c_str());
    }
    return font_manager->MatchFamilyStyleCharacter(
        request.family_name.c_str(), request.style,
        bcp47.empty() ? nullptr : bcp47.data(), static_cast<int>(bcp47.size()),
        static_cast<Unichar>(request.character));
  }
  return nullptr;
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
                       "font_request is required for glyph image probe");
    return false;
  }

  const Json::Value& font_request = root["font_request"];
  if (!ReadNumberField(font_request, "size", &request->size)) {
    AddValidationError(report, "$.font_request.size",
                       "size is required for glyph image probe");
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

Json::Value MatrixToJson(const Matrix& matrix) {
  Json::Value value(Json::objectValue);
  value["scale_x"] = matrix.GetScaleX();
  value["skew_x"] = matrix.GetSkewX();
  value["skew_y"] = matrix.GetSkewY();
  value["scale_y"] = matrix.GetScaleY();
  return value;
}

bool ParseImageRequest(const Json::Value& root, ImageRequest* request,
                       Json::Value* report) {
  if (!root.isMember("image_request")) {
    return true;
  }
  if (!root["image_request"].isObject()) {
    AddValidationError(report, "$.image_request",
                       "image_request must be an object");
    return false;
  }

  const Json::Value& image_request = root["image_request"];
  ReadNumberField(image_request, "context_scale", &request->context_scale);
  if (request->context_scale <= 0.0f) {
    AddValidationError(report, "$.image_request.context_scale",
                       "context_scale must be positive");
    return false;
  }

  if (image_request.isMember("transform")) {
    const Json::Value& transform = image_request["transform"];
    if (!transform.isObject()) {
      AddValidationError(report, "$.image_request.transform",
                         "transform must be an object");
      return false;
    }
    float scale_x = 1.0f;
    float skew_x = 0.0f;
    float skew_y = 0.0f;
    float scale_y = 1.0f;
    ReadNumberField(transform, "scale_x", &scale_x);
    ReadNumberField(transform, "skew_x", &skew_x);
    ReadNumberField(transform, "skew_y", &skew_y);
    ReadNumberField(transform, "scale_y", &scale_y);
    request->transform =
        Matrix(scale_x, skew_x, 0.0f, skew_y, scale_y, 0.0f, 0.0f, 0.0f, 1.0f);
  }
  return true;
}

Json::Value PaintRequestToJson(const PaintRequest& request) {
  Json::Value value(Json::objectValue);
  value["color"] = request.color_string;
  value["style"] = request.style;
  value["stroke_width"] = request.stroke_width;
  return value;
}

Paint MakePaint(const PaintRequest& request) {
  Paint paint;
  paint.SetColor(request.color);
  if (request.style == "stroke") {
    paint.SetStyle(Paint::kStroke_Style);
    paint.SetStrokeWidth(request.stroke_width);
  }
  return paint;
}

bool ParsePaintRequest(const Json::Value& root, PaintRequest* request,
                       Json::Value* report) {
  if (!root.isMember("paint_request")) {
    return true;
  }
  if (!root["paint_request"].isObject()) {
    AddValidationError(report, "$.paint_request",
                       "paint_request must be an object");
    return false;
  }

  const Json::Value& paint_request = root["paint_request"];
  std::string color;
  if (ReadStringField(paint_request, "color", &color)) {
    Color parsed_color = Color_BLACK;
    if (!ParseColorString(color, &parsed_color)) {
      AddValidationError(report, "$.paint_request.color",
                         "color must be #RRGGBB or #RRGGBBAA");
      return false;
    }
    request->color = parsed_color;
    request->color_string = ColorToString(parsed_color);
  }

  std::string style;
  if (ReadStringField(paint_request, "style", &style)) {
    if (style != "fill" && style != "stroke") {
      AddValidationError(report, "$.paint_request.style",
                         "style must be fill or stroke");
      return false;
    }
    request->style = style;
  }

  if (paint_request.isMember("stroke_width")) {
    if (!paint_request["stroke_width"].isNumeric()) {
      AddValidationError(report, "$.paint_request.stroke_width",
                         "stroke_width must be numeric");
      return false;
    }
    float stroke_width = paint_request["stroke_width"].asFloat();
    if (stroke_width <= 0.0f) {
      AddValidationError(report, "$.paint_request.stroke_width",
                         "stroke_width must be positive");
      return false;
    }
    request->stroke_width = stroke_width;
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

bool PutFinite(Json::Value* object, const std::string& field, float value,
               const std::string& path, std::vector<std::string>* errors) {
  if (!std::isfinite(value)) {
    errors->push_back(path + "." + field + " is not finite");
    return false;
  }
  (*object)[field] = value;
  return true;
}

std::string BitmapFormatToString(BitmapFormat format) {
  switch (format) {
    case BitmapFormat::kUnknown:
      return "unknown";
    case BitmapFormat::kGray8:
      return "gray8";
    case BitmapFormat::kBGRA8:
      return "bgra8";
    case BitmapFormat::kRGBA8:
      return "rgba8";
  }
  return "unknown";
}

size_t BytesPerPixel(BitmapFormat format) {
  switch (format) {
    case BitmapFormat::kGray8:
      return 1;
    case BitmapFormat::kBGRA8:
    case BitmapFormat::kRGBA8:
      return 4;
    case BitmapFormat::kUnknown:
      return 0;
  }
  return 0;
}

Json::Value GlyphImageToJson(const GlyphData& glyph, const std::string& path,
                             std::vector<std::string>* errors) {
  const GlyphBitmapData& image = glyph.Image();
  Json::Value value(Json::objectValue);
  value["glyph_id"] = static_cast<Json::UInt64>(glyph.Id());
  PutFinite(&value, "origin_x", image.origin_x, path, errors);
  PutFinite(&value, "origin_y", image.origin_y, path, errors);
  PutFinite(&value, "origin_x_for_raster", image.origin_x_for_raster, path,
            errors);
  PutFinite(&value, "origin_y_for_raster", image.origin_y_for_raster, path,
            errors);
  PutFinite(&value, "width", image.width, path, errors);
  PutFinite(&value, "height", image.height, path, errors);
  value["format"] = BitmapFormatToString(image.format);
  value["format_value"] = static_cast<int>(image.format);
  value["has_buffer"] = image.buffer != nullptr;
  value["need_free"] = image.need_free;

  const size_t width =
      image.width > 0.0f ? static_cast<size_t>(image.width) : 0;
  const size_t height =
      image.height > 0.0f ? static_cast<size_t>(image.height) : 0;
  const size_t byte_size = width * height * BytesPerPixel(image.format);
  value["byte_size"] = static_cast<Json::UInt64>(byte_size);
  if (image.buffer != nullptr && byte_size > 0) {
    value["digest"] = DigestBytes(image.buffer, byte_size);
  }
  return value;
}

void ReleaseGlyphImage(const GlyphData& glyph) {
  const GlyphBitmapData& image = glyph.Image();
  if (!image.need_free) {
    return;
  }

  std::free(image.buffer);
  GlyphBitmapData& mutable_image = const_cast<GlyphBitmapData&>(image);
  mutable_image.buffer = nullptr;
  mutable_image.need_free = false;
}

Json::Value BuildFontGlyphImages(const Font& font,
                                 const std::vector<GlyphRequest>& glyphs,
                                 const ImageRequest& image_request,
                                 const PaintRequest& paint_request,
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
  Paint paint = MakePaint(paint_request);
  font.LoadGlyphBitmapInfo(
      glyph_ids.data(), static_cast<uint32_t>(glyph_ids.size()),
      glyph_data.data(), paint, image_request.context_scale,
      image_request.transform);
  font.LoadGlyphBitmap(glyph_ids.data(),
                       static_cast<uint32_t>(glyph_ids.size()),
                       glyph_data.data(), paint, image_request.context_scale,
                       image_request.transform);

  for (size_t i = 0; i < glyphs.size(); ++i) {
    const std::string item_path =
        "$.font_result.glyph_images[" + std::to_string(i) + "]";
    Json::Value item = GlyphRequestToJson(glyphs[i]);
    if (glyph_data[i] == nullptr) {
      errors->push_back(item_path + ".glyph_data is missing");
    } else {
      item["image"] =
          GlyphImageToJson(*glyph_data[i], item_path + ".image", errors);
      ReleaseGlyphImage(*glyph_data[i]);
    }
    value.append(std::move(item));
  }
  return value;
}

Json::Value BuildImageReport(const Json::Value& root,
                             const CaseValidationResult& validation,
                             const std::string& category,
                             const Json::Value& typeface_source,
                             const FontRequest& font_request,
                             const ImageRequest& image_request,
                             const PaintRequest& paint_request,
                             const std::shared_ptr<Typeface>& typeface,
                             std::vector<std::string>* errors) {
  Json::Value report =
      BuildBaseProbeReport(validation.case_id, validation.backend);
  report["ok"] = true;

  Json::Value probe(Json::objectValue);
  probe["category"] = category;
  probe["typeface_source"] = typeface_source;
  probe["font_request"] = FontRequestToJson(font_request);
  probe["image_request"]["context_scale"] = image_request.context_scale;
  probe["image_request"]["transform"] = MatrixToJson(image_request.transform);
  probe["paint_request"] = PaintRequestToJson(paint_request);

  Font font = MakeFont(typeface, font_request);
  const std::vector<GlyphRequest> glyphs =
      BuildGlyphRequests(root, typeface, &report);

  Json::Value glyph_requests(Json::arrayValue);
  for (const auto& glyph : glyphs) {
    glyph_requests.append(GlyphRequestToJson(glyph));
  }
  probe["glyph_requests"] = std::move(glyph_requests);

  Json::Value font_result(Json::objectValue);
  font_result["glyph_images"] =
      BuildFontGlyphImages(font, glyphs, image_request, paint_request, errors);
  probe["font_result"] = std::move(font_result);

  report["glyph_image_probe"] = std::move(probe);
  return report;
}

}  // namespace

GlyphImageProbeResult RunGlyphImageProbe(
    const GlyphImageProbeRequest& request) {
  const std::string fallback_case_id =
      StemOrFallback(request.case_path, "case");
  if (!IsExplicitSourceProbeBackend(request.backend) ||
      !IsExplicitSourceProbeBackendAvailable(request.backend)) {
    return BuildFailure(GlyphImageProbeStatus::kBackendUnavailable,
                        fallback_case_id, request.backend,
                        "backend_unavailable",
                        ExplicitSourceBackendUnavailableMessage(
                            request.backend, "glyph image probe"));
  }

  Json::Value root;
  std::string error;
  if (!LoadJsonFile(request.case_path, &root, &error)) {
    return BuildFailure(GlyphImageProbeStatus::kSchemaValidationFailed,
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
    GlyphImageProbeResult result = BuildFailure(
        GlyphImageProbeStatus::kSchemaValidationFailed, validation.case_id,
        validation.backend, "schema_validation_failed");
    result.report["validation_errors"] = validation.errors.ToJson();
    result.report["normalized_case"] = validation.normalized_case;
    return result;
  }

  if (validation.backend != request.backend) {
    GlyphImageProbeResult result = BuildFailure(
        GlyphImageProbeStatus::kSchemaValidationFailed, validation.case_id,
        validation.backend, "schema_validation_failed");
    AddValidationError(&result.report, "$.backend",
                       "case backend does not match --backend");
    return result;
  }

  std::string category;
  ReadStringField(root, "category", &category);
  if (category != "glyph_image") {
    return BuildFailure(GlyphImageProbeStatus::kProbeFailed, validation.case_id,
                        validation.backend, "probe_category_unimplemented",
                        "unsupported case category for glyph image probe");
  }

  Json::Value scratch_report =
      BuildBaseProbeReport(validation.case_id, validation.backend);
  FontRequest font_request;
  if (!ParseFontRequest(root, &font_request, &scratch_report)) {
    GlyphImageProbeResult result = BuildFailure(
        GlyphImageProbeStatus::kSchemaValidationFailed, validation.case_id,
        validation.backend, "schema_validation_failed");
    result.report["validation_errors"] = scratch_report["validation_errors"];
    return result;
  }
  ImageRequest image_request;
  if (!ParseImageRequest(root, &image_request, &scratch_report)) {
    GlyphImageProbeResult result = BuildFailure(
        GlyphImageProbeStatus::kSchemaValidationFailed, validation.case_id,
        validation.backend, "schema_validation_failed");
    result.report["validation_errors"] = scratch_report["validation_errors"];
    return result;
  }
  PaintRequest paint_request;
  if (!ParsePaintRequest(root, &paint_request, &scratch_report)) {
    GlyphImageProbeResult result = BuildFailure(
        GlyphImageProbeStatus::kSchemaValidationFailed, validation.case_id,
        validation.backend, "schema_validation_failed");
    result.report["validation_errors"] = scratch_report["validation_errors"];
    return result;
  }

  std::shared_ptr<Typeface> typeface;
  Json::Value typeface_source(Json::objectValue);
  if (root.isMember("typeface_request")) {
    const Json::Value& typeface_request = root["typeface_request"];
    std::string entry;
    std::string font_file_id;
    ReadStringField(typeface_request, "entry", &entry);
    ReadStringField(typeface_request, "font_file", &font_file_id);

    if (entry != "MakeFromFile" && entry != "MakeFromData" &&
        entry != "MakeVariation") {
      GlyphImageProbeResult result = BuildFailure(
          GlyphImageProbeStatus::kSchemaValidationFailed, validation.case_id,
          validation.backend, "schema_validation_failed");
      AddValidationError(&result.report, "$.typeface_request.entry",
                         "glyph image probe supports only MakeFromFile, "
                         "MakeFromData, and MakeVariation");
      return result;
    }

    const ResolvedFontFile* font_file =
        FindResolvedFont(validation.resolved_font_files, font_file_id);
    if (font_file == nullptr) {
      return BuildFailure(GlyphImageProbeStatus::kSchemaValidationFailed,
                          validation.case_id, validation.backend,
                          "schema_validation_failed",
                          "typeface_request.font_file was not resolved");
    }

    typeface =
        MakeTypeface(entry, typeface_request, *font_file, &scratch_report);
    typeface_source =
        ExplicitTypefaceSourceToJson(entry, font_file_id, *font_file);
  } else {
    FontManagerTypefaceRequest font_manager_request;
    if (!ParseFontManagerTypefaceRequest(root, &font_manager_request,
                                         &scratch_report)) {
      GlyphImageProbeResult result = BuildFailure(
          GlyphImageProbeStatus::kSchemaValidationFailed, validation.case_id,
          validation.backend, "schema_validation_failed");
      result.report["validation_errors"] = scratch_report["validation_errors"];
      return result;
    }
    typeface_source = FontManagerTypefaceSourceToJson(font_manager_request);
    typeface = MakeTypefaceFromFontManager(font_manager_request);
  }
  if (scratch_report.isMember("validation_errors")) {
    GlyphImageProbeResult result = BuildFailure(
        GlyphImageProbeStatus::kSchemaValidationFailed, validation.case_id,
        validation.backend, "schema_validation_failed");
    result.report["validation_errors"] = scratch_report["validation_errors"];
    return result;
  }
  if (typeface == nullptr) {
    return BuildFailure(GlyphImageProbeStatus::kProbeFailed, validation.case_id,
                        validation.backend, "typeface_create_failed",
                        "failed to create typeface");
  }

  std::vector<std::string> image_errors;
  GlyphImageProbeResult result;
  result.case_id = validation.case_id;
  result.backend = validation.backend;
  result.report = BuildImageReport(root, validation, category, typeface_source,
                                   font_request, image_request, paint_request,
                                   typeface, &image_errors);
  if (scratch_report.isMember("source_data")) {
    result.report["source_data"] = scratch_report["source_data"];
  }
  if (scratch_report.isMember("variation_request")) {
    result.report["variation_request"] = scratch_report["variation_request"];
  }

  if (!image_errors.empty()) {
    result.status = GlyphImageProbeStatus::kProbeFailed;
    result.report["ok"] = false;
    result.report["reason_code"] = "glyph_image_invalid";
    Json::Value errors(Json::arrayValue);
    for (const auto& image_error : image_errors) {
      errors.append(image_error);
    }
    result.report["glyph_image_errors"] = std::move(errors);
    return result;
  }

  result.status = GlyphImageProbeStatus::kSuccess;
  return result;
}

}  // namespace font_harness
}  // namespace skity
