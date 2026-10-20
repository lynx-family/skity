// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_IO_SRC_IO_FLAT_FONT_DESC_FLAT_HPP
#define MODULE_IO_SRC_IO_FLAT_FONT_DESC_FLAT_HPP

#include <skity/text/font_descriptor.hpp>
#include <string>
#include <utility>
#include <vector>

#include "src/stream/stream.hpp"

namespace skity {

void SerializeFontDescriptor(WriteStream& stream, const FontDescriptor& desc);

bool DeserializeFontDescriptor(ReadStream& stream, FontDescriptor& desc);

// Font descriptor as flat fields: the write-side companion of
// FontDescriptor that avoids materializing the legacy struct (whose
// default construction binds a core symbol). The style word is the
// packed FontStyle storage (weight | width << 16 | slant << 24).
struct FontDescriptorFields {
  uint32_t style_packed = 0;
  std::string family_name;
  std::string full_name;
  std::string post_script_name;
  int32_t collection_index = 0;
  std::vector<std::pair<uint32_t, float>> variations;
  uint32_t factory_id = 0;
};

// Serializes the fields with the same byte layout as
// SerializeFontDescriptor(stream, FontDescriptor).
void SerializeFontDescriptorFields(WriteStream& stream,
                                   const FontDescriptorFields& fields);

// Parses and discards a serialized FontDescriptor, advancing the stream the
// same way DeserializeFontDescriptor does. Used by readers that only need to
// get past the descriptor without materializing one (its default member
// initializer would bind a core symbol).
bool SkipFontDescriptor(ReadStream& stream);

}  // namespace skity

#endif  // MODULE_IO_SRC_IO_FLAT_FONT_DESC_FLAT_HPP
