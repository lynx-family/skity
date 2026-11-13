// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_TEXT_SDF_GLYPH_RUN_HPP
#define SRC_RENDER_TEXT_SDF_GLYPH_RUN_HPP

#include "src/render/text/glyph_run.hpp"

namespace skity {

void AppendSDFGlyphRuns(const GlyphRunInput& input, const Paint& paint,
                        const GlyphRunBuildContext& context,
                        GlyphSubRunList& runs);

}  // namespace skity

#endif  // SRC_RENDER_TEXT_SDF_GLYPH_RUN_HPP
