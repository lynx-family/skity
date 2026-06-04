// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "harness/font/probe/font_manager_probe.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <memory>
#include <skity/graphic/paint.hpp>
#include <skity/text/font.hpp>
#include <skity/text/font_manager.hpp>
#include <skity/text/glyph.hpp>
#include <skity/text/typeface.hpp>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "harness/font/artifact/json_io.hpp"
#include "harness/font/case/case_document.hpp"
#include "src/render/text/text_transform.hpp"
#include "src/text/scaler_context.hpp"
#include "src/text/scaler_context_desc.hpp"

#ifndef SKITY_FONT_HARNESS_HAS_CORETEXT
#define SKITY_FONT_HARNESS_HAS_CORETEXT 0
#endif

namespace skity {
namespace font_harness {

namespace {

constexpr int kDefaultSampleLimit = 8;
constexpr int kMaxSampleLimit = 32;
constexpr float kDefaultProbeSize = 64.0f;

struct GlyphProbeRequest {
  std::string label;
  uint32_t code_point = 0;
};

struct FontProbeRequest {
  float size = kDefaultProbeSize;
  float scale_x = 1.0f;
  float skew_x = 0.0f;
  bool linear_metrics = false;
  bool subpixel = true;
  bool embolden = false;
  Font::FontHinting hinting = Font::FontHinting::kNormal;
};

struct ParsedFontManagerRequest {
  std::string entry;
  std::string family_name;
  FontStyle style = FontStyle::Normal();
  bool has_character = false;
  uint32_t character = 0;
  std::string character_label;
  std::vector<std::string> bcp47;
  int sample_limit = kDefaultSampleLimit;
  FontProbeRequest font_request;
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

bool ReadIntField(const Json::Value& root, const std::string& field, int* out) {
  if (!root.isObject() || !root.isMember(field) || !root[field].isInt()) {
    return false;
  }
  *out = root[field].asInt();
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

bool ReadBoolField(const Json::Value& root, const std::string& field,
                   bool* out) {
  if (!root.isObject() || !root.isMember(field) || !root[field].isBool()) {
    return false;
  }
  *out = root[field].asBool();
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

std::string TrimNullPadding(std::string value) {
  while (!value.empty() && value.back() == '\0') {
    value.pop_back();
  }
  return value;
}

std::string CodePointToLabel(uint32_t code_point) {
  std::ostringstream stream;
  stream << "U+" << std::uppercase << std::hex
         << std::setw(code_point <= 0xFFFF ? 4 : 6) << std::setfill('0')
         << code_point;
  return stream.str();
}

bool ParseCodePoint(const std::string& value, uint32_t* code_point) {
  if (value.size() < 3 || value.rfind("U+", 0) != 0) {
    return false;
  }
  try {
    size_t consumed = 0;
    unsigned long parsed = std::stoul(value.substr(2), &consumed, 16);
    if (consumed != value.size() - 2 || parsed > 0x10FFFFul) {
      return false;
    }
    *code_point = static_cast<uint32_t>(parsed);
    return true;
  } catch (...) {
    return false;
  }
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

Json::Value FontStyleToJson(const FontStyle& style) {
  Json::Value value(Json::objectValue);
  value["weight"] = style.weight();
  value["width"] = style.width();
  value["slant"] = SlantToString(style.slant());
  return value;
}

Json::Value FontProbeRequestToJson(const FontProbeRequest& request) {
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

Json::Value ParsedRequestToJson(const ParsedFontManagerRequest& request) {
  Json::Value value(Json::objectValue);
  value["entry"] = request.entry;
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
  value["sample_limit"] = request.sample_limit;
  value["font_request"] = FontProbeRequestToJson(request.font_request);
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

FontManagerProbeResult BuildFailure(FontManagerProbeStatus status,
                                    const std::string& case_id,
                                    const std::string& backend,
                                    const std::string& reason_code,
                                    const std::string& message = {}) {
  FontManagerProbeResult result;
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

bool PutFinite(Json::Value* object, const std::string& field, float value,
               const std::string& path, std::vector<std::string>* errors) {
  if (!std::isfinite(value)) {
    errors->push_back(path + "." + field + " is not finite");
    return false;
  }
  (*object)[field] = value;
  return true;
}

Json::Value FontMetricsToJson(const FontMetrics& metrics,
                              const std::string& path,
                              std::vector<std::string>* errors) {
  Json::Value value(Json::objectValue);
  PutFinite(&value, "top", metrics.top_, path, errors);
  PutFinite(&value, "ascent", metrics.ascent_, path, errors);
  PutFinite(&value, "descent", metrics.descent_, path, errors);
  PutFinite(&value, "bottom", metrics.bottom_, path, errors);
  PutFinite(&value, "leading", metrics.leading_, path, errors);
  PutFinite(&value, "avg_char_width", metrics.avg_char_width_, path, errors);
  PutFinite(&value, "max_char_width", metrics.max_char_width_, path, errors);
  PutFinite(&value, "x_min", metrics.x_min_, path, errors);
  PutFinite(&value, "x_max", metrics.x_max_, path, errors);
  PutFinite(&value, "x_height", metrics.x_height_, path, errors);
  PutFinite(&value, "cap_height", metrics.cap_height_, path, errors);
  PutFinite(&value, "underline_thickness", metrics.underline_thickness_, path,
            errors);
  PutFinite(&value, "underline_position", metrics.underline_position_, path,
            errors);
  PutFinite(&value, "strikeout_thickness", metrics.strikeout_thickness_, path,
            errors);
  PutFinite(&value, "strikeout_position", metrics.strikeout_position_, path,
            errors);
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

Font MakeFont(const std::shared_ptr<Typeface>& typeface,
              const FontProbeRequest& request) {
  Font font(typeface, request.size, request.scale_x, request.skew_x);
  font.SetLinearMetrics(request.linear_metrics);
  font.SetSubpixel(request.subpixel);
  font.SetEmbolden(request.embolden);
  font.SetHinting(request.hinting);
  return font;
}

Json::Value FontDescriptorToJson(const FontDescriptor& descriptor) {
  Json::Value value(Json::objectValue);
  value["family_name"] = TrimNullPadding(descriptor.family_name);
  value["post_script_name"] = TrimNullPadding(descriptor.post_script_name);
  value["full_name"] = TrimNullPadding(descriptor.full_name);
  value["style"] = FontStyleToJson(descriptor.style);
  value["collection_index"] = descriptor.collection_index;
  value["factory_id"] = TagToString(descriptor.factory_id);
  value["factory_id_value"] = static_cast<Json::UInt64>(descriptor.factory_id);
  return value;
}

void AppendGlyphIfMissing(std::vector<GlyphProbeRequest>* glyphs,
                          const std::string& label, uint32_t code_point) {
  for (const auto& glyph : *glyphs) {
    if (glyph.code_point == code_point) {
      return;
    }
  }
  GlyphProbeRequest glyph;
  glyph.label = label;
  glyph.code_point = code_point;
  glyphs->push_back(std::move(glyph));
}

std::vector<GlyphProbeRequest> BuildGlyphProbeRequests(
    const Json::Value& root, const ParsedFontManagerRequest& request) {
  std::vector<GlyphProbeRequest> glyphs;
  if (root.isMember("glyphs") && root["glyphs"].isObject() &&
      root["glyphs"].isMember("chars") && root["glyphs"]["chars"].isArray()) {
    for (const auto& item : root["glyphs"]["chars"]) {
      if (!item.isString()) {
        continue;
      }
      uint32_t code_point = 0;
      const std::string label = item.asString();
      if (ParseCodePoint(label, &code_point)) {
        AppendGlyphIfMissing(&glyphs, label, code_point);
      }
    }
  }

  if (glyphs.empty()) {
    AppendGlyphIfMissing(&glyphs, "U+0041", 0x0041);
  }
  if (request.has_character) {
    AppendGlyphIfMissing(&glyphs, request.character_label, request.character);
  }
  return glyphs;
}

Json::Value GlyphProbeToJson(const std::shared_ptr<Typeface>& typeface,
                             const std::vector<GlyphProbeRequest>& glyphs) {
  Json::Value value(Json::arrayValue);
  for (const auto& glyph : glyphs) {
    const GlyphID glyph_id = typeface->UnicharToGlyph(glyph.code_point);
    Json::Value item(Json::objectValue);
    item["char"] = glyph.label;
    item["code_point"] = static_cast<Json::UInt64>(glyph.code_point);
    item["glyph_id"] = static_cast<Json::UInt64>(glyph_id);
    item["contains"] =
        typeface->ContainGlyph(static_cast<Unichar>(glyph.code_point));
    value.append(std::move(item));
  }
  return value;
}

Json::Value BuildProbeSummary(const std::shared_ptr<Typeface>& typeface,
                              const FontProbeRequest& request,
                              const std::vector<GlyphProbeRequest>& glyphs,
                              const std::string& path,
                              std::vector<std::string>* errors) {
  Json::Value summary(Json::objectValue);
  summary["font_request"] = FontProbeRequestToJson(request);

  Font font = MakeFont(typeface, request);
  Json::Value font_result(Json::objectValue);
  FontMetrics public_metrics{};
  font.GetMetrics(&public_metrics);
  font_result["font_metrics"] = FontMetricsToJson(
      public_metrics, path + ".font_result.font_metrics", errors);
  summary["font_result"] = std::move(font_result);

  ScalerContextDesc desc = ScalerContextDesc::MakeCanonicalized(font, Paint());
  auto scaler_context = typeface->CreateScalerContext(&desc);
  Json::Value scaler_result(Json::objectValue);
  scaler_result["desc"] = ScalerContextDescToJson(desc);
  if (scaler_context == nullptr) {
    scaler_result["available"] = false;
    errors->push_back(path + ".scaler_context_result.context is missing");
  } else {
    scaler_result["available"] = true;
    FontMetrics scaler_metrics{};
    scaler_context->GetFontMetrics(&scaler_metrics);
    scaler_result["font_metrics"] = FontMetricsToJson(
        scaler_metrics, path + ".scaler_context_result.font_metrics", errors);
  }
  summary["scaler_context_result"] = std::move(scaler_result);
  summary["glyphs"] = GlyphProbeToJson(typeface, glyphs);
  return summary;
}

Json::Value BuildTypefaceSummary(const std::string& label,
                                 const std::shared_ptr<Typeface>& typeface,
                                 const FontProbeRequest& request,
                                 const std::vector<GlyphProbeRequest>& glyphs,
                                 const std::string& path,
                                 std::vector<std::string>* errors) {
  Json::Value value(Json::objectValue);
  value["label"] = label;
  value["available"] = typeface != nullptr;
  if (typeface == nullptr) {
    errors->push_back(path + ".typeface is missing");
    return value;
  }

  value["typeface_id"] = static_cast<Json::UInt64>(typeface->TypefaceId());
  value["font_style"] = FontStyleToJson(typeface->GetFontStyle());
  value["descriptor"] = FontDescriptorToJson(typeface->GetFontDescriptor());
  value["units_per_em"] = static_cast<Json::UInt64>(typeface->GetUnitsPerEm());
  value["contains_color_table"] = typeface->ContainsColorTable();
  value["table_count"] = typeface->CountTables();
  value["probe_summary"] = BuildProbeSummary(typeface, request, glyphs,
                                             path + ".probe_summary", errors);
  return value;
}

Json::Value BuildFamilySamples(FontManager* font_manager, int family_count,
                               int sample_limit) {
  Json::Value samples(Json::arrayValue);
  const int count = std::min(family_count, sample_limit);
  for (int i = 0; i < count; ++i) {
    Json::Value item(Json::objectValue);
    item["index"] = i;
    item["family_name"] = font_manager->GetFamilyName(i);
    samples.append(std::move(item));
  }
  return samples;
}

int FindFamilyIndex(FontManager* font_manager, int family_count,
                    const std::string& family_name) {
  if (family_name.empty()) {
    return -1;
  }
  for (int i = 0; i < family_count; ++i) {
    if (font_manager->GetFamilyName(i) == family_name) {
      return i;
    }
  }
  return -1;
}

Json::Value BuildFontManagerState(FontManager* font_manager,
                                  const ParsedFontManagerRequest& request) {
  Json::Value state(Json::objectValue);
  const int family_count = font_manager->CountFamilies();
  state["family_count"] = family_count;
  state["family_samples"] =
      BuildFamilySamples(font_manager, family_count, request.sample_limit);

  if (!request.family_name.empty()) {
    const int family_index =
        FindFamilyIndex(font_manager, family_count, request.family_name);
    state["requested_family_name"] = request.family_name;
    state["requested_family_found"] = family_index >= 0;
    state["requested_family_index"] = family_index;
    if (family_index >= 0) {
      state["resolved_family_name"] = font_manager->GetFamilyName(family_index);
    }
  }
  return state;
}

Json::Value BuildStyleSetSummary(const std::string& label,
                                 const std::shared_ptr<FontStyleSet>& style_set,
                                 const ParsedFontManagerRequest& request,
                                 const std::vector<GlyphProbeRequest>& glyphs,
                                 const std::string& path,
                                 Json::Value* matched_typefaces,
                                 std::vector<std::string>* errors) {
  Json::Value value(Json::objectValue);
  value["label"] = label;
  value["available"] = style_set != nullptr;
  if (style_set == nullptr) {
    errors->push_back(path + ".style_set is missing");
    return value;
  }

  const int style_count = style_set->Count();
  value["style_count"] = style_count;
  if (style_count <= 0) {
    errors->push_back(path + ".style_count is zero");
  }

  Json::Value styles(Json::arrayValue);
  const int sample_count = std::min(style_count, request.sample_limit);
  for (int i = 0; i < sample_count; ++i) {
    FontStyle style;
    std::string style_name;
    style_set->GetStyle(i, &style, &style_name);

    Json::Value item(Json::objectValue);
    item["index"] = i;
    item["style_name"] = style_name;
    item["style"] = FontStyleToJson(style);
    item["create_typeface"] = BuildTypefaceSummary(
        label + ".CreateTypeface", style_set->CreateTypeface(i),
        request.font_request, glyphs,
        path + ".styles[" + std::to_string(i) + "].create_typeface", errors);
    styles.append(std::move(item));
  }
  value["styles"] = std::move(styles);

  Json::Value match_style(Json::objectValue);
  match_style["pattern"] = FontStyleToJson(request.style);
  Json::Value matched = BuildTypefaceSummary(
      label + ".MatchStyle", style_set->MatchStyle(request.style),
      request.font_request, glyphs, path + ".match_style.typeface", errors);
  match_style["typeface"] = matched;
  matched_typefaces->append(std::move(matched));
  value["match_style"] = std::move(match_style);
  return value;
}

void ParseFontProbeRequest(const Json::Value& root, FontProbeRequest* request) {
  if (!root.isMember("font_request") || !root["font_request"].isObject()) {
    return;
  }
  const Json::Value& value = root["font_request"];
  ReadNumberField(value, "size", &request->size);
  ReadNumberField(value, "scale_x", &request->scale_x);
  ReadNumberField(value, "skew_x", &request->skew_x);
  ReadBoolField(value, "linear_metrics", &request->linear_metrics);
  ReadBoolField(value, "subpixel", &request->subpixel);
  ReadBoolField(value, "embolden", &request->embolden);

  std::string hinting;
  if (ReadStringField(value, "hinting", &hinting)) {
    request->hinting = HintingFromString(hinting);
  }
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

  if (value.isMember("weight")) {
    if (!ReadIntField(value, "weight", &weight) || weight < 0 ||
        weight > FontStyle::kExtraBlack_Weight) {
      AddValidationError(report, path + ".style.weight",
                         "weight must be an integer in [0, 1000]");
      valid = false;
    }
  }

  if (value.isMember("width")) {
    if (!ReadIntField(value, "width", &width) ||
        width < FontStyle::kUltraCondensed_Width ||
        width > FontStyle::kUltraExpanded_Width) {
      AddValidationError(report, path + ".style.width",
                         "width must be an integer in [1, 9]");
      valid = false;
    }
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

bool IsKnownEntry(const std::string& entry) {
  return entry == "GetDefaultTypeface" || entry == "MatchFamily" ||
         entry == "MatchFamilyStyle" || entry == "MatchFamilyStyleCharacter" ||
         entry == "CreateStyleSet";
}

bool RequiresFamilyName(const std::string& entry) {
  return entry == "MatchFamily" || entry == "MatchFamilyStyle" ||
         entry == "MatchFamilyStyleCharacter" || entry == "CreateStyleSet";
}

bool ParseFontManagerRequest(const Json::Value& root,
                             const std::string& category,
                             ParsedFontManagerRequest* request,
                             Json::Value* report) {
  if (!root.isMember("font_manager_request") ||
      !root["font_manager_request"].isObject()) {
    AddValidationError(report, "$.font_manager_request",
                       "font_manager_request is required");
    return false;
  }

  const Json::Value& value = root["font_manager_request"];
  bool valid = true;
  if (!ReadStringField(value, "entry", &request->entry) ||
      !IsKnownEntry(request->entry)) {
    AddValidationError(report, "$.font_manager_request.entry",
                       "unsupported FontManager entry");
    valid = false;
  }

  if (category == "family_style_set" && request->entry != "CreateStyleSet" &&
      request->entry != "MatchFamily") {
    AddValidationError(report, "$.font_manager_request.entry",
                       "family_style_set supports CreateStyleSet or "
                       "MatchFamily");
    valid = false;
  }

  ReadStringField(value, "family_name", &request->family_name);
  if (RequiresFamilyName(request->entry) && request->family_name.empty()) {
    AddValidationError(report, "$.font_manager_request.family_name",
                       "family_name is required for this entry");
    valid = false;
  }

  valid =
      ParseStyle(value, "$.font_manager_request", &request->style, report) &&
      valid;

  if (value.isMember("character")) {
    if (!value["character"].isString() ||
        !ParseCodePoint(value["character"].asString(), &request->character)) {
      AddValidationError(report, "$.font_manager_request.character",
                         "character must be a U+XXXX code point string");
      valid = false;
    } else {
      request->has_character = true;
      request->character_label = value["character"].asString();
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

  if (value.isMember("sample_limit")) {
    if (!ReadIntField(value, "sample_limit", &request->sample_limit) ||
        request->sample_limit <= 0) {
      AddValidationError(report, "$.font_manager_request.sample_limit",
                         "sample_limit must be a positive integer");
      valid = false;
    } else {
      request->sample_limit = std::min(request->sample_limit, kMaxSampleLimit);
    }
  }

  ParseFontProbeRequest(root, &request->font_request);
  if (!request->has_character && request->character_label.empty()) {
    request->character_label = CodePointToLabel(request->character);
  }
  return valid;
}

Json::Value BuildFontManagerProbeReport(const Json::Value& root,
                                        const CaseValidationResult& validation,
                                        const std::string& category,
                                        const ParsedFontManagerRequest& request,
                                        std::vector<std::string>* errors) {
  Json::Value report =
      BuildBaseProbeReport(validation.case_id, validation.backend);
  report["ok"] = true;

  auto font_manager = FontManager::RefDefault();
  const std::vector<GlyphProbeRequest> glyphs =
      BuildGlyphProbeRequests(root, request);

  Json::Value probe(Json::objectValue);
  probe["category"] = category;
  probe["request"] = ParsedRequestToJson(request);
  probe["font_manager"] = BuildFontManagerState(font_manager.get(), request);
  Json::Value matched_typefaces(Json::arrayValue);

  Json::Value operation(Json::objectValue);
  operation["entry"] = request.entry;
  if (request.entry == "GetDefaultTypeface") {
    Json::Value matched = BuildTypefaceSummary(
        "FontManager.GetDefaultTypeface",
        font_manager->GetDefaultTypeface(request.style), request.font_request,
        glyphs, "$.font_manager_probe.operation.matched_typeface", errors);
    operation["matched_typeface"] = matched;
    matched_typefaces.append(std::move(matched));
  } else if (request.entry == "MatchFamily") {
    operation["style_set"] = BuildStyleSetSummary(
        "FontManager.MatchFamily",
        font_manager->MatchFamily(request.family_name.c_str()), request, glyphs,
        "$.font_manager_probe.operation.style_set", &matched_typefaces, errors);
  } else if (request.entry == "MatchFamilyStyle") {
    Json::Value matched = BuildTypefaceSummary(
        "FontManager.MatchFamilyStyle",
        font_manager->MatchFamilyStyle(request.family_name.c_str(),
                                       request.style),
        request.font_request, glyphs,
        "$.font_manager_probe.operation.matched_typeface", errors);
    operation["matched_typeface"] = matched;
    matched_typefaces.append(std::move(matched));
  } else if (request.entry == "MatchFamilyStyleCharacter") {
    std::vector<const char*> bcp47;
    bcp47.reserve(request.bcp47.size());
    for (const auto& tag : request.bcp47) {
      bcp47.push_back(tag.c_str());
    }
    Json::Value matched = BuildTypefaceSummary(
        "FontManager.MatchFamilyStyleCharacter",
        font_manager->MatchFamilyStyleCharacter(
            request.family_name.c_str(), request.style,
            bcp47.empty() ? nullptr : bcp47.data(),
            static_cast<int>(bcp47.size()),
            static_cast<Unichar>(request.character)),
        request.font_request, glyphs,
        "$.font_manager_probe.operation.matched_typeface", errors);
    operation["matched_typeface"] = matched;
    matched_typefaces.append(std::move(matched));
  } else if (request.entry == "CreateStyleSet") {
    const int family_count = font_manager->CountFamilies();
    const int family_index =
        FindFamilyIndex(font_manager.get(), family_count, request.family_name);
    operation["requested_family_index"] = family_index;
    if (family_index < 0) {
      errors->push_back(
          "$.font_manager_probe.operation.family_index is "
          "missing for CreateStyleSet");
    } else {
      operation["create_style_set"] = BuildStyleSetSummary(
          "FontManager.CreateStyleSet",
          font_manager->CreateStyleSet(family_index), request, glyphs,
          "$.font_manager_probe.operation.create_style_set", &matched_typefaces,
          errors);
    }
    operation["match_family"] = BuildStyleSetSummary(
        "FontManager.MatchFamily",
        font_manager->MatchFamily(request.family_name.c_str()), request, glyphs,
        "$.font_manager_probe.operation.match_family", &matched_typefaces,
        errors);
  } else {
    errors->push_back("$.font_manager_probe.operation.entry is unsupported");
  }

  probe["operation"] = std::move(operation);
  probe["matched_typefaces"] = std::move(matched_typefaces);
  report["font_manager_probe"] = std::move(probe);
  return report;
}

bool IsFontManagerCategory(const std::string& category) {
  return category == "font_manager" || category == "family_style_set";
}

}  // namespace

FontManagerProbeResult RunFontManagerProbe(
    const FontManagerProbeRequest& request) {
  const std::string fallback_case_id =
      StemOrFallback(request.case_path, "case");
  if (request.backend != "coretext") {
    return BuildFailure(
        FontManagerProbeStatus::kBackendUnavailable, fallback_case_id,
        request.backend, "backend_unavailable",
        "font manager probe supports only the coretext backend");
  }
#if !SKITY_FONT_HARNESS_HAS_CORETEXT
  return BuildFailure(FontManagerProbeStatus::kBackendUnavailable,
                      fallback_case_id, request.backend, "backend_unavailable",
                      "CoreText backend is unavailable; build on macOS with "
                      "SKITY_CT_FONT=ON");
#else
  Json::Value root;
  std::string error;
  if (!LoadJsonFile(request.case_path, &root, &error)) {
    return BuildFailure(FontManagerProbeStatus::kSchemaValidationFailed,
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
    FontManagerProbeResult result = BuildFailure(
        FontManagerProbeStatus::kSchemaValidationFailed, validation.case_id,
        validation.backend, "schema_validation_failed");
    result.report["validation_errors"] = validation.errors.ToJson();
    result.report["normalized_case"] = validation.normalized_case;
    return result;
  }

  if (validation.backend != request.backend) {
    FontManagerProbeResult result = BuildFailure(
        FontManagerProbeStatus::kSchemaValidationFailed, validation.case_id,
        validation.backend, "schema_validation_failed");
    AddValidationError(&result.report, "$.backend",
                       "case backend does not match --backend");
    return result;
  }

  std::string category;
  ReadStringField(root, "category", &category);
  if (!IsFontManagerCategory(category)) {
    return BuildFailure(FontManagerProbeStatus::kProbeFailed,
                        validation.case_id, validation.backend,
                        "probe_category_unimplemented",
                        "unsupported case category for font manager probe");
  }

  Json::Value scratch_report =
      BuildBaseProbeReport(validation.case_id, validation.backend);
  ParsedFontManagerRequest font_manager_request;
  if (!ParseFontManagerRequest(root, category, &font_manager_request,
                               &scratch_report)) {
    FontManagerProbeResult result = BuildFailure(
        FontManagerProbeStatus::kSchemaValidationFailed, validation.case_id,
        validation.backend, "schema_validation_failed");
    result.report["validation_errors"] = scratch_report["validation_errors"];
    return result;
  }

  std::vector<std::string> probe_errors;
  FontManagerProbeResult result;
  result.case_id = validation.case_id;
  result.backend = validation.backend;
  result.report = BuildFontManagerProbeReport(
      root, validation, category, font_manager_request, &probe_errors);

  if (!probe_errors.empty()) {
    result.status = FontManagerProbeStatus::kProbeFailed;
    result.report["ok"] = false;
    result.report["reason_code"] = "font_manager_probe_invalid";
    Json::Value errors(Json::arrayValue);
    for (const auto& probe_error : probe_errors) {
      errors.append(probe_error);
    }
    result.report["probe_errors"] = std::move(errors);
    return result;
  }

  result.status = FontManagerProbeStatus::kSuccess;
  return result;
#endif
}

}  // namespace font_harness
}  // namespace skity
