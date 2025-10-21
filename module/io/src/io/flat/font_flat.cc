// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/text/font.hpp>
#include <skity/text/typeface.hpp>

#include "src/io/memory_read.hpp"
#include "src/io/memory_writer.hpp"

namespace skity {

namespace {

static constexpr uint32_t kDefaultFontFalgs = 1 << 5;  // kBaselineSnap_PrivFlag
static constexpr uint32_t kDefaultEdging = 1;          // kAntiAlias_Edging
static constexpr uint32_t kDefaultHinting =
    2;  // FontHinting::kNormal | glyph outlines

enum {
  kSize_Is_Byte_Bit = 1 << 31,
  kHas_ScaleX_Bit = 1 << 30,
  kHas_SkewX_Bit = 1 << 29,
  kHas_Typeface_Bit = 1 << 28,

  kShift_for_Size = 16,
  kMask_For_Size = 0xFF,

  kShift_For_Flags = 4,
  kMask_For_Flags = 0xFFF,

  kShift_For_Edging = 2,
  kMask_For_Edging = 0x3,

  kShift_For_Hinting = 0,
  kMask_For_Hinting = 0x3
};

bool scalar_is_byte(float x) {
  int ix = static_cast<int>(x);
  return ix == x && ix >= 0 && ix <= kMask_For_Size;
}

}  // namespace

template <>
void FlatIntoMemory<Font>(Font const& font, WriteBuffer& buffer) {
  uint32_t packed = 0;
  packed |= kDefaultFontFalgs << kShift_For_Flags;
  packed |= kDefaultEdging << kShift_For_Edging;
  packed |= kDefaultHinting << kShift_For_Hinting;

  if (scalar_is_byte(font.GetSize())) {
    packed |= kSize_Is_Byte_Bit;
    packed |= static_cast<int32_t>(font.GetSize()) << kShift_for_Size;
  }

  if (font.GetScaleX() != 1.0f) {
    packed |= kHas_ScaleX_Bit;
  }

  if (font.GetSkewX() != 0.0f) {
    packed |= kHas_SkewX_Bit;
  }

  if (font.GetTypeface()) {
    packed |= kHas_Typeface_Bit;
  }

  buffer.WriteInt32(packed);

  if (!(packed & kSize_Is_Byte_Bit)) {
    buffer.WriteFloat(font.GetSize());
  }

  if (packed & kHas_ScaleX_Bit) {
    buffer.WriteFloat(font.GetScaleX());
  }

  if (packed & kHas_SkewX_Bit) {
    buffer.WriteFloat(font.GetSkewX());
  }

  if (packed & kHas_Typeface_Bit) {
    buffer.WriteTypeface(font.GetTypeface());
  }
}

template <>
std::optional<Font> ReadFromMemory(ReadBuffer& buffer) {
  auto packed = buffer.ReadU32();

  Font font;

  if (packed & kSize_Is_Byte_Bit) {
    font.SetSize(
        static_cast<float>((packed >> kShift_for_Size) & kMask_For_Size));
  } else {
    font.SetSize(buffer.ReadFloat());
  }

  if (packed & kHas_ScaleX_Bit) {
    font.SetScaleX(buffer.ReadFloat());
  }

  if (packed & kHas_SkewX_Bit) {
    font.SetSkewX(buffer.ReadFloat());
  }

  if (packed & kHas_Typeface_Bit) {
    font.SetTypeface(buffer.ReadTypeface());
  }

  auto edging = (packed >> kShift_For_Edging) & kMask_For_Edging;

  if (edging > static_cast<uint32_t>(Font::Edging::kSubpixelAntiAlias)) {
    edging = 0;
  }

  font.SetEdging(static_cast<Font::Edging>(edging));

  auto hinting = (packed >> kShift_For_Hinting) & kMask_For_Hinting;
  if (hinting > static_cast<uint32_t>(Font::FontHinting::kFull)) {
    hinting = 0;
  }
  font.SetHinting(static_cast<Font::FontHinting>(hinting));

  if (buffer.IsValid()) {
    return {font};
  } else {
    return std::nullopt;
  }
}

}  // namespace skity
