// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_TEXT_ATLAS_ATLAS_BITMAP_HPP
#define SRC_RENDER_TEXT_ATLAS_ATLAS_BITMAP_HPP

#include <memory>
#include <optional>
#include <skity/graphic/paint.hpp>
#include <skity/text/typeface.hpp>
#include <vector>

#include "src/render/text/atlas/atlas_allocator.hpp"
#include "src/render/text/atlas/atlas_glyph.hpp"

namespace skity {

class AtlasBitmap {
 public:
  AtlasBitmap(uint32_t width, uint32_t height, uint32_t bytes_per_pixel);

  ~AtlasBitmap();

  // query cache
  glm::ivec4 GetGlyphRegion(GlyphKey key);

  // add one glyph to memory atlas
  glm::ivec4 GenerateGlyphRegion(GlyphKey const& key,
                                 const GlyphBitmapData& bitmap);

  std::optional<glm::ivec4> DirtyRect() const { return dirty_rect_; }

  void SetAllDirty() { dirty_rect_ = {0, 0, width_, height_}; }

  void SetAllClean() { dirty_rect_ = std::nullopt; }

  uint8_t* MemData() const { return mem_data_; }

 private:
  uint32_t width_;
  [[maybe_unused]] uint32_t height_;
  uint32_t bytes_per_pixel_;
  std::unique_ptr<AtlasAllocator> allocator_;
  // std::unordered_map<GlyphKey, glm::ivec4, GlyphKey::Hash, GlyphKey::Equal>
  //     glyph_regions_ = {};
  std::unordered_map<GlyphKey, glm::ivec4, GlyphKey::Hash, GlyphKey::Equal>
      glyph_regions_ = {};
  uint8_t* mem_data_ = nullptr;
  std::optional<glm::ivec4> dirty_rect_ = std::nullopt;
};

}  // namespace skity

#endif  // SRC_RENDER_TEXT_ATLAS_ATLAS_BITMAP_HPP
