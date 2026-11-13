// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/text/path_glyph_run.hpp"

#include "src/tracing.hpp"

namespace skity {

void PathGlyphRun::Record(const GlyphDrawContext& context,
                          GlyphDrawTarget& target) const {
  SKITY_TRACE_EVENT(PathGlyphRun_Record);
  for (const auto& glyph : paths_) {
    Path path = glyph.path.CopyWithMatrix(
        Matrix::Translate(glyph.position.x, glyph.position.y));
    target.SubmitPath(path, paint_, context.local_to_layer);
  }
}

}  // namespace skity
