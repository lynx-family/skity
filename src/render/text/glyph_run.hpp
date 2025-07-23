// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_TEXT_GLYPH_RUN_HPP
#define SRC_RENDER_TEXT_GLYPH_RUN_HPP

#include <assert.h>

#include <memory>
#include <skity/text/font.hpp>
#include <vector>

#include "src/render/text/atlas/atlas_manager.hpp"
#include "src/utils/arena_allocator.hpp"
#include "src/utils/array_list.hpp"

namespace skity {

class GlyphRun;
class HWPathRaster;
class HWDraw;
class HWDrawHeap;
class HWFontTexture;
class HWFontTextureGroup;
class HWStageBuffer;
using GlyphRunList = ArrayList<GlyphRun*, 16>;
using DrawPathFunc = std::function<void(const Path& path, const Paint& paint)>;

class GlyphRun {
 public:
  static GlyphRunList Make(const uint32_t count, const GlyphID* glyphs,
                           const Point& origin, const float* position_x,
                           const float* position_y, const Font& font,
                           const Paint& paint, float context_scale,
                           const Matrix& transform, AtlasManager* atlas_manager,
                           ArenaAllocator* arena_allocator,
                           DrawPathFunc draw_path_func);

  virtual ~GlyphRun();

  virtual HWDraw* Draw(Matrix transform, ArenaAllocator* arena_allocator,
                       float canvas_scale, bool enable_text_linear_filter) = 0;

  virtual Rect GetBounds() = 0;

  virtual bool IsStroke() = 0;
};

}  // namespace skity

#endif  // SRC_RENDER_TEXT_GLYPH_RUN_HPP
