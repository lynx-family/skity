// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_TEXT_PORTS_COLOR_FREETYPE_HPP
#define SRC_TEXT_PORTS_COLOR_FREETYPE_HPP

#include <freetype/freetype.h>
#include <freetype/ftcolor.h>

#include <memory>
#include <skity/graphic/bitmap.hpp>
#include <skity/render/canvas.hpp>
#include <skity/text/glyph.hpp>
#include <vector>

#include "src/text/ports/path_freetype.hpp"

namespace skity {

class ColorFreeType {
 public:
  explicit ColorFreeType(PathFreeType* path_unitls)
      : path_unitls_(path_unitls) {}

  bool DrawColorV1Glyph(FT_Face face, const GlyphData& glyph);

  bool ComputeColorV1Glyph(FT_Face face, const GlyphData& glyph, Rect* bounds);

  Bitmap* GetBitmap() { return bitmap_.get(); }

 private:
  void PreparePalette(FT_Face face);
  void PrepareCanvas(const GlyphData& glyph);

  [[maybe_unused]] PathFreeType* path_unitls_ = nullptr;

  std::unique_ptr<Bitmap> bitmap_;
  std::unique_ptr<Canvas> canvas_;

  size_t palette_count_ = 0;
  std::vector<Color> palette_;
  // TODO(jingle) Is black right?
  Color foreground_color_ = ColorSetARGB(0xFF, 0x00, 0x00, 0x00);
};

}  // namespace skity

#endif  // SRC_TEXT_PORTS_COLOR_FREETYPE_HPP
