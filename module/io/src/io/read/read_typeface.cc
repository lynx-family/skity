// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/io/read/read_typeface.hpp"

#include "src/io/core_bridge.hpp"
#include "src/io/flat/font_desc_flat.hpp"

namespace skity {
namespace io {

std::shared_ptr<Typeface> TypefaceMakeFromStream(ReadStream& stream) {
  if (!SkipFontDescriptor(stream)) {
    return {};
  }

  size_t length;

  if (!stream.ReadPackedUint(&length)) {
    return {};
  }

  auto data = core_bridge::MakeDataFromMalloc(std::malloc(length), length);

  if (!data ||
      stream.Read(const_cast<void*>(data->RawData()), length) != length) {
    return {};
  }

  return core_bridge::MakeTypefaceFromBytes(data->Bytes(), data->Size());
}

}  // namespace io
}  // namespace skity
