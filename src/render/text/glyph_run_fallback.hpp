// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_TEXT_GLYPH_RUN_FALLBACK_HPP
#define SRC_RENDER_TEXT_GLYPH_RUN_FALLBACK_HPP

#include "src/render/text/atlas/atlas_glyph.hpp"
#include "src/render/text/glyph_run.hpp"

namespace skity {

// Routing only: each strategy owns its representation and recording code.
void AppendFallbackGlyphRuns(const GlyphRunInput& input, const Paint& paint,
                             AtlasFormat format,
                             const GlyphRunBuildContext& context,
                             GlyphSubRunList& runs,
                             const GlyphData* const* prepared_paths = nullptr,
                             bool retry_image = true);

}  // namespace skity

#endif  // SRC_RENDER_TEXT_GLYPH_RUN_FALLBACK_HPP
