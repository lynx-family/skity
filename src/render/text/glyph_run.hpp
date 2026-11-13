// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_TEXT_GLYPH_RUN_HPP
#define SRC_RENDER_TEXT_GLYPH_RUN_HPP

#include <cassert>
#include <skity/graphic/paint.hpp>
#include <skity/graphic/path.hpp>
#include <skity/text/font.hpp>

#include "src/utils/arena_allocator.hpp"
#include "src/utils/array_list.hpp"

namespace skity {

class AtlasManager;
class HWDraw;

// Borrowed input, used only during Build(). Subruns own their prepared data.
struct GlyphRunInput {
  uint32_t count;
  const GlyphID* glyphs;
  Point origin;
  const float* position_x;
  const float* position_y;
  const Font& font;

  GlyphRunInput Slice(uint32_t offset, uint32_t length) const {
    assert(offset <= count && length <= count - offset);
    return {length,
            glyphs + offset,
            origin,
            position_x + offset,
            position_y + offset,
            font};
  }
};

struct GlyphRunBuildContext {
  AtlasManager& atlas_manager;
  ArenaAllocator& arena;
  float device_scale;
  Matrix local_to_raster;
};

// Prepared atlas geometry is tied to the build transform and device scale.
// Recording may map it into a different layer, but cannot rerasterize it.
struct GlyphDrawContext {
  ArenaAllocator& arena;
  Matrix raster_to_layer;
  Matrix local_to_layer;
  bool linear_filter;
};

// All metadata describes this operation, including paint-expanded layer bounds.
struct GlyphAtlasDraw {
  HWDraw* draw;
  Rect layer_bounds;
  BlendMode blend_mode;
  bool has_coverage;
  bool source_opaque;
};

// A subrun can submit zero or more operations. The target owns layer, clip,
// blending and submission policy; subruns own glyph representation and
// geometry.
class GlyphDrawTarget {
 public:
  virtual ~GlyphDrawTarget() = default;
  virtual void SubmitAtlas(const GlyphAtlasDraw& operation) = 0;
  virtual void SubmitPath(const Path& path, const Paint& paint,
                          const Matrix& local_to_layer) = 0;
};

class GlyphSubRun {
 public:
  virtual ~GlyphSubRun() = default;
  virtual void Record(const GlyphDrawContext& context,
                      GlyphDrawTarget& target) const = 0;
};

using GlyphSubRunList = ArrayList<GlyphSubRun*, 16>;

class GlyphRunBuilder {
 public:
  // Both the arena and atlas resources must outlive recording. Subruns are
  // frame-scoped and must not be reused after the owning surface is flushed.
  static GlyphSubRunList Build(const GlyphRunInput& input, const Paint& paint,
                               const GlyphRunBuildContext& context);
};

}  // namespace skity

#endif  // SRC_RENDER_TEXT_GLYPH_RUN_HPP
