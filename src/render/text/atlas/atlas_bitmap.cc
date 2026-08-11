// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/text/atlas/atlas_bitmap.hpp"

#include <cstdlib>
#include <cstring>
#include <skity/text/font.hpp>

namespace skity {

AtlasBitmap::AtlasBitmap(uint32_t width, uint32_t height,
                         uint32_t bytes_per_pixel)
    : width_(width),
      height_(height),
      bytes_per_pixel_(bytes_per_pixel),
      allocator_(std::make_unique<AtlasAllocator>(width, height)) {
  mem_data_ = reinterpret_cast<uint8_t*>(
      std::malloc(width * height * bytes_per_pixel * sizeof(uint8_t)));
  std::memset(mem_data_, 0, width * height * bytes_per_pixel * sizeof(uint8_t));
}

AtlasBitmap::~AtlasBitmap() {
  if (mem_data_) {
    std::free(mem_data_);
  }
}

GlyphRegion AtlasBitmap::GetGlyphRegion(GlyphKey key) {
  auto it = glyph_regions_.find(key);
  if (it != glyph_regions_.end()) {
    return it->second;
  }
  return GlyphRegion{0, INVALID_LOC, 1.f};
}

GlyphRegion AtlasBitmap::GenerateGlyphRegion(GlyphKey const& key,
                                             const GlyphBitmapData& bitmap) {
  const uint32_t glyph_width = static_cast<uint32_t>(bitmap.width);
  const uint32_t glyph_height = static_cast<uint32_t>(bitmap.height);
  if (glyph_width == 0 || glyph_height == 0) {
    return GlyphRegion{0, {0, 0, 0, 0}, 1.f};
  }

  const size_t data_row_size =
      static_cast<size_t>(glyph_width) * bytes_per_pixel_;
  const size_t source_row_size = bitmap.RowBytes();
  if (!bitmap.buffer || source_row_size < data_row_size) {
    return GlyphRegion{0, {0, 0, 0, 0}, 1.f};
  }

  uint32_t width = glyph_width + Atlas_Padding;
  uint32_t height = glyph_height + Atlas_Padding;
  if (width > width_ - 2 || height > height_ - 2) {
    return GlyphRegion{0, {0, 0, 0, 0}, 1.f};
  }
  glm::ivec4 region = allocator_->AllocateRegion(width, height);
  if (region == INVALID_LOC) {
    return GlyphRegion{0, region, 1.f};
  }

  // Do not assume textures are zero-initialized on macOS — newly created
  // texture contents may be non-zero.Therefore, we need to treat the padding
  // area as a dirty region. Since the padding region is initialized to zero,
  // sampling from it will result in transparent output.
  glm::ivec4 dirty_region = region;
  region.x += Atlas_Padding / 2;
  region.y += Atlas_Padding / 2;
  region.z -= Atlas_Padding;
  region.w -= Atlas_Padding;
  GlyphRegion glyph_region{0, region, 1.f, bitmap.origin_x, bitmap.origin_y};
  glyph_regions_.insert(std::make_pair(key, glyph_region));

  // Copy bitmap to memory storage
  size_t self_row_size = static_cast<size_t>(this->width_) * bytes_per_pixel_;

  for (uint32_t i = 0; i < glyph_height; i++) {
    uint8_t* src = bitmap.buffer + source_row_size * i;
    uint8_t* dst = this->mem_data_ + self_row_size * (i + region.y) +
                   region.x * bytes_per_pixel_;

    std::memcpy(dst, src, data_row_size);
  }

  // calculate dirty rect
  if (!dirty_rect_.has_value()) {
    dirty_rect_ = {dirty_region.x, dirty_region.y,
                   dirty_region.x + dirty_region.z,
                   dirty_region.y + dirty_region.w};
  } else {
    dirty_rect_->x = std::min(dirty_rect_->x, dirty_region.x);
    dirty_rect_->y = std::min(dirty_rect_->y, dirty_region.y);
    dirty_rect_->z = std::max(dirty_rect_->z, dirty_region.x + dirty_region.z);
    dirty_rect_->w = std::max(dirty_rect_->w, dirty_region.y + dirty_region.w);
  }

  return glyph_region;
}

}  // namespace skity
