// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <vector>

#include "harness/font/artifact/artifact_validator.hpp"
#include "harness/font/case/font_manager_contract.hpp"

namespace skity {
namespace font_harness {
namespace {

// Invalid container types are reported instead of invoking JsonCpp's throwing
// subscript operators. This also keeps diagnostics attached to the input path.
struct Node {
  const Json::Value& value;
  std::string path;
  ValidationContext* errors;

  Node operator[](const std::string& key) const {
    static const Json::Value null;
    return {value.isObject() ? value[key] : null, ChildPath(path, key), errors};
  }
  Node operator[](Json::ArrayIndex i) const {
    static const Json::Value null;
    return {value.isArray() && i < value.size() ? value[i] : null,
            IndexPath(path, i), errors};
  }
  bool Check(bool valid, const std::string& message) const {
    if (!valid) {
      errors->AddError(path, message);
    }
    return valid;
  }
  bool Object() const { return Check(value.isObject(), "expected object"); }
  bool Array(size_t count) const {
    return Check(value.isArray() && value.size() == count,
                 "expected array with " + std::to_string(count) + " items");
  }
  bool String(bool nonempty = false) const {
    return Check(value.isString() && (!nonempty || !value.asString().empty()),
                 nonempty ? "expected nonempty string" : "expected string");
  }
  bool Number() const {
    return Check(value.isNumeric() && std::isfinite(value.asDouble()),
                 "expected finite number");
  }
  bool Integer() const {
    return Check(value.isUInt64(), "expected nonnegative integer");
  }
  bool Equal(const Json::Value& expected) const {
    const bool equal = value.isNumeric() && expected.isNumeric()
                           ? value.asDouble() == expected.asDouble()
                           : value == expected;
    return Check(equal, "does not match case request");
  }
};

void CheckFiniteTree(Node node) {
  if (node.value.isDouble()) {
    node.Number();
  } else if (node.value.isObject()) {
    for (const auto& name : node.value.getMemberNames()) {
      CheckFiniteTree(node[name]);
    }
  } else if (node.value.isArray()) {
    for (Json::ArrayIndex i = 0; i < node.value.size(); ++i) {
      CheckFiniteTree(node[i]);
    }
  }
}

void CheckRequest(Node node, const Json::Value& request) {
  if (request.isObject()) {
    if (!node.Object()) {
      return;
    }
    for (const auto& name : request.getMemberNames()) {
      CheckRequest(node[name], request[name]);
    }
  } else if (request.isNumeric()) {
    // Font APIs consume float scalars. Accept the exact JSON value or its
    // exact float representation; this is request validation, not a tolerance
    // for comparing the engines' computed results.
    node.Check(
        node.value.isNumeric() &&
            (node.value.asDouble() == request.asDouble() ||
             node.value.asDouble() == static_cast<double>(request.asFloat())),
        "does not match case request");
  } else {
    node.Equal(request);
  }
}

void CheckMetrics(Node metrics) {
  if (!metrics.Object()) {
    return;
  }
  for (const char* name :
       {"top", "ascent", "descent", "bottom", "leading", "x_height",
        "cap_height", "avg_char_width", "max_char_width", "x_min", "x_max",
        "underline_thickness", "underline_position", "strikeout_thickness",
        "strikeout_position"}) {
    metrics[name].Number();
  }
}

void CheckStyle(Node style) {
  style.Object();
  style["weight"].Integer();
  style["width"].Integer();
  style["slant"].Check(style["slant"].value == "upright" ||
                           style["slant"].value == "italic" ||
                           style["slant"].value == "oblique",
                       "expected font slant");
}

void CheckIdentity(Node identity) {
  identity.Object();
  identity["family_name"].String(true);
  identity["post_script_name"].String();
  CheckStyle(identity["style"]);
}

void CheckTables(Node probe) {
  const Node count = probe["table_count"];
  if (!count.Integer() ||
      !count.Check(count.value.asUInt64() > 0, "expected font tables")) {
    return;
  }
  Node tables = probe["tables"];
  if (!tables.Array(count.value.asUInt64())) {
    return;
  }
  std::set<std::string> tags;
  for (Json::ArrayIndex i = 0; i < tables.value.size(); ++i) {
    Node table = tables[i];
    if (table["tag"].String()) {
      table["tag"].Check(table["tag"].value.asString().size() == 4 &&
                             tags.insert(table["tag"].value.asString()).second,
                         "expected unique four-byte table tag");
    }
    if (table["size"].Integer()) {
      table["size"].Check(table["size"].value.asUInt64() > 0, "empty table");
      table["full_copied_size"].Equal(table["size"].value);
    }
    table["full_digest"].String(true);
  }
}

void CheckMappings(Node mappings, const Json::Value& chars) {
  if (!mappings.Array(chars.size())) {
    return;
  }
  for (Json::ArrayIndex i = 0; i < chars.size(); ++i) {
    Node glyph = mappings[i];
    glyph["char"].Equal(chars[i]);
    if (glyph["glyph_id"].Integer()) {
      glyph["contains"].Equal(glyph["glyph_id"].value.asUInt64() != 0);
    }
  }
}

const ResolvedFontFile* SourceFont(const CaseValidationResult& input) {
  const auto& request = input.normalized_case["typeface_request"];
  for (const auto& font : input.resolved_font_files) {
    if (request["font_file"] == font.id) {
      return &font;
    }
  }
  return nullptr;
}

void CheckTypefaces(const CaseValidationResult& input, Node root) {
  const auto& request = input.normalized_case["typeface_request"];
  const auto* font = SourceFont(input);
  if (!font) {
    root.errors->AddError("$.typeface_result", "unresolved font input");
    return;
  }
  const bool collection = request["collection_indices"] == "all";
  uint32_t count = 1;
  if (collection) {
    unsigned char header[12] = {};
    std::ifstream stream(font->absolute_path, std::ios::binary);
    stream.read(reinterpret_cast<char*>(header), sizeof(header));
    if (stream.gcount() == sizeof(header) && header[0] == 't' &&
        header[1] == 't' && header[2] == 'c' && header[3] == 'f') {
      count = (uint32_t(header[8]) << 24) | (uint32_t(header[9]) << 16) |
              (uint32_t(header[10]) << 8) | header[11];
    }
    if (!root["typeface_collection"].Check(count > 0 && count <= 65535,
                                           "invalid input collection count")) {
      return;
    }
    root["typeface_collection"]["collection_count"].Equal(count);
    if (!root["typeface_results"].Array(count) ||
        !root["typeface_probes"].Array(count) ||
        !root["typeface_collection"]["indices"].Array(count)) {
      return;
    }
  }
  for (Json::ArrayIndex i = 0; i < count; ++i) {
    const auto index = collection ? i : font->collection_index;
    Node face =
        collection ? root["typeface_results"][i] : root["typeface_result"];
    Node probe =
        collection ? root["typeface_probes"][i] : root["typeface_probe"];
    face["collection_index"].Equal(index);
    face["request_entry"].Equal(request["entry"]);
    face["font_file_id"].Equal(font->id);
    face["font_file_uri"].Equal(font->uri);
    if (collection) {
      probe["collection_index"].Equal(index);
      root["typeface_collection"]["indices"][i].Equal(index);
    }
    CheckIdentity(face["identity"]);
    face["identity"]["units_per_em"].Integer();
    face["identity"]["glyph_count"].Integer();
    CheckTables(probe);
    CheckMappings(probe["glyphs"], input.normalized_case["glyphs"]["chars"]);
    if (request["entry"] == "MakeVariation") {
      for (const char* field : {"variation_axes", "variation_position"}) {
        Node values = probe[field];
        values.Check(values.value.isArray() && !values.value.empty(),
                     "missing variable font axes or position");
      }
      if (!probe["variation_position"].value.isArray()) continue;
      for (const auto& coordinate : request["variation_position"]) {
        bool found = false;
        for (const auto& position : probe["variation_position"].value) {
          if (position.isObject() && position["axis"] == coordinate["axis"] &&
              position["value"].isNumeric() &&
              coordinate["value"].isNumeric() &&
              position["value"].asDouble() == coordinate["value"].asDouble()) {
            found = true;
          }
        }
        probe["variation_position"].Check(found, "requested axis not captured");
      }
    }
  }
}

Json::Value GlyphLabels(const Json::Value& input) {
  Json::Value labels(Json::arrayValue);
  for (const auto& c : input["glyphs"]["chars"]) {
    labels.append(c);
  }
  for (const auto& id : input["glyphs"]["glyph_ids"]) {
    labels.append("gid:" + id.asString());
  }
  return labels;
}

bool CheckGlyphItems(Node items, const Json::Value& labels, Node requests) {
  if (!items.Array(labels.size())) {
    return false;
  }
  for (Json::ArrayIndex i = 0; i < labels.size(); ++i) {
    items[i]["label"].Equal(labels[i]);
    items[i]["glyph_id"].Integer();
    items[i]["glyph_id"].Equal(requests[i]["glyph_id"].value);
    items[i]["source"].Equal(requests[i]["source"].value);
  }
  return true;
}

void CheckPath(Node item, const Json::Value& input) {
  if (!item.Object()) return;
  item["path_finite"].Equal(true);
  Node path =
      item.value.isMember("path") ? item["path"] : item["normalized_path"];
  if (!path.Object()) {
    return;
  }
  item["path_empty"].Check(item["path_empty"].value.isBool(), "expected bool");
  path["empty"].Equal(item["path_empty"].value);
  path["finite"].Equal(true);
  Node verbs = path["verbs"];
  if (!verbs.Check(verbs.value.isArray(), "expected path verbs")) {
    return;
  }
  path["verb_count"].Equal(verbs.value.size());
  if (input["path_expectation"].isMember("empty")) {
    item["path_empty"].Equal(input["path_expectation"]["empty"]);
  }
  size_t points = 0;
  for (Json::ArrayIndex i = 0; i < verbs.value.size(); ++i) {
    Node verb = verbs[i];
    const auto& name = verb["verb"].value;
    const int count = name == "move"                      ? 1
                      : name == "line"                    ? 2
                      : name == "quad" || name == "conic" ? 3
                      : name == "cubic"                   ? 4
                      : name == "close"                   ? 0
                                                          : -1;
    if (!verb["verb"].Check(count >= 0, "unknown path verb")) {
      continue;
    }
    if (verb["points"].Array(count)) {
      for (Json::ArrayIndex p = 0; p < static_cast<unsigned>(count); ++p) {
        verb["points"][p]["x"].Number();
        verb["points"][p]["y"].Number();
      }
    }
    if (name == "conic") {
      verb["weight"].Number();
    }
    points += count;
  }
  path["point_count"].Equal(Json::UInt64(points));
}

void CheckImage(Node image) {
  if (!image.Object()) {
    return;
  }
  for (const char* key : {"width", "height", "origin_x", "origin_y"}) {
    image[key].Number();
  }
  image["format"].String(true);
  image["has_buffer"].Check(image["has_buffer"].value.isBool(),
                            "expected bool");
  if (!image["byte_size"].Integer()) {
    return;
  }
  const auto size = image["byte_size"].value.asUInt64();
  if (size == 0) {
    const auto& width = image["width"].value;
    const auto& height = image["height"].value;
    image["byte_size"].Check(
        width.isUInt64() && height.isUInt64() &&
            (width.asUInt64() == 0 || height.asUInt64() == 0),
        "empty pixel capture requires an empty image dimension");
    if (image.value.isMember("pixels_hex")) {
      image["pixels_hex"].Equal("");
    }
    return;
  }
  image["has_buffer"].Equal(true);
  image["digest"].String(true);
  const auto format = image["format"].value;
  const unsigned bpp = format == "gray8" || format == "sdf"     ? 1
                       : format == "lcd16"                      ? 2
                       : format == "3d"                         ? 3
                       : format == "rgba8" || format == "bgra8" ? 4
                                                                : 0;
  const auto& width = image["width"].value;
  const auto& height = image["height"].value;
  if (width.isNumeric() && height.isNumeric()) {
    image["byte_size"].Check(
        bpp && width.asDouble() >= 0 && height.asDouble() >= 0 &&
            std::floor(width.asDouble()) == width.asDouble() &&
            std::floor(height.asDouble()) == height.asDouble() &&
            width.asDouble() * height.asDouble() * bpp ==
                static_cast<double>(size),
        "pixel dimensions/format disagree with byte_size");
  }
  Node pixels = image["pixels_hex"];
  if (!pixels.String()) {
    return;
  }
  const auto& hex = pixels.value.asString();
  const bool valid_pixels = pixels.Check(
      size <= std::numeric_limits<size_t>::max() / 2 &&
          hex.size() == size * 2 &&
          std::all_of(hex.begin(), hex.end(),
                      [](unsigned char c) {
                        return (c >= '0' && c <= '9') ||
                               (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
                      }),
      "incomplete or invalid pixel capture");
  if (valid_pixels) {
    auto nibble = [](unsigned char c) {
      return c <= '9' ? c - '0' : (c | 32) - 'a' + 10;
    };
    uint64_t digest = 14695981039346656037ull;
    for (size_t i = 0; i < hex.size(); i += 2) {
      digest ^= (nibble(hex[i]) << 4) | nibble(hex[i + 1]);
      digest *= 1099511628211ull;
    }
    std::ostringstream expected;
    expected << "fnv1a64:" << std::hex << std::setfill('0') << std::setw(16)
             << digest;
    image["digest"].Check(image["digest"].value == expected.str(),
                          "pixel digest does not match captured bytes");
  }
}

void CheckGlyphProbe(const CaseValidationResult& input, Node root) {
  const auto& c = input.normalized_case;
  const bool path = c["category"] == "glyph_path";
  const bool image = c["category"] == "glyph_image";
  Node probe = root[path    ? "glyph_path_probe"
                    : image ? "glyph_image_probe"
                            : "metrics_probe"];
  if (!probe.Object()) {
    return;
  }
  probe["category"].Equal(c["category"]);
  CheckRequest(probe["font_request"], c["font_request"]);
  const auto* font = SourceFont(input);
  if (font) {
    probe["typeface_source"]["entry"].Equal(c["typeface_request"]["entry"]);
    probe["typeface_source"]["font_file_id"].Equal(font->id);
    probe["typeface_source"]["collection_index"].Equal(font->collection_index);
  } else {
    Json::Value source = c["font_manager_request"];
    source["entry"] = "FontManager." + source["entry"].asString();
    source.removeMember("sample_limit");
    if (source["family_name"].isNull() || source["family_name"] == "") {
      source.removeMember("family_name");
    }
    CheckRequest(probe["typeface_source"], source);
  }
  const auto labels = GlyphLabels(c);
  Node requests = probe["glyph_requests"];
  if (!requests.Array(labels.size())) {
    return;
  }
  const auto chars = c["glyphs"]["chars"].size();
  for (Json::ArrayIndex i = 0; i < labels.size(); ++i) {
    requests[i]["label"].Equal(labels[i]);
    requests[i]["glyph_id"].Integer();
    requests[i]["source"].Equal(i < chars ? "char" : "glyph_id");
    if (i >= chars) {
      requests[i]["glyph_id"].Equal(c["glyphs"]["glyph_ids"][i - chars]);
    }
  }
  for (const char* branch : {"font_result", "scaler_context_result"}) {
    if (image && std::string(branch) == "scaler_context_result") {
      continue;
    }
    Node result = probe[branch];
    if (!result.Object()) {
      continue;
    }
    if (std::string(branch) == "scaler_context_result") {
      result["available"].Equal(true);
    }
    const auto field = path    ? "glyph_paths"
                       : image ? "glyph_images"
                               : "glyph_metrics";
    Node items = result[field];
    if (!CheckGlyphItems(items, labels, requests)) {
      continue;
    }
    if (!path && !image) {
      CheckMetrics(result["font_metrics"]);
    }
    for (Json::ArrayIndex i = 0; i < items.value.size(); ++i) {
      if (path) {
        CheckPath(items[i], c);
      } else if (image) {
        CheckImage(items[i]["image"]);
      } else {
        Node data = items[i]["glyph_data"];
        data["glyph_id"].Equal(requests[i]["glyph_id"].value);
        for (const char* key :
             {"advance_x", "advance_y", "left", "top", "width", "height"}) {
          data[key].Number();
        }
      }
    }
  }
}

void CheckMatchedFace(const Json::Value& c, Node face) {
  if (!face["available"].Check(face["available"].value.isBool(),
                               "expected availability") ||
      face["available"].value != true) {
    return;
  }
  CheckIdentity(face["identity"]);
  Node summary = face["probe_summary"];
  CheckMetrics(summary["font_result"]["font_metrics"]);
  summary["scaler_context_result"]["available"].Equal(true);
  CheckMetrics(summary["scaler_context_result"]["font_metrics"]);
  Json::Value chars = c["glyphs"]["chars"];
  if (!chars.isArray() || chars.empty()) {
    chars = Json::Value(Json::arrayValue);
    chars.append("U+0041");
  }
  const auto& request = c["font_manager_request"];
  if (request["entry"] == "MatchFamilyStyleCharacter" &&
      std::find(chars.begin(), chars.end(), request["character"]) ==
          chars.end()) {
    chars.append(request["character"]);
  }
  CheckMappings(summary["glyphs"], chars);
  if (request["entry"] == "MatchFamilyStyleCharacter" &&
      summary["glyphs"].value.isArray()) {
    bool found = false;
    for (const auto& g : summary["glyphs"].value) {
      if (g.isObject() && g["char"] == request["character"] &&
          g["glyph_id"].isUInt() && g["glyph_id"].asUInt() != 0) {
        found = true;
      }
    }
    summary["glyphs"].Check(found, "fallback lacks the requested character");
  }
}

void CheckManager(const CaseValidationResult& input, Node root) {
  const auto& c = input.normalized_case;
  const auto& request = c["font_manager_request"];
  Node probe = root["font_manager_probe"];
  if (!probe.Object()) return;
  probe["request_input"].Equal(request);
  Node operation = probe["operation"];
  operation["entry"].Equal(request["entry"]);
  const bool create = request["entry"] == "CreateStyleSet";
  Node matches = probe["matched_typefaces"];
  if (matches.Array(create ? 2 : 1)) {
    for (Json::ArrayIndex i = 0; i < matches.value.size(); ++i) {
      CheckMatchedFace(c, matches[i]);
    }
  }
  if (create || request["entry"] == "MatchFamily") {
    const auto limit = static_cast<Json::UInt64>(
        std::min(request.get("sample_limit", 8).asInt(), 32));
    for (const std::string& key :
         create ? std::vector<std::string>{"create_style_set", "match_family"}
                : std::vector<std::string>{"style_set"}) {
      Node styles = operation[key];
      if (styles["style_count"].Integer()) {
        const auto count =
            std::min(styles["style_count"].value.asUInt64(), limit);
        if (styles["styles"].Array(count)) {
          for (Json::ArrayIndex i = 0; i < count; ++i) {
            Node item = styles["styles"][i];
            item["index"].Equal(i);
            CheckStyle(item["style"]);
            item["create_typeface"]["available"].Equal(true);
            CheckMatchedFace(c, item["create_typeface"]);
          }
        }
      }
      CheckMatchedFace(c, styles["match_style"]["typeface"]);
    }
  }
  if (c["font_manager_expectation"].isMember("inventory_count")) {
    probe["font_manager"].Object();
  }
  if (root.errors->IsValid()) {
    ValidateFontManagerResult(c, root.value, root.errors);
  }
}

}  // namespace

ArtifactValidationResult ValidateProbeForCase(const CaseValidationResult& input,
                                              const Json::Value& artifact) {
  auto result = ValidateProbeResultDocument(artifact);
  Node root{artifact, "$", &result.errors};
  if (!input.valid || !artifact.isObject()) {
    result.errors.AddError("$", "invalid case or artifact");
    result.valid = false;
    return result;
  }
  root["contract_version"].Check(
      root["contract_version"].value == 2,
      "expected contract_version 2; regenerate artifact");
  root["case_id"].Equal(input.case_id);
  root["backend"].Equal(input.backend);
  root["ok"].Equal(true);
  CheckFiniteTree(root);
  const auto& category = input.normalized_case["category"];
  if (category == "typeface_probe" || category == "variation" ||
      category == "font_tables") {
    CheckTypefaces(input, root);
  } else if (category == "font_manager" || category == "family_style_set") {
    CheckManager(input, root);
  } else if (category == "font_metrics" || category == "glyph_metrics" ||
             category == "scaler_context" || category == "glyph_path" ||
             category == "glyph_image") {
    CheckGlyphProbe(input, root);
  } else {
    root.Check(false, "unsupported artifact category");
  }
  result.valid = result.errors.IsValid();
  return result;
}

}  // namespace font_harness
}  // namespace skity
