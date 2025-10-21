// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/io/read/read_typeface.hpp"

#include "src/io/flat/font_desc_flat.hpp"

namespace skity {
namespace io {

std::shared_ptr<Typeface> TypefaceMakeFromStream(ReadStream& stream) {
  FontDescriptor desc;
  if (!DeserializeFontDescriptor(stream, desc)) {
    return {};
  }

  size_t length;

  if (!stream.ReadPackedUint(&length)) {
    return {};
  }

  auto data = Data::MakeFromMalloc(std::malloc(length), length);

  if (stream.Read(const_cast<void*>(data->RawData()), length) != length) {
    return {};
  }

  auto fm = FontManager::RefDefault();

  return fm->MakeFromData(data);
}

}  // namespace io
}  // namespace skity
