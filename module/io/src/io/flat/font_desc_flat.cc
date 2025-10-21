// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/io/flat/font_desc_flat.hpp"

#include <cmath>

namespace skity {

namespace {

enum {
  kInvalid = 0x00,

  // Related to a font request.
  kFontFamilyName = 0x01,  // int length, data[length]
  kFullName = 0x04,        // int length, data[length]
  kPostscriptName = 0x06,  // int length, data[length]
  kWeight = 0x10,          // scalar (1 - 1000)
  kWidth = 0x11,           // scalar (percentage, 100 is 'normal')
  kSlant = 0x12,   // scalar (cw angle, 14 is a normal right leaning oblique)
  kItalic = 0x13,  // scalar (0 is Roman, 1 is fully Italic)

  // Related to font data. Can also be used with a requested font.
  kPaletteIndex = 0xF8,           // int
  kPaletteEntryOverrides = 0xF9,  // int count, (int, u32)[count]
  kFontVariation = 0xFA,          // int count, (u32, scalar)[count]

  // Related to font data.
  kFactoryId = 0xFC,  // int
  kFontIndex = 0xFD,  // int
  kSentinel = 0xFF,   // no data
};

void write_string(WriteStream& stream, const std::string& str, uint32_t id) {
  if (str.empty()) {
    return;
  }

  stream.WritePackedUint(id);
  stream.WritePackedUint(str.size());
  stream.Write(str.c_str(), str.size());
}

void write_scalar(WriteStream& stream, float n, uint32_t id) {
  stream.WritePackedUint(id);
  stream.WriteFloat(n);
}

void write_uint(WriteStream& stream, size_t n, uint32_t id) {
  stream.WritePackedUint(id);
  stream.WritePackedUint(n);
}

static constexpr float width_for_usWidth[0x10] = {
    50,  50,  62.5, 75,  87.5, 100, 112.5, 125,
    150, 200, 200,  200, 200,  200, 200,   200};

size_t read_id(ReadStream& stream) {
  size_t i;

  if (!stream.ReadPackedUint(&i)) {
    return kInvalid;
  }

  return i;
}

bool read_string(ReadStream& stream, std::string& str) {
  size_t len;

  if (!stream.ReadPackedUint(&len)) {
    return false;
  }

  str.resize(len);
  if (stream.Read(str.data(), len) != len) {
    return false;
  }

  return true;
}

}  // namespace

void SerializeFontDescriptor(WriteStream& stream, const FontDescriptor& desc) {
  auto style = desc.style;

  uint32_t style_bits =
      (style.weight() << 16) | (style.width() << 8) | style.slant();

  stream.WritePackedUint(style_bits);

  write_string(stream, desc.family_name, kFontFamilyName);
  write_string(stream, desc.full_name, kFullName);
  write_string(stream, desc.post_script_name, kPostscriptName);

  write_scalar(stream, style.weight(), kWeight);
  write_scalar(stream, style.width()[width_for_usWidth], kWidth);
  write_scalar(stream, style.slant() == FontStyle::kUpright_Slant ? 0 : 14,
               kSlant);
  write_scalar(stream, style.slant() == FontStyle::kItalic_Slant ? 1 : 0,
               kItalic);

  if (desc.collection_index > 0) {
    write_uint(stream, desc.collection_index, kFontIndex);
  }

  // we do not support palette for now

  // write font variation
  if (!desc.variation_position.GetCoordinates().empty()) {
    auto count = desc.variation_position.GetCoordinates().size();

    write_uint(stream, count, kFontVariation);

    const auto& coords = desc.variation_position.GetCoordinates();
    for (const auto& [axis, value] : coords) {
      stream.WriteU32(axis);
      stream.WriteFloat(value);
    }
  }

  write_uint(stream, desc.factory_id, kFactoryId);

  stream.WritePackedUint(kSentinel);
}

bool DeserializeFontDescriptor(ReadStream& stream, FontDescriptor& desc) {
  using FactoryIdType = FourByteTag;

  size_t factory_id;
  size_t coordnate_count;
  size_t index;
  size_t palette_index;
  size_t palette_entry_override_count;
  size_t palette_override_index;

  float weight = FontStyle::Weight::kNormal_Weight;
  float width = FontStyle::Width::kNormal_Width;
  float slant = 0;
  float italic = 0;

  size_t style_bits;

  if (!stream.ReadPackedUint(&style_bits)) {
    return false;
  }

  weight = ((style_bits >> 16) & 0xFFFF);
  width = ((style_bits >> 8) & 0x000F)[width_for_usWidth];
  slant = ((style_bits >> 0) & 0x000F) != FontStyle::kUpright_Slant ? 14 : 0;
  italic = ((style_bits >> 0) & 0x000F) == FontStyle::kItalic_Slant ? 1 : 0;

  for (size_t id; (id = read_id(stream)) != kSentinel;) {
    switch (id) {
      case kFontFamilyName:
        if (!read_string(stream, desc.family_name)) {
          return false;
        }
        break;
      case kFullName:
        if (!read_string(stream, desc.full_name)) {
          return false;
        }
        break;
      case kPostscriptName:
        if (!read_string(stream, desc.post_script_name)) {
          return false;
        }
        break;
      case kWeight:
        if (!stream.ReadFloat(&weight)) {
          return false;
        }
        break;
      case kWidth:
        if (!stream.ReadFloat(&width)) {
          return false;
        }
        break;
      case kSlant:
        if (!stream.ReadFloat(&slant)) {
          return false;
        }
        break;
      case kItalic:
        if (!stream.ReadFloat(&italic)) {
          return false;
        }
        break;
      case kFontVariation:
        if (!stream.ReadPackedUint(&coordnate_count)) {
          return false;
        }

        for (size_t i = 0; i < coordnate_count; i++) {
          uint32_t axis;
          float value;

          if (!stream.ReadU32(&axis)) {
            return false;
          }

          if (!stream.ReadFloat(&value)) {
            return false;
          }

          desc.variation_position.AddCoordinate(axis, value);
        }
        break;
      case kFontIndex:
        if (!stream.ReadPackedUint(&index)) {
          return false;
        }

        desc.collection_index = static_cast<int32_t>(index);
        break;
      case kPaletteIndex:
        if (!stream.ReadPackedUint(&palette_index)) {
          return false;
        }
        // we do not support palette for now so skip this value

        break;
      case kPaletteEntryOverrides:
        if (!stream.ReadPackedUint(&palette_entry_override_count)) {
          return false;
        }

        // we do not support palette entry override for now so just read these
        // value, and skip them
        for (size_t i = 0; i < palette_entry_override_count; i++) {
          if (!stream.ReadPackedUint(&palette_override_index)) {
            return false;
          }

          uint32_t color;
          if (!stream.ReadU32(&color)) {
            return false;
          }
        }
        break;
      case kFactoryId:
        if (!stream.ReadPackedUint(&factory_id)) {
          return false;
        }

        desc.factory_id = static_cast<FactoryIdType>(factory_id);
        break;
      default:
        return false;
    }
  }

  FontStyle::Slant slant_enum = FontStyle::kUpright_Slant;
  if (slant != 0) {
    slant_enum = FontStyle::kOblique_Slant;
  }

  if (0 < italic) {
    slant_enum = FontStyle::kItalic_Slant;
  }

  FontStyle::Width width_enum = FontStyle::WidthFromAxisWidth(width);

  desc.style = FontStyle(std::round(weight), width_enum, slant_enum);

  return true;
}

}  // namespace skity
