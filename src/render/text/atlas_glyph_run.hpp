// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_TEXT_ATLAS_GLYPH_RUN_HPP
#define SRC_RENDER_TEXT_ATLAS_GLYPH_RUN_HPP

#include <optional>
#include <utility>
#include <vector>

#include "src/render/hw/draw/geometry/wgsl_text_geometry.hpp"
#include "src/render/text/atlas/atlas_manager.hpp"
#include "src/render/text/glyph_run.hpp"

namespace skity {

// Quads and bounds share the coordinate space chosen by the raster strategy.
// Atlas locations remain valid only until the owning surface is flushed.
struct AtlasGlyphBatch {
  uint32_t group_index;
  GlyphFormat format;
  std::vector<GlyphRect> quads;
  Rect bounds = Rect::MakeEmpty();
};

// Only adjacent glyphs with compatible bindings may share a batch. A repeated
// group later in the input starts a new batch, preserving compositing order.
class AtlasGlyphBatchBuilder {
 public:
  explicit AtlasGlyphBatchBuilder(Atlas& atlas) : atlas_(atlas) {}

  void Append(const GlyphRegion& region, GlyphFormat format,
              const Rect& bounds);

  std::vector<AtlasGlyphBatch> TakeBatches() { return std::move(batches_); }

 private:
  Atlas& atlas_;
  std::vector<AtlasGlyphBatch> batches_;
};

// CPU quad mapping and GPU vertex mapping are separate: mapping a quad to an
// axis-aligned rectangle is not equivalent to transforming its four vertices.
struct AtlasGlyphDrawParams {
  Matrix draw_transform;
  Matrix quad_transform;
  Matrix local_to_layer;
  Rect layer_bounds;
  GPUFilterMode filter = GPUFilterMode::kLinear;
};

class AtlasGlyphRun : public GlyphSubRun {
 public:
  void Record(const GlyphDrawContext& context,
              GlyphDrawTarget& target) const final;

 protected:
  AtlasGlyphRun(Atlas& atlas, AtlasGlyphBatch batch, Paint paint,
                bool is_stroke, TextAtlasEffect effect);

  const Rect& GeometryBounds() const { return batch_.bounds; }

  virtual std::optional<AtlasGlyphDrawParams> GetDrawParams(
      const GlyphDrawContext& context) const = 0;

 private:
  Atlas& atlas_;
  const AtlasGlyphBatch batch_;
  const Paint paint_;
  const bool is_stroke_;
  const TextAtlasEffect effect_;
};

GlyphFormat ResolveAtlasGlyphFormat(AtlasFormat format, const GlyphData& glyph);
TextAtlasEffect MaskAtlasEffect(GlyphFormat format);

}  // namespace skity

#endif  // SRC_RENDER_TEXT_ATLAS_GLYPH_RUN_HPP
