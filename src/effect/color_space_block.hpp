// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_EFFECT_COLOR_SPACE_BLOCK_HPP
#define SRC_EFFECT_COLOR_SPACE_BLOCK_HPP

#include <cstring>
#include <skity/graphic/color_space.hpp>
#include <skity/io/flattenable.hpp>
#include <vector>

namespace skity {

// Serialized color space block in the Skia v1 layout: a 4-byte version
// header (version 1 plus three reserved bytes) followed by the transfer
// function (7 floats) and the gamut matrix (9 floats) - 68 bytes total.
// Every writer in this repo (color filters, gradient stops) uses this one
// helper, and the readers reject other sizes or versions, so the wire
// stays interchangeable with Skia's own color space payloads.
inline void WriteColorSpaceBlock(WriteBuffer& buffer,
                                 const float transfer_fn[7],
                                 const float gamut_xyz[9]) {
  struct Header {
    uint8_t version = 1;
    uint8_t reserved0 = 0;
    uint8_t reserved1 = 0;
    uint8_t reserved2 = 0;
  };

  std::vector<uint8_t> data(sizeof(Header) + 16 * sizeof(float));
  Header header{};
  std::memcpy(data.data(), &header, sizeof(header));
  std::memcpy(data.data() + sizeof(Header), transfer_fn, 7 * sizeof(float));
  std::memcpy(data.data() + sizeof(Header) + 7 * sizeof(float), gamut_xyz,
              9 * sizeof(float));
  buffer.WriteByteArray(data.data(), data.size());
}

inline void WriteColorSpaceBlock(WriteBuffer& buffer, const ColorSpace& space) {
  WriteColorSpaceBlock(buffer, space.transfer_fn, space.gamut_xyz);
}

}  // namespace skity

#endif  // SRC_EFFECT_COLOR_SPACE_BLOCK_HPP
