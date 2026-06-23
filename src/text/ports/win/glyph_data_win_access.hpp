// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_TEXT_PORTS_WIN_GLYPH_DATA_WIN_ACCESS_HPP
#define SRC_TEXT_PORTS_WIN_GLYPH_DATA_WIN_ACCESS_HPP

#include <optional>
#include <skity/geometry/rect.hpp>
#include <skity/text/glyph.hpp>
#include <utility>

namespace skity {

class GlyphDataWinAccess {
 public:
  static void SetMetrics(GlyphData* glyph, float advance_x, float advance_y,
                         const Rect& bounds,
                         std::optional<GlyphFormat> format) {
    glyph->advance_x_ = advance_x;
    glyph->advance_y_ = advance_y;
    glyph->width_ = bounds.Width();
    glyph->height_ = bounds.Height();
    glyph->hori_bearing_x_ = bounds.Left();
    glyph->hori_bearing_y_ = -bounds.Top();
    glyph->y_min_ = bounds.Top();
    glyph->y_max_ = bounds.Bottom();
    glyph->format_ = format;
  }

  static void SetPath(GlyphData* glyph, Path path) {
    glyph->path_ = std::move(path);
  }

  static void SetImage(GlyphData* glyph, GlyphBitmapData image) {
    glyph->image_ = image;
  }

  static void SetFormat(GlyphData* glyph, std::optional<GlyphFormat> format) {
    glyph->format_ = format;
  }
};

}  // namespace skity

#endif  // SRC_TEXT_PORTS_WIN_GLYPH_DATA_WIN_ACCESS_HPP
