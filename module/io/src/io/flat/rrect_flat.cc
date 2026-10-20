// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <cstring>

#include "src/io/memory_read.hpp"
#include "src/io/memory_writer.hpp"

namespace skity {

static constexpr size_t kRRectSize = 12 * sizeof(float);

template <>
void FlatIntoMemory(const IORRect& value, MemoryWriter32& writer) {
  auto ptr = writer.Reserve(kRRectSize);

  std::memcpy(ptr, &value, kRRectSize);
}

template <>
std::optional<IORRect> ReadFromMemory(ReadBuffer& buffer) {
  if (!buffer.Validate(buffer.Available() >= kRRectSize)) {
    return std::nullopt;
  }

  IORRect rrect;

  // The 12 floats land in bounds + radii order; corner radii normalization
  // happens inside the core when the value is turned into geometry.
  if (!buffer.ReadPad32(&rrect, kRRectSize)) {
    return std::nullopt;
  }

  return {rrect};
}

}  // namespace skity
