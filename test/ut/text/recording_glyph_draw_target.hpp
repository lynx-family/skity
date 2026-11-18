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
  struct DrawableDraw {
    const GlyphDrawable* drawable;
    Paint paint;
    Matrix glyph_to_local;
    Matrix local_to_layer;
  };

  void SubmitAtlas(const GlyphAtlasDraw& operation) override {
    events.push_back({true, atlas_draws.size()});
    atlas_draws.push_back(operation);
  }

  void SubmitPath(const Path& path, const Paint& paint,
                  const Matrix& local_to_layer) override {
    events.push_back({false, path_draws.size()});
    path_draws.push_back({path, paint, local_to_layer});
  }

  void SubmitDrawable(const GlyphDrawable& drawable, const Paint& paint,
                      const Matrix& glyph_to_local,
                      const Matrix& local_to_layer) override {
    events.push_back({false, drawable_draws.size(), true});
    drawable_draws.push_back(
        {&drawable, paint, glyph_to_local, local_to_layer});
  }

  std::vector<GlyphAtlasDraw> atlas_draws;
  std::vector<PathDraw> path_draws;
  std::vector<DrawableDraw> drawable_draws;
  struct Event {
    bool atlas;
    size_t index;
    bool drawable = false;
  };
  std::vector<Event> events;
};

inline GlyphAtlasDraw RecordAtlasGlyphRun(const GlyphSubRun& run,
                                          const GlyphDrawContext& context) {
  RecordingGlyphDrawTarget target;
  run.Record(context, target);
  EXPECT_TRUE(target.path_draws.empty());
  EXPECT_TRUE(target.drawable_draws.empty());
  EXPECT_EQ(target.atlas_draws.size(), 1u);
  return target.atlas_draws.empty() ? GlyphAtlasDraw{} : target.atlas_draws[0];
}

}  // namespace skity::test

#endif  // TEST_UT_TEXT_RECORDING_GLYPH_DRAW_TARGET_HPP
