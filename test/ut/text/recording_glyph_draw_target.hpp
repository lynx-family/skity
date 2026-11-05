// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef TEST_UT_TEXT_RECORDING_GLYPH_DRAW_TARGET_HPP
#define TEST_UT_TEXT_RECORDING_GLYPH_DRAW_TARGET_HPP

#include <gtest/gtest.h>

#include <vector>

#include "src/render/text/glyph_run.hpp"

namespace skity::test {

class RecordingGlyphDrawTarget : public GlyphDrawTarget {
 public:
  struct PathDraw {
    Path path;
    Paint paint;
    Matrix local_to_layer;
  };

  void SubmitAtlas(const GlyphAtlasDraw& operation) override {
    atlas_draws.push_back(operation);
  }

  void SubmitPath(const Path& path, const Paint& paint,
                  const Matrix& local_to_layer) override {
    path_draws.push_back({path, paint, local_to_layer});
  }

  std::vector<GlyphAtlasDraw> atlas_draws;
  std::vector<PathDraw> path_draws;
};

inline GlyphAtlasDraw RecordAtlasGlyphRun(const GlyphSubRun& run,
                                          const GlyphDrawContext& context) {
  RecordingGlyphDrawTarget target;
  run.Record(context, target);
  EXPECT_TRUE(target.path_draws.empty());
  EXPECT_EQ(target.atlas_draws.size(), 1u);
  return target.atlas_draws.empty() ? GlyphAtlasDraw{} : target.atlas_draws[0];
}

}  // namespace skity::test

#endif  // TEST_UT_TEXT_RECORDING_GLYPH_DRAW_TARGET_HPP
