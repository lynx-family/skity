// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "common/app_utils.hpp"

namespace skity {
namespace example {

void pixmap_swizzle_rb(skity::Pixmap* pixmap) {
  if (pixmap == nullptr || pixmap->Addr() == nullptr) {
    return;
  }
  uint8_t* pixels =
      reinterpret_cast<uint8_t*>(const_cast<void*>(pixmap->Addr()));
  uint32_t w = pixmap->Width();
  uint32_t h = pixmap->Height();
  size_t rb = pixmap->RowBytes();
  for (size_t r = 0; r < h; ++r) {
    uint8_t* p = pixels + rb * r;
    for (size_t c = 0; c < w; ++c) {
      std::swap(p[0], p[2]);
      p += 4;
    }
  }
}

void set_bitmap_color(skity::Bitmap& bitmap, const skity::Rect& bounds,
                      skity::Color color) {
  for (int32_t x = bounds.Left(); x < bounds.Right(); ++x) {
    for (int32_t y = bounds.Top(); y < bounds.Bottom(); ++y) {
      bitmap.SetPixel(x, y, color);
    }
  }
}

std::shared_ptr<skity::Pixmap> make_rect_image(float out_size,
                                               skity::Color out_color,
                                               float in_size,
                                               skity::Color in_color,
                                               float in_offset) {
  skity::Bitmap bitmap(out_size, out_size);

  skity::Rect bounds = skity::Rect::MakeXYWH(0, 0, out_size, out_size);
  if (out_color != skity::Color_TRANSPARENT) {
    set_bitmap_color(bitmap, bounds, out_color);
  }

  if (in_size >= 0) {
    if (in_offset < 0) {
      in_offset = (float(out_size) - float(in_size)) / 2;
    }
    bounds = skity::Rect::MakeXYWH(in_offset, in_offset, in_size, in_size);
    set_bitmap_color(bitmap, bounds, in_color);
  }

  return bitmap.GetPixmap();
}

}  // namespace example
}  // namespace skity
