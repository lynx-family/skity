// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/text/atlas/atlas_bitmap.hpp"

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

glm::ivec4 AtlasBitmap::GetGlyphRegion(GlyphKey key) {
  auto it = glyph_regions_.find(key);
  if (it != glyph_regions_.end()) {
    return it->second;
  }
  return INVALID_LOC;
}

glm::ivec4 AtlasBitmap::GenerateGlyphRegion(GlyphKey const& key,
                                            const GlyphBitmapData& bitmap) {
  if (static_cast<uint32_t>(bitmap.height) == 0 ||
      static_cast<uint32_t>(bitmap.height) == 0) {
    return {0, 0, 0, 0};
  }
  uint32_t width = static_cast<uint32_t>(bitmap.width) + Atlas_Padding;
  uint32_t height = static_cast<uint32_t>(bitmap.height) + Atlas_Padding;
  if (width > width_ - 2 || height > height_ - 2) {
    return {0, 0, 0, 0};
  }
  glm::ivec4 region = allocator_->AllocateRegion(width, height);
  if (region == INVALID_LOC) {
    return region;
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
  glyph_regions_.insert(std::make_pair(key, region));

  // Copy bitmap to memory storage
  uint32_t data_row_size =
      static_cast<uint32_t>(bitmap.width) * bytes_per_pixel_;
  uint32_t self_row_size = this->width_ * bytes_per_pixel_;

  for (uint32_t i = 0; i < static_cast<uint32_t>(bitmap.height); i++) {
    uint8_t* src = bitmap.buffer + data_row_size * i;
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

  return region;
}

}  // namespace skity
