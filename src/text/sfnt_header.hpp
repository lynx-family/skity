// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <cstdint>
#include <skity/text/font_arguments.hpp>

#ifndef SRC_TEXT_SFNT_HEADER_HPP
#define SRC_TEXT_SFNT_HEADER_HPP

namespace skity {

using SKITY_SFNT_USHORT = uint16_t;
using SKITY_SFNT_ULONG = uint32_t;

static constexpr uint32_t kWindowsTrueTypeTag = SetFourByteTag(0, 1, 0, 0);
static constexpr uint32_t kMacTrueTypeTag = SetFourByteTag('t', 'r', 'u', 'e');
static constexpr uint32_t kPostScriptTag = SetFourByteTag('t', 'y', 'p', '1');
static constexpr uint32_t kOpenTypeCFFTag = SetFourByteTag('O', 'T', 'T', 'O');

#pragma pack(push, 1)

struct SFNTHeader {
  SKITY_SFNT_ULONG font_type;
  SKITY_SFNT_USHORT num_tables;
  SKITY_SFNT_USHORT search_range;
  SKITY_SFNT_USHORT entry_selector;
  SKITY_SFNT_USHORT range_shift;
};

struct SFNTTableDirectoryEntry {
  SKITY_SFNT_ULONG tag;
  SKITY_SFNT_ULONG checksum;
  SKITY_SFNT_ULONG offset;
  SKITY_SFNT_ULONG length;
};

#pragma pack(pop)

static_assert(sizeof(SFNTHeader) == 12, "SFNTHeader size error");
static_assert(sizeof(SFNTTableDirectoryEntry) == 16,
              "SFNTTableDirectoryEntry size error");

}  // namespace skity

#endif  // SRC_TEXT_SFNT_HEADER_HPP
