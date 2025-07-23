// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/sw/sw_a8_drawable.hpp"

namespace skity {

void SWA8Drawable::OnBuildSpan(int x, int y, int width, const uint8_t alpha) {
  int h = static_cast<int>(pixmap_->Height());
  int w = static_cast<int>(pixmap_->Width());
  if (y < 0 || y >= h || x >= w) {
    return;
  }

  int l = std::max(x, 0);
  int r = std::min(x + width - 1, w - 1);
  if (l > r) {
    return;
  }

  size_t base_index = y * row_bytes_ + l;
  std::memset(pixel_addr_ + base_index, alpha, r - l + 1);
}

void SWA8Drawable::Draw(Path const& path, Matrix const& transform) {
  SWRaster raster;
  raster.RastePath(path, transform, SWRaster::kCullRect, this);
}

}  // namespace skity
