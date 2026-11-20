// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_TEXT_DRAWABLE_GLYPH_RUN_HPP
#define SRC_RENDER_TEXT_DRAWABLE_GLYPH_RUN_HPP

#include <memory>
#include <vector>

#include "src/render/text/glyph_run.hpp"
#include "src/text/glyph_drawable.hpp"

namespace skity::drawable {

// Canonical strike size: vector recordings are independent of device scale,
// perspective, subpixel phase and the requested text size.
constexpr float kStrikeSize = 64.f;

std::vector<std::shared_ptr<const GlyphDrawable>> PrepareGlyphs(
    const GlyphRunInput& input, const Paint& paint);

void AppendGlyphRuns(const GlyphRunInput& input,
                     const std::shared_ptr<const GlyphDrawable>* glyphs,
                     const Paint& paint, const GlyphRunBuildContext& context,
                     GlyphSubRunList& runs);

}  // namespace skity::drawable

#endif  // SRC_RENDER_TEXT_DRAWABLE_GLYPH_RUN_HPP
