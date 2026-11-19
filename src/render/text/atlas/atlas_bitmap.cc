// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/text/atlas/atlas_bitmap.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
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
  if (mem_data_) {
    std::memset(mem_data_, 0,
                width * height * bytes_per_pixel * sizeof(uint8_t));
  }
}

AtlasBitmap::~AtlasBitmap() {
  if (mem_data_) {
    std::free(mem_data_);
  }
}

GlyphRegion AtlasBitmap::GetGlyphRegion(const GlyphKey& key) {
  auto it = glyph_regions_.find(key);
  if (it != glyph_regions_.end()) {
    return it->second;
  }
  return {0, INVALID_LOC, 1.f};
}

AtlasGlyphResult AtlasBitmap::GenerateGlyphRegion(
    GlyphKey const& key, const GlyphBitmapData& bitmap) {
  if (!mem_data_) {
    return {AtlasGlyphStatus::kInvalidImage};
  }
  auto valid_dimension = [](float value) {
    return std::isfinite(value) && value >= 0.f && std::floor(value) == value &&
           static_cast<double>(value) <= std::numeric_limits<int32_t>::max();
  };
  if (!valid_dimension(bitmap.width) || !valid_dimension(bitmap.height) ||
      !std::isfinite(bitmap.origin_x) || !std::isfinite(bitmap.origin_y)) {
    return {AtlasGlyphStatus::kInvalidImage};
  }
  const uint32_t glyph_width = static_cast<uint32_t>(bitmap.width);
  const uint32_t glyph_height = static_cast<uint32_t>(bitmap.height);
  if (glyph_width == 0 || glyph_height == 0) {
    return {AtlasGlyphStatus::kNoImage};
  }
  GlyphFormat format;
  switch (bitmap.format) {
    case BitmapFormat::kGray8:
      format = GlyphFormat::A8;
      break;
    case BitmapFormat::kRGBA8:
      format = GlyphFormat::RGBA32;
      break;
    case BitmapFormat::kBGRA8:
      format = GlyphFormat::BGRA32;
      break;
    default:
      return {AtlasGlyphStatus::kInvalidImage};
  }
  const uint32_t image_bpp = format == GlyphFormat::A8 ? 1 : 4;
  const size_t data_row_size = static_cast<size_t>(glyph_width) * image_bpp;
  const size_t source_row_size = bitmap.RowBytes();
  if (!bitmap.buffer || image_bpp != bytes_per_pixel_ ||
      source_row_size < data_row_size ||
      source_row_size > std::numeric_limits<size_t>::max() / glyph_height) {
    return {AtlasGlyphStatus::kInvalidImage};
  }
  if (width_ <= Atlas_Padding + 2 || height_ <= Atlas_Padding + 2 ||
      glyph_width > width_ - Atlas_Padding - 2 ||
      glyph_height > height_ - Atlas_Padding - 2) {
    GlyphRegion rejected;
    rejected.format = format;
    return {AtlasGlyphStatus::kTooLarge, rejected, glyph_width, glyph_height};
  }
  glm::ivec4 region = allocator_->AllocateRegion(glyph_width + Atlas_Padding,
                                                 glyph_height + Atlas_Padding);
  if (region == INVALID_LOC) {
    return {AtlasGlyphStatus::kPageFull, {}, glyph_width, glyph_height};
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
  GlyphRegion glyph_region{0,     region, 1.f, bitmap.origin_x, bitmap.origin_y,
                           format};
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

  return {AtlasGlyphStatus::kReady, glyph_region, glyph_width, glyph_height};
}

}  // namespace skity
