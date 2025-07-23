// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_TEXT_PORTS_PATH_FREETYPE_HPP
#define SRC_TEXT_PORTS_PATH_FREETYPE_HPP

#include <freetype/freetype.h>

#include <skity/graphic/path.hpp>
#include <skity/text/glyph.hpp>

namespace skity {

class PathFreeType {
 public:
  bool GenerateGlyphPath(FT_Face face, Path* path);

  bool GenerateFacePath(FT_Face face, GlyphID glyph_id, Path* path);
};

}  // namespace skity

#endif  // SRC_TEXT_PORTS_PATH_FREETYPE_HPP
