// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/io/memory_read.hpp"
#include "src/picture_priv.hpp"

namespace skity {

namespace {

enum VertexMode {
  kTriangles_VertexMode,
  kTriangleStrip_VertexMode,
  kTriangleFan_VertexMode,

  kLast_VertexMode = kTriangleFan_VertexMode,
};

}  // namespace

struct Vertices {};

#define kMode_Mask 0x0FF
#define kHasTexs_Mask 0x100
#define kHasColors_Mask 0x200

template <typename T>
void SkipReadArrayN(ReadBuffer& buffer, uint32_t size) {
  std::vector<T> array(size);

  buffer.ReadArrayN<T>(array.data(), size);
}

template <>
void SkipFromMemory<Vertices>(ReadBuffer& buffer) {
  bool has_custom_data =
      buffer.IsVersionLT(Version::kVerticesRemoveCustomData_Version);

  uint32_t packed = buffer.ReadU32();
  auto vertex_count = buffer.ReadInt();
  auto index_count = buffer.ReadInt();
  auto attr_count = buffer.ReadInt();

  auto mode = static_cast<VertexMode>(packed & kMode_Mask);

  bool has_texs = packed & kHasTexs_Mask;
  bool has_colors = packed & kHasColors_Mask;

  if (!buffer.IsValid()) {
    return;
  }

  auto v_size = vertex_count * sizeof(Vec2);
  auto t_size = has_texs ? vertex_count * sizeof(Vec2) : 0;
  auto c_size = has_colors ? vertex_count * sizeof(Color) : 0;

  auto i_size = vertex_count * sizeof(uint16_t);

  if (mode == kTriangleFan_VertexMode && index_count == 0) {
    auto num_fran_tris = vertex_count - 2;
    if (num_fran_tris <= 0) {
      buffer.Validate(false);
      return;
    }

    i_size = num_fran_tris * 3 * sizeof(uint16_t);
  }

  SkipReadArrayN<uint8_t>(buffer, v_size);
  if (has_custom_data) {
    size_t custom_data_size = 0;
    buffer.SkipByteArray(custom_data_size);

    if (custom_data_size != 0) {
      buffer.Validate(false);
      return;
    }
  }

  SkipReadArrayN<uint8_t>(buffer, t_size);
  SkipReadArrayN<uint8_t>(buffer, c_size);
  SkipReadArrayN<uint8_t>(buffer, i_size);
}

}  // namespace skity
