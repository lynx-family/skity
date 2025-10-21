// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_IO_SRC_IO_FLAT_FONT_DESC_FLAT_HPP
#define MODULE_IO_SRC_IO_FLAT_FONT_DESC_FLAT_HPP

#include <skity/io/stream.hpp>
#include <skity/text/font_descriptor.hpp>

namespace skity {

void SerializeFontDescriptor(WriteStream& stream, const FontDescriptor& desc);

bool DeserializeFontDescriptor(ReadStream& stream, FontDescriptor& desc);

}  // namespace skity

#endif  // MODULE_IO_SRC_IO_FLAT_FONT_DESC_FLAT_HPP
