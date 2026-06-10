// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "harness/font/probe/typeface_identity.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <optional>
#include <sstream>
#include <utility>

namespace skity {
namespace font_harness {
namespace {

constexpr FontTableTag kMaxpTag = SetFourByteTag('m', 'a', 'x', 'p');
constexpr double kVariationEpsilon = 0.001;

std::string TrimNullPadding(std::string value) {
  while (!value.empty() && value.back() == '\0') {
    value.pop_back();
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

double NormalizeVariationScalar(float value) {
  if (!std::isfinite(value)) {
    return value;
  }
  double normalized =
      std::round(static_cast<double>(value) / kVariationEpsilon) *
      kVariationEpsilon;
  if (normalized == 0.0 || std::fabs(normalized) < kVariationEpsilon * 0.5) {
    return 0.0;
  }
  return normalized;
}

std::optional<uint16_t> ReadGlyphCountFromMaxp(
    const std::shared_ptr<Typeface>& typeface) {
  if (typeface == nullptr || typeface->GetTableSize(kMaxpTag) < 6) {
    return std::nullopt;
  }

  std::array<uint8_t, 6> header{};
  const size_t copied =
      typeface->GetTableData(kMaxpTag, 0, header.size(), header.data());
  if (copied < header.size()) {
    return std::nullopt;
  }
  return static_cast<uint16_t>((static_cast<uint16_t>(header[4]) << 8) |
                               static_cast<uint16_t>(header[5]));
}

}  // namespace

std::string FontHarnessTagToString(uint32_t tag) {
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

Json::Value FontStyleToIdentityJson(const FontStyle& style) {
  Json::Value value(Json::objectValue);
  value["weight"] = style.weight();
  value["width"] = style.width();
  value["slant"] = SlantToString(style.slant());
  return value;
}

Json::Value TypefaceVariationPositionToJson(const VariationPosition& position) {
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
    item["axis"] = FontHarnessTagToString(coordinate.axis);
    item["axis_value"] = static_cast<Json::UInt64>(coordinate.axis);
    item["value"] = NormalizeVariationScalar(coordinate.value);
    value.append(std::move(item));
  }
  return value;
}

Json::Value TypefaceVariationAxesToJson(std::vector<VariationAxis> axes) {
  std::sort(axes.begin(), axes.end(),
            [](const auto& lhs, const auto& rhs) { return lhs.tag < rhs.tag; });

  Json::Value value(Json::arrayValue);
  for (const auto& axis : axes) {
    Json::Value item(Json::objectValue);
    item["tag"] = FontHarnessTagToString(axis.tag);
    item["tag_value"] = static_cast<Json::UInt64>(axis.tag);
    item["min"] = NormalizeVariationScalar(axis.min);
    item["default"] = NormalizeVariationScalar(axis.def);
    item["max"] = NormalizeVariationScalar(axis.max);
    item["hidden"] = axis.hidden;
    value.append(std::move(item));
  }
  return value;
}

Json::Value TypefaceIdentityToJson(const std::shared_ptr<Typeface>& typeface,
                                   const FontDescriptor& descriptor) {
  Json::Value value(Json::objectValue);
  value["family_name"] = TrimNullPadding(descriptor.family_name);
  value["post_script_name"] = TrimNullPadding(descriptor.post_script_name);
  value["full_name"] = TrimNullPadding(descriptor.full_name);
  value["style"] = FontStyleToIdentityJson(descriptor.style);
  value["collection_index"] = descriptor.collection_index;
  value["factory_id"] = FontHarnessTagToString(descriptor.factory_id);
  value["factory_id_value"] = static_cast<Json::UInt64>(descriptor.factory_id);

  if (typeface == nullptr) {
    return value;
  }

  value["units_per_em"] = static_cast<Json::UInt64>(typeface->GetUnitsPerEm());
  value["table_count"] = typeface->CountTables();
  if (std::optional<uint16_t> glyph_count = ReadGlyphCountFromMaxp(typeface)) {
    value["glyph_count"] = static_cast<Json::UInt64>(*glyph_count);
  }
  value["variation_position"] =
      TypefaceVariationPositionToJson(typeface->GetVariationDesignPosition());
  value["variation_axes"] =
      TypefaceVariationAxesToJson(typeface->GetVariationDesignParameters());
  return value;
}

}  // namespace font_harness
}  // namespace skity
