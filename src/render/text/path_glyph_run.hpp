// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_TEXT_PATH_GLYPH_RUN_HPP
#define SRC_RENDER_TEXT_PATH_GLYPH_RUN_HPP

#include <skity/graphic/path.hpp>
#include <utility>
#include <vector>

#include "src/render/text/glyph_run.hpp"

namespace skity {

struct PositionedGlyphPath {
  Path path;
  Vec2 position;
};

void AppendPathGlyphRuns(const GlyphRunInput& input,
                         const GlyphData* const* glyphs, const Paint& paint,
                         const GlyphRunBuildContext& context,
                         GlyphSubRunList& runs);

class PathGlyphRun final : public GlyphSubRun {
 public:
  PathGlyphRun(std::vector<PositionedGlyphPath> paths, Paint paint)
      : paths_(std::move(paths)), paint_(std::move(paint)) {}

  void Record(const GlyphDrawContext& context,
              GlyphDrawTarget& target) const override;

 private:
  const std::vector<PositionedGlyphPath> paths_;
  const Paint paint_;
};

}  // namespace skity

#endif  // SRC_RENDER_TEXT_PATH_GLYPH_RUN_HPP
