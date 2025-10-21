// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <cstring>
#include <skity/geometry/rrect.hpp>

#include "src/io/memory_read.hpp"
#include "src/io/memory_writer.hpp"

namespace skity {

static constexpr size_t kRRectSize = 12 * sizeof(float);

template <>
void FlatIntoMemory(const RRect& value, MemoryWriter32& writer) {
  auto ptr = writer.Reserve(kRRectSize);

  std::memcpy(ptr, &value, kRRectSize);
}

template <>
std::optional<RRect> ReadFromMemory(ReadBuffer& buffer) {
  if (!buffer.Validate(buffer.Available() >= kRRectSize)) {
    return std::nullopt;
  }

  RRect tmp;

  if (!buffer.ReadPad32(&tmp, kRRectSize)) {
    return std::nullopt;
  }

  RRect rrect;

  rrect.SetRectRadii(tmp.GetRect(), tmp.Radii());

  return {rrect};
}

}  // namespace skity
