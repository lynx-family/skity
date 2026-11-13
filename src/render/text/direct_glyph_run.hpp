// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_TEXT_DIRECT_GLYPH_RUN_HPP
#define SRC_RENDER_TEXT_DIRECT_GLYPH_RUN_HPP

#include "src/render/text/glyph_run.hpp"

namespace skity {

void AppendDirectGlyphRuns(const GlyphRunInput& input, const Paint& paint,
                           AtlasFormat format, bool is_stroke,
                           const GlyphRunBuildContext& context,
                           GlyphSubRunList& runs);

}  // namespace skity

#endif  // SRC_RENDER_TEXT_DIRECT_GLYPH_RUN_HPP
