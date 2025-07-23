// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_TEXT_ATLAS_ATLAS_GLYPH_HPP
#define SRC_RENDER_TEXT_ATLAS_ATLAS_GLYPH_HPP

#include <glm/glm.hpp>
#include <memory>
#include <skity/text/glyph.hpp>
#include <skity/text/typeface.hpp>

#include "src/base/hash.hpp"
#include "src/text/scaler_context_desc.hpp"

namespace skity {

constexpr auto Atlas_Padding = 2;
constexpr glm::ivec4 INVALID_LOC = glm::ivec4{-1, -1, 0, 0};

struct AtlasConfig {
  explicit AtlasConfig(AtlasFormat format, bool enable_larger_atlas) {
    // There are 4 or 16 bitmaps to be uploaded onto one texture, and 4 textures
    // to be used dynamiclly, regardless of format. We only config different
    // size of bitmap to match different formats.
    max_num_bitmap_per_texture = enable_larger_atlas ? 16 : 4;
    col_mask = max_num_bitmap_per_texture == 4 ? 0x1 : 0x3;
    row_mask = max_num_bitmap_per_texture == 4 ? 0x2 : 0xC;
    row_shift = max_num_bitmap_per_texture == 4 ? 1 : 2;
    switch (format) {
      case AtlasFormat::A8: {
        max_bitmap_size = 512;
      } break;
      case AtlasFormat::RGBA32: {
        max_bitmap_size = 256;
      } break;
    }
    max_texture_size =
        max_bitmap_size * (max_num_bitmap_per_texture == 4 ? 2 : 4);
    max_num_bitmap_per_atlas =
        max_num_bitmap_per_texture * MAX_NUM_TEXTURE_PER_ATLAS;
  }

  std::uint16_t max_num_bitmap_per_texture;
  std::uint16_t max_num_bitmap_per_atlas;
  std::uint16_t col_mask;
  std::uint16_t row_mask;
  std::uint16_t row_shift;
  std::uint16_t max_bitmap_size;
  std::uint16_t max_texture_size;
  // Sync with the number of textures in fragment shader
  static constexpr std::uint16_t MAX_NUM_TEXTURE_PER_ATLAS = 4;
};

struct GlyphRegion {
  uint32_t index_in_group;
  glm::ivec4 loc;
  float scale;
};

struct GlyphKey {
  const GlyphID glyph_id;
  const ScalerContextDesc scaler_context_desc;

  GlyphKey(GlyphID id, const ScalerContextDesc& desc)
      : glyph_id(id), scaler_context_desc(desc) {}

  struct Hash {
    std::size_t operator()(const GlyphKey& key) const {
      return skity::Hash32(&key, sizeof(GlyphKey));
    }
  };

  struct Equal {
    bool operator()(const GlyphKey& lhs, const GlyphKey& rhs) const {
      return lhs.glyph_id == rhs.glyph_id &&
             lhs.scaler_context_desc == rhs.scaler_context_desc;
    }
  };
};

}  // namespace skity

#endif  // SRC_RENDER_TEXT_ATLAS_ATLAS_GLYPH_HPP
