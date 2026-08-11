// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_TEXT_TRANSFORMED_MASK_GLYPH_RUN_HPP
#define SRC_RENDER_TEXT_TRANSFORMED_MASK_GLYPH_RUN_HPP

#include <cstdint>

#include "src/render/text/glyph_run.hpp"

namespace skity {
namespace transformed_mask {

GlyphRunList MakeGlyphRunList(uint32_t count, const GlyphID* glyphs,
                              const Point& origin, const float* position_x,
                              const float* position_y, const Font& font,
                              const Paint& paint, AtlasFormat format,
                              float context_scale, const Matrix& transform,
                              bool is_stroke, AtlasManager* atlas_manager,
                              ArenaAllocator* arena_allocator);

Paint MakeBitmapOnlyFillPaint(const Paint& paint);

float QuantizeCreationScaleDown(float scale);

uint32_t MaximumAtlasGlyphDimension(const AtlasConfig& atlas_config);

float FitCreationScaleToAtlas(float requested_scale, float logical_dimension,
                              float context_scale, uint32_t maximum_dimension);

bool ComputeViewDifference(const Matrix& position_matrix,
                           const Matrix& creation_matrix,
                           Matrix* view_difference);

Rect MapBounds(const Matrix& transform, const Rect& bounds);

}  // namespace transformed_mask
}  // namespace skity

#endif  // SRC_RENDER_TEXT_TRANSFORMED_MASK_GLYPH_RUN_HPP
